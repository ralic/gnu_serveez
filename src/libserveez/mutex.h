/*
 * mutex.h - thread mutex definitions and implementations
 *
 * Copyright (C) 2003 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: mutex.h,v 1.2 2003/06/14 14:57:59 ela Exp $
 *
 */

#ifndef __MUTEX_H__
#define __MUTEX_H__ 1

#include "libserveez/defines.h"

#if SVZ_HAVE_PTHREAD_H
# include <pthread.h>
#endif

#ifdef __MINGW32__
# include <windows.h>
#endif

#if SVZ_HAVE_THREADS

# ifdef __MINGW32__ /* Windows native */

typedef svz_t_handle svz_mutex_t;
#  define SVZ_MUTEX_INITIALIZER NULL

/* Declares a @var{mutex} object externally.  This is useful when the
   @var{mutex} object is defined in another file. */
#  define svz_mutex_declare(mutex) \
          extern svz_mutex_t mutex;
/* Defines a @var{mutex} object globally. */
#  define svz_mutex_define(mutex) \
          svz_mutex_t mutex = SVZ_MUTEX_INITIALIZER;
/* Creates and initializes the given @var{mutex} object.  The mutex is
   in an unlocked state.  The macro must be called before using
   @code{svz_mutex_lock()} or @code{svz_mutex_unlock()}.  The user
   must call @code{svz_mutex_destroy()} for each mutex created by this
   function. */
#  define svz_mutex_create(mutex) mutex = CreateMutex (NULL, FALSE, NULL)
/* Destroys the given @var{mutex} object which has been created by
   @code{svz_mutex_create()}. */
#  define svz_mutex_destroy(mutex) \
          CloseHandle (mutex); mutex = SVZ_MUTEX_INITIALIZER
/* Locks a @var{mutex} object and sets the current thread into an idle
   state if the @var{mutex} object has been currently locked by another
   thread. */
#  define svz_mutex_lock(mutex) WaitForSingleObject (mutex, INFINITE)
/* Releases the given @var{mutex} object and thereby possibly resumes
   a waiting thread calling @code{svz_mutex_lock()}. */
#  define svz_mutex_unlock(mutex) ReleaseMutex (mutex)

# else /* POSIX threads */

typedef pthread_mutex_t svz_mutex_t;
#  define SVZ_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

#  define svz_mutex_declare(mutex) extern svz_mutex_t mutex;
#  define svz_mutex_define(mutex) \
          svz_mutex_t mutex = SVZ_MUTEX_INITIALIZER;
#  define svz_mutex_create(mutex) pthread_mutex_init (&(mutex), NULL)
#  define svz_mutex_destroy(mutex) pthread_mutex_destroy (&(mutex))
#  define svz_mutex_lock(mutex) pthread_mutex_lock (&(mutex))
#  define svz_mutex_unlock(mutex) pthread_mutex_unlock (&(mutex))

# endif

#else /* !SVZ_HAVE_THREADS */

# define svz_mutex_declare(mutex)
# define svz_mutex_define(mutex)
# define svz_mutex_create(mutex)
# define svz_mutex_destroy(mutex)
# define svz_mutex_lock(mutex)
# define svz_mutex_unlock(mutex)

#endif /* SVZ_HAVE_THREADS */

#endif /* not __MUTEX_H__ */
