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
 * $Id: server.h,v 1.1 2001/01/28 03:26:55 ela Exp $
 *
 */

#ifndef __SERVER_H__
#define __SERVER_H__ 1

#include "libserveez/defines.h"

/* protocol definitions */
#define PROTO_TCP   0x00000001 /* tcp  - bidirectional, reliable */
#define PROTO_UDP   0x00000002 /* udp  - multidirectional, unreliable */
#define PROTO_PIPE  0x00000004 /* pipe - unidirectional, reliable */
#define PROTO_ICMP  0x00000008 /* icmp - multidirectional, unreliable */
#define PROTO_RAW   0x00000010 /* raw  - multidirectional, unreliable */

/*
 * Each server can have a an array of name-value-pairs specific for it.
 * Use macros at end ot this file for setting up these.
 */
typedef struct key_value_pair 
{
  int type;
  char *name;
  int defaultable;
  void *address;
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
 * Every server needs such a thing
 */
typedef struct server_definition 
{
  char *name;                                   /* escriptive name of server */
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
 * This structure is used by server_bind () to collect various server
 * instances and their port configurations.
 */
typedef struct
{
  server_t *server; /* server instance */
  portcfg_t *cfg;   /* port configuration */
}
server_binding_t;

__BEGIN_DECLS

SERVEEZ_API extern int server_definitions;
SERVEEZ_API extern struct server_definition **server_definition;
SERVEEZ_API extern int server_instances;
SERVEEZ_API extern struct server **servers;

SERVEEZ_API void server_add_definition __P ((server_definition_t *));

/*
 * Start all server bindings (instances of servers).
 */
SERVEEZ_API int server_start __P ((void));

/*
 * This functions binds a previouly instanciated server to a specified
 * port configuration.
 */
SERVEEZ_API int server_bind __P ((server_t *server, portcfg_t *cfg));

/*
 * Find a server instance by a given configuration structure. Return NULL
 * if there is no such configuration.
 */
SERVEEZ_API server_t *server_find __P ((void *cfg));

/*
 * Add a server to the list of all servers.
 */
SERVEEZ_API void server_add __P ((struct server *server));

/*
 * Run all the server instances's timer routines. This is called within
 * the server_periodic_tasks() function in `server-core.c'.
 */
SERVEEZ_API void server_run_notify __P ((void));

/*
 * Compare if two given portcfg structures are equal i.e. specifying 
 * the same port. Returns non-zero if a and b are equal.
 */
SERVEEZ_API int server_portcfg_equal __P ((portcfg_t *a, portcfg_t *b));

/*
 * Use these functions.
 */
SERVEEZ_API int server_global_init __P ((void));
SERVEEZ_API int server_init_all __P ((void));
SERVEEZ_API int server_finalize_all __P ((void));
SERVEEZ_API int server_global_finalize __P ((void));

#if ENABLE_DEBUG
SERVEEZ_API void server_print_definitions __P ((void));
#endif

__END_DECLS

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
 * A simple int.
 */
#define REGISTER_INT(name, address, defaultable) \
  { ITEM_INT, name, defaultable, &address }

/*
 * An int array:
 * [0] is length, followed by ints.
 */
#define REGISTER_INTARRAY(name, address, defaultable)\
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
 * A hashtable associating strings with strings.
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

#endif /* not __SERVER_H__ */
