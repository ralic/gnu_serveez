/*
 * portcfg.h - port configuration interface
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: portcfg.h,v 1.14 2001/06/08 15:37:37 ela Exp $
 *
 */

#ifndef __PORTCFG_H__
#define __PORTCFG_H__ 1

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>

#ifndef __MINGW32__
# include <netinet/in.h>
#else
# include <winsock2.h>
#endif

#include "libserveez/defines.h"
#include "libserveez/array.h"
#include "libserveez/hash.h"
#include "libserveez/pipe-socket.h"

/* Port configuration items. */
#define PORTCFG_PORT    "port"
#define PORTCFG_PROTO   "proto"
#define PORTCFG_INPIPE  "inpipe"
#define PORTCFG_OUTPIPE "outpipe"
#define PORTCFG_TCP     "tcp"
#define PORTCFG_UDP     "udp"
#define PORTCFG_ICMP    "icmp"
#define PORTCFG_RAW     "raw"
#define PORTCFG_PIPE    "pipe"
#define PORTCFG_IP      "ipaddr"
#define PORTCFG_NOIP    "*"
#define PORTCFG_BACKLOG "backlog"
#define PORTCFG_TYPE    "type"

/* Pipe definitions. */
#define PORTCFG_RECV  "recv"
#define PORTCFG_SEND  "send"
#define PORTCFG_NAME  "name"
#define PORTCFG_PERMS "permissions"
#define PORTCFG_USER  "user"
#define PORTCFG_GROUP "group"
#define PORTCFG_UID   "uid"
#define PORTCFG_GID   "gid"

/* Miscellaneous definitions. */
#define PORTCFG_SEND_BUFSIZE "send-buffer-size"
#define PORTCFG_RECV_BUFSIZE "recv-buffer-size"
#define PORTCFG_FREQ         "connect-frequency"
#define PORTCFG_ALLOW        "allow"
#define PORTCFG_DENY         "deny"

/*
 * Definition of a single port configuration reflecting either a network
 * (TCP, UDP, ICMP or RAW) or filesystem configuration (PIPE).
 */
typedef struct svz_portcfg
{
  /* the symbolic name of this port configuration */
  char *name;

  /* one of the PROTO_ flags defined in <core.h> */
  int proto;

  /* unified structure for each type of port */
  union protocol_t
  {
    /* tcp port */
    struct tcp_t
    {
      unsigned short port;     /* TCP/IP port */
      char *ipaddr;            /* dotted decimal or "*" for any address */
      struct sockaddr_in addr; /* converted from the above 2 values */
      int backlog;             /* backlog argument for listen() */
    } tcp;

    /* udp port */
    struct udp_t
    {
      unsigned short port;     /* UDP port */
      char *ipaddr;            /* dotted decimal or "*" */
      struct sockaddr_in addr; /* converted from the above 2 values */
    } udp;

    /* icmp port */
    struct icmp_t
    {
      char *ipaddr;            /* dotted decimal or "*" */
      struct sockaddr_in addr; /* converted from the above value */
      unsigned char type;      /* message type */
    } icmp;

    /* raw ip port */
    struct raw_t
    {
      char *ipaddr;            /* dotted decimal or "*" */
      struct sockaddr_in addr; /* converted from the above value */
    } raw;

    /* pipe port */
    struct pipe_t
    {
      svz_pipe_t recv;         /* pipe for sending data into serveez */
      svz_pipe_t send;         /* pipe serveez sends responses out on */
    } pipe;
  }
  protocol;

  /* maximum number of bytes for protocol identification */
  int detection_fill;

  /* maximum seconds for protocol identification */
  int detection_wait;

  /* initial buffer sizes */
  int send_buffer_size;
  int recv_buffer_size;

  /* allowed number of connects per second (hammer protection) */
  int connect_freq;

  /* remembers connect frequency for each ip */
  svz_hash_t *accepted;

  /* denied and allowed access list (ip based) */
  svz_array_t *deny;
  svz_array_t *allow;
}
svz_portcfg_t;

/* 
 * Accessor definitions for each type of protocol. 
 */
#define tcp_port protocol.tcp.port
#define tcp_addr protocol.tcp.addr
#define tcp_ipaddr protocol.tcp.ipaddr
#define tcp_backlog protocol.tcp.backlog

#define udp_port protocol.udp.port
#define udp_addr protocol.udp.addr
#define udp_ipaddr protocol.udp.ipaddr

#define icmp_addr protocol.icmp.addr
#define icmp_ipaddr protocol.icmp.ipaddr
#define icmp_type protocol.icmp.type

#define raw_addr protocol.raw.addr
#define raw_ipaddr protocol.raw.ipaddr

#define pipe_recv protocol.pipe.recv
#define pipe_send protocol.pipe.send

/*
 * Return the pointer of the @code{sockaddr_in} structure of the given
 * port configuration @var{port} if it is a network port configuration. 
 * Otherwise return @code{NULL}.
 */
#define svz_portcfg_addr(port)                               \
  (((port)->proto & PROTO_TCP) ? &((port)->tcp_addr) :       \
   ((port)->proto & PROTO_UDP) ? &((port)->udp_addr) :       \
   ((port)->proto & PROTO_ICMP) ? &((port)->icmp_addr) :     \
   ((port)->proto & PROTO_RAW) ? &((port)->raw_addr) : NULL) \

/*
 * Return the pointer to the ip address @code{ipaddr} of the given
 * port configuration @var{port} if it is a network port configuration. 
 * Otherwise return @code{NULL}.
 */
#define svz_portcfg_ipaddr(port)                            \
  (((port)->proto & PROTO_TCP) ? (port)->tcp_ipaddr :       \
   ((port)->proto & PROTO_UDP) ? (port)->udp_ipaddr :       \
   ((port)->proto & PROTO_ICMP) ? (port)->icmp_ipaddr :     \
   ((port)->proto & PROTO_RAW) ? (port)->raw_ipaddr : NULL) \

/*
 * Return the UDP or TCP port of the given port configuration or zero
 * if it neither TCP nor UDP.
 */
#define svz_portcfg_port(port)                         \
  (((port)->proto & PROTO_TCP) ? (port)->tcp_port :    \
   ((port)->proto & PROTO_UDP) ? (port)->udp_port : 0) \

/*
 * Create a new blank port configuration.
 */
#define svz_portcfg_create() \
  (svz_portcfg_t *) svz_calloc (sizeof (svz_portcfg_t))

__BEGIN_DECLS

SERVEEZ_API int svz_portcfg_equal __P ((svz_portcfg_t *, svz_portcfg_t *));
SERVEEZ_API svz_portcfg_t *svz_portcfg_add __P ((char *, svz_portcfg_t *));
SERVEEZ_API svz_portcfg_t *svz_portcfg_del __P ((char *));
SERVEEZ_API svz_portcfg_t *svz_portcfg_get __P ((char *));
SERVEEZ_API void svz_portcfg_destroy __P ((svz_portcfg_t *port));
SERVEEZ_API void svz_portcfg_finalize __P ((void));
SERVEEZ_API int svz_portcfg_mkaddr __P ((svz_portcfg_t *this));
SERVEEZ_API void svz_portcfg_prepare __P ((svz_portcfg_t *port));
SERVEEZ_API void svz_portcfg_print __P ((svz_portcfg_t *this, FILE *stream));
SERVEEZ_API svz_portcfg_t *svz_portcfg_dup __P ((svz_portcfg_t *port));
SERVEEZ_API svz_array_t *svz_portcfg_expand __P ((svz_portcfg_t *this));
SERVEEZ_API int svz_portcfg_set_ipaddr __P ((svz_portcfg_t *, char *));
SERVEEZ_API void svz_portcfg_destroy_access __P ((svz_portcfg_t *port));
SERVEEZ_API void svz_portcfg_destroy_accepted __P ((svz_portcfg_t *port));

__END_DECLS

#endif /* __PORTCFG_H__ */
