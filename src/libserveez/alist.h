/*
 * alist.h - array list interface
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: alist.h,v 1.3 2001/02/06 17:24:20 ela Exp $
 *
 */

#ifndef __ALIST_H__
#define __ALIST_H__ 1

#include "libserveez/defines.h"

/* general array list defines */
#define SVZ_ARRAY_BITS 4                     /* values 1 .. 6 possible */
#define SVZ_ARRAY_SIZE (1 << SVZ_ARRAY_BITS) /* values 1 .. 64 possible */
#define SVZ_ARRAY_MASK ((1 << SVZ_ARRAY_SIZE) - 1)

/* 
 * On 32 bit architectures SVZ_ARRAY_SIZE is no larger than 32 and on 
 * 64 bit architectures it is no larger than 64. It specifies the number 
 * of bits the `alist->fill' (unsigned long) field can hold.
 */

/* array chunk structure */
typedef struct svz_array_chunk svz_array_t;
struct svz_array_chunk
{
  svz_array_t *next;           /* pointer to next array chunk */
  svz_array_t *prev;           /* pointer to previous array chunk */
  unsigned long offset;        /* first array index in this chunk */
  unsigned long fill;          /* usage bit-field */
  unsigned long size;          /* size of this chunk */
  void *value[SVZ_ARRAY_SIZE]; /* value storage */
};

/* top level array list structure */
typedef struct svz_array_list svz_alist_t;
struct svz_array_list
{
  unsigned long length; /* size of the array (last index plus one) */
  unsigned long size;   /* element count */
  svz_array_t *first;   /* first array chunk */
  svz_array_t *last;    /* last array chunk */
};

__BEGIN_DECLS

/* 
 * Exported array list functions. An array list is a kind of data array 
 * which grows and shrinks on demand. It unifies the advantages of chained
 * lists (less memory usage than simple arrays) and arrays (faster access 
 * to specific elements). This implementation can handle gaps in between
 * the array elements.
 */

SERVEEZ_API svz_alist_t *svz_alist_create __P ((void));
SERVEEZ_API void svz_alist_destroy __P ((svz_alist_t *));
SERVEEZ_API void svz_alist_add __P ((svz_alist_t *, void *));
SERVEEZ_API void svz_alist_clear __P ((svz_alist_t *));
SERVEEZ_API unsigned long svz_alist_contains __P ((svz_alist_t *, void *));
SERVEEZ_API void *svz_alist_get __P ((svz_alist_t *, unsigned long));
SERVEEZ_API unsigned long svz_alist_index __P ((svz_alist_t *, void *));
SERVEEZ_API void *svz_alist_delete __P ((svz_alist_t *, unsigned long));
SERVEEZ_API unsigned long svz_alist_delete_range __P ((svz_alist_t *, 
						       unsigned long, 
						       unsigned long));
SERVEEZ_API void *svz_alist_set __P ((svz_alist_t *, unsigned long, void *));
SERVEEZ_API void *svz_alist_unset __P ((svz_alist_t *, unsigned long));
SERVEEZ_API unsigned long svz_alist_size __P ((svz_alist_t *));
SERVEEZ_API unsigned long svz_alist_length __P ((svz_alist_t *));
SERVEEZ_API void svz_alist_insert __P ((svz_alist_t *, 
					unsigned long, void *));
SERVEEZ_API void **svz_alist_values __P ((svz_alist_t *));
SERVEEZ_API void svz_alist_pack __P ((svz_alist_t *));

__END_DECLS

#endif /* not __ALIST_H__ */
