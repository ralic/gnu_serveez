/*
 * nut-request.c - gnutella requests implementations
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
 * $Id: nut-request.c,v 1.10 2001/04/28 12:37:06 ela Exp $
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

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#include "libserveez.h"
#include "gnutella.h"
#include "nut-core.h"
#include "nut-transfer.h"
#include "nut-hostlist.h"
#include "nut-request.h"

/*
 * This routine will be called when a search reply occurs. Here we
 * can check if the reply was created by a packet we sent ourselves.
 */
int
nut_reply (socket_t sock, nut_header_t *hdr, byte *packet)
{
  nut_config_t *cfg = sock->cfg;
  socket_t xsock;
  nut_packet_t *pkt;
  nut_record_t *record;
  nut_client_t *client = sock->data;
  char *p, *end, *file;
  int n;
  byte *id;
  nut_reply_t *reply;

  reply = nut_get_reply (packet);
  nut_host_catcher (sock, reply->ip, reply->port);
  pkt = (nut_packet_t *) svz_hash_get (cfg->packet, (char *) hdr->id);

  /* check client guid at the end of the packet */
  id = packet + hdr->length - NUT_GUID_SIZE;
  if (id < packet + SIZEOF_NUT_REPLY)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: dropping invalid query hit\n");
#endif
      client->dropped++;
      return -1;
    }

  /* is that query hit (reply) an answer to my own request ? */
  if (pkt != NULL)
    {
      xsock = pkt->sock;
      p = (char *) packet + SIZEOF_NUT_REPLY;
      end = (char *) packet + hdr->length - NUT_GUID_SIZE;
      memcpy (reply->id, id, NUT_GUID_SIZE);

#if 0
      printf ("records : %d\n", reply->records);
      printf ("port    : %u\n", ntohs (reply->port));
      printf ("ip      : %s\n", svz_inet_ntoa (reply->ip));
      printf ("speed   : %u kbit/s\n", reply->speed);
      printf ("guid    : %s\n", nut_print_guid (reply->id));
#endif /* 0 */

      /* process only if the connection has a minimum speed */
      if (reply->speed < cfg->min_speed)
	return 0;

      /* go through all query hit records */
      for (n = 0; n < reply->records && p < end; n++)
	{
	  record = nut_get_record ((byte *) p);
	  p += SIZEOF_NUT_RECORD;
	  file = p;

	  /* check if the reply is valid */
	  while (p < end && *p)
	    p++;
	  if (p == end || *(p + 1))
	    {
#if ENABLE_DEBUG
	      log_printf (LOG_DEBUG, "nut: invalid query hit payload\n");
#endif
	      client->dropped++;
	      return -1;
	    }
	  p += 2;
	  nut_canonize_file (file);
#if 0
	  printf ("record %d\n", n + 1);
	  printf ("file index : %u\n", record->index);
	  printf ("file size  : %u\n", record->size);
	  printf ("file       : %s\n", file);
#endif

	  /* startup transfer if possible */
	  if (cfg->dnloads < cfg->max_dnloads)
	    {
	      nut_init_transfer (sock, reply, record, file);
	    }
	}
    }
  /* save the reply id to the reply hash for routing push requests */
  else
    {
      svz_hash_put (cfg->reply, (char *) id, sock);
    }

  return 0;
}

/*
 * This is the callback for push requests.
 */
int
nut_push_request (socket_t sock, nut_header_t *hdr, byte *packet)
{
  nut_config_t *cfg = sock->cfg;
  nut_client_t *client = sock->data;
  socket_t xsock;
  nut_push_t *push;
  nut_file_t *entry;
  byte *header;

  push = nut_get_push (packet);

  /* is the guid of this push request in the reply hash ? */
  if ((xsock = (socket_t) svz_hash_get (cfg->reply, (char *) push->id)) != NULL)
    {
      header = nut_put_header (hdr);
      if (sock_write (xsock, (char *) header, SIZEOF_NUT_HEADER) == -1 ||
	  sock_write (xsock, (char *) packet, SIZEOF_NUT_PUSH) == -1)
	{
	  sock_schedule_for_shutdown (xsock);
	  return -1;
	}
    }
  /* push request for ourselves ? */
  else if (!memcmp (cfg->guid, push->id, NUT_GUID_SIZE))
    {
#if 0
      printf ("push request for us\n"
	      "file index : %u\n", push->index);
      printf ("ip         : %s\n", svz_inet_ntoa (push->ip));
      printf ("port       : %u\n", htons (push->port));
#endif

      /* find requested file in database */
      if (cfg->uploads <= cfg->max_uploads &&
	  (entry = nut_get_database (cfg, NULL, push->index)) != NULL)
	{
	  /* try to connect to given host */
	  if ((xsock = tcp_connect (push->ip, push->port)) != NULL)
	    {
	      log_printf (LOG_NOTICE, "nut: connecting %s:%u\n",
			  svz_inet_ntoa (push->ip), ntohs (push->port));

	      xsock->userflags |= NUT_FLAG_UPLOAD;
	      xsock->cfg = cfg;

	      /* 
	       * we are not sure about the format of this line, but two
	       * of the reviewed clients (gtk_gnutella and gnutella itself)
	       * use it as is
	       */
	      if (sock_printf (xsock, NUT_GIVE "%d:%s/%s\n\n",
			       entry->index, nut_text_guid (cfg->guid),
			       entry->file) == -1)
		{
		  sock_schedule_for_shutdown (xsock);
		  return -1;
		}
	      xsock->check_request = nut_check_upload;
	      xsock->idle_func = nut_connect_timeout;
	      xsock->idle_counter = NUT_CONNECT_TIMEOUT;
	    }
	}
    }
  /* drop this push request */
  else
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: dropping push request\n");
#endif
      client->dropped++;
      return -1;
    }

  return 0;
}

/*
 * This is called whenever there was a search query received.
 */
int
nut_query (socket_t sock, nut_header_t *hdr, byte *packet)
{
  nut_config_t *cfg = sock->cfg;
  nut_reply_t reply;
  nut_record_t record;
  nut_query_t *query;
  nut_file_t *entry;
  byte *file, *p, *buffer = NULL;
  unsigned n, len = 0, size;

  /* shall we reply to this query ? */
  query = nut_get_query (packet);
  if (query->speed > cfg->speed)
    return -1;

  /* check validity of search request */
  p = file = packet + SIZEOF_NUT_QUERY;
  len = SIZEOF_NUT_QUERY;
  while (*p++ && len < hdr->length)
    len++;
  if (len >= hdr->length && *file)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: invalid query payload\n");
#endif
      return -1;
    }

  /* create new gnutella header */
  hdr->function = NUT_SEARCH_ACK;
  hdr->ttl = hdr->hop;
  hdr->hop = 0;
  
  /* go through database and build the record array */
  for (size = 0, n = 0, entry = NULL; n < 256 && (int) n < cfg->search_limit;)
    {
      if ((entry = nut_find_database (cfg, entry, (char *) file)) != NULL)
	{
	  len = strlen (entry->file) + 2;
	  size += SIZEOF_NUT_RECORD + len;
	  buffer = svz_realloc (buffer, size);
	  p = buffer + size - len;
	  memcpy (p, entry->file, len - 1);
	  p += len - 1;
	  *p = '\0';

	  p = buffer + size - len - SIZEOF_NUT_RECORD;
	  record.index = entry->index;
	  record.size = entry->size;
	  memcpy (p, nut_put_record (&record), SIZEOF_NUT_RECORD);

	  n++;
	}
      else
	break;
    }

  /* no files found in database */
  if (!n)
    return 0;

  /* create gnutella search reply packet */
  reply.records = (byte) n;
  reply.ip = cfg->ip ? cfg->ip : sock->local_addr;
  reply.port = (unsigned short) (cfg->port ? cfg->port : 
				 0/*FIXME: htons (cfg->netport->port)*/);
  reply.speed = (unsigned short) cfg->speed;
  
  /* save packet length */
  hdr->length = SIZEOF_NUT_REPLY + size + NUT_GUID_SIZE;
  
  /* send header, reply, array of records and guid */
  if (sock_write (sock, (char *) nut_put_header (hdr), 
		  SIZEOF_NUT_HEADER) == -1 ||
      sock_write (sock, (char *) nut_put_reply (&reply), 
		  SIZEOF_NUT_REPLY) == -1 ||
      sock_write (sock, (char *) buffer, size) == -1 ||
      sock_write (sock, (char *) cfg->guid, NUT_GUID_SIZE) == -1)
    {
      svz_free (buffer);
      return -1;
    }

  svz_free (buffer);
  return 0;
}

/*
 * This routine will be called when some gnutella server sent a
 * ping reply.
 */
int
nut_pong (socket_t sock, nut_header_t *hdr, byte *packet)
{
  nut_config_t *cfg = sock->cfg;
  socket_t xsock;
  nut_packet_t *pkt;
  nut_pong_t *reply;
  nut_client_t *client = sock->data;

  /* put to host catcher hash */
  reply = nut_get_pong (packet);
  nut_host_catcher (sock, reply->ip, reply->port);
  pkt = (nut_packet_t *) svz_hash_get (cfg->packet, (char *) hdr->id);

  /* is this a reply to my own gnutella packet ? */
  if (pkt != NULL)
    {
      xsock = pkt->sock;
#if 0
      printf ("port    : %u\n", ntohs (reply->port));
      printf ("ip      : %s\n", svz_inet_ntoa (reply->ip));
      printf ("files   : %u\n", reply->files);
      printf ("size    : %u kb\n", reply->size);
#endif
      /* update statistics */
      cfg->nodes++;
      client->nodes++;
      if (reply->files && reply->size)
	{
	  cfg->files += reply->files;
	  cfg->size += reply->size;
	  client->files += reply->files;
	  client->size += reply->size;
	}
    } 

  return 0;
}

/*
 * This callback is called if a ping request was received. We just
 * reply with our own configuration.
 */
int
nut_ping (socket_t sock, nut_header_t *hdr, byte *null)
{
  nut_config_t *cfg = sock->cfg;
  nut_pong_t reply;
  byte *header, *pong;

  /* create new gnutella packets */
  hdr->function = NUT_PING_ACK;
  hdr->length = SIZEOF_NUT_PONG;
  hdr->ttl = hdr->hop;
  hdr->hop = 0;

  reply.ip = cfg->ip ? cfg->ip : sock->local_addr;
  reply.port = (unsigned short) (cfg->port ? cfg->port : 
				 0 /*FIXME: htons (cfg->netport->port)*/);
  reply.files = cfg->db_files;
  reply.size = cfg->db_size / 1024;
  header = nut_put_header (hdr);
  pong = nut_put_pong (&reply);
  
  /* try sending this packet */
  if (sock_write (sock, (char *) header, SIZEOF_NUT_HEADER) == -1 ||
      sock_write (sock, (char *) pong, SIZEOF_NUT_PONG) == -1)
    {
      sock_schedule_for_shutdown (sock);
      return -1;
    }

  return 0;
}

#else /* ENABLE_GNUTELLA */

int nut_request_dummy; /* Shut compiler warnings up. */

#endif /* not ENABLE_GNUTELLA */
