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
 * $Id: server.c,v 1.13 2001/04/28 12:37:06 ela Exp $
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
      log_printf (LOG_ERROR, "invalid server type\n");
      return;
    }

  /* Check if the server is already registered. */
  svz_array_foreach (svz_servertypes, stype, n)
    {
      if (!strcmp (server->varname, stype->varname))
	{
	  log_printf (LOG_ERROR, "server type `%s' already registered\n", 
		      server->name);
	  return;
	}
    }

  /* Run the global server type initializer. */
  if (server->global_init != NULL) 
    if (server->global_init () < 0) 
      {
	log_printf (LOG_ERROR, "error running global init for `%s'\n",
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
 * known server types and run its global finalizer if necessary.
 */
void
svz_servertype_del (unsigned long index)
{
  svz_servertype_t *server;

  /* Return here if there is no such server type. */
  if (svz_servertypes == NULL || index >= svz_array_size (svz_servertypes))
    return;

  /*
   * Run the server type's global finalizer if necessary and delete it 
   * from the list of known servers then.
   */
  if ((server = svz_array_get (svz_servertypes, index)) != NULL)
    {
      if (server->global_finalize != NULL)
	server->global_finalize ();
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

  log_printf (LOG_NOTICE, "running global server type finalizers\n");
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
      svz_server_free (server);
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
  svz_servertype_t *stype = server->type;
  int n;
  void **target;

  /* Go through the list of configuration items. */
  for (n = 0; stype->items[n].type != ITEM_END; n++)
    {
      /* Calculate the target address. */
      target = (void **) ((char *) server->cfg + 
			  (unsigned long) ((char *) stype->items[n].address - 
					   (char *) stype->prototype_start));

      /* Depending on the type of configuration item we need to free
	 different data structures. */
      switch (stype->items[n].type) 
        {
          /* Integer array. */
        case ITEM_INTARRAY:
	  if (*target)
	    svz_array_destroy (*target);
          break;

	  /* Simple string. */
        case ITEM_STR:
	  if (*target)
	    svz_free (*target);
          break;
          
	  /* Array of strings. */
        case ITEM_STRARRAY:
	  if (*target)
	    svz_array_destroy (*target);
          break;

	  /* Hash table. */
        case ITEM_HASH:
	  if (*target)
	    svz_hash_destroy (*target);
          break;

	  /* Port configuration. */
        case ITEM_PORTCFG:
	  if (*target)
	    {
	      svz_portcfg_destroy (*target);
	      svz_free (*target);
	    }
          break;
        }
    }
  svz_free (server->cfg);
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
  unsigned long offset, size;

  /* Make a simple copy of the example configuration structure definition 
     for that server instance. */
  cfg = svz_malloc (server->prototype_size);
  /*FIXME:  memcpy (cfg, server->prototype_start, server->prototype_size); */
  memset (cfg, 0, server->prototype_size);

  /* Go through list of configuration items. */
  for (n = 0; server->items[n].type != ITEM_END; n++)
    {
      /* Calculate the target address. */
      offset = (char *) server->items[n].address - 
	(char *) server->prototype_start;
      def = server->items[n].address;
      target = (char *) cfg + offset;
      size = e = 0;

      /* Depending on the type of configuration item we need at this
	 point we call the given callbacks and check their return values. */
      switch (server->items[n].type) 
        {
	  /* Integer value. */
        case ITEM_INT:
	  size = sizeof (int);
	  if (configure && configure->integer)
	    e = configure->integer (name, arg, server->items[n].name,
				    (int *) target, *(int *) def);
          break;

          /* Integer array. */
        case ITEM_INTARRAY:
	  size = sizeof (svz_array_t *);
	  if (configure && configure->intarray)
	    e = configure->intarray (name, arg, server->items[n].name,
				     (svz_array_t **) target,
				     (svz_array_t *) def);
          break;

	  /* Simple string. */
        case ITEM_STR:
	  size = sizeof (char *);
	  if (configure && configure->string)
	    e = configure->string (name, arg, server->items[n].name,
				   (char **) target, (char *) def);
          break;
          
	  /* Array of strings. */
        case ITEM_STRARRAY:
	  size = sizeof (svz_array_t *);
	  if (configure && configure->strarray)
	    e = configure->strarray (name, arg, server->items[n].name,
				     (svz_array_t **) target, 
				     (svz_array_t *) def);
          break;

	  /* Hash table. */
        case ITEM_HASH:
	  size = sizeof (svz_hash_t *);
	  if (configure && configure->hash)
	    e = configure->hash (name, arg, server->items[n].name,
				 (svz_hash_t ***) target,
				 (svz_hash_t **) def);
          break;

	  /* Port configuration. */
        case ITEM_PORTCFG:
	  size = sizeof (svz_portcfg_t *);
	  if (configure && configure->portcfg)
	    e = configure->portcfg (name, arg, server->items[n].name,
				    (svz_portcfg_t **) target,
				    (svz_portcfg_t *) def);
          break;

	  /* Unknown configuration item. */
        default:
          log_printf (LOG_FATAL, "inconsistent data in " __FILE__ "\n");
          e = -1;
        }

      /* Check the return value of the configure functions. If the value at
	 `target' has been modified it should return something non-zero. */
      if (e == 0)
	{
	  if (!server->items[n].defaultable)
	    {
	      log_printf (LOG_ERROR,
			  "`%s' does not define a default for `%s' in `%s'\n",
			  server->name, server->items[n].name, name);
	      error = -1;
	    }
	  /* Assuming default value. */
	  else
	    {
	      switch (server->items[n].type) 
		{
		case ITEM_INT:
		  memcpy (target, def, size);
		  break;
		case ITEM_INTARRAY:
		  break;
		case ITEM_STR:
		  *((char **) &target) = (char *) svz_strdup (def);
		  break;
		case ITEM_STRARRAY:
		  break;
		case ITEM_HASH:
		  break;
		case ITEM_PORTCFG:
		  break;
		}
	    }
	  continue;
        }
      else if (e < 0)
	error = -1;
    }

  /* Release memory reserved for configuration on errors. */
  if (error)
    svz_free_and_zero (cfg);

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

  log_printf (LOG_NOTICE, "initializing all server instances\n");
  svz_hash_foreach_value (svz_servers, server, i)
    {
      if (server[i]->init != NULL) 
	if (server[i]->init (server[i]) < 0) 
	  {
	    errneous = -1;
	    log_printf (LOG_ERROR, "error initializing `%s'\n", 
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

  log_printf (LOG_NOTICE, "running all server finalizers\n");
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
