/*
 * src/snprintf.h - (v)snprintf function interface
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
 * $Id: snprintf.h,v 1.5 2000/07/15 11:44:17 ela Exp $
 *
 */

#ifndef __SNPRINTF_H__
#define __SNPRINTF_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/* to be sure not to redefine va_start in stdarg.h */
#if defined(HAVE_VARARGS_H) && !defined(va_start)
# include <varargs.h>
#endif

/* vsnprintf() */

#ifdef __MINGW32__
/*
 * Both of these functions are actually implemented but not within the
 * B20.1 release of CygWin, but in the latest. Soo we define them here
 * ourselves.
 */
int _snprintf (char *, size_t, const char *, ...);
int _vsnprintf (char *, size_t, const char *, va_list);
# define vsnprintf _vsnprintf
# define snprintf _snprintf
#endif

#ifndef HAVE_VSNPRINTF
# define vsnprintf(str, n, format, ap) vsprintf(str, format, ap)
#endif

/* snprintf() */

#ifndef HAVE_SNPRINTF
# ifndef __STDC__
int snprintf ();
# else
int snprintf (char *, size_t, const char *, ...);
# endif
#endif

#endif /* not __SNPRINTF_H__ */
