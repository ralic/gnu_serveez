/*
 * src/alist.h - array list interface
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: alist.h,v 1.8 2001/01/13 18:12:49 ela Exp $
 *
 */

#ifndef __ALIST_H__
#define __ALIST_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

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

/* Exported functions. */
alist_t * alist_create (void);
void alist_destroy (alist_t *list);
void alist_add (alist_t *list, void *value);
void alist_clear (alist_t *list);
unsigned alist_contains (alist_t *list, void *value);
void * alist_get (alist_t *list, unsigned index);
int alist_index (alist_t *list, void *value);
void * alist_delete (alist_t *list, unsigned index);
unsigned alist_delete_range (alist_t *list, unsigned from, unsigned to);
void * alist_set (alist_t *list, unsigned index, void *value);
void * alist_unset (alist_t *list, unsigned index);
unsigned alist_size (alist_t *list);
unsigned alist_length (alist_t *list);
void alist_insert (alist_t *list, unsigned index, void *value);
void ** alist_values (alist_t *list);
void alist_pack (alist_t *list);

#endif /* not __ALIST_H__ */
