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
 * $Id: icmp-socket.h,v 1.2 2000/10/01 11:11:20 ela Exp $
 *
 */

#ifndef __ICMP_SOCKET_H__
#define __ICMP_SOCKET_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include "socket.h"

#define ICMP_BUFFER_SIZE (64 * 1024)
#define SIZEOF_UINT16 2
#define SIZEOF_UINT32 4

/* version and length are 4 bit values in the ip header */
#define IP_HDR_VERSION(ip_hdr) ((ip_hdr.version_length >> 4) & 0x0f)
#define IP_HDR_LENGTH(ip_hdr)  ((ip_hdr.version_length & 0x0f) << 2)

/* IP header structure. */
typedef struct
{
  byte version_length;        /* header length and ip version */
  byte unused;                /* unused pad byte */
  unsigned short length;      /* total ip packet length */
  unsigned short id;          /* ip identifier */
  byte flags;                 /* ip flags */
  unsigned short frag_offset; /* fragment offset */
  byte ttl;                   /* time to live */
  byte protocol;              /* ip protocol */
  unsigned short checksum;    /* ip header check sum */
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
  unsigned short id;       /* identifier */
  unsigned short sequence; /* sequence number */
}
icmp_header_t;

/* Exported functions. */
int icmp_read_socket (socket_t sock);

#endif /* __ICMP_SOCKET_H__ */
