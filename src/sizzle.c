/*
 * sizzle.c - interface to sizzle core library
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
 * $Id: sizzle.c,v 1.2 2001/01/28 13:11:54 ela Exp $
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
# include <winsock2.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#include "libserveez/alloc.h"
#include "libserveez/hash.h"
#include "libserveez/socket.h"
#include "libserveez/server.h"

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
	      zzz_scm_t val, int *def)
{
  int erroneous = 0;
  int i;
  int *array = NULL;

  if (val == zzz_undefined) 
    {
      (*location) = def;
      return 0;
    }

  array = (int *) svz_pmalloc (sizeof (int) * (zzz_list_length (val) + 1));

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
	(*location) = svz_pstrdup (def);
      return 0;
    }

  if (!string_p (val))
    {
      fprintf (stderr,
	       "%s: property `%s' of `%s' should be a string but is not\n",
	       cfgfile, key, var);
      return -1;
    }

  (*location) = svz_pstrdup (string_val (val)); 
  return 0;
}

static int
set_stringarray (char *cfgfile, char *var, char *key, char ***location,
		 zzz_scm_t val, char **def)
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

  array = (char **) svz_pmalloc (sizeof (char *) * (zzz_list_length (val) + 1));

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
	  array[i] = svz_pstrdup (string_val (car (val)));
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
  hash_t **href = svz_pmalloc (sizeof (hash_t **));

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
		    svz_strdup (string_val (cdr (car (foo)))));
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
#define PORTCFG_RAW     "raw"
#define PORTCFG_PIPE    "pipe"
#define PORTCFG_IP      "ipaddr"
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

      (*location) = (struct portcfg *) svz_pmalloc (sizeof (struct portcfg));
      memcpy ((*location), def, sizeof (struct portcfg));
      newport = (*location);
    } 
  else 
    {
      newport = (struct portcfg *) svz_pmalloc (sizeof (struct portcfg));
    
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

	  if (newport->proto == PROTO_ICMP)
	    {
	      newport->port = 0;
	    }
	  else
	    {
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
	    }

	  /* Figure out the local ip address, "*" means any. */
	  hash_key = zzz_make_string (PORTCFG_IP, -1);
	  hash_val = zzz_hash_ref (val, hash_key, zzz_undefined);

	  if (hash_val == zzz_undefined) 
	    {
	      newport->ipaddr = PORTCFG_NOIP;
	    } 
	  else if (string_p (hash_val)) 
	    {
	      newport->ipaddr = svz_pstrdup (string_val (hash_val));
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
	  newport->inpipe = svz_pstrdup (string_val (hash_val));

	  hash_key = zzz_make_string (PORTCFG_OUTPIPE, -1);
	  hash_val = zzz_hash_ref (val, hash_key, zzz_undefined);

	  if (!string_p (hash_val)) 
	    {
	      fprintf (stderr, 
		       "%s: `%s': %s should be a string in `%s'\n",
		       cfgfile, var, PORTCFG_OUTPIPE, key);
	      return -1;
	    }

	  newport->outpipe = svz_pstrdup (string_val (hash_val));

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
      /* prepare the local address structure */
      newaddr = (struct sockaddr_in *) 
	svz_pmalloc (sizeof (struct sockaddr_in));
      newport->addr = newaddr;
      memset (newaddr, 0, sizeof (struct sockaddr_in));

      /* ...and the local ip address with "*" being any */
      if (!strcmp (newport->ipaddr, PORTCFG_NOIP)) 
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
#elif defined (__MINGW32__)
	  int len = sizeof (struct sockaddr_in);
	  if (WSAStringToAddress (string_val (hash_val), AF_INET, NULL, 
				  (struct sockaddr *) newaddr, &len) != 0)
	    {
	      fprintf (stderr, "%s: `%s': %s should be an ip address "
		       "in dotted decimal form in `%s'\n",
		       cfgfile, var, PORTCFG_IP, key);
              return -1;
            }
#else /* not HAVE_INET_ATON and not __MINGW32__ */
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
zzz_server_instantiate (char *cfgfile, zzz_scm_t hash,
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
  cfg = svz_pmalloc (sd->prototype_size);
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
			       *(char ***) sd->items[i].address);
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
zzz_server_load_cfg (char *cfgfile)
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
	  symname = svz_strdup (string_val (symbol_name (sym)));

	  for (j = 0; j < (unsigned) server_definitions; j++)
	    {
	      sd = server_definition[j];

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

		  cfg = zzz_server_instantiate (cfgfile, symval, sd, symname);

		  if (cfg != NULL)
		    {
		      server = (server_t *) svz_pmalloc (sizeof (server_t));
		      server->cfg = cfg;
		      server->name = svz_pstrdup (symname);
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
	  svz_free (symname);
	}
    }

  return erroneous;
}
