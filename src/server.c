/*
 * src/server.c - register your servers here
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
 * $Id: server.c,v 1.28 2000/10/05 09:52:20 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <libsizzle/libsizzle.h>

/* for backward compatibility with older versions of sizzle core */
#ifndef hashtable_p
# define hashtable_p(c) vector_p(c)
#endif
#ifndef zzz_interaction_environment
# define zzz_interaction_environment zzz_toplevel_env
#endif

#ifdef __MINGW32__
# include <winsock.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#include "alloc.h"
#include "util.h"
#include "hash.h"
#include "server-core.h"
#include "server-socket.h"
#include "server.h"

/* 
 * Include headers of servers.
 */
#include "foo-server/foo-proto.h"
#if ENABLE_AWCS_PROTO
# include "awcs-server/awcs-proto.h"
#endif
#if ENABLE_HTTP_PROTO
# include "http-server/http-proto.h"
#endif
#if ENABLE_IRC_PROTO
# include "irc-server/irc-proto.h"
#endif
#if ENABLE_CONTROL_PROTO
# include "ctrl-server/control-proto.h"
#endif
#if ENABLE_Q3KEY_PROTO
# include "q3key-server/q3key-proto.h"
#endif
#if ENABLE_GNUTELLA
# include "nut-server/gnutella.h"
#endif

/*
 * The list of registered server. Feel free to add yours.
 */
struct server_definition * all_server_definitions [] = 
{
  &foo_server_definition,
#if ENABLE_AWCS_PROTO
  &awcs_server_definition,
#endif
#if ENABLE_HTTP_PROTO
  &http_server_definition,
#endif
#if ENABLE_IRC_PROTO
  &irc_server_definition,
#endif
#if ENABLE_CONTROL_PROTO
  &ctrl_server_definition,
#endif
#if ENABLE_Q3KEY_PROTO
  &q3key_server_definition,
#endif
#if ENABLE_GNUTELLA
  &nut_server_definition,
#endif
  NULL
};

/*
 * A list of actually instantiated servers.
 */
int server_instances = 0;
struct server **servers = NULL;

/*
 * A list of bound servers.
 */
int server_bindings = 0;
server_binding_t *server_binding = NULL;

/*
 * Local forward declarations.
 */
static int set_int (char *, char *, char *, int *, zzz_scm_t, int);
static int set_intarray (char*, char*, char*, int **, zzz_scm_t, int*);
static int set_string (char*, char*, char*, char**, zzz_scm_t, char*);
static int set_stringarray (char*, char*, char*, char***, zzz_scm_t, char**);
static int set_hash (char*, char*, char*, hash_t***, zzz_scm_t, hash_t**);
static int set_port (char*, char*, char*, struct portcfg **, zzz_scm_t,
		     struct portcfg *);
static void * server_instantiate (char*, zzz_scm_t, 
				  struct server_definition *, char *);
static void server_add (struct server *);

/*
 * Helpers to extract info from val. If val is NULL then use the default.
 */
static int
set_int (char *cfgfile, 
	 char *var, 
	 char *key, 
	 int *location,
	 zzz_scm_t val, 
	 int def)
{
  if (val == zzz_undefined) 
    {
      (*location) = def;
      return 0;
    }
  
  if (!integer_p (val))
    {
      fprintf (stderr,
	       "%s: property `%s' of `%s' should be an integer but is not\n",
	       cfgfile, key, var);
      return -1;
    }

  /* FIXME: do strings and atoi() ? */
  (*location) = integer_val (val);
  return 0;
}

static int
set_intarray (char *cfgfile, char *var, char *key, int **location,
	      zzz_scm_t val, int* def)
{
  int erroneous = 0;
  int i;
  int *array = NULL;

  if (val == zzz_undefined) 
    {
      (*location) = def;
      return 0;
    }

  array = (int *) xpmalloc (sizeof (int) * (zzz_list_length (val) +1));

  for (i = 1; cons_p (val); i++, val = cdr (val))
    {
      if (!integer_p (car (val))) 
	{
	  fprintf  (stderr,
		    "%s: element %d of list `%s' is not an integer in `%s'\n",
		    cfgfile, i - 1, key, var);
	  erroneous = -1;
	} 
      else 
	{
	  array[i] = integer_val (car (val));
	}
    }
  array[0] = i;

  (*location) = array;

  return erroneous;
}

static int
set_string (char *cfgfile, char *var, char *key, char **location,
	    zzz_scm_t val, char *def)
{
  if (val == zzz_undefined) 
    {
      if (def == NULL)
	(*location) = NULL;
      else
	(*location) = xpstrdup (def);
      return 0;
    }

  if (!string_p (val))
    {
      fprintf (stderr,
	       "%s: property `%s' of `%s' should be a string but is not\n",
	       cfgfile, key, var);
      return -1;
    }

  (*location) = xpstrdup (string_val (val)); 
  return 0;
}

static int
set_stringarray (char *cfgfile, char *var, char *key, char*** location,
		 zzz_scm_t val, char** def)
{
  int erroneous = 0;
  int i;
  char **array = NULL;

  if (val == zzz_undefined)
    {
      (*location) = def;
      return 0;
    }

  if (!list_p (val))
    {
      fprintf (stderr,
	       "%s: property `%s' of `%s' should be a list but is not\n",
	       cfgfile, key, var);
      return -1;
    }

  array = (char**) xpmalloc (sizeof (char *) * (zzz_list_length (val) + 1));

  for (i = 0; cons_p (val); i++, val = cdr (val))
    {
      if (!string_p (car (val))) 
	{
	  fprintf (stderr,
		   "%s: element `%d' of list `%s' is not a string in `%s'\n",
		   cfgfile, i, key, var);
	  erroneous = -1;
	} 
      else 
	{
	  array[i] = xpstrdup (string_val (car (val)));
	}
    }
  array[i] = NULL;

  (*location) = array;
  return erroneous;
}

static int
set_hash (char *cfgfile, char *var, char *key, hash_t ***location,
	  zzz_scm_t val, hash_t **def)
{
  int erroneous = 0;
  unsigned int i;
  zzz_scm_t foo;
  hash_t *h;
  hash_t **href = xpmalloc (sizeof (hash_t **));

  if (val == zzz_undefined) 
    {
      if (def == NULL)
	{
	  (*href) = NULL;
	  (*location) = href;
	}
	else
	{
	  (*location) = def;
	}
      return 0;
    }

  if (!hashtable_p (val)) 
    {
      fprintf (stderr, 
	       "%s: element `%s' of `%s' should be a hash but is not\n",
	       cfgfile, key, var);
      return -1;
    }

  /*
   * Don't forget to free in instance finalizer.
   */
  h = hash_create (4);

  for (i = 0; i < vector_len (val); i++)
    {
      for (foo = zzz_vector_get (val, i); cons_p (foo); foo = cdr (foo))
	{
	  if (!string_p (car (car (foo)))) 
	    {
	      fprintf (stderr, "%s: hash `%s' in `%s' is broken\n",
		       cfgfile, key, var);
	      erroneous = -1;
	      continue;
	    }

	  if (!string_p (cdr (car (foo)))) 
	    {
	      fprintf (stderr, 
		       "%s: %s: value of `%s' is not a string in `%s'\n",
		       cfgfile, var, string_val (car (car (foo))), 
		       key);
	      erroneous = -1;
	      continue;
	    }

	  /*
	   * The hash keeps a copy of the key itself.
	   */
	  hash_put (h, string_val (car (car (foo))),
		    xstrdup (string_val (cdr (car (foo)))));
	}
    }

  (*href) = h;
  (*location) = href;
  
  return erroneous;
}

#define PORTCFG_PORT    "port"
#define PORTCFG_PROTO   "proto"
#define PORTCFG_INPIPE  "inpipe"
#define PORTCFG_OUTPIPE "outpipe"
#define PORTCFG_TCP     "tcp"
#define PORTCFG_UDP     "udp"
#define PORTCFG_ICMP    "icmp"
#define PORTCFG_PIPE    "pipe"
#define PORTCFG_IP      "local-ip"
#define PORTCFG_NOIP    "*"

/*
 * Set a portcfg from a scheme variable. The default value is copied.
 */
static int set_port (char *cfgfile, char *var, char *key, 
		     struct portcfg **location, 
		     zzz_scm_t val, struct portcfg *def)
{
  zzz_scm_t hash_key = NULL;
  zzz_scm_t hash_val = NULL;
  char *tstr = NULL;
  struct portcfg *newport;
  struct sockaddr_in *newaddr;

  /* First, fill the structure with the values we know already. */
  if (val == zzz_undefined)
    {
      if (def == NULL) 
	{
	  fprintf (stderr, "%s: the default value of a portcfg may not be "
		   "a null pointer (`%s' in `%s')\n",
		   cfgfile, key, var);
	  return -1;
	}

      (*location) = (struct portcfg *) xpmalloc (sizeof (struct portcfg));
      memcpy ((*location), def, sizeof (struct portcfg));
      newport = (*location);
    } 
  else 
    {
      newport = (struct portcfg *) xpmalloc (sizeof (struct portcfg));
    
      if (!hashtable_p (val)) 
	{
	  fprintf (stderr,
		   "%s: portcfg `%s' of `%s' does not specify a protocol\n",
		   cfgfile, key, var);
	  return -1;
	}

      /* First, find out what kind of port is about to be recognized. */
      hash_key = zzz_make_string (PORTCFG_PROTO, -1);
      hash_val = zzz_hash_ref (val, hash_key, zzz_undefined);

      if (!string_p (hash_val)) 
	{
	  fprintf (stderr,
		   "%s: `%s' of portcfg `%s' of `%s' should be a string\n",
		   cfgfile, PORTCFG_PROTO, key, var);
	  return -1;
	}

      tstr = string_val (hash_val);

      if (!strcmp (tstr, PORTCFG_TCP) || !strcmp (tstr, PORTCFG_UDP) ||
	  !strcmp (tstr, PORTCFG_ICMP)) 
	{
	  /* This is tcp or udp, both share the local address. */
	  if (!strcmp (tstr, PORTCFG_TCP))
	    newport->proto = PROTO_TCP;
	  else if (!strcmp (tstr, PORTCFG_UDP))
	    newport->proto = PROTO_UDP;
	  else if (!strcmp (tstr, PORTCFG_ICMP))
	    newport->proto = PROTO_ICMP;

	  /* Figure out the port value and set it. */
	  hash_key = zzz_make_string (PORTCFG_PORT, -1);
	  hash_val = zzz_hash_ref (val, hash_key, zzz_undefined);
      
	  if (!integer_p (hash_val)) 
	    {
	      fprintf (stderr, "%s: `%s': %s is not numerical in `%s'\n",
		       cfgfile, var, PORTCFG_PORT, key);
	      return -1;
	    }
	  newport->port = (unsigned short int) integer_val (hash_val);

	  /* Figure out the local ip address, "*" means any. */
	  hash_key = zzz_make_string (PORTCFG_IP, -1);
	  hash_val = zzz_hash_ref (val, hash_key, zzz_undefined);

	  if (hash_val == zzz_undefined) 
	    {
	      newport->localip = PORTCFG_NOIP;
	    } 
	  else if (string_p (hash_val)) 
	    {
	      newport->localip = xpstrdup (string_val (hash_val));
	    }
	  else 
	    {
	      fprintf (stderr, 
		       "%s: `%s': %s should be a string in `%s'\n",
		       cfgfile, var, PORTCFG_IP, key);
	      return -1;
	    }
	} 
      else if (!strcmp (tstr, PORTCFG_PIPE)) 
	{
	  newport->proto = PROTO_PIPE;

	  hash_key = zzz_make_string (PORTCFG_INPIPE, -1);
	  hash_val = zzz_hash_ref (val, hash_key, zzz_undefined);

	  if (!string_p (hash_val)) 
	    {
	      fprintf (stderr, 
		       "%s: `%s': %s should be a string in `%s'\n",
		       cfgfile, var, PORTCFG_INPIPE, key);
	      return -1;
	    }
	  newport->inpipe = xpstrdup (string_val (hash_val));

	  hash_key = zzz_make_string (PORTCFG_OUTPIPE, -1);
	  hash_val = zzz_hash_ref (val, hash_key, zzz_undefined);

	  if (!string_p (hash_val)) 
	    {
	      fprintf (stderr, 
		       "%s: `%s': %s should be a string in `%s'\n",
		       cfgfile, var, PORTCFG_OUTPIPE, key);
	      return -1;
	    }

	  newport->outpipe = xpstrdup (string_val (hash_val));

	} 
      else 
	{
	  fprintf (stderr,
		   "%s: `%s' of portcfg `%s' of `%s' does not specify a "
		   "valid protocol (%s, %s, %s, %s)\n",
		   cfgfile, PORTCFG_PROTO, key, var, 
		   PORTCFG_TCP, PORTCFG_UDP, PORTCFG_PIPE, PORTCFG_ICMP);
	  return -1;
	}
    
      (*location) = newport;
    }

  /* Second, fill the sockaddr struct from the values we just read. */
  if (newport->proto & (PROTO_TCP | PROTO_UDP | PROTO_ICMP)) 
    {
      /* prepate the local address structure */
      newaddr = (struct sockaddr_in*) xpmalloc (sizeof (struct sockaddr_in));
      newport->localaddr = newaddr;
      memset (newaddr, 0, sizeof (struct sockaddr_in));

      /* ...and the local ip address with "*" being any */
      if (!strcmp (newport->localip, PORTCFG_NOIP)) 
	{
	  newaddr->sin_addr.s_addr = INADDR_ANY;
	} 
      else 
	{
#if HAVE_INET_ATON
	  if (inet_aton (string_val (hash_val), &newaddr->sin_addr) == 0) 
	    {
	      fprintf (stderr, "%s: `%s': %s should be an ip address "
		       "in dotted decimal form in `%s'\n",
		       cfgfile, var, PORTCFG_IP, key);
	      return -1;
	    }
#else /* not HAVE_INET_ATON */
	  newaddr->sin_addr.s_addr = inet_addr (string_val (hash_val));
#endif /* not HAVE_INET_ATON */
	}

      /* this surely is internet */
      newaddr->sin_family = AF_INET;

      /* determine port... */
      newaddr->sin_port = htons (newport->port);
    }
  
  return 0;
}

/*
 * Instantiate a server, given a server_definition and a sizzle hash.
 */
static void *
server_instantiate (char *cfgfile, zzz_scm_t hash,
		    server_definition_t *sd, char *var)
{
  void *cfg = NULL;
  void *target = NULL;
  unsigned long offset;
  int i, e = 0;
  int erroneous = 0;
  zzz_scm_t hashkey;
  zzz_scm_t hashval;

  /* 
   * Checking if that is really a hash.
   */
  if (!hashtable_p (hash) || error_p (hash)) 
    {
      fprintf (stderr, "%s: `%s' is not a hash\n", cfgfile, var);
      return NULL;
    }

  /*
   * Make a simple copy of the example configuration structure 
   * definition for that server instance.
   */
  cfg = xpmalloc (sd->prototype_size);
  memcpy (cfg, sd->prototype_start, sd->prototype_size);

  /* Go through list of configuration items. */
  for (i = 0; sd->items[i].type != ITEM_END; i++)
    {
      hashkey = zzz_make_string (sd->items[i].name, -1);
      hashval = zzz_hash_ref (hash, hashkey, zzz_undefined);

      if (hashval == zzz_undefined && !sd->items[i].defaultable)
	{
	  fprintf (stderr,
		   "%s: `%s' does not define a default for `%s' in `%s'\n",
		   cfgfile, sd->name, sd->items[i].name, var);
	  erroneous = -1;
	  continue;
	}

      /* Calculate target address. */
      offset = (char *) sd->items[i].address - (char *) sd->prototype_start;
      target = (char *) cfg + offset;

      switch (sd->items[i].type) 
	{
	case ITEM_INT:
	  e = set_int (cfgfile, var, sd->items[i].name,
		       (int *) target, hashval, 
		       *(int *) sd->items[i].address);
	  break;
	  
	case ITEM_INTARRAY:
	  e = set_intarray (cfgfile, var, sd->items[i].name,
			    (int **) target, hashval,
			    *(int **) sd->items[i].address);
	  break;

	case ITEM_STR:
	  e = set_string (cfgfile, var, sd->items[i].name,
			  (char **) target, hashval,
			  *(char **) sd->items[i].address);
	  break;
	  
	case ITEM_STRARRAY:
	  e = set_stringarray (cfgfile, var, sd->items[i].name,
			       (char ***) target, hashval,
			       *(char ***)sd->items[i].address);
	  break;

	case ITEM_HASH:
	  e = set_hash (cfgfile, var, sd->items[i].name,
			(hash_t ***) target, hashval,
			*(hash_t ***) sd->items[i].address);
	  break;

	case ITEM_PORTCFG:
	  e = set_port (cfgfile, var, sd->items[i].name,
			(portcfg_t **) target, hashval,
			*(portcfg_t **) sd->items[i].address);
	  break;

	default:
	  fprintf (stderr, "fatal: inconsistent data in " __FILE__ "\n");
	  erroneous = -1;
	}

      /* propagate error, if any */
      if (e < 0)
	erroneous = -1;
    }

  return (erroneous ? NULL : cfg);
}

/*
 * Get server settings from a file and instantiate servers as needed.
 */
int
server_load_cfg (char *cfgfile)
{
  char *symname = NULL;
  zzz_scm_t symlist = NULL;
  zzz_scm_t sym = NULL;
  zzz_scm_t symval = NULL;
  unsigned int i, j;
  int erroneous = 0;
  server_definition_t *sd;
  server_t *server;
  void *cfg;

  /*
   * If the file was not found, the environment should be ok anyway.
   * Trying to use defaults...
   */

  /* Scan for variables that could be meant for us. */
  for (i = 0; i < vector_len (zzz_symbol_table); i++)
    {
      for (symlist = zzz_vector_get (zzz_symbol_table, i);
	   cons_p (symlist);
	   symlist = cdr (symlist))
	{
	  sym = car (symlist);
	  symname = xstrdup (string_val (symbol_name (sym)));

	  for (j = 0; NULL != (sd = all_server_definitions[j]); j++)
	    {
	      /* 
	       * A varname is meant for us if it begins like one of our
	       * server definitions and ends with a '-'. e.g.: foo => foo-
	       */
	      if (!strncmp (symname, sd->varname, strlen (sd->varname)) &&
		  symname[strlen (sd->varname)] == '-')
		{
		  if (zzz_get_symbol_value (zzz_interaction_environment, 
					    sym, &symval) != RESULT_SUCCESS)
		    continue;

		  cfg = server_instantiate (cfgfile, symval,
					    sd, symname);

		  if (cfg != NULL)
		    {
		      server = (server_t *) xpmalloc (sizeof (server_t));
		      server->cfg = cfg;
		      server->name = xpstrdup (symname);
		      server->detect_proto = sd->detect_proto;
		      server->connect_socket = sd->connect_socket;
		      server->handle_request = sd->handle_request;
		      server->init = sd->init;
		      server->finalize = sd->finalize;
		      server->info_client = sd->info_client;
		      server->info_server = sd->info_server;
		      server->notify = sd->notify;
		      server->description = sd->name;
		      server_add (server);
		    } 
		  else 
		    {
		      erroneous = -1;
		    }
		}
	    }
	  xfree (symname);
	}
    }

  return erroneous;
}

/*
 * Debug helper funtion to traverse server_definitions.
 */
#if ENABLE_DEBUG
void
server_print_definitions (void)
{
  int s, i;
  struct server_definition *sd;

  for (s = 0; all_server_definitions[s] != NULL; s++)
    {
      sd = all_server_definitions[s];
      printf("[%d] - %s\n", s, sd->name);
      printf("  detect_proto() at %p"
	     "  connect_socket() at %p\n",
	     sd->detect_proto, sd->connect_socket);
      
      if (sd->prototype_start != NULL)
	{
	  printf ("  configblock %d byte at %p: \n",
		  sd->prototype_size, sd->prototype_start);

	  for (i = 0; sd->items[i].type != ITEM_END; i++)
	    {
	      long offset = (char *) sd->items[i].address -
		(char *) sd->prototype_start;
	      
	      printf ("   variable `%s' at offset %d, %sdefaultable: ",
		      sd->items[i].name, (int)offset,
		      (sd->items[i].defaultable?"":"not "));

	      switch (sd->items[i].type) 
		{
		case ITEM_INT:
		  printf ("int\n");
		  break;
		case ITEM_INTARRAY:
		  printf ("int array\n");
		  break;
		case ITEM_STR:
		  printf ("string\n");
		  break;
		case ITEM_STRARRAY:
		  printf ("string array\n");
		  break;
		case ITEM_HASH:
		  printf ("hash\n");
		  break;
		case ITEM_PORTCFG:
		  printf ("port configuration\n");
		  break;
		default:
		  printf ("unknown\n");
		}
	    }
	} 
      else 
	{
	  printf ("  no configuration option\n");
	}
    }
}
#endif /* ENABLE_DEBUG */

/*
 * Run all the server instances's timer routines. This is called within
 * the server_periodic_tasks() function in `server-core.c'.
 */
void
server_run_notify (void)
{
  int n;
  server_t *server;

  for (n = 0; n < server_instances; n++)
    {
      server = servers[n];
      if (server->notify)
	server->notify (server);
    }
}

/*
 * Find a server instance by a given configuration structure. Return NULL
 * if there is no such configuration.
 */
server_t *
server_find (void *cfg)
{
  int n;
  
  for (n = 0; n < server_instances; n++)
    {
      if (servers[n]->cfg == cfg)
	return servers[n];
    }
  return NULL;
}

/*
 * Add a server to the list of all servers.
 */
static void
server_add (struct server *server)
{
  servers = (struct server**) 
    xprealloc (servers, (server_instances + 1) * sizeof (struct server *));
  servers[server_instances++] = server;
}

/*
 * Run the global initializers of all servers. return -1 if some
 * server(class) doesn't feel like running.
 */
int
server_global_init (void)
{
  int erroneous = 0;
  int i;
  struct server_definition *sd;

  log_printf (LOG_NOTICE, "running global initializers\n");
  
  for (i = 0; NULL != (sd = all_server_definitions[i]); i++)
    {
      if (sd->global_init != NULL) 
	{
	  if (sd->global_init() < 0) 
	    {
	      erroneous = -1;
	      fprintf (stderr, 
		       "error running global init for `%s'\n", sd->name);
	    }
	}
    }

  return erroneous;
}

/*
 * Run the initialisers of all servers, return -1 if some server didn't
 * think it's a good idea to run...
 */
int
server_init_all (void)
{
  int errneous = 0;
  int i;

  log_printf (LOG_NOTICE, "initializing all server instances\n");
  for (i = 0; i < server_instances; i++)
    {
      if (servers[i]->init != NULL) 
	{
	  if (servers[i]->init(servers[i]) < 0) 
	    {
	      errneous = -1;
	      fprintf (stderr, 
		       "error while initializing %s\n", servers[i]->name);
	    }
	}
    }

  return errneous;
}

/*
 * Run the global finalizers per server definition.
 */
int
server_finalize_all (void)
{
  int i;

  log_printf (LOG_NOTICE, "running all finalizers\n");

  for (i = 0; i < server_instances; i++)
    {
      if (servers[i]->finalize != NULL)
	servers[i]->finalize (servers[i]);
    }

  return 0;
}

/*
 * Run the local finalizers for all server instances.
 */
int
server_global_finalize (void)
{
  int i;
  struct server_definition *sd;

  log_printf (LOG_NOTICE, "running global finalizers\n");

  for (i = 0; NULL != (sd = all_server_definitions[i]); i++)
    {
      if (sd->global_finalize != NULL)
	sd->global_finalize ();
    }

  return 0;
}

/*
 * Compare if two given portcfg structures are equal i.e. specifying 
 * the same port. Returns nonzero if a and b are equal.
 */
static int
server_portcfg_equal (struct portcfg *a, struct portcfg *b)
{
  if ((a->proto & (PROTO_TCP | PROTO_UDP | PROTO_ICMP)) &&
      (a->proto == b->proto))
    {
      /* 2 inet ports are equal if both local port and address are equal */
      if (a->port == b->port && !strcmp (a->localip, b->localip))
	return 1;
    } 
  else if (a->proto == PROTO_PIPE && a->proto == b->proto) 
    {
      /* 2 pipe configs are equal if they use the same files */
      if (!strcmp (a->inpipe, b->inpipe) && !strcmp (b->outpipe, b->outpipe))
	return 1;
    } 

  /* do not even the same proto flag -> cannot be equal */
  return 0;
}

/*
 * This functions binds a previouly instanciated server to a specified
 * port configuration.
 */
int
server_bind (server_t *server, portcfg_t *cfg)
{
  int n;

  for (n = 0; n < server_bindings; n++)
    {
      if (server_portcfg_equal (server_binding[n].cfg, cfg))
	{
	  if (server_binding[n].server == server)
	    {
	      log_printf (LOG_ERROR, "duplicate server (%s) "
			  "on single port\n", server->name);
	      return -1;
	    }
#if ENABLE_DEBUG
	  if (cfg->proto & PROTO_PIPE)
	    {
	      log_printf (LOG_DEBUG, "binding pipe server to existing "
			  "file %s\n", cfg->outpipe);
	    }
	  else if (cfg->proto & (PROTO_TCP | PROTO_UDP | PROTO_ICMP))
	    {
	      log_printf (LOG_DEBUG, "binding %s server to existing "
			  "port %s:%d\n", 
			  cfg->proto & PROTO_TCP ? "tcp" : "udp",
			  cfg->localip, cfg->port);
	    }
#endif /* ENABLE_DEBUG */
	}
    }

  n = server_bindings++;
  server_binding = xrealloc (server_binding, 
			     sizeof (server_binding_t) * server_bindings);
  server_binding[n].server = server;
  server_binding[n].cfg = cfg;

  return 0;
}

/*
 * Start all server bindings (instances of servers). Go through all port
 * configurations, skip duplicate port configurations, etc.
 */
int
server_start (void)
{
  int n, b;
  socket_t sock;

  /* Go through all port bindings. */
  for (b = 0; b < server_bindings; b++)
    {
      /* Look for duplicate port configurations. */
      for (sock = socket_root; sock; sock = sock->next)
	{
	  /* Is this socket usable for this port configuration ? */
	  if (sock->data && sock->cfg && 
	      sock->flags & SOCK_FLAG_LISTENING &&
	      server_portcfg_equal (sock->cfg, server_binding[b].cfg))
	    {
	      /* Extend the server array in sock->data. */
	      for (n = 0; SERVER (sock->data, n) != NULL; n++);
	      sock->data = xrealloc (sock->data, 
				     sizeof (void *) * (n + 2));
	      SERVER (sock->data, n) = server_binding[b].server;
	      SERVER (sock->data, n + 1) = NULL;
	      break;
	    }
	}

      /* No appropiate socket structure for this port configuration found. */
      if (!sock)
	{
	  /* Try creatng a server socket. */
	  sock = server_create (server_binding[b].cfg);
	  if (sock)
	    {
	      /* 
	       * Enqueue the server socket, put the port config into
	       * sock->cfg and initialize the server array (sock->data).
	       */
	      sock_enqueue (sock);
	      sock->cfg = server_binding[b].cfg;
	      sock->data = xmalloc (sizeof (void *) * 2);
	      SERVER (sock->data, 0) = server_binding[b].server;
	      SERVER (sock->data, 1) = NULL;
	    }
	}
    }
  
  if (server_bindings)
    {
      xfree (server_binding);
      server_binding = NULL;
      server_bindings = 0;
    }
  return 0;
}
