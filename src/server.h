/*
 * src/server.h - Generic server definitions
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: server.h,v 1.3 2000/06/18 16:25:19 ela Exp $
 *
 */

#ifndef __SERVER_H__
#define __SERVER_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "socket.h"

/*
 * Use my functions:
 */
int server_load_cfg (char *cfgfile);
int server_global_init (void);
int server_init_all (void);
int server_finalize_all (void);
int server_global_finalize (void);
#if ENABLE_DEBUG
void server_show_definitions (void);
#endif


#define PROTO_TCP   0x00000001
#define PROTO_UDP   0x00000002
#define PROTO_PIPE  0x00000004

/*
 * Each server can have a an array of name-value-pairs specific for it.
 * Use macros at end ot this file for setting up
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
 * Each server instance gets such a struct
 */
typedef struct server
{
  int  proto;           /* one of the PROTO_ flags */
  char *name;           /* Variable name in sizzle, used to identify it */
  void *cfg;                                 /* configuration structure */
  int (* init)(struct server *);             /* Init of instance*/
  int (* detect_proto) (void *, socket_t);   /* protocol detection */
  int (* connect_socket) (void *, socket_t); /* what to do if detected */ 
  int (* finalize)(struct server *);         /* finalize this instance */
}
server_t;

/*
 * Used when binding ports
 */
typedef struct portcfg
{
  int proto;            /* one of the PROTO_ flags */

  /* TCP and UDP */
  int port;

  /* Pipe */
  char *inpipe;
  char *outpipe;
}
portcfg_t;


/*
 * Every server needs such a thing
 */
typedef struct server_definition {
  char *name;                                /* Descriptive name of server */
  char *varname;                             /* varprefix as used in cfg   */

  int (* global_init)(void);                 /* Run once per serverdef.    */
  int (* init)(struct server*);              /* per instance callback      */
  int (* detect_proto)(void *, socket_t);    /* Protocol detector          */
  int (* connect_socket)(void *, socket_t);  /* For accepting a client     */
  int (* finalize)(struct server*);          /* per instance               */
  int (* global_finalize)(void);             /* per serverdef              */

  void *prototype_start;                     /* Start of example struct    */
  int  prototype_size;                       /* sizeof() the above         */

  struct key_value_pair *items;              /* Array of name-value-pairs  */
                                             /* of config items            */
}
server_definition_t;

extern struct server_definition *all_server_definitions[];
extern int server_instances;
extern struct server **servers;

/*
 * Helper cast to get n-th server_t from a void*
 */
#define SERVER(addr, no) (((server_t **) addr)[no])

/*
 * Helper macros for filling the config prototypes
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
 * A simple int
 */
#define REGISTER_INT(name, address, defaultable) \
  { ITEM_INT, name, defaultable, &address }

/*
 * An int array:
 * [0] is length, followed by ints
 */
#define REGISTER_INTARRAY(name, address, defaultable)\
  { ITEM_INTARRAY, name, defaultable, &(address) }

/*
 * A string, char *
 */
#define REGISTER_STR(name, address, defaultable) \
  { ITEM_STR, name, defaultable, &(address) }

/*
 * A string array, NULL terminated pointerlist
 */
#define REGISTER_STRARRAY(name, address, defaultable) \
  { ITEM_STRARRAY, name, defaultable, &(address) }

/*
 * A hashtable associating strings with strings
 */
#define REGISTER_HASH(name, address, defaultable) \
  { ITEM_HASH, name, defaultable, &(address) }

/*
 * A Port configuration
 */
#define REGISTER_PORTCFG(name, address, defaultable) \
  { ITEM_PORTCFG, name, defaultable, &(address) }

/*
 * Dummy for ending the list
 */
#define REGISTER_END() \
  { ITEM_END, NULL, 0, NULL }

#endif
