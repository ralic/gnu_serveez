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
 * $Id: server-socket.c,v 1.10 2000/06/20 21:57:38 raimi Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

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
#include "server-core.h"
#include "serveez.h"
#include "server-socket.h"
#include "server.h"

#if ENABLE_IRC_PROTO
# include "irc-server/irc-proto.h"
#endif

server_binding_t *server_binding = NULL;
int server_bindings = 0;

#ifdef __MINGW32__

/*
 * This routine has to be called once before you could use any of the
 * Winsock32 API functions under Win32.
 */
int
net_startup (void)
{
  unsigned long blockMode = 1;
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
 * This functions binds a previouly instanciated server to a specified
 * port configuration.
 */
int
server_bind (server_t *server, portcfg_t *cfg)
{
  int n;

  n = server_bindings++;
  server_binding = xrealloc (server_binding, 
			     sizeof (server_binding_t) * server_bindings);
  server_binding[n].server = server;
  server_binding[n].cfg = cfg;

  return 0;
}

/*
 * Start all server bindings (instances of servers). Go through all port
 * configurations, skip duplicate port configurations, etc.
 */
int
server_start (void)
{
  int n, b;
  socket_t sock;
  server_t *server;

  /*
   * Go through all port bindings.
   */
  for (b = 0; b < server_bindings; b++)
    {
      /*
       * Look for duplicate port configurations.
       */
      for (sock = socket_root; sock; sock = sock->next)
	{
	  /*
	   * Check for duplicate server configurations.
	   */
	  server = NULL;
	  for (n = 0; sock->data && (server = SERVER (sock->data, n)); n++)
	    {
	      if (server->cfg == server_binding[b].server->cfg &&
		  sock->proto == server_binding[b].server->proto)
		{
		  fprintf (stderr, "Cannot bind duplicate server (%s) "
			   "to a single port.\n",
			   server->name);
		  break;
		}
	    }
	  /*
	   * No server configuration found so far.
	   */
	  if (server == NULL)
	    {
	      /*
	       * Is this socket usable for this port configuration ?
	       */
	      if (sock->proto & PROTO_TCP &&
		  server_binding[b].cfg->proto & PROTO_TCP &&
		  server_binding[b].cfg->port == sock->local_port)
		{
		  sock->data = xrealloc (sock->data, 
					 sizeof (void *) * (n + 2));
		  SERVER (sock->data, n) = server_binding[b].server;
		  SERVER (sock->data, n + 1) = NULL;
		  log_printf (LOG_DEBUG, "binding tcp server to existing "
			      "port %d\n", sock->local_port);
		  break;
		}

	      if (sock->proto & PROTO_PIPE &&
		  server_binding[b].cfg->proto & PROTO_PIPE &&
		  !strcmp (server_binding[b].cfg->outpipe, sock->send_pipe))
		{
		  sock->data = xrealloc (sock->data, 
					 sizeof (void *) * (n + 2));
		  SERVER (sock->data, n) = server_binding[b].server;
		  SERVER (sock->data, n + 1) = NULL;
		  log_printf (LOG_DEBUG, "binding pipe server to existing "
			      "file %s\n", sock->send_pipe);
		  break;
		}
	    }
	}
      /*
       * No apropiate socket structure for this port configuration found.
       */
      if (sock == NULL)
	{
	  /*
	   * TCP network port creation.
	   */
	  if (server_binding[b].cfg->proto & PROTO_TCP)
	    {
	      sock = server_create (server_binding[b].cfg);
	      if (sock)
		sock_enqueue (sock);
	    }
	  /*
	   * UDP network port creation.
	   */
	  else if (server_binding[b].cfg->proto & PROTO_UDP)
	    {
	      sock = server_create (server_binding[b].cfg);
	      if (sock)
		sock_enqueue (sock);
	    }
	  /*
	   * Pipe port creation.
	   */
	  else if (server_binding[b].cfg->proto & PROTO_PIPE)
	    {
	      sock = server_create (server_binding[b].cfg);
	      if (sock)
		sock_enqueue (sock);
	    }

	  if (sock)
	    {
	      sock->data = xmalloc (sizeof (void *) * 2);
	      SERVER (sock->data, 0) = server_binding[b].server;
	      SERVER (sock->data, 1) = NULL;
	    }
	}
    }
  
  xfree (server_binding);
  server_binding = NULL;
  server_bindings = 0;
  return 0;
}

/*
 * Create a listening server socket. TYPE is either UDP or TCP. 
 * PROTOCOL can be one of the apropiate SERV_FLAG_* flags and
 * PORT is the port to bind the server socket to.
 * Return a null pointer on errors.
 */
socket_t
server_create (portcfg_t *cfg)
{
  SOCKET server_socket;      /* server socket descriptor */
  struct sockaddr_in server; /* this sockets inet structure */
  socket_t sock;             /* socket structure */
  int stype;                 /* socket type (TCP or UDP) */
  int optval;                /* value for setsockopt() */

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

      /* Assign the apropiate socket type. */
      switch (cfg->proto)
	{
	case PROTO_TCP:
	  stype = SOCK_STREAM;
	  break;
	case PROTO_UDP:
	  stype = SOCK_DGRAM;
	  break;
	default:
	  stype = SOCK_STREAM;
	  break;
	}

      /* First, create a server socket for listening.  */
      if ((server_socket = socket (AF_INET, stype, 0)) == INVALID_SOCKET)
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
	  if (CLOSE_SOCKET (server_socket) < 0)
	    log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	  return NULL;
	}

      /*
       * Make this socket route around its table.
       */
      optval = 1;
      if (setsockopt (server_socket, SOL_SOCKET, SO_DONTROUTE,
		      (void *) &optval, sizeof (optval)) < 0)
	{
	  log_printf (LOG_ERROR, "setsockopt: %s\n", NET_ERROR);
	  if (CLOSE_SOCKET (server_socket) < 0)
	    log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	  return NULL;
	}

      /*
	memset (&server, 0, sizeof (server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons (cfg->port);
      */

      /* Second, bind the socket to a port. */
      if (bind (server_socket, (struct sockaddr *) cfg->localaddr, 
		sizeof (struct sockaddr)) < 0)
	{
	  log_printf (LOG_ERROR, "bind: %s\n", NET_ERROR);
	  if (CLOSE_SOCKET (server_socket) < 0)
	    {
	      log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	    }
	  return NULL;
	}

      /* Prepare for listening on that port. */
      if (listen (server_socket, SOMAXCONN) < 0)
	{
	  log_printf (LOG_ERROR, "listen: %s\n", NET_ERROR);
	  if (CLOSE_SOCKET (server_socket) < 0)
	    {
	      log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	    }
	  return NULL;
	}

      /*
       * Create a unique socket structure for the listening server socket.
       */
      if ((sock = sock_create (server_socket)) == NULL)
	{
	  /* Close the server socket if this routine failed. */
	  if (CLOSE_SOCKET (server_socket) < 0)
	    {
	      log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	    }
	}
    }

  sock->check_request = default_detect_proto; 

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
    }

  /* Setup the socket structure. */
  sock->flags |= SOCK_FLAG_LISTENING;
  sock->flags &= ~SOCK_FLAG_CONNECTED;
  sock->local_addr = INADDR_ANY;
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
      sock->disconnected_socket = pipe_disconnected;
      log_printf (LOG_NOTICE, "listening on pipe %s\n", sock->send_pipe);
    }
  else
    {
      sock->read_socket = server_accept_socket;
      log_printf (LOG_NOTICE, "listening on %s port %s:%d\n",
		  cfg->proto & PROTO_TCP ? "tcp" : "udp",
		  cfg->localaddr->sin_addr.s_addr == INADDR_ANY ? "*" : 
		  util_inet_ntoa (htonl (cfg->localaddr->sin_addr.s_addr)),
		  sock->local_port);
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
      if (CLOSE_SOCKET(client_socket) < 0)
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
      if (CLOSE_SOCKET (client_socket) < 0)
	{
	  log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	}
      return 0;
    }
#endif
	  
  log_printf (LOG_NOTICE, "TCP:%d: accepting client on socket %d\n", 
	      server_sock->local_port, client_socket);
	  
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
      if (CLOSE_SOCKET (client_socket) < 0)
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
  struct stat buf;
  HANDLE recv_pipe, send_pipe;
  socket_t sock;

  server_sock->idle_counter = 1;

  /*
   * Pipe requested via port configuration ?
   */
  if (!server_sock->recv_pipe || !server_sock->send_pipe)
    return 0;

#if HAVE_MKFIFO
  /* 
   * Test if both of the named pipes have been created yet. 
   * If not then create them locally.
   */
  if (stat (server_sock->recv_pipe, &buf) == -1)
    {
      if (mkfifo (server_sock->recv_pipe, 0666) == -1)
        {
          log_printf (LOG_ERROR, "mkfifo: %s\n", SYS_ERROR);
          return 0;
        }
    }

  if (stat (server_sock->send_pipe, &buf) == -1)
    {
      if (mkfifo (server_sock->send_pipe, 0666) == -1)
        {
          log_printf (LOG_ERROR, "mkfifo: %s\n", SYS_ERROR);
          return 0;
        }
    }

  /* 
   * Try opening the server's send pipe. This will fail 
   * until the client has opened it for reading.
   */
  if ((send_pipe = open (server_sock->send_pipe, O_NONBLOCK|O_WRONLY)) == -1)
    {
      return 0;
    }

  /* Try opening the server's read pipe. */
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

  log_printf (LOG_NOTICE, "%s: accepting client on pipe (%d-%d)\n",
              server_sock->send_pipe, 
	      sock->pipe_desc[READ], sock->pipe_desc[WRITE]);

  sock->read_socket = pipe_read;
  sock->write_socket = pipe_write;
  sock->flags |= SOCK_FLAG_PIPE;
  sock->parent = server_sock;
  sock->data = server_sock->data;
  sock->check_request = server_sock->check_request;
  sock->disconnected_socket = server_sock->disconnected_socket;
  sock->idle_func = default_idle_func; 
  sock->idle_counter = 1;
  sock_enqueue (sock);

  server_sock->flags |= SOCK_FLAG_INITED;

#elif defined (__MINGW32__) /* not HAVE_MKFIFO */

  /*
   * Create both of the named pipes and put the handles into
   * the server socket structure.
   */
  if (server_sock->pipe_desc[READ] == INVALID_HANDLE_VALUE)
    {
      if ((recv_pipe = CreateNamedPipe (
             server_sock->recv_pipe,
	     PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
	     PIPE_READMODE_BYTE | PIPE_NOWAIT,
	     1,
	     0, 0,
	     100,
	     NULL)) == INVALID_HANDLE_VALUE)
	{
	  log_printf (LOG_ERROR, "CreateNamedPipe: %s\n", SYS_ERROR);
	  return 0;
	}
      server_sock->pipe_desc[READ] = recv_pipe;
    }
  else
    recv_pipe = server_sock->pipe_desc[READ];

  if (server_sock->pipe_desc[WRITE] == INVALID_HANDLE_VALUE)
    {
      if ((send_pipe = CreateNamedPipe (
             server_sock->send_pipe,
	     PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
	     PIPE_TYPE_BYTE | PIPE_NOWAIT,
	     1,
	     0, 0,
	     100,
	     NULL)) == INVALID_HANDLE_VALUE)
	{
	  log_printf (LOG_ERROR, "CreateNamedPipe: %s\n", SYS_ERROR);
	  return 0;
	}
      server_sock->pipe_desc[WRITE] = send_pipe;
    }
  else
    send_pipe = server_sock->pipe_desc[WRITE];

  /*
   * Now try connecting to one of these pipes. This will fail until
   * a client has been connected.
   */
  if (!ConnectNamedPipe (recv_pipe, NULL))
    {
      log_printf (LOG_ERROR, "ConnectNamedPipe: %s\n", SYS_ERROR);
      return 0;
    }

  if (!ConnectNamedPipe (send_pipe, NULL))
    {
      return 0;
    }

  /* Create a socket structure for the client pipe. */
  if ((sock = pipe_create (recv_pipe, send_pipe)) == NULL)
    {
      DisconnectNamedPipe (send_pipe);
      CloseHandle (send_pipe);
      DisconnectNamedPipe (recv_pipe);
      CloseHandle (recv_pipe);
      return 0;
    }

  sock->read_socket = pipe_read;
  sock->write_socket = pipe_write;
  sock->flags |= SOCK_FLAG_PIPE;
  sock->parent = server_sock;
  sock->data = server_sock->data;
  sock->check_request = server_sock->check_request;
  sock->disconnected_socket = server_sock->disconnected_socket;
  sock->idle_func = default_idle_func; 
  sock->idle_counter = 1;
  sock_enqueue (sock);

  server_sock->flags |= SOCK_FLAG_INITED;

  log_printf (LOG_NOTICE, "%s: accepting client on pipe (%d-%d)\n",
              server_sock->send_pipe, 
	      sock->pipe_desc[READ], sock->pipe_desc[WRITE]);

#else /* not __MINGW32__ */

  return 0;

#endif /* not HAVE_MKFIFO */
  
  return -1;
}
