/*
 * icmp-socket.h - ICMP socket definitions and declarations
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
 * $Id: icmp-socket.h,v 1.2 2001/01/30 11:49:57 ela Exp $
 *
 */

#ifndef __ICMP_SOCKET_H__
#define __ICMP_SOCKET_H__ 1

#include "libserveez/defines.h"
#include "libserveez/socket.h"

/* local definitions */
#define ICMP_HEADER_SIZE 10
#define ICMP_MSG_SIZE    (64 * 1024)
#define ICMP_BUF_SIZE    (4 * (ICMP_MSG_SIZE + ICMP_HEADER_SIZE + 24))

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
#define ICMP_MAX_TYPE           18

/* serveez ICMP types and sub-codes */
#define ICMP_SERVEEZ        42
#define ICMP_SERVEEZ_DATA    0
#define ICMP_SERVEEZ_REQ     1
#define ICMP_SERVEEZ_ACK     2
#define ICMP_SERVEEZ_CLOSE   3
#define ICMP_SERVEEZ_CONNECT 4

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

__BEGIN_DECLS

#ifdef __MINGW32__

/* Exported ICMP.DLL functions. */
SERVEEZ_API void icmp_startup __P ((void));
SERVEEZ_API void icmp_cleanup __P ((void));

#endif /* __MINGW32__ */

/* Exported ICMP socket functions. */
SERVEEZ_API int icmp_read_socket __P ((socket_t sock));
SERVEEZ_API int icmp_write_socket __P ((socket_t sock));
SERVEEZ_API int icmp_check_request __P ((socket_t sock));
SERVEEZ_API socket_t icmp_connect __P ((unsigned long, unsigned short));
SERVEEZ_API int icmp_send_control __P ((socket_t sock, byte type));
SERVEEZ_API int icmp_write __P ((socket_t sock, char *buf, int length));
SERVEEZ_API int icmp_printf __P ((socket_t sock, const char *fmt, ...));

__END_DECLS

#endif /* !__ICMP_SOCKET_H__ */
