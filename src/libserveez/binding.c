/*
 * binding.c - server to port binding implementation
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: binding.c,v 1.2 2001/04/19 14:08:09 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "libserveez/alloc.h"
#include "libserveez/util.h"
#include "libserveez/socket.h"
#include "libserveez/core.h"
#include "libserveez/server.h"
#include "libserveez/server-core.h"
#include "libserveez/portcfg.h"
#include "libserveez/server-socket.h"
#include "libserveez/binding.h"

static socket_t
svz_server_find_portcfg (svz_server_t *server, svz_portcfg_t *port)
{
  socket_t sock;
  
  /* Look for duplicate port configurations. */
  svz_sock_foreach (sock)
    {
      /* Is this socket usable for this port configuration ? */
      if (sock->data && sock->cfg && sock->flags & SOCK_FLAG_LISTENING &&
	  svz_portcfg_equal (sock->cfg, port))
	{
	  /* Extend the server array in sock->data. */
	  svz_array_add (sock->data, server);
	  break;
	}
    }
  return sock;
}

/*
 * Bind the server instance @var{server} to the port configuration 
 * @var{port} if possible. Return non-zero on errors otherwise zero. It
 * might occur that a single server is bound to more than one network port 
 * if e.g. the tcp/ip address is specified by "*" since we do not do any
 * INADDR_ANY bindings.
 */
int
svz_server_bind (svz_server_t *server, svz_portcfg_t *port)
{
  svz_array_t *ports;
  socket_t sock;
  svz_portcfg_t *copy;
  int n;

  /* First expand the given port configuration. */
  ports = svz_portcfg_expand (port);
  svz_array_foreach (ports, copy, n)
    {
      /* Find appropriate socket structure for this port configuration. */
      if ((sock = svz_server_find_portcfg (server, copy)) == NULL)
	{
	  /* Try creating a server socket. */
	  if ((sock = svz_server_create (copy)) != NULL)
	    {
	      /* 
	       * Enqueue the server socket, put the port config into
	       * sock->cfg and initialize the server array (sock->data).
	       */
	      sock_enqueue (sock);
	      sock->cfg = copy;
	      sock->data = svz_array_create (1);
	      svz_array_add (sock->data, server);
	    }
	}
      /* Port configuration already exists. */
      else
	{
	  svz_portcfg_destroy (copy);
	}
    }
  svz_array_destroy (ports);
  return 0;
}

/*
 * A list of bound servers.
 */
int server_bindings = 0;
server_binding_t *server_binding = NULL;

/*
 * This functions binds a previously instantiated server to a specified
 * port configuration.
 */
int
server_bind (svz_server_t *server, portcfg_t *cfg)
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
