/*
 * gnutella.c - gnutella protocol implementation
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
 * $Id: gnutella.c,v 1.1 2000/08/26 18:05:18 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_GNUTELLA

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef __MINGW32__
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "alloc.h"
#include "util.h"
#include "socket.h"
#include "connect.h"
#include "server.h"
#include "gnutella.h"

portcfg_t nut_port = 
{
  PROTO_TCP,      /* we are tcp */
  NUT_PORT,       /* standard port to listen on */
  "*",            /* bind all local addresses */
  NULL,           /* calculated from above values later */
  NULL,           /* no inpipe for us */
  NULL            /* no outpipe for us */
};

nut_config_t nut_config = 
{
  &nut_port,
  0,
  NULL,
  NUT_GUID,
  NULL,
  NULL,
  "Puppe3000"
};

/*
 * Defining configuration file associations with key-value-pairs.
 */
key_value_pair_t nut_config_prototype [] = 
{
  REGISTER_PORTCFG ("port", nut_config.port, DEFAULTABLE),
  REGISTER_STRARRAY ("hosts", nut_config.hosts, NOTDEFAULTABLE),
  REGISTER_STR ("search", nut_config.search, DEFAULTABLE),
  REGISTER_END ()
};

/*
 * Definition of this server.
 */
server_definition_t nut_server_definition =
{
  "gnutella spider version " NUT_VERSION,
  "nut",
  nut_global_init,
  nut_init,
  nut_detect_proto,
  nut_connect_socket,
  nut_finalize,
  nut_global_finalize,
  NULL,
  NULL,
  NULL,
  NULL,
  &nut_config,
  sizeof (nut_config),
  nut_config_prototype
};

static void
nut_calc_guid (byte *guid)
{
  int n;

  for (n = 0; n < NUT_GUID_SIZE; n++)
    {
      /*guid[n] = 256 * rand () / (RAND_MAX + 1);*/
      guid[n] = (rand () >> 1) & 0xff;
    }
}

unsigned
nut_hash_keylen (char *id)
{
  return NUT_GUID_SIZE;
}

int 
nut_hash_equals (char *id1, char *id2)
{
  return memcmp (id1, id2, NUT_GUID_SIZE);
}
 
unsigned long 
nut_hash_code (char *id)
{
  int n;
  unsigned long code = 0;
  
  for (n = 0; n < NUT_GUID_SIZE; n++)
    code = (code << 2) ^ id[n];

  return code;
}

int
nut_init_ping (socket_t sock)
{
  nut_header_t hdr;

  nut_calc_guid ((byte *) &hdr.id);
  hdr.function = NUT_PING_REQ;
  hdr.ttl = 0x05;
  hdr.hop = 0;
  hdr.length = 0;

  sock_write (sock, (char *) &hdr, sizeof (nut_header_t));

  return 0;
}

int
nut_global_init (void)
{
  srand (time (NULL));
  return 0;
}

static int
nut_parse_addr (char *addr, unsigned long *ip, unsigned short *port)
{
  char *p = addr, *colon;

  /* skip leading invalid characters */
  while (*p < '0' && *p > '9' && *p) p++;
  if (!*p) return -1;
  
  /* find seperating colon */
  colon = p;
  while (*colon != ':' && *colon) colon++;
  if (!*colon) return -1;
  *colon = '\0';
  colon++;

  *ip = inet_addr (p);
  *port = htons (util_atoi (colon));
  return 0;
}

int
nut_init (server_t *server)
{
  nut_config_t *cfg = server->cfg;
  socket_t sock;
  int n = 0;
  unsigned long ip;
  unsigned short port;

  cfg->conn = hash_create (4);
  cfg->route = hash_create (4);
  cfg->route->code = nut_hash_code;
  cfg->route->equals = nut_hash_equals;
  cfg->route->keylen = nut_hash_keylen;
  nut_calc_guid (cfg->guid);

  if (cfg->hosts)
    {
      while (cfg->hosts[n])
	{
	  if (nut_parse_addr (cfg->hosts[n], &ip, &port) == -1)
	    {
	      log_printf (LOG_ERROR, "nut: invalid address %s\n",
			  cfg->hosts[n]);
	      n++;
	      continue;
	    }

	  if ((sock = sock_connect (ip, port)) != NULL)
	    {
	      log_printf (LOG_NOTICE, "nut: try connecting %s:%u\n",
			  util_inet_ntoa (ip), ntohs (port));

	      sock->cfg = cfg;
	      sock->disconnected_socket = nut_disconnect;
	      sock->check_request = nut_check_request;
	      sock->idle_func = nut_idle_searching;
	      sock->idle_counter = NUT_SEARCH_INTERVAL;
	      sock_printf (sock, NUT_CONNECT);
	      hash_put (cfg->conn, util_itoa (sock->id), sock);
	    }
	  n++;
	}
    }

  server_bind (server, cfg->port);
  return 0;
}

int
nut_finalize (server_t *server)
{
  nut_config_t *cfg = server->cfg;

  hash_destroy (cfg->conn);
  hash_destroy (cfg->route);
  return 0;
}

int
nut_global_finalize (void)
{
  return 0;
}

static int
nut_reply (socket_t sock, nut_header_t *hdr, nut_reply_t *reply)
{
  nut_config_t *cfg = sock->cfg;
  socket_t xsock;

  hdr->hop++;
  hdr->ttl--;

  xsock = (socket_t) hash_get (cfg->route, (char *) hdr->id);

  if (xsock == NULL)
    {
      log_printf (LOG_DEBUG, "nut: packet received !!!\n");
      return -1;
    }

  hash_delete (cfg->route, (char *) hdr->id);
  sock_write (xsock, (char *) hdr, sizeof (nut_header_t));
  sock_write (xsock, (char *) reply, hdr->length);

  return 0;
}

static int
nut_query (socket_t sock, nut_header_t *hdr, nut_query_t *query)
{
  nut_config_t *cfg = sock->cfg;
  socket_t xsock;
  socket_t *conn;
  int n;

  if (--hdr->ttl > 0)
    {
      hdr->hop++;
      xsock = (socket_t) hash_get (cfg->route, (char *) hdr->id);
      if (xsock != NULL)
	{
	  log_printf (LOG_DEBUG, "nut: dropping packet\n");
	  return -1;
	}
      hash_put (cfg->route, (char *) hdr->id, sock);

      if ((conn = (socket_t *) hash_values (cfg->conn)) != NULL)
	{
	  for (n = 0; n < hash_size (cfg->conn); n++)
	    {
	      xsock = conn[n];
	      if (xsock != sock)
		{
		  sock_write (xsock, (char *) hdr, sizeof (nut_header_t));
		  sock_write (xsock, (char *) query, hdr->length);
		}
	    }
	  hash_xfree (conn);
	}
    }
  return 0;
}

static int
nut_ping_request (socket_t sock, nut_header_t *hdr, void *null)
{
  nut_config_t *cfg = sock->cfg;
  socket_t xsock;
  socket_t *conn;
  nut_ping_reply_t reply;
  int n;

  if (--hdr->ttl > 0)
    {
      hdr->hop++;
      xsock = (socket_t) hash_get (cfg->route, (char *) hdr->id);
      if (xsock != NULL)
	{
	  log_printf (LOG_DEBUG, "nut: dropping packet\n");
	  return -1;
	}
      hash_put (cfg->route, (char *) hdr->id, sock);

      if ((conn = (socket_t *) hash_values (cfg->conn)) != NULL)
	{
	  for (n = 0; n < hash_size (cfg->conn); n++)
	    {
	      xsock = conn[n];
	      if (xsock != sock)
		{
		  sock_write (xsock, (char *) hdr, sizeof (nut_header_t));
		}
	    }
	  hash_xfree (conn);
	}
      
      hdr->function = NUT_PING_ACK;
      hdr->length = sizeof (nut_ping_reply_t);
      reply.port = sock->local_port;
      reply.ip = sock->local_addr;
      reply.files = 0;
      reply.size = 0;
      sock_write (sock, (char *) hdr, sizeof (nut_header_t));
      sock_write (sock, (char *) &reply, hdr->length);
    }
  return 0;
}

int
nut_disconnect (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;
  byte *id;

  while ((id = (byte *) hash_contains (cfg->route, sock)) != NULL)
    hash_delete (cfg->route, (char *) id);

  hash_delete (cfg->conn, util_itoa (sock->id));
  return 0;
}

int
nut_check_request (socket_t sock)
{
  nut_header_t *hdr;
  void *request;
  int len = strlen (NUT_OK);
  unsigned fill = sock->recv_buffer_fill;

  if (fill >= (unsigned) len && !memcmp (sock->recv_buffer, NUT_OK, len))
    {
      log_printf (LOG_NOTICE, "nut: host %s:%u connected\n",
		  util_inet_ntoa (sock->remote_addr),
		  ntohs (sock->remote_port));
      sock_reduce_recv (sock, len);
      nut_init_ping (sock);
    }

  while ((fill = sock->recv_buffer_fill) >= sizeof (nut_header_t))
    {
      hdr = (nut_header_t *) sock->recv_buffer;

      if (fill >= sizeof (nut_header_t) + hdr->length)
	{
	  len = sizeof (nut_header_t) + hdr->length;
	  request = sock->recv_buffer + sizeof (nut_header_t);
#if 1
	  util_hexdump (stdout, "gnutella packet", sock->sock_desc,
			sock->recv_buffer, len, 0);
#endif
	  

	  switch (hdr->function)
	    {
	    case NUT_PING_REQ:
	      nut_ping_request (sock, hdr, NULL);
	      break;
	    case NUT_PING_ACK:
	      break;
	    case NUT_PUSH_REQ:
	      break;
	    case NUT_SEARCH_REQ:
	      nut_query (sock, hdr, request);
	      break;
	    case NUT_SEARCH_ACK:
	      nut_reply (sock, hdr, request);
	      break;
	    default:
	      log_printf (LOG_ERROR, "nut: invalid request 0x%02X\n",
			  hdr->function);
	    }
	  sock_reduce_recv (sock, len);
	}
      else
	break;
    }
  return 0;
}

int
nut_idle_searching (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;
  nut_header_t hdr;
  nut_query_t query;
  socket_t *conn;
  int n, len;

  if (cfg->search)
    {
      if ((conn = (socket_t *) hash_values (cfg->conn)) != NULL)
	{
	  nut_calc_guid ((byte *) &hdr.id);
	  hdr.function = NUT_SEARCH_REQ;
	  hdr.ttl = 0x05;
	  hdr.hop = 0;
	  hdr.length = strlen (cfg->search) + 1;
	  len = sizeof (nut_query_t) - sizeof (char *);
	  hdr.length += len;

	  for (n = 0; n < hash_size (cfg->conn); n++)
	    {
	      sock_write (conn[n], (char *) &hdr, sizeof (hdr));
	      sock_write (conn[n], (char *) &query, len);
	      sock_write (conn[n], cfg->search, strlen (cfg->search) + 1);
	    }
	  hash_xfree (conn);
	}
    }

  sock->idle_counter = NUT_SEARCH_INTERVAL;
  return 0;
}

int 
nut_detect_proto (void *nut_cfg, socket_t sock)
{
  int len = strlen (NUT_CONNECT);

  if (sock->recv_buffer_fill >= len &&
      !memcmp (sock->recv_buffer, NUT_CONNECT, len))
    {
      log_printf (LOG_NOTICE, "gnutella protocol detected\n");
      sock_reduce_recv (sock, len);
      return -1;
    }
  return 0;
}

int 
nut_connect_socket (void *nut_cfg, socket_t sock)
{
  nut_config_t *cfg = nut_cfg;

  if (sock_printf (sock, NUT_OK) == -1)
    return -1;

  sock->disconnected_socket = nut_disconnect;
  sock->check_request = nut_check_request;
  sock->idle_func = nut_idle_searching;
  sock->idle_counter = NUT_SEARCH_INTERVAL;
  nut_init_ping (sock);
  hash_put (cfg->conn, util_itoa (sock->id), sock);

  return 0;
}

int have_gnutella = 1;

#else /* ENABLE_GNUTELLA */

int have_gnutella = 0;	/* Shut compiler warnings up, remember for runtime */

#endif /* not ENABLE_GNUTELLA */
