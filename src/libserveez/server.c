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
 * $Id: server.c,v 1.5 2001/03/04 13:13:41 ela Exp $
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
#include "libserveez/socket.h"
#include "libserveez/server-core.h"
#include "libserveez/server.h"
#include "libserveez/server-socket.h"

/*
 * The list of registered server. Feel free to add yours.
 */
int server_definitions = 0;
server_definition_t **server_definition = NULL;

/*
 * Add another server definition to the currently registered servers.
 */
void
server_add_definition (server_definition_t *definition)
{
  int n;

  /* Check if the server definition is valid. */
  if (!definition || !definition->varname || !definition->name)
    {
      log_printf (LOG_ERROR, "invalid server definition\n");
      return;
    }

  /* Check if the server is already registered. */
  for (n = 0; n < server_definitions; n++)
    if (!strcmp (definition->varname, server_definition[n]->varname))
      {
	log_printf (LOG_ERROR, "server `%s' already registered\n", 
		    definition->name);
	return;
      }

  /* Add this definition to the registered servers. */
  server_definition = (server_definition_t **) 
    svz_prealloc (server_definition, (server_definitions + 1) * 
		  sizeof (server_definition_t *));
  server_definition[server_definitions++] = definition;
}

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
 * Debug helper function to traverse all server definitions.
 */
#if ENABLE_DEBUG
void
server_print_definitions (void)
{
  int s, i;
  struct server_definition *sd;

  for (s = 0; s < server_definitions; s++)
    {
      sd = server_definition[s];
      printf ("[%d] - %s\n", s, sd->name);
      printf ("  detect_proto() at %p"
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
		      sd->items[i].name, (int) offset,
		      sd->items[i].defaultable ? "" : "not ");

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
 * Run all the server instances's timer routines. This should be called 
 * within the `server_periodic_tasks ()' function.
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
void
server_add (struct server *server)
{
  servers = (struct server **) 
    svz_prealloc (servers, (server_instances + 1) * sizeof (struct server *));
  servers[server_instances++] = server;
}

/*
 * Run the global initializers of all servers. Return -1 if some
 * server (class) does not feel like running.
 */
int
server_global_init (void)
{
  int erroneous = 0;
  int i;
  struct server_definition *sd;

  log_printf (LOG_NOTICE, "running global initializers\n");
  
  for (i = 0; i < server_definitions; i++)
    {
      sd = server_definition[i];
      if (sd->global_init != NULL) 
	{
	  if (sd->global_init () < 0) 
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
 * Run the initializers of all servers, return -1 if some server did not
 * think it is a good idea to run...
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
	  if (servers[i]->init (servers[i]) < 0) 
	    {
	      errneous = -1;
	      fprintf (stderr, "error initializing `%s'\n", servers[i]->name);
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

  for (i = 0; i < server_definitions; i++)
    {
      sd = server_definition[i];
      if (sd->global_finalize != NULL)
	sd->global_finalize ();
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

/*
 * This functions binds a previously instantiated server to a specified
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
			  cfg->ipaddr, cfg->port);
	    }
#endif /* ENABLE_DEBUG */
	}
    }

  n = server_bindings++;
  server_binding = svz_realloc (server_binding, 
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
      for (sock = sock_root; sock; sock = sock->next)
	{
	  /* Is this socket usable for this port configuration ? */
	  if (sock->data && sock->cfg && 
	      sock->flags & SOCK_FLAG_LISTENING &&
	      server_portcfg_equal (sock->cfg, server_binding[b].cfg))
	    {
	      /* Extend the server array in sock->data. */
	      for (n = 0; SERVER (sock->data, n) != NULL; n++);
	      sock->data = svz_realloc (sock->data, 
					sizeof (void *) * (n + 2));
	      SERVER (sock->data, n) = server_binding[b].server;
	      SERVER (sock->data, n + 1) = NULL;
	      break;
	    }
	}

      /* No appropriate socket structure for this port configuration found. */
      if (!sock)
	{
	  /* Try creating a server socket. */
	  sock = server_create (server_binding[b].cfg);
	  if (sock)
	    {
	      /* 
	       * Enqueue the server socket, put the port config into
	       * sock->cfg and initialize the server array (sock->data).
	       */
	      sock_enqueue (sock);
	      sock->cfg = server_binding[b].cfg;
	      sock->data = svz_malloc (sizeof (void *) * 2);
	      SERVER (sock->data, 0) = server_binding[b].server;
	      SERVER (sock->data, 1) = NULL;
	    }
	}
    }
  
  if (server_bindings)
    {
      svz_free (server_binding);
      server_binding = NULL;
      server_bindings = 0;
      return 0;
    }

  log_printf (LOG_FATAL, "no server instances found\n");
  return -1;
}
