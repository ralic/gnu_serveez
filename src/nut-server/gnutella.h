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
 * $Id: gnutella.h,v 1.15 2000/09/15 08:22:51 ela Exp $
 *
 */

#ifndef __GNUTELLA_H__
#define __GNUTELLA_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <time.h>

#include "util.h"
#include "hash.h"

/* general defines */
#define NUT_VERSION   "0.48"
#define NUT_CONNECT   "GNUTELLA CONNECT/0.4\n\n"
#define NUT_OK        "GNUTELLA OK\n\n"
#define NUT_HOSTS     "GET /gnutella-net HTTP/1."

/* default values */
#define NUT_PORT             6346         /* gnutella default tcp port */
#define NUT_GUID             {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#define NUT_GUID_SIZE        16           /* GUID length in bytes */
#define NUT_SEARCH_INTERVAL  10           /* send search queries */
#define NUT_TTL              5            /* default packet TTL */
#define NUT_MAX_TTL          5            /* default maximum packet TTL */
#define NUT_CONNECT_INTERVAL 2            /* reconnect to gnutella hosts */
#define NUT_SEND_BUFSIZE     (1024 * 100) /* host list buffer size */
#define NUT_CONNECT_TIMEOUT  20           /* close connection then */
#define NUT_ENTRY_AGE        (60 * 3)     /* maximum hash entry age */

/* function IDs */
#define NUT_PING_REQ   0x00 /* ping */
#define NUT_PING_ACK   0x01 /* ping response */
#define NUT_PUSH_REQ   0x40 /* client push request */
#define NUT_SEARCH_REQ 0x80 /* search request */
#define NUT_SEARCH_ACK 0x81 /* search response */

/* protocol flags */
#define NUT_FLAG_HTTP   0x0001
#define NUT_FLAG_HDR    0x0002
#define NUT_FLAG_HOSTS  0x0004
#define NUT_FLAG_CLIENT 0x0008
#define NUT_FLAG_UPLOAD 0x0010

/* guid:
 * The header contains a Microsoft GUID (Globally Unique Identifier for 
 * you nonWinblows people) which is the message identifer. My crystal ball 
 * reports that "the GUIDs only have to be unique on the client", which 
 * means that you can really put anything here, as long as you keep track 
 * of it (a client won't respond to you if it sees the same message id 
 * again). If you're responding to a message, be sure you haven't seen the 
 * message id (from that host) before, copy their message ID into your 
 * response and send it on it's way.
 */

/*
 * The Gnutella packets are all in little endian byte order except
 * ip adresses which are in network byte order (big endian). So they
 * need to be converted to host byte order if necessary.
 */

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
#define SIZEOF_NUT_HEADER (NUT_GUID_SIZE + 7)

/* ping response structure */
typedef struct
{
  unsigned short port; /* port number of the listening host */
  unsigned long ip;    /* address of the listening host, network byte order */
  unsigned int files;  /* number of files shared by the host */
  unsigned int size;   /* total size of files shared by the host, in KB */
}
nut_ping_reply_t;
#define SIZEOF_NUT_PING_REPLY (14)

/* search query header */
typedef struct
{
  unsigned short speed; /* minimum speed (in kbps) */
  char search[1];       /* search request (NULL terminated) */
}
nut_query_t;
#define SIZEOF_NUT_QUERY (2)

/* search record structure */
typedef struct
{
  unsigned int index; /* file index */
  unsigned int size;  /* file size */
  char file[1];       /* file name (double-NULL terminated) */
}
nut_record_t;
#define SIZEOF_NUT_RECORD (8)

/* search reply header */
typedef struct
{
  byte records;           /* number of records which follow this header */
  unsigned short port;    /* listening port number of the host */
  unsigned long ip;       /* ip address of the host, network byte order */
  unsigned short speed;   /* speed of the host which found the results */
  unsigned short pad;     /* dunno */
  nut_record_t record[1]; /* array of records */
  byte id[NUT_GUID_SIZE]; /* clientID128 of the host */
}
nut_reply_t;
#define SIZEOF_NUT_REPLY (11)

/* client push request structure */
typedef struct
{
  byte id[NUT_GUID_SIZE]; /* servers GUID the client wishes the push from */
  unsigned int index;     /* index of file requested */
  unsigned long ip;       /* ip address of the host requesting the push */
  unsigned short port;    /* port number of the host requesting the push */
}
nut_push_t;
#define SIZEOF_NUT_PUSH (26)

/* gnutella host structure */
typedef struct
{
  byte id[NUT_GUID_SIZE]; /* clientID128 GUID */
  unsigned long ip;       /* IP address */
  unsigned short port;    /* TCP port */
  time_t last_reply;      /* last packet received */
}
nut_host_t;

/* each gnutella host connection gets such a structure */
typedef struct
{
  unsigned dropped; /* number of dropped packets */
  unsigned packets; /* number of received packets */
  unsigned invalid; /* number of invalid packet types */
  unsigned queries; /* number of queries */
  unsigned files;   /* file at this connection */
  unsigned size;    /* file size (in KB) here */
  unsigned nodes;   /* number of hosts at this connection */
}
nut_client_t;

/* sent packet structure */
typedef struct
{
  time_t sent;   /* when was this packet sent */
  socket_t sock; /* sent to this socket */
}
nut_packet_t;

/* reply structure */
typedef struct
{
  socket_t sock;      /* routing information */
  unsigned int index; /* file index to push */
}
nut_push_reply_t;

/* files in the sharing directory */
typedef struct
{
  off_t size;     /* file size */
  unsigned index; /* database index */
  char *file;     /* filename */
  char *path;     /* path to file */
  void *next;     /* pointer to next file entry */
}
nut_file_t;

/*
 * Protocol server specific configuration.
 */
typedef struct
{
  portcfg_t *port;          /* port configuration */
  int disable;              /* if set we do not listen on the above port cfg */
  int max_ttl;              /* maximum ttl for a gnutella packet */
  int ttl;                  /* initial ttl for a gnutella packet */
  char **hosts;             /* array of initial hosts */
  byte guid[NUT_GUID_SIZE]; /* this servers GUID */
  hash_t *route;            /* routing table */
  hash_t *conn;             /* connected hosts hash */
  char **search;            /* search pattern array */
  int search_index;         /* current search pattern index */
  int search_limit;         /* limit amount of search reply records */
  hash_t *packet;           /* this servers created packets */
  unsigned errors;          /* routing errors */
  unsigned files;           /* files within connected network */
  unsigned size;            /* file size (in KB) */
  unsigned nodes;           /* hosts within the connected network */
  char *save_path;          /* where to store downloaded files */
  char *share_path;         /* local search database path */
  int dnloads;              /* concurrent downloads */
  int max_dnloads;          /* maximum concurrent downloads */
  int speed;                /* connection speed (KBit/s) */
  int min_speed;            /* minimum connection speed for searching */
  char **extensions;        /* file extensions */
  hash_t *net;              /* host catcher */
  int connections;          /* number of connections to keep up */
  char *force_ip;           /* force the local ip to this value */
  unsigned long ip;         /* calculated from `force_ip' */
  hash_t *query;            /* recent query hash */
  hash_t *reply;            /* reply hash for routing push requests */
  nut_file_t *database;     /* shared file array */
  unsigned db_files;        /* number of database files */
  unsigned db_size;         /* size of database in bytes */
  int uploads;              /* current number of uploads */
  int max_uploads;          /* maximum number of uploads */
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
int nut_server_timer (server_t *server);
char *nut_info_server (server_t *server);
char *nut_info_client (void *nut_cfg, socket_t sock);
int nut_connect_timeout (socket_t sock);

/*
 * This server's definition.
 */
extern server_definition_t nut_server_definition;

#endif /* __GNUTELLA_H__ */
