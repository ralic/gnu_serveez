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
 * $Id: alist.h,v 1.1 2001/01/28 03:26:55 ela Exp $
 *
 */

#ifndef __ALIST_H__
#define __ALIST_H__ 1

#include "libserveez/defines.h"

/* general defines */
#define ARRAY_BITS 4
#define ARRAY_SIZE (1 << ARRAY_BITS)
#define ARRAY_MASK ((1 << ARRAY_SIZE) - 1)

/* array chunk structure */
typedef struct array_chunk array_t;
struct array_chunk
{
  array_t *next;           /* pointer to next array chunk */
  array_t *prev;           /* pointer to previous array chunk */
  unsigned offset;         /* first array index in this chunk */
  unsigned fill;           /* usage bit-field */
  unsigned size;           /* size of this chunk */
  void *value[ARRAY_SIZE]; /* value storage */
};

/* top level array list structure */
typedef struct array_list alist_t;
struct array_list
{
  unsigned length; /* size of the array (last index plus one) */
  unsigned size;   /* element count */
  array_t *first;  /* first array chunk */
  array_t *last;   /* last array chunk */
};

__BEGIN_DECLS

/* Exported functions. */
SERVEEZ_API alist_t * alist_create __P ((void));
SERVEEZ_API void alist_destroy __P ((alist_t *list));
SERVEEZ_API void alist_add __P ((alist_t *list, void *value));
SERVEEZ_API void alist_clear __P ((alist_t *list));
SERVEEZ_API unsigned alist_contains __P ((alist_t *list, void *value));
SERVEEZ_API void * alist_get __P ((alist_t *list, unsigned index));
SERVEEZ_API int alist_index __P ((alist_t *list, void *value));
SERVEEZ_API void * alist_delete __P ((alist_t *list, unsigned index));
SERVEEZ_API unsigned alist_delete_range __P ((alist_t *, unsigned, unsigned));
SERVEEZ_API void * alist_set __P ((alist_t *, unsigned, void *));
SERVEEZ_API void * alist_unset __P ((alist_t *list, unsigned index));
SERVEEZ_API unsigned alist_size __P ((alist_t *list));
SERVEEZ_API unsigned alist_length __P ((alist_t *list));
SERVEEZ_API void alist_insert __P ((alist_t *, unsigned, void *));
SERVEEZ_API void ** alist_values __P ((alist_t *list));
SERVEEZ_API void alist_pack __P ((alist_t *list));

__END_DECLS

#endif /* not __ALIST_H__ */
