/*
 * server.c - server object functions
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
 * $Id: server.c,v 1.18 2001/05/20 20:30:43 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

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
#include "libserveez/util.h"
#include "libserveez/core.h"
#include "libserveez/array.h"
#include "libserveez/socket.h"
#include "libserveez/server-core.h"
#include "libserveez/server.h"
#include "libserveez/binding.h"

/*
 * The list of registered server. Feel free to add yours.
 */
svz_array_t *svz_servertypes = NULL;

/*
 * Add the server type @var{server} to the currently registered servers.
 */
void
svz_servertype_add (svz_servertype_t *server)
{
  int n;
  svz_servertype_t *stype;

  /* Check if the server definition is valid. */
  if (!server || !server->varname || !server->name)
    {
      svz_log (LOG_ERROR, "invalid server type\n");
      return;
    }

  /* Check if the server is already registered. */
  svz_array_foreach (svz_servertypes, stype, n)
    {
      if (!strcmp (server->varname, stype->varname))
	{
	  svz_log (LOG_ERROR, "server type `%s' already registered\n", 
		   server->name);
	  return;
	}
    }

  /* Run the global server type initializer. */
  if (server->global_init != NULL) 
    if (server->global_init () < 0) 
      {
	svz_log (LOG_ERROR, "error running global init for `%s'\n",
		 server->name);
	return;
      }

  /* Add this definition to the registered servers. */
  if (svz_servertypes == NULL)
    if ((svz_servertypes = svz_array_create (1)) == NULL)
      return;
  svz_array_add (svz_servertypes, server);
}

/*
 * Delete the server type with the index @var{index} from the list of
 * known server types and run its global finalizer if necessary. Moreover
 * we remove and finalize each server instance of this server type.
 */
void
svz_servertype_del (unsigned long index)
{
  svz_servertype_t *stype;
  svz_server_t **server;
  int n, i;

  /* Return here if there is no such server type. */
  if (svz_servertypes == NULL || index >= svz_array_size (svz_servertypes))
    return;

  /* Run the server type's global finalizer if necessary and delete it 
     from the list of known servers then. */
  if ((stype = svz_array_get (svz_servertypes, index)) != NULL)
    {
      /* Find server instance of this server type and remove and finalize
	 them if necessary. */
      n = svz_hash_size (svz_servers) - 1;
      svz_hash_foreach_value (svz_servers, server, i)
	{
	  if (server[n]->type == stype)
	    {
	      svz_server_del (server[n]->name);
	      i--;
	    }
	  n--;
	}

      if (stype->global_finalize != NULL)
	stype->global_finalize ();
      svz_array_del (svz_servertypes, index);
    }
}

/*
 * Run the global finalizers of each server type and delete all server
 * types.
 */
void
svz_servertype_finalize (void)
{
  int i;
  svz_servertype_t *stype;

  svz_log (LOG_NOTICE, "running global server type finalizers\n");
  svz_array_foreach (svz_servertypes, stype, i)
    {
      if (stype->global_finalize != NULL)
	stype->global_finalize ();
    }
  if (svz_servertypes != NULL)
    {
      svz_array_destroy (svz_servertypes);
      svz_servertypes = NULL;
    }
}

/*
 * Find a given server instance's @var{server} server type. Return @code{NULL}
 * if there is no such server type (which should never occur since a server is
 * a child of an server type.
 */
svz_servertype_t *
svz_servertype_find (svz_server_t *server)
{
  return server->type;
}

#if ENABLE_DEBUG
/*
 * Debug helper function to traverse all currently known server types.
 */
void
svz_servertype_print (void)
{
  int s, i;
  svz_servertype_t *stype;

  svz_array_foreach (svz_servertypes, stype, s)
    {
      printf ("[%d] - %s\n", s, stype->name);
      printf ("  detect_proto() at %p"
	      "  connect_socket() at %p\n",
	      stype->detect_proto, stype->connect_socket);
      
      if (stype->prototype_start != NULL)
	{
	  printf ("  configblock %d byte at %p: \n",
		  stype->prototype_size, stype->prototype_start);

	  for (i = 0; stype->items[i].type != ITEM_END; i++)
	    {
	      long offset = (char *) stype->items[i].address -
		(char *) stype->prototype_start;
	      
	      printf ("   variable `%s' at offset %d, %sdefaultable: ",
		      stype->items[i].name, (int) offset,
		      stype->items[i].defaultable ? "" : "not ");

	      switch (stype->items[i].type) 
		{
		case ITEM_BOOL:
		  printf ("bool\n");
		  break;
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
 * This is the list of actually instantiated servers. The hash table 
 * associates the servers' names with the server instances.
 */
svz_hash_t *svz_servers = NULL;

/*
 * Run all the server instances's notify routines. This should be regularily
 * called within the @code{svz_server_periodic_tasks()} function.
 */
void
svz_server_notifiers (void)
{
  int n;
  svz_server_t **server;

  svz_hash_foreach_value (svz_servers, server, n)
    {
      if (server[n]->notify)
	server[n]->notify (server[n]);
    }
}

/*
 * Find a server instance by the given configuration structure @var{cfg}. 
 * Return @code{NULL} if there is no such configuration in any server
 * instance.
 */
svz_server_t *
svz_server_find (void *cfg)
{
  int n;
  svz_server_t **server;

  svz_hash_foreach_value (svz_servers, server, n)
    {
      if (server[n]->cfg == cfg)
	return server[n];
    }
  return NULL;
}

/*
 * Add the server instance @var{server} to the list of instanciated 
 * servers.
 */
void
svz_server_add (svz_server_t *server)
{
  if (svz_servers == NULL)
    svz_servers = svz_hash_create (4);
  svz_hash_put (svz_servers, server->name, server);
}

/*
 * Get the server instance with the given instance name @var{name}.
 * Return @code{NULL} if there is no such server yet.
 */
svz_server_t *
svz_server_get (char *name)
{
  if (svz_servers == NULL || name == NULL)
    return NULL;
  return svz_hash_get (svz_servers, name);
}

/*
 * Remove the server instance identified by the name @var{name}.
 */
void
svz_server_del (char *name)
{
  svz_server_t *server;

  if (svz_servers == NULL)
    return;
  if ((server = svz_hash_delete (svz_servers, name)) != NULL)
    {
      svz_server_unbind (server);
      svz_server_free (server);
    }
}

/*
 * Release the configuration @var{cfg} of the given server type @var{server}.
 * If the servers configuration equals @code{NULL} no operation is 
 * performed.
 */
void
svz_config_free (svz_servertype_t *server, void *cfg)
{
  int n;
  void **target;

  /* Return here if there nothing to do. */
  if (server == NULL || cfg == NULL)
    return;

  /* Go through the list of configuration items. */
  for (n = 0; server->items[n].type != ITEM_END; n++)
    {
      /* Calculate the target address. */
      target = (void **) ((char *) cfg + 
			  (unsigned long) ((char *) server->items[n].address - 
					   (char *) server->prototype_start));

      /* Depending on the type of configuration item we need to free
	 different data structures. */
      switch (server->items[n].type) 
        {
          /* Integer array. */
        case ITEM_INTARRAY:
	  if (*target)
	    svz_config_intarray_destroy (*target);
          break;

	  /* Simple character string. */
        case ITEM_STR:
	  if (*target)
	    svz_free (*target);
          break;
          
	  /* Array of strings. */
        case ITEM_STRARRAY:
	  if (*target)
	    svz_config_strarray_destroy (*target);
          break;

	  /* Hash table. */
        case ITEM_HASH:
	  if (*target)
	    svz_config_hash_destroy (*target);
          break;

	  /* Port configuration. */
        case ITEM_PORTCFG:
	  if (*target)
	    svz_portcfg_destroy (*target);
          break;
        }
    }
  svz_free (cfg);
}

/*
 * Clear each configuration item within the given configuration @var{cfg} of
 * the server type @var{server}. This function is used by 
 * @code{svz_server_configure()} after copying the default configuration.
 */
static void
svz_config_clobber (svz_servertype_t *server, void *cfg)
{
  int n;
  void **target;

  /* Return here if there nothing to do. */
  if (server == NULL || cfg == NULL)
    return;

  /* Go through the list of configuration items. */
  for (n = 0; server->items[n].type != ITEM_END; n++)
    {
      /* Calculate the target address. */
      target = (void **) ((char *) cfg + 
			  (unsigned long) ((char *) server->items[n].address - 
					   (char *) server->prototype_start));

      /* Clobber only configuration items which are pointers. */
      if (server->items[n].type == ITEM_INTARRAY ||
	  server->items[n].type == ITEM_STR ||
	  server->items[n].type == ITEM_STRARRAY ||
	  server->items[n].type == ITEM_HASH ||
	  server->items[n].type == ITEM_PORTCFG)
        {
	  *target = NULL;
	}
    }
}

/*
 * Completely destroy the given server instance @var{server}. This 
 * especially means to go through each item of the server instance's 
 * configuration.
 */
void
svz_server_free (svz_server_t *server)
{
  svz_config_free (server->type, server->cfg);
  svz_free (server->name);
  svz_free (server);
}

/*
 * Create a new server instance of the server type @var{stype} with the
 * instance name @var{name}.
 */
svz_server_t *
svz_server_instantiate (svz_servertype_t *stype, char *name)
{
  svz_server_t *server;
  
  /* Create server instance itself. */
  server = (svz_server_t *) svz_malloc (sizeof (svz_server_t));
  server->name = svz_strdup (name);
  server->type = stype;

  /* Transfer callbacks. */
  server->detect_proto = stype->detect_proto;
  server->connect_socket = stype->connect_socket;
  server->handle_request = stype->handle_request;
  server->init = stype->init;
  server->finalize = stype->finalize;
  server->info_client = stype->info_client;
  server->info_server = stype->info_server;
  server->notify = stype->notify;
  server->description = stype->name;

  return server;
}

/*
 * Create an array (@code{svz_array_t}) of integers. The given integer
 * array @var{intarray} is a list of integers where its first element which
 * is @code{intarray[0]} contains the actual length of the given array.
 */
svz_array_t *
svz_config_intarray_create (int *intarray)
{
  int i;
  svz_array_t *array = svz_array_create (1);

  if (intarray)
    {
      for (i = 0; i < intarray[0]; i++)
	svz_array_add (array, (void *) intarray[i + 1]);
    }
  return array;
}

/*
 * Destroy the given integer array @var{intarray}. This function is the
 * counter part of @code{svz_config_intarray_create()}.
 */
void
svz_config_intarray_destroy (svz_array_t *intarray)
{
  if (intarray)
    {
      svz_array_destroy (intarray);
    }
}

/*
 * Make a plain copy of the given integer array @var{intarray}. If this
 * value is @code{NULL} no operation is performed and the return value
 * is @code{NULL} too.
 */
svz_array_t *
svz_config_intarray_dup (svz_array_t *intarray)
{
  int i;
  void *value;
  svz_array_t *array = NULL;
  
  if (intarray)
    {
      array = svz_array_create (svz_array_size (intarray));
      svz_array_foreach (intarray, value, i)
	svz_array_add (array, value);
    }
  return array;
}

/*
 * Create an array of strings. The given list of strings @var{strarray}
 * must be @code{NULL} terminated in order to indicate its end.
 */
svz_array_t *
svz_config_strarray_create (char **strarray)
{
  int i;
  svz_array_t *array = svz_array_create (1);

  if (strarray)
    {
      for (i = 0; strarray[i] != NULL; i++)
	svz_array_add (array, svz_strdup (strarray[i]));
    }
  return array;
}

/*
 * Destroy the given string array @var{strarray}.
 */
void
svz_config_strarray_destroy (svz_array_t *strarray)
{
  int i;
  char *string;

  if (strarray)
    {
      svz_array_foreach (strarray, string, i)
	svz_free (string);
      svz_array_destroy (strarray);
    }
}

/*
 * Duplicate the given array of strings @var{strarray}. Return @code{NULL}
 * if @var{strarray} equals @code{NULL}.
 */
svz_array_t *
svz_config_strarray_dup (svz_array_t *strarray)
{
  int i;
  char *value;
  svz_array_t *array = NULL;
  
  if (strarray)
    {
      array = svz_array_create (svz_array_size (strarray));
      svz_array_foreach (strarray, value, i)
	svz_array_add (array, svz_strdup (value));
    }
  return array;
}

/*
 * Create a hash table from the given array of strings @var{strarray} which
 * must be @code{NULL} terminated in order to indicate the end of the list. 
 * The array consists of pairs of strings where the first one specifies a 
 * key and the following the associated string value. This function is 
 * useful when creating default values for server type configurations.
 */
svz_hash_t *
svz_config_hash_create (char **strarray)
{
  int i;
  svz_hash_t *hash = svz_hash_create (4);

  if (strarray)
    {
      for (i = 0; strarray[i] != NULL; i += 2)
	{
	  if (strarray[i + 1])
	    {
	      svz_hash_put (hash, strarray[i], svz_strdup (strarray[i + 1]));
	    }
	}
    }
  return hash;
}

/*
 * This function is the counter part of @code{svz_config_hash_create()}. It
 * destroys the given hash table @var{strhash} assuming it is a hash 
 * associating strings with strings.
 */
void
svz_config_hash_destroy (svz_hash_t *strhash)
{
  char **strarray;
  int i;

  if (strhash)
    {
      svz_hash_foreach_value (strhash, strarray, i)
	svz_free (strarray[i]);
      svz_hash_destroy (strhash);
    }
}

/*
 * Duplicate the given hash table @var{strhash} assuming it is a hash 
 * associating strings with strings. Return @code{NULL} if @var{strhash} is
 * @code{NULL} too.
 */
svz_hash_t *
svz_config_hash_dup (svz_hash_t *strhash)
{
  svz_hash_t *hash = NULL;
  int i;
  char **keys;

  if (strhash)
    {
      hash = svz_hash_create (4);
      svz_hash_foreach_key (strhash, keys, i)
	{
	  svz_hash_put (hash, keys[i], 
			svz_strdup (svz_hash_get (strhash, keys[i])));
	}
    }
  return hash;
}

/*
 * This function configures a server instance by modifying its default
 * configuration by the @var{configure} callbacks. Therefore you need to pass
 * the type of server in @var{server}, the @var{name} of the server instance
 * and the (optional) modifier callback structure @var{configure}. The
 * @var{arg} argument is passed to each of the callbacks (e.g. specifying
 * a scheme cell). The function returns either a valid server instance 
 * configuration or @code{NULL} on errors.
 */
void *
svz_server_configure (svz_servertype_t *server, 
		      char *name, 
		      void *arg,
		      svz_server_config_t *configure)
{
  int e, n, error = 0;
  void *cfg, *def, *target = NULL;
  unsigned long offset;

  /* Make a simple copy of the example configuration structure definition 
     for that server instance. */
  cfg = svz_malloc (server->prototype_size);
  memcpy (cfg, server->prototype_start, server->prototype_size);

  /* Clear all server configuration items which are pointers. Thus we
     are able to reverse the changes below. */
  svz_config_clobber (server, cfg);

  /* Go through list of configuration items. */
  for (n = 0; server->items[n].type != ITEM_END; n++)
    {
      /* Calculate the target address. */
      offset = (char *) server->items[n].address - 
	(char *) server->prototype_start;
      def = server->items[n].address;
      target = (char *) cfg + offset;
      e = 0;

      /* Depending on the type of configuration item we need at this
	 point we call the given callbacks and check their return values. */
      switch (server->items[n].type) 
        {
	  /* Integer value. */
        case ITEM_INT:
	  if (configure && configure->integer)
	    e = configure->integer (name, arg, server->items[n].name,
				    (int *) target, *(int *) def);
          break;

	  /* Boolean value. */
        case ITEM_BOOL:
	  if (configure && configure->boolean)
	    e = configure->boolean (name, arg, server->items[n].name,
				    (int *) target, *(int *) def);
          break;

          /* Integer array. */
        case ITEM_INTARRAY:
	  if (configure && configure->intarray)
	    e = configure->intarray (name, arg, server->items[n].name,
				     (svz_array_t **) target,
				     *(svz_array_t **) def);
          break;

	  /* Simple string. */
        case ITEM_STR:
	  if (configure && configure->string)
	    e = configure->string (name, arg, server->items[n].name,
				   (char **) target, *(char **) def);
          break;
          
	  /* Array of strings. */
        case ITEM_STRARRAY:
	  if (configure && configure->strarray)
	    e = configure->strarray (name, arg, server->items[n].name,
				     (svz_array_t **) target, 
				     *(svz_array_t **) def);
          break;

	  /* Hash table. */
        case ITEM_HASH:
	  if (configure && configure->hash)
	    e = configure->hash (name, arg, server->items[n].name,
				 (svz_hash_t **) target,
				 *(svz_hash_t **) def);
          break;

	  /* Port configuration. */
        case ITEM_PORTCFG:
	  if (configure && configure->portcfg)
	    e = configure->portcfg (name, arg, server->items[n].name,
				    (svz_portcfg_t **) target,
				    *(svz_portcfg_t **) def);
          break;

	  /* Unknown configuration item. */
        default:
          svz_log (LOG_FATAL, 
		   "inconsistent ITEM_ data in server type `%s'\n",
		   server->name);
          e = -1;
        }

      /* Check the return value of the configure functions. If the value at
	 `target' has been modified it should return something non-zero. */
      if (e == 0)
	{
	  /* Target not configured. Defaultable ? */
	  if (!server->items[n].defaultable)
	    {
	      svz_log (LOG_ERROR,
		       "`%s' lacks a default %s for `%s' in `%s'\n",
		       server->name, ITEM_TEXT (server->items[n].type),
		       server->items[n].name, name);
	      error = -1;
	    }
	  /* Assuming default value. */
	  else
	    {
	      switch (server->items[n].type) 
		{
		  /* Normal integer. */
		case ITEM_INT:
		  *(int *) target = *(int *) def;
		  break;

		  /* Boolean value. */
		case ITEM_BOOL:
		  *(int *) target = *(int *) def;
		  break;

		  /* Integer array. */
		case ITEM_INTARRAY:
		  *(svz_array_t **) target = 
		    svz_config_intarray_dup (*(svz_array_t **) def);
		  break;

		  /* Character string. */
		case ITEM_STR:
		  *(char **) target = (char *) svz_strdup (*(char **) def);
		  break;

		  /* Array of strings. */
		case ITEM_STRARRAY:
		  *(svz_array_t **) target = 
		    svz_config_strarray_dup (*(svz_array_t **) def);
		  break;

		  /* Hash table. */
		case ITEM_HASH:
		  *(svz_hash_t **) target = 
		    svz_config_hash_dup (*(svz_hash_t **) def);
		  break;

		  /* Port configuration. */
		case ITEM_PORTCFG:
		  *(svz_portcfg_t **) target =
		    svz_portcfg_dup (*(svz_portcfg_t **) def);
		  break;
		}
	    }
	  continue;
        }
      /* Negative return values indicate an error. */
      else if (e < 0)
	{
	  error = -1;
	}
    }

  /* Release memory reserved for configuration on errors. This means
     to reverse the above changes. */
  if (error)
    {
      svz_config_free (server, cfg);
      cfg = NULL;
    }
  return cfg;
}

/*
 * Run the initializers of all servers, return -1 if some server did not
 * think it is a good idea to run.
 */
int
svz_server_init_all (void)
{
  int errneous = 0, i;
  svz_server_t **server;

  svz_log (LOG_NOTICE, "initializing all server instances\n");
  svz_hash_foreach_value (svz_servers, server, i)
    {
      if (server[i]->init != NULL) 
	if (server[i]->init (server[i]) < 0) 
	  {
	    errneous = -1;
	    svz_log (LOG_ERROR, "error initializing `%s'\n", 
		     server[i]->name);
	  }
    }
  return errneous;
}

/*
 * Run the local finalizers for all server instances.
 */
int
svz_server_finalize_all (void)
{
  int i, n;
  svz_server_t **server;

  svz_log (LOG_NOTICE, "running all server finalizers\n");
  n = svz_hash_size (svz_servers) - 1;
  svz_hash_foreach_value (svz_servers, server, i)
    {
      if (server[n]->finalize != NULL)
	server[n]->finalize (server[n]);
      svz_server_del (server[n]->name);
      i--;
      n--;
    }
  svz_hash_destroy (svz_servers);
  svz_servers = NULL;

  return 0;
}
