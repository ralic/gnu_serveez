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
 * $Id: server.c,v 1.13 2000/07/20 22:49:01 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <libsizzle/libsizzle.h>

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
#include "awcs-server/awcs-proto.h"
#include "http-server/http-proto.h"
#include "irc-server/irc-proto.h"
#include "ctrl-server/control-proto.h"

/*
 * The list of registered server. Feel free to add yours.
 */
struct server_definition * all_server_definitions [] = 
{
  &foo_server_definition,
  &awcs_server_definition,
  &http_server_definition,
  &irc_server_definition,
  &ctrl_server_definition,
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
static int set_int (char*,  char*, char*, int *, zzz_scm_t, int);
static int set_intarray (char*, char*, char*, int **, zzz_scm_t, int*);
static int set_string (char*,  char*, char*, char**, zzz_scm_t, char*);
static int set_stringarray (char*, char*, char*, char***, zzz_scm_t, char**);
static int set_hash (char*, char*, char*, hash_t ***, zzz_scm_t,hash_t**);
static int set_port (char*, char*, char*, struct portcfg **, zzz_scm_t,
		     struct portcfg *);
static void * server_instantiate (char*, zzz_scm_t, 
				  struct server_definition *, char *);
static void server_add (struct server *);

/*
 * Helpers to extract info from val, if val == NULL, use the default.
 */
static int
set_int (char *cfgfile,
	 char *varname,
	 char *keyname,
	 int *where,
	 zzz_scm_t val,
	 int def)
{
  
  if (val == zzz_undefined) 
    {
      (*where) = def;
      return 0;
    }
  
  if (!integer_p (val))
    {
      fprintf (stderr,
	       "%s: property `%s' of %s should be an integer but is not\n",
	       cfgfile, keyname, varname);
      return -1;
    }

  /* FIXME: do strings and atoi() ? */
  (*where) = integer_val (val);
  return 0;
}

static int
set_intarray (char *cfgfile,
	      char *varname,
	      char *keyname,
	      int **where,
	      zzz_scm_t val,
	      int* def)
{
  int erroneous = 0;
  int i;
  int *array = NULL;

  if (val == zzz_undefined) 
    {
      (*where) = def;
      return 0;
    }

  array = (int *) xpmalloc (sizeof (int) * (zzz_list_length (val) +1));

  for (i = 1; cons_p (val); i++, val = cdr (val))
    {
      if (!integer_p (car (val))) 
	{
	  fprintf  (stderr,
		    "%s: element %d of list `%s' is not an integer in `%s'\n",
		    cfgfile, i - 1, keyname, varname);
	  erroneous = -1;
	} 
      else 
	{
	  array[i] = integer_val (car (val));
	}
    }
  array[0] = i;

  (*where) = array;

  return erroneous;
}

static int
set_string (char *cfgfile,
	    char *varname,
	    char *keyname,
	    char **where,
	    zzz_scm_t val,
	    char *def)
{
  if (val == zzz_undefined) 
    {
      if (def == NULL)
	(*where) = NULL;
      else
	(*where) = xpstrdup (def);
      return 0;
    }

  if (!string_p (val))
    {
      fprintf (stderr,
	       "%s: property `%s' of %s should be a string but is not\n",
	       cfgfile, keyname, varname);
      return -1;
    }

  (*where) = xpstrdup (string_val (val)); 
  return 0;
}

static int
set_stringarray (char *cfgfile,
		 char *varname,
		 char *keyname,
		 char*** where,
		 zzz_scm_t val,
		 char** def)
{
  int erroneous = 0;
  int i;
  char **array = NULL;

  if (val == zzz_undefined)
    {
      (*where) = def;
      return 0;
    }

  if (!list_p (val))
    {
      fprintf (stderr,
	       "%s: property `%s' of %s should be a list but is not\n",
	       cfgfile, keyname, varname);
      return -1;
    }

  array = (char**) xpmalloc (sizeof (char *) * (zzz_list_length (val) + 1));

  for (i = 0; cons_p (val); i++, val = cdr (val))
    {
      if (!string_p (car (val))) 
	{
	  fprintf (stderr,
		   "%s: element %d of list `%s' is not a string in `%s'\n",
		   cfgfile, i, keyname, varname);
	  erroneous = -1;
	} 
      else 
	{
	  array[i] = xpstrdup (string_val (car (val)));
	}
    }
  array[i] = NULL;

  (*where) = array;
  return erroneous;
}

static int
set_hash (char *cfgfile,
	  char *varname,
	  char *keyname,
	  hash_t *** where,
	  zzz_scm_t val,
	  hash_t ** def)
{
  int erroneous = 0;
  unsigned int i;
  zzz_scm_t foo;
  hash_t *h;
  hash_t ** href = (hash_t**) xpmalloc (sizeof (hash_t**));

  if (val == zzz_undefined) 
    {
      (*where) = def;
      return 0;
    }

  /*
   * Don't forget to free in instance finalizer.
   */
  h = hash_create (17);

  if (!vector_p (val)) 
    {
      fprintf (stderr, 
	       "%s: element `%s' of `%s' should be a hash but is not\n",
	       cfgfile, keyname, varname);
      return -1;
    }

  for (i = 0; i < vector_len (val); i++)
    {
      for (foo = zzz_vector_get (val, i); cons_p (foo); foo = cdr (foo))
	{
	  if (!string_p (car (car (foo)))) 
	    {
	      fprintf (stderr, "%s: hash `%s' in `%s' is broken\n",
		       cfgfile, keyname, varname);
	      erroneous = -1;
	      continue;
	    }

	  if (!string_p (cdr (car (foo)))) 
	    {
	      fprintf (stderr, 
		       "%s: %s: value of `%s' is not a string in `%s'\n",
		       cfgfile, varname, string_val (car (car (foo))), 
		       keyname);
	      erroneous = -1;
	      continue;
	    }

	  /*
	   * The hash keeps a copy of the key itself.
	   */
	  hash_put (h, string_val( car (car (foo))),
		    xpstrdup (string_val (cdr (car (foo)))));
	}
    }

  (*href) = h;
  (*where) = href;
  
  return erroneous;
}

/*
 * Set a portcfg from a scheme variable. The default value is copied.
 */
static int set_port (char *cfgfile,
		     char *varname,
		     char *keyname,
		     struct portcfg **where,
		     zzz_scm_t val,
		     struct portcfg *def)
{
  zzz_scm_t key = NULL;
  zzz_scm_t tmp = NULL;
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
		   cfgfile, keyname, varname);
	  return -1;
	}

      (*where) = (struct portcfg *) xpmalloc (sizeof (struct portcfg));
      memcpy ((*where), def, sizeof (struct portcfg));
      newport = (*where);
    } 
  else 
    {
      newport = (struct portcfg *) xpmalloc (sizeof (struct portcfg));
    
      if (!vector_p (val)) 
	{
	  fprintf (stderr,
		   "%s: portcfg `%s' of `%s' does not specify a protocol\n",
		   cfgfile, keyname, varname);
	  return -1;
	}

      /* First, find out what kind of port is about to be recognized. */
      key = zzz_make_string ("proto", -1);
      tmp = zzz_hash_ref (val, key, zzz_undefined);

      if (!string_p (tmp)) 
	{
	  fprintf (stderr,
		   "%s: `proto' of portcfg `%s' of `%s' should be a string\n",
		   cfgfile, keyname, varname);
	  return -1;
	}

      tstr = string_val (tmp);

      if (!strcmp (tstr, "tcp") || !strcmp (tstr, "udp")) 
	{
	  /* This is tcp or udp, both share the local address. */
	  if (!strcmp (tstr, "tcp"))
	    newport->proto = PROTO_TCP;
	  else
	    newport->proto = PROTO_UDP;

	  /* Figure out the port value and set it. */
	  key = zzz_make_string ("port", -1);
	  tmp = zzz_hash_ref (val, key, zzz_undefined);
      
	  if (!integer_p (tmp)) 
	    {
	      fprintf (stderr, "%s: `%s': port is not numerical in `%s'\n",
		       cfgfile, varname, keyname);
	      return -1;
	    }
	  newport->port = (unsigned short int) integer_val (tmp);

	  /* Figure out the local ip address, "*" means any. */
	  key = zzz_make_string ("local-ip", -1);
	  tmp = zzz_hash_ref (val, key, zzz_undefined);

	  if (tmp == zzz_undefined) 
	    {
	      newport->localip = "*";
	    } 
	  else if (string_p (tmp)) 
	    {
	      newport->localip = xpstrdup (string_val (tmp));
	    }
	  else 
	    {
	      fprintf (stderr, 
		       "%s: `%s': local-ip should be a string in `%s'\n",
		       cfgfile, varname, keyname);
	      return -1;
	    }
	} 
      else if (!strcmp (tstr, "pipe")) 
	{
	  newport->proto = PROTO_PIPE;

	  key = zzz_make_string ("inpipe", -1);
	  tmp = zzz_hash_ref (val, key, zzz_undefined);

	  if (!string_p (tmp)) 
	    {
	      fprintf (stderr, 
		       "%s: `%s': inpipe should be a string in `%s'\n",
		       cfgfile, varname, keyname);
	      return -1;
	    }
	  newport->inpipe = xpstrdup (string_val (tmp));

	  key = zzz_make_string ("outpipe", -1);
	  tmp = zzz_hash_ref (val, key, zzz_undefined);

	  if (!string_p (tmp)) 
	    {
	      fprintf (stderr, 
		       "%s: `%s': outpipe should be a string in `%s'\n",
		       cfgfile, varname, keyname);
	      return -1;
	    }

	  newport->outpipe = xpstrdup (string_val (tmp));

	} 
      else 
	{
	  fprintf (stderr,
		   "%s: `proto' of portcfg `%s' of `%s' does not specify a "
		   "valid protocol (tcp, udp, pipe)\n",
		   cfgfile, keyname, varname);
	  return -1;
	}
    
      (*where) = newport;
    }

  /* Second, fill the sockaddr struct from the values we just read. */
  if (newport->proto == PROTO_TCP || newport->proto == PROTO_UDP) 
    {
      /* prepate the local address structure */
      newaddr = (struct sockaddr_in*) xpmalloc (sizeof (struct sockaddr_in));
      newport->localaddr = newaddr;
      memset (newaddr, 0, sizeof (struct sockaddr_in));

      /* ...and the local ip address with "*" being any */
      if (!strcmp (newport->localip, "*")) 
	{
	  newaddr->sin_addr.s_addr = INADDR_ANY;
	} 
      else 
	{
#ifndef __MINGW32__
	  if (inet_aton (string_val (tmp), &newaddr->sin_addr) == 0) 
	    {
	      fprintf (stderr, "%s: `%s': local-ip should be an ip address "
		       "in dotted decimal form in `%s'\n",
		       cfgfile, varname, keyname);
	      return -1;
	    }
#else /* __MINGW32__ */
	  newaddr->sin_addr.s_addr = inet_addr (string_val (tmp));
#endif /* __MINGW32__ */
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
server_instantiate (char *cfgfile,
		    zzz_scm_t hash,
		    struct server_definition *sd,
		    char *varname)
{
  char * newserver = NULL;
  int i, e = 0;
  int erroneous = 0;
  zzz_scm_t hashkey;
  zzz_scm_t hashval;
  long offset;

  /* 
   * FIXME: better checking if that is really a hash.
   */
  if (!vector_p (hash) || error_p(hash)) 
    {
      fprintf (stderr, "%s: %s is not a hash\n", cfgfile, varname);
      return NULL;
    }

  newserver = (char *) xpmalloc (sd->prototype_size);
  /*
   * FIXME: use again
   * memcpy (newserver, sd->prototype_start, sd->prototype_size);
   */

  for (i = 0; sd->items[i].type != ITEM_END; i++)
    {
      offset = (char *) sd->items[i].address - (char *) sd->prototype_start;

      hashkey = zzz_make_string (sd->items[i].name, -1);
      hashval = zzz_hash_ref (hash, hashkey, zzz_undefined);

      if (hashval == zzz_undefined && !sd->items[i].defaultable)
	{
	  fprintf (stderr,
		   "%s: `%s' does not define a default for `%s' in `%s'\n",
		   cfgfile, sd->name, sd->items[i].name, varname);
	  erroneous = -1;
	  continue;
	}

      switch (sd->items[i].type) 
	{
	case ITEM_INT:
	  e = set_int (cfgfile,
		       varname,
		       sd->items[i].name,
		       (int *)(newserver + offset),
		       hashval,
		       *(int *)sd->items[i].address);
	  break;
	  
	case ITEM_INTARRAY:
	  e = set_intarray (cfgfile,
			    varname,
			    sd->items[i].name,
			    (int **)(newserver + offset),
			    hashval,
			    *(int **)sd->items[i].address);
	  break;

	case ITEM_STR:
	  e = set_string (cfgfile,
			  varname,
			  sd->items[i].name,
			  (char **)(newserver + offset),
			  hashval,
			  *(char **)sd->items[i].address ) ;
	  break;
	  
	case ITEM_STRARRAY:
	  e = set_stringarray (cfgfile,
			       varname,
			       sd->items[i].name,
			       (char ***)(newserver + offset),
			       hashval,
			       *(char ***)sd->items[i].address);
	  break;

	case ITEM_HASH:
	  e = set_hash (cfgfile,
			varname,
			sd->items[i].name,
			(hash_t ***)(newserver + offset),
			hashval,
			*(hash_t ***)sd->items[i].address);
	  break;

	case ITEM_PORTCFG:
	  e = set_port (cfgfile,
			varname,
			sd->items[i].name,
			(struct portcfg **)(newserver + offset),
			hashval,
			*(struct portcfg **)sd->items[i].address);
	  break;

	default:
	  fprintf (stderr, "fatal: inconsistent data in " __FILE__ "\n");
	  erroneous = -1;
	}

      /* propagate error, if any */
      if (e < 0)
	erroneous = -1;
    }

  return (erroneous ? NULL : newserver);
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
  struct server_definition *sd;
  void *newserver_cfg;
  struct server *newserver;
  int erroneous = 0;

  /*
   * If the file was not found, the environment should be ok, anyway
   * Trying to use defaults...
   */

  /* Scan for variables that could be meant for us */
  for (i = 0; i < vector_len (zzz_symbol_table); i++)
    {
      for (symlist = zzz_vector_get (zzz_symbol_table, i);
	   cons_p (symlist);
	   symlist = cdr (symlist))
	{
	  sym = car (symlist);

	  for (j = 0; NULL != (sd = all_server_definitions[j]); j++)
	    {
	      symname = xpstrdup (string_val (symbol_name (sym)));
	      if (strncmp (symname, sd->varname, strlen (sd->varname)) == 0 )
		{
		  zzz_get_symbol_value (zzz_toplevel_env, sym, &symval);
		  newserver_cfg = server_instantiate (cfgfile,
						      symval,
						      sd,
						      symname);

		  if (newserver_cfg != NULL)
		    {
		      newserver =
			(struct server*) xpmalloc (sizeof (struct server));
		      newserver->cfg = newserver_cfg;
		      newserver->name = symname;
		      newserver->detect_proto = sd->detect_proto;
		      newserver->connect_socket = sd->connect_socket;
		      newserver->init = sd->init;
		      newserver->finalize = sd->finalize;
		      server_add(newserver);
		    } 
		  else 
		    {
		      /* FIXME: remove message */
		      fprintf (stderr, " no cfg for %s\n", symname);
		      erroneous = -1;
		    }
		}
	    }
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
		(char *)sd->prototype_start;
	      
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
 * Helper: compare if two given portcfg structures are equal
 * i.e. specifying the same port. Returns nonzero if a and b are equal.
 */
static int
equal_portcfg (struct portcfg *a, struct portcfg *b)
{
  if ((a->proto == PROTO_TCP || a->proto == PROTO_UDP) &&
      (a->proto == b->proto)) 
    {
      /* two inet ports are equal if both local port and address are equal */
      if (a->port == b->port && !strcmp (a->localip, b->localip))
	return 1;
      else
	return 0;
    } 
  else if (a->proto == PROTO_PIPE && a->proto == b->proto) 
    {
      /* two pipe configs are equal when they use the same filenames */
      if (!strcmp (a->inpipe, b->inpipe) &&
	  !strcmp (b->outpipe, b->outpipe))
	return 1;
      else
	return 0;
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
  server_t *server;

  /*
   * Go through all port bindings.
   */
  for (b = 0; b < server_bindings; b++)
    {
      /*
       * Look for duplicate port configurations.
       */
      for (sock = socket_root; sock; sock = sock->next)
	{
	  /*
	   * Check for duplicate server configurations.
	   */
	  server = NULL;
	  n = 0;
	  if (sock->data && sock->flags & SOCK_FLAG_LISTENING)
	    {
	      for (; (server = SERVER (sock->data, n)) != NULL; n++)
		{
		  if (server->cfg == server_binding[b].server->cfg &&
		      sock->proto == server_binding[b].server->proto)
		    {
		      fprintf (stderr, "Cannot bind duplicate server (%s) "
			       "to a single port.\n",
			       server->name);
		      break;
		    }
		}
	    }
	  /*
	   * No server configuration found so far.
	   */
	  if (server == NULL)
	    {
	      /*
	       * Is this socket usable for this port configuration ?
	       */
	      if (sock->proto & PROTO_TCP &&
		  server_binding[b].cfg->proto & PROTO_TCP &&
		  server_binding[b].cfg->port == sock->local_port)
		{
		  sock->data = xrealloc (sock->data, 
					 sizeof (void *) * (n + 2));
		  SERVER (sock->data, n) = server_binding[b].server;
		  SERVER (sock->data, n + 1) = NULL;
		  log_printf (LOG_DEBUG, "binding tcp server to existing "
			      "port %d\n", sock->local_port);
		  break;
		}

	      if (sock->proto & PROTO_PIPE &&
		  server_binding[b].cfg->proto & PROTO_PIPE &&
		  !strcmp (server_binding[b].cfg->outpipe, sock->send_pipe))
		{
		  sock->data = xrealloc (sock->data, 
					 sizeof (void *) * (n + 2));
		  SERVER (sock->data, n) = server_binding[b].server;
		  SERVER (sock->data, n + 1) = NULL;
		  log_printf (LOG_DEBUG, "binding pipe server to existing "
			      "file %s\n", sock->send_pipe);
		  break;
		}
	    }
	}
      /*
       * No apropiate socket structure for this port configuration found.
       */
      if (sock == NULL)
	{
	  /*
	   * TCP network port creation.
	   */
	  if (server_binding[b].cfg->proto & PROTO_TCP)
	    {
	      sock = server_create (server_binding[b].cfg);
	      if (sock)
		sock_enqueue (sock);
	    }
	  /*
	   * UDP network port creation.
	   */
	  else if (server_binding[b].cfg->proto & PROTO_UDP)
	    {
	      sock = server_create (server_binding[b].cfg);
	      if (sock)
		sock_enqueue (sock);
	    }
	  /*
	   * Pipe port creation.
	   */
	  else if (server_binding[b].cfg->proto & PROTO_PIPE)
	    {
	      sock = server_create (server_binding[b].cfg);
	      if (sock)
		sock_enqueue (sock);
	    }

	  if (sock)
	    {
	      sock->data = xmalloc (sizeof (void *) * 2);
	      SERVER (sock->data, 0) = server_binding[b].server;
	      SERVER (sock->data, 1) = NULL;
	    }
	}
    }
  
  xfree (server_binding);
  server_binding = NULL;
  server_bindings = 0;
  return 0;
}
