/*
 * icmp-socket.c - ICMP socket implementations
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
 * $Id: icmp-socket.c,v 1.8 2001/04/01 13:32:29 ela Exp $
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
# include <winsock2.h>
# include <process.h>
#endif

#include "libserveez/snprintf.h"
#include "libserveez/util.h"
#include "libserveez/socket.h"
#include "libserveez/core.h"
#include "libserveez/server-core.h"
#include "libserveez/icmp-socket.h"
#include "libserveez/raw-socket.h"
#include "libserveez/server.h"

/* Text representation of ICMP type codes. */
static char *icmp_request[] = {
  "echo reply",
  NULL,
  NULL,
  "destination unreachable",
  "source quench",
  "redirect (change route)",
  NULL,
  NULL,
  "echo request",
  NULL,
  NULL,
  "time exceeded",
  "parameter problem",
  "timestamp request",
  "timestamp reply",
  "information request",
  "information reply",
  "address mask request",
  "address mask reply"
};

#ifdef __MINGW32__

/*
 * Microsoft discourages the use of their ICMP.DLL API, but it seems
 * to be the only way to make use of raw sockets anyway. The API is
 * almostly unusable because:
 * 1. you cannot receive if not previously sent a packet
 * 2. the IcmpSendEcho call is blocking
 * 3. receive and send is one call
 * 4. you cannot set the ICMP header (type, code)
 */

/* 
 * Note 2: For the most part, you can refer to RFC 791 for details 
 * on how to fill in values for the IP option information structure. 
 */
typedef struct ip_option_information
{
  unsigned char Ttl;          /* Time To Live (used for traceroute) */
  unsigned char Tos;          /* Type Of Service (usually 0) */
  unsigned char Flags;        /* IP header flags (usually 0) */
  unsigned char OptionsSize;  /* Size of options data (usually 0, max 40) */
  unsigned char *OptionsData; /* Options data buffer */
}
IPINFO;

/* 
 * Note 1: The Reply Buffer will have an array of ICMP_ECHO_REPLY
 * structures, followed by options and the data in ICMP echo reply
 * datagram received. You must have room for at least one ICMP
 * echo reply structure, plus 8 bytes for an ICMP header. 
 */
typedef struct icmp_echo_reply
{
  unsigned long Address;   /* source address */
  unsigned long Status;    /* IP status value (see below) */
  unsigned long RTTime;    /* Round Trip Time in milliseconds */
  unsigned short DataSize; /* reply data size */
  unsigned short Reserved; /* */
  void *Data;              /* reply data buffer */
  IPINFO Options;          /* reply options */
}
ICMPECHO;

/*
 * DLL function definitions of IcmpCloseHandle, IcmpCreateFile, 
 * IcmpParseReplies, IcmpSendEcho and IcmpSendEcho2.
 */
typedef HANDLE (__stdcall * IcmpCreateFileProc) (void);
typedef BOOL (__stdcall * IcmpCloseHandleProc) (HANDLE IcmpHandle);
typedef DWORD (__stdcall * IcmpSendEchoProc) (
  HANDLE IcmpHandle,          /* handle returned from IcmpCreateFile() */
  unsigned long DestAddress,  /* destination IP address (in network order) */
  void *RequestData,          /* pointer to buffer to send */
  unsigned short RequestSize, /* length of data in buffer */
  IPINFO *RequestOptns,       /* see Note 2 */
  void *ReplyBuffer,          /* see Note 1 */
  unsigned long ReplySize,    /* length of reply (at least 1 reply) */
  unsigned long Timeout       /* time in milliseconds to wait for reply */
);

/*
 * Error definitions.
 */
#define IP_STATUS_BASE           11000
#define IP_SUCCESS               0
#define IP_BUF_TOO_SMALL         (IP_STATUS_BASE + 1)
#define IP_DEST_NET_UNREACHABLE  (IP_STATUS_BASE + 2)
#define IP_DEST_HOST_UNREACHABLE (IP_STATUS_BASE + 3)
#define IP_DEST_PROT_UNREACHABLE (IP_STATUS_BASE + 4)
#define IP_DEST_PORT_UNREACHABLE (IP_STATUS_BASE + 5)
#define IP_NO_RESOURCES          (IP_STATUS_BASE + 6)
#define IP_BAD_OPTION            (IP_STATUS_BASE + 7)
#define IP_HW_ERROR              (IP_STATUS_BASE + 8)
#define IP_PACKET_TOO_BIG        (IP_STATUS_BASE + 9)
#define IP_REQ_TIMED_OUT         (IP_STATUS_BASE + 10)
#define IP_BAD_REQ               (IP_STATUS_BASE + 11)
#define IP_BAD_ROUTE             (IP_STATUS_BASE + 12)
#define IP_TTL_EXPIRED_TRANSIT   (IP_STATUS_BASE + 13)
#define IP_TTL_EXPIRED_REASSEM   (IP_STATUS_BASE + 14)
#define IP_PARAM_PROBLEM         (IP_STATUS_BASE + 15)
#define IP_SOURCE_QUENCH         (IP_STATUS_BASE + 16)
#define IP_OPTION_TOO_BIG        (IP_STATUS_BASE + 17)
#define IP_BAD_DESTINATION       (IP_STATUS_BASE + 18)
#define IP_ADDR_DELETED          (IP_STATUS_BASE + 19)
#define IP_SPEC_MTU_CHANGE       (IP_STATUS_BASE + 20)
#define IP_MTU_CHANGE            (IP_STATUS_BASE + 21)
#define IP_UNLOAD                (IP_STATUS_BASE + 22)
#define IP_GENERAL_FAILURE       (IP_STATUS_BASE + 50)
#define MAX_IP_STATUS            IP_GENERAL_FAILURE
#define IP_PENDING               (IP_STATUS_BASE + 255)

/* Functions and handles for the ICMP.DLL API. */
static IcmpCreateFileProc IcmpCreateFile = NULL;
static IcmpCloseHandleProc IcmpCloseHandle = NULL;
static IcmpSendEchoProc IcmpSendEcho = NULL;
static HANDLE IcmpHandle = NULL;
static HANDLE hIcmp = INVALID_HANDLE_VALUE;

/*
 * Load the ICMP.DLL library into process address space and get all
 * necessary function pointers.
 */
void
icmp_startup (void)
{
  /* load library */
  if ((IcmpHandle = LoadLibrary ("ICMP.DLL")) == NULL)
    {
      log_printf (LOG_ERROR, "icmp: LoadLibrary: %s\n", SYS_ERROR);
      return;
    }

  /* obtain functions */
  IcmpCreateFile = (IcmpCreateFileProc) 
    GetProcAddress (IcmpHandle, "IcmpCreateFile");
  IcmpCloseHandle = (IcmpCloseHandleProc)
    GetProcAddress (IcmpHandle, "IcmpCloseHandle");
  IcmpSendEcho = (IcmpSendEchoProc)
    GetProcAddress (IcmpHandle, "IcmpSendEcho");
  if (IcmpSendEcho == NULL || 
      IcmpCloseHandle == NULL || IcmpCreateFile == NULL)
    {
      log_printf (LOG_ERROR, "icmp: GetProcAddress: %s\n", SYS_ERROR);
      FreeLibrary (IcmpHandle);
      IcmpHandle = NULL;
      return;
    }

  /* open ping service */
  if ((hIcmp = IcmpCreateFile ()) == INVALID_HANDLE_VALUE)
    {
      log_printf (LOG_ERROR, "IcmpCreateFile: %s\n", SYS_ERROR);
      FreeLibrary (IcmpHandle);
      IcmpHandle = NULL;
      return;
    }

#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "icmp services successfully initialized\n");
#endif
}

/*
 * Shutdown the ping service.
 */
void
icmp_cleanup (void)
{
  /* close ip service */
  if (hIcmp != INVALID_HANDLE_VALUE)
    {
      if (!IcmpCloseHandle (hIcmp))
       log_printf (LOG_ERROR, "IcmpCloseHandle: %s\n", SYS_ERROR);
    }

  /* release ICMP.DLL */
  if (IcmpHandle)
    {
      FreeLibrary (IcmpHandle);
      IcmpHandle = NULL;
    }
}
#endif /* __MINGW32__ */

/* Static buffer for ip packets. */
static char icmp_buffer[IP_HEADER_SIZE + ICMP_HEADER_SIZE + ICMP_MSG_SIZE];

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
static byte *
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

#define ICMP_ERROR      -1
#define ICMP_DISCONNECT -2

/*
 * Parse and check IP and ICMP header. Return the amount of leading bytes 
 * to be truncated. Return ICMP_ERROR on packet errors and return 
 * ICMP_DISCONNECT when we received an disconnection signal.
 */
static int
icmp_check_packet (socket_t sock, byte *data, int len)
{
  int length;
  byte *p = data;
  icmp_header_t *header;

  /* First check the IP header. */
  if ((length = raw_check_ip_header (p, len)) == -1)
    return ICMP_ERROR;

  /* Get the actual ICMP header. */
  header = icmp_get_header (p + length);
  p += length + ICMP_HEADER_SIZE;
  len -= length + ICMP_HEADER_SIZE;

  /* Do these checks only if it is the right kind of packet. */
  if (header->type == ICMP_SERVEEZ)
    {
      /* validate the ICMP data checksum */
      if (header->checksum != raw_ip_checksum (p, len))
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "icmp: invalid data checksum\n");
#endif
	  return ICMP_ERROR;
	}

      /* check the ICMP header identification */
      if (header->ident == getpid () + sock->id)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "icmp: rejecting native packet\n");
#endif
	  return ICMP_ERROR;
	}

      /* check ICMP remote port */
      if ((header->port != sock->remote_port) && 
	  !(sock->flags & SOCK_FLAG_LISTENING))
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "icmp: rejecting filtered packet\n");
#endif
	  return ICMP_ERROR;
	}
      sock->remote_port = header->port;
    }

  /* What kind of packet is this ? */
#if ENABLE_DEBUG
  if (header->type <= ICMP_MAX_TYPE)
    {
      if (icmp_request[header->type])
	log_printf (LOG_DEBUG, "icmp: %s received\n", 
		    icmp_request[header->type]);
      else
	log_printf (LOG_DEBUG, "unsupported protocol 0x%02X received\n", 
		    header->type);
      return ICMP_ERROR;
    }
#endif /* ENABLE_DEBUG */

  if (header->type == ICMP_SERVEEZ)
    {
      if (header->code == ICMP_SERVEEZ_CONNECT && 
	  sock->flags & SOCK_FLAG_LISTENING)
	{
	  log_printf (LOG_NOTICE, "icmp: accepting connection\n");
	}
      else if (header->code == ICMP_SERVEEZ_CLOSE)
	{
	  log_printf (LOG_NOTICE, "icmp: closing connection\n");
	  return ICMP_DISCONNECT;
	}
      return (length + ICMP_HEADER_SIZE);
    }
#if ENABLE_DEBUG
  else
    {
      log_printf (LOG_DEBUG, "unsupported protocol 0x%02X received\n", 
		  header->type);
    }
#endif /* ENABLE_DEBUG */

  return ICMP_ERROR;
}

/*
 * Default reader for ICMP sockets. The sender is stored within 
 * `sock->remote_addr' and `sock->remote_port' afterwards.
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
      svz_hexdump (stdout, "icmp packet received", sock->sock_desc,
		   icmp_buffer, num_read, 0);
#endif
      sock->last_recv = time (NULL);
      if (!(sock->flags & SOCK_FLAG_FIXED))
        {
	  sock->remote_addr = sender.sin_addr.s_addr;
	}
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "icmp: recv%s: %s (%u bytes)\n",
		  sock->flags & SOCK_FLAG_CONNECTED ? "" : "from",
		  svz_inet_ntoa (sock->remote_addr), num_read);
#endif /* ENABLE_DEBUG */

      /* 
       * Check the ICMP packet and put the packet load only into the
       * receive buffer of the socket structure.
       */
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
      else if (trunc == ICMP_DISCONNECT)
	{
	  return -1;
	}
    }
  /* Some error occurred. */
  else
    {
      log_printf (LOG_ERROR, "icmp: recv%s: %s\n", 
		  sock->flags & SOCK_FLAG_CONNECTED ? "" : "from", NET_ERROR);
      if (svz_errno != SOCK_UNAVAILABLE)
	return -1;
    }
  return 0;
}

/*
 * The default ICMP write callback is called whenever the socket 
 * descriptor has been `select ()'ed or `poll ()'ed to be ready for 
 * sending.
 */
int
icmp_write_socket (socket_t sock)
{
  int num_written;
  unsigned do_write;
  char *p;
  socklen_t len;
  struct sockaddr_in receiver;

  /* Return here if there is nothing to do. */
  if (sock->send_buffer_fill <= 0)
    return 0;

  len = sizeof (struct sockaddr_in);
  receiver.sin_family = AF_INET;

  /* Get destination address and data length from send buffer. */
  p = sock->send_buffer;
  memcpy (&do_write, p, sizeof (do_write));
  p += sizeof (do_write);
  memcpy (&receiver.sin_addr.s_addr, p, sizeof (sock->remote_addr));
  p += sizeof (sock->remote_addr);
  memcpy (&receiver.sin_port, p, sizeof (sock->remote_port));
  p += sizeof (sock->remote_port);
  assert ((int) do_write <= sock->send_buffer_fill);

  /* If socket is `connect ()'ed use `send ()' instead of `sendto ()'. */
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
      log_printf (LOG_ERROR, "icmp: send%s: %s\n", 
		  sock->flags & SOCK_FLAG_CONNECTED ? "" : "to", NET_ERROR);
      if (svz_errno == SOCK_UNAVAILABLE)
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
  log_printf (LOG_DEBUG, "icmp: send%s: %s (%u bytes)\n",
	      sock->flags & SOCK_FLAG_CONNECTED ? "" : "to",
              svz_inet_ntoa (receiver.sin_addr.s_addr),
              do_write - (p - sock->send_buffer));
#endif /* ENABLE_DEBUG */

  return num_written < 0 ? -1 : 0;
}

/*
 * If you are calling this function we will send an empty ICMP packet
 * signaling that this connection is going down soon.
 */
int
icmp_send_control (socket_t sock, byte type)
{
  static char *buffer = icmp_buffer;
  icmp_header_t hdr;
  unsigned len;
  int ret = 0;

  len = sizeof (len);
  memcpy (&buffer[len], &sock->remote_addr, sizeof (sock->remote_addr));
  len += sizeof (sock->remote_addr);
  memcpy (&buffer[len], &sock->remote_port, sizeof (sock->remote_port));
  len += sizeof (sock->remote_port);

  hdr.type = ICMP_SERVEEZ;
  hdr.code = type;
  hdr.checksum = raw_ip_checksum (NULL, 0);
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

/*
 * Send a given buffer BUF with length LENGTH via this ICMP socket. If 
 * the length argument supersedes the maximum ICMP message size the buffer 
 * is splitted into smaller packets.
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
      if ((size = length) > ICMP_MSG_SIZE)
	size = ICMP_MSG_SIZE;

      /* Create ICMP header and put it in front of packet load. */
      hdr.type = ICMP_SERVEEZ;
      hdr.code = ICMP_SERVEEZ_DATA;
      hdr.checksum = raw_ip_checksum ((byte *) buf, size);
      hdr.ident = (unsigned short) (getpid () + sock->id);
      hdr.sequence = sock->send_seq++;
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
 * destination is taken from `sock->remote_addr'. Furthermore a valid
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
  len = svz_vsnprintf (buffer, VSNPRINTF_BUF_SIZE, fmt, args);
  va_end (args);
  
  return icmp_write (sock, buffer, len);
}

/*
 * Default `check_request' callback for ICMP sockets.
 */
int
icmp_check_request (socket_t sock)
{
  int n;
  server_t *server;

  if (sock->data == NULL && sock->handle_request == NULL)
    return -1;

  /* 
   * If there is a valid `handle_request' callback (dedicated icmp 
   * connection) call it. This kind of behaviour is due to a socket 
   * creation via 'icmp_connect' (s.b.) and setting up a static 
   * `handle_request' callback.
   */
  if (sock->handle_request)
    {
      if (sock->handle_request (sock, sock->recv_buffer,
                                sock->recv_buffer_fill))
        return -1;
      sock->recv_buffer_fill = 0;
      return 0;
    }

  /* Go through all icmp servers on this server socket. */
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

  /* Check if any server processed this packet. */
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
 * Return NULL on errors, otherwise an enqueued socket structure.
 */
socket_t
icmp_connect (unsigned long host, unsigned short port)
{
  SOCKET sockfd;
  socket_t sock;

  /* Create a client socket. */
  if ((sockfd = svz_socket_create (PROTO_ICMP)) == -1)
      return NULL;

  /* Try to connect to the server. Does it make sense for ICMP ? */
  if (svz_socket_connect (sockfd, host, port) == -1)
     return NULL;

  /* Create socket structure and enqueue it. */
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

  /* Put foreign address here. */
  sock->remote_addr = host;

  sock->read_socket = icmp_read_socket;
  sock->write_socket = icmp_write_socket;
  sock->check_request = icmp_check_request;
  sock->remote_port = (unsigned short) sock->id;

  /* Finally send a connection message. */
  icmp_send_control (sock, ICMP_SERVEEZ_CONNECT);
  sock_connections++;
  return sock;
}
