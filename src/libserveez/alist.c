/*
 * alist.c - array list functions
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
 * $Id: alist.c,v 1.2 2001/01/29 22:41:32 ela Exp $
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
#include "libserveez/alist.h"

/* check if a given array index can be in this array chunk */
#define array_range_all(ARRAY, IDX) \
  (IDX >= ARRAY->offset && IDX < ARRAY->offset + ARRAY_SIZE)

/* check if a given array index is in this array chunk */
#define array_range(ARRAY, IDX) \
  (IDX >= ARRAY->offset && IDX < ARRAY->offset + ARRAY->size)

/* define if development code should be included */
#define DEVEL 1

/*
 * Create and initialize a new array chunk at a given array index OFFSET.
 * Return this array chunk.
 */
static array_t *
alist_create_array (unsigned long offset)
{
  array_t *array;

  array = svz_malloc (sizeof (array_t));
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
  if (list->first)
    list->first->prev = insert;
  list->first = insert;
  if (!list->last)
    list->last = insert;
}

/*
 * Cut the given array chunk DELETE off the array list LIST chain.
 */
static void
alist_unhook (alist_t *list, array_t *delete)
{
  if (list->first == delete)
    {
      list->first = delete->next;
      if (list->first)
	list->first->prev = NULL;
      if (list->last == delete)
	{
	  list->last = NULL;
	  list->length = list->size = 0;
	}
      return;
    } 
  if (list->last == delete)
    {
      list->last = delete->prev;
      if (list->last)
	{
	  list->last->next = NULL;
	  list->length = list->last->offset + list->last->size;
	}
      else
	list->length = 0;
      return;
    }
  delete->prev->next = delete->next;
  delete->next->prev = delete->prev;
}

/*
 * Try to find a given array list INDEX in the array list chunks as 
 * fast as possible and return it.
 */
static array_t *
alist_find_array (alist_t *list, unsigned long index)
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
 * Print text representation of the array list LIST. This function is 
 * for testing and debugging purposes only and should not go into any 
 * distribution.
 */
void
alist_analyse (alist_t *list)
{
  unsigned long n;
  array_t *array;

  for (n = 0, array = list->first; array; n++, array = array->next)
    {
      fprintf (stdout, 
	       "chunk %06lu at %p, ofs: %06lu, size: %02lu, fill: %08lX, "
	       "prev: %p, next %p\n",
	       n + 1, array, array->offset, array->size, array->fill, 
	       array->prev, array->next);
    }
  fprintf (stdout, "length: %lu, size: %lu, first: %p, last: %p\n", 
	   list->length, list->size, list->first, list->last);
}

/*
 * Validate the given array list LIST and print invalid array lists.
 * Passing the DESCRIPTION text you can figure out the stage an error
 * occurred. Return zero if there occurred an error otherwise non-zero.
 */
static int
alist_validate (alist_t *list, char *description)
{
  array_t *array, *next, *prev;
  unsigned long n, bits;
  int ok = 1;

  /* any valid list ? */
  assert (list);

  /* go through all the array list chunks */
  for (n = 0, array = list->first; array; n++, array = array->next)
    {
      next = array->next;
      prev = array->prev;

      /* check chain first */
      if ((!next && array != list->last) || (!prev && array != list->first))
	{
	  fprintf (stdout, "alist_validate: invalid last or first\n");
	  ok = 0;
	  break;
	}
      if ((next && next->prev != array) || (prev && prev->next != array))
	{
	  fprintf (stdout, "alist_validate: invalid next or prev\n");
	  ok = 0;
	  break;
	}

      /* check chunk size and offset */
      if (next && array->offset + array->size > next->offset)
	{
	  fprintf (stdout, "alist_validate: invalid size or offset\n");
	  ok = 0;
	  break;
	}

      /* check array chunk fill */
      bits = (1 << array->size) - 1;
      if (array->fill & ~bits || !(array->fill & ((bits + 1) >> 1)) ||
	  array->size == 0 || array->fill == 0)
	{
	  fprintf (stdout, "alist_validate: invalid chunk fill\n");
	  ok = 0;
	  break;
	}
    }

  /* check array length */
  array = list->last;
  if (array && array->offset + array->size != list->length)
    {
      fprintf (stdout, "alist_validate: invalid array length\n");
      ok = 0;
    }

  /* print out error description and array list text representation 
     if necessary */
  if (!ok)
    {
      fprintf (stdout, "error in chunk %06lu (%s)\n", n + 1,
	       description ? description : "unspecified");
      alist_analyse (list);
    }
  return ok;
}

/*
 * Construct an empty array list without initial capacity. Return the
 * newly created array list.
 */
alist_t *
alist_create (void)
{
  alist_t *list;

  assert (ARRAY_SIZE <= sizeof (unsigned long) * 8);
  list = svz_malloc (sizeof (alist_t));
  memset (list, 0, sizeof (alist_t));
  return list;
}

/*
 * Destroy the given array list completely. The argument cannot be used
 * afterwards because it is invalid.
 */
void
alist_destroy (alist_t *list)
{
#if DEVEL
  alist_validate (list, "destroy");
#endif /* DEVEL */

  alist_clear (list);
  svz_free (list);
}

/*
 * Appends the specified element VALUE at the end of the array list LIST.
 */
void
alist_add (alist_t *list, void *value)
{
  array_t *array, *last = list->last;

#if DEVEL
  alist_validate (list, "add");
#endif /* DEVEL */

  /* append an array chunk if necessary */
  if (!last || last->size == ARRAY_SIZE)
    {
      array = alist_create_array (last ? last->offset + ARRAY_SIZE : 0);
      if (last)
	{
	  last->next = array;
	  array->prev = list->last;
	}
      else
	list->first = array;
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
 * Removes all of the elements from the array list LIST. The array list
 * will be as clean as created with `alist_create ()' then.
 */
void
alist_clear (alist_t *list)
{
  array_t *next, *array = list->first;
  unsigned long length = list->length;

#if DEVEL
  alist_validate (list, "clear");
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
      if (next)
	length -= (next->offset - array->offset - array->size);
      svz_free (array);
      array = next;
    }

  /* cleanup array list */
  assert (length == 0);
  list->last = list->first = NULL;
  list->length = 0;
  list->size = 0;
}

/*
 * Returns non-zero if the array list LIST contains the specified element 
 * VALUE.
 */
unsigned long
alist_contains (alist_t *list, void *value)
{
  array_t *array = list->first;
  unsigned long n, fill, found = 0;

#if DEVEL
  alist_validate (list, "contains");
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
 * Returns the element at the specified position INDEX in the array 
 * list LIST or NULL if there is no such element.
 */
void *
alist_get (alist_t *list, unsigned long index)
{
  array_t *array;

#if DEVEL
  alist_validate (list, "get");
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
  if (!array)
    return NULL;
  index -= array->offset;
  if (array->fill & (1 << index))
    return array->value[index];
  return NULL;
}

/*
 * Searches for the first occurrence of the given argument VALUE. 
 * Return -1 if the value VALUE could not be found in the array list LIST.
 */
unsigned long
alist_index (alist_t *list, void *value)
{
  array_t *array = list->first;
  unsigned long n, fill;

#if DEVEL
  alist_validate (list, "index");
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
  return (unsigned long) -1;
}

/*
 * Removes the element at the specified position INDEX in the array 
 * list LIST and returns its previous value.
 */
void *
alist_delete (alist_t *list, unsigned long index)
{
  array_t *array, *next;
  void *value = NULL;
  unsigned long bit, idx, fill;

#if DEVEL
  char text[128];
  alist_validate (list, "delete");
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
  if (!array)
    return NULL;
  idx = index - array->offset;

  /* is there any value at the given index ? */
  if (!(array->fill & (1 << idx)))
    return NULL;

  /* delete this value */
  array->fill &= ~(1 << idx);
  list->size--;
  list->length--;

  /* adjust array chunk size */
  if (!(array->fill & ~((1 << idx) - 1)))
    {
      for (bit = 1 << idx; bit && !(array->fill & bit); bit >>= 1) 
	array->size--;
    }
  else
    array->size--;

  /* adjust array list length */
  if (array == list->last)
    list->length = array->offset + array->size;

  value = array->value[idx];

  /* release this array chunk if possible */
  if (array->size == 0)
    {
      assert (array->fill == 0);

      /* break here if the list is empty */
      if (list->size == 0)
	{
	  svz_free (array);
	  list->last = list->first = array = NULL;
	  list->length = 0;
	  return value;
	}

      /* rearrange array list */
      alist_unhook (list, array);
      next = array->next;
      svz_free (array);
      array = next;
    }

  /* shuffle value data if necessary */
  else if (idx < array->size)
    {
      /* delete a bit */
      bit = (1 << idx) - 1;
      fill = array->fill;
      array->fill = (fill & bit) | ((fill >> 1) & ~bit);
      assert (array->fill);
	      
      /* delete a value */
      memmove (&array->value[idx], &array->value[idx + 1],
	       (array->size - idx) * sizeof (void *));
    }

  /* reduce array index offset by one */
  while (array)
    {
      if (array->offset > index)
	array->offset--;
      array = array->next;
    }

#if DEVEL
  sprintf (text, "post-delete (%lu) = %p", index, value);
  alist_validate (list, "delete");
#endif /* DEVEL */

  /* return deleted value */
  return value;
}

/*
 * Removes all of the elements whose index is between FROM (inclusive) 
 * and TO (exclusive) from the array list LIST. Returns the amount of 
 * actually deleted elements.
 */
unsigned long
alist_delete_range (alist_t *list, unsigned long from, unsigned long to)
{
  unsigned long idx, n = 0;

#if DEVEL
  alist_validate (list, "delete range");
#endif /* DEVEL */

  /* swap the `to' and `from' indexes if necessary */
  if (to < from)
    {
      idx = to;
      to = from + 1;
      from = idx + 1;
    }

  /* return here if there is nothing to do */
  if (to > list->length)
    to = list->length;
  if (from > list->length)
    from = list->length;
  if (to == from)
    return 0;

  /* special case: delete all list elements */
  if (from == 0 && to == list->length)
    {
      n = list->size;
      alist_clear (list);
      return n;
    }

  /* go through the index range and delete each list item */
  for (idx = from; idx < to;)
    {
      if (alist_delete (list, idx))
	{
	  to--;
	  n++;
	}
      else
	idx++;
    }
  return n;
}

/*
 * Replaces the element at the specified position INDEX in the array 
 * list LIST by the specified element VALUE and return its previous value.
 */
void *
alist_set (alist_t *list, unsigned long index, void *value)
{
  array_t *array, *next;
  void *replace = NULL;
  unsigned long idx;

#if DEVEL
  alist_validate (list, "set");
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
	  if (idx >= array->size)
	    array->size = idx + 1;
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
  if (list->length <= next->offset)
    list->length = index + 1;

  /* return replaced value */
  return replace;
}

/*
 * Delete the element at the given position INDEX from the array list LIST 
 * but leave all following elements untouched (unlike `alist_delete ()'). 
 * Return the its previous value if there is one otherwise return NULL.
 */
void *
alist_unset (alist_t *list, unsigned long index)
{
  array_t *array;
  void *unset = NULL;
  unsigned long idx, bit;

#if DEVEL
  alist_validate (list, "unset");
#endif /* DEVEL */

  /* return if index is invalid */
  if (index >= list->length)
    return NULL;

  /* find an appropriate array chunk */
  if ((array = alist_find_array (list, index)) == NULL)
    return NULL;

  idx = index - array->offset;

  /* is there a value set ? */
  if (!(array->fill & (1 << idx)))
    return NULL;

  /* save unset value for returning it */
  unset = array->value[idx];

  /* delete this value */
  list->size--;
  array->fill &= ~(1 << idx);
  if (idx + 1 == array->size)
    for (bit = 1 << idx; bit && !(array->fill & bit); bit >>= 1)
      {
	array->size--;
	if (array == list->last)
	  list->length--;
      }
  if (array->size == 0)
    {
      alist_unhook (list, array);
      svz_free (array);
    }

  /* return unset value */
  return unset;
}

/*
 * Returns the number of elements in the array list LIST.
 */
unsigned long
alist_size (alist_t *list)
{
#if DEVEL
  alist_validate (list, "size");
#endif /* DEVEL */

  return list->size;
}

/*
 * Returns the index of the last element of the array list LIST plus one.
 */
unsigned long
alist_length (alist_t *list)
{
#if DEVEL
  alist_validate (list, "length");
#endif /* DEVEL */

  return list->length;
}

/*
 * Inserts the specified element VALUE at the given position INDEX in 
 * the array list LIST.
 */
void
alist_insert (alist_t *list, unsigned long index, void *value)
{
  array_t *array, *next;
  unsigned long idx, fill, bit;

#if DEVEL
  alist_validate (list, "insert");
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
	  if (idx >= array->size)
	    array->size = idx + 1;

	  if (idx < array->size)
	    {
	      /* insert a bit */
	      bit = (1 << idx) - 1;
	      fill = array->fill;
	      array->fill = (fill & bit) | ((fill << 1) & ~bit);

	      /* shuffle value data */
	      memmove (&array->value[idx + 1], &array->value[idx],
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
  if (++list->length < index + 1)
    list->length = index + 1;
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
 * Rearranges the given array list LIST. After that there are no more gaps 
 * within the array list. The index - value relationship gets totally lost
 * by this operation.
 */
void
alist_pack (alist_t *list)
{
  array_t *array, *next, *prev;
  unsigned long need2pack, bits, n, size;
  void **value;

#if DEVEL
  alist_validate (list, "pack");
#endif /* DEVEL */

  if (!list->size)
    return;

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
  if (!need2pack)
    return;

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
      if (!prev)
	list->first = array;
      else
	prev->next = array;
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
      if (!prev)
	list->first = array;
      else
	prev->next = array;
      array->prev = prev;
    }
  list->last = array;
  list->length = list->size;
  svz_free (value);
}

/*
 * Delivers all values within the given array list LIST in a single linear 
 * chunk. You have to `svz_free ()' it after usage.
 */
void **
alist_values (alist_t *list)
{
  array_t *array;
  void **value;
  unsigned long index, bit, n;

#if DEVEL
  alist_validate (list, "values");
#endif /* DEVEL */

  if (!list->size) 
    return NULL;

  value = svz_malloc (list->size * sizeof (void *));
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
