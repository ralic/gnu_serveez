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
 * $Id: binding.c,v 1.10 2001/06/07 17:22:01 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <assert.h>
#include <string.h>
#include <sys/types.h>

#ifndef __MINGW32__
# include <sys/socket.h>
#endif

#ifdef __MINGW32__
# include <winsock2.h>
#endif

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
 * Return a static text representation of the server instances @var{server}
 * port configuration bindings.
 */
char *
svz_server_bindings (svz_server_t *server)
{
  static char text[256];
  svz_socket_t *sock;
  struct sockaddr_in *addr;
  svz_portcfg_t *port;

  text[0] = '\0'; 
  svz_sock_foreach (sock)
    {
      if (sock->flags & SOCK_FLAG_LISTENING && sock->port)
	{
	  /* The server in the array of servers and port configuration
	     valid ? */
	  if (svz_array_contains (sock->data, server) && 
	      (port = sock->port) != NULL)
	    {
	      /* TCP and UDP */ 
	      if (port->proto & (PROTO_TCP | PROTO_UDP))
		{
		  addr = svz_portcfg_addr (port);
		  strcat (text, svz_inet_ntoa (addr->sin_addr.s_addr));
		  strcat (text, ":");
		  strcat (text, svz_itoa (ntohs (addr->sin_port)));
		}
	      /* RAW and ICMP */
	      else if (port->proto & (PROTO_RAW | PROTO_ICMP))
		{
		  addr = svz_portcfg_addr (port);
		  strcat (text, svz_inet_ntoa (addr->sin_addr.s_addr));
		}
	      /* PIPE */
	      else if (port->proto & PROTO_PIPE)
		{
		  strcat (text, port->pipe_recv.name);
		  strcat (text, "<-->");
		  strcat (text, port->pipe_send.name);
		}
	      strcat (text, " ");
	    }
	}
    }
  if (strlen (text))
    text[strlen (text) - 1] = '\0';
  return text;
}

/*
 * Return an array of port configuration to which the given server instance
 * @var{server} is currently bound to or @code{NULL} ff there is no such 
 * binding.
 */
svz_array_t *
svz_server_portcfg (svz_server_t *server)
{
  svz_array_t *port = svz_array_create (1);
  svz_socket_t *sock;

  svz_sock_foreach (sock)
    {
      if (sock->flags & SOCK_FLAG_LISTENING && sock->port)
	{
	  if (svz_array_contains (sock->data, server))
	    svz_array_add (port, sock->port);
	}
    }
  if (svz_array_size (port) == 0)
    {
      svz_array_destroy (port);
      port = NULL;
    }
  return port;
}

/*
 * Return a @code{socket_t} representing a server socket with the port
 * configuration @var{port} and add the given server instance @var{server}
 * to this structure. If there is no such port configuration yet return
 * @code{NULL}.
 */
static svz_socket_t *
svz_server_find_portcfg (svz_server_t *server, svz_portcfg_t *port)
{
  svz_socket_t *sock;
  
  /* Look for duplicate port configurations. */
  svz_sock_foreach (sock)
    {
      /* Is this socket usable for this port configuration ? */
      if (sock->data && sock->port && sock->flags & SOCK_FLAG_LISTENING &&
	  svz_portcfg_equal (sock->port, port))
	{
	  /* Do not bind a server to a single port twice. */
	  if (!svz_array_contains (sock->data, server))
	    {
	      /* Extend the server array in sock->data. */
	      svz_array_add (sock->data, server);
	    }
	  break;
	}
    }
  return sock;
}

/*
 * Bind the server instance @var{server} to the port configuration 
 * @var{port} if possible. Return non-zero on errors otherwise zero. It
 * might occur that a single server is bound to more than one network port 
 * if e.g. the TCP/IP address is specified by "*" since we do not do any
 * @code{INADDR_ANY} bindings.
 */
int
svz_server_bind (svz_server_t *server, svz_portcfg_t *port)
{
  svz_array_t *ports;
  svz_socket_t *sock;
  svz_portcfg_t *copy;
  int n;

  /* First expand the given port configuration. */
  ports = svz_portcfg_expand (port);
  svz_array_foreach (ports, copy, n)
    {
      /* Prepare port configuartion. */
      svz_portcfg_prepare (copy);

      /* Find appropriate socket structure for this port configuration. */
      if ((sock = svz_server_find_portcfg (server, copy)) == NULL)
	{
	  /* Try creating a server socket. */
	  if ((sock = svz_server_create (copy)) != NULL)
	    {
	      /* 
	       * Enqueue the server socket, put the port config into
	       * sock->port and initialize the server array (sock->data).
	       */
	      svz_sock_enqueue (sock);
	      sock->port = copy;
	      sock->data = svz_array_create (1);
	      svz_array_add (sock->data, server);
	    }
	  /* Could not create this port configuration listener. */
	  else
	    {
	      svz_portcfg_destroy (copy);
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
 * Remove the given server instance @var{server} entirely from the list
 * of enqueued sockets. This means to delete it from each server socket on
 * the one hand and to shutdown every child client spawned from this server
 * on the other hand.
 */
void
svz_server_unbind (svz_server_t *server)
{
  svz_socket_t *sock, *parent;
  unsigned long n;

  /* Go through all enqueued sockets. */
  svz_sock_foreach (sock)
    {
      /* Client structures. */
      if (!(sock->flags & SOCK_FLAG_LISTENING) && 
	  (parent = svz_sock_getparent (sock)) != NULL)
	{
	  /* If the parent of a client is the given servers child
	     then also shutdown this client. */
	  if (parent->flags & SOCK_FLAG_LISTENING && parent->port && 
	      parent->data && svz_array_contains (parent->data, server))
	    svz_sock_schedule_for_shutdown (sock);
	}
    }

  /* Go through all enqueued sockets once more. */
  svz_sock_foreach (sock)
    {
      /* A server socket structure ? */
      if (sock->flags & SOCK_FLAG_LISTENING && sock->port && sock->data)
	{
	  /* Delete all servers and shutdown the socket structure if
	     there are no more servers left. */
	  while ((n = svz_array_idx (sock->data, server)) != 
		 (unsigned long) -1)
	    svz_array_del (sock->data, n);
	  if (svz_array_size (sock->data) == 0)
	    svz_sock_schedule_for_shutdown (sock);
	}
    }
}
