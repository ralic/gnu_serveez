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
 * $Id: array.h,v 1.1 2001/03/08 22:15:13 raimi Exp $
 *
 */

#ifndef __ARRAY_H__
#define __ARRAY_H__ 1

#include "libserveez/defines.h"

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

__END_DECLS

#endif /* not __ARRAY_H__ */
