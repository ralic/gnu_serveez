/*
 * udp-socket.c - udp socket implementations
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
 * $Id: udp-socket.c,v 1.10 2000/10/28 13:03:11 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "socket.h"
#include "alloc.h"
#include "util.h"
#include "snprintf.h"
#include "server.h"
#include "server-core.h"
#include "udp-socket.h"

/*
 * This routine is the default reader for udp sockets. Whenever the socket
 * descriptor is selected for reading it is called by default and reads as
 * much data as possible (whole packets only) and saves the sender into
 * the remote_addr field. Packet data is written into recv_buffer.
 */
int
udp_read_socket (socket_t sock)
{
  int do_read, num_read;
  socklen_t len;
  struct sockaddr_in sender;

  len = sizeof (struct sockaddr_in);
  do_read = sock->recv_buffer_size - sock->recv_buffer_fill;
  if (do_read <= 0)
    {
      log_printf (LOG_ERROR, "receive buffer overflow on udp socket %d\n",
		  sock->sock_desc);
      return -1;
    }
  
  num_read = recvfrom (sock->sock_desc,
		       sock->recv_buffer + sock->recv_buffer_fill,
		       do_read, 0, (struct sockaddr *) &sender, &len);

  /* Valid packet data arrived. */
  if (num_read > 0)
    {
#if 0
      util_hexdump (stdout, "udp packet received", sock->sock_desc,
		    sock->recv_buffer + sock->recv_buffer_fill,
		    num_read, 0);
#endif
      sock->last_recv = time (NULL);
      sock->recv_buffer_fill += num_read;
      if (!(sock->flags & SOCK_FLAG_FIXED))
	{
	  sock->remote_port = sender.sin_port;
	  sock->remote_addr = sender.sin_addr.s_addr;
	}
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "udp: recvfrom: %s:%u (%d bytes)\n",
		  util_inet_ntoa (sock->remote_addr),
		  ntohs (sock->remote_port), num_read);
#endif
      if (sock->check_request)
        sock->check_request (sock);
    }
  /* Some error occured. */
  else
    {
      log_printf (LOG_ERROR, "udp: recvfrom: %s\n", NET_ERROR);
      if (last_errno != SOCK_UNAVAILABLE)
	return -1;
    }
  return 0;
}

/*
 * The udp_write_socket() callback should be called whenever the udp socket
 * descriptor is ready for sending. It sends a single packet within the
 * send_buffer to the destination address specified by remote_addr and
 * remote_port.
 */
int
udp_write_socket (socket_t sock)
{
  int num_written;
  unsigned do_write;
  char *p;
  socklen_t len;
  struct sockaddr_in receiver;

  /* return here if there is nothing to send */
  if (sock->send_buffer_fill <= 0)
    return 0;

  len = sizeof (struct sockaddr_in);
  receiver.sin_family = AF_INET;

  /* get dest address, port and data length from buffer */
  p = sock->send_buffer;
  memcpy (&do_write, p, sizeof (do_write));
  p += sizeof (do_write);
  memcpy (&receiver.sin_addr.s_addr, p, sizeof (sock->remote_addr));
  p += sizeof (sock->remote_addr);
  memcpy (&receiver.sin_port, p, sizeof (sock->remote_port));
  p += sizeof (sock->remote_port);

  num_written = sendto (sock->sock_desc, p, 
			do_write - (p - sock->send_buffer),
			0, (struct sockaddr *) &receiver, len);
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "udp: sendto: %s:%u (%u bytes)\n",
	      util_inet_ntoa (receiver.sin_addr.s_addr),
	      ntohs (receiver.sin_port),
	      do_write - (p - sock->send_buffer));
#endif  

  /* Some error occured while sending. */
  if (num_written < 0)
    {
      log_printf (LOG_ERROR, "udp: sendto: %s\n", NET_ERROR);
      if (last_errno != SOCK_UNAVAILABLE)
	return -1;
    }
  /* Packet data could be transmitted. */
  else
    {
      sock->last_send = time (NULL);
      if ((unsigned) sock->send_buffer_fill > do_write)
        {
          memmove (sock->send_buffer, 
                   sock->send_buffer + do_write,
                   sock->send_buffer_fill - do_write);
        }
      sock->send_buffer_fill -= do_write;
    }
  return 0;
}

/*
 * This is the default check_request routine for udp servers. Whenever
 * new data arrived at an udp server socket we call this function to
 * process the packet data. Any given handle_request callback MUST return
 * zero if it successfully processed the data and non-zero if it could
 * not.
 */
int
udp_check_request (socket_t sock)
{
  int n;
  server_t *server;

  if (sock->data == NULL && sock->handle_request == NULL)
    return -1;

  /* 
   * If there is a valid handle_request callback (dedicated udp connection)
   * call it. This kind of behaviour is due to a socket creation via
   * udp_connect (s.b.) and setting up a static handle_request callback.
   */
  if (sock->handle_request)
    {
      if (sock->handle_request (sock, sock->recv_buffer,
				sock->recv_buffer_fill))
	return -1;
      sock->recv_buffer_fill = 0;
      return 0;
    }

  /* go through all udp servers on this server socket */
  for (n = 0; (server = SERVER (sock->data, n)); n++)
    {
      sock->cfg = server->cfg;
      
      if (server->handle_request)
	{
	  if (!server->handle_request (sock, sock->recv_buffer,
				       sock->recv_buffer_fill))
	    {
	      sock->recv_buffer_fill = 0;
	      break;
	    }
        }
    }

  /* check if any server processed this packet */
  if (sock->recv_buffer_fill)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "rejecting udp packet on socket %d\n",
		  sock->sock_desc);
#endif
      sock->recv_buffer_fill = 0;
    }

  sock->cfg = NULL;
  return 0;
}

/*
 * Write the given BUF into the send queue of the UDP socket. If the
 * length argument supersedes the maximum length for UDP messages it
 * is splitted into smaller blocks.
 */
int
udp_write (socket_t sock, char *buf, int length)
{
  char *buffer;
  unsigned len, size;
  int ret = 0;

  /* return if the socket has already been killed */
  if (sock->flags & SOCK_FLAG_KILLED)
    return 0;

  /* allocate memory block */
  buffer = xmalloc ((length > UDP_MSG_SIZE ? UDP_MSG_SIZE : length) + 
		    sizeof (len) + sizeof (sock->remote_addr) +
		    sizeof (sock->remote_port));

  while (length)
    {
      /* 
       * Put the data length, destination address and port in front
       * of each data packet.
       */
      len = sizeof (len);
      memcpy (&buffer[len], &sock->remote_addr, sizeof (sock->remote_addr));
      len += sizeof (sock->remote_addr);
      memcpy (&buffer[len], &sock->remote_port, sizeof (sock->remote_port));
      len += sizeof (sock->remote_port);

      /* copy the given buffer */
      if ((size = length) > UDP_MSG_SIZE) size = UDP_MSG_SIZE;
      memcpy (&buffer[len], buf, size);
      len += size;
      memcpy (buffer, &len, sizeof (len));
      buf += size;
      length -= size;

      /* actually send the data or put it into the send buffer queue */
      if ((ret = sock_write (sock, buffer, len)) == -1)
	{
	  sock->flags |= SOCK_FLAG_KILLED;
	  break;
	}
    }

  /* release memory block */
  xfree (buffer);
  return ret;
}

/*
 * Print a formatted string on the udp socket SOCK. FMT is the printf()-
 * style format string, which describes how to format the optional
 * arguments. See the printf(3) manual page for details. The destination
 * address and port is saved for sending. This you might specify them
 * in sock->remote_addr and sock->remote_port.
 */
int
udp_printf (socket_t sock, const char *fmt, ...)
{
  va_list args;
  static char buffer[VSNPRINTF_BUF_SIZE];
  int len;

  if (sock->flags & SOCK_FLAG_KILLED)
    return 0;

  va_start (args, fmt);
  len = vsnprintf (buffer, VSNPRINTF_BUF_SIZE, fmt, args);
  va_end (args);

  return udp_write (sock, buffer, len);
}

/*
 * Create a UDP connection to HOST and set the socket descriptor in
 * structure SOCK to the resulting socket. Return a NULL value on errors.
 * ***
 * This function can be used for port bouncing. If you assign the
 * `handle_request' callback to something server specific and the `cfg'
 * field to the server's configuration to the returned socket structure this
 * socket is able to handle a dedicated udp connection to some other
 * udp server.
 */
socket_t
udp_connect (unsigned long host, unsigned short port)
{
  struct sockaddr_in client;
  SOCKET sockfd;
  socket_t sock;
#ifdef __MINGW32__
  unsigned long blockMode = 1;
#endif

  /* create a socket for communication with the server */
  if ((sockfd = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
    {
      log_printf (LOG_ERROR, "socket: %s\n", NET_ERROR);
      return NULL;
    }

  /* make the socket non-blocking */
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
  
  /* try to connect to the server */
  client.sin_family = AF_INET;
  client.sin_addr.s_addr = host;
  client.sin_port = port;
  
  if (connect (sockfd, (struct sockaddr *) &client,
	       sizeof (client)) == -1)
    {
      log_printf (LOG_ERROR, "connect: %s\n", NET_ERROR);
      closesocket (sockfd);
      return NULL;
    }

  /* create socket structure and enqueue it */
  if ((sock = sock_alloc ()) == NULL)
    {
      closesocket (sockfd);
      return NULL;
    }

  sock_resize_buffers (sock, UDP_BUF_SIZE, UDP_BUF_SIZE);
  sock_unique_id (sock);
  sock->sock_desc = sockfd;
  sock->flags |= (SOCK_FLAG_SOCK | SOCK_FLAG_CONNECTED | SOCK_FLAG_FIXED);
  sock_enqueue (sock);
  sock_intern_connection_info (sock);

  sock->read_socket = udp_read_socket;
  sock->write_socket = udp_write_socket;
  sock->check_request = udp_check_request;

  connected_sockets++;
  return sock;
}
