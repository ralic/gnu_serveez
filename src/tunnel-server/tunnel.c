/*
 * tunnel.c - port forward implementations
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.  
 *
 * $Id: tunnel.c,v 1.3 2000/10/25 07:54:06 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_TUNNEL

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#ifndef __MINGW32__
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#include "util.h"
#include "hash.h"
#include "alloc.h"
#include "socket.h"
#include "connect.h"
#include "udp-socket.h"
#include "server-core.h"
#include "server.h"
#include "tunnel.h"

tnl_config_t tnl_config = 
{
  NULL, /* the source port to forward from */
  NULL, /* target port to forward to */
  NULL, /* the source client socket hash */
  NULL  /* target client hash */
};

/*
 * Defining configuration file associations with key-value-pairs.
 */
key_value_pair_t tnl_config_prototype [] = 
{
  REGISTER_PORTCFG ("source", tnl_config.source, NOTDEFAULTABLE),
  REGISTER_PORTCFG ("target", tnl_config.target, NOTDEFAULTABLE),
  REGISTER_END ()
};

/*
 * Definition of this server.
 */
server_definition_t tnl_server_definition =
{
  "tunnel server",
  "tunnel",
  tnl_global_init,
  tnl_init,
  tnl_detect_proto,
  tnl_connect_socket,
  tnl_finalize,
  tnl_global_finalize,
  NULL,
  NULL,
  NULL,
  NULL,
  &tnl_config,
  sizeof (tnl_config),
  tnl_config_prototype
};

int
tnl_global_init (void)
{
  return 0;
}

int
tnl_global_finalize (void)
{
  return 0;
}

/*
 * Tunnel server instance initializer. Check the configuration.
 */
int
tnl_init (server_t *server)
{
  tnl_config_t *cfg = server->cfg;

  /* protocol supported ? */
  if (!(cfg->source->proto & (PROTO_TCP | PROTO_ICMP | PROTO_UDP)) ||
      !(cfg->target->proto & (PROTO_TCP | PROTO_ICMP | PROTO_UDP)))
    {
      log_printf (LOG_ERROR, "tunnel: protocol not supported\n");
      return -1;
    }

  /* check identity of source and target port configurations */
  if (server_portcfg_equal (cfg->source, cfg->target))
    {
      log_printf (LOG_ERROR, "tunnel: source and target identical\n");
      return -1;
    }

  /* broadcast target ip address not allowed */
  if (cfg->target->localaddr->sin_addr.s_addr == INADDR_ANY)
    {
      log_printf (LOG_ERROR, "tunnel: broadcast target ip not allowed\n");
      return -1;
    }

  /* bind the source port */
  server_bind (server, cfg->source);
  return 0;
}

int
tnl_finalize (server_t *server)
{
  return 0;
}

/*
 * Tunnel server TCP detection routine. It is greedy. Thus it cannot share
 * the port with other servers.
 */
int
tnl_detect_proto (void *cfg, socket_t sock)
{
  log_printf (LOG_NOTICE, "tunnel: TCP connection accepted\n");
  return -1;
}

/*
 * If any TCP connection has been accepted this routine is called to setup
 * the tunnel server specific callbacks.
 */
int
tnl_connect_socket (void *config, socket_t sock)
{
  tnl_config_t *cfg = config;
  socket_t xsock;
  unsigned long ip;
  unsigned short port;
  
  sock->flags |= SOCK_FLAG_NOFLOOD;
  sock->check_request = tnl_check_request_tcp;
  sock->disconnected_socket = tnl_disconnect;

  /* depending on the target configuration we assign different callbacks */
  switch (cfg->target->proto)
    {
    case PROTO_TCP:
      sock->userflags |= TNL_FLAG_TGT_TCP;
      break;
    case PROTO_UDP:
      sock->userflags |= TNL_FLAG_TGT_UDP;
      break;
    case PROTO_ICMP:
      sock->userflags |= TNL_FLAG_TGT_ICMP;
      break;
    default:
      log_printf (LOG_ERROR, "tunnel: invalid target configuration\n");
      return -1;
    }

  /* target is a tcp connection */
  if (sock->userflags & TNL_FLAG_TGT_TCP)
    {
      ip = cfg->target->localaddr->sin_addr.s_addr;
      port = cfg->target->localaddr->sin_port;

      if ((xsock = sock_connect (ip, port)) == NULL)
	{
	  log_printf (LOG_ERROR, "tunnel: tcp: cannot connect to %s:%u\n",
		      util_inet_ntoa (ip), ntohs (port));
	  return -1;
	}

#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "tunnel: tcp: connecting to %s:%u\n",
		  util_inet_ntoa (ip), ntohs (port));
#endif
      xsock->cfg = cfg;
      xsock->flags |= SOCK_FLAG_NOFLOOD;
      xsock->userflags = sock->userflags | TNL_FLAG_SRC_TCP;
      xsock->check_request = tnl_check_request_tcp;
      xsock->disconnected_socket = tnl_disconnect;
      xsock->referer = sock;
      sock->referer = xsock;
    }

  /* target is an udp connection */
  else if (sock->userflags & TNL_FLAG_TGT_UDP)
    {
      ip = cfg->target->localaddr->sin_addr.s_addr;
      port = cfg->target->localaddr->sin_port;

      if ((xsock = udp_connect (ip, port)) == NULL)
	{
	  log_printf (LOG_ERROR, "tunnel: udp: cannot connect to %s:%u\n",
		      util_inet_ntoa (ip), ntohs (port));
	  return -1;
	}

#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "tunnel: udp: connecting to %s:%u\n",
		  util_inet_ntoa (ip), ntohs (port));
#endif
      xsock->cfg = cfg;
      xsock->flags |= SOCK_FLAG_NOFLOOD;
      xsock->userflags = sock->userflags | TNL_FLAG_SRC_TCP;
      xsock->handle_request = tnl_handle_request_udp;
      xsock->disconnected_socket = tnl_disconnect;
      xsock->referer = sock;
      sock->referer = xsock;
    }

  return 0;
}

/*
 * The tunnel servers TCP check_request routine. It simply copies the 
 * received data to the send buffer.
 */
int
tnl_check_request_tcp (socket_t sock)
{
  tnl_config_t *cfg = sock->cfg;

  /* target is TCP */
  if (sock->userflags & TNL_FLAG_TGT_TCP)
    {
      if (sock_write (sock->referer, sock->recv_buffer, 
		      sock->recv_buffer_fill) == -1)
	{
	  return -1;
	}
    }
  /* target is UDP */
  else if (sock->userflags & TNL_FLAG_TGT_UDP)
    {
      if (udp_write (sock->referer, sock->recv_buffer, 
		     sock->recv_buffer_fill) == -1)
	{
	  return -1;
	}
    }

  sock->recv_buffer_fill = 0;
  return 0;
}

/*
 * This function is the handle_request routine for UDP sockets.
 */
int
tnl_handle_request_udp (socket_t sock, char *packet, int len)
{
  if (sock->userflags & TNL_FLAG_SRC_TCP)
    {
      if (sock_write (sock->referer, packet, len) == -1)
	{
	  return -1;
	}
    }
  return 0;
}

/*
 * What happens if a connection gets lost.
 */
int
tnl_disconnect (socket_t sock)
{
  if (sock->userflags & (TNL_FLAG_TGT_TCP | TNL_FLAG_TGT_UDP |
			 TNL_FLAG_SRC_TCP | TNL_FLAG_SRC_UDP))
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "tunnel: shutdown referrer id %d\n",
		  sock->referer->id);
#endif
      sock_schedule_for_shutdown (sock->referer);
    }
  return 0;
}

#else /* not ENABLE_TUNNEL */

int tunnel_dummy; /* Shut compiler warnings up. */

#endif /* not ENABLE_TUNNEL */
