/*
 * icmp-socket.h - Internet Control Message Protocol socket definitions
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
 * $Id: icmp-socket.h,v 1.9 2000/11/10 11:24:05 ela Exp $
 *
 */

#ifndef __ICMP_SOCKET_H__
#define __ICMP_SOCKET_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include "socket.h"

/* local definitions */
#define IP_VERSION_4     4
#define ICMP_PROTOCOL    1
#define IP_CHECKSUM_OFS  10
#define IP_HEADER_SIZE   20
#define ICMP_HEADER_SIZE 10
#define ICMP_MSG_SIZE    (64 * 1024)
#define ICMP_BUF_SIZE    (4 * (ICMP_MSG_SIZE + ICMP_HEADER_SIZE + 24))
#define SIZEOF_UINT16    2
#define SIZEOF_UINT32    4

/* general definitions */
#define ICMP_ECHOREPLY          0       /* Echo Reply                   */
#define ICMP_DEST_UNREACH       3       /* Destination Unreachable      */
#define ICMP_SOURCE_QUENCH      4       /* Source Quench                */
#define ICMP_REDIRECT           5       /* Redirect (change route)      */
#define ICMP_ECHO               8       /* Echo Request                 */
#define ICMP_TIME_EXCEEDED      11      /* Time Exceeded                */
#define ICMP_PARAMETERPROB      12      /* Parameter Problem            */
#define ICMP_TIMESTAMP          13      /* Timestamp Request            */
#define ICMP_TIMESTAMPREPLY     14      /* Timestamp Reply              */
#define ICMP_INFO_REQUEST       15      /* Information Request          */
#define ICMP_INFO_REPLY         16      /* Information Reply            */
#define ICMP_ADDRESS            17      /* Address Mask Request         */
#define ICMP_ADDRESSREPLY       18      /* Address Mask Reply           */

/* serveez ICMP types and sub-codes */
#define ICMP_SERVEEZ        42
#define ICMP_SERVEEZ_DATA    0
#define ICMP_SERVEEZ_REQ     1
#define ICMP_SERVEEZ_ACK     2
#define ICMP_SERVEEZ_CLOSE   3
#define ICMP_SERVEEZ_CONNECT 4

/* version and length are 4 bit values in the ip header */
#define IP_HDR_VERSION(hdr) ((hdr->version_length >> 4) & 0x0f)
#define IP_HDR_LENGTH(hdr)  ((hdr->version_length & 0x0f) << 2)

/* ip header flags (part of frag_offset) */
#define IP_HDR_FLAGS(hdr) ((hdr->frag_offset) & 0xE000)
#define IP_FLAG_DF 0x4000 /* Don't Fragment This Datagram (DF). */
#define IP_FLAG_MF 0x2000 /* More Fragments Flag (MF). */

/* IP header structure. */
typedef struct
{
  byte version_length;        /* header length (in DWORDs) and ip version */
  byte tos;                   /* type of service = 0 */
  unsigned short length;      /* total ip packet length */
  unsigned short ident;       /* ip identifier */
  unsigned short frag_offset; /* fragment offset (in 8 bytes) and flags */
  byte ttl;                   /* time to live */
  byte protocol;              /* ip protocol */
  unsigned short checksum;    /* ip header checksum */
  unsigned long src;          /* source address */
  unsigned long dst;          /* destination address */
}
ip_header_t;

/* ICMP header structure. */
typedef struct
{
  byte type;               /* message type */
  byte code;               /* type sub-code */
  unsigned short checksum; /* check sum */
  unsigned short ident;    /* identifier */
  unsigned short sequence; /* sequence number */
  unsigned short port;     /* remote port address */
}
icmp_header_t;

/* Exported functions. */
int icmp_read_socket (socket_t sock);
int icmp_write_socket (socket_t sock);
int icmp_check_request (socket_t sock);
socket_t icmp_connect (unsigned long host, unsigned short port);
int icmp_write (socket_t sock, char *buf, int length);

#ifndef __STDC__
int icmp_printf ();
#else
int icmp_printf (socket_t sock, const char * fmt, ...);
#endif

#endif /* __ICMP_SOCKET_H__ */
