/*
 * alloc.c - memory allocation module implementation
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: alloc.c,v 1.6 2001/02/23 08:29:01 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif 

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libserveez/alloc.h"
#include "libserveez/util.h"

#if DEBUG_MEMORY_LEAKS
# include "libserveez/hash.h"
#endif /* DEBUG_MEMORY_LEAKS */

#if ENABLE_DEBUG
/* The variable @var{svz_allocated_bytes} holds the overall number of bytes 
   allocated by the core library. */
size_t svz_allocated_bytes = 0;
/* This variable holds the number of memory blocks reserved by the core
   library. */
size_t svz_allocated_blocks = 0;
#endif /* ENABLE_DEBUG */

/* The @var{svz_malloc_func} variable is a function pointer for allocating 
   dynamic memory. */
svz_malloc_func_t svz_malloc_func = malloc;
/* This function pointer is called whenever the core library needs to
   reallocate (resize) a memory block. */
svz_realloc_func_t svz_realloc_func = realloc;
/* In order to free a block of memory the core library calls this function
   pointer. */
svz_free_func_t svz_free_func = free;

#if DEBUG_MEMORY_LEAKS

/* heap hash table */
static hash_t *heap = NULL;

/* return static heap hash code key length */
static unsigned
heap_hash_keylen (char *id)
{
  return SIZEOF_VOID_P;
}

/* compare two heap hash values */
static int 
heap_hash_equals (char *id1, char *id2)
{
  return memcmp (id1, id2, SIZEOF_VOID_P);
}

/* calculate heap hash code */
static unsigned long 
heap_hash_code (char *id)
{
  unsigned long code = UINT32 (id);
  code >>= 3;
  return code;
}

/* structure for heap management */
typedef struct
{
  void *ptr;     /* memory pointer */
  size_t size;   /* memory block's size */
  void *caller;  /* the caller */
}
heap_block_t;

/* add another heap block to the heap management */
static void
heap_add (heap_block_t *block)
{
  if (heap == NULL)
    {
      heap = hash_create (4);
      heap->keylen = heap_hash_keylen;
      heap->code = heap_hash_code;
      heap->equals = heap_hash_equals;
    }
  hash_put (heap, (char *) &block->ptr, block);
}

#ifdef MSC_VER
# include <windows.h>
# include <imagehlp.h>
# define __builtin_return_address(stack) stack.AddrReturn
# define heap_caller()                                                     \
    STACKFRAME stack;                                                      \
    StackWalk (IMAGE_FILE_MACHINE_I386, GetCurrentProcess (),              \
	       GetCurrentThread (), &stack, NULL, NULL, NULL, NULL, NULL); \
#else
# ifndef __GNUC__
#  define __builtin_return_address(no) 0
# endif
# define heap_caller()
#endif

#else /* !DEBUG_MEMORY_LEAKS */
# define heap_caller()
#endif /* !DEBUG_MEMORY_LEAKS */

/*
 * Allocate @var{size} bytes of memory and return a pointer to this block.
 */
void * 
svz_malloc (size_t size)
{
  void *ptr;
#if ENABLE_DEBUG
  size_t *up;
#if DEBUG_MEMORY_LEAKS
  heap_block_t *block;
#endif /* DEBUG_MEMORY_LEAKS */
#endif /* ENABLE_DEBUG */

  heap_caller ();
  assert (size);

#if ENABLE_DEBUG
  if ((ptr = (void *) svz_malloc_func (size + 2 * sizeof (size_t))) != NULL)
    {
#if ENABLE_HEAP_COUNT
      /* save size at the beginning of the block */
      up = (size_t *) ptr;
      *up = size;
      up += 2;
      ptr = (void *) up;
#if DEBUG_MEMORY_LEAKS
      /* put heap pointer into special heap hash */
      block = svz_malloc_func (sizeof (heap_block_t));
      block->ptr = ptr;
      block->size = size;
      block->caller = __builtin_return_address (0);
      heap_add (block);
#endif /* DEBUG_MEMORY_LEAKS */
      svz_allocated_bytes += size;
#endif /* ENABLE_HEAP_COUNT */
      svz_allocated_blocks++;
      return ptr;
    }
#else /* not ENABLE_DEBUG */
  if ((ptr = (void *) svz_malloc_func (size)) != NULL)
    {
      return ptr;
    }
#endif /* not ENABLE_DEBUG */
  else
    {
      log_printf (LOG_FATAL, "malloc: virtual memory exhausted\n");
      exit (1);
    }
}

/*
 * Change the size of a @code{svz_malloc()}'ed block of memory. @var{size} 
 * is the new size of the block in bytes, @var{ptr} is the pointer returned 
 * by @code{svz_malloc()} or NULL.
 */
void *
svz_realloc (void *ptr, size_t size)
{
#if ENABLE_DEBUG
  size_t old_size;
  size_t *up;
#endif /* ENABLE_DEBUG */
#if DEBUG_MEMORY_LEAKS
  heap_block_t *block;
#endif /* DEBUG_MEMORY_LEAKS */

  heap_caller ();
  assert (size);

  if (ptr)
    {
#if ENABLE_DEBUG
#if ENABLE_HEAP_COUNT
#if DEBUG_MEMORY_LEAKS
      if ((block = hash_delete (heap, (char *) &ptr)) == NULL ||
	  block->ptr != ptr)
	{
	  fprintf (stdout, "realloc: %p not found in heap (caller: %p)\n", 
		   ptr, __builtin_return_address (0));
	  assert (0);
	}
      svz_free_func (block);
#endif /* DEBUG_MEMORY_LEAKS */

      /* get previous blocksize */
      up = (size_t *) ptr;
      up -= 2;
      old_size = *up;
      ptr = (void *) up;
#endif /* ENABLE_HEAP_COUNT */

      if ((ptr = (void *) svz_realloc_func (ptr, size + 2 * sizeof (size_t))) 
	  != NULL)
	{
#if ENABLE_HEAP_COUNT
	  /* save block size */
	  up = (size_t *) ptr;
	  *up = size;
	  up += 2;
	  ptr = (void *) up;

#if DEBUG_MEMORY_LEAKS
	  block = svz_malloc_func (sizeof (heap_block_t));
	  block->ptr = ptr;
	  block->size = size;
	  block->caller = __builtin_return_address (0);
	  heap_add (block);
#endif /* DEBUG_MEMORY_LEAKS */

	  svz_allocated_bytes += size - old_size;
#endif /* ENABLE_HEAP_COUNT */

	  return ptr;
	}
#else /* not ENABLE_DEBUG */
      if ((ptr = (void *) svz_realloc_func (ptr, size)) != NULL)
	{
	  return ptr;
	}
#endif /* not ENABLE_DEBUG */
      else
	{
	  log_printf (LOG_FATAL, "realloc: virtual memory exhausted\n");
	  exit (1);
	}
    }
  else
    {
      ptr = svz_malloc (size);
      return ptr;
    }
}

/*
 * Free a block of @code{svz_malloc()}'ed or @code{svz_realloc()}'ed memory 
 * block.
 */
void
svz_free (void *ptr)
{
#if ENABLE_DEBUG
#if ENABLE_HEAP_COUNT
  size_t size;
  size_t *up;
#if DEBUG_MEMORY_LEAKS
  heap_block_t *block;
#endif /* DEBUG_MEMORY_LEAKS */
#endif /* ENABLE_HEAP_COUNT */
#endif /* ENABLE_DEBUG */

  heap_caller ();
  assert (ptr);

  if (ptr)
    {
#if ENABLE_DEBUG
#if ENABLE_HEAP_COUNT
      up = (size_t *) ptr;

#if DEBUG_MEMORY_LEAKS
      if ((block = hash_delete (heap, (char *) &ptr)) == NULL ||
	  block->ptr != ptr)
	{
	  fprintf (stdout, "free: %p not found in heap (caller: %p)\n", 
		   ptr, __builtin_return_address (0));
	  assert (0);
	}
     svz_free_func (block);
#endif /* DEBUG_MEMORY_LEAKS */

      /* get blocksize */
      up -= 2;
      size = *up;
      assert (size);
      svz_allocated_bytes -= size;
      ptr = (void *) up;
#endif /* ENABLE_HEAP_COUNT */

      svz_allocated_blocks--;
#endif /* ENABLE_DEBUG */
     svz_free_func (ptr);
    }
}

#if DEBUG_MEMORY_LEAKS
/*
 * Print a list of non-released memory blocks. This is for debugging only
 * and should never occur in final software releases.
 */
void
svz_heap (void)
{
  heap_block_t **block;
  unsigned n;
  size_t *up;

  if ((block = (heap_block_t **) hash_values (heap)) != NULL)
    {
      for (n = 0; n < (unsigned) hash_size (heap); n++)
	{
	  up = (size_t *) block[n]->ptr;
	  up -= 2;
	  fprintf (stdout, "heap: caller = %p, ptr = %p, size = %u\n",
		   block[n]->caller, block[n]->ptr, block[n]->size);
	  util_hexdump (stdout, "unreleased heap", (int) block[n]->ptr,
			block[n]->ptr, *up, 256);
	 svz_free_func (block[n]);
	}
      hash_xfree (block);
    }
  else
    {
      fprintf (stdout, "heap: no unreleased heap blocks\n");
    }
  hash_destroy (heap);
  heap = NULL;
}
#endif /* DEBUG_MEMORY_LEAKS */

/*
 * Duplicate the given string @var{src} if it is not NULL and has got a 
 * valid length. Return the pointer to the copied string.
 */
char *
svz_strdup (char *src)
{
  char *dst;
  int len;

  if (src == NULL || (len = strlen (src)) == 0)
    return NULL;

  dst = svz_malloc (len + 1);
  memcpy (dst, src, len + 1);
  return dst;
}

/*
 * Allocate a block of memory with the size @var{size} permanently.
 */
void *
svz_pmalloc (size_t size)
{
  void *ptr = svz_malloc_func (size);
  if (ptr == NULL) 
    {
      log_printf (LOG_FATAL, "malloc: virtual memory exhausted\n");
      exit (1);
    }
  return ptr;
}

/*
 * Resize the memory block pointed to by @var{ptr} to @var{size} bytes. This
 * routine also allocates memory permanently.
 */
void *
svz_prealloc (void *ptr, size_t size)
{
  void *dst = svz_realloc_func (ptr, size);
  if (dst == NULL) 
    {
      log_printf (LOG_FATAL, "realloc: virtual memory exhausted\n");
      exit (1);
    }
  return dst;
}

/*
 * Duplicate the given string @var{src} permanently.
 */
char *
svz_pstrdup (char *src)
{
  char *dst;

  assert (src);
  dst = svz_pmalloc (strlen (src) + 1);
  memcpy (dst, src, strlen (src) + 1);

  return dst;
}
