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
 * $Id: icmp-socket.c,v 1.13 2000/11/12 01:48:54 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <assert.h>
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
# include <process.h>
#endif

#include "socket.h"
#include "alloc.h"
#include "util.h"
#include "snprintf.h"
#include "server.h"
#include "server-core.h"
#include "icmp-socket.h"

/* Static buffer for ip packets. */
static char icmp_buffer[IP_HEADER_SIZE + ICMP_HEADER_SIZE + ICMP_MSG_SIZE];

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
  hdr.tos = *data++;
  memcpy (&uint16, data, SIZEOF_UINT16);
  hdr.length = ntohs (uint16);
  data += SIZEOF_UINT16;
  memcpy (&uint16, data, SIZEOF_UINT16);
  hdr.ident = ntohs (uint16);
  data += SIZEOF_UINT16;
  memcpy (&uint16, data, SIZEOF_UINT16);
  hdr.frag_offset = ntohs (uint16);
  data += SIZEOF_UINT16;
  hdr.ttl = *data++;
  hdr.protocol = *data++;
  memcpy (&uint16, data, SIZEOF_UINT16);
  hdr.checksum = ntohs (uint16);
  data += SIZEOF_UINT16;
  memcpy (&uint32, data, SIZEOF_UINT32);
  hdr.src = uint32;
  data += SIZEOF_UINT32;
  memcpy (&uint32, data, SIZEOF_UINT32);
  hdr.dst = uint32;

  return &hdr;
}

/*
 * Put IP header to plain data. This is currently not in use but can be
 * used when creating raw sockets with setsockopt (SOL_IP, IP_HDRINCL).
 */
byte *
icmp_put_ip_header (ip_header_t *hdr)
{
  static byte buffer[IP_HEADER_SIZE];
  byte *data = buffer;
  unsigned short uint16;
  unsigned int uint32;

  *data++ = hdr->version_length;
  *data++ = hdr->tos;
  uint16 = htons (hdr->length);
  memcpy (data, &uint16, SIZEOF_UINT16);
  data += SIZEOF_UINT16;
  uint16 = htons (hdr->ident);
  memcpy (data, &uint16, SIZEOF_UINT16);
  data += SIZEOF_UINT16;
  uint16 = htons (hdr->frag_offset);
  memcpy (data, &uint16, SIZEOF_UINT16);
  data += SIZEOF_UINT16;
  *data++ = hdr->ttl;
  *data++ = hdr->protocol;
  uint16 = htons (hdr->checksum);
  memcpy (data, &uint16, SIZEOF_UINT16);
  data += SIZEOF_UINT16;
  uint32 = hdr->src;
  memcpy (data, &uint32, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  uint32 = hdr->dst;
  memcpy (data, &uint32, SIZEOF_UINT32);

  return buffer;
}

/*
 * Get ICMP header from plain data.
 */
static icmp_header_t *
icmp_get_header (byte *data)
{
  static icmp_header_t hdr;
  unsigned short uint16;

  hdr.type = *data++;
  hdr.code = *data++;
  memcpy (&uint16, data, SIZEOF_UINT16);
  hdr.checksum = ntohs (uint16);
  data += SIZEOF_UINT16;
  memcpy (&uint16, data, SIZEOF_UINT16);
  hdr.ident = ntohs (uint16);
  data += SIZEOF_UINT16;
  memcpy (&uint16, data, SIZEOF_UINT16);
  hdr.sequence = ntohs (uint16);
  data += SIZEOF_UINT16;
  memcpy (&uint16, data, SIZEOF_UINT16);
  hdr.port = uint16;

  return &hdr;
}

/*
 * Create ICMP header (data block) from given structure.
 */
byte *
icmp_put_header (icmp_header_t *hdr)
{
  static byte buffer[ICMP_HEADER_SIZE];
  byte *data = buffer;
  unsigned short uint16;

  *data++ = hdr->type;
  *data++ = hdr->code;
  uint16 = htons (hdr->checksum);
  memcpy (data, &uint16, SIZEOF_UINT16);
  data += SIZEOF_UINT16;
  uint16 = htons (hdr->ident);
  memcpy (data, &uint16, SIZEOF_UINT16);
  data += SIZEOF_UINT16;
  uint16 = htons (hdr->sequence);
  memcpy (data, &uint16, SIZEOF_UINT16);
  data += SIZEOF_UINT16;
  uint16 = hdr->port;
  memcpy (data, &uint16, SIZEOF_UINT16);

  return buffer;
}

/*
 * Recalculate IP header checksum.
 */
static unsigned short
icmp_ip_checksum (byte *data, int len)
{
  register unsigned checksum = 0;

  /* 
   * Calculate the 16 bit one's complement of the one's complement sum 
   * of all 16 bit words in the header. For computing the checksum, 
   * the checksum field should be zero. This checksum may be replaced in 
   * the future.
   */
  while (len > 1)
    {
      /* This is the inner loop */
      checksum += *data | (*(data + 1) << 8);
      len -= 2;
      data += 2;
    }

  /* Add left-over byte, if any */
  if (len > 0) checksum += *data;

  /* Fold 32-bit checksum to 16 bits */
  while (checksum >> 16)
    checksum = (checksum & 0xffff) + (checksum >> 16);
  checksum = ~checksum;

  return htons ((unsigned short) checksum);
}

/*
 * Checking the IP header only. Return the length of the header if it 
 * is valid, otherwise -1.
 */
static int
icmp_check_ip_header (byte *data, int len)
{
  ip_header_t *ip_header;

  /* get the IP header and reject the checksum within plain data */
  ip_header = icmp_get_ip_header (data);
  data[IP_CHECKSUM_OFS] = 0;
  data[IP_CHECKSUM_OFS + 1] = 0;

#if 0
  printf ("ip version      : %d\n"
	  "header length   : %d byte\n"
	  "type of service : %d\n"
	  "total length    : %d byte\n"
	  "ident           : 0x%04X\n"
	  "frag. offset    : %d\n"
	  "ttl             : %d\n"
	  "protocol        : 0x%02X\n"
	  "checksum        : 0x%04X\n"
	  "source          : %s\n",
	  IP_HDR_VERSION (ip_header), IP_HDR_LENGTH (ip_header),
	  ip_header->tos, ip_header->length, ip_header->ident,
	  ip_header->frag_offset, ip_header->ttl, ip_header->protocol,
	  ip_header->checksum, util_inet_ntoa (ip_header->src));
  printf ("destination     : %s\n", util_inet_ntoa (ip_header->dst));
#endif

  /* Is this IPv4 version ? */
  if (IP_HDR_VERSION (ip_header) != IP_VERSION_4)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "icmp: cannot handle IPv%d\n", 
		  IP_HDR_VERSION (ip_header));
#endif
      return -1;
    }

  /* Check Internet Header Length. */
  if (IP_HDR_LENGTH (ip_header) > len)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "icmp: invalid IHL (%d > %d)\n",
		  IP_HDR_LENGTH (ip_header), len);
#endif
      return -1;
    }

  /* Check total length. */
  if (ip_header->length < len)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "icmp: invalid total length (%d < %d)\n",
		  ip_header->length, len);
#endif
      return -1;
    }

  /* Check protocol type. */
  if (ip_header->protocol != ICMP_PROTOCOL)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "icmp: invalid protocol 0x%02X\n",
		  ip_header->protocol);
#endif
      return -1;
    }

  /* Recalculate and check the header checksum. */
  if (icmp_ip_checksum (data, IP_HDR_LENGTH (ip_header)) != 
      ip_header->checksum)
    {
      /* FIXME: Why are header checksum invalid on big packets ? */
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, 
		  "icmp: invalid ip header checksum (%04X != %04X)\n",
		  icmp_ip_checksum (data, IP_HDR_LENGTH (ip_header)),
		  ip_header->checksum);
#endif
    }

  return IP_HDR_LENGTH (ip_header);
}

/*
 * Parse and check IP and ICMP header. Return the amount of leading bytes 
 * to be truncated. Otherwise -1.
 */
static int
icmp_check_packet (socket_t sock, byte *data, int len)
{
  int length;
  byte *p = data;
  icmp_header_t *header;

  /* First check the IP header. */
  if ((length = icmp_check_ip_header (p, len)) == -1)
    return -1;

  /* Get the actual ICMP header. */
  header = icmp_get_header (p + length);
  p += length + ICMP_HEADER_SIZE;
  len -= length + ICMP_HEADER_SIZE;

  /* validate the ICMP data checksum */
  if (header->checksum != icmp_ip_checksum (p, len))
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "icmp: invalid data checksum\n");
#endif
      return -1;
    }

  /* check the ICMP header identification */
  if (header->ident == getpid () + sock->id)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "icmp: rejecting native packet\n");
#endif
      return -1;
    }

  /* check ICMP remote port */
  if ((header->port != sock->remote_port) && 
      !(sock->flags & SOCK_FLAG_LISTENING))
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "icmp: rejecting filtered packet\n");
#endif
      return -1;
    }
  sock->remote_port = header->port;

  /* What kind of packet is this ? */
  switch (header->type)
    {
    case ICMP_ECHOREPLY:
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "icmp: echo reply received\n");
#endif
      return -1;
      break;

    case ICMP_ECHO:
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "icmp: echo request received\n");
#endif
      return -1;
      break;

    case ICMP_SERVEEZ:
      if (header->code == ICMP_SERVEEZ_CONNECT && 
	  sock->flags & SOCK_FLAG_LISTENING)
	{
	  log_printf (LOG_NOTICE, "icmp: accepting connection\n");
	}
      else if (header->code == ICMP_SERVEEZ_CLOSE)
	{
	  log_printf (LOG_NOTICE, "icmp: closing connection\n");
	  return -2;
	}
      return (length + ICMP_HEADER_SIZE);
      break;

    default:
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "icmp: unsupported protocol 0x%02X\n",
		  header->type);
#endif
      return -1;
      break;
    }

  return -1;
}

/*
 * Default reader for ICMP sockets. The sender is stored within 
 * `sock->remote_addr' and `sock->remote_port'.
 */
int
icmp_read_socket (socket_t sock)
{
  int num_read;
  socklen_t len;
  struct sockaddr_in sender;
  int trunc;

  len = sizeof (struct sockaddr_in);

  /* Receive data. */
  if (!(sock->flags & SOCK_FLAG_CONNECTED))
    {
      num_read = recvfrom (sock->sock_desc, icmp_buffer, sizeof (icmp_buffer), 
			   0, (struct sockaddr *) &sender, &len);
    }
  else
    {
      num_read = recv (sock->sock_desc, icmp_buffer, sizeof (icmp_buffer), 0);
    }

  /* Valid packet data arrived. */
  if (num_read > 0)
    {
#if 0
      util_hexdump (stdout, "icmp packet received", sock->sock_desc,
		    icmp_buffer, num_read, 0);
#endif
      sock->last_recv = time (NULL);
      if (!(sock->flags & SOCK_FLAG_FIXED))
        {
	  sock->remote_addr = sender.sin_addr.s_addr;
	}
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "icmp: recvfrom: %s (%u bytes)\n",
		  util_inet_ntoa (sock->remote_addr), num_read);
#endif

      /* Check ICMP packet and put packet load only to receive buffer. */
      if ((trunc = 
	   icmp_check_packet (sock, (byte *) icmp_buffer, num_read)) >= 0)
	{
	  num_read -= trunc;
	  if (num_read > sock->recv_buffer_size - sock->recv_buffer_fill)
	    {
	      log_printf (LOG_ERROR, 
			  "receive buffer overflow on icmp socket %d\n",
			  sock->sock_desc);
	      return -1;
	    }
  
	  memcpy (sock->recv_buffer + sock->recv_buffer_fill,
		  icmp_buffer + trunc, num_read);
	  sock->recv_buffer_fill += num_read;

	  if (sock->check_request)
	    sock->check_request (sock);
	}
      else if (trunc == -2)
	{
	  return -1;
	}
    }
  /* Some error occurred. */
  else
    {
      log_printf (LOG_ERROR, "icmp: recvfrom: %s\n", NET_ERROR);
      if (last_errno != SOCK_UNAVAILABLE)
	return -1;
    }
  return 0;
}

/*
 * The default ICMP write callback is called whenever the socket fd has
 * been select()ed or poll()ed.
 */
int
icmp_write_socket (socket_t sock)
{
  int num_written;
  unsigned do_write;
  char *p;
  socklen_t len;
  struct sockaddr_in receiver;

  /* return here if there is nothing to do */
  if (sock->send_buffer_fill <= 0)
    return 0;

  len = sizeof (struct sockaddr_in);
  receiver.sin_family = AF_INET;

  /* get destination address and data length from buffer */
  p = sock->send_buffer;
  memcpy (&do_write, p, sizeof (do_write));
  p += sizeof (do_write);
  memcpy (&receiver.sin_addr.s_addr, p, sizeof (sock->remote_addr));
  p += sizeof (sock->remote_addr);
  memcpy (&receiver.sin_port, p, sizeof (sock->remote_port));
  p += sizeof (sock->remote_port);
  assert ((int) do_write <= sock->send_buffer_fill);

  /* if socket is connect()ed use send() instead of sendto() */
  if (!(sock->flags & SOCK_FLAG_CONNECTED))
    {
      num_written = sendto (sock->sock_desc, p,
			    do_write - (p - sock->send_buffer),
			    0, (struct sockaddr *) &receiver, len);
    }
  else
    {
      num_written = send (sock->sock_desc, p,
			  do_write - (p - sock->send_buffer), 0);
    }

  /* Some error occurred while sending. */
  if (num_written < 0)
    {
      log_printf (LOG_ERROR, "icmp: sendto: %s\n", NET_ERROR);
      if (last_errno == SOCK_UNAVAILABLE)
        num_written = 0;
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

#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "icmp: sendto: %s (%u bytes)\n",
              util_inet_ntoa (receiver.sin_addr.s_addr),
              do_write - (p - sock->send_buffer));
#endif  

  return num_written < 0 ? -1 : 0;
}

/*
 * Send a given buffer via this ICMP socket. If the length argument 
 * supersedes the maximum ICMP message size the buffer is splitted into
 * smaller blocks.
 */
int
icmp_write (socket_t sock, char *buf, int length)
{
  static char *buffer = icmp_buffer;
  icmp_header_t hdr;
  unsigned len, size;
  int ret = 0;

  /* Return if the socket has already been killed. */
  if (sock->flags & SOCK_FLAG_KILLED)
    return 0;

  /* Send a disconnection packet if the given buffer BUF is NULL. */
  if (buf == NULL)
    {
      len = sizeof (len);
      memcpy (&buffer[len], &sock->remote_addr, sizeof (sock->remote_addr));
      len += sizeof (sock->remote_addr);
      memcpy (&buffer[len], &sock->remote_port, sizeof (sock->remote_port));
      len += sizeof (sock->remote_port);
      size = 0;

      hdr.type = ICMP_SERVEEZ;
      hdr.code = ICMP_SERVEEZ_CLOSE;
      hdr.checksum = icmp_ip_checksum ((byte *) buf, size);
      hdr.ident = (unsigned short) (getpid () + sock->id);
      hdr.sequence = sock->send_seq;
      hdr.port = sock->remote_port;
      memcpy (&buffer[len], icmp_put_header (&hdr), ICMP_HEADER_SIZE);
      len += ICMP_HEADER_SIZE;

      memcpy (buffer, &len, sizeof (len));

      if ((ret = sock_write (sock, buffer, len)) == -1)
        {
          sock->flags |= SOCK_FLAG_KILLED;
        }
      return ret;
    }

  while (length)
    {
      /* 
       * Put the data length and destination address in front 
       * of each packet. 
       */
      len = sizeof (len);
      memcpy (&buffer[len], &sock->remote_addr, sizeof (sock->remote_addr));
      len += sizeof (sock->remote_addr);
      memcpy (&buffer[len], &sock->remote_port, sizeof (sock->remote_port));
      len += sizeof (sock->remote_port);
      if ((size = length) > ICMP_MSG_SIZE) size = ICMP_MSG_SIZE;

      /* Create ICMP header and put it in front of packet load. */
      hdr.type = ICMP_SERVEEZ;
      hdr.code = (byte) (sock->send_seq++ ? 
			 ICMP_SERVEEZ_DATA : ICMP_SERVEEZ_CONNECT);
      hdr.checksum = icmp_ip_checksum ((byte *) buf, size);
      hdr.ident = (unsigned short) (getpid () + sock->id);
      hdr.sequence = sock->send_seq;
      hdr.port = sock->remote_port;
      memcpy (&buffer[len], icmp_put_header (&hdr), ICMP_HEADER_SIZE);
      len += ICMP_HEADER_SIZE;

      /* Copy the given buffer. */
      memcpy (&buffer[len], buf, size);
      len += size;

      /* Put chunk length to buffer. */
      memcpy (buffer, &len, sizeof (len));
      buf += size;
      length -= size;

      /* Actually send the data or put it into the send buffer queue. */
      if ((ret = sock_write (sock, buffer, len)) == -1)
        {
          sock->flags |= SOCK_FLAG_KILLED;
          break;
        }
    }

  return ret;
}


/*
 * Put a formatted string to the icmp socket SOCK. Packet length and
 * destination address are additionally saved to the send buffer. The
 * destination is taken from sock->remote_addr. Furthermore a valid
 * icmp header is stored in front of the actual packet data.
 */
int
icmp_printf (socket_t sock, const char *fmt, ...)
{
  va_list args;
  static char buffer[VSNPRINTF_BUF_SIZE];
  unsigned len;

  /* return if there is nothing to do */
  if (sock->flags & SOCK_FLAG_KILLED)
    return 0;

  /* save actual packet load */
  va_start (args, fmt);
  len = vsnprintf (buffer, VSNPRINTF_BUF_SIZE, fmt, args);
  va_end (args);
  
  return icmp_write (sock, buffer, len);
}

/*
 * Default check_request callback for ICMP sockets.
 */
int
icmp_check_request (socket_t sock)
{
  int n;
  server_t *server;

  if (sock->data == NULL && sock->handle_request == NULL)
    return -1;

  /* 
   * If there is a valid handle_request callback (dedicated icmp connection)
   * call it. This kind of behavior is due to a socket creation via
   * icmp_connect (s.b.) and setting up a static handle_request callback.
   */
  if (sock->handle_request)
    {
      if (sock->handle_request (sock, sock->recv_buffer,
                                sock->recv_buffer_fill))
        return -1;
      sock->recv_buffer_fill = 0;
      return 0;
    }

  /* go through all icmp servers on this server socket */
  for (n = 0; (server = SERVER (sock->data, n)) != NULL; n++)
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
      log_printf (LOG_DEBUG, "rejecting icmp packet on socket %d\n",
                  sock->sock_desc);
#endif
      sock->recv_buffer_fill = 0;
    }

  sock->cfg = NULL;
  return 0;
}

/*
 * This function creates an ICMP socket for receiving and sending.
 * Return NULL on errors, otherwise an enqueued socket_t structure.
 */
socket_t
icmp_connect (unsigned long host, unsigned short port)
{
  struct sockaddr_in client;
  SOCKET sockfd;
  socket_t sock;
#ifdef __MINGW32__
  unsigned long blockMode = 1;
#endif

  /* create a socket for communication with the server */
  if ((sockfd = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP)) == INVALID_SOCKET)
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

  sock_resize_buffers (sock, ICMP_BUF_SIZE, ICMP_BUF_SIZE);
  sock_unique_id (sock);
  sock->sock_desc = sockfd;
  sock->flags |= (SOCK_FLAG_SOCK | SOCK_FLAG_CONNECTED | SOCK_FLAG_FIXED);
  sock_enqueue (sock);
  sock_intern_connection_info (sock);

  sock->read_socket = icmp_read_socket;
  sock->write_socket = icmp_write_socket;
  sock->check_request = icmp_check_request;
  sock->remote_port = (unsigned short) sock->id;

  connected_sockets++;
  return sock;
}
