/*
 * array.h - array declarations
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
 * $Id: array.h,v 1.6 2001/04/04 22:20:02 ela Exp $
 *
 */

#ifndef __ARRAY_H__
#define __ARRAY_H__ 1

#include "libserveez/defines.h"

#if ENABLE_DEBUG

typedef struct
{
  unsigned long size;
  unsigned long capacity;
  void **data;
}
svz_array_t;

__BEGIN_DECLS

SERVEEZ_API svz_array_t * svz_array_create __P ((unsigned long capacity));
SERVEEZ_API void svz_array_clear __P ((svz_array_t *array));
SERVEEZ_API void svz_array_destroy __P ((svz_array_t *array));
SERVEEZ_API void *svz_array_get __P ((svz_array_t *array, 
				      unsigned long index));
SERVEEZ_API void *svz_array_set __P ((svz_array_t *array, unsigned long index, 
				      void *value));
SERVEEZ_API void svz_array_add __P ((svz_array_t *array, void *value));
SERVEEZ_API void *svz_array_del __P ((svz_array_t *array, 
				      unsigned long index));
SERVEEZ_API unsigned long svz_array_capacity __P ((svz_array_t *array));
SERVEEZ_API unsigned long svz_array_size __P ((svz_array_t *array));
SERVEEZ_API unsigned long svz_array_ins __P ((svz_array_t *array, 
					      unsigned long index, 
					      void *value));
SERVEEZ_API unsigned long svz_array_idx __P ((svz_array_t *array, 
					      void *value));
SERVEEZ_API unsigned long svz_array_contains __P ((svz_array_t *array, 
						   void *value));

__END_DECLS

#else /* ENABLE_DEBUG */

/* Everything via inline functions. */

typedef void * svz_array_t;

static inline svz_array_t * 
svz_array_create (unsigned long capacity)
{
  svz_array_t *array;

  if (!capacity)
    capacity = 16;
  array = svz_malloc ((2 + capacity) * sizeof (void *));
  array[0] = (void *) 0;
  array[1] = (void *) capacity;
  return array;
}

#define svz_array_clear(array) \
  (array)[0] = (void *) 0

#define svz_array_destroy(array) \
  svz_free ((array))

#define svz_array_ensure_capacity(array, size)                         \
  if ((size) > (unsigned long) (array)[1])                             \
    {                                                                  \
      (array)[1] = (void *) ((unsigned long) (array)[1] * 3 / 2 + 1);  \
      (array) = svz_realloc ((array), (2 + (unsigned long) (array)[1]) \
                             * sizeof (void *));                       \
    }                                                                  

#define svz_array_get(array, index)                                 \
  (((unsigned long) (index) >= (unsigned long) (array)[0]) ? NULL : \
    (array)[index + 2])

static inline void *
svz_array_set (svz_array_t *array, unsigned long index, void *value)
{
  void *prev;

  if (index >= (unsigned long) array[0])
    return NULL;
  prev =  array[index + 2];
  array[index + 2] = value;
  return prev;
}

#define svz_array_add(array, value)                                        \
  do {                                                                     \
    svz_array_ensure_capacity ((array), ((unsigned long) (array)[0] + 1)); \
    (array)[(unsigned long) (array)[0] + 2] = (value);                     \
    (array)[0] = (void *) ((unsigned long) (array)[0] + 1);                \
  } while (0)

static inline void *
svz_array_del (svz_array_t *array, unsigned long index)
{
  void *value;

  if (index >= (unsigned long) array[0])
    return NULL;
  value = array[index + 2];
  if (index != (unsigned long) array[0] - 1)
    memmove (&array[index + 2], &array[index + 3], 
             ((unsigned long) array[0] - index - 1) * sizeof (void *));
  array[0] = (void *) ((unsigned long) array[0] - 1);
  return value;
}

#define svz_array_capacity(array) \
  ((unsigned long) (array)[1])

#define svz_array_size(array) \
  ((unsigned long) (array)[0])

static inline unsigned long
svz_array_contains (svz_array_t *array, void *value)
{
  unsigned long n = (unsigned long) array[0], found = 0;

  while (n--)
    if (array[n + 2] == value)
      found++;
  return found;
}

static inline unsigned long
svz_array_idx (svz_array_t *array, void *value)
{
  unsigned long n;

  for (n = 0; n < (unsigned long) array[0]; n++)
    if (array[n + 2] == value)
      return n;
  return (unsigned long) -1;
}

static inline unsigned long
svz_array_ins (svz_array_t *array, unsigned long index, void *value)
{
  if (index > (unsigned long) array[0])
    return (unsigned long) -1;
  svz_array_ensure_capacity (array, (unsigned long) array[0] + 1);
  if (index < (unsigned long) array[0])
    memmove (&array[index + 3], &array[index + 2], 
	     ((unsigned long) array[0] - index) * sizeof (void *));
  array[index + 2] = value;
  array[0] = (void *) ((unsigned long) array[0] + 1);
  return index;
}

#endif /* not ENABLE_DEBUG */

/*
 * This is the iteration macro for the array implementation of the core
 * library. @var{array} specifies the array to iterate, @var{value} the
 * pointer each element of the array gets assigned and @var{i} is the
 * iteration variable.
 */
#define svz_array_foreach(array, value, i)                           \
  for ((i) = 0, (value) = array ? svz_array_get ((array), 0) : NULL; \
       array && (unsigned long) i < svz_array_size (array);          \
       (value) = svz_array_get ((array), ++(i)))

#endif /* not __ARRAY_H__ */
