/*
 * alloc.c - memory allocation module implementation
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
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
 * $Id: alloc.c,v 1.5 2000/06/30 15:05:39 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "util.h"

#if ENABLE_DEBUG
unsigned allocated_bytes = 0;
unsigned allocated_blocks = 0;
#endif

void * 
xmalloc (unsigned size)
{
  void *ptr;

#if ENABLE_DEBUG
  unsigned *up;
#endif

  assert (size);

#if ENABLE_DEBUG
  if ((ptr = (void *) malloc (size + 2*SIZEOF_UNSIGNED)) != NULL)
    {
#if ENABLE_HEAP_COUNT
      /* save size at the beginning of the block */
      up = (unsigned *)ptr;
      *up = size;
      up+=2;
      ptr = (void *)up;
      allocated_bytes += size;
#endif /* ENABLE_HEAP_COUNT */
      allocated_blocks++;

      return ptr;
    }
#else /* not ENABLE_DEBUG */
  if ((ptr = (void *) malloc (size)) != NULL)
    {
      return ptr;
    }
#endif /* not ENABLE_DEBUG */
  else
    {
      log_printf (LOG_FATAL, "virtual memory exhausted\n");
      exit (1);
    }
}

void *
xrealloc (void * ptr, unsigned size)
{
#if ENABLE_DEBUG
  unsigned old_size;
  unsigned *up;
#endif

  assert (size);

  if (ptr)
    {
#if ENABLE_DEBUG
#if ENABLE_HEAP_COUNT
      /* get previous blocksize */
      up = (unsigned *)ptr;
      up-=2;
      old_size = *up;
      ptr = (void *)up;
#endif /* ENABLE_HEAP_COUNT */

      if ((ptr = (void *) realloc (ptr, size + 2*SIZEOF_UNSIGNED)) != NULL)
	{
#if ENABLE_HEAP_COUNT
	  /* save block size */
	  up = (unsigned *)ptr;
	  *up = size;
	  up+=2;
	  ptr = (void *)up;
	  allocated_bytes += size - old_size;
#endif /* ENABLE_HEAP_COUNT */

	  return ptr;
	}
#else /* not ENABLE_DEBUG */
      if ((ptr = (void *) realloc (ptr, size)) != NULL)
	{
	  return ptr;
	}      
#endif /* not ENABLE_DEBUG */
      else
	{
	  log_printf (LOG_FATAL, "virtual memory exhausted\n");
	  exit (1);
	}
    }
  else
    {
      ptr = xmalloc (size);
      return ptr;
    }
}

void
xfree (void * ptr)
{
#if ENABLE_DEBUG
  unsigned size;
  unsigned *up;
#endif
  
  assert (ptr);

  if (ptr)
    {
#if ENABLE_DEBUG
#if ENABLE_HEAP_COUNT
      /* get blocksize */
      up = (unsigned *)ptr;
      up-=2;
      size = *up;
      assert (size);
      allocated_bytes -= size;
      ptr = (void *)up;
#endif /* ENABLE_HEAP_COUNT */
      allocated_blocks--;
#endif /* ENABLE_DEBUG */
      free (ptr);
    }
}

/*
 * Permanent memory allocators.
 */
void *
xpmalloc (unsigned size)
{
  void *newmem = malloc (size);
  if (newmem == NULL) 
    {
      log_printf (LOG_FATAL, "virtual memory exhausted\n");
      exit (1);
    }
  return newmem;
}

void *
xprealloc (void * ptr, unsigned size)
{
  void *newmem = realloc (ptr, size);
  if (newmem == NULL) 
    {
      log_printf (LOG_FATAL, "virtual memory exhausted\n");
      exit (1);
    }
  return newmem;
}

char *
xpstrdup (char *src)
{
  char *dst = xpmalloc (strlen (src) + 1);

  memcpy (dst, src, strlen (src) + 1);

  return dst;
}
