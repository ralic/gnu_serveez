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
 * $Id: alist.c,v 1.9 2000/11/03 01:25:06 ela Exp $
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

#define DEVEL 0

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
 * Put the given array chunk INSERT into the array list LIST.
 */
static void
alist_hook (alist_t *list, array_t *insert)
{
  array_t *array, *next;

  /* find the appropriate array chunk */
  for (array = list->first; array; array = array->next)
    {
      if (insert->offset > array->offset)
	{
	  next = array->next;
	  /* really insert the chunk */
	  if (next && insert->offset <= next->offset)
	    {
	      insert->next = next;
	      insert->prev = array;
	      array->next = insert;
	      next->prev = insert;
	      return;
	    }
	  /* append at the end of chunks */
	  else if (!next)
	    {
	      array->next = insert;
	      insert->prev = array;
	      list->last = insert;
	      return;
	    }
	}
    }

  /* no chunk found, thus INSERT gets the first chunk */
  insert->next = list->first;
  if (list->first) list->first->prev = insert;
  list->first = insert;
  if (!list->last) list->last = insert;
}

/*
 * Cut the given chunk off the array list chain.
 */
static void
alist_unhook (alist_t *list, array_t *delete)
{
  if (list->first == delete)
    {
      list->first = delete->next;
      if (list->first) list->first->prev = NULL;
      return;
    } 
  if (list->last == delete)
    {
      list->last = delete->prev;
      if (list->last) list->last->next = NULL;
      return;
    }
  delete->prev->next = delete->next;
  delete->next->prev = delete->prev;
}

/*
 * Try to find a given INDEX in the array list chunks as fast as
 * possible.
 */
static array_t *
alist_find_array (alist_t *list, unsigned index)
{
  array_t *array = NULL;

  /* index larger than list length ? */
  if (index >= list->length)
    {
      /* is index available in last chunk ? */
      if (list->last && array_range_all (list->last, index))
	array = list->last;
    }
  /* start seeking in second half */
  else if (index > list->length >> 1)
    {
      for (array = list->last; array; array = array->prev)
	if (array_range_all (array, index))
	  break;
    }
  /* start seeking at the start of the list (usual case) */ 
  else
    {
      /* index lesser than offset of first array chunk ? */
      array = list->first;
      if (array && index < array->offset)
	return NULL;

      for (; array; array = array->next)
	if (array_range_all (array, index))
	  {
	    if (array->next && array_range_all (array->next, index))
	      continue;
	    break;
	  }
    }
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

  for (n = 0, array = list->first; array; n++, array = array->next)
    {
      fprintf (stdout, 
	       "chunk %06u at %p, ofs: %06u, size: %02u, fill: %08X, "
	       "prev: %p, next %p\n",
	       n + 1, array, array->offset, array->size, array->fill, 
	       array->prev, array->next);
    }
  fprintf (stdout, "length: %u, size: %u, first: %p, last: %p\n", 
	   list->length, list->size, list->first, list->last);
}

/*
 * Validates the given array list and prints invalid lists.
 */
int
alist_validate (alist_t *list)
{
  array_t *array, *next, *prev;
  unsigned ok = 1, n, bits;

  assert (list);
  for (n = 0, array = list->first; array; n++, array = array->next)
    {
      next = array->next;
      prev = array->prev;

      /* check chain first */
      if ((!next && array != list->last) || (!prev && array != list->first))
	{
	  fprintf (stdout, "invalid last or first\n");
	  ok = 0;
	  break;
	}
      if ((next && next->prev != array) || (prev && prev->next != array))
	{
	  fprintf (stdout, "invalid next or prev\n");
	  ok = 0;
	  break;
	}

      /* check chunk size and offset */
      if (next && array->offset + array->size > next->offset)
	{
	  fprintf (stdout, "invalid size or offset\n");
	  ok = 0;
	  break;
	}

      /* check array chunk fill */
      bits = (1 << array->size) - 1;
      if (array->fill & ~bits || !(array->fill & ((bits + 1) >> 1)))
	{
	  fprintf (stdout, "invalid chunk fill\n");
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
  array_t *array, *last = list->last;

#if DEVEL
  alist_validate (list);
#endif /* DEVEL */

  /* append a chunk if necessary */
  if (!last || last->size == ARRAY_SIZE)
    {
      array = alist_create_array (last ? last->offset + ARRAY_SIZE : 0);
      if (last)
	{
	  last->next = array;
	  array->prev = list->last;
	}
      else list->first = array;
      list->last = last = array;
    }

  /* append the given value */
  last->value[last->size] = value;
  last->fill |= (1 << last->size);
  last->size++;
  
  /* adjust array list properties */
  list->length++;
  list->size++;
}

/*
 * Removes all of the elements from this list.
 */
void
alist_clear (alist_t *list)
{
  array_t *next, *array = list->first;
  unsigned length = list->length;

#if DEVEL
  alist_validate (list);
#endif /* DEVEL */

  /* return here if there is nothing to do */
  if (!array || !length)
    return;

  /* go through all array chunks and release these */
  length -= array->offset;
  while (array)
    {
      next = array->next;
      length -= array->size;
      if (next) length -= (next->offset - array->offset - array->size);
      xfree (array);
      array = next;
    }

  /* cleanup array list */
  assert (length == 0);
  list->last = list->first = NULL;
  list->length = 0;
  list->size = 0;
}

/*
 * Returns non-zero if this list contains the specified element.
 */
unsigned
alist_contains (alist_t *list, void *value)
{
  array_t *array = list->first;
  unsigned n, fill, found = 0;

#if DEVEL
  alist_validate (list);
#endif /* DEVEL */

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

#if DEVEL
  alist_validate (list);
#endif /* DEVEL */

  /* return here if there is no such index at all */
  if (index >= list->length)
    return NULL;

  /* start searching at first or last chunk ? */
  if (index > list->length >> 1)
    {
      for (array = list->last; array; array = array->prev)
	if (array_range (array, index))
	  break;
    }
  else
    {
      for (array = list->first; array; array = array->next)
	if (array_range (array, index))
	  break;
    }

  /* evaluate peeked array chunk */
  if (!array) return NULL;
  index -= array->offset;
  if (array->fill & (1 << index))
    return array->value[index];
  return NULL;
}

/*
 * Searches for the first occurrence of the given argument. Return -1 if
 * the value could not be found.
 */
int
alist_index (alist_t *list, void *value)
{
  array_t *array = list->first;
  unsigned n, fill;

#if DEVEL
  alist_validate (list);
#endif /* DEVEL */

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
  array_t *array, *next;
  void *value = NULL;
  unsigned bit, idx, fill;

#if DEVEL
  alist_validate (list);
#endif /* DEVEL */

  /* return if index is invalid */
  if (index >= list->length)
    return NULL;

  /* start at first or last array chunk ? */
  if (index > list->length >> 1)
    {
      for (array = list->last; array; array = array->prev)
	if (array_range (array, index))
	  break;
    }
  else
    {
      for (array = list->first; array; array = array->next)
	if (array_range (array, index))
	  break;
    }

  /* evaluate peeked array chunk */
  if (!array) return NULL;
  idx = index - array->offset;

  /* is there any value at the given index ? */
  if (!(array->fill & (1 << idx)))
    return NULL;

  /* delete this value */
  array->fill &= ~(1 << idx);
  list->size--;
  list->length--;
  array->size--;
  value = array->value[idx];

  /* release this array chunk if possible */
  if (array->size == 0)
    {
      /* break here if the list is empty */
      if (list->size == 0)
	{
	  xfree (array);
	  list->last = list->first = array = NULL;
	  return value;
	}

      /* rearrange array list */
      alist_unhook (list, array);
      next = array->next;
      xfree (array);
      array = next;
    }

  /* shuffle value data if necessary */
  else if (idx < array->size)
    {
      /* delete a bit */
      bit = (1 << idx) - 1;
      fill = array->fill;
      array->fill = (fill & bit) | ((fill >> 1) & ~bit);
	      
      /* delete a value */
      memmove (&array->value[idx], &array->value[idx+1],
	       (array->size - idx) * sizeof (void *));
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
 * from, inclusive and to, exclusive. Return the amount of really 
 * deleted items.
 */
unsigned
alist_delete_range (alist_t *list, unsigned from, unsigned to)
{
  unsigned idx, n = 0;

#if DEVEL
  alist_validate (list);
#endif /* DEVEL */

  /* swap the `to' and `from' indexes if necessary */
  if (to < from)
    {
      idx = to;
      to = from + 1;
      from = idx + 1;
    }

  /* return here if there is nothing to do */
  if (to > list->length) to = list->length;
  if (from > list->length) from = list->length;
  if (to == from) return 0;

  /* special case: delete all list elements */
  if (from == 0 && to == list->length)
    {
      n = list->size;
      alist_clear (list);
      return n;
    }

  /* go through the index range and delete each list item */
  for (idx = from; idx < to; )
    {
      if (alist_delete (list, idx))
	{
	  to--;
	  n++;
	}
      else idx++;
    }
  return n;
}

/*
 * Replaces the element at the specified position in this list with 
 * the specified element.
 */
void *
alist_set (alist_t *list, unsigned index, void *value)
{
  array_t *array, *next;
  void *replace = NULL;
  unsigned idx;

#if DEVEL
  alist_validate (list);
#endif /* DEVEL */

  /* start at first or last array chunk ? */
  array = alist_find_array (list, index);

  /* found a valid array chunk ? */
  if (array)
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
  unsigned idx, fill, bit;

#if DEVEL
  alist_validate (list);
#endif /* DEVEL */

  /* start at first or last array chunk ? */
  array = alist_find_array (list, index);

  /* found a valid array chunk ? */
  if (array)
    {
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
	      bit = (1 << idx) - 1;
	      fill = array->fill;
	      array->fill = (fill & bit) | ((fill << 1) & ~bit);

	      /* shuffle value data */
	      memmove (&array->value[idx+1], &array->value[idx],
		       (array->size - 1 - idx) * sizeof (void *));
	    }
	      
	  /* insert the value */
	  array->fill |= (1 << idx);
	  array->value[idx] = value;
	  array = array->next;
	}

      /* no, chunk is full, need to split the chunk */
      else
	{
	  next = alist_create_array (index + 1);

	  /* keep less indexes in old chunk and copy greater to next */
	  memcpy (next->value, &array->value[idx], 
		  (ARRAY_SIZE - idx) * sizeof (void *));
	  next->fill = (array->fill >> idx);
	  next->size = ARRAY_SIZE - idx;

	  array->value[idx] = value;
	  array->fill &= (1 << (idx + 1)) - 1;
	  array->fill |= (1 << idx);
	  array->size = idx + 1;

	  alist_hook (list, next);
	  array = next->next;
	}
    }

  /* add another chunk */
  else
    {
      next = alist_create_array (index);
      next->fill = 1;
      next->size = 1;
      next->value[0] = value;
      alist_hook (list, next);
      array = next->next;
    }

  /* adjust array list properties here */
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

#if DEVEL
  alist_validate (list);
#endif /* DEVEL */

  if (!list->size) return;

  /* first check if it is necessary to pack the chunks */
  for (need2pack = 0, array = list->first; array; array = array->next)
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
  prev = list->first;
  for (n = 0; n <= size - ARRAY_SIZE; n += ARRAY_SIZE)
    {
      array = alist_create_array (n);
      array->fill = ARRAY_MASK;
      array->size = ARRAY_SIZE;
      list->size += ARRAY_SIZE;
      memcpy (array->value, &value[n], ARRAY_SIZE * sizeof (void *));
      if (!prev) list->first = array;
      else       prev->next = array;
      array->prev = prev;
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
      if (!prev) list->first = array;
      else       prev->next = array;
      array->prev = prev;
    }
  list->last = array;
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

#if DEVEL
  alist_validate (list);
#endif /* DEVEL */

  if (!list->size) 
    return NULL;

  value = xmalloc (list->size * sizeof (void *));
  for (index = 0, array = list->first; array; array = array->next) 
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
