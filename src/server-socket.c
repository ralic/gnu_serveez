/*
 * server-socket.c - server sockets for TCP, UDP and pipes
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: server-socket.c,v 1.29 2000/09/27 14:31:25 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif

#include "util.h"
#include "alloc.h"
#include "socket.h"
#include "pipe-socket.h"
#include "udp-socket.h"
#include "server-core.h"
#include "serveez.h"
#include "server-socket.h"
#include "server.h"

#ifdef __MINGW32__

/*
 * This routine has to be called once before you could use any of the
 * Winsock32 API functions under Win32.
 */
int
net_startup (void)
{
  WSADATA WSAData;
 
  if (WSAStartup (WINSOCK_VERSION, &WSAData) == SOCKET_ERROR)
    {
      log_printf (LOG_ERROR, "WSAStartup: %s\n", NET_ERROR);
      WSACleanup ();
      return 0;
    }
  return 1;
}

/*
 * Shutdown the Winsock32 API.
 */
int
net_cleanup (void)
{
  if (WSACleanup() == SOCKET_ERROR)
    {
      log_printf (LOG_ERROR, "WSACleanup: %s\n", NET_ERROR);
      return 0;
    }
  return 1;
}

#endif /* not __MINGW32__ */

/*
 * Create a listening server socket. PORTCFG is the port to bind the 
 * server socket to. Return a null pointer on errors.
 */
socket_t
server_create (portcfg_t *cfg)
{
  SOCKET server_socket;      /* server socket descriptor */
  socket_t sock;             /* socket structure */
  int stype;                 /* socket type (TCP or UDP) */
  int optval;                /* value for setsockopt() */
  int ptype;                 /* protocol type (IP/UDP ?) */

  /* create listening pipe server ? */
  if (cfg->proto & PROTO_PIPE)
    {
      sock = sock_alloc ();
      if (sock)
	{
	  sock_unique_id (sock);
	}
      else
	{
	  log_printf (LOG_ERROR, "unable to allocate socket structure\n");
	  return NULL;
	}
    }

  /* create listening tcp or udp server ! */
  else
    {

      /* Assign the appropiate socket type. */
      switch (cfg->proto)
	{
	case PROTO_TCP:
	  stype = SOCK_STREAM;
	  ptype = IPPROTO_IP;
	  break;
	case PROTO_UDP:
	  stype = SOCK_DGRAM;
	  ptype = IPPROTO_UDP;
	  break;
	case PROTO_ICMP:
	  stype = SOCK_RAW;
	  ptype = IPPROTO_ICMP;
	default:
	  stype = SOCK_STREAM;
	  ptype = IPPROTO_IP;
	  break;
	}

      /* First, create a server socket for listening.  */
      if ((server_socket = socket (AF_INET, stype, ptype)) == INVALID_SOCKET)
	{
	  log_printf (LOG_ERROR, "socket: %s\n", NET_ERROR);
	  return NULL;
	}

      /* 
       * Make the socket be reusable (minimize socket dead-time on 
       * server death).
       */
      optval = 1;
      if (setsockopt (server_socket, SOL_SOCKET, SO_REUSEADDR,
		      (void *) &optval, sizeof (optval)) < 0)
	{
	  log_printf (LOG_ERROR, "setsockopt: %s\n", NET_ERROR);
	  if (closesocket (server_socket) < 0)
	    log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	  return NULL;
	}

      /*
       * Make this socket route around its table. Not a good
       * idea for the real internet...
       */

      /*
      optval = 1;
      if (setsockopt (server_socket, SOL_SOCKET, SO_DONTROUTE,
		      (void *) &optval, sizeof (optval)) < 0)
	{
	  log_printf (LOG_ERROR, "setsockopt: %s\n", NET_ERROR);
	  if (closesocket (server_socket) < 0)
	    log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	  return NULL;
	}
      */

      /* Second, bind the socket to a port. */
      if (bind (server_socket, (struct sockaddr *) cfg->localaddr, 
		sizeof (struct sockaddr)) < 0)
	{
	  log_printf (LOG_ERROR, "bind: %s\n", NET_ERROR);
	  if (closesocket (server_socket) < 0)
	    {
	      log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	    }
	  return NULL;
	}

      /* Prepare for listening on that port. */
      if (cfg->proto & PROTO_TCP)
	{
	  if (listen (server_socket, SOMAXCONN) < 0)
	    {
	      log_printf (LOG_ERROR, "listen: %s\n", NET_ERROR);
	      if (closesocket (server_socket) < 0)
		{
		  log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
		}
	      return NULL;
	    }
	}

      /*
       * Create a unique socket structure for the listening server socket.
       */
      if ((sock = sock_create (server_socket)) == NULL)
	{
	  /* Close the server socket if this routine failed. */
	  if (closesocket (server_socket) < 0)
	    {
	      log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	    }
	}
    }

  /* 
   * Free the receive and send buffers not needed for TCP server
   * sockets and PIPE server.
   */
  if (cfg->proto & (PROTO_TCP | PROTO_PIPE))
    {
      xfree (sock->recv_buffer);
      xfree (sock->send_buffer);
      sock->recv_buffer_size = 0;
      sock->send_buffer_size = 0;
      sock->recv_buffer = NULL;
      sock->send_buffer = NULL;
      sock->check_request = default_detect_proto; 
    }

  /* Setup the socket structure. */
  sock->flags |= SOCK_FLAG_LISTENING;
  sock->flags &= ~SOCK_FLAG_CONNECTED;
  sock->proto |= cfg->proto;

  if (cfg->proto & PROTO_PIPE)
    {
      sock->flags |= SOCK_FLAG_PIPE;
#ifndef __MINGW32__
      sock->recv_pipe = xmalloc (strlen (cfg->inpipe) + 1);
      strcpy (sock->recv_pipe, cfg->inpipe);
      sock->send_pipe = xmalloc (strlen (cfg->outpipe) + 1);
      strcpy (sock->send_pipe, cfg->outpipe);
#else /* __MINGW32__ */
      sock->recv_pipe = xmalloc (strlen (cfg->inpipe) + 10);
      sprintf (sock->recv_pipe, "\\\\.\\pipe\\%s", cfg->inpipe);
      sock->send_pipe = xmalloc (strlen (cfg->outpipe) + 10);
      sprintf (sock->send_pipe, "\\\\.\\pipe\\%s", cfg->outpipe);
#endif /* __MINGW32__ */
      sock->read_socket = server_accept_pipe;
      log_printf (LOG_NOTICE, "listening on pipe %s\n", sock->send_pipe);
    }
  else
    {
      if (cfg->proto & PROTO_TCP)
	{
	  sock->read_socket = server_accept_socket;
	}
      else if (cfg->proto & PROTO_UDP)
	{
	  sock->read_socket = udp_read_socket;
	  sock->write_socket = udp_write_socket;
	  sock->check_request = udp_check_request;
	}
      else if (cfg->proto & PROTO_ICMP)
	{
	  /* FIXME: */
	}

      log_printf (LOG_NOTICE, "listening on %s port %s:%u\n",
		  cfg->proto & PROTO_TCP ? "tcp" : "udp",
		  cfg->localaddr->sin_addr.s_addr == INADDR_ANY ? "*" : 
		  util_inet_ntoa (cfg->localaddr->sin_addr.s_addr),
		  ntohs (sock->local_port));
    }

  return sock;
}

/*
 * Something happened on the a server socket, most probably a client 
 * connection which we will normally accept. This is the default callback
 * for READ_SOCKET for listening tcp sockets.
 */
int
server_accept_socket (socket_t server_sock)
{
  SOCKET client_socket;		/* socket to accept clients on */
  struct sockaddr_in client;	/* address of connecting clients */
  socklen_t client_size;	/* size of the address above */
  socket_t sock;

  memset (&client, 0, sizeof (client));
  client_size = sizeof (client);

  client_socket = accept (server_sock->sock_desc, (struct sockaddr *) &client, 
			  &client_size);

  if (client_socket == INVALID_SOCKET)
    {
      log_printf (LOG_WARNING, "accept: %s\n", NET_ERROR);
      return 0;
    }

  if (connected_sockets >= serveez_config.max_sockets)
    {
      log_printf(LOG_WARNING, "socket descriptor exceeds "
		 "socket limit %d\n", serveez_config.max_sockets);
      if (closesocket(client_socket) < 0)
	{
	  log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	}
      return 0;
    }

#ifndef __MINGW32__
  /* 
   * ... SNIP : from the cygwin mail archives 1999/05 ...
   * The problem is in socket() call on W95 - the socket returned 
   * is non-inheritable handle (unlike NT and Unixes, where
   * sockets are inheritable). To fix the problem DuplicateHandle 
   * call is used to create inheritable handle, and original 
   * handle is closed.
   * ... SNAP ...
   *
   * Thus here is NO NEED to set the FD_CLOEXEC flag and no
   * chance anyway.
   */
  if ((fcntl (client_socket, F_SETFD, FD_CLOEXEC)) < 0)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", NET_ERROR);
      if (closesocket (client_socket) < 0)
	{
	  log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	}
      return 0;
    }
#endif
	  
  log_printf (LOG_NOTICE, "TCP:%u: accepting client on socket %d\n", 
	      ntohs (server_sock->local_port), client_socket);
	  
  /* 
   * Sanity check. Just to be sure that we always handle
   * correctly connects/disconnects.
   */
  sock = socket_root;
  while (sock && sock->sock_desc != client_socket)
    sock = sock->next;
  if (sock)
    {
      log_printf (LOG_FATAL, "socket %d already in use\n",
		  sock->sock_desc);
      if (closesocket (client_socket) < 0)
	{
	  log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	}
      return -1;
    }
  
  /*
   * Now enqueue the accepted client socket and assign the 
   * CHECK_REQUEST callback.
   */
  if ((sock = sock_create (client_socket)))
    {
      sock->flags |= SOCK_FLAG_CONNECTED;
      sock->data = server_sock->data;
      sock->check_request = server_sock->check_request;
      sock->idle_func = default_idle_func; 
      sock->idle_counter = 1;
      sock_enqueue (sock);
      connected_sockets++;
    }

  return 0;
}

/*
 * Check if client pipe is connected. This is the default callback for
 * IDLE_FUNC for listening pipe sockets.
 */
int
server_accept_pipe (socket_t server_sock)
{
#if HAVE_MKFIFO
  struct stat buf;
  mode_t mask;
#endif

#ifdef __MINGW32__
  DWORD connect;
#endif

#if defined (HAVE_MKFIFO) || defined (__MINGW32__)
  HANDLE recv_pipe, send_pipe;
  socket_t sock;
  server_sock->idle_counter = 1;

  /*
   * Pipe requested via port configuration ?
   */
  if (!server_sock->recv_pipe || !server_sock->send_pipe)
    return -1;
#endif

#if HAVE_MKFIFO
  /* Save old permissions and set new. */
  mask = umask (0);

  /* 
   * Test if both of the named pipes have been created yet. 
   * If not then create them locally.
   */
  if (stat (server_sock->recv_pipe, &buf) == -1)
    {
      if (mkfifo (server_sock->recv_pipe, 0666) != 0)
        {
          log_printf (LOG_ERROR, "mkfifo: %s\n", SYS_ERROR);
	  umask (mask);
          return -1;
        }
      if (stat (server_sock->recv_pipe, &buf) == -1 || !S_ISFIFO (buf.st_mode))
	{
          log_printf (LOG_ERROR, "stat: mkfifo() did not create a fifo\n");
	  umask (mask);
          return -1;
	}
    }

  if (stat (server_sock->send_pipe, &buf) == -1)
    {
      if (mkfifo (server_sock->send_pipe, 0666) != 0)
        {
          log_printf (LOG_ERROR, "mkfifo: %s\n", SYS_ERROR);
	  umask (mask);
          return -1;
        }
      if (stat (server_sock->send_pipe, &buf) == -1 || !S_ISFIFO (buf.st_mode))
	{
          log_printf (LOG_ERROR, "stat: mkfifo() did not create a fifo\n");
	  umask (mask);
          return -1;
	}
    }

  /* reassign old umask permissions */
  umask (mask);

  /* 
   * Try opening the server's send pipe. This will fail 
   * until the client has opened it for reading.
   */
  if ((send_pipe = open (server_sock->send_pipe, O_NONBLOCK|O_WRONLY)) == -1)
    {
      if (errno != ENXIO)
	{
	  log_printf (LOG_ERROR, "open: %s\n", SYS_ERROR);
	  return -1;
	}
      return 0;
    }

  /* Try opening the server's read pipe. Should always be possible. */
  if ((recv_pipe = open (server_sock->recv_pipe, O_NONBLOCK|O_RDONLY)) == -1)
    {
      close (send_pipe);
      log_printf (LOG_ERROR, "open: %s\n", SYS_ERROR);
      return 0;
    }

  /* Create a socket structure for the client pipe. */
  if ((sock = pipe_create (recv_pipe, send_pipe)) == NULL)
    {
      close (send_pipe);
      close (recv_pipe);
      return 0;
    }

#elif defined (__MINGW32__) /* not HAVE_MKFIFO */

  /*
   * Create both of the named pipes and put the handles into
   * the server socket structure.
   */
  if (server_sock->pipe_desc[READ] == INVALID_HANDLE)
    {
      recv_pipe = CreateNamedPipe (
	server_sock->recv_pipe,                     /* path */
	PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, /* receive + overlapped */
	PIPE_READMODE_BYTE | PIPE_NOWAIT,           /* binary + non-blocking */
	1,                                          /* one instance only */
	0, 0,                                       /* default buffer sizes */
	100,                                        /* timeout in ms */
	NULL);                                      /* no security */

      if (recv_pipe == INVALID_HANDLE_VALUE || !recv_pipe)
	{
	  log_printf (LOG_ERROR, "CreateNamedPipe: %s\n", SYS_ERROR);
	  return -1;
	}
      server_sock->pipe_desc[READ] = recv_pipe;
    }
  else
    recv_pipe = server_sock->pipe_desc[READ];

  if (server_sock->pipe_desc[WRITE] == INVALID_HANDLE)
    {
      send_pipe = CreateNamedPipe (
	server_sock->send_pipe,                      /* path */
	PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED, /* send + overlapped */
	PIPE_TYPE_BYTE | PIPE_NOWAIT,                /* bin + non-blocking */
	1,                                           /* one instance only */
	0, 0,                                        /* default buffer sizes */
	100,                                         /* timeout in ms */
	NULL);                                       /* no security */
      
      if (send_pipe == INVALID_HANDLE_VALUE || !send_pipe)
	{
	  log_printf (LOG_ERROR, "CreateNamedPipe: %s\n", SYS_ERROR);
	  return -1;
	}
      server_sock->pipe_desc[WRITE] = send_pipe;
    }
  else
    send_pipe = server_sock->pipe_desc[WRITE];

#if 0
  /*
   * Initialize overlapped structures.
   */
  if (os_version >= WinNT4x)
    {
      if (!server_sock->overlap[READ])
	{
	  server_sock->overlap[READ] = xmalloc (sizeof (OVERLAPPED));
	  memset (server_sock->overlap[READ], 0, sizeof (OVERLAPPED));
	  server_sock->overlap[READ]->hEvent = 
	    CreateEvent (NULL, TRUE, TRUE, NULL);
	}
      if (!server_sock->overlap[WRITE])
	{
	  server_sock->overlap[WRITE] = xmalloc (sizeof (OVERLAPPED));
	  memset (server_sock->overlap[WRITE], 0, sizeof (OVERLAPPED));
	  server_sock->overlap[WRITE]->hEvent = 
	    CreateEvent (NULL, TRUE, TRUE, NULL);
	}
    }
#endif

  /*
   * Now try connecting to one of these pipes. This will fail until
   * a client has been connected.
   */
  if (!ConnectNamedPipe (recv_pipe, NULL))
    {
      connect = GetLastError ();
      /* Pipe is listening ? */
      if (connect == ERROR_PIPE_LISTENING)
	return 0;
      /* Pipe finally connected ? */
      if (connect != ERROR_PIPE_CONNECTED)
	{
	  log_printf (LOG_ERROR, "ConnectNamedPipe: %s\n", SYS_ERROR);
	  return -1;
	}
      /* If we got here then a client pipe is successfully connected. */
    }
  /* Because these pipes are non-blocking this is never occuring. */
  else return 0;

  if (!ConnectNamedPipe (send_pipe, NULL))
    {
      connect = GetLastError ();
      /* Pipe is listening ? */
      if (connect == ERROR_PIPE_LISTENING)
	return 0;
      /* Pipe finally connected ? */
      if (connect != ERROR_PIPE_CONNECTED)
	{
	  log_printf (LOG_ERROR, "ConnectNamedPipe: %s\n", SYS_ERROR);
	  return -1;
	}
    }
  /* Overlapped ConnectNamedPipe should return zero. */
  else return 0;

  /* Create a socket structure for the client pipe. */
  if ((sock = pipe_create (recv_pipe, send_pipe)) == NULL)
    {
      /* Just disconnect the client pipes. */
      if (!DisconnectNamedPipe (send_pipe))
	log_printf (LOG_ERROR, "DisconnectNamedPipe: %s\n", SYS_ERROR);
      if (!DisconnectNamedPipe (recv_pipe))
	log_printf (LOG_ERROR, "DisconnectNamedPipe: %s\n", SYS_ERROR);
      return 0;
    }

#if 0
  /* Copy overlapped structures. */
  if (os_version >= WinNT4x)
    {
      sock->overlap[READ] = server_sock->overlap[READ];
      sock->overlap[WRITE] = server_sock->overlap[WRITE];
    }
#endif

#else /* not __MINGW32__ */

  return -1;

#endif /* neither HAVE_MKFIFO nor __MINGW32__ */

#if defined (HAVE_MKFIFO) || defined (__MINGW32__)
  sock->read_socket = pipe_read;
  sock->write_socket = pipe_write;
  sock->flags |= SOCK_FLAG_PIPE;
  sock->referer = server_sock;
  sock->data = server_sock->data;
  sock->check_request = server_sock->check_request;
  sock->disconnected_socket = server_sock->disconnected_socket;
  sock->idle_func = default_idle_func; 
  sock->idle_counter = 1;
  sock_enqueue (sock);

  log_printf (LOG_NOTICE, "%s: accepting client on pipe (%d-%d)\n",
              server_sock->send_pipe, 
	      sock->pipe_desc[READ], sock->pipe_desc[WRITE]);

  server_sock->flags |= SOCK_FLAG_INITED;
  server_sock->referer = sock;
  return 0;
#endif /* HAVE_MKFIFO or __MINGW32__ */
}
