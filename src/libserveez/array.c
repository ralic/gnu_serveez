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
 * $Id: array.c,v 1.1 2001/03/08 22:15:13 raimi Exp $
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

/*
 * Create a new array with the initial capacity @var{capacity} and return
 * the pointer to it.
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
 * Delete all values within the array @var{arrray}. Set the size to zero.
 */
void
svz_array_clear (svz_array_t *array)
{
  assert (array);
  if (array->size && array->data)
    {
      svz_free (array->data);
      array->data = NULL;
      array->size = 0;
    }
}

/*
 * Completely destroy the array @var{arrray}.
 */
void
svz_array_destroy (svz_array_t *array)
{
  svz_array_clear (array);
  svz_free (array);
}

/*
 * Check if the given @var{size} argument supersedes the capacity of the
 * array @var{arrray} and reallocate the array if necessary.
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
 * @var{arrray} if the index is within the array range. Return NULL if not.
 */
void *
svz_array_get (svz_array_t *array, unsigned long index)
{
  assert (array  && array->data);
  if (index >= array->size)
    return NULL;
  return array->data[index];
}

/*
 * Replace the array element at the position @var{index} of the array
 * @var{arrray} with the value @var{value} and return its previous value.
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
 * Append the value @var{value} at the end of the array @var{arrray}.
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
 * @var{arrray}. Return its previous value.
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
 * Return the array's @var{arrray} capacity.
 */
unsigned long
svz_array_capacity (svz_array_t *array)
{
  assert (array);
  return array->capacity;
}

/*
 * Return the array's @var{arrray} current size.
 */
unsigned long
svz_array_size (svz_array_t *array)
{
  assert (array);
  return array->size;
}
