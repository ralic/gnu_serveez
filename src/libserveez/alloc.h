/*
 * alloc.h - memory allocation module declarations
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
 * $Id: alloc.h,v 1.5 2001/04/21 16:24:24 ela Exp $
 *
 */

#ifndef __XALLOC_H__
#define __XALLOC_H__ 1

#include "libserveez/defines.h"

#define svz_free_and_zero(var) \
  do { svz_free (var); (var) = NULL; } while (0)

__BEGIN_DECLS

#if ENABLE_DEBUG
SERVEEZ_API extern size_t svz_allocated_bytes;
SERVEEZ_API extern size_t svz_allocated_blocks;
#endif /* ENABLE_DEBUG */

/* Function type definitions. */
typedef void * (* svz_malloc_func_t) __P ((size_t));
typedef void * (* svz_realloc_func_t) __P ((void *, size_t));
typedef void (* svz_free_func_t) __P ((void *));

/* Global allocator functions. */
SERVEEZ_API extern svz_malloc_func_t svz_malloc_func;
SERVEEZ_API extern svz_realloc_func_t svz_realloc_func;
SERVEEZ_API extern svz_free_func_t svz_free_func;

/* Internal allocator functions. */
SERVEEZ_API void *svz_malloc __P ((size_t size));
SERVEEZ_API void *svz_calloc __P ((size_t size));
SERVEEZ_API void *svz_realloc __P ((void *ptr, size_t size));
SERVEEZ_API void svz_free __P ((void *ptr));
SERVEEZ_API char *svz_strdup __P ((char *src));

/* Internal permanent allocator functions. */
SERVEEZ_API void *svz_pmalloc __P ((size_t size));
SERVEEZ_API void *svz_prealloc __P ((void *ptr, size_t size));
SERVEEZ_API char *svz_pstrdup __P ((char *src));

#if DEBUG_MEMORY_LEAKS
SERVEEZ_API void svz_heap __P ((void));
#endif /* DEBUG_MEMORY_LEAKS */

__END_DECLS

#endif /* not __XALLOC_H__ */
