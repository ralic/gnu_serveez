/*
 * defines.h - useful global definitions for portability
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.  
 *
 * $Id: defines.h,v 1.5 2001/06/16 15:02:46 ela Exp $
 *
 */

#ifndef __DEFINES_H__
#define __DEFINES_H__ 1

/* Depending on the kind of build include either <config.h> (for internal
   build) or <svzconfig.h> for the external usage of the core library. */

#if defined (HAVE_CONFIG_H) && defined (__BUILD_SVZ_LIBRARY__)
# include <config.h>
#else
# include <svzconfig.h>
#endif

/* __BEGIN_DECLS should be used at the beginning of your declarations,
   so that C++ compilers don't mangle their names.  Use __END_DECLS at
   the end of C declarations. */

#undef __BEGIN_DECLS
#undef __END_DECLS
#ifdef __cplusplus
# define __BEGIN_DECLS extern "C" {
# define __END_DECLS }
#else
# define __BEGIN_DECLS
# define __END_DECLS
#endif

/* __P is a macro used to wrap function prototypes, so that compilers
   that don't understand ANSI C prototypes still work, and ANSI C
   compilers can issue warnings about type mismatches. */

#undef __P
#if defined (__STDC__) || defined (_AIX) \
        || (defined (__mips) && defined (_SYSTYPE_SVR4)) \
        || defined (__MINGW32__) || defined (_WIN32) || defined(__cplusplus)
# define __P(protos) protos
#else
# define __P(protos) ()
#endif

/* SERVEEZ_API is a macro prepended to all function and data definitions
   which should be exported or imported in the resulting dynamic link
   library in the Win32 port. */

#if defined (__SERVEEZ_EXPORT__) || defined (DLL_EXPORT)
# define SERVEEZ_API __declspec (dllexport)
#elif defined (__SERVEEZ_IMPORT__)
# define SERVEEZ_API __declspec (dllimport)
#else
# define SERVEEZ_API
#endif

/* When building the core library or any outside module on Win32 systems
   include the Winsock interface here. */

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#endif /* !__DEFINES_H__ */
