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
 * $Id: alist.c,v 1.4 2000/10/20 13:13:48 ela Exp $
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

#define array_range_all(array, index) \
  (index >= array->offset && index < array->offset + ARRAY_SIZE)

#define array_range(array, index) \
  (index >= array->offset && index < array->offset + array->size)

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
 * Print text representation of the array list.
 */
void
alist_analyse (alist_t *list)
{
  unsigned int n;
  array_t *array;

  for (n = 0, array = list->array; array; n++, array = array->next)
    {
      fprintf (stdout, 
	       "chunk %04u, offset: %04u, size: %02u, fill: 0x%08X\n",
	       n + 1, array->offset, array->size, array->fill);
    }
  fprintf (stdout, "length: %u, size: %u\n", list->length, list->size);
}

/*
 * Constructs an empty list without initial capacity.
 */
alist_t *
alist_create (void)
{
  alist_t *list;

  list = xmalloc (sizeof (alist_t));
  memset (list, 0, sizeof (alist_t));
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
  xfree (list);
}

/*
 * Appends the specified element to the end of this list.
 */
void
alist_add (alist_t *list, void *value)
{
  array_t *array = list->array;
  array_t *last = NULL;

  /* find last array chunk */
  while (array)
    {
      last = array;
      array = array->next;
    }

  /* append a new chunk if necessary */
  if (!last || last->size == ARRAY_SIZE)
    {
      array = alist_create_array (last ? last->offset + ARRAY_SIZE : 0);
      if (last)
	last->next = array;
      else
	list->array = array;
      last = array;
    }

  /* append the given value */
  last->value[last->size] = value;
  last->fill |= (1 << last->size);
  last->size++;
  
  list->length++;
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
  unsigned length = list->length;

  /* go through all array chunks and release them */
  while (array)
    {
      next = array->next;
      length -= array->size;
      xfree (array);
      array = next;
    }

  assert (length == 0);
  list->length = 0;
  list->size = 0;
}

/*
 * Returns non-zero if this list contains the specified element.
 */
unsigned
alist_contains (alist_t *list, void *value)
{
  array_t *array = list->array;
  unsigned n, fill, found = 0;

  while (array)
    {
      for (fill = 1, n = 0; n < array->size; n++, fill <<= 1)
	{
	  if (array->fill & fill && array->value[n] == value)
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

  if (index >= list->length)
    return NULL;

  for (array = list->array; array; array = array->next)
    {
      if (array_range (array, index))
	{
	  index -= array->offset;
	  if (array->fill & (1 << index))
	    return array->value[index];
	  else
	    return NULL;
	}
    }
  return NULL;
}

/*
 * Searches for the first occurrence of the given argument. Return -1 if
 * the value could not be found.
 */
int
alist_index (alist_t *list, void *value)
{
  array_t *array = list->array;
  unsigned n, fill;

  while (array)
    {
      for (fill = 1, n = 0; n < array->size; n++, fill <<= 1)
	{
	  if (array->fill & fill && array->value[n] == value)
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
alist_delete (alist_t *list, unsigned index)
{
  array_t *array, *prev;
  void *value = NULL;
  unsigned bit, n, idx, fill;

  if (index >= list->length)
    return NULL;

  for (array = prev = list->array; array; prev = array, array = array->next)
    {
      /* can the value be found in this chunk ? */
      if (array_range (array, index))
	{
	  idx = index - array->offset;

	  /* is there any value at the given index ? */
	  if (!(array->fill & (1 << idx)))
	    return NULL;

	  /* remove this value */
	  array->fill &= ~(1 << idx);
	  list->size--;
	  list->length--;
	  array->size--;
	  value = array->value[idx];

	  /* release this array chunk if possible */
	  if (array->fill == 0)
	    {
	      assert (array->size == 0);
	      /* break here if the list is empty */
	      if (list->size == 0)
		{
		  xfree (array);
		  list->array = array = NULL;
		  break;
		}
	      /* rearrange array list */
	      if (array != prev) prev->next = array->next;
	      else               list->array = array->next;
	      xfree (array);
	      array = prev->next;
	      break;
	    }

	  /* shuffle value data if necessary */
	  if (idx < array->size)
	    {
	      /* delete a bit */
	      for (fill = n = 0, bit = 1; n < array->size; bit <<= 1, n++)
		{
		  if (n < idx) fill |= (array->fill & bit);
		  else         fill |= (array->fill >> 1) & bit;
		}
	      array->fill = fill;
	      
	      /* delete a value */
	      memmove (&array->value[idx], &array->value[idx+1],
		       (array->size - idx) * sizeof (void *));
	    }
	  break;
	}
    }

  /* reduce array index offset by one */
  while (array)
    {
      if (array->offset > index)
	array->offset--;
      array = array->next;
    }

  /* return deleted value */
  return value;
}

/*
 * Removes from this list all of the elements whose index is between 
 * from, inclusive and to, exclusive.
 */
void
alist_delete_range (alist_t *list, unsigned from, unsigned to)
{
}

/*
 * Replaces the element at the specified position in this list with 
 * the specified element.
 */
void *
alist_set (alist_t *list, unsigned index, void *value)
{
  array_t *array, *next, *prev;
  void *replace = NULL;

  for (prev = array = list->array; array; prev = array, array = array->next)
    {
      /* needs the value to be set in this chunk ? */
      if (array_range_all (array, index))
	{
	  index -= array->offset;
	  /* already set ? */
	  if (array->fill & (1 << index))
	    {
	      replace = array->value[index];
	    }
	  /* no, just place the value there */
	  else
	    {
	      array->fill |= (1 << index);
	      if (index >= array->size) array->size = index + 1;
	      list->size++;
	      if (list->length < array->offset + array->size)
		list->length = array->offset + array->size;
	    }
	  array->value[index] = value;
	  return replace;
	}
    }

  /* no array chunk found, create one at the given offset */
  next = alist_create_array (index);
  next->value[0] = value;
  next->fill |= 1;
  next->size = 1;
  list->size++;
  list->length = index + 1;
  if (prev)
    prev->next = next;
  else
    list->array = next;

  /* return replaced value */
  return replace;
}

/*
 * Returns the number of elements in this list.
 */
unsigned
alist_size (alist_t *list)
{
  return list->size;
}

/*
 * Returns the index of the last element of this list plus one.
 */
unsigned
alist_length (alist_t *list)
{
  return list->length;
}

/*
 * Inserts the specified element at the specified position in this list.
 */
void
alist_insert (alist_t *list, unsigned index, void *value)
{
  array_t *array, *next;
  unsigned idx, fill, n, bit, inserted = 0;

  /* search through existing array chunks */
  for (array = list->array; array; array = array->next)
    {
      if (array_range_all (array, index))
	{
	  inserted = 1;
	  idx = index - array->offset;
	  /* can the value be inserted here ? */
	  if (array->size < ARRAY_SIZE)
	    {
	      /* adjust array chunk size */
	      array->size++;
	      if (idx >= array->size) array->size = idx + 1;

	      if (idx < array->size)
		{
		  /* insert a bit */
		  for (fill = n = 0, bit = 1; n < array->size; bit <<= 1, n++)
		    {
		      if (n < idx) fill |= (array->fill & bit);
		      else         fill |= (array->fill << 1) & bit;
		    }
		  array->fill = fill;

		  /* shuffle value data */
		  memmove (&array->value[idx+1], &array->value[idx],
			   (array->size - idx) * sizeof (void *));
		}
	      
	      /* insert the value */
	      array->fill |= (1 << idx);
	      array->value[idx] = value;
	      array = array->next;
	      break;
	    }

	  /* no, chunk is full, need to split the chunk */
	  next = alist_create_array (index + 1);

	  /* keep less indexes in old chunk and copy greater to next */
	  memcpy (&next->value[0], &array->value[idx], 
		  (ARRAY_SIZE - idx) * sizeof (void *));
	  next->fill = (array->fill >> idx);
	  next->size = ARRAY_SIZE - idx;
	  next->next = array->next;

	  array->value[idx] = value;
	  array->fill &= (1 << (idx + 1)) - 1;
	  array->fill |= (1 << idx);
	  array->size = idx + 1;
	  array->next = next;
	  array = next->next;
	  break;
	}
    }

  /* add another chunk */
  if (!inserted)
    {
      next = alist_create_array (index);
      next->fill = 1;
      next->size = 1;
      next->value[0] = value;
      for (array = list->array; array && array->next; array = array->next);
      if (array) array->next = next;
      if (!list->array) list->array = next;
      list->length = index + 1;
      list->size++;
      return;
    }

  /* increase offset of all later array chunks */
  while (array)
    {
      if (array->offset + array->size > index)
	array->offset++;
      array = array->next;
    }

  list->length++;
  list->size++;
}

/*
 * Rearranges the given array list. After that there are no gaps within
 * the array list.
 */
void
alist_pack (alist_t *list)
{
}

/*
 * Delivers all values within this list in a single linear chunk. You 
 * must xfree() it after usage.
 */
void **
alist_values (alist_t *list)
{
  return NULL;
}
