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
 * $Id: binding.c,v 1.3 2001/04/28 12:37:06 ela Exp $
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

/*
 * Return a @code{socket_t} representing a server socket with the port
 * configuration @var{port} and add the given server instance @var{server}
 * to this structure. If there is no such port configuration yet return
 * @code{NULL}.
 */
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
	  /* Could not create this port configuration listener. */
	  else
	    {
	      svz_portcfg_destroy (copy);
	      svz_free (copy);
	    }
	}
      /* Port configuration already exists. */
      else
	{
	  svz_portcfg_destroy (copy);
	  svz_free (copy);
	}
    }
  svz_array_destroy (ports);
  return 0;
}
