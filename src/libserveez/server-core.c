/*
 * server-core.c - server core implementation
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.  
 *
 * $Id: server-core.c,v 1.8 2001/04/04 22:20:02 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#ifdef __MINGW32__
# include <process.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
# if HAVE_WAIT_H
#  include <wait.h>
# endif
# if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
# endif
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#include "libserveez/alloc.h"
#include "libserveez/util.h"
#include "libserveez/socket.h"
#include "libserveez/pipe-socket.h"
#include "libserveez/server-loop.h"
#include "libserveez/server-core.h"
#include "libserveez/coserver/coserver.h"
#include "libserveez/server.h"

/* 
 * When SERVER_NUKE_HAPPENED is set to a non-zero value, the server
 * will terminate its main loop.
 */
int server_nuke_happened;

/*
 * When SERVER_RESET_HAPPENED gets set to a non-zero value, the server
 * will try to re-initialize itself on the next execution of the main
 * loop.
 */
static int server_reset_happened;

/*
 * SERVER_PIPE_BROKE is set to a non-zero value whenever the server
 * receives a SIGPIPE signal.
 */
static int server_pipe_broke;

/*
 * SERVER_CHILD_DIED is set to a non-zero value whenever the server
 * receives a SIGCHLD signal.
 */
HANDLE server_child_died;

/* 
 * This holds the time on which the next call to server_periodic_tasks()
 * should occur.
 */
time_t server_notify;

/*
 * SOCK_ROOT is the pointer to the head of the list of sockets, which are
 * handled by the server loop.
 */
socket_t sock_root = NULL;

/*
 * SOCK_LAST always points to the last structure in the socket queue
 * and is NULL when the queue is empty.
 */
socket_t sock_last = NULL;

/*
 * SOCK_LOOKUP_TABLE is used to speed up references to socket
 * structures by socket's id.
 */
static socket_t sock_lookup_table[SOCKET_MAX_IDS];
static int socket_id = 0;
static int socket_version = 0;

/*
 * Handle some signals to handle server resets (SIGHUP), to ignore
 * broken pipes (SIGPIPE) and to exit gracefully if requested by the
 * user (SIGINT, SIGTERM).
 */
RETSIGTYPE 
server_signal_handler (int sig)
{
  switch (sig)
    {
#ifdef SIGHUP
    case SIGHUP:
      server_reset_happened = 1;
      signal (SIGHUP, server_signal_handler);
      break;
#endif
#ifdef SIGPIPE
    case SIGPIPE:
      server_pipe_broke = 1;
      signal (SIGPIPE, server_signal_handler);
      break;
#endif
#ifdef SIGCHLD
    case SIGCHLD:
#if HAVE_WAITPID
      {
	int status, pid;
	/* check if the child has been just stopped */
	if ((pid = waitpid (-1, &status, WNOHANG | WUNTRACED)) != -1)
	  {
	    if (!WIFSTOPPED (status))
	      server_child_died = pid;
	  }
      }
#else /* HAVE_WAITPID */
      if ((server_child_died = wait (NULL)) == -1)
	server_child_died = 0;
#endif /* not HAVE_WAITPID */
      signal (SIGCHLD, server_signal_handler);
      break;
#endif
#ifdef SIGBREAK
    case SIGBREAK:
      /*
       * reset signal handlers to the default, so the server
       * can get killed on second try
       */
      server_nuke_happened = 1;
      signal (SIGBREAK, SIG_DFL);
      break;
#endif
#ifdef SIGTERM
    case SIGTERM:
      server_nuke_happened = 1;
      signal (SIGTERM, SIG_DFL);
      break;
#endif
#ifdef SIGINT
    case SIGINT:
      server_nuke_happened = 1;
      signal (SIGINT, SIG_DFL);
      break;
#endif
    default:
      log_printf (LOG_DEBUG, "uncaught signal %d\n", sig);
      break;
    }

#if HAVE_STRSIGNAL
  log_printf (LOG_WARNING, "signal: %s\n", strsignal (sig));
#endif

#ifdef NONVOID_SIGNAL
  return 0;
#endif
}

/*
 * Abort the process, printing the error message MSG first.
 */
static int
server_abort (char *msg)
{
  log_printf (LOG_FATAL, "list validation failed: %s\n", msg);
  abort ();
  return 0;
}

#if ENABLE_DEBUG
/*
 * This function is for debugging purposes only. It shows a text 
 * representation of the current socket list.
 */
static void
sock_print_list (void)
{
  socket_t sock = sock_root;

  while (sock)
    {
      fprintf (stdout, "id: %04d, sock: %p == %p, prev: %p, next: %p\n",
	       sock->id, sock, sock_lookup_table[sock->id], 
	       sock->prev, sock->next);
      sock = sock->next;
    }

  fprintf (stdout, "\n");
}

/*
 * Go through the socket list and check if it is still consistent.
 * Abort the program with an error message, if it is not.
 */
static int
sock_validate_list (void)
{
  socket_t sock, prev;
  
#if 0
  sock_print_list ();
#endif

  prev = NULL;
  sock = sock_root;
  while (sock)
    {
      /* check if the descriptors are valid */
      if (sock->flags & SOCK_FLAG_SOCK)
	{
	  if (sock_valid (sock) == -1)
	    {
	      server_abort ("invalid socket descriptor");
	    }
	}
      if (sock->flags & SOCK_FLAG_PIPE)
	{
	  if (pipe_valid (sock) == -1)
	    {
	      server_abort ("invalid pipe descriptor");
	    }
	}
      
      /* check socket list structure */
      if (sock_lookup_table[sock->id] != sock)
	{
	  server_abort ("lookup table corrupted");
	}
      if (prev != sock->prev)
	{
	  server_abort ("list structure invalid (sock->prev)");
	}
      prev = sock;
      sock = sock->next;
    }

  if (prev != sock_last)
    {
      server_abort ("list structure invalid (last socket)");
    }
  return 0;
}
#endif /* ENABLE_DEBUG */

/*
 * Rechain the socket list to prevent sockets from starving at the end
 * of this list. We will call it every time when a `select ()' or `poll ()' 
 * has returned. Listeners are kept at the beginning of the chain anyway.
 */
static void
sock_rechain_list (void)
{
  socket_t sock;
  socket_t last_listen;
  socket_t end_socket;

  sock = sock_last;
  if (sock && sock->prev)
    {
      end_socket = sock->prev;
      for (last_listen = sock_root; last_listen && last_listen != sock && 
	     last_listen->flags & (SOCK_FLAG_LISTENING | SOCK_FLAG_PRIORITY) &&
	     !(sock->flags & SOCK_FLAG_LISTENING);
	   last_listen = last_listen->next);

      /* just listeners in the list, return */
      if (!last_listen)
	return;

      /* sock is the only non-listening (connected) socket */
      if (sock == last_listen)
	return;

      /* one step back unless we are at the socket root */
      if (last_listen->prev)
	{
	  last_listen = last_listen->prev;

	  /* put sock in front of chain behind listeners */
	  sock->next = last_listen->next;
	  sock->next->prev = sock;

	  /* put sock behind last listener */
	  last_listen->next = sock;
	  sock->prev = last_listen;
	}
      else 
	{
	  /* enqueue at root */
	  sock->next = sock_root;
	  sock->prev = NULL;
	  sock->next->prev = sock;
	  sock_root = sock;
	}
      
      /* mark the new end of chain */
      end_socket->next = NULL;
      sock_last = end_socket;
    }
}

/*
 * Enqueue the socket SOCK into the list of sockets handled by
 * the server loop.
 */
int
sock_enqueue (socket_t sock)
{
  /* check for validity of pipe descriptors */
  if (sock->flags & SOCK_FLAG_PIPE)
    {
      if (pipe_valid (sock) == -1)
	{
	  log_printf (LOG_FATAL, "cannot enqueue invalid pipe\n");
	  return -1;
	}
    }

  /* check for validity of socket descriptors */
  if (sock->flags & SOCK_FLAG_SOCK)
    {
      if (sock_valid (sock) == -1)
	{
	  log_printf (LOG_FATAL, "cannot enqueue invalid socket\n");
	  return -1;
	}
    }

  /* check lookup table */
  if (sock_lookup_table[sock->id] || sock->flags & SOCK_FLAG_ENQUEUED)
    {
      log_printf (LOG_FATAL, "socket id %d has been already enqueued\n", 
		  sock->id);
      return -1;
    }

  /* really enqueue socket */
  sock->next = NULL;
  sock->prev = NULL;
  if (!sock_root)
    {
      sock_root = sock;
    }
  else
    {
      sock_last->next = sock;
      sock->prev = sock_last;
    }

  sock_last = sock;
  sock->flags |= SOCK_FLAG_ENQUEUED;
  sock_lookup_table[sock->id] = sock;

  return 0;
}

/*
 * Remove the socket SOCK from the list of sockets handled by
 * the server loop.
 */
int
sock_dequeue (socket_t sock)
{
  /* check for validity of pipe descriptors */
  if (sock->flags & SOCK_FLAG_PIPE)
    {
      if (pipe_valid (sock) == -1)
	{
	  log_printf (LOG_FATAL, "cannot dequeue invalid pipe\n");
	  return -1;
	}
    }

  /* check for validity of socket descriptors */
  if (sock->flags & SOCK_FLAG_SOCK)
    {
      if (sock_valid (sock) == -1)
	{
	  log_printf (LOG_FATAL, "cannot dequeue invalid socket\n");
	  return -1;
	}
    }

  /* check lookup table */
  if (!sock_lookup_table[sock->id] || !(sock->flags & SOCK_FLAG_ENQUEUED))
    {
      log_printf (LOG_FATAL, "socket id %d has been already dequeued\n", 
		  sock->id);
      return -1;
    }

  /* really dequeue socket */
  if (sock->next)
    sock->next->prev = sock->prev;
  else
    sock_last = sock->prev;

  if (sock->prev)
    sock->prev->next = sock->next;
  else
    sock_root = sock->next;

  sock->flags &= ~SOCK_FLAG_ENQUEUED;
  sock_lookup_table[sock->id] = NULL;

  return 0;
}

/*
 * Return the socket structure for the socket id ID and the version 
 * VERSION or NULL if no such socket exists. If VERSION is -1 it is not
 * checked.
 */
socket_t
sock_find (int id, int version)
{
  socket_t sock;

  if (id & ~(SOCKET_MAX_IDS - 1))
    {
      log_printf (LOG_FATAL, "socket id %d is invalid\n", id);
      return NULL;
    }

  sock = sock_lookup_table[id];
  if (version != -1 && sock && sock->version != version)
    {
      log_printf (LOG_WARNING, "socket version %d (id %d) is invalid\n", 
		  version, id);
      return NULL;
    }

  return sock_lookup_table[id];
}

/*
 * Calculate unique socket structure id and assign a version for a 
 * given SOCK. The version is for validating socket structures. It is 
 * currently used in the coserver callbacks.
 */
int
sock_unique_id (socket_t sock)
{
  do
    {
      socket_id++;
      socket_id &= (SOCKET_MAX_IDS - 1);
    }
  while (sock_lookup_table[socket_id]);

  sock->id = socket_id;
  sock->version = socket_version++;
  
  return socket_id;
}

/*
 * This gets called when the server receives a SIGHUP, which means
 * that the server should be reset.
 */
static int
server_reset (void)
{
  /* FIXME: Maybe `server_t' reset callback ? */
  return 0;
}

/*
 * Do everything to shut down the socket SOCK. The socket structure
 * gets removed from the socket queue, the file descriptor is closed 
 * and all memory used by the socket gets freed. Note that this
 * function calls SOCK's disconnect handler if defined.
 */
static int
sock_shutdown (socket_t sock)
{
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "shutting down socket id %d\n", sock->id);
#endif

  if (sock->disconnected_socket)
    sock->disconnected_socket (sock);

  sock_dequeue (sock);

  if (sock->flags & SOCK_FLAG_SOCK)
    sock_disconnect (sock);
  if (sock->flags & SOCK_FLAG_PIPE)
    pipe_disconnect (sock);

  sock_free (sock);

  return 0;
}

/*
 * Shutdown all sockets within the socket list no matter if it was
 * scheduled for shutdown or not.
 */
void
sock_shutdown_all (void)
{
  socket_t sock;

  sock = sock_root;
  while (sock)
    {
      sock_shutdown (sock);
      sock = sock_root;
    }
  sock_root = sock_last = NULL;
}

/*
 * Mark socket SOCK as killed.  That means that no operations except
 * disconnecting and freeing are allowed anymore.  All marked sockets
 * will be deleted once the server loop is through.  
 */
int
sock_schedule_for_shutdown (socket_t sock)
{
  if (!(sock->flags & SOCK_FLAG_KILLED))
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "scheduling socket id %d for shutdown\n",
		  sock->id);
#endif /* ENABLE_DEBUG */

      sock->flags |= SOCK_FLAG_KILLED;
    }
  return 0;
}

/*
 * This routine gets called once a second and is supposed to perform any 
 * task that has to get scheduled periodically. It checks all sockets' 
 * timers and calls their timer functions when necessary.
 */
int
server_periodic_tasks (void)
{
  socket_t sock;

  server_notify += 1;

  sock = sock_root; 
  while (sock)
    {
#if ENABLE_FLOOD_PROTECTION
      if (sock->flood_points > 0)
	{
	  sock->flood_points--;
	}
#endif /* ENABLE_FLOOD_PROTECTION */

      if (sock->idle_func && sock->idle_counter > 0)
	{
	  if (--sock->idle_counter <= 0)
	    {
	      if (sock->idle_func (sock))
		{
		  log_printf (LOG_ERROR, 
			      "idle function for socket id %d "
			      "returned error\n", sock->id);
		  sock_schedule_for_shutdown (sock);
		}
	    }
	}
      sock = sock->next;
    }

#ifdef __MINGW32__
  /* check regularly for internal coserver responses...  */
  coserver_check ();
#endif /* not __MINGW32__ */

  /* run the server instance timer routines */
  svz_server_notifiers ();

  return 0;
}

/*
 * Goes through all socket and shuts invalid ones down.
 */
void
server_check_bogus (void)
{
#ifdef __MINGW32__
  unsigned long readBytes;
#endif
  socket_t sock;

#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "checking for bogus sockets\n");
#endif /* ENABLE_DEBUG */

  for (sock = sock_root; sock; sock = sock->next)
    {
      if (sock->flags & SOCK_FLAG_SOCK)
	{
#ifdef __MINGW32__
	  if (ioctlsocket (sock->sock_desc, FIONREAD, &readBytes) == 
	      SOCKET_ERROR)
	    {
#else /* not __MINGW32__ */
	  if (fcntl (sock->sock_desc, F_GETFL) < 0)
	    {
#endif /* not __MINGW32__ */
	      log_printf (LOG_ERROR, "socket %d has gone\n", sock->sock_desc);
	      sock_schedule_for_shutdown (sock);
	    }
	}

#ifndef __MINGW32__
      if (sock->flags & SOCK_FLAG_RECV_PIPE)
	{
	  if (fcntl (sock->pipe_desc[READ], F_GETFL) < 0)
	    {
	      log_printf (LOG_ERROR, "pipe %d has gone\n", 
			  sock->pipe_desc[READ]);
	      sock_schedule_for_shutdown (sock);
	    }
	}
      if (sock->flags & SOCK_FLAG_SEND_PIPE)
	{
	  if (fcntl (sock->pipe_desc[WRITE], F_GETFL) < 0)
	    {
	      log_printf (LOG_ERROR, "pipe %d has gone\n", 
			  sock->pipe_desc[WRITE]);
	      sock_schedule_for_shutdown (sock);
	    }
	}
#endif /* not __MINGW32__ */
    }
}
  
/*
 * Setup signaling for the core library.
 */
void
server_signal_up (void)
{
#ifdef SIGTERM
  signal (SIGTERM, server_signal_handler);
#endif
#ifdef SIGINT
  signal (SIGINT, server_signal_handler);
#endif
#ifdef SIGBREAK
  signal (SIGBREAK, server_signal_handler);
#endif
#ifdef SIGCHLD
  signal (SIGCHLD, server_signal_handler);
#endif
#ifdef SIGHUP
  signal (SIGHUP, server_signal_handler);
#endif
#ifdef SIGPIPE
  signal (SIGPIPE, server_signal_handler);
#endif
}

/*
 * Deinstall signaling for the core library.
 */
void
server_signal_dn (void)
{
#ifdef SIGTERM
  signal (SIGTERM, SIG_DFL);
#endif
#ifdef SIGINT
  signal (SIGINT, SIG_DFL);
#endif
#ifdef SIGBREAK
  signal (SIGBREAK, SIG_DFL);
#endif
#ifdef SIGHUP
  signal (SIGHUP, SIG_DFL);
#endif
#ifdef SIGPIPE
  signal (SIGPIPE, SIG_DFL);
#endif
#ifdef SIGCHLD
  signal (SIGCHLD, SIG_DFL);
#endif
}

/*
 * This routine handles all things once and is called regularly in the
 * below `server_loop ()' routine.
 */
void
server_loop_one (void)
{
  socket_t sock, next;
  static int rechain = 0;

  /*
   * FIXME: Remove this once the server is stable.
   */
#if ENABLE_DEBUG
  sock_validate_list ();
#endif /* ENABLE_DEBUG */

  if (server_reset_happened)
    {
      /* SIGHUP received. */
      log_printf (LOG_NOTICE, "resetting server\n");
      server_reset ();
      server_reset_happened = 0;
    }

  if (server_pipe_broke)
    {
      /* SIGPIPE received. */ 
      log_printf (LOG_ERROR, "broken pipe, continuing\n");
      server_pipe_broke = 0;
    }

  if (server_child_died)
    {
      /* SIGCHLD received. */
      log_printf (LOG_ERROR, "child pid %d died\n", (int) server_child_died);
      server_child_died = 0;
    }

  /*
   * Check for new connections on server port, incoming data from
   * clients and process queued output data.
   */
  server_check_sockets ();

  /*
   * Reorder the socket chain every 16 select loops. We do not do it
   * every time for performance reasons.
   */
  if (rechain++ & 16)
    sock_rechain_list ();

  /*
   * Shut down all sockets that have been scheduled for closing.
   */
  sock = sock_root; 
  while (sock)
    {
      next = sock->next;
      if (sock->flags & SOCK_FLAG_KILLED)
	sock_shutdown (sock);
      sock = next;
    }
}

/*
 * Main server loop. Handle all signals, incoming and outgoing connections 
 * and listening server sockets.
 */
int
server_loop (void)
{
  /* Setting up signaling. */
  server_signal_up ();

  /* 
   * Setting up control variables. These get set either in the signal 
   * handler or from a command processing routine.
   */
  server_nuke_happened = 0;
  server_reset_happened = 0;
  server_child_died = 0;
  server_pipe_broke = 0;
  server_notify = time (NULL);

  /* Run the server loop. */
  log_printf (LOG_NOTICE, "entering server loop\n");
  while (!server_nuke_happened)
    {
      server_loop_one ();
    }
  log_printf (LOG_NOTICE, "leaving server loop\n");

  /* Shutdown all socket structures. */
  sock_shutdown_all ();

  /* Reset signaling. */
  server_signal_dn ();

  return 0;
}
