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
 * $Id: udp-socket.c,v 1.1 2000/07/26 14:56:08 ela Exp $
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

#ifndef __MINGW32__
# include <sys/socket.h>
# include <netinet/in.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "socket.h"
#include "util.h"
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
  do_read = sock->recv_buffer_fill - sock->recv_buffer_fill;
  if (do_read <= 0)
    {
      log_printf (LOG_ERROR, "receive buffer overflow on udp socket %d\n",
		  sock->sock_desc);
      return 0;
    }
  
  num_read = recvfrom (sock->sock_desc,
		       sock->recv_buffer + sock->recv_buffer_fill,
		       do_read, 0, &sender, &len);

  /* Valid packet data arrived. */
  if (num_read > 0)
    {
#if 1
      util_hexdump (stdout, "udp received", sock->sock_desc,
		    sock->recv_buffer + sock->recv_buffer_fill,
		    num_read, 0);
#endif
      sock->last_recv = time (NULL);
      sock->recv_buffer_fill += num_read;
      if (sock->remote_addr != sender.sin_addr.s_addr)
	{
	  sock->remote_port = sender.sin_port;
	  sock->remote_addr = sender.sin_addr.s_addr;
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "udp packet received from %s:%d", 
		      util_inet_ntoa (sock->remote_addr), 
		      ntohs (sock->remote_port));
#endif
	}
      if (sock->check_request)
        sock->check_request (sock);
    }
  /* Some error occured. */
  else
    {
      log_printf (LOG_ERROR, "udp read: %s\n", NET_ERROR);
      if (last_errno != SOCK_UNAVAILABLE)
	return -1;
    }
  return 0;
}

/*
 * The udp_write_socket() callback should be called whenever the udp socket
 * descriptor is ready for sending. It sends all the data within the
 * send_buffer to the destination address specified by remote_addr and
 * remote_port.
 */
int
udp_write_socket (socket_t sock)
{
  int do_write, num_written;
  socklen_t len;
  struct sockaddr_in receiver;

  len = sizeof (struct sockaddr_in);
  receiver.sin_port = sock->remote_port;
  receiver.sin_addr.s_addr = sock->remote_addr;
  do_write = sock->send_buffer_fill;

  num_written = sendto (sock->sock_desc, sock->send_buffer,
			sock->send_buffer_fill,
			0, &receiver, len);
  
  /* Some error occured while sending. */
  if (num_written < 0)
    {
      log_printf (LOG_ERROR, "udp write: %s\n", NET_ERROR);
      if (last_errno != SOCK_UNAVAILABLE)
	return -1;
    }
  /* Packet data could be transmitted. */
  else
    {
      sock->last_send = time (NULL);
      if (sock->send_buffer_fill > num_written)
        {
          memmove (sock->send_buffer, 
                   sock->send_buffer + num_written,
                   sock->send_buffer_fill - num_written);
        }
      sock->send_buffer_fill -= num_written;
    }
  return 0;
}
