/*
 * http-cache.c - http protocol file cache
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
 * $Id: http-cache.c,v 1.6 2000/06/30 15:05:39 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_HTTP_PROTO

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
#endif

#include "alloc.h"
#include "util.h"
#include "http-proto.h"
#include "http-cache.h"

http_cache_entry_t *http_cache = NULL; /* actual cache entries */
int http_used_entries = 0;             /* currently used cache entries */
int http_cache_entries = 0;            /* maximum amount of cache entries */

/*
 * This will initialize the http cache entries.
 */
void
http_alloc_cache (int entries)
{
  if (entries > http_cache_entries || http_cache == NULL)
    {
      http_cache = xrealloc (http_cache, 
			     sizeof (http_cache_entry_t) * entries);
      memset (http_cache, 0, sizeof (http_cache_entry_t) * entries);
      http_used_entries = 0;
      http_cache_entries = entries;
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "cache: created %d cache entries\n", entries);
#endif
    }
}

/*
 * Free all the cache entries.
 */
void
http_free_cache (void)
{
  int n, total, files;

  files = 0;
  total = 0;
  for (n = 0; n < http_cache_entries; n++)
    {
      if (http_cache[n].used)
	{
	  xfree (http_cache[n].buffer);
	  xfree (http_cache[n].file);
	  total += http_cache[n].size;
	  files++;
	}
    }
  http_used_entries = 0;
  xfree (http_cache);
  http_cache = NULL;
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "cache: freeing %d byte in %d entries\n", 
	      total, files); 
#endif
}

/*
 * This function will make the given cache entry CACHE the most recent
 * within the whole HTTP file cache. All other used entries will be less
 * recent afterwards.
 */
void
http_recent_cache (http_cache_entry_t *cache)
{
  int n;

  /* go through all cache entries */
  for (n = 0; n < http_cache_entries; n++)
    {
      /* 
       * make all used cache entries currently being more recent 
       * than the given entry one tick less recent
       */
      if (http_cache[n].used && cache != &http_cache[n] && 
	  http_cache[n].recent > cache->recent)
	{
	  http_cache[n].recent--;
	}
    }

  /* set the given cache entry to the most recent one */
  cache->recent = http_used_entries;
}

/*
 * This routine checks if a certain FILE is already within
 * the HTTP file cache. It returns zero if it is already cached and
 * fills in the CACHE entry. This entry will be additionally the most
 * recent afterwards.
 */
int
http_check_cache (char *file, http_cache_t *cache)
{
  int n;

  /* go through all cache entries */
  for (n = 0; n < http_cache_entries; n++)
    {
      /* is this entry used and fully read by the cache reader ? */
      if (http_cache[n].used && http_cache[n].ready &&
	  !strcmp (file, http_cache[n].file))
	{
	  /* fill in the cache entry for the cache writer */
	  cache->entry = &http_cache[n];
	  cache->buffer = http_cache[n].buffer;
	  cache->length = http_cache[n].length;

	  /* set this entry to the most recent */
	  http_recent_cache (&http_cache[n]);
	  return 0;
	}
    }
  return -1;
}

/*
 * Find a free slot in the http file cache entries. If neccessary
 * delete the least recent. Return zero if there was a free slot.
 */
int
http_init_cache (char *file, http_cache_t *cache)
{
  int n;
  int recent = http_used_entries;
  http_cache_entry_t *slot = NULL;

  /* go through all cache entries and find a slot */
  for (n = 0; n < http_cache_entries; n++)
    {
      /* is the least recent entry currently in use ? */
      if (!http_cache[n].used)
	{
	  slot = &http_cache[n];
	  break;
	}
      /* 
       * in order to reinit a cache entry it MUST NOT be in usage
       * by the cache reader or writer
       */
      else if (http_cache[n].recent <= recent && http_cache[n].usage == 0)
	{
	  slot = &http_cache[n];
	  recent = slot->recent;
	}
    }

  /* no "reinitalable" cache entry found */
  if (!slot) return -1;

  /* cache entry is not yet currently used */
  if (!slot->used)
    {
      http_used_entries++;
      slot->used = 42;
      slot->recent = http_used_entries;
    }
  /* is currently used, so free the entry previously */
  else
    {
      if (slot->buffer)
	{
	  xfree (slot->buffer);
	  slot->buffer = NULL;
	}
      xfree (slot->file);
      slot->file = NULL;
      http_recent_cache (slot);
    }

  slot->usage = 0; /* not used by cache reader or writer */
  slot->hits = 0;  /* no cache hits until now */
  slot->ready = 0; /* is not ready yet (set by cache reader later) */
  slot->file = xmalloc (strlen (file) + 1);
  strcpy (slot->file, file);

  /*
   * initialize the cache entry for the cache file reader: cachebuffer 
   * is not allocated yet and current cache length is zero
   */
  cache->entry = slot;
  cache->buffer = NULL;
  cache->length = 0;

  return 0;
}

/*
 * Refresh a certain cache entry for reusing it afterwards. So we do not
 * destroy the "used" flag and the "file", but the actual cache content.
 */
void
http_refresh_cache (http_cache_t *cache)
{
  xfree (cache->entry->buffer);
  cache->entry->buffer = NULL;
  cache->entry->ready = 0;
  cache->entry->hits = 0;
  cache->entry->usage = 0;
  cache->length = 0;
  cache->buffer = NULL;
}

/*
 * Send a complete cache entry to a http connection.
 */
int
http_cache_write (socket_t sock)
{
  int num_written;
  int do_write;
  http_socket_t *http;
  http_cache_t *cache;

  /* get additional cache and http structures */
  http = sock->data;
  cache = http->cache;

  do_write = cache->length;
  if (do_write > (SOCK_MAX_WRITE << 5)) do_write = (SOCK_MAX_WRITE << 5);
  num_written = send (sock->sock_desc, cache->buffer, do_write, 0);

  if (num_written > 0)
    {
      sock->last_send = time (NULL);
      cache->buffer += num_written;
      cache->length -= num_written;
    }
  else if (num_written < 0)
    {
      log_printf (LOG_ERROR, "cache: write: %s\n", NET_ERROR);
      if (last_errno == SOCK_UNAVAILABLE)
	{
	  sock->unavailable = time (NULL) + RELAX_FD_TIME;
	  num_written = 0;
	}
      else
	{
	  cache->entry->usage--;
	}
    }

  /*
   * Check if the http cache has (success)fully been sent.
   * If yes then return non-zero in order to shutdown the
   * socket SOCK.
   */
  if (cache->length <= 0)
    {
      cache->entry->usage--;
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "cache: file successfully sent\n");
#endif
      num_written = http_keep_alive (sock);
    }
  
  /*
   * Return a non-zero value if an error occured.
   */
  return (num_written < 0) ? -1 : 0;
}

/*
 * Do just the same as the http_file_read() but additionally copy
 * the data into the cache entry.
 */
int
http_cache_read (socket_t sock)
{
  int num_read;
  int do_read;
  http_socket_t *http;
  http_cache_t *cache;

  /* get additional cache and http structures */
  http = sock->data;
  cache = http->cache;

  do_read = sock->send_buffer_size - sock->send_buffer_fill;

  /* 
   * This means the send buffer is currently full, we have to 
   * wait until some data has been send via the socket.
   */
  if (do_read <= 0)
    {
      return 0;
    }

  /*
   * Try to read as much data as possible from the file.
   */
  num_read = read (sock->file_desc,
		   sock->send_buffer + sock->send_buffer_fill,
		   do_read);
  
  /* Read error occured. */
  if (num_read < 0)
    {
      log_printf (LOG_ERROR, "cache: read: %s\n", SYS_ERROR);

      /* release the actual cache entry previously reserved */
      http_recent_cache (cache->entry);
      cache->entry->used = 0;
      http_used_entries--;
      if (cache->length > 0) 
	{
	  xfree (cache->buffer);
	  cache->buffer = NULL;
	}
      xfree (cache->entry->file);
      cache->entry->file = NULL;
      return -1;
    }

  /* Data has been read. */
  else if (num_read > 0)
    {
      /* 
       * Reserve some more memory and then copy the gained data
       * to the cache entry.
       */
      cache->buffer = xrealloc (cache->buffer, 
				cache->length + num_read);
      memcpy (cache->buffer + cache->length,
	      sock->send_buffer + sock->send_buffer_fill,
	      num_read);
      cache->length += num_read;

      sock->send_buffer_fill += num_read;
      http->filelength -= num_read;
    }

  /* EOF reached and set the apropiate flags */
  if (http->filelength <= 0)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "cache: file successfully read\n");
#endif

      /* fill in the actual cache entry */
      cache->entry->size = cache->length;
      cache->entry->length = cache->length;
      cache->entry->buffer = cache->buffer;
      cache->entry->ready = 42;

      /* set flags */
      sock->userflags |= HTTP_FLAG_DONE;
      sock->flags &= ~SOCK_FLAG_FILE;
    }

  return 0;
}

#else /* ENABLE_HTTP_PROTO */
 
int http_cache_dummy_variable; /* Silence compiler */

#endif /* not ENABLE_HTTP_PROTO */
