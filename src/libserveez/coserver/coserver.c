/*
 * coserver.c - basic internal coserver routines
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
 * $Id: coserver.c,v 1.2 2001/02/02 11:26:24 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <signal.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#ifndef __MINGW32__
# if HAVE_WAIT_H
#  include <wait.h>
# endif
# if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
# endif
# include <netinet/in.h>
#endif

#include "libserveez/snprintf.h"
#include "libserveez/alloc.h"
#include "libserveez/util.h"
#include "libserveez/hash.h"
#include "libserveez/pipe-socket.h"
#include "libserveez/server-core.h"
#include "libserveez/coserver/coserver.h"

/* coserver-TODO: include header here */

#if ENABLE_DNS_LOOKUP
# include "dns.h"
#endif

#if ENABLE_REVERSE_LOOKUP
# include "reverse-dns.h"
#endif

#if ENABLE_IDENT
# include "ident.h"
#endif

#ifdef __MINGW32__
/* define for the thread priority in Win32 */
#define COSERVER_THREAD_PRIORITY THREAD_PRIORITY_IDLE
#endif

#define COSERVER_PACKET_BOUNDARY '\n' /* packet boundary */
#define COSERVER_ID_BOUNDARY     ':'  /* id boundary */

/*
 * Both of these variables are for storing the given callbacks which get
 * called when the coservers delivered some result.
 */
static unsigned coserver_hash_id = 1;
static hash_t *coserver_hash = NULL;

/* coserver-TODO: place wrapper function here */

#if ENABLE_REVERSE_LOOKUP
/*
 * This is a wrapper function for the reverse DNS lookup coserver.
 */
void
coserver_reverse_invoke (unsigned long ip, 
			 coserver_handle_result_t cb, coserver_arglist_t)
{
  coserver_send_request (COSERVER_REVERSE_DNS, 
			 util_inet_ntoa (ip), cb, arg0, arg1);
}
#endif /* ENABLE_REVERSE_LOOKUP */

#if ENABLE_DNS_LOOKUP
/*
 * Wrapper for the DNS coserver.
 */
void
coserver_dns_invoke (char *host, 
		     coserver_handle_result_t cb, coserver_arglist_t)
{
  coserver_send_request (COSERVER_DNS, host, cb, arg0, arg1);
}
#endif /* ENABLE_DNS_LOOKUP */

#if ENABLE_IDENT
/*
 * Wrapper for the ident coserver.
 */
void
coserver_ident_invoke (socket_t sock, 
		       coserver_handle_result_t cb, coserver_arglist_t)
{
  char buffer[COSERVER_BUFSIZE];
  snprintf (buffer, COSERVER_BUFSIZE, "%s:%u:%u",
	    util_inet_ntoa (sock->remote_addr),
	    ntohs (sock->remote_port), ntohs (sock->local_port));
  coserver_send_request (COSERVER_IDENT, buffer, cb, arg0, arg1);
}
#endif /* ENABLE_IDENT */

/*
 * This structure contains the type id and the callback
 * pointer of the internal coserver routines.
 */
coserver_type_t 
coserver_type[MAX_COSERVER_TYPES] = 
{
  /* coserver-TODO: place coserver callbacks and identification here */

#if ENABLE_REVERSE_LOOKUP
  { COSERVER_REVERSE_DNS, "reverse dns", 
    reverse_dns_handle_request, 1, reverse_dns_init },
#else
  { COSERVER_REVERSE_DNS, NULL, NULL, 0, NULL },
#endif

#if ENABLE_IDENT
  { COSERVER_IDENT, "ident", 
    ident_handle_request, 1, NULL},
#else
  { COSERVER_IDENT, NULL, NULL, 0, NULL},
#endif

#if ENABLE_DNS_LOOKUP
  { COSERVER_DNS, "dns", 
    dns_handle_request, 1, NULL }
#else
  { COSERVER_DNS, NULL, NULL, 0, NULL }
#endif

};

/*
 * Internal coserver instances.
 */
coserver_t **coserver_instance = NULL;
int coserver_instances = 0;

/*
 * This routine gets the coserver hash id from a given response and 
 * cuts it from the given response buffer.
 */
static unsigned
coserver_get_id (char *response)
{
  char *p = response;
  unsigned id = 0;

  while (*p >= '0' && *p <= '9')
    {
      id *= 10;
      id += *p - '0';
      p++;
    }
  if (*p != COSERVER_ID_BOUNDARY)
    {
      log_printf (LOG_WARNING,
		  "coserver: invalid protocol character (0x%02x)\n", *p);
      return 0;
    }
  p++;
  
  while (*p != COSERVER_PACKET_BOUNDARY)
    {
      *response++ = *p++;
    }
  *response = '\0';
  return id;
}

/*
 * This function adds a given coserver hash id to the response.
 */
static void
coserver_put_id (unsigned id, char *response)
{
  char buffer[COSERVER_BUFSIZE];

  snprintf (buffer, COSERVER_BUFSIZE, "%u:%s\n", id, response);
  strcpy (response, buffer);
}

/*************************************************************************/
/*            This is part of the coserver process / thread.             */
/*************************************************************************/

/*
 * Win32:
 * coserver_loop() is the actual thread routine being an infinite loop. 
 * It MUST be resumed via ResumeThread() by the server.
 * When running it first checks if there is any request lingering
 * in the client structure "sock", reads it out, processes it
 * (can be blocking) and finally sends back a respond to the
 * server.
 *
 * Unices:
 * coserver_loop() is a infinite loop in a separate process. It reads
 * blocking from a receive pipe, processes the request and puts the
 * result to a sending pipe to the server.
 *
 * The coserver loop heavily differs in Win32 and Unices...
 */

/* Debug info Macro. */
#if ENABLE_DEBUG
# define COSERVER_REQUEST_INFO() \
  log_printf (LOG_DEBUG, "%s: coserver request occurred\n",   \
	      coserver_type[coserver->type].name);
#else
# define COSERVER_REQUEST_INFO()
#endif

/* Post-Processing Macro. */
#if ENABLE_DEBUG
# define COSERVER_RESULT() \
  log_printf (LOG_DEBUG, "%s: coserver request processed\n", \
	      coserver_type[coserver->type].name);
#else
# define COSERVER_RESULT()
#endif

/* Pre-Processing Macro. */
#define COSERVER_REQUEST()                                   \
  COSERVER_REQUEST_INFO ();                                  \
  /* Process the request here. Might be blocking indeed ! */ \
  if ((id = coserver_get_id (request)) != 0)                 \
    {                                                        \
      if ((result = coserver->callback (request)) == NULL)   \
        {                                                    \
          result = request;                                  \
          *result = '\0';                                    \
        }                                                    \
      coserver_put_id (id, result);                          \
    }                                                        \


#ifdef __MINGW32__
static void
coserver_loop (coserver_t *coserver, socket_t sock)
{
  char *p;
  int len;
  char request[COSERVER_BUFSIZE];
  char *result = NULL;
  unsigned id;

  /* wait until the thread handle has been passed */
  while (coserver->thread == INVALID_HANDLE_VALUE);

  /* infinite loop */
  for (;;)
    {
      /* check if there is anything in the receive buffer */
      while (sock->send_buffer_fill > 0)
	{
	  p = sock->send_buffer;
	  while (*p != COSERVER_PACKET_BOUNDARY && 
		 p < sock->send_buffer + sock->send_buffer_fill)
	    p++;
	  len = p - sock->send_buffer + 1;
	  
	  /* Copy the coserver request to static buffer. */
	  assert (len <= COSERVER_BUFSIZE);
	  memcpy (request, sock->send_buffer, len);

	  /* Enter a synchronized section (exclusive access to all data). */
	  EnterCriticalSection (&coserver->sync);
	  if (sock->send_buffer_fill > len)
	    {
	      memmove (sock->send_buffer, p + 1,
		       sock->send_buffer_fill - len);
	    }
	  sock->send_buffer_fill -= len;
	  LeaveCriticalSection (&coserver->sync);

	  COSERVER_REQUEST ();

	  if (id && result)
	    {
	      EnterCriticalSection (&coserver->sync);
	      memcpy (sock->recv_buffer + sock->recv_buffer_fill, 
		      result, strlen (result));
	      sock->recv_buffer_fill += strlen (result);
	      LeaveCriticalSection (&coserver->sync);
	      COSERVER_RESULT ();
	    }
	}

      /* suspend myself and wait for being resumed ... */
      if (SuspendThread (coserver->thread) == 0xFFFFFFFF)
	{
	  log_printf (LOG_ERROR, "SuspendThread: %s\n", SYS_ERROR);
	}
    }
}

#else /* not __MINGW32__ */

static void
coserver_loop (coserver_t *coserver, int in_pipe, int out_pipe)
{
  FILE *in, *out;
  char request[COSERVER_BUFSIZE];
  char *result = NULL;
  unsigned id;

  in = fdopen (in_pipe, "r");
  out = fdopen (out_pipe, "w");
  if (in == NULL || out == NULL)
    {
      log_printf (LOG_ERROR, "cannot access pipes %d or %d\n",
		  in_pipe, out_pipe);
      return;
    }

  while (NULL != fgets (request, COSERVER_BUFSIZE, in))
    {
      
      COSERVER_REQUEST ();
	  
      if (id && result)
	{
	  fprintf (out, "%s", result);
	  fflush (out);
	  COSERVER_RESULT ();
	}
    }
  
  /* error in reading pipe */
  if (fclose (in))
    log_printf (LOG_ERROR, "fclose: %s\n", SYS_ERROR);
  if (fclose (out))
    log_printf (LOG_ERROR, "fclose: %s\n", SYS_ERROR);
}

#endif /* not __MINGW32__ */

/*************************************************************************/
/*                   This is part of the server process.                 */
/*************************************************************************/

#ifdef __MINGW32__

/*
 * This routine is the actual threads callback, but calls the coserver's 
 * callback indeed. It is a wrapper routine for Win32, because you can pass 
 * only a single argument to a thread routine.
 */
static DWORD WINAPI 
coserver_thread (LPVOID thread)
{
  coserver_t *coserver;

  coserver = (coserver_t *) thread;
  coserver_loop (coserver, coserver->sock);
  ExitThread (0);

  return 0;
}

/*
 * Reactivate all specific coservers with type TYPE. In Win32
 * you have to call this if you want the coserver start working.
 */
static void
coserver_activate (int type)
{
  int n, count, res;
  coserver_t *coserver;
  
  /* go through all internal coserver threads */
  for (count = 0, n = 0; n < coserver_instances; n++)
    {
      coserver = coserver_instance[n];
      /* is this structure of the requested type ? */
      if (coserver->type == type)
	{
	  /* activated the thread */
          while ((res = ResumeThread (coserver->thread)) > 0);
          if (res == 0)
	    count++;
	}
    }

#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "%d internal %s coserver activated\n",
	      count, coserver_type[type].name);
#endif /* ENABLE_DEBUG */
}

/*
 * Call this routine whenever there is time, e.g. within the timeout of 
 * the `select ()' statement. Indeed I built it in the 
 * `server_periodic_tasks ()' statement. The routine checks if there was 
 * any response from an active coserver.
 */
void
coserver_check (void)
{
  coserver_t *coserver;
  socket_t sock;
  int n;
  
  /* go through all coservers */
  for (n = 0; n < coserver_instances; n++)
    {
      coserver = coserver_instance[n];
      sock = coserver->sock;

      while (sock->recv_buffer_fill > 0)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "%s: coserver response detected\n",
		      coserver_type[coserver->type].name);
#endif
	  /* find a full response within the receive buffer */
	  if (sock->check_request)
	    {
	      EnterCriticalSection (&coserver->sync);
	      sock->check_request (sock);
	      LeaveCriticalSection (&coserver->sync);
	    }
	}
    }
}
#endif /* __MINGW32__ */

/*
 * Delete the n'th internal coserver from coserver array.
 */
static void
coserver_delete (int n)
{
  svz_free (coserver_instance[n]);

  if (--coserver_instances != 0)
    {
      coserver_instance[n] = coserver_instance[coserver_instances];
      coserver_instance = svz_realloc (coserver_instance, 
				       sizeof (coserver_t *) * 
				       coserver_instances);
    }
  else
    {
      svz_free (coserver_instance);
      coserver_instance = NULL;
    }
}

#ifndef __MINGW32__
/*
 * Disconnects a internal coserver. This is the callback routine for the
 * socket structure entry `disconnected_socket'.
 */
static int
coserver_disconnect (socket_t sock)
{
  int n;
  coserver_t *coserver;

  for (n = 0; n < coserver_instances; n++)
    {
      coserver = coserver_instance[n];
      if (coserver->sock == sock)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, 
		      "%s: killing coserver pid %d\n",
		      coserver_type[coserver->type].name, coserver->pid);
#endif /* ENABLE_DEBUG */
	  if (kill (coserver->pid, SIGKILL) == -1)
	    log_printf (LOG_ERROR, "kill: %s\n", SYS_ERROR);
#if HAVE_WAITPID
	  /* cleanup coserver child process */
	  else if (waitpid (coserver->pid, NULL, WNOHANG) == -1)
	    log_printf (LOG_ERROR, "waitpid: %s\n", SYS_ERROR);
#endif /* HAVE_WAITPID */
	  /* re-arrange the internal coserver array */
	  coserver_delete (n);
	  break;
	}
    }
  return 0;
}
#endif /* not __MINGW32__ */

/*
 * This routine has to be called for coservers requests. It is the default 
 * `check_request ()' routine for coservers detecting full responses as 
 * lines (trailing '\n').
 */
static int
coserver_check_request (socket_t sock)
{
  char *packet = sock->recv_buffer;
  char *p = packet;
  int request_len;
  int len = 0;
  coserver_t *coserver;

  do
    {
      /* find a line (trailing '\n') */
      while (*p != COSERVER_PACKET_BOUNDARY &&
	     p < sock->recv_buffer + sock->recv_buffer_fill)	     
	p++;

      if (*p == COSERVER_PACKET_BOUNDARY && 
	  p < sock->recv_buffer + sock->recv_buffer_fill)
	{
	  coserver = sock->data;
	  assert (coserver);
	  coserver->busy--;
	  p++;
	  request_len = p - packet;
	  len += request_len;
	  if (sock->handle_request)
	    sock->handle_request (sock, packet, request_len);
	  packet = p;
	}
    }
  while (p < sock->recv_buffer + sock->recv_buffer_fill);
      
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "coserver: %d byte response\n", len);
#endif

  /* remove data from receive buffer if necessary */
  if (len > 0 && sock->recv_buffer_fill > len)
    {
      memmove (sock->recv_buffer, packet, sock->recv_buffer_fill - len);
    }
  sock->recv_buffer_fill -= len;

  return 0;
}

/*
 * The standard coserver `handle_request ()' routine is called whenever
 * the standard `check_request ()' detected a full packet by any coserver.
 */
static int
coserver_handle_request (socket_t sock, char *request, int len)
{
  int ret;
  unsigned id;
  char *p, *end, *data;
  coserver_callback_t *cb;
  
  /* Search for coserver hash id. */
  id = 0;
  p = request;
  end = p + len;
  while (*p != COSERVER_ID_BOUNDARY && p < end) 
    {
      if (*p < '0' || *p > '9')
	{
	  log_printf (LOG_WARNING, 
		      "coserver: invalid character in id (0x%02X)\n", *p);
	  return -1;
	}
      id *= 10;
      id += *p - '0';
      p++;
    }
  if (p == end)
    {
      log_printf (LOG_WARNING, 
		  "coserver: invalid coserver response (no id)\n");
      return -1;
    }
  data = ++p;

  /* Search for packet end. */
  while (*p != COSERVER_PACKET_BOUNDARY && p < end)
    p++;
  if (p == end)
    {
      log_printf (LOG_WARNING, 
		  "coserver: invalid coserver response (no data)\n");
      return -1;
    }
  *p = '\0';

  /* Have a look at the coserver callback hash. */
  if (NULL == (cb = hash_get (coserver_hash, util_itoa (id))))
    {
      log_printf (LOG_ERROR, "coserver: invalid callback for id %u\n", id);
      return -1;
    }

  /* 
   * Run the callback inclusive its arg. Second arg is either NULL for
   * error detection or the actual result string. Afterwards free the 
   * callback structure and delete it from the coserver callback hash.
   */
  ret = cb->handle_result (*data ? data : NULL, cb->arg[0], cb->arg[1]);
  hash_delete (coserver_hash, util_itoa (id));
  svz_free (cb);

  return ret;
}

/*
 * Destroy specific coservers. This works for Win32 and Unices.
 */
void
coserver_destroy (int type)
{
  int n, count;
  coserver_t *coserver;
  
  for (count = 0, n = 0; n < coserver_instances; n++)
    {
      coserver = coserver_instance[n];
      if (coserver->type == type)
	{
#ifdef __MINGW32__
	  /* stop the thread and close its handle */
	  if (!TerminateThread (coserver->thread, 0))
	    log_printf (LOG_ERROR, "TerminateThread: %s\n", SYS_ERROR);
	  if (!CloseHandle (coserver->thread))
	    log_printf (LOG_ERROR, "CloseHandle: %s\n", SYS_ERROR);
	  DeleteCriticalSection (&coserver->sync);

	  /* free all data reserved by the coserver */
	  sock_free (coserver->sock);
#else /* not __MINGW32__ */
	  if (kill (coserver->pid, SIGKILL) == -1)
	    log_printf (LOG_ERROR, "kill: %s\n", SYS_ERROR);
#if HAVE_WAITPID
	  /* cleanup coserver child process */
	  else if (waitpid (coserver->pid, NULL, WNOHANG) == -1)
	    log_printf (LOG_ERROR, "waitpid: %s\n", SYS_ERROR);
#endif /* HAVE_WAITPID */
#endif /* not __MINGW32__ */
	  coserver_delete (n);
	  n--;
	  count++;
	}
    }

#ifdef ENABLE_DEBUG
  if (count > 0)
    {
      log_printf (LOG_DEBUG, "%d internal %s coserver destroyed\n", 
		  count, coserver_type[type].name);
    }
#endif /* ENABLE_DEBUG */
}

/*
 * Start a specific internal coserver. This works for Win32 and
 * Unices. Whereas in Unix a process is `fork ()' ed and in Win32
 * a thread gets started.
 */
static socket_t
coserver_start (int type) 
{
  socket_t sock;
  coserver_t *coserver;
  
#ifndef __MINGW32__
  int s2c[2];
  int c2s[2];
  int pid;
#else /* not __MINGW32__ */
  HANDLE thread;
  DWORD tid;
#endif /* not __MINGW32__ */

  log_printf (LOG_NOTICE, "starting internal %s coserver\n", 
	      coserver_type[type].name);

  coserver_instances++;
  coserver_instance = svz_realloc (coserver_instance, 
				   sizeof (coserver_t *) * 
				   coserver_instances);

  coserver = svz_malloc (sizeof (coserver_t));
  coserver_instance[coserver_instances - 1] = coserver;
  coserver->type = type;
  coserver->busy = 0;

  /* fill in the actual coserver callback */
  coserver->callback = coserver_type[type].callback;

#ifdef __MINGW32__
  if ((sock = sock_alloc ()) == NULL)
    return NULL;

  InitializeCriticalSection (&coserver->sync);
  sock->write_socket = NULL;
  sock->read_socket = NULL;
  coserver->sock = sock;
  coserver->thread = INVALID_HANDLE_VALUE;

  if ((thread = CreateThread(
      (LPSECURITY_ATTRIBUTES) NULL, /* ignore security attributes */
      (DWORD) 0,                    /* default stack size */
      (LPTHREAD_START_ROUTINE) coserver_thread, /* thread routine */
      (LPVOID) coserver,            /* thread argument */
      (DWORD) CREATE_SUSPENDED,     /* creation flags */
      (LPDWORD) &tid)) == INVALID_HANDLE_VALUE) /* thread id */
    {
      log_printf (LOG_ERROR, "CreateThread: %s\n", SYS_ERROR);
      DeleteCriticalSection (&coserver->sync);
      sock_free (sock);
      return NULL;
    }

  /* fill in thread access variables */
  coserver->tid = tid;
  coserver->thread = thread;

  /* set thread priority */
  if (!SetThreadPriority (thread, COSERVER_THREAD_PRIORITY))
    log_printf (LOG_ERROR, "SetThreadPriority: %s\n", SYS_ERROR);

#ifdef ENABLE_DEBUG
  log_printf (LOG_DEBUG, "coserver thread id is 0x%08X\n", tid);
#endif

#else /* not __MINGW32__ */

  /* create pipes for process communication */
  if (pipe (s2c) < 0)
    {
      log_printf (LOG_ERROR, "pipe server-coserver: %s\n", SYS_ERROR);
      return NULL;
    }
  if (pipe (c2s) < 0)
    {
      close (s2c[0]);
      close (s2c[1]);
      log_printf (LOG_ERROR, "pipe coserver-server: %s\n", SYS_ERROR);
      return NULL;
    }

  /* fork() us here */
  if ((pid = fork ()) == 0)
    {
      /* close the servers pipe descriptors */
      if (close (s2c[WRITE]) < 0)
	log_printf (LOG_ERROR, "close: %s\n", SYS_ERROR);
      if (close (c2s[READ]) < 0)
	log_printf (LOG_ERROR, "close: %s\n", SYS_ERROR);

      /* start the internal coserver */
      coserver_loop (coserver, s2c[READ], c2s[WRITE]);
      exit (0);
    }
  else if (pid == -1)
    {
      log_printf (LOG_ERROR, "fork: %s\n", SYS_ERROR);
      close (s2c[READ]);
      close (s2c[WRITE]);
      close (c2s[READ]);
      close (c2s[WRITE]);
      return NULL;
    }

  /* the old server process continues here */
  
#ifdef ENABLE_DEBUG
  log_printf (LOG_DEBUG, "coserver process id is %d\n", pid);
#endif

  /* close the coservers pipe descriptors */
  if (close (s2c[READ]) < 0)
    log_printf (LOG_ERROR, "close: %s\n", SYS_ERROR);
  if (close (c2s[WRITE]) < 0)
    log_printf (LOG_ERROR, "close: %s\n", SYS_ERROR);

  if ((sock = pipe_create (c2s[READ], s2c[WRITE])) == NULL)
    {
      if (close (c2s[READ]) < 0)
	log_printf (LOG_ERROR, "close: %s\n", SYS_ERROR);
      if (close (s2c[WRITE]) < 0)
	log_printf (LOG_ERROR, "close: %s\n", SYS_ERROR);
      return NULL;
    }

  coserver->pid = pid;
  coserver->sock = sock;
  sock->disconnected_socket = coserver_disconnect;
  sock->write_socket = pipe_write_socket;
  sock->read_socket = pipe_read_socket;
  sock_enqueue (sock);

#endif /* __MINGW32__ and Unices */

  sock->data = coserver;
  sock->check_request = coserver_check_request;
  sock->handle_request = coserver_handle_request;
  sock->flags |= (SOCK_FLAG_NOFLOOD | SOCK_FLAG_COSERVER);
  return sock;
}

/*
 * Create one coserver with type TYPE.
 */
void 
coserver_create (int type)
{
  if (coserver_type[type].init)
    coserver_type[type].init ();
  coserver_start (type);
}

/*
 * Global coserver initialization. Here you should start all the internal
 * coservers you want to use later.
 */
int
coserver_init (void)
{
  int i, n;
  coserver_type_t *coserver;

  coserver_hash = hash_create (4);
  coserver_hash_id = 1;

  for (n = 0; n < MAX_COSERVER_TYPES; n++)
    {
      coserver = &coserver_type[n];
      if (coserver->init)
	coserver->init ();
      for (i = 0; i < coserver->instances; i++)
	{
	  coserver_start (coserver->type);
	}
    }

  return 0;
}

/*
 * Global coserver finalization.
 */
int
coserver_finalize (void)
{
  int n;
  coserver_callback_t **cb;
  coserver_type_t *coserver;

  for (n = 0; n < MAX_COSERVER_TYPES; n++)
    {
      coserver = &coserver_type[n];
      coserver_destroy (coserver->type);
    }

  /* `svz_free ()' all callbacks left so far. */
  if (NULL != (cb = (coserver_callback_t **) hash_values (coserver_hash)))
    {
      for (n = 0; n < hash_size (coserver_hash); n++)
	svz_free (cb[n]);
      hash_xfree (cb);
    }
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "coserver: %d callback(s) left\n",
	      hash_size (coserver_hash));
#endif
  hash_destroy (coserver_hash);
  return 0;
}

/*
 * Invoke a REQUEST for one of the running internal coservers
 * with type TYPE. HANDLE_RESULT and ARG specify what should happen
 * if the coserver delivers a result.
 */
void
coserver_send_request (int type, char *request, 
		       coserver_handle_result_t handle_result, 
		       coserver_arglist_t)
{
  int n, busy;
  coserver_t *coserver;
  coserver_callback_t *cb;
  
  /* 
   * Go through all coservers and find out which coserver 
   * type TYPE is the least busiest.
   */
  coserver = NULL;
  for (n = 0; n < coserver_instances; n++)
    {
      if (coserver_instance[n]->type == type)
	{
	  if (coserver == NULL)
	    { 
	      coserver = coserver_instance[n];
	      busy = coserver->busy;
	    }
	  else if (coserver_instance[n]->busy <= coserver->busy)
	    {
	      coserver = coserver_instance[n];
	      busy = coserver->busy;
	    }
	}
    }

  /* found this an appropriate coserver */
  if (coserver)
    {
      /*
       * Create new callback hash entry for this coserver request and
       * put it into the global coserver callback hash.
       */
      cb = svz_malloc (sizeof (coserver_callback_t));
      cb->handle_result = handle_result;
      cb->arg[0] = arg0;
      cb->arg[1] = arg1;
      hash_put (coserver_hash, util_itoa (coserver_hash_id), cb);

      coserver->busy++;
#ifdef __MINGW32__
      EnterCriticalSection (&coserver->sync);
#endif /* __MINGW32__ */
      if (sock_printf (coserver->sock, "%u:%s\n", coserver_hash_id, request))
	{
	  sock_schedule_for_shutdown (coserver->sock);
	}
      coserver_hash_id++;
#ifdef __MINGW32__
      LeaveCriticalSection (&coserver->sync);
      coserver_activate (coserver->type);
#endif /* __MINGW32__ */
    }
}
