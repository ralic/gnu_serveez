/*
 * array.c - array functions
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2001 Raimund Jacob <raimi@lkcc.org>
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
 * $Id: array.c,v 1.5 2001/05/19 23:04:57 ela Exp $
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
#include "libserveez/array.h"

#if ENABLE_DEBUG

/*
 * Create a new array with the initial capacity @var{capacity} and return
 * a pointer to it.
 */
svz_array_t *
svz_array_create (unsigned long capacity)
{
  svz_array_t *array;

  if (!capacity)
    capacity = 16;
  array = svz_malloc (sizeof (svz_array_t));
  memset (array, 0, sizeof (svz_array_t));
  array->data = svz_malloc (sizeof (void *) * capacity);
  array->capacity = capacity;
  return array;
}

/*
 * Delete all values within the array @var{array} and set its size to zero.
 * The array itself keeps valid.
 */
void
svz_array_clear (svz_array_t *array)
{
  assert (array);
  if (array->data)
    {
      svz_free (array->data);
      array->data = NULL;
      array->size = 0;
      array->capacity = 0;
    }
}

/*
 * Completely destroy the array @var{array}. The @var{array} handle is
 * invalid afterwards.
 */
void
svz_array_destroy (svz_array_t *array)
{
  svz_array_clear (array);
  svz_free (array);
}

/*
 * Check if the given @var{size} argument supersedes the capacity of the
 * array @var{array} and reallocate the array if necessary.
 */
static void
svz_array_ensure_capacity (svz_array_t *array, unsigned long size)
{
  if (size > array->capacity)
    {
      array->capacity = array->capacity * 3 / 2 + 1;
      array->data = svz_realloc (array->data, sizeof (void *) * 
				 array->capacity);
    }
}

/*
 * Return the array element at the position @var{index} of the array 
 * @var{array} if the index is within the array range. Return @code{NULL}
 * if not.
 */
void *
svz_array_get (svz_array_t *array, unsigned long index)
{
  assert (array);
  if (index >= array->size)
    return NULL;
  return array->data[index];
}

/*
 * Replace the array element at the position @var{index} of the array
 * @var{array} with the value @var{value} and return the previous value
 * at this index.
 */
void *
svz_array_set (svz_array_t *array, unsigned long index, void *value)
{
  void *prev;

  assert (array  && array->data);
  if (index >= array->size)
    return NULL;
  prev = array->data[index];
  array->data[index] = value;
  return prev;
}

/*
 * Append the value @var{value} at the end of the array @var{array}.
 */
void
svz_array_add (svz_array_t *array, void *value)
{
  assert (array);
  svz_array_ensure_capacity (array, array->size + 1);
  array->data[array->size++] = value;
}

/*
 * Remove the array element at the position @var{index} of the array
 * @var{array}. Return its previous value or @code{NULL} if the index
 * is out of the array's range.
 */
void *
svz_array_del (svz_array_t *array, unsigned long index)
{
  void *value;

  assert (array && array->data);
  if (index >= array->size)
    return NULL;
  value = array->data[index];
  if (index != array->size - 1)
    memmove (&array->data[index], &array->data[index + 1], 
	     (array->size - index - 1) * sizeof (void *));
  array->size--;
  return value;
}

/*
 * Return the given array's @var{array} current capacity.
 */
unsigned long
svz_array_capacity (svz_array_t *array)
{
  assert (array);
  return array->capacity;
}

/*
 * Return the given array's @var{array} current size.
 */
unsigned long
svz_array_size (svz_array_t *array)
{
  assert (array);
  return array->size;
}

/*
 * Returns how often the given value @var{value} is stored in the the array
 * @var{array}. Return zero if there is no such value.
 */
unsigned long
svz_array_contains (svz_array_t *array, void *value)
{
  unsigned long n, found;

  assert (array);
  for (found = n = 0; n < array->size; n++)
    {
      if (array->data[n] == value)
	found++;
    }
  return found;
}

/*
 * This function returns the index of the first occurrence of the value 
 * @var{value} in the array @var{array}. It returns (-1) if there is no
 * such value stored within the array.
 */
unsigned long
svz_array_idx (svz_array_t *array, void *value)
{
  unsigned long n;

  assert (array);
  for (n = 0; n < array->size; n++)
    if (array->data[n] == value)
      return n;
  return (unsigned long) -1;
}

/*
 * This routine inserts the given value @var{value} at the position 
 * @var{index}. The indices of all following values in the array @var{array}
 * and the size of the array get automatically incremented. Return the
 * values index or (-1) if the the index is out of array bounds.
 */
unsigned long
svz_array_ins (svz_array_t *array, unsigned long index, void *value)
{
  assert (array);
  if (index > array->size)
    return (unsigned long) -1;
  svz_array_ensure_capacity (array, array->size + 1);
  if (index < array->size)
    memmove (&array->data[index + 1], &array->data[index], 
	     (array->size - index) * sizeof (void *));
  array->data[index] = value;
  array->size++;
  return index;
}

#endif /* not ENABLE_DEBUG */
