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
 * $Id: alist.c,v 1.5 2000/10/22 19:11:03 ela Exp $
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

#define array_range_all(ARRAY, IDX) \
  (IDX >= ARRAY->offset && IDX < ARRAY->offset + ARRAY_SIZE)

#define array_range(ARRAY, IDX) \
  (IDX >= ARRAY->offset && IDX < ARRAY->offset + ARRAY->size)

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
 * Put a given array chunk into the array list.
 */
static void
alist_hook (alist_t *list, array_t *insert)
{
  array_t *array, *next;

  /* find the appropiate array chunk */
  for (array = list->array; array; array = array->next)
    {
      if (insert->offset > array->offset)
	{
	  next = array->next;
	  if (next && insert->offset <= next->offset)
	    {
	      insert->next = array->next;
	      array->next = insert;
	      return;
	    }
	  else if (!next)
	    {
	      array->next = insert;
	      return;
	    }
	}
    }

  /* no chunk found, thus `insert' gets the first chunk */
  insert->next = list->array;
  list->array = insert;
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
	       "chunk %06u, offset: %06u, size: %02u, fill: 0x%08X\n",
	       n + 1, array->offset, array->size, array->fill);
    }
  fprintf (stdout, "length: %u, size: %u\n", list->length, list->size);
}

/*
 * Validates the given array list and prints invalid lists.
 */
int
alist_validate (alist_t *list)
{
  array_t *array, *next;
  unsigned ok = 1, n, bits;

  assert (list);
  for (n = 0, array = list->array; array; n++, array = array->next)
    {
      next = array->next;
      if (next && array->offset + array->size > next->offset)
	{
	  ok = 0;
	  break;
	}
      bits = (1 << array->size) - 1;
      if (array->fill & ~bits || !(array->fill & ((bits + 1) >> 1)))
	{
	  ok = 0;
	  break;
	}
    }
  if (!ok)
    {
      fprintf (stdout, "error in chunk %06u\n", n + 1);
      alist_analyse (list);
    }
  return ok;
}

/*
 * Constructs an empty list without initial capacity.
 */
alist_t *
alist_create (void)
{
  alist_t *list;

  assert (ARRAY_SIZE <= sizeof (unsigned) * 8);
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
      if (next) length -= (next->offset - array->offset - array->size);
      xfree (array);
      array = next;
    }

  list->array = NULL;
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
  unsigned idx;

  for (prev = array = list->array; array; prev = array, array = array->next)
    {
      /* needs the value to be set in this chunk ? */
      if (array_range_all (array, index))
	{
	  idx = index - array->offset;

	  /* already set ? */
	  if (array->fill & (1 << idx))
	    {
	      replace = array->value[idx];
	      array->value[idx] = value;
	      return replace;
	    }
	  /* no, just place the value there */
	  else if (array->next == NULL || idx < array->size)
	    {
	      array->fill |= (1 << idx);
	      if (idx >= array->size) array->size = idx + 1;
	      list->size++;
	      if (list->length < array->offset + array->size)
		list->length = array->offset + array->size;
	      array->value[idx] = value;
	      return replace;
	    }
	}
    }

  /* no array chunk found, create one at the given offset */
  next = alist_create_array (index);
  next->value[0] = value;
  next->fill |= 1;
  next->size = 1;
  alist_hook (list, next);

  /* adjust list properties */
  list->size++;
  if (list->length <= next->offset) list->length = index + 1;

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
	  next = array->next;
	  if (next && array_range_all (next, index))
	    continue;

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

	  array->value[idx] = value;
	  array->fill &= (1 << (idx + 1)) - 1;
	  array->fill |= (1 << idx);
	  array->size = idx + 1;

	  alist_hook (list, next);
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
      alist_hook (list, next);
      array = next->next;
    }
  if (++list->length < index) list->length = index + 1;
  list->size++;

  /* increase offset of all later array chunks */
  while (array)
    {
      if (array->offset > index)
	array->offset++;
      array = array->next;
    }

}

/*
 * Rearranges the given array list. After that there are no gaps within
 * the array list.
 */
void
alist_pack (alist_t *list)
{
  array_t *array, *next, *prev;
  unsigned need2pack, bits, n, size;
  void **value;

  if (!list->size) return;

  /* first check if it is necessary to pack the chunks */
  for (need2pack = 0, array = list->array; array; array = array->next)
    {
      next = array->next;
      if (next && array->size == ARRAY_SIZE)
	{
	  if (array->fill != ARRAY_MASK ||
	      array->offset + ARRAY_SIZE != next->offset)
	    {
	      need2pack = 1;
	      break;
	    }
	}
      if (next && array->size < ARRAY_SIZE)
	{
	  need2pack = 1;
	  break;
	}
      if (!next)
	{
	  bits = (1 << (list->length - array->offset)) - 1;
	  if ((array->fill & bits) != bits)
	    {
	      need2pack = 1;
	      break;
	    }
	}
    }

  /* return if packing is not necessary */
  if (!need2pack) return;

  /* rebuild array list */
  value = alist_values (list);
  size = alist_size (list);
  alist_clear (list);
  prev = list->array;
  for (n = 0; n <= size - ARRAY_SIZE; n += ARRAY_SIZE)
    {
      array = alist_create_array (n);
      array->fill = ARRAY_MASK;
      array->size = ARRAY_SIZE;
      list->size += ARRAY_SIZE;
      memcpy (array->value, &value[n], ARRAY_SIZE * sizeof (void *));
      if (!prev) list->array = array;
      else       prev->next = array;
      prev = array;
    }
  if (size % ARRAY_SIZE)
    {
      size %= ARRAY_SIZE;
      array = alist_create_array (n);
      array->fill = (1 << size) - 1;
      array->size = size;
      list->size += size;
      memcpy (array->value, &value[n], size * sizeof (void *));
      if (!prev) list->array = array;
      else       prev->next = array;
    }
  list->length = list->size;
  xfree (value);
}

/*
 * Delivers all values within this list in a single linear chunk. You 
 * must xfree() it after usage.
 */
void **
alist_values (alist_t *list)
{
  array_t *array;
  void **value;
  unsigned index, bit, n;

  if (!list->size) 
    return NULL;

  value = xmalloc (list->size * sizeof (void *));
  for (index = 0, array = list->array; array; array = array->next) 
    {
      for (bit = 1, n = 0; n < array->size; bit <<= 1, n++)
	{
	  if (array->fill & bit)
	    value[index++] = array->value[n];
	  assert (index <= list->size);
	}
    }
  return value;
}
