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
 * $Id: gnutella.c,v 1.15 2000/09/09 16:33:43 ela Exp $
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
#include <sys/stat.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
# include <io.h>
#endif

#if HAVE_DIRECT_H
# include <direct.h>
# ifdef __MINGW32__
#  define mkdir(path, mode) mkdir (path)
# endif
#endif

#include "alloc.h"
#include "util.h"
#include "socket.h"
#include "connect.h"
#include "server.h"
#include "server-core.h"
#include "serveez.h"
#include "gnutella.h"
#include "nut-transfer.h"
#include "nut-route.h"
#include "nut-core.h"

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
  0,           /* hosts within the connected network */
  "/tmp",      /* where to store downloaded files */
  "/tmp",      /* local search database path */
  0,           /* concurrent downloads */
  4,           /* maximum concurrent downloads */
  28,          /* connection speed (KBit/s) */
  28,          /* minimum connection speed for searching */
  NULL,        /* file extensions */
  NULL,        /* host catcher */
  4,           /* number of connections to keep up */
  NULL,        /* force the local ip to this value */
  INADDR_NONE, /* calculated from `force_ip' */
  NULL,        /* recent query hash */
  NULL,        /* reply hash for routing push requests */
  NULL,        /* shared file array */
  0,           /* number of database files */
  0,           /* size of database in KB */
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
  REGISTER_STR ("download-path", nut_config.save_path, DEFAULTABLE),
  REGISTER_STR ("share-path", nut_config.share_path, DEFAULTABLE),
  REGISTER_INT ("max-downloads", nut_config.max_dnloads, DEFAULTABLE),
  REGISTER_INT ("connection-speed", nut_config.speed, DEFAULTABLE),
  REGISTER_INT ("min-speed", nut_config.min_speed, DEFAULTABLE),
  REGISTER_STRARRAY ("file-extensions", nut_config.extensions, DEFAULTABLE),
  REGISTER_INT ("connections", nut_config.connections, DEFAULTABLE),
  REGISTER_STR ("force-ip", nut_config.force_ip, DEFAULTABLE),
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
  nut_info_client,                        /* server info callback */
  nut_info_server,                        /* client info callback */
  nut_server_timer,                       /* server timer routine */
  NULL,                                   /* no handle request callback */
  &nut_config,                            /* default configuration */
  sizeof (nut_config),                    /* size of this configuration */
  nut_config_prototype                    /* configuration items */
};

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
 * This is the default idle function for self conncted gnutella hosts.
 * It simply returns an error if the socket was not connected in a
 * certain time.
 */
int
nut_connect_timeout (socket_t sock)
{
  return -1;
}

/*
 * The following routine tries to connect to a given gnutella host.
 * It returns -1 on errors and zero otherwise.
 */
static int
nut_connect_host (nut_config_t *cfg, char *host)
{
  nut_host_t *client;
  socket_t sock;
  unsigned long ip;
  unsigned short port;
  int ret = -1;

  /* try getting ip address and port */
  if (nut_parse_addr (host, &ip, &port) == -1)
    {
      log_printf (LOG_WARNING, "nut: invalid host `%s'\n", host);
      return ret;
    }

  /* get client from host catcher hash */
  client = (nut_host_t *) hash_get (cfg->net, host);

  /* try to connect to this host */
  if ((sock = sock_connect (ip, port)) != NULL)
    {
      log_printf (LOG_NOTICE, "nut: connecting %s:%u\n",
		  util_inet_ntoa (ip), ntohs (port));
      
      sock->cfg = cfg;
      sock->flags |= SOCK_FLAG_NOFLOOD;
      sock->disconnected_socket = nut_disconnect;
      sock->check_request = nut_check_request;
      sock->idle_func = nut_connect_timeout;
      sock->idle_counter = NUT_CONNECT_TIMEOUT;
      sock_printf (sock, NUT_CONNECT);
      ret = 0;
    }

  /* 
   * If we could not connect then delete the client from host catcher 
   * hash and free the client structure.
   */
  if (client)
    {
      hash_delete (cfg->net, host);
      xfree (client);
    }
  return ret;
}

/*
 * When establishing a new connection to another gnutella server this
 * functions pings it to get all further servers behind this server.
 */
int
nut_init_ping (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;
  nut_client_t *client = sock->data;
  nut_packet_t *pkt;
  nut_header_t hdr;
  byte *header;

  /* create new gnutella header */
  nut_calc_guid (hdr.id);
  hdr.function = NUT_PING_REQ;
  hdr.ttl = cfg->ttl;
  hdr.hop = 0;
  hdr.length = 0;
  header = nut_put_header (&hdr);

  /* put into sent packet hash */
  pkt = xmalloc (sizeof (nut_packet_t));
  pkt->sock = sock;
  pkt->sent = time (NULL);
  hash_put (cfg->packet, (char *) hdr.id, pkt);

  /* update client and server statistics */
  cfg->nodes -= client->nodes;
  cfg->files -= client->files;
  cfg->size -= client->size;
  client->nodes = 0;
  client->files = 0;
  client->size = 0;

  return sock_write (sock, (char *) header, SIZEOF_NUT_HEADER);
}

/*
 * The gnutella servers global initializer.
 */
int
nut_global_init (void)
{
#ifdef __MINGW32__
  /* try getting M$'s GUID creation routine */
  if ((oleHandle = LoadLibrary ("ole32.dll")) != NULL)
    {
      CreateGuid = (CreateGuidProc) 
	GetProcAddress (oleHandle, "CoCreateGuid");
    }
#endif /* __MINGW32__ */

  /* initialize random seed */
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
  int n = 0;
  struct stat buf;
  char *p;

  /* check the download and share path first */
  if (strlen (cfg->save_path) == 0 || strlen (cfg->share_path) == 0)
    {
      log_printf (LOG_ERROR, "nut: no download/share path given\n");
      return -1;
    }
  p = cfg->save_path + strlen (cfg->save_path) - 1;
  if (*p == '/' || *p == '\\') *p = '\0';
  p = cfg->share_path + strlen (cfg->share_path) - 1;
  if (*p == '/' || *p == '\\') *p = '\0';

  /* check for existence and create them if necessary */
  if (stat (cfg->save_path, &buf) == -1)
    {
      /* create the download directory */
      if (mkdir (cfg->save_path, S_IRWXU) == -1)
	{
	  log_printf (LOG_ERROR, "nut: mkdir: %s\n", SYS_ERROR);
	  return -1;
	}
    }
  /* check if the given path is a directory already */
  else if (!S_ISDIR (buf.st_mode))
    {
      log_printf (LOG_ERROR, "nut: %s is not a directory\n", cfg->save_path);
      return -1;
    }

  /* checking for the share path only if the paths differ */
  if (strcmp (cfg->save_path, cfg->share_path))
    {
      if (stat (cfg->share_path, &buf) == -1)
	{
	  if (mkdir (cfg->share_path, S_IRWXU) == -1)
	    {
	      log_printf (LOG_ERROR, "nut: mkdir: %s\n", SYS_ERROR);
	      return -1;
	    }
	}
      else if (!S_ISDIR (buf.st_mode))
	{
	  log_printf (LOG_ERROR, "nut: %s is not a directory\n", 
		      cfg->share_path);
	  return -1;
	}
    }

  /* read sharing files */
  nut_read_database (cfg, cfg->share_path);

  /* calculate forced local ip if necessary */
  if (cfg->force_ip)
    {
      cfg->ip = inet_addr (cfg->force_ip);
    }

  /* create and modify packet hash */
  cfg->packet = hash_create (4);
  cfg->packet->code = nut_hash_code;
  cfg->packet->equals = nut_hash_equals;
  cfg->packet->keylen = nut_hash_keylen;

  /* create and modify reply hash */
  cfg->reply = hash_create (4);
  cfg->reply->code = nut_hash_code;
  cfg->reply->equals = nut_hash_equals;
  cfg->reply->keylen = nut_hash_keylen;

  /* create current connection hash */
  cfg->conn = hash_create (4);

  /* create host catcher hash */
  cfg->net = hash_create (4);

  /* create recent query hash */
  cfg->query = hash_create (4);

  /* create and modify the routing table hash */
  cfg->route = hash_create (4);
  cfg->route->code = nut_hash_code;
  cfg->route->equals = nut_hash_equals;
  cfg->route->keylen = nut_hash_keylen;

  /* calculate this server instance's GUID */
  nut_calc_guid (cfg->guid);

  /* go through all given hosts and try to connect to them */
  if (cfg->hosts)
    {
      while (cfg->hosts[n])
	{
	  nut_connect_host (cfg, cfg->hosts[n]);
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
  nut_host_t **client;
  nut_packet_t **pkt;
  int n;

  /* destroy sharing files */
  nut_destroy_database (cfg);

  hash_destroy (cfg->conn);
  hash_destroy (cfg->route);
  hash_destroy (cfg->query);
  hash_destroy (cfg->reply);

  /* destroy sent packet hash */
  if ((pkt = (nut_packet_t **) hash_values (cfg->packet)) != NULL)
    {
      for (n = 0; n < hash_size (cfg->packet); n++)
	{
	  xfree (pkt[n]);
	}
      hash_xfree (pkt);
    }
  hash_destroy (cfg->packet);

  /* destroy hast catcher hash */
  if ((client = (nut_host_t **) hash_values (cfg->net)) != NULL)
    {
      for (n = 0; n < hash_size (cfg->net); n++)
	{
	  xfree (client[n]);
	}
      hash_xfree (client);
    }
  hash_destroy (cfg->net);

  return 0;
}

/*
 * Global gnutella finalizer.
 */
int
nut_global_finalize (void)
{
#ifdef __MINGW32__
  if (oleHandle) 
    FreeLibrary (oleHandle);
#endif /* __MINGW32__ */

  return 0;
}

/*
 * Within this routine we collect all available gnutella hosts. Thus
 * we might never ever lack of gnutella net connections. IP and PORT
 * must be both in network byte order.
 */
static int
nut_host_catcher (nut_config_t *cfg, unsigned long ip, unsigned short port)
{
  nut_host_t *client;

  client = (nut_host_t *) hash_get (cfg->net, nut_client_key (ip, port));

  /* not yet in host catcher hash */
  if (client == NULL)
    {
      /* check if it is a valid ip/host combination */
      if ((ip & 0xff000000) == 0 || (ip & 0x00ff0000) == 0 ||
	  (ip & 0x0000ff00) == 0 || (ip & 0x000000ff) == 0 ||
	  (ip & 0xff000000) == 0xff000000 || (ip & 0x00ff0000) == 0x00ff0000 ||
	  (ip & 0x0000ff00) == 0x0000ff00 || (ip & 0x000000ff) == 0x000000ff ||
	  port == 0)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: invalid host: %s:%u\n", 
		      util_inet_ntoa (ip), ntohs (port));
#endif
	  return -1;
	}

      client = xmalloc (sizeof (nut_host_t));
      client->last_reply = time (NULL);
      client->ip = ip;
      client->port = port;
      memset (client->id, 0, NUT_GUID_SIZE);
      hash_put (cfg->net, nut_client_key (ip, port), client);
    }
  
  /* just update last seen time stamp */
  else
    {
      client->last_reply = time (NULL);
    }
  return 0;
}

/*
 * This routine will be called when a search reply occurs. Here we
 * can check if the reply was created by a packet we sent ourselves.
 */
static int
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
  nut_host_catcher (cfg, reply->ip, reply->port);
  pkt = (nut_packet_t *) hash_get (cfg->packet, (char *) hdr->id);

  /* is that query hit (reply) an answer to my own request ? */
  if (pkt != NULL)
    {
      xsock = pkt->sock;
#if 0
      printf ("records : %d\n", reply->records);
      printf ("port    : %u\n", ntohs (reply->port));
      printf ("ip      : %s\n", util_inet_ntoa (reply->ip));
      printf ("speed   : %u kbit/s\n", reply->speed);
#endif

      /* process only if the connection has a minimum speed */
      if (reply->speed >= cfg->min_speed)
	{
	  p = (char *) packet + SIZEOF_NUT_REPLY;
	  end = p + hdr->length;

	  /* go through all query hit records */
	  for (n = 0; n < reply->records && p < end; n++)
	    {
	      record = nut_get_record ((byte *) p);
	      p += SIZEOF_NUT_RECORD;
	      file = p;

	      /* check if the reply is valid */
	      while (p < end && *p) p++;
	      if (p == end || *(p+1))
		{
#if ENABLE_DEBUG
		  log_printf (LOG_DEBUG, "nut: invalid query hit payload\n");
#endif
		  client->dropped++;
		  return -1;
		}
	      p += 2;
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
	  id = (byte *) p;
#if 0
	  printf ("guid    : %s\n", nut_print_guid (id));
#endif
	}
    }
  /* save the reply id to the reply hash for routing push requests */
  else
    {
      id = packet + hdr->length - NUT_GUID_SIZE;
      if (id < packet + SIZEOF_NUT_REPLY)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: dropping invalid query hit\n");
#endif
	  client->dropped++;
	  return -1;
	}
      hash_put (cfg->reply, (char *) id, sock);
    }

  return 0;
}

/*
 * This is the callback for push requests.
 */
static int
nut_push_request (socket_t sock, nut_header_t *hdr, byte *packet)
{
  nut_config_t *cfg = sock->cfg;
  nut_client_t *client = sock->data;
  socket_t xsock;
  nut_push_t *push;
  byte *header;

  push = nut_get_push (packet);

  /* is the guid of this push request in the reply hash ? */
  if ((xsock = (socket_t) hash_get (cfg->reply, (char *) push->id)) != NULL)
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
#if 1
      printf ("push request for us\n"
	      "file index : %u\n", push->index);
      printf ("ip         : %s\n", util_inet_ntoa (push->ip));
      printf ("port       : %u\n", htons (push->port));
#endif
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
static int
nut_query (socket_t sock, nut_header_t *hdr, byte *packet)
{
  nut_config_t *cfg = sock->cfg;
  nut_client_t *client = sock->data;
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
  while (*p++ && len < hdr->length) len++;
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
  for (size = 0, n = 0, entry = NULL; n < 256; )
    {
      if ((entry = nut_find_database (cfg, entry, (char *) file)) != NULL)
	{
	  len = strlen (entry->file) + 2;
	  size += SIZEOF_NUT_RECORD + len;
	  buffer = xrealloc (buffer, size);
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
      else break;
    }

  /* no files found in database */
  if (!n) return 0;

  /* create gnutella search reply packet */
  reply.records = n;
  reply.port = htons (cfg->port->port);
  reply.ip = cfg->ip != INADDR_NONE ? cfg->ip : sock->local_addr;
  reply.speed = cfg->speed;
  
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
      xfree (buffer);
      return -1;
    }

  xfree (buffer);
  return 0;
}

/*
 * This routine will be called when some gnutella server sent a
 * ping reply.
 */
static int
nut_ping_reply (socket_t sock, nut_header_t *hdr, byte *packet)
{
  nut_config_t *cfg = sock->cfg;
  socket_t xsock;
  nut_packet_t *pkt;
  nut_ping_reply_t *reply;
  nut_client_t *client = sock->data;

  /* put to host catcher hash */
  reply = nut_get_ping_reply (packet);
  nut_host_catcher (cfg, reply->ip, reply->port);
  pkt = (nut_packet_t *) hash_get (cfg->packet, (char *) hdr->id);

  /* is this a reply to my own gnutella packet ? */
  if (pkt != NULL)
    {
      xsock = pkt->sock;
#if 0
      printf ("port    : %u\n", ntohs (reply->port));
      printf ("ip      : %s\n", util_inet_ntoa (reply->ip));
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
static int
nut_ping_request (socket_t sock, nut_header_t *hdr, byte *null)
{
  nut_config_t *cfg = sock->cfg;
  nut_ping_reply_t reply;
  byte *header, *pong;

  /* create new gnutella packets */
  hdr->function = NUT_PING_ACK;
  hdr->length = SIZEOF_NUT_PING_REPLY;
  hdr->ttl = hdr->hop;
  hdr->hop = 0;

  reply.port = htons (cfg->port->port);
  if (cfg->ip != INADDR_NONE) reply.ip = cfg->ip;
  else                        reply.ip = sock->local_addr;
  reply.files = cfg->db_files;
  reply.size = cfg->db_size / 1024;
  header = nut_put_header (hdr);
  pong = nut_put_ping_reply (&reply);
  
  /* try sending this packet */
  if (sock_write (sock, (char *) header, SIZEOF_NUT_HEADER) == -1 ||
      sock_write (sock, (char *) pong, SIZEOF_NUT_PING_REPLY) == -1)
    {
      sock_schedule_for_shutdown (sock);
      return -1;
    }

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
  nut_host_t *host;
  byte *id;
  char *key, **keys;
  int size, n;
  nut_packet_t *pkt;
  nut_client_t *client = sock->data;

  /* delete all push request routing information for this connection */
  while ((id = (byte *) hash_contains (cfg->reply, sock)) != NULL)
    hash_delete (cfg->reply, (char *) id);

  /* delete all routing information for this connection */
  while ((id = (byte *) hash_contains (cfg->route, sock)) != NULL)
    hash_delete (cfg->route, (char *) id);

  /* drop all packet information for this connection */
  if ((keys = (char **) hash_keys (cfg->packet)) != NULL)
    {
      size = hash_size (cfg->packet);
      for (n = 0; n < size; n++)
	{
	  pkt = (nut_packet_t *) hash_get (cfg->packet, keys[n]);
	  if (pkt->sock == sock)
	    {
	      hash_delete (cfg->packet, keys[n]);
	      xfree (pkt);
	    }
	}
      hash_xfree (keys);
    }
  
  /* remove this socket from the current connection hash */
  key = nut_client_key (sock->remote_addr, sock->remote_port);
  hash_delete (cfg->conn, key);

  /* remove the connection from the host catcher */
  if ((host = hash_delete (cfg->net, key)) != NULL)
    xfree (host);

  /* free client structure */
  if (client)
    {
      cfg->nodes -= client->nodes;
      cfg->files -= client->files;
      cfg->size -= client->size;
      xfree (client);
      sock->data = NULL;
    }

  return 0;
}

/*
 * This callback is regularily called in the `handle_periodic_tasks'
 * routine. Here we try connecting to more gnutella hosts.
 */
int
nut_server_timer (server_t *server)
{
  nut_config_t *cfg = server->cfg;
  static int count = NUT_CONNECT_INTERVAL;
  char **keys;
  nut_packet_t *pkt;
  int n, size, connect;
  time_t t, received;

  /* go sleep if we still do not want to do something */
  if (count-- > 0) return 0;
    
  /* do we have enough connections ? */
  connect = cfg->connections - hash_size (cfg->conn);
  if (connect > 0)
    {
      /* are there hosts in the host catcher hash ? */
      if ((keys = (char **) hash_keys (cfg->net)) != NULL)
	{
	  /* go through all caught hosts */
	  for (n = 0; n < hash_size (cfg->net) && connect; n++)
	    {
	      /* check if we are not already connected */
	      if (hash_get (cfg->conn, keys[n]) == NULL)
		{
		  if (nut_connect_host (cfg, keys[n]) != -1)
		    connect--;
		}
	    }
	  hash_xfree (keys);
	}
    }

  /* go through the sent packet hash and drop old entries */
  if ((keys = (char **) hash_keys (cfg->packet)) != NULL)
    {
      t = time (NULL);
      size = hash_size (cfg->packet);
      for (n = 0; n < size; n++)
	{
	  pkt = (nut_packet_t *) hash_get (cfg->packet, keys[n]);
	  if (t - pkt->sent > NUT_ENTRY_AGE)
	    {
	      hash_delete (cfg->packet, keys[n]);
	      xfree (pkt);
	    }
	}
      hash_xfree (keys);
    }

  /* drop older entries from the recent query hash */
  if ((keys = (char **) hash_keys (cfg->query)) != NULL)
    {
      t = time (NULL);
      size = hash_size (cfg->query);
      for (n = 0; n < size; n++)
	{
	  received = (time_t) hash_get (cfg->query, keys[n]);
	  if (t - received > NUT_ENTRY_AGE)
	    {
	      hash_delete (cfg->query, keys[n]);
	    }
	}
      hash_xfree (keys);
    }

  /* wake up in a certain time */
  count = NUT_CONNECT_INTERVAL;
  return 0;
}

/*
 * Gnutella client structure creator.
 */
nut_client_t *
nut_create_client (void)
{
  nut_client_t *client;

  client = xmalloc (sizeof (nut_client_t));
  memset (client, 0, sizeof (nut_client_t));
  return client;
}

/*
 * Whenever there is data arriving for this socket we call this 
 * routine.
 */
int
nut_check_request (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;
  nut_client_t *client;
  nut_header_t *hdr;
  byte *packet;
  int len = strlen (NUT_OK);
  unsigned fill = sock->recv_buffer_fill;

  /* check for self connected response */
  if (fill >= (unsigned) len && !memcmp (sock->recv_buffer, NUT_OK, len))
    {
      /* add this client to the current connection hash */
      hash_put (cfg->conn, 
		nut_client_key (sock->remote_addr, sock->remote_port), 
		sock);

      sock->userflags |= NUT_FLAG_CLIENT;
      log_printf (LOG_NOTICE, "nut: host %s:%u connected\n",
		  util_inet_ntoa (sock->remote_addr),
		  ntohs (sock->remote_port));
      sock_reduce_recv (sock, len);

      sock->data = nut_create_client ();
      sock->idle_func = nut_idle_searching;
      sock->idle_counter = NUT_SEARCH_INTERVAL;

      if (nut_init_ping (sock) == -1) 
	return -1;
    }

  /* go through all packets in the receive queue */
  while ((fill = sock->recv_buffer_fill) >= SIZEOF_NUT_HEADER)
    {
      client = sock->data;
      hdr = nut_get_header ((byte *) sock->recv_buffer);

      /* is there enough data to fulfill a complete packet ? */
      if (fill >= SIZEOF_NUT_HEADER + hdr->length)
	{
	  len = SIZEOF_NUT_HEADER + hdr->length;
	  packet = (byte *) sock->recv_buffer + SIZEOF_NUT_HEADER;
	  client->packets++;
#if 0
	  util_hexdump (stdout, "gnutella packet", sock->sock_desc,
			sock->recv_buffer, len, 0);
#endif
	  
	  /* try to route the packet */
	  if (nut_route (sock, hdr, packet) == 0)
	    {
	      /* handle the packet */
	      switch (hdr->function)
		{
		case NUT_PING_REQ:
		  nut_ping_request (sock, hdr, NULL);
		  break;
		case NUT_PING_ACK:
		  nut_ping_reply (sock, hdr, packet);
		  break;
		case NUT_PUSH_REQ:
		  nut_push_request (sock, hdr, packet);
		  break;
		case NUT_SEARCH_REQ:
		  nut_query (sock, hdr, packet);
		  break;
		case NUT_SEARCH_ACK:
		  nut_reply (sock, hdr, packet);
		  break;
		}
	    }
	  else
	    client->dropped++;

	  /* cut this packet from the send buffer queue */
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
  nut_packet_t *pkt;
  nut_header_t hdr;
  nut_query_t query;
  byte *header, *search;

  /* search string given ? */
  if (cfg->search)
    {
      /* create new gnutella packet */
      nut_calc_guid (hdr.id);
      hdr.function = NUT_SEARCH_REQ;
      hdr.ttl = cfg->ttl;
      hdr.hop = 0;
      hdr.length = SIZEOF_NUT_QUERY + strlen (cfg->search) + 1;
      query.speed = cfg->min_speed;
      header = nut_put_header (&hdr);
      search = nut_put_query (&query);

      /* try sending this packet to this connection */
      if (sock_write (sock, (char *) header, SIZEOF_NUT_HEADER) == -1 ||
	  sock_write (sock, (char *) search, SIZEOF_NUT_QUERY) == -1 ||
	  sock_write (sock, cfg->search, strlen (cfg->search) + 1) == -1)
	{
	  return -1;
	}
      
      /* save this packet for later routing */
      pkt = xmalloc (sizeof (nut_packet_t));
      pkt->sock = sock;
      pkt->sent = time (NULL);
      hash_put (cfg->packet, (char *) hdr.id, pkt);
    }

  /* wake up in a certain time */
  sock->idle_counter = NUT_SEARCH_INTERVAL;
  return 0;
}

/*
 * This routine is the write_socket callback when delivering the 
 * host catcher list. It just waits until the whole HTML has been
 * successfully sent and closes the connection afterwards.
 */
static int
nut_hosts_write (socket_t sock)
{
  int num_written;

  /* write as much data as possible */
  num_written = send (sock->sock_desc, sock->send_buffer,
                      sock->send_buffer_fill, 0);

  /* some data has been written */
  if (num_written > 0)
    {
      sock->last_send = time (NULL);

      /* reduce send buffer */
      if (sock->send_buffer_fill > num_written)
        {
          memmove (sock->send_buffer, 
                   sock->send_buffer + num_written,
                   sock->send_buffer_fill - num_written);
        }
      sock->send_buffer_fill -= num_written;
    }
  /* seems like an error */
  else if (num_written < 0)
    {
      log_printf (LOG_ERROR, "nut: write: %s\n", NET_ERROR);
      if (last_errno == SOCK_UNAVAILABLE)
        {
          sock->unavailable = time (NULL) + RELAX_FD_TIME;
          num_written = 0;
        }
    }
  
  /* has all data been sent successfully ? */
  if (sock->send_buffer_fill <= 0 && !(sock->userflags & NUT_FLAG_HOSTS))
    {
      num_written = -1;
    }

  return (num_written < 0) ? -1 : 0;
}

/*
 * This is the check_request callback for the HTML host list output.
 */
#define NUT_HTTP_HEADER "HTTP 200 OK\r\n"                           \
		        "Server: Gnutella\r\n"                      \
		        "Content-type: text/html\r\n"               \
		        "\r\n"
#define NUT_HTML_HEADER "<html><body bgcolor=white text=black><br>" \
	                "<h1>%d Gnutella Hosts</h1>"                \
	                "<hr noshade><pre>"
#define NUT_HTML_FOOTER "</pre><hr noshade>"                        \
                         "<i>%s/%s server at %s port %d</i>"        \
	                 "</body></html>"

static int
nut_hosts_check (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;
  nut_host_t **host;
  int n;

  /* do not enter this routine if you do not want to send something */
  if (!(sock->userflags & NUT_FLAG_HOSTS))
    return 0;

  /* send normal HTTP header */
  if (sock_printf (sock, NUT_HTTP_HEADER) == -1)
    return -1;

  /* send HTML header */
  if (sock_printf (sock, NUT_HTML_HEADER, hash_size (cfg->net)) == -1)
    return -1;

  /* go through all caught gnutella hosts and print their info */
  if ((host = (nut_host_t **) hash_values (cfg->net)) != NULL)
    {
      for (n = 0; n < hash_size (cfg->net); n++)
	{
	  if (sock->send_buffer_fill > (NUT_SEND_BUFSIZE - 256))
	    {
	      /* send buffer queue overrun ... */
	      if (sock_printf (sock, ".\n.\n.\n") == -1)
		return -1;
	      break;
	    }
	  else
	    {
	      /* usual gnutella host output */
	      if (sock_printf (sock, "%-32s %-20s\n",
			       nut_client_key (host[n]->ip, host[n]->port),
			       util_time (host[n]->last_reply)) == -1)
		return -1;
	    }
	}
      hash_xfree (host);
    }

  /* send HTML footer */
  if (sock_printf (sock, NUT_HTML_FOOTER,
		   serveez_config.program_name, 
		   serveez_config.version_string,
		   util_inet_ntoa (sock->local_addr), 
		   ntohs (sock->local_port)) == -1)
    return -1;

  /* state that we have sent all available data */
  sock->userflags &= ~NUT_FLAG_HOSTS;

  /* shutdown the socket if all data has been written */
  if (sock->send_buffer_fill <= 0)
    return -1;

  return 0;
}

/*
 * Gnutella server info callback.
 */
char *
nut_info_server (server_t *server)
{
  nut_config_t *cfg = server->cfg;
  static char info[80*17];
  char *ext = NULL;
  int n;

  /* create file extension list */
  n = 0;
  if (cfg->extensions)
    {
      while (cfg->extensions[n])
	{
	  if (!ext)
	    {
	      ext = xmalloc (strlen (cfg->extensions[n]) + 2);
	      strcpy (ext, cfg->extensions[n]);
	    }
	  else
	    {
	      ext = xrealloc (ext, strlen (ext) + 
			      strlen (cfg->extensions[n]) + 2);
	      strcat (ext, cfg->extensions[n]);
	    }
	  n++;
	  strcat (ext, ";");
	}
      ext[strlen (ext) - 1] = '\0';
    }

  sprintf (info,
	   " tcp port        : %u\r\n"
	   " ip              : %s\r\n"
	   " maximum ttl     : %u\r\n"
	   " default ttl     : %u\r\n"
	   " speed           : %u KBit/s\r\n"
	   " clientID128     : %s\r\n"
	   " download path   : %s\r\n"
	   " share path      : %s\r\n"
	   " search pattern  : %s\r\n"
	   " file extensions : %s\r\n"
	   " routing table   : %u entries\r\n"
	   " connected hosts : %u/%u\r\n"
	   " sent packets    : %u\r\n"
	   " routing errors  : %u\r\n"
	   " hosts           : %u gnutella clients seen\r\n"
	   " data pool       : %u MB in %u files on %u hosts\r\n"
	   " downloads       : %u/%u\r\n"
	   " queries         : %u",
	   cfg->port->port,
	   cfg->ip != INADDR_NONE ? util_inet_ntoa (cfg->ip) : "no specified",
	   cfg->max_ttl,
	   cfg->ttl,
	   cfg->speed,
	   nut_print_guid (cfg->guid),
	   cfg->save_path,
	   cfg->share_path,
	   cfg->search,
	   ext ? ext : "no extensions",
	   hash_size (cfg->route),
	   hash_size (cfg->conn), cfg->connections,
	   hash_size (cfg->packet),
	   cfg->errors,
	   hash_size (cfg->net),
	   cfg->size / 1024, cfg->files, cfg->nodes,
	   cfg->dnloads, cfg->max_dnloads,
	   hash_size (cfg->query));

  xfree (ext);
  return info;
}

/*
 * Gnutella client info callback.
 */
char *
nut_info_client (void *nut_cfg, socket_t sock)
{
  nut_config_t *cfg = nut_cfg;
  static char info[80*3];
  static char text[128];
  nut_transfer_t *transfer = sock->data;
  nut_client_t *client = sock->data;
  unsigned current, all;

  sprintf (info, "This is a gnutella spider client.\r\n\r\n");

  if (sock->userflags & NUT_FLAG_CLIENT)
    {
      sprintf (text, 
	       "  * ususal gnutella host\r\n"
	       "  * dropped packets : %u/%u\r\n"
	       "  * invalid packets : %u\r\n",
	       client->dropped, client->packets, client->invalid);
      strcat (info, text);
      sprintf (text, 
	       "  * data pool       : %u MB in %u files on %u hosts\r\n",
	       client->size / 1024, client->files, client->nodes);
      strcat (info, text);
    }

  if (sock->userflags & NUT_FLAG_HTTP)
    {
      current = transfer->original_size - transfer->size;
      all = transfer->original_size;
      sprintf (text, 
	       "  * download progress : %u/%u (%u.%u%%)\r\n",
	       current, all,
	       current * 100 / all, (current * 1000 / all) % 10);
      strcat (info, text);
    }

  if (sock->userflags & NUT_FLAG_HDR)
    {
      strcat (info, "  * header received\r\n");
    }

  if (sock->userflags & NUT_FLAG_HOSTS)
    {
      strcat (info, "  * sending host catcher list\r\n");
    }

  return info;
}

/*
 * Incoming connections will be protocol checked.
 */
int 
nut_detect_proto (void *nut_cfg, socket_t sock)
{
  int len = strlen (NUT_CONNECT);

  /* detect normal connect */
  len = strlen (NUT_CONNECT);
  if (sock->recv_buffer_fill >= len &&
      !memcmp (sock->recv_buffer, NUT_CONNECT, len))
    {
      sock->userflags |= NUT_FLAG_CLIENT;
      log_printf (LOG_NOTICE, "gnutella protocol detected (client)\n");
      sock_reduce_recv (sock, len);
      return -1;
    }

  /* detect host catcher request */
  len = strlen (NUT_HOSTS);
  if (sock->recv_buffer_fill >= len &&
      !memcmp (sock->recv_buffer, NUT_HOSTS, len))
    {
      sock->userflags |= NUT_FLAG_HOSTS;
      log_printf (LOG_NOTICE, "gnutella protocol detected (host list)\n");
      sock_reduce_recv (sock, len);
      return -1;
    }

  /* detect upload request */
  len = strlen (NUT_GET);
  if (sock->recv_buffer_fill >= len &&
      !memcmp (sock->recv_buffer, NUT_GET, len))
    {
      sock->userflags |= NUT_FLAG_UPLOAD;
      log_printf (LOG_NOTICE, "gnutella protocol detected (upload)\n");
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

  /* assign host catcher request routines */
  if (sock->userflags & NUT_FLAG_HOSTS)
    {
      sock->check_request = nut_hosts_check;
      sock->write_socket = nut_hosts_write;
      return 0;
    }

  /* assign upload request routines */
  if (sock->userflags & NUT_FLAG_UPLOAD)
    {
      sock->check_request = nut_check_upload;
      return 0;
    }

  /* send the first reply */
  if (sock_printf (sock, NUT_OK) == -1)
    return -1;

  /* assign gnutella specific callbacks */
  sock->flags |= SOCK_FLAG_NOFLOOD;
  sock->disconnected_socket = nut_disconnect;
  sock->check_request = nut_check_request;
  sock->idle_func = nut_idle_searching;
  sock->idle_counter = NUT_SEARCH_INTERVAL;
  sock->data = nut_create_client ();

  /* send inital ping */
  if (nut_init_ping (sock) == -1)
    return -1;

  /* put this client to the current connection hash */
  hash_put (cfg->conn, 
	    nut_client_key (sock->remote_addr, sock->remote_port), 
	    sock);

  return 0;
}

int have_gnutella = 1;

#else /* ENABLE_GNUTELLA */

int have_gnutella = 0;	/* Shut compiler warnings up, remember for runtime */

#endif /* not ENABLE_GNUTELLA */
