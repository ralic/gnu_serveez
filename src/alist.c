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
 * $Id: alist.c,v 1.2 2000/10/16 22:58:35 ela Exp $
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
 * Create and initialize a new array chunk at a given index offset.
 */
static array_t *
alist_create_array (unsigned offset)
{
  array_t *array;

  array = xmalloc (sizeof (array_t));
  memset (array, 0, sizeof (array_t));
  array->offset = offset;
  return array;
}

/*
 * Constructs an empty list with an initial capacity.
 */
alist_t *
alist_create (void)
{
  alist_t *list;

  list = xmalloc (sizeof (alist_t));
  list->size = 0;
  list->array = alist_create_array (0);

  return list;
}

/*
 * Destroys the given list completely. The argument cannot be used
 * afterwards.
 */
void
alist_destroy (alist_t *list)
{
  alist_clear (list);
  xfree (list->array);
  xfree (list);
}

/*
 * Appends the specified element to the end of this list.
 */
void
alist_add (alist_t *list, void *value)
{
  array_t *array = list->array;

  /* find last array chunk */
  while (array->next) array = array->next;

  /* append a new chunk if necessary */
  if (array->fill == ARRAY_SIZE)
    {
      array->next = alist_create_array (array->offset + ARRAY_SIZE);
      array = array->next;
    }

  array->value[array->fill] = value;
  array->fill++;
  list->size++;
}

/*
 * Removes all of the elements from this list.
 */
void
alist_clear (alist_t *list)
{
  array_t *array = list->array;
  array_t *next = array->next;
  unsigned size = list->size;

  /* go through all array chunks and release them */
  while (next)
    {
      array = next;
      size -= array->fill;
      next = next->next;
      xfree (array);
    }

  assert (list->array->fill == size);
  list->size = size;
}

/*
 * Returns non-zero if this list contains the specified element.
 */
unsigned
alist_contains (alist_t *list, void *value)
{
  array_t *array = list->array;
  unsigned n, found = 0;

  while (array)
    {
      for (n = 0; n < array->fill; n++)
	{
	  if (array->value[n] == value)
	    found++;
	}
      array = array->next;
    }

  return found;
}

/*
 * Returns the element at the specified position in this list.
 */
void *
alist_get (alist_t *list, unsigned index)
{
  array_t *array;

  if (index >= list->size)
    return NULL;

  for (array = list->array; array; array = array->next)
    {
      if (index >= array->offset && index < array->offset + ARRAY_SIZE)
	{
	  index -= array->offset;
	  return array->value[index];
	}
    }
  return NULL;
}

/*
 * Searches for the first occurence of the given argument. Return -1 if
 * the value could not be found.
 */
int
alist_index (alist_t *list, void *value)
{
  array_t *array = list->array;
  unsigned n;

  while (array)
    {
      for (n = 0; n < array->fill; n++)
	{
	  if (array->value[n] == value)
	    return (n + array->offset);
	}
      array = array->next;
    }
  return -1;
}

/*
 * Removes the element at the specified position in this list.
 */
void *
alist_del (alist_t *list, unsigned index)
{
  array_t *array, *prev;
  void *value = NULL;

  for (array = prev = list->array; array; prev = array, array = array->next)
    {
      /* can the value be found in this chunk ? */
      if (index >= array->offset && index < array->offset + ARRAY_SIZE)
	{
	  list->size--;
	  array->fill--;
	  value = array->value[index - array->offset];

	  /* release this array chunk if possible */
	  if (array->fill == 0 && array != prev)
	    {
	      prev->next = array->next;
	      xfree (array);
	      array = prev;
	    }
	}

      /* reduce array index offset by one */
      if (index >= array->offset + ARRAY_SIZE)
	array->offset--;
    }

  /* return deleted value */
  return value;
}

/*
 * Removes from this list all of the elements whose index is between 
 * from, inclusive and to, exclusive.
 */
void
alist_del_range (alist_t *list, unsigned from, unsigned to)
{
}

/*
 * Replaces the element at the specified position in this list with 
 * the specified element.
 */
void *
alist_set (alist_t *list, unsigned index, void *value)
{
  array_t *array;
  void *replace = NULL;

  for (array = list->array; array; array = array->next)
    {
      /* needs the value to be set in this chunk ? */
      if (index >= array->offset && index < array->offset + ARRAY_SIZE)
	{
	  replace = array->value[index - array->offset];
	  array->value[index - array->offset] = value;
	  return replace;
	}
    }

  /* return replaced value */
  return replace;
}

/*
 * Returns the number of elements in this list.
 */
int
alist_size (alist_t *list)
{
  return list->size;
}

/*
 * Inserts the specified element at the specified position in this list.
 */
void
alist_insert (alist_t *list, int index, void *value)
{
}





