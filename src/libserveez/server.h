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
 * $Id: server.h,v 1.14 2001/05/20 20:30:43 ela Exp $
 *
 */

#ifndef __SERVER_H__
#define __SERVER_H__ 1

#include "libserveez/defines.h"
#include "libserveez/array.h"
#include "libserveez/hash.h"
#include "libserveez/portcfg.h"

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
  svz_servertype_t *type;

  /* init of instance */
  int (* init) (struct svz_server *);
  /* protocol detection */
  int (* detect_proto) (void *, svz_socket_t *);
  /* what to do if detected */ 
  int (* connect_socket) (void *, svz_socket_t *);
  /* finalize this instance */
  int (* finalize) (struct svz_server *);
  /* return client info */
  char * (* info_client) (void *, svz_socket_t *);
  /* return server info */
  char * (* info_server) (struct svz_server *);
  /* server timer */
  int (* notify) (struct svz_server *);
  /* packet processing */
  int (* handle_request) (svz_socket_t *, char *, int);
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
  int (* detect_proto) (void *, svz_socket_t *);
  /* for accepting a client (tcp or pipe only) */
  int (* connect_socket) (void *, svz_socket_t *);
  /* per instance */
  int (* finalize) (svz_server_t *);
  /* per server definition */
  int (* global_finalize) (void);
  /* return client info */
  char * (* info_client) (void *, svz_socket_t *);
  /* return server info */
  char * (* info_server) (svz_server_t *);
  /* server timer */
  int (* notify) (svz_server_t *);
  /* packet processing */
  int (* handle_request) (svz_socket_t *, char *, int);

  /* start of example struct */
  void *prototype_start;
  /* size of the above structure */
  int  prototype_size;
  /* array of key-value-pairs of config items */
  svz_key_value_pair_t *items;
};

/*
 * This structure defines the callbacks for the @code{svz_server_configure}
 * function. Each of these have the following arguments:
 * server: might be the name of the server instance to configure
 * arg:    an optional argument (e.g. scheme cell)
 * name:   the name of the configuration item
 * target: target address of the configuration item
 * def:    the default value for this configuration item
 */
typedef struct
{
  int (* integer)  (char *server, void *arg, char *name,
		    int *target, int def);
  int (* boolean)  (char *server, void *arg, char *name,
		    int *target, int def);
  int (* intarray) (char *server, void *arg, char *name,
		    svz_array_t **target, svz_array_t *def);
  int (* string)   (char *server, void *arg, char *name, 
		    char **target, char *def);
  int (* strarray) (char *server, void *arg, char *name,
		    svz_array_t **target, svz_array_t *def);
  int (* hash)     (char *server, void *arg, char *name, 
		    svz_hash_t **target, svz_hash_t *def);
  int (* portcfg)  (char *server, void *arg, char *name, 
		    svz_portcfg_t **target, svz_portcfg_t *def);
}
svz_server_config_t;

/* Constants for the @var{defaultable} argument. */
#define DEFAULTABLE     1
#define NOTDEFAULTABLE  0

/* Configuration item identifier. */
#define ITEM_END      0
#define ITEM_INT      1
#define ITEM_INTARRAY 2
#define ITEM_STR      3
#define ITEM_STRARRAY 4
#define ITEM_HASH     5
#define ITEM_PORTCFG  6
#define ITEM_BOOL     7

/*
 * Macro for defining the example server configuration @var{config} and its
 * configuration items @var{prototypes} within a server type definition.
 */
#define SVZ_DEFINE_CONFIG(config, prototypes) \
  &(config), sizeof (config), (prototypes)

/* Return a text representation of an item. */
#define ITEM_TEXT(i)                                  \
  ((i) == ITEM_INT) ? "integer" :                     \
  ((i) == ITEM_INTARRAY) ? "integer array" :          \
  ((i) == ITEM_STR) ? "string" :                      \
  ((i) == ITEM_STRARRAY) ? "string array" :           \
  ((i) == ITEM_HASH) ? "hash table" :                 \
  ((i) == ITEM_BOOL) ? "boolean" :                    \
  ((i) == ITEM_PORTCFG) ? "port configuration" : NULL

/*
 * Register a simple integer. C-type: @code{int}. The given @var{name} 
 * specifies the symbolic name of the integer and @var{item} the integer 
 * itself (not its address). The @var{defaultable} argument can be either 
 * @code{DEFAULTABLE} or @code{NOTDEFAULTABLE}.
 */
#define SVZ_REGISTER_INT(name, item, defaultable) \
  { ITEM_INT, (name), (defaultable), &(item) }

/*
 * Register an array of integers. C-type: @code{svz_array_t *}.
 */
#define SVZ_REGISTER_INTARRAY(name, item, defaultable) \
  { ITEM_INTARRAY, (name), (defaultable), &(item) }

/*
 * Register a boolean value. C-type: @code{int}.
 */
#define SVZ_REGISTER_BOOL(name, item, defaultable) \
  { ITEM_BOOL, (name), (defaultable), &(item) }

/*
 * Register a simple character string. C-type: @code{char *}.
 */
#define SVZ_REGISTER_STR(name, item, defaultable) \
  { ITEM_STR, (name), (defaultable), &(item) }

/*
 * Register a string array. C-type: @code{svz_array_t *}.
 */
#define SVZ_REGISTER_STRARRAY(name, item, defaultable) \
  { ITEM_STRARRAY, (name), (defaultable), &(item) }

/*
 * Register a hash table associating strings with strings only. C-type:
 * @code{svz_hash_t *}.
 */
#define SVZ_REGISTER_HASH(name, item, defaultable) \
  { ITEM_HASH, (name), (defaultable), &(item) }

/*
 * Register a port configuration. C-type: @code{svz_portcfg_t *}.
 */
#define SVZ_REGISTER_PORTCFG(name, item, defaultable) \
  { ITEM_PORTCFG, (name), (defaultable), &(item) }

/*
 * This macro indicates the end of the list of configuration items. It is
 * the only mandatory item you need to specify in an example server type
 * configuration.
 */
#define SVZ_REGISTER_END() \
  { ITEM_END, NULL, DEFAULTABLE, NULL }

__BEGIN_DECLS

SERVEEZ_API extern svz_hash_t *svz_servers;

SERVEEZ_API void svz_server_add __P ((svz_server_t *));
SERVEEZ_API svz_server_t *svz_server_get __P ((char *));
SERVEEZ_API void svz_server_del __P ((char *));
SERVEEZ_API void svz_server_free __P ((svz_server_t *server));
SERVEEZ_API void svz_config_free __P ((svz_servertype_t *server, void *cfg));
SERVEEZ_API svz_server_t *svz_server_find __P ((void *));
SERVEEZ_API void svz_server_notifiers __P ((void));
SERVEEZ_API svz_server_t *svz_server_instantiate __P ((svz_servertype_t *, 
						       char *));

SERVEEZ_API void *svz_server_configure __P ((svz_servertype_t *, char *, 
					     void *, svz_server_config_t *));
SERVEEZ_API svz_array_t *svz_config_intarray_create __P ((int *intarray));
SERVEEZ_API void svz_config_intarray_destroy __P ((svz_array_t *intarray));
SERVEEZ_API svz_array_t *svz_config_intarray_dup __P ((svz_array_t *intarray));
SERVEEZ_API svz_array_t *svz_config_strarray_create __P ((char **strarray));
SERVEEZ_API void svz_config_strarray_destroy __P ((svz_array_t *strarray));
SERVEEZ_API svz_array_t *svz_config_strarray_dup __P ((svz_array_t *strarray));
SERVEEZ_API svz_hash_t *svz_config_hash_create __P ((char **strarray));
SERVEEZ_API void svz_config_hash_destroy __P ((svz_hash_t *strhash));
SERVEEZ_API svz_hash_t *svz_config_hash_dup __P ((svz_hash_t *strhash));

SERVEEZ_API int svz_server_init_all __P ((void));
SERVEEZ_API int svz_server_finalize_all __P ((void));

SERVEEZ_API extern svz_array_t *svz_servertypes;
SERVEEZ_API void svz_servertype_add __P ((svz_servertype_t *));
SERVEEZ_API void svz_servertype_del __P ((unsigned long));
SERVEEZ_API void svz_servertype_finalize __P ((void));
SERVEEZ_API svz_servertype_t *svz_servertype_find __P ((svz_server_t *));

#if ENABLE_DEBUG
SERVEEZ_API void svz_servertype_print __P ((void));
#endif /* ENABLE_DEBUG */

__END_DECLS

#endif /* not __SERVER_H__ */
