/*
 * server-socket.c - server sockets for TCP, UDP, ICMP and pipes
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: server-socket.c,v 1.4 2001/02/28 21:51:19 raimi Exp $
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
# include <winsock2.h>
# if HAVE_WS2TCPIP_H
#  include <ws2tcpip.h>
# endif
#endif

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif

#include "libserveez/boot.h"
#include "libserveez/util.h"
#include "libserveez/alloc.h"
#include "libserveez/socket.h"
#include "libserveez/pipe-socket.h"
#include "libserveez/udp-socket.h"
#include "libserveez/icmp-socket.h"
#include "libserveez/server-core.h"
#include "libserveez/server.h"
#include "libserveez/server-socket.h"

/*
 * Create a listening server socket. PORTCFG is the port to bind the 
 * server socket to. Return a null pointer on errors.
 */
socket_t
server_create (portcfg_t *cfg)
{
  SOCKET server_socket;      /* server socket descriptor */
  socket_t sock;             /* socket structure */
  int stype;                 /* socket type (STREAM or DGRAM or RAW) */
  int optval;                /* value for setsockopt() */
  int ptype;                 /* protocol type (IP or UDP or ICMP) */

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

  /* Create listening TCP, UDP, ICMP or RAW server socket. */
  else
    {

      /* Assign the appropriate socket type. */
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
	  break;
	  /* This protocol is for sending packets only. The kernel filters
	     any received packets by the socket protocol (here: IPPROTO_RAW
	     which is unspecified). */
	case PROTO_RAW:
	  stype = SOCK_RAW;
	  ptype = IPPROTO_RAW;
	  break;
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

      /* Set this ip option if we are using raw sockets. */
      if (cfg->proto & PROTO_RAW)
	{
#ifdef IP_HDRINCL
	  optval = 1;
	  if (setsockopt (server_socket, IPPROTO_IP, IP_HDRINCL,
			  (void *) &optval, sizeof (optval)) < 0)
	    {
	      log_printf (LOG_ERROR, "setsockopt: %s\n", NET_ERROR);
	      if (closesocket (server_socket) < 0)
		log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	      return NULL;
	    }
#else /* not IP_HDRINCL */
	  closesocket (server_socket);
	  log_printf (LOG_ERROR, "setsockopt: IP_HDRINCL undefined\n");
	  return NULL;
#endif /* IP_HDRINCL */
	}

      /* 
       * Make the socket be reusable (Minimize socket deadtime on 
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

      /* Second, bind the socket to a port. */
      if (bind (server_socket, (struct sockaddr *) cfg->addr, 
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
      svz_free (sock->recv_buffer);
      svz_free (sock->send_buffer);
      sock->recv_buffer_size = 0;
      sock->send_buffer_size = 0;
      sock->recv_buffer = NULL;
      sock->send_buffer = NULL;
      sock->check_request = sock_detect_proto; 
    }

  /* Setup the socket structure. */
  sock->flags |= SOCK_FLAG_LISTENING;
  sock->flags &= ~SOCK_FLAG_CONNECTED;
  sock->proto |= cfg->proto;

  if (cfg->proto & PROTO_PIPE)
    {
#ifndef __MINGW32__
      sock->recv_pipe = svz_malloc (strlen (cfg->inpipe) + 1);
      strcpy (sock->recv_pipe, cfg->inpipe);
      sock->send_pipe = svz_malloc (strlen (cfg->outpipe) + 1);
      strcpy (sock->send_pipe, cfg->outpipe);
#else /* __MINGW32__ */
      sock->recv_pipe = svz_malloc (strlen (cfg->inpipe) + 10);
      sprintf (sock->recv_pipe, "\\\\.\\pipe\\%s", cfg->inpipe);
      sock->send_pipe = svz_malloc (strlen (cfg->outpipe) + 10);
      sprintf (sock->send_pipe, "\\\\.\\pipe\\%s", cfg->outpipe);
#endif /* __MINGW32__ */
      sock->read_socket = server_accept_pipe;
      if (pipe_listener (sock) == -1)
	{
	  sock_free (sock);
	  return NULL;
	}
      log_printf (LOG_NOTICE, "listening on pipe %s\n", sock->recv_pipe);
    }
  else
    {
      char *proto = "unknown";

      if (cfg->proto & PROTO_TCP)
	{
	  sock->read_socket = server_accept_socket;
	  proto = "tcp";
	}
      else if (cfg->proto & PROTO_UDP)
	{
	  sock_resize_buffers (sock, UDP_BUF_SIZE, UDP_BUF_SIZE);
	  sock->read_socket = udp_read_socket;
	  sock->write_socket = udp_write_socket;
	  sock->check_request = udp_check_request;
	  proto = "udp";
	}
      else if (cfg->proto & PROTO_ICMP)
	{
	  sock_resize_buffers (sock, ICMP_BUF_SIZE, ICMP_BUF_SIZE);
	  sock->read_socket = icmp_read_socket;
	  sock->write_socket = icmp_write_socket;
	  sock->check_request = icmp_check_request;
	  proto = "icmp";
	}

      log_printf (LOG_NOTICE, "listening on %s port %s:%u\n", proto,
		  cfg->addr->sin_addr.s_addr == INADDR_ANY ? "*" : 
		  util_inet_ntoa (cfg->addr->sin_addr.s_addr),
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

  if (sock_connections >= svz_config.max_sockets)
    {
      log_printf (LOG_WARNING, "socket descriptor exceeds "
		  "socket limit %d\n", svz_config.max_sockets);
      if (closesocket (client_socket) < 0)
	{
	  log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);
	}
      return 0;
    }

  log_printf (LOG_NOTICE, "TCP:%u: accepting client on socket %d\n", 
	      ntohs (server_sock->local_port), client_socket);
	  
  /* 
   * Sanity check. Just to be sure that we always handle
   * correctly connects/disconnects.
   */
  sock = sock_root;
  while (sock && sock->sock_desc != client_socket)
    sock = sock->next;
  if (sock)
    {
      log_printf (LOG_FATAL, "socket %d already in use\n", sock->sock_desc);
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
  if ((sock = sock_create (client_socket)) != NULL)
    {
      sock->flags |= SOCK_FLAG_CONNECTED;
      sock->data = server_sock->data;
      sock->check_request = server_sock->check_request;
      sock->idle_func = sock_idle_protect; 
      sock->idle_counter = 1;
      sock_enqueue (sock);
      sock_connections++;

      /* 
       * We call the check_request() routine here once in order to
       * allow "greedy" protocols (always returning success 
       * in the detect_proto() routine) to get their connection without
       * sending anything.
       */
      if (sock->check_request)
	if (sock->check_request (sock))
	  sock_schedule_for_shutdown (sock);
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
#ifdef __MINGW32__
  DWORD connect;
#endif

#if defined (HAVE_MKFIFO) || defined (HAVE_MKNOD) || defined (__MINGW32__)
  HANDLE recv_pipe, send_pipe;
  socket_t sock;
  server_sock->idle_counter = 1;
#endif

#if HAVE_MKFIFO || HAVE_MKNOD
  /* 
   * Try opening the server's send pipe. This will fail 
   * until the client has opened it for reading.
   */
  send_pipe = open (server_sock->send_pipe, O_NONBLOCK | O_WRONLY);
  if (send_pipe == -1)
    {
      if (errno != ENXIO)
	{
	  log_printf (LOG_ERROR, "open: %s\n", SYS_ERROR);
	  return -1;
	}
      return 0;
    }
  recv_pipe = server_sock->pipe_desc[READ];

  /* Create a socket structure for the client pipe. */
  if ((sock = pipe_create (recv_pipe, send_pipe)) == NULL)
    {
      close (send_pipe);
      return 0;
    }

#elif defined (__MINGW32__) /* not HAVE_MKFIFO */

  recv_pipe = server_sock->pipe_desc[READ];
  send_pipe = server_sock->pipe_desc[WRITE];

  /*
   * Now try connecting to one of these pipes. This will fail until
   * a client has been connected.
   */
  if (!ConnectNamedPipe (send_pipe, server_sock->overlap[WRITE]))
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
  else
    return 0;

  if (!ConnectNamedPipe (recv_pipe, server_sock->overlap[READ]))
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
  /* Because these pipes are non-blocking this is never occurring. */
  else
    return 0;

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

  /* Copy overlapped structures to client pipes. */
  if (svz_os_version >= WinNT4x)
    {
      sock->overlap[READ] = server_sock->overlap[READ];
      sock->overlap[WRITE] = server_sock->overlap[WRITE];
    }

#else /* not __MINGW32__ */

  return -1;

#endif /* neither HAVE_MKFIFO nor __MINGW32__ */

#if defined (HAVE_MKFIFO) || defined (HAVE_MKNOD) || defined (__MINGW32__)
  sock->read_socket = pipe_read_socket;
  sock->write_socket = pipe_write_socket;
  sock->referrer = server_sock;
  sock->data = server_sock->data;
  sock->check_request = server_sock->check_request;
  sock->disconnected_socket = server_sock->disconnected_socket;
  sock->idle_func = sock_idle_protect;
  sock->idle_counter = 1;
  sock_enqueue (sock);

  log_printf (LOG_NOTICE, "%s: accepting client on pipe (%d-%d)\n",
              server_sock->recv_pipe, 
	      sock->pipe_desc[READ], sock->pipe_desc[WRITE]);

  server_sock->flags |= SOCK_FLAG_INITED;
  server_sock->referrer = sock;

  /* Call the check_request() routine once for greedy protocols. */
  if (sock->check_request)
    if (sock->check_request (sock))
      sock_schedule_for_shutdown (sock);

  return 0;
#endif /* HAVE_MKFIFO or __MINGW32__ */
}
