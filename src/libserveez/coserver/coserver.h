/*
 * coserver.h - internal coserver header definitions
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: coserver.h,v 1.3 2001/02/04 11:48:52 ela Exp $
 *
 */

#ifndef __COSERVER_H__
#define __COSERVER_H__ 1

#include "libserveez/defines.h"
#include "libserveez/socket.h"

/*
 * Every invoked internal coserver has got such a structure.
 * It contains all the data it needs to run properly.
 */
typedef struct 
{
#ifdef __MINGW32__

  /* Win32 specific part. */
  CRITICAL_SECTION sync;        /* critical section handle */
  HANDLE thread;                /* the thread handle for access */
  DWORD tid;                    /* internal thread id */

#else /* not __MINGW32__ */

  /* Unix specific part. */
  int pid;                      /* process id */

#endif /* not __MINGW32__ */

  char * (* callback) (char *); /* callback routine, blocking... */
  socket_t sock;                /* socket structure for this coserver */
  int type;                     /* coserver type id */
  int busy;                     /* is this thread currently busy ? */
}
coserver_t;

/*
 * This structure contains the type id and the callback
 * pointer of the internal coserver routines where CALLBACK is
 * the actual (blocking) processing routine.
 */
typedef struct 
{
  int type;                       /* coserver type id */
  char *name;                     /* name of the internal coserver */
  char * (* callback) (char *);   /* coserver callback */
  int instances;                  /* the amount of coserver instances */
  void (* init) (void);           /* coserver initialization routine */
}
coserver_type_t;

/* Definitions for argument list of the coserver callbacks. */
typedef void * coserver_arg_t;
#define COSERVER_ARGS 2
#define coserver_arglist_t coserver_arg_t arg0, coserver_arg_t arg1

/* Buffer size for the coservers. */
#define COSERVER_BUFSIZE 256

/*
 * The callback structure is used to finally execute some code
 * which should be called whenever one of the coserver's produces
 * any data for the server.
 */
typedef int (* coserver_handle_result_t) (char *, coserver_arglist_t);

typedef struct
{
  coserver_handle_result_t handle_result; /* any code callback */
  coserver_arg_t arg[COSERVER_ARGS];      /* argument array for this routine */
}
coserver_callback_t;

/* 
 * Types of internal servers you can start as threads or processes.
 */
#define COSERVER_REVERSE_DNS 0 /* reverse DNS lookup ID */
#define COSERVER_IDENT       1 /* identification ID */
#define COSERVER_DNS         2 /* DNS lookup ID */
#define MAX_COSERVER_TYPES   3 /* number of different coservers */

__BEGIN_DECLS

SERVEEZ_API extern coserver_type_t coserver_type[MAX_COSERVER_TYPES];
SERVEEZ_API extern coserver_t **coserver_instance;
SERVEEZ_API extern int coserver_instances;

#ifdef __MINGW32__

SERVEEZ_API void coserver_check __P ((void));

#endif /* not __MINGW32__ */

SERVEEZ_API int coserver_init __P ((void));
SERVEEZ_API int coserver_finalize __P ((void));
SERVEEZ_API void coserver_destroy __P ((int type));
SERVEEZ_API void coserver_create __P ((int type));
SERVEEZ_API void coserver_send_request __P ((int, char *, 
					     coserver_handle_result_t,
					     coserver_arglist_t));

/*
 * These are the three wrappers for our existing coservers.
 */
SERVEEZ_API void coserver_reverse_invoke __P ((unsigned long, 
					       coserver_handle_result_t, 
					       coserver_arglist_t));
# define coserver_reverse(ip, cb, arg0, arg1)                         \
    coserver_reverse_invoke (ip, (coserver_handle_result_t) cb,       \
                             (coserver_arg_t) ((unsigned long) arg0), \
			     (coserver_arg_t) ((unsigned long) arg1))

SERVEEZ_API void coserver_dns_invoke __P ((char *, 
					   coserver_handle_result_t, 
					   coserver_arglist_t));
# define coserver_dns(host, cb, arg0, arg1)                       \
    coserver_dns_invoke (host, (coserver_handle_result_t) cb,     \
                         (coserver_arg_t) ((unsigned long) arg0), \
			 (coserver_arg_t) ((unsigned long) arg1))

SERVEEZ_API void coserver_ident_invoke __P ((socket_t, 
					     coserver_handle_result_t,
					     coserver_arglist_t));
# define coserver_ident(sock, cb, arg0, arg1)                       \
    coserver_ident_invoke (sock, (coserver_handle_result_t) cb,     \
                           (coserver_arg_t) ((unsigned long) arg0), \
			   (coserver_arg_t) ((unsigned long) arg1))

#endif /* not __COSERVER_H__ */
