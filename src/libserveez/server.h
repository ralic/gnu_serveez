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
 * $Id: server.h,v 1.6 2001/04/04 22:20:02 ela Exp $
 *
 */

#ifndef __SERVER_H__
#define __SERVER_H__ 1

#include "libserveez/defines.h"
#include "libserveez/array.h"
#include "libserveez/hash.h"

/*
 * Each server can have a an array of key-value-pairs specific for it.
 * Use macros at end of this file for setting up these.
 */
typedef struct svz_key_value_pair 
{
  int type;        /* data type (string, integer, etc.) */
  char *name;      /* variable name (symbol) */
  int defaultable; /* set if this item is defaultable */
  void *address;   /* memory address of the variable */
}
svz_key_value_pair_t;

typedef struct svz_servertype svz_servertype_t;
typedef struct svz_server svz_server_t;

/*
 * Each server instance gets such a structure.
 */
struct svz_server
{
  /* one of the PROTO_ flags defined in <core.h> */
  int proto;
  /* variable name in configuration language, used to identify it */
  char *name;
  /* server description */
  char *description;
  /* configuration structure for this instance */
  void *cfg;
  /* pointer to this server instance's server type */
  svz_servertype_t *server;

  /* init of instance */
  int (* init) (struct svz_server *);
  /* protocol detection */
  int (* detect_proto) (void *, socket_t);
  /* what to do if detected */ 
  int (* connect_socket) (void *, socket_t);
  /* finalize this instance */
  int (* finalize) (struct svz_server *);
  /* return client info */
  char * (* info_client) (void *, socket_t);
  /* return server info */
  char * (* info_server) (struct svz_server *);
  /* server timer */
  int (* notify) (struct svz_server *);
  /* packet processing */
  int (* handle_request) (socket_t, char *, int);
};

/*
 * Every type (class) of server is completely defined by the following
 * structure.
 */
struct svz_servertype
{
  /* full descriptive name */
  char *name;
  /* varprefix (short name) as used in configuration */
  char *varname;

  /* run once per server definition */
  int (* global_init) (void);
  /* per server instance callback */
  int (* init) (svz_server_t *);
  /* protocol detection routine */
  int (* detect_proto) (void *, socket_t);
  /* for accepting a client (tcp or pipe only) */
  int (* connect_socket) (void *, socket_t);
  /* per instance */
  int (* finalize) (svz_server_t *);
  /* per server definition */
  int (* global_finalize) (void);
  /* return client info */
  char * (* info_client) (void *, socket_t);
  /* return server info */
  char * (* info_server) (svz_server_t *);
  /* server timer */
  int (* notify) (svz_server_t *);
  /* packet processing */
  int (* handle_request) (socket_t, char *, int);

  /* start of example struct */
  void *prototype_start;
  /* size of the above structure */
  int  prototype_size;
  /* array of key-value-pairs of config items */
  svz_key_value_pair_t *items;
};

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
 * Helper cast to get n-th server_t from a (void *).
 */
#define SERVER(addr, no) (((svz_server_t **) addr)[no])

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

SERVEEZ_API extern svz_hash_t *svz_servers;

SERVEEZ_API void svz_server_add __P ((svz_server_t *server));
SERVEEZ_API void svz_server_del __P ((char *name));
SERVEEZ_API svz_server_t *svz_server_find __P ((void *cfg));
SERVEEZ_API void svz_server_notifiers __P ((void));
SERVEEZ_API int svz_server_init_all __P ((void));
SERVEEZ_API int svz_server_finalize_all __P ((void));
SERVEEZ_API int server_portcfg_equal __P ((portcfg_t *a, portcfg_t *b));

SERVEEZ_API extern svz_array_t *svz_servertypes;
SERVEEZ_API void svz_servertype_add __P ((svz_servertype_t *));
SERVEEZ_API void svz_servertype_del __P ((unsigned long index));
SERVEEZ_API void svz_servertype_finalize __P ((void));
SERVEEZ_API svz_servertype_t *svz_servertype_find __P ((svz_server_t *server));

#if ENABLE_DEBUG
SERVEEZ_API void svz_servertype_print __P ((void));
#endif /* ENABLE_DEBUG */

__END_DECLS

#endif /* not __SERVER_H__ */
