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
 * $Id: connect.c,v 1.4 2000/07/07 16:26:20 ela Exp $
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

#ifdef __MINGW32__
# include <winsock.h>
#endif

#ifndef __MINGW32__
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif

#include "alloc.h"
#include "util.h"
#include "socket.h"
#include "server-core.h"

/*
 * Create a TCP connection to host HOST and set the socket descriptor in
 * structure SOCK to the resulting socket. Return a zero value on error.
 */
socket_t
sock_connect (unsigned host, int port)
{
  struct sockaddr_in server;
  SOCKET client_socket;
  socket_t sock;
#ifdef __MINGW32__
  unsigned long blockMode = 1;
#endif

  /*
   * first, create a socket for communication with the server
   */
  if ((client_socket = socket (AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
      log_printf (LOG_ERROR, "socket: %s\n", NET_ERROR);
      return NULL;
    }

  /*
   * second, make the socket non-blocking
   */
#ifdef __MINGW32__
  if (ioctlsocket (client_socket, FIONBIO, &blockMode) == SOCKET_ERROR)
    {
      log_printf (LOG_ERROR, "ioctlsocket: %s\n", NET_ERROR);
      return NULL;
    }
#else
  if (fcntl (client_socket, F_SETFL, O_NONBLOCK) < 0)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", NET_ERROR);
      return NULL;
    }
#endif
  
  /*
   * create socket structure and enqueue it
   */
  if (!(sock = sock_alloc ()))
    return NULL;

  sock_unique_id (sock);
  sock->sock_desc = client_socket;
  sock->flags |= SOCK_FLAG_SOCK;
  sock_enqueue (sock);

  /*
   * third, try to connect to the server
   */
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = host;
  server.sin_port = port;
  
  if (connect (client_socket, (struct sockaddr *) &server,
	       sizeof (server)) == -1)
    {
      log_printf (LOG_NOTICE, "irc: connect: %s\n", NET_ERROR);
    }
  sock->flags |= SOCK_FLAG_CONNECTING;
  sock->write_socket = default_connect;

  return sock;
}
