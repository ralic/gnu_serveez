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
 * $Id: http-cache.c,v 1.8 2000/07/14 00:42:06 ela Exp $
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
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
#endif

#include "alloc.h"
#include "util.h"
#include "hash.h"
#include "http-proto.h"
#include "http-cache.h"

hash_t *http_cache = NULL;   /* actual cache entriy hash */
int http_cache_entries = 0;  /* maximum amount of cache entries */

/*
 * This will initialize the http cache entries.
 */
void
http_alloc_cache (int entries)
{
  if (entries > http_cache_entries || http_cache == NULL)
    {
      http_cache = hash_create (entries);
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
  http_cache_entry_t **cache;

  files = 0;
  total = 0;
  if ((cache = (http_cache_entry_t **) hash_values (http_cache)) != NULL)
    {
      for (n = 0; n < hash_size (http_cache); n++)
	{
	  xfree (cache[n]->buffer);
	  total += cache[n]->size;
	  files++;
	}
      xfree (cache);
    }
  hash_destroy (http_cache);
  http_cache = NULL;
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "cache: freeing %d byte in %d entries\n", 
	      total, files); 
#endif
}

/*
 * This function will make the given cache entry CACHE the most recent
 * within the whole HTTP file cache. All other used entries will be less
 * urgent afterwards.
 */
void
http_urgent_cache (http_cache_entry_t *cache)
{
  int n;
  http_cache_entry_t **caches;

  /* go through all cache entries */
  if ((caches = (http_cache_entry_t **) hash_values (http_cache)) != NULL)
    {
      for (n = 0; n < hash_size (http_cache); n++)
	{
	  /* 
	   * make all used cache entries currently being more recent 
	   * than the given entry one tick less urgent
	   */
	  if (cache != caches[n] && caches[n]->urgent > cache->urgent)
	    {
	      caches[n]->urgent--;
	    }
	}
      xfree (caches);
    }

  /* set the given cache entry to the most recent one */
  cache->urgent = hash_size (http_cache);
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
  http_cache_entry_t *cachefile;

  if ((cachefile = hash_get (http_cache, file)) != NULL)
    {
      /* is this entry fully read by the cache reader ? */
      if (cachefile->ready)
	{
	  /* fill in the cache entry for the cache writer */
	  cache->entry = cachefile;
	  cache->buffer = cachefile->buffer;
	  cache->size = cachefile->size;

	  /* set this entry to the most recent */
	  http_urgent_cache (cachefile);
	  return 0;
	}
    }
  return -1;
}

/*
 * Create a new http cache entry and initialize it.
 */
static http_cache_entry_t *
http_cache_create_entry (void)
{
  http_cache_entry_t *cache;

  cache = xmalloc (sizeof (http_cache_entry_t));
  memset (cache, 0, sizeof (http_cache_entry_t));
  return cache;
}

/*
 * Find a free slot in the http file cache entries. If neccessary
 * delete the least recent. Return zero if there was a free slot.
 */
int
http_init_cache (char *file, http_cache_t *cache)
{
  int n;
  int urgent;
  http_cache_entry_t *slot = NULL;
  http_cache_entry_t **caches;

  urgent = hash_size (http_cache);

  /* 
   * If there are still empty cache entries then create a 
   * new cache entry.
   */
  if (urgent < http_cache_entries)
    {
      slot = http_cache_create_entry ();
      slot->urgent = hash_size (http_cache);
    }

  /*
   * Otherwise find the least recent which is not currently in
   * use by the cache writer or reader.
   */
  else
    {
      caches = (http_cache_entry_t **) hash_values (http_cache);
      for (n = 0; n < hash_size (http_cache); n++)
	{
	  if (caches[n]->urgent <= urgent && !caches[n]->usage)
	    {
	      slot = caches[n];
	      urgent = slot->urgent;
	    }
	}
      /* not a "reinitialable" cache entry found */
      if (!slot) return -1;

      /* is currently used, so free the entry previously */
      xfree (slot->buffer);
      hash_delete (http_cache, slot->file);
      xfree (slot->file);
      slot = http_cache_create_entry ();
      slot->urgent = urgent;
      http_urgent_cache (slot);
    }

  slot->file = xmalloc (strlen (file) + 1);
  strcpy (slot->file, file);
  hash_put (http_cache, file, slot);

  /*
   * initialize the cache entry for the cache file reader: cachebuffer 
   * is not allocated yet and current cache length is zero
   */
  cache->entry = slot;
  cache->buffer = NULL;
  cache->size = 0;

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
  cache->size = 0;
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

  do_write = cache->size;
  if (do_write > (SOCK_MAX_WRITE << 5)) do_write = (SOCK_MAX_WRITE << 5);
  num_written = send (sock->sock_desc, cache->buffer, do_write, 0);

  if (num_written > 0)
    {
      sock->last_send = time (NULL);
      cache->buffer += num_written;
      cache->size -= num_written;
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
  if (cache->size <= 0)
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
      http_urgent_cache (cache->entry);
      if (cache->size > 0) 
	{
	  xfree (cache->buffer);
	  cache->buffer = NULL;
	}
      hash_delete (http_cache, cache->entry->file);
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
				cache->size + num_read);
      memcpy (cache->buffer + cache->size,
	      sock->send_buffer + sock->send_buffer_fill,
	      num_read);
      cache->size += num_read;

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
      cache->entry->size = cache->size;
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
