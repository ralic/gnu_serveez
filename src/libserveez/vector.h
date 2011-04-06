/*
 * vector.h - simple vector list declarations
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

#ifndef __VECTOR_H__
#define __VECTOR_H__ 1

/* begin svzint */
#include "libserveez/defines.h"
/* end svzint */

/* begin svzint */
/*
 * Iteration macro for the vector list @var{vector}.  Each of its values
 * gets assigned to @var{value}.  The iteration variable @var{i} runs from
 * 0 to the size-1 of the vector list.
 */
#define svz_vector_foreach(vector, value, i)                            \
  for ((i) = 0, (value) = vector ? svz_vector_get ((vector), 0) : NULL; \
       vector && (unsigned long) i < svz_vector_length (vector);        \
       (value) = svz_vector_get ((vector), ++(i)))
/* end svzint */

__BEGIN_DECLS

/*
 * A vector list is an array of memory chunks with a fixed size.  It
 * holds copies of the values you added to the vector list.  When deleting
 * or inserting an element the indexes of the following elements get
 * either decremented or incremented.
 */

SBO svz_vector_t *svz_vector_create (unsigned long);
SBO void svz_vector_destroy (svz_vector_t *);
SBO unsigned long svz_vector_add (svz_vector_t *, void *);
SBO void *svz_vector_get (svz_vector_t *, unsigned long);
SBO unsigned long svz_vector_del (svz_vector_t *, unsigned long);
SBO unsigned long svz_vector_length (svz_vector_t *);

__END_DECLS

#endif /* not __VECTOR_H__ */
