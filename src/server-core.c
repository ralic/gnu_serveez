/*
 * server.c - server loop implementation
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: server-core.c,v 1.4 2000/06/13 16:50:46 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#ifdef __MINGW32__
# include <process.h>
#else
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

#include "alloc.h"
#include "util.h"
#include "socket.h"
#include "pipe-socket.h"
#include "server-core.h"
#include "serveez.h"
#include "coserver/coserver.h"

#if ENABLE_HTTP_PROTO
# include "http-server/http-proto.h"
# include "http-server/http-cache.h"
#endif 

/*
 * SOCK_LOOKUP_TABLE is used to speed up references to socket
 * structures by socket's id.
 */
extern socket_t sock_lookup_table[SOCKET_MAX_IDS];

/* 
 * When SERVER_NUKE_HAPPENED is set to a non-zero value, the server
 * will terminate its main loop.
 */
int server_nuke_happened;

#ifndef __MINGW32__
/*
 * When SERVER_RESET_HAPPENED gets set to a non-zero value, the server
 * will try to re-initialize itself on the next execution of the main
 * loop.
 */
int server_reset_happened;

/*
 * SERVER_PIPE_BROKE is set to a non-zero value whenever the server
 * receives a SIGPIPE signal.
 */
int server_pipe_broke;
#endif

/*
 * SERVER_COSERVER_DIED is set to a non-zero value whenever the server
 * receives a SIGCHLD signal.
 */
HANDLE server_coserver_died;

/*
 * This is the pointer to the head of the list of sockets, which are
 * handled by the server loop.
 */
socket_t socket_root;

/*
 * SOCKET_LAST always points to the last structure in the socket queue
 * and is NULL when the queue is empty.
 */
static socket_t socket_last;

/* 
 * This holds the time on which the next call to handle_periodic_tasks()
 * should occur.
 */
static time_t next_notify_time;

/*
 * Handle some signals to handle server resets (SIGHUP), to ignore
 * broken pipes (SIGPIPE) and to exit gracefully if requested by the
 * user (SIGINT, SIGTERM).
 */
RETSIGTYPE 
server_signal_handler (int sig)
{

#if HAVE_STRSIGNAL
  log_printf (LOG_WARNING, "signal: %s\n", strsignal (sig));
#endif

  /* we do not have SIGHUP, SIGCHLD and SIGPIPE in Win32 */
#ifndef __MINGW32__ 
  if (sig == SIGHUP)
    {
      server_reset_happened = 1;
      signal(SIGHUP, server_signal_handler);
    }
  else if (sig == SIGPIPE)
    {
      server_pipe_broke = 1;
      signal(SIGPIPE, server_signal_handler);
    }
  else if (sig == SIGCHLD)
    {
      server_coserver_died = wait(NULL);
      signal(SIGCHLD, server_signal_handler);
    }
  else
#endif

#ifdef __MINGW32__
  if (sig == SIGTERM || sig == SIGINT || sig == SIGBREAK)
#else
  if (sig == SIGTERM || sig == SIGINT)
#endif
    {
      /*
       * reset signal handlers to the default, so the server
       * can get killed on second try
       */
      server_nuke_happened = 1;
      signal(SIGTERM, SIG_DFL);
      signal(SIGINT, SIG_DFL);

#ifdef __MINGW32__
      signal(SIGBREAK, SIG_DFL);
#endif
    }
#if ENABLE_DEBUG
  else
    {
      log_printf(LOG_DEBUG, "uncaught signal %d\n", sig);
    }
#endif

#ifdef NONVOID_SIGNAL
  return 0;
#endif
}

/*
 * Abort the process, printing the error message MSG first.
 */
static int
abort_me (char * msg)
{
  log_printf(LOG_FATAL, "list validation failed: %s\n", msg);
  abort();
  return 0;
}

#if ENABLE_DEBUG
/*
 * Go through the socket list and check if it still consistent.
 * Abort the prgram with an error message, if not.
 * FIXME:  Calls to this function should be removed once the
 * server is stable because this is a rather slow function.
 */
static int
validate_socket_list (void)
{
  socket_t sock, prev;
  
  prev = NULL;
  sock = socket_root;
  while (sock)
    {
#ifndef __MINGW32__
      if(!(sock->flags & SOCK_FLAG_PIPE))
	{
#endif
	  if (sock->sock_desc == INVALID_SOCKET)
	    {
	      abort_me("invalid file descriptor");
	    }
	  if (sock_lookup_table[sock->socket_id] != sock)
	    {
	      abort_me("lookup table corrupted");
	    }
	  if (prev != sock->prev)
	    {
	      abort_me("list structure invalid (previous socket != prev)");
	    }
#ifndef __MINGW32__
	}
#endif
      prev = sock;
      sock = sock->next;
    }
  if (prev != socket_last)
    {
      abort_me("list structure invalid (last socket != end of list)");
    }
  return 0;
}
#endif

/*
 * Enqueue the socket SOCK into the list of sockets handled by
 * the server loop.
 */
int
sock_enqueue (socket_t sock)
{
#ifndef __MINGW32__
  if (sock->flags & SOCK_FLAG_PIPE)
    {
      if ((sock->pipe_desc[READ] == INVALID_HANDLE || 
	   sock->pipe_desc[WRITE] == INVALID_HANDLE) &&
	  !(sock->flags & SOCK_FLAG_LISTENING))
	{
	  log_printf(LOG_FATAL, "cannot enqueue invalid pipe\n");
	  return -1;
	}
    }
  else
#endif
  if (sock->sock_desc == INVALID_SOCKET)
    {
      log_printf(LOG_FATAL, "cannot enqueue invalid socket\n");
      return -1;
    }

  if (sock_lookup_table[sock->socket_id])
    {
      log_printf(LOG_FATAL, 
		 "socket has been already enqueued (%d)\n",
		 sock->socket_id);
      return -1;
    }

  sock->next = NULL;
  sock->prev = NULL;
  if (!socket_root)
    {
      socket_root = sock;
    }
  else
    {
      socket_last->next = sock;
      sock->prev = socket_last;
    }

  socket_last = sock;
  sock->flags |= SOCK_FLAG_ENQUEUED;
  sock_lookup_table[sock->socket_id] = sock;

  return 0;
}

/*
 * Remove the socket SOCK from the list of sockets handled by
 * the server loop.
 */
int
sock_dequeue (socket_t sock)
{

#ifndef __MINGW32__
  if (sock->flags & SOCK_FLAG_PIPE)
    {
      if ((sock->pipe_desc[READ] == INVALID_HANDLE || 
	   sock->pipe_desc[WRITE] == INVALID_HANDLE) &&
	  !(sock->flags & SOCK_FLAG_LISTENING))
	{
	  log_printf (LOG_FATAL, "cannot dequeue invalid pipe\n");
	  return -1;
	}
    }
  else
#endif /* not __MINGW32__ */

  if (sock->sock_desc == INVALID_SOCKET)
    {
      log_printf(LOG_FATAL, "cannot dequeue invalid socket\n");
      return -1;
    }

  if(sock->flags & SOCK_FLAG_ENQUEUED)
    {
      if (sock->next)
	sock->next->prev = sock->prev;
      else
	socket_last = sock->prev;

      if (sock->prev)
	sock->prev->next = sock->next;
      else
	socket_root = sock->next;

      sock->flags &= ~SOCK_FLAG_ENQUEUED;
      sock_lookup_table[sock->socket_id] = NULL;
    }
  else
    {
      log_printf(LOG_ERROR, "dequeueing unqueued socket %d\n",
		 sock->sock_desc);
    }
  return 0;
}

/*
 * Return the socket structure for the file descriptor FD or NULL
 * if no such socket exists.
 */
socket_t
find_sock_by_id (int id)
{
  if (id & ~(SOCKET_MAX_IDS-1))
    {
      log_printf(LOG_FATAL, "socket id %d is invalid\n", id);
      return NULL;
    }

  return sock_lookup_table[id];
}

#ifndef __MINGW32__
/*
 * This gets called when the server receives a SIGHUP, which means
 * that the server should be reset.
 */
static int
server_reset (void)
{
#if ENABLE_HTTP_PROTO
  http_free_cache();
#endif
  return 0;
}
#endif

/*
 * Do everything to shut down the socket SOCK.  The socket structure
 * gets removed from the socket queue, the file descriptor is closed 
 * and all memory used by the socket gets freed. Note that this
 * function calls SOCK's disconnect handler if defined.
 */
static int
shutdown_socket (socket_t sock)
{
#if ENABLE_DEBUG
  log_printf(LOG_DEBUG, "shutting down socket id %d\n", sock->socket_id);
#endif

  if(sock->disconnected_socket)
    sock->disconnected_socket(sock);

  sock_dequeue(sock);
  sock_disconnect(sock);
  sock_free(sock);

  return 0;
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
      log_printf(LOG_DEBUG, "scheduling socket id %d for shutdown\n",
		 sock->socket_id);
#endif /* ENABLE_DEBUG */

      sock->flags |= SOCK_FLAG_KILLED;
    }
  return 0;
}

/*
 * This routine gets called once a second and is supposed to
 * perform any task that has to get scheduled periodically.
 * It checks all sockets' timers and calls their timer functions
 * when necessary.
 */
static int
handle_periodic_tasks (void)
{
  socket_t sock;

  next_notify_time += 1;

#if 0
  log_printf(LOG_NOTICE, "idle\n");
#endif

  sock = socket_root; 
  while (sock)
    {
#if defined(__MINGW32__) && defined(ENABLE_HTTP_PROTO)
      /*
       * check if there died a process handle in Win32, this has to be
       * done regularly here because there is no SIGCHLD in Win32 !
       */
      DWORD result;
      http_socket_t *http = sock->http;

      if(sock->flags & SOCK_FLAG_HTTP_CGI && http->pid != INVALID_HANDLE)
	{
	  result = WaitForSingleObject(http->pid, LEAST_WAIT_OBJECT);
	  if(result == WAIT_FAILED)
	    {
	      log_printf(LOG_ERROR, "WaitForSingleObject: %s\n", SYS_ERROR);
	    }
	  else if(result != WAIT_TIMEOUT)
	    {
	      if(CLOSE_HANDLE(http->pid) == -1)
		log_printf(LOG_ERROR, "CloseHandle: %s\n", SYS_ERROR);
	      http->pid = INVALID_HANDLE;
	      server_coserver_died = http->pid;
	    }
	}
#endif /* not __MINGW32__ or ENABLE_HTTP_PROTO */

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
	      if (sock->idle_func(sock))
		{
		  log_printf(LOG_ERROR, 
			     "idle function for socket id %d "
			     "returned error\n", sock->socket_id);
		  sock_schedule_for_shutdown(sock);
		}
	    }
	}
      sock = sock->next;
    }

#ifdef __MINGW32__
  /*
   * check regularly for internal coserver responses ... *sigh*
   */
  check_internal_coservers();
#endif /* not __MINGW32__ */

  return 0;
}

/*
 * Goes through all socket and shuts invalid ones down.
 */
void
check_bogus_sockets(void)
{
#ifdef __MINGW32__
  unsigned long readBytes;
#endif
  socket_t sock;

#if ENABLE_DEBUG
  log_printf(LOG_DEBUG, "checking for bogus sockets\n");
#endif /* ENABLE_DEBUG */

  for (sock = socket_root; sock; sock = sock->next)
    {
#ifdef __MINGW32__
      if (ioctlsocket(sock->sock_desc, FIONREAD, &readBytes) == SOCKET_ERROR)
	{
#else /* not __MINGW32__ */
      if (fcntl(sock->sock_desc, F_GETFL) < 0)
	{
#endif /* not __MINGW32__ */
	  log_printf (LOG_ERROR, "socket %d has gone\n", sock->sock_desc);
	  sock_schedule_for_shutdown (sock);
	}

#ifndef __MINGW32__
      if (sock->flags & SOCK_FLAG_PIPE)
	{
	  if (fcntl(sock->pipe_desc[READ], F_GETFL) < 0)
	    {
	      log_printf(LOG_ERROR, "pipe %d has gone\n", 
			 sock->pipe_desc[READ]);
	      sock_schedule_for_shutdown(sock);
	    }
	  if (fcntl(sock->pipe_desc[WRITE], F_GETFL) < 0)
	    {
	      log_printf(LOG_ERROR, "pipe %d has gone\n", 
			 sock->pipe_desc[WRITE]);
	      sock_schedule_for_shutdown(sock);
	    }
	}
#endif /* not __MINGW32__ */
    }
}

/*
 * Check the server and client sockets for incoming connections 
 * and data, and process outgoing data.
 */
static int
check_sockets (void)
{
  int nfds;			/* count of file descriptors to check */
  fd_set read_fds;		/* bitmasks for file descriptors to check */
  fd_set write_fds;		/* dito */
  fd_set except_fds;		/* dito */
  struct timeval wait;		/* used for timeout in select() */
  socket_t sock;

  /*
   * Prepare the file handle sets for the select() call below.
   */
  FD_ZERO(&read_fds);
  FD_ZERO(&except_fds);
  FD_ZERO(&write_fds);
  nfds = 0;

  /*
   * Here we set the bitmaps for all clients we handle.
   */
  for(sock = socket_root; sock; sock = sock->next)
    {
      /* only put the SOCK into fd set if not killed already */
      if (!(sock->flags & SOCK_FLAG_KILLED))
	{
#if ENABLE_HTTP_PROTO
	  /* 
	   * Do not put a http client's file descriptor into the FD_SET 
	   * but read the file.
	   */
	  if(sock->flags & SOCK_FLAG_HTTP_FILE)
	    {
	      if(sock->read_socket)
		if(sock->read_socket(sock))
		  sock_schedule_for_shutdown(sock);
	    }

	  /* 
	   * Put a cgi pipe's descriptor into WRITE set for posting data.
	   * Do this if it is Unix not Win32. Otherwise simply write blocking.
	   */
	  if(sock->flags & SOCK_FLAG_HTTP_POST)
	    {
#ifdef __MINGW32__
	      if(sock->write_socket)
		if(sock->write_socket(sock))
		  sock_schedule_for_shutdown(sock);
#else /* not __MINGW32__ */
	      FD_SET(sock->pipe_desc[WRITE], &write_fds);
	      if(sock->pipe_desc[WRITE] > nfds)
		nfds = sock->pipe_desc[WRITE];
#endif /* not __MINGW32__ */
	    }

	  /* 
	   * Put a cgi pipe's descriptor into READ set for getting data.
	   * Do this if it is Unix not Win32. Otherwise simply read blocking.
	   */
	  if(sock->flags & SOCK_FLAG_HTTP_CGI)
	    {
#ifdef __MINGW32__
	      if(sock->read_socket)
		if(sock->read_socket(sock))
		  sock_schedule_for_shutdown(sock);
#else  /* not __MINGW32__ */
	      FD_SET(sock->pipe_desc[READ], &read_fds);
	      if(sock->pipe_desc[READ] > nfds)
		nfds = sock->pipe_desc[READ];
#endif	  
	    }
#endif /* not ENABLE_HTTP_PROTO */

#ifndef __MINGW32__
	  /*
	   * Put all coserver pipes into the READ/WRITE/EXCEPT set.
	   */
	  if (sock->flags & SOCK_FLAG_PIPE)
	    {
	      if (sock->flags & SOCK_FLAG_LISTENING)
		{
		  if (!(sock->flags & SOCK_FLAG_INITED))
		    {
		      if (sock->read_socket)
			sock->read_socket (sock);
		    }
		}
	      else
		{
		  if (sock->send_buffer_fill > 0)
		    {
		      FD_SET (sock->pipe_desc[WRITE], &except_fds);
		      FD_SET (sock->pipe_desc[WRITE], &write_fds);
		      if (sock->pipe_desc[WRITE] > nfds)
			nfds = sock->pipe_desc[WRITE];
		    }
		  
		  FD_SET (sock->pipe_desc[READ], &except_fds);
		  FD_SET (sock->pipe_desc[READ], &read_fds);
		  if (sock->pipe_desc[READ] > nfds)
		    nfds = sock->pipe_desc[READ];
		}
	    }
	  else
#endif /* not __MINGW32__ */
	    {
	      /* is the socket descriptor unavailable ? */
	      if(sock->unavailable)
		{
		  if(time (NULL) >= sock->unavailable)
		    sock->unavailable = 0;
		}

	      /* every client's socket into EXCEPT and READ */
	      if(!sock->unavailable)
		{
		  FD_SET(sock->sock_desc, &except_fds);
		  FD_SET(sock->sock_desc, &read_fds);
		  if(sock->sock_desc > (SOCKET)nfds)
		    nfds = sock->sock_desc;

		  /* 
		   * put a client's socket into WRITE if necessary 
		   * and possible 
		   */
		  if(sock->send_buffer_fill > 0)
		    {
		      FD_SET(sock->sock_desc, &write_fds);
		    }
		}
	    }
	}
    }
  
  nfds++;

  /*
   * Adjust timeout value, so we won't wait longer than
   * we want.
   */
  wait.tv_sec = next_notify_time - time(NULL);
  if (wait.tv_sec < 0) wait.tv_sec = 0;
  wait.tv_usec = 0;

  if((nfds = select(nfds, &read_fds, &write_fds, &except_fds, &wait)) <= 0)
    {
      if(nfds < 0)
	{
	  log_printf(LOG_ERROR, "select: %s\n", NET_ERROR);
#ifdef __MINGW32__
	  /* FIXME: What value do we choose here ? */
	  if(last_errno != 0)
#else
	  if(errno == EBADF)
#endif
	    check_bogus_sockets();
	  return -1;
	}
      else 
	{
	  /*
	   * select() timed out, so we can do some administrative stuff.
	   */
	  handle_periodic_tasks();
	}
      return 0;
    }

  /* go through all enqueued SOCKs */
  for (sock = socket_root; sock; sock = sock->next)
    {

#ifndef __MINGW32__
      /* outgoing pipe to coserver writable ? */
      if (sock->flags & SOCK_FLAG_PIPE)
	{
	  if (sock->flags & SOCK_FLAG_LISTENING)
	    continue;
	  if (FD_ISSET (sock->pipe_desc[WRITE], &except_fds) ||
	      FD_ISSET (sock->pipe_desc[READ], &except_fds))
	    {
	      log_printf(LOG_ERROR, "exception on pipe %d %d\n",
			 sock->pipe_desc[WRITE],
			 sock->pipe_desc[READ]);
	      sock_schedule_for_shutdown(sock);
	      continue;
	    }
	  if(FD_ISSET(sock->pipe_desc[WRITE], &write_fds))
	    {
	      if(sock->write_socket)
		if(sock->write_socket(sock))
		  {
		    sock_schedule_for_shutdown(sock);
		    continue;
		  }
	    }
	  if(FD_ISSET(sock->pipe_desc[READ], &read_fds))
	    {
	      if(sock->read_socket)
		if(sock->read_socket(sock))
		  {
		    sock_schedule_for_shutdown(sock);
		    continue;
		  }
	    }
	}
      else
#endif
	{
	  /* in the exception set ? */
	  if(FD_ISSET(sock->sock_desc, &except_fds))
	    {
	      log_printf(LOG_ERROR, "exception on socket %d\n",
			 sock->sock_desc);
	      sock_schedule_for_shutdown(sock);
	      continue;
	    }

	  /* socket readable ? */
	  if(FD_ISSET(sock->sock_desc, &read_fds))
	    {
	      if(sock->read_socket)
		if(sock->read_socket(sock))
		  {
		    sock_schedule_for_shutdown(sock);
		    continue;
		  }
	    }

	  /* socket writeable and not kicked while reading ? */
	  if(FD_ISSET(sock->sock_desc, &write_fds))
	    {
	      if(sock->write_socket)
		if(sock->write_socket(sock))
		  {
		    sock_schedule_for_shutdown(sock);
		    continue;
		  }
	    }

#if defined(ENABLE_HTTP_PROTO) && !defined(__MINGW32__)
	  /* outgoing pipe to cgi script writable ? */
	  if(sock->flags & SOCK_FLAG_HTTP_POST &&
	     FD_ISSET(sock->pipe_desc[WRITE], &write_fds))
	    {
	      if(sock->write_socket)
		if(sock->write_socket(sock))
		  {
		    sock_schedule_for_shutdown(sock);
		    continue;
		  }
	    }

	  /* incoming pipe from cgi script readable ? */
	  if (sock->flags & SOCK_FLAG_HTTP_CGI &&
	      FD_ISSET(sock->pipe_desc[READ], &read_fds))
	    {
	      if(sock->read_socket)
		if(sock->read_socket(sock))
		  {
		    sock_schedule_for_shutdown(sock);
		    continue;
		  }
	    }
#endif
	}
    }

  /*
   * We had no chance to time out so we have to explicitly call the
   * timeout handler.
   */
  if(time(NULL) > next_notify_time)
    {
      handle_periodic_tasks();
    }

  return 0;
}

/*
 * Main server loop.  Handle all signals, incoming connections and
 * listening  server sockets.
 */
int
sock_server_loop (void)
{
  socket_t sock, next_sock;

  /*
   * Setup signaling.
   */
  signal(SIGTERM, server_signal_handler);
  signal(SIGINT, server_signal_handler);

#ifdef __MINGW32__
  signal(SIGBREAK, server_signal_handler);
#else
  signal(SIGCHLD, server_signal_handler);
  signal(SIGHUP, server_signal_handler);
  signal(SIGPIPE, server_signal_handler);
#endif

  log_printf(LOG_NOTICE, "starting server loop\n");
  log_printf(LOG_NOTICE, "using %d socket descriptors\n",
	     serveez_config.max_sockets);

  /* 
   * These get set either in the signal handler or from a 
   * command processing routine.
   */
  server_nuke_happened = 0;
  server_coserver_died = 0;

#ifndef __MINGW32__
  server_reset_happened = 0;
#endif /* not __MINGW32__ */

  next_notify_time = time(NULL);

  while (!server_nuke_happened)
    {
      /*
       * FIXME: Remove this once the server is stable.  This wastes
       * a lot of run time.
       */
#if ENABLE_DEBUG
      validate_socket_list();
#endif /* ENABLE_DEBUG */

#ifndef __MINGW32__
      if (server_reset_happened)
	{
	  /* 
	   * SERVER_RESET_HAPPENED gets set in the signal handler
	   * whenever a SIGHUP is received.
	   */
	  log_printf(LOG_NOTICE, "resetting server\n");
	  server_reset();
	  server_reset_happened = 0;
	}

      if (server_pipe_broke)
	{
	  /* 
	   * SERVER_PIPE_BROKE gets set in the signal handler
	   * whenever a SIGPIPE is received.
	   */
	  log_printf(LOG_ERROR, "broken pipe, continuing\n");
	  server_pipe_broke = 0;
	}
#endif /* not __MINGW32__ */

      if(server_coserver_died)
	{
#if ENABLE_HTTP_PROTO
	  http_socket_t *http;

	  /*
	   * Go through all sockets and check whether the died PID
	   * was a CGI script...
	   */
	  sock = socket_root; 
	  while (sock)
	    {
	      http = sock->http;
	      if (sock->flags & SOCK_FLAG_HTTP_CLIENT)
		{
		  if (http->pid == server_coserver_died)
		    {
		      log_printf (LOG_NOTICE, "cgi script pid %d died\n", 
				  (int) server_coserver_died);
		      server_coserver_died = 0;
		      break;
		    }
		}
	      sock = sock->next;
	    }
#endif /* ENABLE_HTTP_PROTO */

	  if (server_coserver_died)
	    {
	      log_printf(LOG_ERROR, "pid %d died\n", 
			 (int)server_coserver_died);
	      server_coserver_died = 0;
	    }
	}

      /*
       * Check for new connections on server port, incoming data from
       * clients and process queued output data.
       */
      check_sockets();

      /*
       * Shut down all sockets that have been scheduled for closing.
       */
      sock = socket_root; 
      while (sock)
	{
	  next_sock = sock->next;
	  if (sock->flags & SOCK_FLAG_KILLED)
	    {
	      shutdown_socket(sock);
	    }
	  sock = next_sock;
	}
    }

  log_printf(LOG_NOTICE, "leaving server loop\n");

  /*
   * Shutdown all sockets within the socket list no matter if scheduled
   * for shutdown or not.
   */
  sock = socket_root;
  while (sock)
    {
      shutdown_socket (sock);
      sock = socket_root;
    }

  /*
   * Deinstall signaling.
   */
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

#ifdef __MINGW32__
  signal(SIGBREAK, SIG_DFL);
#else
  signal(SIGHUP, SIG_DFL);
  signal(SIGPIPE, SIG_DFL);
  signal(SIGCHLD, SIG_DFL);
#endif

  return 0;
}
