/*
 * nut-core.c - gnutella core implementation
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
 * $Id: nut-core.c,v 1.10 2000/10/05 18:01:46 ela Exp $
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

#ifndef __MINGW32__
# include <sys/types.h>
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
#include "server.h"
#include "gnutella.h"
#include "nut-core.h"

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
 * This routine parses a `a.b.c.d:port' combination from the given 
 * character string ADDR and stores both of the values in IP and PORT 
 * in network byte order.
 */
int
nut_parse_addr (char *addr, unsigned long *ip, unsigned short *port)
{
  char *p, *colon, *host;

  /* create a local copy of the given address string */
  p = host = xstrdup (addr);
  if (!host)
    {
      /* address string was NULL or empty */
      return -1;
    }

  /* skip leading invalid characters */
  while (*p < '0' && *p > '9' && *p) p++;
  if (!*p) 
    {
      xfree (host);
      return -1;
    }
  
  /* find seperating colon */
  colon = p;
  while (*colon != ':' && *colon) colon++;
  if (!*colon) 
    {
      xfree (host);
      return -1;
    }

  *colon = '\0';
  colon++;

  /* convert and store both of the parsed values */
  *ip = inet_addr (p);
  *port = htons ((unsigned short) util_atoi (colon));
  xfree (host);

  return 0;
}

/*
 * This function creates a hash key for a given IP and PORT information
 * for the host catcher hash. Both values must be given in network byte
 * order.
 */
char *
nut_client_key (unsigned long ip, unsigned short port)
{
  static char key[32];

  sprintf (key, "%s:%u", util_inet_ntoa (ip), ntohs (port));
  return key;
}

/* These definitions are for the GUID creating functions in Win32. */
#ifdef __MINGW32__
CreateGuidProc CreateGuid = NULL;
HMODULE oleHandle = NULL;
#endif /* __MINGW32__ */

/*
 * This routine randomly calculates a Globally Unique Identifier (GUID)
 * and stores it in the given argument.
 */
void
nut_calc_guid (byte *guid)
{
  int n;

#ifdef __MINGW32__
  if (CreateGuid != NULL)
    {
      CreateGuid (guid);
      return;
    }
  else
#endif /* __MINGW32__ */

  for (n = 0; n < NUT_GUID_SIZE; n++)
    {
      /*guid[n] = 256 * rand () / RAND_MAX;*/
      guid[n] = (rand () >> 1) & 0xff;
    }
}

/*
 * The following routine delivers a text representation of the given
 * GUID. The format is taken from he M$ headers.
 */
char *
nut_print_guid (byte *guid)
{
  static char id[NUT_GUID_SIZE * 2 + 4];

  sprintf (id, 
	   "%02X%02X%02X%02X-"
	   "%02X%02X-"
	   "%02X%02X-"
	   "%02X%02X%02X%02X%02X%02X%02X%02X",
	   guid[0], guid[1], guid[2], guid[3],
	   guid[4], guid[5],
	   guid[6], guid[7],
	   guid[8], guid[9], guid[10], guid[11],
	   guid[12], guid[13], guid[14], guid[15]);

  return id;
}

char *
nut_text_guid (byte *guid)
{
  static char id[NUT_GUID_SIZE * 2 + 1];
  char idpart[3];
  int n;

  for (n = id[0] = 0; n < NUT_GUID_SIZE; n++)
    {
      sprintf (idpart, "%02X", guid[n]);
      strcat (id, idpart);
    }

  return id;
}

/*
 * Convert gnutella header to binary data and back.
 */
nut_header_t *
nut_get_header (byte *data)
{
  static nut_header_t hdr;
  unsigned int uint32;

  memcpy (hdr.id, data, NUT_GUID_SIZE);
  data += NUT_GUID_SIZE;
  hdr.function = *data++;
  hdr.ttl = *data++;
  hdr.hop = *data++;
  memcpy (&uint32, data, SIZEOF_UINT32);
  hdr.length = ltohl (uint32);
  return (&hdr);
}

byte *
nut_put_header (nut_header_t *hdr)
{
  static byte buffer[SIZEOF_NUT_HEADER];
  byte *data = buffer;
  unsigned int uint32;

  memcpy (data, hdr->id, NUT_GUID_SIZE);
  data += NUT_GUID_SIZE;
  *data++ = hdr->function;
  *data++ = hdr->ttl;
  *data++ = hdr->hop;
  uint32 = htoll (hdr->length);
  memcpy (data, &uint32, SIZEOF_UINT32);
  return buffer;
}

/*
 * Convert gnutella ping response to binary data and back.
 */
nut_pong_t *
nut_get_pong (byte *data)
{
  static nut_pong_t reply;
  unsigned short uint16;
  unsigned int uint32;
  
  memcpy (&uint16, data, SIZEOF_UINT16);
  reply.port = ltons (uint16);
  data += SIZEOF_UINT16;
  memcpy (&reply.ip, data, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  memcpy (&uint32, data, SIZEOF_UINT32);
  reply.files = ltohl (uint32);
  data += SIZEOF_UINT32;
  memcpy (&uint32, data, SIZEOF_UINT32);
  reply.size = ltohl (uint32);
  return (&reply);
}

byte *
nut_put_pong (nut_pong_t *reply)
{
  static byte buffer[SIZEOF_NUT_PONG];
  byte *data = buffer;
  unsigned short uint16;
  unsigned int uint32;
  
  uint16 = ntols (reply->port);
  memcpy (data, &uint16, SIZEOF_UINT16);
  data += SIZEOF_UINT16;
  memcpy (data, &reply->ip, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  uint32 = htoll (reply->files);
  memcpy (data, &uint32, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  uint32 = htoll (reply->size);
  memcpy (data, &uint32, SIZEOF_UINT32);
  return buffer;
}

/*
 * Convert gnutella search query to binary data and back.
 */
nut_query_t *
nut_get_query (byte *data)
{
  static nut_query_t query;
  unsigned short uint16;

  memcpy (&uint16, data, SIZEOF_UINT16);
  query.speed = ltohs (uint16);
  return (&query);
}

byte *
nut_put_query (nut_query_t *query)
{
  static byte buffer[SIZEOF_NUT_QUERY];
  byte *data = buffer;
  unsigned short uint16;

  uint16 = htols (query->speed);
  memcpy (data, &uint16, SIZEOF_UINT16);
  return buffer;
}

/*
 * Convert gnutella file records to binary data and back.
 */
nut_record_t *
nut_get_record (byte *data)
{
  static nut_record_t record;
  unsigned int uint32;

  memcpy (&uint32, data, SIZEOF_UINT32);
  record.index = ltohl (uint32);
  data += SIZEOF_UINT32;
  memcpy (&uint32, data, SIZEOF_UINT32);
  record.size = ltohl (uint32);
  return (&record);
}

byte *
nut_put_record (nut_record_t *record)
{
  static byte buffer[SIZEOF_NUT_RECORD];
  byte *data = buffer;
  unsigned int uint32;

  uint32 = htoll (record->index);
  memcpy (data, &uint32, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  uint32 = htoll (record->size);
  memcpy (data, &uint32, SIZEOF_UINT32);
  return buffer;
}

/*
 * Convert gnutella query hits to binary data and back.
 */
nut_reply_t *
nut_get_reply (byte *data)
{
  static nut_reply_t reply;
  unsigned short uint16;

  reply.records = *data++;
  memcpy (&uint16, data, SIZEOF_UINT16);
  reply.port = ltons (uint16);
  data += SIZEOF_UINT16;
  memcpy (&reply.ip, data, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  memcpy (&uint16, data, SIZEOF_UINT16);
  reply.speed = ltohs (uint16);
  return (&reply);
}

byte *
nut_put_reply (nut_reply_t *reply)
{
  static byte buffer[SIZEOF_NUT_REPLY];
  byte *data = buffer;
  unsigned short uint16;

  *data++ = reply->records;
  uint16 = ntols (reply->port);
  memcpy (data, &uint16, SIZEOF_UINT16);
  data += SIZEOF_UINT16;
  memcpy (data, &reply->ip, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  uint16 = htols (reply->speed);
  memcpy (data, &uint16, SIZEOF_UINT16);
  return buffer;
}

/*
 * Convert gnutella push request to binary data and back.
 */
nut_push_t *
nut_get_push (byte *data)
{
  static nut_push_t push;
  unsigned int uint32;
  unsigned short uint16;

  memcpy (push.id, data, NUT_GUID_SIZE);
  data += NUT_GUID_SIZE;
  memcpy (&uint32, data, SIZEOF_UINT32);
  push.index = ltohl (uint32);
  data += SIZEOF_UINT32;
  memcpy (&push.ip, data, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  memcpy (&uint16, data, SIZEOF_UINT16);
  push.port = ltons (uint16);
  return (&push);
}

byte *
nut_put_push (nut_push_t *push)
{
  static byte buffer[SIZEOF_NUT_PUSH];
  byte *data = buffer;
  unsigned int uint32;
  unsigned short uint16;

  memcpy (data, push->id, NUT_GUID_SIZE);
  data += NUT_GUID_SIZE;
  uint32 = htoll (push->index);
  memcpy (data, &uint32, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  memcpy (data, &push->ip, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  uint16 = ntols (push->port);
  memcpy (data, &uint16, SIZEOF_UINT16);
  return buffer;
}

/*
 * This routine parses a given gnutella (HTTP) header for certains 
 * properties and delivers either a property value which must be 
 * xfree()'d afterwards or NULL.
 */
char *
nut_parse_property (char *header, int len, char *property)
{
  char *h = header, *p = property, *value, *end = header + len;

  while (h < end)
    {
      /* find beginning of property */
      while (h < end && tolower (*h) != tolower (*p)) h++;
      if (h >= end) return NULL;

      /* compare whole property name */
      header = h;
      while (h < end && *p && tolower (*h++) == tolower (*p++));
      if (h >= end) return NULL;
      if (*p)
	{
	  /* fallback to property's first character */
	  h = header + 1;
	  p = property;
	  continue;
	}
      
      /* parse property value */
      while (h < end && *h++ == ' ');
      if (h >= end || *(h-1) != ':') return NULL;
      while (h < end && *h == ' ') h++;
      header = h;
      while (h < end && *h != '\r' && *h != '\n') h++;
      if (h >= end || h == header) return NULL;

      /* copy property value */
      len = h - header;
      value = xmalloc (len + 1);
      memcpy (value, header, len);
      value[len] = '\0';
      return value;
    }
  return NULL;
}

#else /* ENABLE_GNUTELLA */

int nut_core_dummy; /* Shut compiler warnings up. */

#endif /* not ENABLE_GNUTELLA */
