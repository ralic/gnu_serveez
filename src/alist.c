/*
 * src/alist.c - array list functions
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
 * $Id: alist.c,v 1.1 2000/10/16 18:39:49 ela Exp $
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

#include "alloc.h"
#include "util.h"
#include "alist.h"

/*
 * Constructs an empty list with an initial capacity.
 */
alist_t *
alist_create (void)
{
  alist_t *alist;

  alist = xmalloc (sizeof (alist_t));
  alist->size = 0;
  alist->array = xmalloc (sizeof (array_t));
  alist->array->fill = 0;
  alist->array->next = NULL;

  return alist;
}

/*
 * Destroys the given list completely. The argument cannot be used
 * afterwards.
 */
void
alist_destroy (alist_t *alist)
{
  xfree (alist);
}

/*
 * Appends the specified element to the end of this list.
 */
void
alist_add (alist_t *alist, void *value)
{
  int n;
  array_t *array = alist->array;

  while (array->next) array = array->next;

  if (array->fill == ARRAY_SIZE)
    {
      array->next = xmalloc (sizeof (array_t));
      array = array->next;
      array->next = NULL;
      array->fill = 0;
    }

  array->value[array->fill] = value;
  array->fill++;
  alist->size++;
}

/*
 * Removes all of the elements from this list.
 */
void
alist_clear (alist_t *alist)
{
}

/*
 * Returns non-zero if this list contains the specified element.
 */
int
alist_contains (alist_t *alist, void *value)
{
  return 0;
}

/*
 * Returns the element at the specified position in this list.
 */
void *
alist_get (alist_t *alist, int index)
{
  return NULL;
}

/*
 * Searches for the first occurence of the given argument.
 */
int
alist_index (alist_t *alist, void *value)
{
  return -1;
}

/*
 * Removes the element at the specified position in this list.
 */
void *
alist_del (alist_t *alist, int index)
{
  return NULL;
}

/*
 * Removes from this list all of the elements whose index is between 
 * from, inclusive and to, exclusive.
 */
void
alist_del_range (alist_t *alist, int from, int to)
{
}

/*
 * Replaces the element at the specified position in this list with 
 * the specified element.
 */
void *
alist_set (alist_t *alist, int index, void *value)
{
  return NULL;
}

/*
 * Returns the number of elements in this list.
 */
int
alist_size (alist_t *alist)
{
  return alist->size;
}

/*
 * Inserts the specified element at the specified position in this list.
 */
void
alist_insert (alist_t *alist, int index, void *value)
{
}

