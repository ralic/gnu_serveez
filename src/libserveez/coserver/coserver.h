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
 * $Id: coserver.h,v 1.1 2001/01/28 13:24:38 ela Exp $
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
int_coserver_t;

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
int_coserver_type_t;

/* Definitions for argument list of the coserver callbacks. */
typedef void * coserver_arg_t;
#define COSERVER_ARGS 2
#define coserver_arglist_t coserver_arg_t arg0, coserver_arg_t arg1

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

#ifdef __MINGW32__
/*
 * #define's for the thread priority in Win32.
 */
#define COSERVER_THREAD_PRIORITY THREAD_PRIORITY_IDLE
#endif

/* 
 * Types of internal servers you can start as threads or processes.
 */
#define COSERVER_REVERSE_DNS 0 /* reverse DNS lookup ID */
#define COSERVER_IDENT       1 /* identification ID */
#define COSERVER_DNS         2 /* DNS lookup ID */
#define MAX_COSERVER_TYPES   3 /* number of different coservers */

#define COSERVER_BUFSIZE         256  /* buffer size for the coservers */
#define COSERVER_PACKET_BOUNDARY '\n' /* packet boundary */
#define COSERVER_ID_BOUNDARY     ':'  /* id boundary */

__BEGIN_DECLS

SERVEEZ_API extern int_coserver_type_t int_coserver_type[MAX_COSERVER_TYPES];
SERVEEZ_API extern int_coserver_t **int_coserver;
SERVEEZ_API extern int int_coservers;

/* 
 * The prototype of an internal coserver differs in Win32 and Unices.
 */
#ifdef __MINGW32__
SERVEEZ_API void coserver_loop (int_coserver_t *, socket_t);
#else /* not __MINGW32__ */
SERVEEZ_API void coserver_loop (int_coserver_t *, int, int);
#endif /* not __MINGW32__ */

#ifdef __MINGW32__

/*
 * Activate all coservers with type TYPE.
 */
SERVEEZ_API void coserver_activate (int type);

/*
 * Check if there is a valid response by an internal coserver.
 */
SERVEEZ_API void coserver_check (void);

#endif /* not __MINGW32__ */

/*
 * Wrapper to instantiate coservers at startup time.
 */
SERVEEZ_API int coserver_init (void);

/*
 * Wrapper to destroy coservers when leaving server loop.
 */
SERVEEZ_API int coserver_finalize (void);

/*
 * Stop all coservers with type TYPE.
 */
SERVEEZ_API void coserver_destroy (int type);

/*
 * Create one coserver with type TYPE.
 */
SERVEEZ_API void coserver_create (int type);

/*
 * Send a REQUEST to an internal coserver.
 */
SERVEEZ_API void coserver_send_request (int , char *, 
					coserver_handle_result_t,
					coserver_arglist_t);

/*
 * This is the internal coserver check request callback
 * which gets called if there is data within the receive
 * buffer of the coserver's socket structure.
 */
SERVEEZ_API int coserver_check_request (socket_t sock);

/*
 * This routine is called whenever the server got some data from
 * any coserver. This data has always a trailing '\n'.
 */
SERVEEZ_API int coserver_handle_request (socket_t, char *, int);

/*
 * These are the three wrappers for our existing coservers.
 */
#if ENABLE_REVERSE_LOOKUP
SERVEEZ_API void coserver_reverse_invoke (unsigned long, 
					  coserver_handle_result_t, 
					  coserver_arglist_t);
# define coserver_reverse(ip, cb, arg0, arg1)                         \
    coserver_reverse_invoke (ip, (coserver_handle_result_t) cb,       \
                             (coserver_arg_t) ((unsigned long) arg0), \
			     (coserver_arg_t) ((unsigned long) arg1))
#else /* not ENABLE_REVERSE_LOOKUP */
# define coserver_reverse(ip, cb, arg0, arg1)
#endif /* not ENABLE_REVERSE_LOOKUP */

#if ENABLE_DNS_LOOKUP
SERVEEZ_API void coserver_dns_invoke (char *, 
				      coserver_handle_result_t, 
				      coserver_arglist_t);
# define coserver_dns(host, cb, arg0, arg1)                       \
    coserver_dns_invoke (host, (coserver_handle_result_t) cb,     \
                         (coserver_arg_t) ((unsigned long) arg0), \
			 (coserver_arg_t) ((unsigned long) arg1))
#else /* not ENABLE_DNS_LOOKUP */
# define coserver_dns(host, cb, arg0, arg1)
#endif /* not ENABLE_DNS_LOOKUP */

#if ENABLE_IDENT
SERVEEZ_API void coserver_ident_invoke (socket_t, 
					coserver_handle_result_t,
					coserver_arglist_t);
# define coserver_ident(sock, cb, arg0, arg1)                       \
    coserver_ident_invoke (sock, (coserver_handle_result_t) cb,     \
                           (coserver_arg_t) ((unsigned long) arg0), \
			   (coserver_arg_t) ((unsigned long) arg1))
#else /* not ENABLE_IDENT */
# define coserver_ident(sock, cb, arg0, arg1)
#endif /* not ENABLE_IDENT */

#endif /* not __COSERVER_H__ */
