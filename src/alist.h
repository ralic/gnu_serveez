/*
 * src/alist.h - array list interface
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
 * $Id: alist.h,v 1.1 2000/10/16 18:39:49 ela Exp $
 *
 */

#ifndef __ALIST_H__
#define __ALIST_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define ARRAY_BITS 4
#define ARRAY_SIZE (1 << ARRAY_BITS)
#define ARRAY_MASK (ARRAY_SIZE - 1)

typedef struct
{
  void *next;
  unsigned fill;
  void *value[ARRAY_SIZE];
}
array_t;

typedef struct
{
  unsigned size;
  array_t *array;
}
alist_t;

#endif /* not __ALIST_H__ */



