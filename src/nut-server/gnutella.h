/*
 * gnutella.h - gnutella protocol header file
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
 * $Id: gnutella.h,v 1.1 2000/08/26 18:05:18 ela Exp $
 *
 */

#ifndef __GNUTELLA_H__
#define __GNUTELLA_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include "util.h"
#include "hash.h"

/* general defines */
#define NUT_VERSION   "0.48"
#define NUT_CONNECT   "GNUTELLA CONNECT/0.4\n\n"
#define NUT_OK        "GNUTELLA OK\n\n"
#define NUT_GET       "GET /get/"
#define NUT_AGENT     "User-Agent: gnutella"
#define NUT_HTTP      "HTTP/1.0"
#define NUT_RANGE     "Content-range:"
#define NUT_PORT            6346
#define NUT_GUID            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#define NUT_GUID_SIZE       16
#define NUT_SEARCH_INTERVAL 10
/* function IDs */
#define NUT_PING_REQ   0x00 /* ping */
#define NUT_PING_ACK   0x01 /* ping response */
#define NUT_PUSH_REQ   0x40 /* client push request */
#define NUT_SEARCH_REQ 0x80 /* search request */
#define NUT_SEARCH_ACK 0x81 /* search response */

/* id:
 * The header contains a Microsoft GUID (Globally Unique Identifier for 
 * you nonWinblows people) which is the message identifer. My crystal ball 
 * reports that "the GUIDs only have to be unique on the client", which 
 * means that you can really put anything here, as long as you keep track 
 * of it (a client won't respond to you if it sees the same message id 
 * again). If you're responding to a message, be sure you haven't seen the 
 * message id (from that host) before, copy their message ID into your 
 * response and send it on it's way.
 */

#pragma pack (1)

/* gnutella header */
typedef struct
{
  byte id[NUT_GUID_SIZE]; /* message ID */
  byte function;          /* function ID */
  byte ttl;               /* remaining TTL */
  byte hop;               /* hop count */
  unsigned int length;    /* data length */
}
nut_header_t;

/* ping response */
typedef struct
{
  unsigned short port; /* port number of the listening host */
  unsigned long ip;    /* address of the listening host, network byte order */
  unsigned int files;  /* number of files shared by the host */
  unsigned int size;   /* total size of files shared by the host, in KB */
}
nut_ping_reply_t;

/* search query header */
typedef struct
{
  unsigned short speed; /* minimum speed (in kbps) */
  char *search;         /* search request (NULL terminated) */
}
nut_query_t;

/* search record structure */
typedef struct
{
  unsigned int index; /* file index */
  unsigned int size;  /* file size */
  char *file;         /* file name (double-NULL terminated) */
}
nut_record_t;

/* search reply header */
typedef struct
{
  byte records;           /* number of records which follow this header */
  unsigned short port;    /* listening port number of the host */
  unsigned long ip;       /* ip address of the host, network byte order */
  unsigned short speed;   /* speed of the host which found the results */
  unsigned short pad;     /* dunno */
  nut_record_t *record;   /* array of records */
  byte id[NUT_GUID_SIZE]; /* clientID128 of the host */
}
nut_reply_t;

/* client push request structure */
typedef struct
{
  byte id[NUT_GUID_SIZE]; /* servers GUID the client wishes the push from */
  unsigned int index;     /* index of file requested */
  unsigned long ip;       /* ip address of the host requesting the push */
  unsigned short port;    /* port number of the host requesting the push */
}
nut_push_t;

/* gnutella client structure */
typedef struct
{
  byte id[NUT_GUID_SIZE]; /* clientID128 GUID */
  int connected;          /* has the server / client been connected ? */
}
nut_client_t;

/*
 * Protocol server specific configuration.
 */
typedef struct
{
  struct portcfg *port;
  int servers;
  char **hosts;
  byte guid[NUT_GUID_SIZE];
  hash_t *route;
  hash_t *conn;
  char *search;
}
nut_config_t;

/*
 * Basic server callback definitions.
 */
int nut_detect_proto (void *cfg, socket_t sock);
int nut_connect_socket (void *cfg, socket_t sock);
int nut_check_request (socket_t sock);
int nut_disconnect (socket_t sock);
int nut_idle_searching (socket_t sock);
int nut_init (server_t *server);
int nut_global_init (void);
int nut_finalize (server_t *server);
int nut_global_finalize (void);

/*
 * This server's definition.
 */
extern server_definition_t nut_server_definition;


#endif /* __GNUTELLA_H__ */
