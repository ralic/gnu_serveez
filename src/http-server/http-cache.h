/*
 * http-cache.h - http protocol cache header file
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
 * $Id: http-cache.h,v 1.2 2000/06/16 15:36:15 ela Exp $
 *
 */

#ifndef __HTTP_CACHE_H__
#define __HTTP_CACHE_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <time.h>

#include "socket.h"

#define MAX_CACHE          64       /* cache file entries */
#define MAX_CACHE_SIZE     1024*200 /* maximum cache file size */

/*
 * This structure contains the info for a cached file.
 */
typedef struct
{
  char *buffer;        /* pointer to cache buffer */
  int length;          /* file length */
  int size;            /* cache buffer size */
  char *file;          /* actual filename */
  time_t modified;     /* date of last modification */
  int recent;          /* lesser values refer to older entries */
  int used;            /* usage flag */
  int usage;           /* how often this is currently used */
  int hits;            /* cache hits */
  int ready;           /* this flag indicates if the entry is ok */
}
http_cache_entry_t;

/*
 * The http_cache_t type is a structure containing the info
 * a cache writer needs to know.
 */
typedef struct
{
  char *buffer;              /* pointer to cache buffer */
  int length;                /* bytes left within this buffer */
  http_cache_entry_t *entry; /* pointer to original cache file entry */
}
http_cache_t;

extern http_cache_entry_t http_cache[MAX_CACHE];
extern int cache_entries;

void http_free_cache (void);
void http_refresh_cache (http_cache_t *cache);
int http_init_cache (char *file, http_cache_t *cache);
int http_check_cache (char *file, http_cache_t *cache);
int http_cache_write (socket_t sock);
int http_cache_read (socket_t sock);

#endif /* __HTTP_CACHE_H__ */
