/*
 * server.h - generic server definitions
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
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
 * $Id: server.h,v 1.4 2001/03/04 13:13:41 ela Exp $
 *
 */

#ifndef __SERVER_H__
#define __SERVER_H__ 1

#include "libserveez/defines.h"

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

/*
 * Each server can have a an array of name-value-pairs specific for it.
 * Use macros at end of this file for setting up these.
 */
typedef struct key_value_pair 
{
  int type;        /* data type (string, integer, etc.) */
  char *name;      /* variable name (symbol) */
  int defaultable; /* set if this item is defaultable */
  void *address;   /* memory address of the variable */
}
key_value_pair_t;

/*
 * Each server instance gets such a structure.
 */
typedef struct server
{
  int  proto;           /* one of the PROTO_ flags */
  char *name;           /* variable name in sizzle, used to identify it */
  char *description;    /* server description */
  void *cfg;                                    /* configuration structure */
  int (* init)(struct server *);                /* init of instance */
  int (* detect_proto) (void *, socket_t);      /* protocol detection */
  int (* connect_socket) (void *, socket_t);    /* what to do if detected */ 
  int (* finalize)(struct server *);            /* finalize this instance */
  char * (* info_client)(void *, socket_t);     /* return client info */
  char * (* info_server)(struct server *);      /* return server info */
  int (* notify)(struct server *);              /* server timer */
  int (* handle_request)(socket_t, char*, int); /* packet processing */
}
server_t;

/*
 * Used when binding ports this is available from sizzle 
 * and set as a hash:
 *
 *  "proto"    => String: "tcp", "udp", "pipe" or "icmp"
 *  "port"     => Integer: for tcp/udp ports
 *  "ipaddr  " => String: (dotted decimal) for local address or "*" (default)
 *  "inpipe"   => String: pipe for sending data into serveez
 *  "outpipe"  => String: pipe serveez sends responses out on
 */
typedef struct portcfg
{
  int proto;                   /* one of the PROTO_ flags */
  unsigned short port;         /* ip port (TCP and UDP) */
  char *ipaddr;                /* dotted decimal or "*" */
  struct sockaddr_in *addr;    /* converted from the above 2 values */

  /* Pipe */
  char *inpipe;
  char *outpipe;
}
portcfg_t;

/*
 * Every server needs such a thing.
 */
typedef struct server_definition 
{
  char *name;                                   /* descriptive name */
  char *varname;                                /* varprefix as used in cfg */

  int (* global_init)(void);                    /* run once per server def */
  int (* init)(struct server*);                 /* per instance callback */
  int (* detect_proto)(void *, socket_t);       /* protocol detector */
  int (* connect_socket)(void *, socket_t);     /* for accepting a client */
  int (* finalize)(struct server*);             /* per instance */
  int (* global_finalize)(void);                /* per server def */
  char * (* info_client)(void *, socket_t);     /* return client info */
  char * (* info_server)(struct server *);      /* return server info */
  int (* notify)(struct server *);              /* server timer */
  int (* handle_request)(socket_t, char*, int); /* packet processing */

  void *prototype_start;                        /* start of example struct */
  int  prototype_size;                          /* sizeof() the above */

  struct key_value_pair *items;                 /* array of key-value-pairs */
                                                /* of config items */
}
server_definition_t;

/*
 * This structure is used by `server_bind ()' to collect various server
 * instances and their port configurations.
 */
typedef struct
{
  server_t *server; /* server instance */
  portcfg_t *cfg;   /* port configuration */
}
server_binding_t;


/*
 * Helper cast to get n-th server_t from a (void *).
 */
#define SERVER(addr, no) (((server_t **) addr)[no])

/*
 * Helper macros for filling the config prototypes.
 */
#define DEFAULTABLE     1
#define NOTDEFAULTABLE  0

#define ITEM_END      0
#define ITEM_INT      1
#define ITEM_INTARRAY 2
#define ITEM_STR      3
#define ITEM_STRARRAY 4
#define ITEM_HASH     5
#define ITEM_PORTCFG  6

/*
 * A simple integer.
 */
#define REGISTER_INT(name, address, defaultable) \
  { ITEM_INT, name, defaultable, &address }

/*
 * An integer array:
 * [0] is length, followed by integers.
 */
#define REGISTER_INTARRAY(name, address, defaultable) \
  { ITEM_INTARRAY, name, defaultable, &(address) }

/*
 * A string (char *).
 */
#define REGISTER_STR(name, address, defaultable) \
  { ITEM_STR, name, defaultable, &(address) }

/*
 * A string array, NULL terminated list of pointers.
 */
#define REGISTER_STRARRAY(name, address, defaultable) \
  { ITEM_STRARRAY, name, defaultable, &(address) }

/*
 * A hash table associating strings with strings.
 */
#define REGISTER_HASH(name, address, defaultable) \
  { ITEM_HASH, name, defaultable, &(address) }

/*
 * A port configuration.
 */
#define REGISTER_PORTCFG(name, address, defaultable) \
  { ITEM_PORTCFG, name, defaultable, &(address) }

/*
 * Dummy for terminating the list.
 */
#define REGISTER_END() \
  { ITEM_END, NULL, 0, NULL }

__BEGIN_DECLS

SERVEEZ_API extern int server_definitions;
SERVEEZ_API extern struct server_definition **server_definition;
SERVEEZ_API extern int server_instances;
SERVEEZ_API extern struct server **servers;

SERVEEZ_API void server_add_definition __P ((server_definition_t *));
SERVEEZ_API int server_start __P ((void));
SERVEEZ_API int server_bind __P ((server_t *server, portcfg_t *cfg));
SERVEEZ_API server_t *server_find __P ((void *cfg));
SERVEEZ_API void server_add __P ((struct server *server));
SERVEEZ_API void server_run_notify __P ((void));
SERVEEZ_API int server_portcfg_equal __P ((portcfg_t *a, portcfg_t *b));
SERVEEZ_API int server_global_init __P ((void));
SERVEEZ_API int server_init_all __P ((void));
SERVEEZ_API int server_finalize_all __P ((void));
SERVEEZ_API int server_global_finalize __P ((void));

#if ENABLE_DEBUG
SERVEEZ_API void server_print_definitions __P ((void));
#endif /* ENABLE_DEBUG */

__END_DECLS

#endif /* not __SERVER_H__ */
