/*
 * snprintf.c - (v)snprintf function implementation
 *
 * Copyright (C) 2000, 2001, 2003, 2004 Stefan Jahn <stefan@lkcc.org>
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

#if (!defined (SVZ_HAVE_SNPRINTF)) && (!defined (__MINGW32__))
/*
 * Implementation of the @code{snprintf()} if it is not defined. It uses
 * the @code{vsnprintf()} function therefore which will fall back to
 * @code{vsprintf()} if @code{vsnprintf()} does not exist.
 */
int
svz_snprintf (char *str, unsigned int n, svz_c_const char *fmt, ...)
{
  int ret;
  va_list args;

  va_start (args, fmt);
#if SVZ_HAVE_VSNPRINTF
  ret = vsnprintf (str, n, fmt, args);
#else
  ret = vsprintf (str, fmt, args);
#endif
  va_end (args);

  return ret;
}
#endif /* ! (SVZ_HAVE_SNPRINTF || __MINGW32__) */
