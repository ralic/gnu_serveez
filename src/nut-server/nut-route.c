/*
 * nut-route.c - gnutella routing table implementation
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
 * $Id: nut-route.c,v 1.2 2000/09/02 19:33:01 ela Exp $
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
#include <ctype.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "alloc.h"
#include "util.h"
#include "socket.h"
#include "server.h"
#include "server-core.h"
#include "gnutella.h"
#include "nut-route.h"

/*
 * This function canonizes gnutella queries. Thus we prevent the network
 * from unpatient users and often repeated queries.
 */
int
nut_canonize_query (nut_config_t *cfg, char *query)
{
  char *buffer, *p, *save;
  time_t t;
  int ret = 0;

  /* extract alphanumerics only and pack them together as lowercase */
  save = p = buffer = xstrdup (query);
  while (*p)
    {
      if (isalnum (*p)) *save++ = isupper (*p) ? tolower (*p) : *p;
      p++;
    }
  *save = '\0';
  /*
  if ((t = (time_t) hash_get (cfg->query, save)) != 0)
    {
      if (time (NULL) - t < 10)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: dropping too recent query\n");
#endif
	  ret = -1;
	}
    }

  t = time (NULL);
  hash_put (cfg->query, save, t);
  */
  xfree (buffer);

  return ret;
}

/*
 * Gnutella packet validation. This is absolutely necessary. It protects 
 * the local client as well as offer an additional line of defense against 
 * broadcasting spam to other connections. Return values:
 *  1 = packet ok
 *  0 = packet is dropped, but can be processed
 * -1 = invalid packet, do not process at all
 */
int
nut_validate_packet (socket_t sock, nut_header_t *hdr)
{
  nut_config_t *cfg = sock->cfg;
  nut_client_t *client = sock->data;

#if 1
  int n;
  char id[NUT_GUID_SIZE*2+1] = "";
  fprintf (stdout, "validating packet 0x%02X (", hdr->function);
  for (n = 0; n < NUT_GUID_SIZE; n++)
    fprintf (stdout, "%02X", hdr->id[n]);
  fprintf (stdout, ")\n");
#endif /* 1 */

  /* Packet size validation */
  switch (hdr->function)
    {
      /* Ping */
    case 0x00:
      if (hdr->length != 0)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: invalid ping payload\n");
#endif
	  return -1;
	}
      break;
      /* Pong */
    case 0x01:
      if (hdr->length != sizeof (nut_ping_reply_t))
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: invalid pong payload\n");
#endif
	  return -1;
	}
      break;
      /* Push Request */
    case 0x40:
      if (hdr->length != sizeof (nut_push_t))
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: invalid push request payload\n");
#endif
	  return -1;
	}
      break;
      /* Query */
    case 0x80:
      if (hdr->length > 257)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: payload of query too big\n");
#endif
	  return -1;
	}
      break;
      /* Query hits */
    case 0x81:
      if (hdr->length > (sizeof (nut_record_t) + 256) * 256)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: payload of query hits too big\n");
#endif
	  return -1;
	}
      break;
      /* Invalid */
    default:
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: invalid request 0x%02X\n", hdr->function);
#endif
      if (client->invalid++ > 20)
	sock_schedule_for_shutdown (sock);
      return -1;
      break;
    }

  /* Hops and TTLs */
  if (hdr->ttl <= 1)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: packet died (zero TTL)\n",
		  hdr->function);
#endif
      return 0;
    }

  hdr->hop++;
  hdr->ttl--;

  if (hdr->hop > cfg->max_ttl)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: packet died (HOP > MaxTTL)\n",
		  hdr->function); 
#endif
      return 0;
    }

  if (hdr->ttl > 50)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: packet dropped (TTL > 50)\n");
#endif
      return 0;
    }

  if (hdr->ttl > cfg->max_ttl)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: decreasing packet TTL (%d -> %d)\n",
		  hdr->ttl, cfg->max_ttl);
#endif
      hdr->ttl = cfg->max_ttl;
    }

  if (hdr->ttl + hdr->hop > cfg->max_ttl)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: decreasing packet TTL (%d -> %d)\n",
		  hdr->ttl, cfg->max_ttl);
#endif
      hdr->ttl = cfg->max_ttl - hdr->hop;
    }

  return 1;
}

/*
 * This is the routing routine for any incoming gnutella packet.
 * It return non-zero on routing errors and packet death. Otherwise
 * zero.
 */
int
nut_route (socket_t sock, nut_header_t *hdr, void *packet)
{
  nut_config_t *cfg = sock->cfg;
  nut_packet_t *pkt;
  socket_t xsock;
  socket_t *conn;
  int n;

  /* packet validation */
  n = nut_validate_packet (sock, hdr);
  if (n == -1)
    return -1;
  else if (n == 0)
    return 0;

  /* route replies here */
  if (hdr->function & 0x01)
    {
      /* is the GUID in the routing hash ? */
      xsock = (socket_t) hash_get (cfg->route, (char *) hdr->id);
      if (xsock == NULL)
	{
	  pkt = (nut_packet_t *) hash_get (cfg->packet, (char *) hdr->id);
	  if (pkt == NULL)
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
	  /* try sending the header */
	  if (sock_write (xsock, (char *) hdr, sizeof (nut_header_t)) == -1)
	    {
	      sock_schedule_for_shutdown (xsock);
	      return 0;
	    }
	  /* send the packet body if necessary */
	  if (hdr->length)
	    {
	      if (sock_write (xsock, packet, hdr->length) == -1)
		{
		  sock_schedule_for_shutdown (xsock);
		  return 0;
		}
	    }
	}
    }
  /* route queries here */
  else
    {
      /* check if this query has been seen already */
      xsock = (socket_t) hash_get (cfg->route, (char *) hdr->id);
      if (xsock != NULL)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: dropping duplicate packet 0x%02X\n",
		      hdr->function);
#endif
	  return -1;
	}

      /* check if this query has been sent by ourselves */
      pkt = (nut_packet_t *) hash_get (cfg->packet, (char *) hdr->id);
      if (pkt != NULL)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: dropping native packet 0x%02X\n",
		      hdr->function);
#endif
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
	      if (sock_write (xsock, (char *) hdr, 
			      sizeof (nut_header_t)) == -1)
		{
		  sock_schedule_for_shutdown (xsock);
		  return 0;
		}
	      if (hdr->length)
		{
		  if (sock_write (xsock, packet, hdr->length) == -1)
		    {
		      sock_schedule_for_shutdown (xsock);
		      return 0;
		    }
		}
	    }
	  hash_xfree (conn);
	}
    }
  return 0;
}

#else /* ENABLE_GNUTELLA */

int nut_route_dummy; /* Shut compiler warnings up. */

#endif /* not ENABLE_GNUTELLA */
