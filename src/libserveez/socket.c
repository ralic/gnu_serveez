/*
 * socket.c - socket management implementation
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: socket.c,v 1.8 2001/05/05 15:45:51 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif

#include "libserveez/snprintf.h"
#include "libserveez/alloc.h"
#include "libserveez/util.h"
#include "libserveez/socket.h"
#include "libserveez/core.h"
#include "libserveez/pipe-socket.h"
#include "libserveez/tcp-socket.h"
#include "libserveez/server-core.h"
#include "libserveez/server.h"

/*
 * Count the number of currently connected sockets.
 */
SOCKET sock_connections = 0;

#if ENABLE_FLOOD_PROTECTION
/*
 * This routine can be called if flood protection is wished for
 * socket readers. Return non-zero if the socket should be kicked
 * because of flood.
 */
int
sock_flood_protect (socket_t sock, int num_read)
{
  if (!(sock->flags & SOCK_FLAG_NOFLOOD))
    {
      /* 
       * Since the default flood limit is 100 a reader can produce 
       * 5000 bytes per second before it gets kicked.
       */
      sock->flood_points += 1 + (num_read / 50);
	  
      if (sock->flood_points > sock->flood_limit)
	{
	  if (sock->kicked_socket)
	    sock->kicked_socket (sock, 0);
	  return -1;
	}
    }
  return 0;
}
#endif /* ENABLE_FLOOD_PROTECTION */

/*
 * The default function which gets called when a client shuts down
 * its socket. SOCK is the socket which was closed.
 */
static int
sock_default_disconnect (socket_t sock)
{
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "socket id %d disconnected\n", sock->id);
#endif

  return 0;
}

/*
 * This routine gets called whenever data is read from a client socket
 * accepted by any connection oriented protocol layer (TCP or PIPE). We
 * try to detect the data streams protocol here.
 */
int
sock_detect_proto (socket_t sock)
{
  int n;
  svz_server_t *server;
  svz_portcfg_t *port;

  /* return if there are no servers bound to this socket */
  if (sock->data == NULL)
    return -1;

  /* get port configuration of parent */
  port = sock_portcfg (sock);

  /* go through each server stored in the data field of this socket */
  svz_array_foreach (sock->data, server, n)
    {
      /* call protocol detection routine of the server */
      if (server->detect_proto (server->cfg, sock))
	{
	  sock->idle_func = NULL;
	  sock->data = NULL;
	  sock->cfg = server->cfg;
	  if (!server->connect_socket)
	    return -1;
	  if (server->connect_socket (server->cfg, sock))
	    return -1;
	  return sock->check_request (sock);
	}
    }

  /*
   * Discard this socket if there were not any valid protocol
   * detected and its receive buffer fill exceeds a maximum value.
   */
  if (sock->recv_buffer_fill > port->detection_fill)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "socket id %d detection failed\n", sock->id);
#endif
      return -1;
    }

  return 0;
}

/*
 * Default idle function. This routine simply checks for "dead" 
 * (non-receiving) sockets (connection oriented protocols only) and rejects 
 * them by return a non-zero value.
 */
int
sock_idle_protect (socket_t sock)
{
  svz_portcfg_t *port = sock_portcfg (sock);

  if (time (NULL) - sock->last_recv > port->detection_wait)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "socket id %d detection failed\n", sock->id);
#endif
      return -1;
    }

  sock->idle_counter = 1;
  return 0;
}

/*
 * This `check_request ()' routine could be used by any protocol to 
 * detect and finally handle packets depending on a specific packet 
 * boundary. The appropriate `handle_request ()' is called for each packet
 * explicitly with the packet length inclusive the packet boundary.
 */
static int
sock_check_request_array (socket_t sock)
{
  int len = 0;
  char *p, *packet, *end;

  p = sock->recv_buffer;
  end = p + sock->recv_buffer_fill - sock->boundary_size + 1;
  packet = p;

  do
    {
      /* Find packet boundary in the receive buffer. */
      while (p < end && memcmp (p, sock->boundary, sock->boundary_size))
        p++;

      /* Found ? */
      if (p < end && !memcmp (p, sock->boundary, sock->boundary_size))
        {
          p += sock->boundary_size;
          len += (p - packet);

	  /* Call the handle request callback. */
	  if (sock->handle_request)
	    {
	      if (sock->handle_request (sock, packet, p - packet))
		return -1;
	    }
	  packet = p;
        }
    }
  while (p < end);
  
  /* Shuffle data in the receive buffer around. */
  sock_reduce_recv (sock, len);
  
  return 0;
}

/*
 * This is just the same routine as above, but optimized for one byte
 * packet delimiters.
 */
static int
sock_check_request_byte (socket_t sock)
{
  int len = 0;
  char *p, *packet, *end;

  p = sock->recv_buffer;
  end = p + sock->recv_buffer_fill;
  packet = p;

  do
    {
      /* Find packet boundary in the receive buffer. */
      while (p < end && *p != *sock->boundary)
        p++;

      /* Found ? */
      if (p < end && *p == *sock->boundary)
        {
          p++;
          len += (p - packet);

	  /* Call the handle request callback. */
	  if (sock->handle_request)
	    {
	      if (sock->handle_request (sock, packet, p - packet))
		return -1;
	    }
	  packet = p;
        }
    }
  while (p < end);
  
  /* Shuffle data in the receive buffer around. */
  sock_reduce_recv (sock, len);
  
  return 0;
}

/*
 * this function simply checks for the kind of packet delimiter within 
 * the given socket structure and and assigns one of the above
 * `check_request ()' routines (one or more byte delimiters). Afterwards 
 * this routine will never ever be called again because the callback gets
 * overwritten here.
 */
int
sock_check_request (socket_t sock)
{
  assert (sock->boundary);
  assert (sock->boundary_size);

  if (sock->boundary_size > 1)
    sock->check_request = sock_check_request_array;
  else
    sock->check_request = sock_check_request_byte;

  return sock->check_request (sock);
}

/*
 * Allocate a structure of type `socket_t' and initialize its data
 * fields. Assign some of the default callbacks for TCP connections.
 */
socket_t
sock_alloc (void)
{
  socket_t sock;
  char *in;
  char *out;

  sock = svz_malloc (sizeof (socket_data_t));
  memset (sock, 0, sizeof (socket_data_t));
  in = svz_malloc (RECV_BUF_SIZE);
  out = svz_malloc (SEND_BUF_SIZE);

  sock->proto = SOCK_FLAG_INIT;
  sock->flags = SOCK_FLAG_INIT | SOCK_FLAG_INBUF | SOCK_FLAG_OUTBUF;
  sock->userflags = SOCK_FLAG_INIT;
  sock->file_desc = -1;
  sock->sock_desc = (SOCKET) -1;
  sock->pipe_desc[READ] = INVALID_HANDLE;
  sock->pipe_desc[WRITE] = INVALID_HANDLE;

  sock->read_socket = tcp_read_socket;
  sock->write_socket = tcp_write_socket;
  sock->check_request = sock_detect_proto;
  sock->disconnected_socket = sock_default_disconnect;

  sock->recv_buffer = in;
  sock->recv_buffer_size = RECV_BUF_SIZE;
  sock->send_buffer = out;
  sock->send_buffer_size = SEND_BUF_SIZE;
  sock->last_send = time (NULL);
  sock->last_recv = time (NULL);

#if ENABLE_FLOOD_PROTECTION
  sock->flood_limit = 100;
#endif /* ENABLE_FLOOD_PROTECTION */

  return sock;
}

/*
 * Resize the send and receive buffers for the socket SOCK. SEND_BUF_SIZE
 * is the new size for the send buffer, RECV_BUF_SIZE for the receive
 * buffer. Note that data may be lost when the buffers shrink.
 */
int 
sock_resize_buffers (socket_t sock, int send_buf_size, int recv_buf_size)
{
  char *send, *recv;

  if (sock->send_buffer_size != send_buf_size)
    send = svz_realloc (sock->send_buffer, send_buf_size);
  else
    send = sock->send_buffer;
  if (sock->recv_buffer_size != recv_buf_size)
    recv = svz_realloc (sock->recv_buffer, recv_buf_size);
  else
    recv = sock->recv_buffer;

  sock->send_buffer = send;
  sock->recv_buffer = recv;
  sock->send_buffer_size = send_buf_size;
  sock->recv_buffer_size = recv_buf_size;

  return 0;
}

/*
 * Free the socket structure SOCK. Return a non-zero value on error.
 */
int
sock_free (socket_t sock)
{
  if (sock->recv_buffer)
    svz_free (sock->recv_buffer);
  if (sock->send_buffer)
    svz_free (sock->send_buffer);
  if (sock->flags & SOCK_FLAG_LISTENING)
    {
      if (sock->data)
	svz_array_destroy (sock->data);
      if (sock->cfg)
	{
	  svz_portcfg_destroy (sock->cfg);
	  svz_free (sock->cfg);
	}
    }
  if (sock->recv_pipe)
    svz_free (sock->recv_pipe);
  if (sock->send_pipe)
    svz_free (sock->send_pipe);

#ifdef __MINGW32__
  if (sock->overlap[READ])
    svz_free (sock->overlap[READ]);
  if (sock->overlap[WRITE])
    svz_free (sock->overlap[WRITE]);
#endif /* __MINGW32__ */

  svz_free (sock);

  return 0;
}

/*
 * Get local and remote addresses and ports of socket SOCK and save them
 * into the socket structure.
 */
int
sock_intern_connection_info (socket_t sock)
{
  struct sockaddr_in s;
  socklen_t size = sizeof (s);
  unsigned short port;
  unsigned long addr;

  if (!getpeername (sock->sock_desc, (struct sockaddr *) &s, &size))
    {
      addr = s.sin_addr.s_addr;
      port = s.sin_port;
    }
  else
    {
      addr = INADDR_ANY;
      port = 0;
    }
  sock->remote_port = port;
  sock->remote_addr = addr;

  size = sizeof (s);
  if (!getsockname (sock->sock_desc, (struct sockaddr *) &s, &size))
    {
      addr = s.sin_addr.s_addr;
      port = s.sin_port;
    }
  else
    {
      addr = INADDR_ANY;
      port = 0;
    }
  sock->local_port = port;
  sock->local_addr = addr;

  return 0;
}

/*
 * Get and clear the pending socket error of a given socket. Print
 * the result to the log file.
 */
int
sock_error_info (socket_t sock)
{
  int error;
  socklen_t optlen = sizeof (int);

  if (getsockopt (sock->sock_desc, SOL_SOCKET, SO_ERROR,
                  (void *) &error, &optlen) < 0)
    {
      log_printf (LOG_ERROR, "getsockopt: %s\n", NET_ERROR);
      return -1;
    }
  if (error)
    {
#ifdef __MINGW32__
      WSASetLastError (error);
#else
      errno = error;
#endif
      log_printf (LOG_ERROR, "%s\n", NET_ERROR);
      return -1;
    }
  return 0;
}

/*
 * Check if a given socket is still valid. Return non-zero if it is
 * not.
 */
int
sock_valid (socket_t sock)
{
  if (!(sock->flags & (SOCK_FLAG_LISTENING | 
		       SOCK_FLAG_CONNECTED | SOCK_FLAG_CONNECTING)))
    return -1;

  if (sock->sock_desc == INVALID_SOCKET)
    return -1;

  return 0;
}

/*
 * Create a socket structure from the file descriptor FD. Set the socket
 * descriptor to non-blocking I/O. Return NULL on errors.
 */
socket_t
sock_create (int fd)
{
  socket_t sock;

  if (svz_fd_nonblock (fd) != 0)
    return NULL;
  if (svz_fd_cloexec (fd) != 0)
    return NULL;

  if ((sock = sock_alloc ()) != NULL)
    {
      sock_unique_id (sock);
      sock->sock_desc = fd;
      sock->flags |= (SOCK_FLAG_CONNECTED | SOCK_FLAG_SOCK);
      sock_intern_connection_info (sock);
    }
  return sock;
}

/*
 * Disconnect the socket SOCK from the network and calls the disconnect 
 * function for the socket if set. Return a non-zero value on errors.
 */
int
sock_disconnect (socket_t sock)
{
  /* shutdown client connection */
  if (sock->flags & SOCK_FLAG_CONNECTED)
    {
      if (shutdown (sock->sock_desc, 2) < 0)
	log_printf (LOG_ERROR, "shutdown: %s\n", NET_ERROR);
      sock_connections--;
    }
      
  /* close the server/client socket */
  if (closesocket (sock->sock_desc) < 0)
    log_printf (LOG_ERROR, "close: %s\n", NET_ERROR);

#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "socket %d disconnected\n", sock->sock_desc);
#endif

  sock->sock_desc = INVALID_SOCKET;
  return 0;
}

/*
 * Write LEN bytes from the memory location pointed to by BUF to the 
 * output buffer of the socket SOCK. Also try to flush the buffer to the 
 * socket of SOCK if possible.  Return a non-zero value on error, which 
 * normally means a buffer overflow.
 */
int
sock_write (socket_t sock, char *buf, int len)
{
  int ret;
  int space;

  if (sock->flags & SOCK_FLAG_KILLED)
    return 0;

  while (len > 0)
    {
      /* Try to flush the queue of this socket. */
      if (sock->write_socket && !sock->unavailable && 
	  sock->flags & SOCK_FLAG_CONNECTED && sock->send_buffer_fill)
	{
	  if ((ret = sock->write_socket (sock)) != 0)
	    return ret;
	}

      if (sock->send_buffer_fill >= sock->send_buffer_size)
	{
	  /* Queue is full, unlucky socket or pipe ... */
	  if (sock->flags & SOCK_FLAG_SEND_PIPE)
	    log_printf (LOG_ERROR,
			"send buffer overflow on pipe (%d-%d) (id %d)\n",
			sock->pipe_desc[READ], sock->pipe_desc[WRITE],
			sock->id);
	  else
	    log_printf (LOG_ERROR,
			"send buffer overflow on socket %d (id %d)\n",
			sock->sock_desc, sock->id);
	
	  if (sock->kicked_socket)
	    sock->kicked_socket (sock, 1);

	  return -1;
	}
    
      /* Now move as much of BUF into the send queue. */
      if (sock->send_buffer_fill + len < sock->send_buffer_size)
	{
	  memcpy (sock->send_buffer + sock->send_buffer_fill, buf, len);
	  sock->send_buffer_fill += len;
	  len = 0;
	}
      else
	{
	  space = sock->send_buffer_size - sock->send_buffer_fill;
	  memcpy (sock->send_buffer + sock->send_buffer_fill, buf, space);
	  sock->send_buffer_fill += space;
	  len -= space;
	  buf += space;
	}
    }

  return 0;
}

/*
 * Print a formatted string on the socket SOCK. FMT is the printf()-
 * style format string, which describes how to format the optional
 * arguments. See the printf(3) manual page for details.
 */
int
sock_printf (socket_t sock, const char *fmt, ...)
{
  va_list args;
  static char buffer[VSNPRINTF_BUF_SIZE];
  unsigned len;

  if (sock->flags & SOCK_FLAG_KILLED)
    return 0;

  va_start (args, fmt);
  len = svz_vsnprintf (buffer, VSNPRINTF_BUF_SIZE, fmt, args);
  va_end (args);

  /* Just to be sure... */
  if (len > sizeof (buffer))
    len = sizeof (buffer);

  return sock_write (sock, buffer, len);
}
