/*
 * tcp-socket.c - TCP socket connection implementation
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
 * $Id: tcp-socket.c,v 1.2 2001/02/02 11:26:24 ela Exp $
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

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif

#include "libserveez/util.h"
#include "libserveez/socket.h"
#include "libserveez/server-core.h"
#include "libserveez/tcp-socket.h"

/*
 * Default function for writing to socket SOCK. Simply flushes the output 
 * buffer to the network. Write as much as possible into the socket SOCK.
 * Writing is performed non-blocking, so only as much as fits into the 
 * network buffer will be written on each call.
 */
int
tcp_write_socket (socket_t sock)
{
  int num_written;
  int do_write;
  SOCKET desc;

  desc = sock->sock_desc;

  /* 
   * Write as many bytes as possible, remember how many were actually 
   * sent. Limit the maximum sent bytes to SOCK_MAX_WRITE.
   */
  do_write = sock->send_buffer_fill;
  if (do_write > SOCK_MAX_WRITE)
    do_write = SOCK_MAX_WRITE;
  num_written = send (desc, sock->send_buffer, do_write, 0);

  /* Some data has been written. */
  if (num_written > 0)
    {
      sock->last_send = time (NULL);

      /*
       * Shuffle the data in the output buffer around, so that
       * new data can get stuffed into it.
       */
      sock_reduce_send (sock, num_written);
    }
  /* Error occurred while sending. */
  else if (num_written < 0)
    {
      log_printf (LOG_ERROR, "tcp: send: %s\n", NET_ERROR);
      if (svz_errno == SOCK_UNAVAILABLE)
	{
	  sock->unavailable = time (NULL) + RELAX_FD_TIME;
	  num_written = 0;
	}
    }

  /* If final write flag is set, then schedule for shutdown. */
  if (sock->flags & SOCK_FLAG_FINAL_WRITE && sock->send_buffer_fill == 0)
    num_written = -1;

  /* Return a non-zero value if an error occurred. */
  return (num_written < 0) ? -1 : 0;
}

/*
 * Default function for reading from the socket SOCK. This function only 
 * reads all data from the socket and calls the CHECK_REQUEST function for 
 * the socket, if set. Returns -1 if the socket has died, returns zero 
 * otherwise.
 */
int
tcp_read_socket (socket_t sock)
{
  int num_read;
  int ret;
  int do_read;
  SOCKET desc;

  desc = sock->sock_desc;

  /* 
   * Calculate how many bytes fit into the receive buffer.
   */
  do_read = sock->recv_buffer_size - sock->recv_buffer_fill;

  /*
   * Check if enough space is left in the buffer, kick the socket
   * if not. The main loop will kill the socket if we return a non-zero
   * value.
   */
  if (do_read <= 0)
    {
      log_printf (LOG_ERROR, "receive buffer overflow on socket %d\n", desc);
      if (sock->kicked_socket)
	sock->kicked_socket (sock, 0);
      return -1;
    }

  /*
   * Try to read as much data as possible.
   */
  num_read = recv (desc,
		   sock->recv_buffer + sock->recv_buffer_fill, do_read, 0);

  /* Error occurred while reading. */
  if (num_read < 0)
    {
      /*
       * This means that the socket was shut down. Close the socket in this 
       * case, which the main loop will do for us if we return a non-zero 
       * value.
       */
      log_printf (LOG_ERROR, "tcp: recv: %s\n", NET_ERROR);
      if (svz_errno == SOCK_UNAVAILABLE)
	num_read = 0;
      else
	return -1;
    }
  /* Some data has been read successfully. */
  else if (num_read > 0)
    {
      sock->last_recv = time (NULL);

#if ENABLE_FLOOD_PROTECTION
      if (sock_flood_protect (sock, num_read))
	{
	  log_printf (LOG_ERROR, "kicked socket %d (flood)\n", desc);
	  return -1;
	}
#endif /* ENABLE_FLOOD_PROTECTION */

      sock->recv_buffer_fill += num_read;

      if (sock->check_request)
	{
	  if ((ret = sock->check_request (sock)) != 0)
	    return ret;
	}
    }
  /* The socket was `select ()'ed but there is no data. */
  else
    {
      log_printf (LOG_ERROR, "tcp: recv: no data on socket %d\n", desc);
      return -1;
    }
  
  return 0;
}

/*
 * Create a TCP connection to host HOST and set the socket descriptor in
 * structure SOCK to the resulting socket. Return a zero value on error.
 */
socket_t
tcp_connect (unsigned long host, unsigned short port)
{
  struct sockaddr_in client;
  SOCKET sockfd;
  socket_t sock;
  int error;
#ifdef __MINGW32__
  unsigned long blockMode = 1;
#endif

  /*
   * first, create a socket for communication with the host
   */
  if ((sockfd = socket (AF_INET, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET)
    {
      log_printf (LOG_ERROR, "socket: %s\n", NET_ERROR);
      return NULL;
    }

  /*
   * second, make the socket non-blocking
   */
#ifdef __MINGW32__
  if (ioctlsocket (sockfd, FIONBIO, &blockMode) == SOCKET_ERROR)
    {
      log_printf (LOG_ERROR, "ioctlsocket: %s\n", NET_ERROR);
      closesocket (sockfd);
      return NULL;
    }
#else
  if (fcntl (sockfd, F_SETFL, O_NONBLOCK) < 0)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", NET_ERROR);
      closesocket (sockfd);
      return NULL;
    }
#endif
  
  /*
   * third, try to connect to the host
   */
  client.sin_family = AF_INET;
  client.sin_addr.s_addr = host;
  client.sin_port = port;
  
  if (connect (sockfd, (struct sockaddr *) &client, sizeof (client)) == -1)
    {
#ifdef __MINGW32__
      error = WSAGetLastError ();
#else
      error = errno;
#endif
      if (error != SOCK_INPROGRESS && error != SOCK_UNAVAILABLE)
	{
	  log_printf (LOG_ERROR, "connect: %s\n", NET_ERROR);
	  closesocket (sockfd);
	  return NULL;
	}
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "connect: %s\n", NET_ERROR);
#endif
    }

  /*
   * create socket structure and enqueue it
   */
  if ((sock = sock_alloc ()) == NULL)
    {
      closesocket (sockfd);
      return NULL;
    }

  sock_unique_id (sock);
  sock->sock_desc = sockfd;
  sock->flags |= (SOCK_FLAG_SOCK | SOCK_FLAG_CONNECTING);
  sock->connected_socket = tcp_default_connect;
  sock_enqueue (sock);

  return sock;
}

/*
 * The default routine for connecting a socket SOCK. When we get 
 * `select ()'ed or `poll ()'ed via the WRITE_SET we simply check for 
 * network errors,
 */
int
tcp_default_connect (socket_t sock)
{
  int error;
  socklen_t optlen = sizeof (int);

  /* check if the socket has been finally connected */
  if (getsockopt (sock->sock_desc, SOL_SOCKET, SO_ERROR,
		  (void *) &error, &optlen) < 0)
    {
      log_printf (LOG_ERROR, "getsockopt: %s\n", NET_ERROR);
      return -1;
    }

  /* any errors ? */
  if (error)
    {
#ifdef __MINGW32__
      WSASetLastError (error);
#else
      errno = error;
#endif
      if (error != SOCK_INPROGRESS && error != SOCK_UNAVAILABLE)
	{
	  log_printf (LOG_ERROR, "connect: %s\n", NET_ERROR);
	  return -1;
	}
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "connect: %s\n", NET_ERROR);
#endif
      return 0;
    }

  /* successfully connected */
  sock->flags |= SOCK_FLAG_CONNECTED;
  sock->flags &= ~SOCK_FLAG_CONNECTING;
  sock_intern_connection_info (sock);
  sock_connections++;

  return 0;
}
