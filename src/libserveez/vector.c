/*
 * vector.c - simple vector list implementation
 *
 * Copyright (C) 2011 Thien-Thi Nguyen
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "timidity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libserveez/alloc.h"
#include "libserveez/util.h"
#include "libserveez/vector.h"

/* Definition of an vector structure.  */
struct svz_vector
{
  unsigned long length;     /* number of elements in this vector */
  unsigned long chunk_size; /* size of each element */
  void *chunks;             /* pointer to first element */
};

/*
 * Create a new vector list without elements.  Each element will have
 * the given size @var{size} in bytes.
 */
svz_vector_t *
svz_vector_create (unsigned long size)
{
  svz_vector_t *vec;

  if (size == 0)
    return NULL;
  vec = svz_malloc (sizeof (svz_vector_t));
  memset (vec, 0, sizeof (svz_vector_t));
  vec->chunk_size = size;
  return vec;
}

/*
 * Destroy a given vector list @var{vec}.  This pointer is invalid afterwards.
 * The routine @code{svz_free}s all elements.
 */
void
svz_vector_destroy (svz_vector_t *vec)
{
  if (vec->length && vec->chunks)
    svz_free (vec->chunks);
  svz_free (vec);
}

/*
 * Add an element to the end of the given vector list @var{vec}.  Return the
 * position the element got.  @var{value} is a pointer to a chunk of the
 * vector lists chunk size.
 */
unsigned long
svz_vector_add (svz_vector_t *vec, void *value)
{
  vec->chunks = svz_realloc (vec->chunks,
                             vec->chunk_size * (vec->length + 1));
  memcpy ((char *) vec->chunks + vec->chunk_size * vec->length, value,
          vec->chunk_size);
  vec->length++;

  return (vec->length - 1);
}

/*
 * Get an vector list element of the vector list @var{vec} at the given
 * position @var{index}.  Return @code{NULL} if the index is out of range.
 */
void *
svz_vector_get (svz_vector_t *vec, unsigned long index)
{
  if (index < vec->length)
    return ((char *) vec->chunks + index * vec->chunk_size);
  return NULL;
}

/*
 * Delete the element of the vector @var{vec} at the position @var{index}.
 * Return -1 if the given index is out of range otherwise the new length
 * of the vector list.
 */
unsigned long
svz_vector_del (svz_vector_t *vec, unsigned long index)
{
  char *p;

  if (index >= vec->length)
    return (unsigned long) -1;

  /* delete all elements */
  if (vec->length == 1)
    {
      svz_free (vec->chunks);
      vec->chunks = NULL;
    }
  /* delete last element */
  else if (vec->length - 1 == index)
    {
      vec->chunks = svz_realloc (vec->chunks, vec->chunk_size * index);
    }
  /* delete element within the chunk */
  else
    {
      p = (char *) vec->chunks + vec->chunk_size * index;
      memmove ((char *) p, (char *) p + vec->chunk_size,
               (vec->length - index - 1) * vec->chunk_size);
      vec->chunks = svz_realloc (vec->chunks,
                                 vec->chunk_size * (vec->length - 1));
    }
  vec->length--;
  return vec->length;
}

/*
 * Return the current length of the vector list @var{vec}.
 */
unsigned long
svz_vector_length (svz_vector_t *vec)
{
  assert (vec);
  return vec->length;
}
