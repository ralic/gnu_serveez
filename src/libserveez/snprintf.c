/*
 * snprintf.c - (v)snprintf function implementation
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
 * $Id: snprintf.c,v 1.2 2001/02/02 11:26:23 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "libserveez/snprintf.h"

#if (!defined (HAVE_SNPRINTF)) && (!defined (__MINGW32__))
/*
 * Implementation of the snprintf() if it is not defined. It uses
 * the vsnprintf() function therefore which will fall back to vsprintf()
 * if vsnprintf() does not exist.
 */
int 
snprintf (char *str, size_t n, const char *fmt, ...)
{
  int ret;
  va_list args;

  va_start (args, fmt);
  ret = vsnprintf (str, n, fmt, args);
  va_end (args);

  return ret;
}
#endif /* ! (HAVE_SNPRINTF || __MINGW32__) */
