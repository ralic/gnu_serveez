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
 * $Id: nut-core.c,v 1.1 2000/09/03 21:28:05 ela Exp $
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
 * Convert gnutella header to binary data and back,
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

void
nut_put_header (nut_header_t *hdr, byte *data)
{
  unsigned int uint32;

  memcpy (data, hdr->id, NUT_GUID_SIZE);
  data += NUT_GUID_SIZE;
  *data++ = hdr->function;
  *data++ = hdr->ttl;
  *data++ = hdr->hop;
  uint32 = htoll (hdr->length);
  memcpy (data, &uint32, SIZEOF_UINT32);
}

/*
 * Convert gnutella ping response to binary data and back,
 */
nut_ping_reply_t *
nut_get_ping_reply (byte *data)
{
  static nut_ping_reply_t reply;
  unsigned short uint16;
  unsigned int uint32;
  
  memcpy (&uint16, data, SIZEOF_UINT16);
  reply.port = ltons (uint16);
  data += SIZEOF_UINT16;
  memcpy (&uint32, data, SIZEOF_UINT32);
  reply.ip = uint32;
  data += SIZEOF_UINT32;
  memcpy (&uint32, data, SIZEOF_UINT32);
  reply.files = ltohl (uint32);
  data += SIZEOF_UINT32;
  memcpy (&uint32, data, SIZEOF_UINT32);
  reply.size = ltohl (uint32);
  return (&reply);
}

void
nut_put_ping_reply (nut_ping_reply_t *reply, byte *data)
{
  unsigned short uint16;
  unsigned int uint32;
  
  uint16 = ntols (reply->port);
  memcpy (data, &uint16, SIZEOF_UINT16);
  data += SIZEOF_UINT16;
  uint32 = reply->ip;
  memcpy (data, &uint32, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  uint32 = htoll (reply->files);
  memcpy (data, &uint32, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  uint32 = htoll (reply->size);
  memcpy (data, &uint32, SIZEOF_UINT32);
}

/*
 * Convert gnutella search query to binary data and back,
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

void
nut_put_query (nut_query_t *query, byte *data)
{
  unsigned short uint16;

  uint16 = htols (query->speed);
  memcpy (data, &uint16, SIZEOF_UINT16);
}

/*
 * Convert gnutella file records to binary data and back,
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

void
nut_put_record (nut_record_t *record, byte *data)
{
  unsigned int uint32;

  uint32 = htoll (record->index);
  memcpy (data, &uint32, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  uint32 = htoll (record->size);
  memcpy (data, &uint32, SIZEOF_UINT32);
}

/*
 * Convert gnutella query hits to binary data and back,
 */
nut_reply_t *
nut_get_reply (byte *data)
{
  static nut_reply_t reply;
  unsigned int uint32;
  unsigned short uint16;

  reply.records = *data++;
  memcpy (&uint16, data, SIZEOF_UINT16);
  reply.port = ltons (uint16);
  data += SIZEOF_UINT16;
  memcpy (&uint32, data, SIZEOF_UINT32);
  reply.ip = uint32;
  data += SIZEOF_UINT32;
  memcpy (&uint16, data, SIZEOF_UINT16);
  reply.speed = ltohs (uint16);
  return (&reply);
}

void
nut_put_reply (nut_reply_t *reply, byte *data)
{
  unsigned int uint32;
  unsigned short uint16;

  *data++ = reply->records;
  uint16 = ntols (reply->port);
  memcpy (data, &uint16, SIZEOF_UINT16);
  data += SIZEOF_UINT16;
  uint32 = reply->ip;
  memcpy (data, &uint32, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  uint16 = htols (reply->speed);
  memcpy (data, &uint16, SIZEOF_UINT16);
}

/*
 * Convert gnutella push request to binary data and back,
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
  memcpy (&uint32, data, SIZEOF_UINT32);
  push.ip = uint32;
  data += SIZEOF_UINT32;
  memcpy (&uint16, data, SIZEOF_UINT16);
  push.port = ltons (uint16);
  return (&push);
}

void
nut_put_push (nut_push_t *push, byte *data)
{
  unsigned int uint32;
  unsigned short uint16;

  memcpy (data, push->id, NUT_GUID_SIZE);
  data += NUT_GUID_SIZE;
  uint32 = htoll (push->index);
  memcpy (data, &uint32, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  uint32 = push->ip;
  memcpy (data, &uint32, SIZEOF_UINT32);
  data += SIZEOF_UINT32;
  uint16 = ntols (push->port);
  memcpy (data, &uint16, SIZEOF_UINT16);
}

#else /* ENABLE_GNUTELLA */

int nut_core_dummy; /* Shut compiler warnings up */

#endif /* not ENABLE_GNUTELLA */
