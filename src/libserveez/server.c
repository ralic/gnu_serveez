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
 * $Id: server.c,v 1.8 2001/04/06 15:32:35 raimi Exp $
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

/*
 * Debug helper function to traverse all currently known server types.
 */
#if ENABLE_DEBUG
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
 * Remove the server instance identified by the name @var{name}.
 */
void
svz_server_del (char *name)
{
  if (svz_servers == NULL)
    return;
  svz_hash_delete (svz_servers, name);
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
  int i;
  svz_server_t **server;

  log_printf (LOG_NOTICE, "running all server finalizers\n");
  svz_hash_foreach_value (svz_servers, server, i)
    {
      if (server[i]->finalize != NULL)
	server[i]->finalize (server[i]);
    }

  return 0;
}

/*
 * Compare if two given portcfg structures are equal i.e. specifying 
 * the same port. Returns non-zero if A and B are equal.
 */
int
server_portcfg_equal (portcfg_t *a, portcfg_t *b)
{
  if ((a->proto & (PROTO_TCP | PROTO_UDP | PROTO_ICMP)) &&
      (a->proto == b->proto))
    {
      /* 2 inet ports are equal if both local port and address are equal */
      if (a->port == b->port && !strcmp (a->ipaddr, b->ipaddr))
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
