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
 * $Id: gnutella.c,v 1.2 2000/08/27 17:29:51 ela Exp $
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

/*
 * This port configuration is the default port for the gnutella server to
 * listen on.
 */
portcfg_t nut_port = 
{
  PROTO_TCP,      /* tcp port */
  NUT_PORT,       /* standard port to listen on */
  "*",            /* bind all local addresses */
  NULL,           /* calculated later */
  NULL,           /* no inpipe */
  NULL            /* no outpipe */
};

/*
 * Default configuration hash for the gnutella spider.
 */
nut_config_t nut_config = 
{
  &nut_port,   /* port configuration */
  NUT_MAX_TTL, /* maximum ttl for a gnutella packet */
  NUT_TTL,     /* initial ttl for a gnutella packet */
  NULL,        /* array of initial hosts */
  NUT_GUID,    /* this servers GUID */
  NULL,        /* routing table */
  NULL,        /* connected hosts hash */
  "Puppe3000", /* default search pattern */
  NULL,        /* this servers created packets */
  0,           /* routing errors */
  0,           /* files within connected network */
  0,           /* file size (in KB) */
};

/*
 * Defining configuration file associations with key-value-pairs.
 */
key_value_pair_t nut_config_prototype [] = 
{
  REGISTER_PORTCFG ("port", nut_config.port, DEFAULTABLE),
  REGISTER_STRARRAY ("hosts", nut_config.hosts, NOTDEFAULTABLE),
  REGISTER_STR ("search", nut_config.search, DEFAULTABLE),
  REGISTER_INT ("max-ttl", nut_config.max_ttl, DEFAULTABLE),
  REGISTER_INT ("ttl", nut_config.ttl, DEFAULTABLE),
  REGISTER_END ()
};

/*
 * Definition of this server.
 */
server_definition_t nut_server_definition =
{
  "gnutella spider version " NUT_VERSION, /* long description */
  "nut",                                  /* instance description */
  nut_global_init,                        /* global initializer */
  nut_init,                               /* instance initializer */
  nut_detect_proto,                       /* protocol detection */
  nut_connect_socket,                     /* client connection callback */
  nut_finalize,                           /* instance destructor */
  nut_global_finalize,                    /* class destructor */
  NULL,                                   /* */
  NULL,                                   /* */
  NULL,                                   /* */
  NULL,                                   /* */
  &nut_config,                            /* default configuration */
  sizeof (nut_config),                    /* size of this configuration */
  nut_config_prototype                    /* configuration items */
};

/*
 * This routine randomly calculates a Globally Unique Identifier (GUID)
 * and stores it in the given argument.
 */
static void
nut_calc_guid (byte *guid)
{
  int n;

  for (n = 0; n < NUT_GUID_SIZE; n++)
    {
      /*guid[n] = 256 * rand () / RAND_MAX;*/
      guid[n] = (rand () >> 1) & 0xff;
    }
}


/*
 * The next three functions `nut_hash_keylen', `nut_hash_equals' and
 * `nut_hash_code' are the routing table hash callbacks to handle
 * GUIDs as keys instead of plain NULL terminated character strings.
 */
static unsigned
nut_hash_keylen (char *id)
{
  return NUT_GUID_SIZE;
}

static int 
nut_hash_equals (char *id1, char *id2)
{
  return memcmp (id1, id2, NUT_GUID_SIZE);
}
 
static unsigned long 
nut_hash_code (char *id)
{
  int n;
  unsigned long code = 0;
  
  for (n = 0; n < NUT_GUID_SIZE; n++)
    {
      code = (code << 2) ^ id[n];
    }

  return code;
}

/*
 * This routine parses a `ip:port' combination from the given character
 * string ADDR and stores both of the values in IP and PORT in network
 * byte order.
 */
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

  /* convert and store both of the parsed values */
  *ip = inet_addr (p);
  *port = htons (util_atoi (colon));
  return 0;
}

/*
 * When establishing a new connection to another gnutella server this
 * functions pings it to get all further servers behind this server.
 */
int
nut_init_ping (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;
  nut_header_t hdr;

  nut_calc_guid ((byte *) &hdr.id);
  hdr.function = NUT_PING_REQ;
  hdr.ttl = cfg->ttl;
  hdr.hop = 0;
  hdr.length = 0;
  hash_put (cfg->packet, (char *) &hdr.id, sock);

  return sock_write (sock, (char *) &hdr, sizeof (nut_header_t));
}

/*
 * The gnutella servers global initializer.
 */
int
nut_global_init (void)
{
  srand (time (NULL));
  return 0;
}

/*
 * Gnutella spider server's instance initializer.
 */
int
nut_init (server_t *server)
{
  nut_config_t *cfg = server->cfg;
  socket_t sock;
  int n = 0;
  unsigned long ip;
  unsigned short port;

  /* create and modify packet hash */
  cfg->packet = hash_create (4);
  cfg->packet->code = nut_hash_code;
  cfg->packet->equals = nut_hash_equals;
  cfg->packet->keylen = nut_hash_keylen;

  /* create current connection hash */
  cfg->conn = hash_create (4);

  /* create and modify the routing table hash */
  cfg->route = hash_create (4);
  cfg->route->code = nut_hash_code;
  cfg->route->equals = nut_hash_equals;
  cfg->route->keylen = nut_hash_keylen;

  /* calculate this server instance's GUID */
  nut_calc_guid (cfg->guid);

  /* go through all given hosts */
  if (cfg->hosts)
    {
      while (cfg->hosts[n])
	{
	  /* try getting ip address and port */
	  if (nut_parse_addr (cfg->hosts[n], &ip, &port) == -1)
	    {
	      log_printf (LOG_WARNING, "nut: invalid host `%s'\n",
			  cfg->hosts[n]);
	      n++;
	      continue;
	    }

	  /* try to connect to this host */
	  if ((sock = sock_connect (ip, port)) != NULL)
	    {
	      log_printf (LOG_NOTICE, "nut: connecting %s:%u\n",
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

  /* bind listening server to configurable port address */
  server_bind (server, cfg->port);
  return 0;
}

/*
 * Instance finalizer.
 */
int
nut_finalize (server_t *server)
{
  nut_config_t *cfg = server->cfg;

  hash_destroy (cfg->conn);
  hash_destroy (cfg->route);
  hash_destroy (cfg->packet);
  return 0;
}

/*
 * Global gnutella finalizer.
 */
int
nut_global_finalize (void)
{
  return 0;
}

/*
 * This is the routing routine for any incoming gnutella packet.
 * It return non-zero on routing errors and packet death. Otherwise
 * zero.
 */
static int
nut_route (socket_t sock, nut_header_t *hdr, void *packet)
{
  nut_config_t *cfg = sock->cfg;
  socket_t xsock;
  socket_t *conn;
  int n;

  /* check the current TTL value */
  if (--hdr->ttl <= 0)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: packet 0x%02X died (zero TTL)\n",
		  hdr->function);
#endif
      return -1;
    }

  /* 
   * Set the outgoing TTL to either MyTTL or the MAX_TTL value of this
   * gnutella server.
   */
  if (hdr->ttl > cfg->max_ttl)
    hdr->ttl = cfg->max_ttl;
  if (hdr->ttl > cfg->ttl)
    hdr->ttl = cfg->ttl;
  
  /* check if this packet had enough life */
  if (hdr->hop > cfg->max_ttl)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: packet 0x%02X died (HOP > TTL)\n",
		  hdr->function); 
#endif
      return -1;
    }

  /* route replies here */
  if (hdr->function & 0x01)
    {
      hdr->hop++;

      /* is the GUID in the routing hash ? */
      xsock = (socket_t) hash_get (cfg->route, (char *) hdr->id);
      if (xsock == NULL)
	{
	  xsock = (socket_t) hash_get (cfg->packet, (char *) hdr->id);
	  if (xsock == NULL)
	    {
	      log_printf (LOG_ERROR, "nut: error routing packet 0x%02X\n",
			  hdr->function);
	      cfg->errors++;
	      return -1;
	    }
#if 1
	  log_printf (LOG_DEBUG, "nut: packet 0x%02X reply received\n",
		      hdr->function);
#endif
	}
      /* yes, send it to the connection the query came from */
      else
	{
	  sock_write (xsock, (char *) hdr, sizeof (nut_header_t));
	  if (hdr->length)
	    sock_write (xsock, packet, hdr->length);
	}
    }
  /* route queries here */
  else
    {
      hdr->hop++;

      /* check if this query has been seen already */
      xsock = (socket_t) hash_get (cfg->route, (char *) hdr->id);
      if (xsock != NULL)
	{
	  log_printf (LOG_DEBUG, "nut: dropping duplicate packet 0x%02X\n",
		      hdr->function);
	  return -1;
	}
      /* add the query to routing table */
      hash_put (cfg->route, (char *) hdr->id, sock);

      /* 
       * Forward this query to all connections except the connection
       * the server got it from.
       */
      if ((conn = (socket_t *) hash_values (cfg->conn)) != NULL)
	{
	  for (n = 0; n < hash_size (cfg->conn); n++)
	    {
	      xsock = conn[n];
	      if (xsock == sock) continue;
	      sock_write (xsock, (char *) hdr, sizeof (nut_header_t));
	      if (hdr->length)
		sock_write (xsock, packet, hdr->length);
	    }
	  hash_xfree (conn);
	}
      
    }
  return 0;
}

/*
 * This routine will be called when a search reply occurs. Here we
 * can check if the reply was created by a packet we sent ourselves.
 */
static int
nut_reply (socket_t sock, nut_header_t *hdr, nut_reply_t *reply)
{
  nut_config_t *cfg = sock->cfg;
  socket_t xsock;

  if ((xsock = (socket_t) hash_get (cfg->packet, (char *) hdr->id)) != NULL)
    {
#if 1
      printf ("records : %d\n", reply->records);
      printf ("port    : %u\n", ntohs (reply->port));
      printf ("ip      : %s\n", util_inet_ntoa (reply->ip));
      printf ("speed   : %d kbit/s\n", reply->speed);
#endif
    } 

  return 0;
}

/*
 * This is called whenever there was a search query received.
 */
static int
nut_query (socket_t sock, nut_header_t *hdr, nut_query_t *query)
{
  nut_config_t *cfg = sock->cfg;

  return 0;
}

/*
 * This routine will be called when some gnutella server sent a
 * ping reply.
 */
static int
nut_ping_reply (socket_t sock, nut_header_t *hdr, nut_ping_reply_t *reply)
{
  nut_config_t *cfg = sock->cfg;
  socket_t xsock;

  if ((xsock = (socket_t) hash_get (cfg->packet, (char *) hdr->id)) != NULL)
    {
#if 1
      printf ("port    : %u\n", reply->port);/*ntohs (reply->port));*/
      printf ("ip      : %s\n", util_inet_ntoa (reply->ip));
      printf ("files   : %d\n", reply->files);
      printf ("size    : %d kb\n", reply->size);
#endif
      cfg->files += reply->files;
      cfg->size += reply->size;
    } 

  return 0;
}

/*
 * This callback is called if a ping request was received. We just
 * reply with our own configuration.
 */
static int
nut_ping_request (socket_t sock, nut_header_t *hdr, void *null)
{
  nut_config_t *cfg = sock->cfg;
  nut_ping_reply_t reply;

  hdr->function = NUT_PING_ACK;
  hdr->length = sizeof (nut_ping_reply_t);
  hdr->ttl = hdr->hop;
  reply.port = cfg->port->port;
  reply.ip = sock->local_addr;
  reply.files = 0;
  reply.size = 0;
  sock_write (sock, (char *) hdr, sizeof (nut_header_t));
  sock_write (sock, (char *) &reply, hdr->length);

  return 0;
}

/*
 * This is the sock->disconnected_socket callback for gnutella 
 * connections.
 */
int
nut_disconnect (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;
  byte *id;

  /* delete all routing information for this connection */
  while ((id = (byte *) hash_contains (cfg->route, sock)) != NULL)
    hash_delete (cfg->route, (char *) id);

  /* drop all packet information for this connection */
  while ((id = (byte *) hash_contains (cfg->route, sock)) != NULL)
    hash_delete (cfg->route, (char *) id);

  /* remove this socket from the current connection hash */
  hash_delete (cfg->conn, util_itoa (sock->id));
  return 0;
}

/*
 * Whenever there is data arriving for this socket we call this 
 * routine.
 */
int
nut_check_request (socket_t sock)
{
  nut_header_t *hdr;
  void *request;
  int len = strlen (NUT_OK);
  unsigned fill = sock->recv_buffer_fill;

  /* check for self connected response */
  if (fill >= (unsigned) len && !memcmp (sock->recv_buffer, NUT_OK, len))
    {
      log_printf (LOG_NOTICE, "nut: host %s:%u connected\n",
		  util_inet_ntoa (sock->remote_addr),
		  ntohs (sock->remote_port));
      sock_reduce_recv (sock, len);
      nut_init_ping (sock);
    }

  /* go through all packets in the receive queue */
  while ((fill = sock->recv_buffer_fill) >= sizeof (nut_header_t))
    {
      hdr = (nut_header_t *) sock->recv_buffer;

      /* is there enough data to fulfill a complete packet ? */
      if (fill >= sizeof (nut_header_t) + hdr->length)
	{
	  len = sizeof (nut_header_t) + hdr->length;
	  request = sock->recv_buffer + sizeof (nut_header_t);
#if 1
	  util_hexdump (stdout, "gnutella packet", sock->sock_desc,
			sock->recv_buffer, len, 0);
#endif
	  /* try to route the packet */
	  if (nut_route (sock, hdr, request) == 0)
	    {
	      /* handle the packet */
	      switch (hdr->function)
		{
		case NUT_PING_REQ:
		  nut_ping_request (sock, hdr, NULL);
		  break;
		case NUT_PING_ACK:
		  nut_ping_reply (sock, hdr, request);
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
	    }
	  sock_reduce_recv (sock, len);
	}
      else
	break;
    }
  return 0;
}

/*
 * This routine is the sock->idle_func callback for each gnutella
 * connection. We will regularily search for specific files.
 */
int
nut_idle_searching (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;
  nut_header_t hdr;
  nut_query_t query;
  int n, len;

  if (cfg->search)
    {
      nut_calc_guid ((byte *) &hdr.id);
      hdr.function = NUT_SEARCH_REQ;
      hdr.ttl = cfg->ttl;
      hdr.hop = 0;
      hdr.length = strlen (cfg->search) + 1;
      len = sizeof (nut_query_t) - sizeof (char *);
      hdr.length += len;

      sock_write (sock, (char *) &hdr, sizeof (hdr));
      sock_write (sock, (char *) &query, len);
      sock_write (sock, cfg->search, strlen (cfg->search) + 1);

      hash_put (cfg->packet, (char *) &hdr.id, sock);
    }

  sock->idle_counter = NUT_SEARCH_INTERVAL;
  return 0;
}

/*
 * Incoming connections will be protocol checked.
 */
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

/*
 * This routine will be called when the detection routine return 
 * success.
 */
int 
nut_connect_socket (void *nut_cfg, socket_t sock)
{
  nut_config_t *cfg = nut_cfg;

  /* send the first reply */
  if (sock_printf (sock, NUT_OK) == -1)
    return -1;

  /* assign gnutella specific callbacks */
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
