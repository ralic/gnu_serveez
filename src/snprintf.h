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
 * $Id: snprintf.h,v 1.3 2000/06/16 15:36:15 ela Exp $
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
#define vsnprintf _vsnprintf
#define snprintf _snprintf
#endif

#ifndef HAVE_VSNPRINTF
# define vsnprintf(str, n, format, ap) vsprintf(str, format, ap)
#endif

/* snprintf() */

#ifndef HAVE_SNPRINTF
int snprintf(char *, size_t, const char *, ...);
#endif

#endif /* not __SNPRINTF_H__ */
