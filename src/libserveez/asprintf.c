/*
 * asprintf.c - (v)asprintf function implementation
 *
 * Copyright (C) 2011 Thien-Thi Nguyen
 * Copyright (C) 2002, 2003 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2002 Andreas Rottmann <a.rottmann@gmx.at>
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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "libserveez/asprintf.h"
#include "libserveez/alloc.h"

/* to be sure not to redefine `va_start' in <stdarg.h> */
#if defined (SVZ_HAVE_VARARGS_H) && !defined (va_start)
# include <varargs.h>
#endif

static int
homegrown_vasprintf (char **str, const char *fmt, va_list args)
{
  size_t size = 128;
  void *mem;

  *str = NULL;
  /* Try to allocate some memory and ‘snprintf’ into it.  */
  while ((mem = svz_realloc (*str, size)))
    {
      int count = vsnprintf ((*str = mem), size, fmt, args);

      /* Success: We're done.  */
      if (-1 < count && (unsigned) count < size)
        return count;

      /* Not enough space.  Bump ‘size’ and try again.  */
      size = 0 > count
        ? 2 * size                      /* imprecisely */
        : 1 + (unsigned) count;         /* precisely */
    }

  /* Allocation failure.  */
  if (*str)
    {
      svz_free (*str);
      *str = NULL;
    }
  return -1;
}

/*
 * Implementation of @code{asprintf}.  It formats a character
 * string without knowing the actual length of it.  The routine
 * dynamically allocates buffer space via @code{svz_malloc} and
 * returns the final length of the string.  The calling function is
 * responsible to run @code{svz_free} on @var{str}.
 */
int
svz_asprintf (char **str, const char *fmt, ...)
{
  va_list args;
  int retval;

  va_start (args, fmt);
  retval = homegrown_vasprintf (str, fmt, args);
  va_end (args);

  return retval;
}
