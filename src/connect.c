/*
 * connect.c - socket connection implementation
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
 * $Id: connect.c,v 1.14 2000/10/15 11:46:41 ela Exp $
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

#include "util.h"
#include "socket.h"
#include "server-core.h"
#include "connect.h"

/*
 * Create a TCP connection to host HOST and set the socket descriptor in
 * structure SOCK to the resulting socket. Return a zero value on error.
 */
socket_t
sock_connect (unsigned long host, unsigned short port)
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
  
  if (connect (sockfd, (struct sockaddr *) &client,
	       sizeof (client)) == -1)
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
  sock->connected_socket = default_connect;
  sock_enqueue (sock);

  return sock;
}

/*
 * The default routine for connecting a socket SOCK.
 * When we get select()ed or poll()ed via the WRITE_SET we simply check 
 * for network errors,
 */
int
default_connect (socket_t sock)
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

  sock->flags |= SOCK_FLAG_CONNECTED;
  sock->flags &= ~SOCK_FLAG_CONNECTING;
  sock_intern_connection_info (sock);
  connected_sockets++;

  return 0;
}
