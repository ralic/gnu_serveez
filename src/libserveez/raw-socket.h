/*
 * raw-socket.h - raw socket header definitions
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
 * $Id: raw-socket.h,v 1.1 2001/01/28 03:26:55 ela Exp $
 *
 */

#ifndef __RAW_SOCKET_H__
#define __RAW_SOCKET_H__ 1

#include "libserveez/defines.h"

/* local definitions */
#define IP_VERSION_4     4
#define IP_CHECKSUM_OFS  10
#define IP_HEADER_SIZE   20
#define SIZEOF_UINT16    2
#define SIZEOF_UINT32    4

/* ip protocol definitions */
#define ICMP_PROTOCOL  1
#define TCP_PROTOCOL   6
#define UDP_PROTOCOL  17

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

__BEGIN_DECLS

/* Exported RAW socket functions. */
SERVEEZ_API ip_header_t * raw_get_ip_header __P ((byte *data));
SERVEEZ_API byte * raw_put_ip_header __P ((ip_header_t *hdr));
SERVEEZ_API unsigned short raw_ip_checksum __P ((byte *data, int len));
SERVEEZ_API int raw_check_ip_header __P ((byte *data, int len));

__END_DECLS

#endif /* !__RAW_SOCKET_H__ */
