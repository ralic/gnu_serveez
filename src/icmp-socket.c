/*
 * icmp-socket.c - Internet Control Message Protocol socket implementations
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
 * $Id: icmp-socket.c,v 1.2 2000/10/01 11:11:20 ela Exp $
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
#include "util.h"
#include "snprintf.h"
#include "server.h"
#include "server-core.h"
#include "icmp-socket.h"

/* Static buffer for ip packets. */
static char icmp_buffer[ICMP_BUFFER_SIZE];

/*
 * Get IP header from plain data.
 */
ip_header_t *
icmp_get_ip_header (byte *data)
{
  static ip_header_t hdr;
  unsigned short uint16;
  unsigned int uint32;

  hdr.version_length = *data++;
  hdr.unused = *data++;
  memcpy (&uint16, data, SIZEOF_UINT16);
  hdr.length = uint16;
  data += SIZEOF_UINT16;
  memcpy (&uint16, data, SIZEOF_UINT16);
  hdr.id = uint16;
  data += SIZEOF_UINT16;
  hdr.flags = *data++;
  memcpy (&uint16, data, SIZEOF_UINT16);
  hdr.frag_offset = uint16;
  data += SIZEOF_UINT16;
  hdr.ttl = *data++;
  hdr.protocol = *data++;
  memcpy (&uint16, data, SIZEOF_UINT16);
  hdr.checksum = uint16;
  data += SIZEOF_UINT16;
  memcpy (&uint32, data, SIZEOF_UINT32);
  hdr.src = uint32;
  data += SIZEOF_UINT32;
  memcpy (&uint32, data, SIZEOF_UINT32);
  hdr.dst = uint32;

  return &hdr;
}

/*
 * Parse and check IP and ICMP header.
 */
static int
icmp_check_packet (byte *data)
{
  ip_header_t *ip_header;

  ip_header = icmp_get_ip_header (data);
  return 0;
}

/*
 * Default reader for icmp sockets.
 */
int
icmp_read_socket (socket_t sock)
{
  int do_read, num_read;
  socklen_t len;
  struct sockaddr_in sender;

  len = sizeof (struct sockaddr_in);
  do_read = sock->recv_buffer_size - sock->recv_buffer_fill;
  if (do_read <= 0)
    {
      log_printf (LOG_ERROR, "receive buffer overflow on icmp socket %d\n",
		  sock->sock_desc);
      return -1;
    }
  
  num_read = recvfrom (sock->sock_desc, icmp_buffer, ICMP_BUFFER_SIZE, 
		       0, (struct sockaddr *) &sender, &len);

  /* Valid packet data arrived. */
  if (num_read > 0)
    {
#if 1
      util_hexdump (stdout, "icmp received", sock->sock_desc,
		    icmp_buffer, num_read, 0);
#endif
      sock->last_recv = time (NULL);
      sock->remote_port = sender.sin_port;
      sock->remote_addr = sender.sin_addr.s_addr;
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "icmp packet received from %s:%u\n",
		  util_inet_ntoa (sock->remote_addr),
		  ntohs (sock->remote_port));
#endif

      /* Check ICMP packet and put packet load only to receive buffer. */
      icmp_check_packet ((byte *) icmp_buffer);

      if (sock->check_request)
        sock->check_request (sock);
    }
  /* Some error occured. */
  else
    {
      log_printf (LOG_ERROR, "icmp read: %s\n", NET_ERROR);
      if (last_errno != SOCK_UNAVAILABLE)
	return -1;
    }
  return 0;
}
