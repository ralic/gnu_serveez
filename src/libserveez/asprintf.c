/*
 * asprintf.c - (v)asprintf function implementation
 *
 * Copyright (C) 2002 Andreas Rottmann <a.rottmann@gmx.at>
 * Copyright (C) 2002, 2003 Stefan Jahn <stefan@lkcc.org>
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

#include "libserveez/snprintf.h"
#include "libserveez/asprintf.h"
#include "libserveez/alloc.h"

/*
 * Implementation of @code{asprintf()}.  The function uses
 * @code{svz_vasprintf()}.  It can be used to format a character
 * string without knowing the actual length of it.  The routine
 * dynamically allocates buffer space via @code{svz_malloc()} and
 * returns the final length of the string.  The calling function is
 * responsible to run @code{svz_free()} on @var{str}.
 */
int
svz_asprintf (char **str, svz_c_const char *fmt, ...)
{
  va_list args;
  int retval;
  
  va_start (args, fmt);
  retval = svz_vasprintf (str, fmt, args);
  va_end (args);

  return retval;
}

/*
 * This function is used by @code{svz_asprintf()} and is meant to be a
 * helper function only.
 */
int
svz_vasprintf (char **str, svz_c_const char *fmt, va_list args)
{
  int size = 128; /* guess we need no more than 128 characters of space */
  int nchars;
  
  *str = (char *) svz_realloc (*str, size);

  /* Try to print in the allocated space. */
  nchars = svz_vsnprintf (*str, size, fmt, args);
  while (nchars >= size || nchars <= -1)
    {
      /* Reallocate buffer. */
      size = (nchars <= -1) ? (nchars + 1) : size * 2;
      *str = (char *) svz_realloc (*str, size);
      /* Try again. */
      nchars = svz_vsnprintf (*str, size, fmt, args);
    }
  
  return nchars;
}
