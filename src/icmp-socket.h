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
 * $Id: icmp-socket.h,v 1.15 2001/01/07 13:58:33 ela Exp $
 *
 */

#ifndef __ICMP_SOCKET_H__
#define __ICMP_SOCKET_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include "socket.h"

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
typedef HANDLE (__stdcall *IcmpCreateFileProc) (void);
typedef BOOL   (__stdcall *IcmpCloseHandleProc) (HANDLE IcmpHandle);
typedef DWORD  (__stdcall *IcmpSendEchoProc) (
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

/* Exported functions. */
void icmp_startup (void);
void icmp_cleanup (void);

#endif /* __MINGW32__ */

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

/* Exported functions. */
int icmp_read_socket (socket_t sock);
int icmp_write_socket (socket_t sock);
int icmp_check_request (socket_t sock);
socket_t icmp_connect (unsigned long host, unsigned short port);
int icmp_send_control (socket_t sock, byte type);
int icmp_write (socket_t sock, char *buf, int length);

#ifndef __STDC__
int icmp_printf ();
#else
int icmp_printf (socket_t sock, const char * fmt, ...);
#endif

#endif /* __ICMP_SOCKET_H__ */
