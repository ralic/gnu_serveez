/*
 * guile-server.c - guile server modules
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: guile-server.c,v 1.30 2001/11/10 17:45:11 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_GUILE_SERVER

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#if GUILE_SOURCE
# include <libguile/gh.h>
#else
# include <guile/gh.h>
#endif

#include "libserveez.h"
#include "guile.h"
#include "guile-bin.h"
#include "guile-server.h"

/* The guile server hash. */
static svz_hash_t *guile_server = NULL;

/* The guile socket hash. */
static svz_hash_t *guile_sock = NULL;

/* List of functions to configure. */
static char *guile_functions[] = {
  "global-init", "init", "detect-proto", "connect-socket", "finalize",
  "global-finalize", "info-client", "info-server", "notify", 
  "handle-request", NULL };

/* If set to zero exception handling is disabled. */
static int guile_use_exceptions = 1;

/*
 * Creates a Guile SMOB (small object). The @var{cfunc} specifies a base 
 * name for all defined functions. The argument @var{description} is used
 * in the printer function. The macro creates various function to operate
 * on a SMOB:
 * a) creator - Creates a new instance.
 * b) getter  - Converts a scheme cell to the C structure.
 * c) checker - Checks if the scheme cell represents this SMOB.
 * d) printer - Used when applying (display . args) in Guile.
 * e) init    - Initialization of the SMOB.
 * f) tag     - The new scheme tag used to identify a SMOB.
 */
#define MAKE_SMOB_DEFINITION(cfunc, description)                             \
long guile_##cfunc##_tag = 0;                                                \
SCM guile_##cfunc##_create (void *data) {                                    \
  SCM value;                                                                 \
  SCM_NEWSMOB (value, guile_##cfunc##_tag, data);                            \
  return value;                                                              \
}                                                                            \
void * guile_##cfunc##_get (SCM smob) {                                      \
  return (void *) SCM_SMOB_DATA (smob);                                      \
}                                                                            \
int guile_##cfunc##_p (SCM smob) {                                           \
  return (SCM_NIMP (smob) &&                                                 \
          SCM_TYP16 (smob) == guile_##cfunc##_tag) ? 1 : 0;                  \
}                                                                            \
int guile_##cfunc##_print (SCM smob, SCM port, scm_print_state *state) {     \
  static char txt[256];                                                      \
  sprintf (txt, "#<%s %p>", description, (void *) SCM_SMOB_DATA (smob));     \
  scm_puts (txt, port);                                                      \
  return 1;                                                                  \
}                                                                            \
void guile_##cfunc##_init (void) {                                           \
  guile_##cfunc##_tag = scm_make_smob_type (description, 0);                 \
  scm_set_smob_print (guile_##cfunc##_tag, guile_##cfunc##_print);           \
}

#define INIT_SMOB(cfunc) \
  guile_##cfunc##_init ()

#define MAKE_SMOB(cfunc, data) \
  guile_##cfunc##_create (data)

#define CHECK_SMOB(cfunc, smob) \
  guile_##cfunc##_p (smob)

#define CHECK_SMOB_ARG(cfunc, smob, arg, description, var)        \
  do {                                                            \
    if (!CHECK_SMOB (cfunc, smob))                                \
      scm_wrong_type_arg_msg (FUNC_NAME, arg, smob, description); \
    else                                                          \
      var = GET_SMOB (cfunc, smob);                               \
  } while (0)

#define GET_SMOB(cfunc, smob) \
  guile_##cfunc##_get (smob)

#define INIT_SMOB_MARKER(cfunc) \
  scm_set_smob_mark (guile_##cfunc##_tag, guile_##cfunc##_gc)

/* This macro creates a socket callback getter/setter for use in Guile.
   The procedure returns any previously set callback or an undefined value. */
#define MAKE_SOCK_CALLBACK(func, assoc)                                \
static SCM guile_sock_##func (SCM sock, SCM proc) {                    \
  svz_socket_t *xsock;                                                 \
  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);    \
  if (!gh_eq_p (proc, SCM_UNDEFINED)) {                                \
    SCM_ASSERT_TYPE (gh_procedure_p (proc), proc, SCM_ARG2, FUNC_NAME, \
		     "procedure");                                     \
    xsock->##func = guile_func_##func;                                 \
    return guile_sock_setfunction (xsock, assoc, proc); }              \
  return guile_sock_getfunction (xsock, assoc);                        \
}

/* Provides a socket callback setter/getter. */
#define DEFINE_SOCK_CALLBACK(assoc, func) \
  gh_new_procedure (assoc, guile_sock_##func, 1, 1, 0)

MAKE_SMOB_DEFINITION (svz_socket, "svz-socket")
MAKE_SMOB_DEFINITION (svz_server, "svz-server")
MAKE_SMOB_DEFINITION (svz_servertype, "svz-servertype")

/* Garbage collector function for guile servertype definitions. */
static SCM
guile_svz_servertype_gc (SCM server)
{
  return SCM_BOOL_F;
}

/* Garbage collector function for guile socket structures. */
static SCM
guile_svz_socket_gc (SCM socket)
{
  return SCM_BOOL_F;
}

/* Garbage collector function for guile server definitions. */
static SCM
guile_svz_server_gc (SCM server)
{
  return SCM_BOOL_F;
}

/*
 * Extract a guile procedure from an option hash. Return zero on success.
 */
static int
optionhash_extract_proc (svz_hash_t *hash,
			 char *key,        /* the key to find       */
			 int hasdef,       /* if there is a default */
			 SCM defvar,       /* default               */
			 SCM *target,      /* where to put it       */
			 char *txt)        /* appended to error     */
{
  SCM proc, hvalue = optionhash_get (hash, key);
  int err = 0;
  char *str = NULL;

  /* Is there such a string in the option-hash ? */
  if (gh_eq_p (hvalue, SCM_UNSPECIFIED))
    {
      /* Nothing in hash, try to use default. */
      if (hasdef)
	*target = defvar;
      else
	{
	  guile_error ("No default procedure for `%s' %s", key, txt);
	  err = 1;
	}
      return err;
    }

  /* Is that guile procedure ? */
  if (gh_procedure_p (hvalue))
    {
      *target = hvalue;
    }
  else if ((str = guile_to_string (hvalue)) != NULL)
    {
      proc = gh_lookup (str);
      if (!gh_eq_p (proc, SCM_UNDEFINED) && gh_procedure_p (proc))
	*target = proc;
      else
	{
	  guile_error ("No such procedure `%s' for `%s' %s", str, key, txt);
	  err = 1;
	}
      scm_must_free (str);
    }
  else
    {
      guile_error ("Invalid procedure for `%s' %s", key, txt);
      err = 1;
    }
  return err;
}

/*
 * Add another server type @var{server} to the list of known server types.
 * Each server type is associated with a number of functions which is also
 * a hash associating function names with its guile procedures.
 */
static void
guile_servertype_add (svz_servertype_t *server, svz_hash_t *functions)
{
  svz_hash_put (functions, "server-type", server);
  svz_hash_put (guile_server, server->prefix, functions);
  svz_servertype_add (server);
}

/*
 * Lookup a functions name @var{func} in the list of known servertypes.
 * The returned guile procedure depends on the given server type 
 * @var{server}. If the lookup fails this routine return SCM_UNDEFINED.
 */
static SCM
guile_servertype_getfunction (svz_servertype_t *server, char *func)
{
  svz_hash_t *gserver;
  SCM proc;

  if (server == NULL || guile_server == NULL)
    return SCM_UNDEFINED;

  if ((gserver = svz_hash_get (guile_server, server->prefix)) == NULL)
    return SCM_UNDEFINED;

  if ((proc = (SCM) SVZ_PTR2NUM (svz_hash_get (gserver, func))) == 0)
    return SCM_UNDEFINED;
  
  return proc;
}

/*
 * Return the procedure @var{func} associated with the socket structure
 * @var{sock} or SCM_UNDEFINED if there is no such function yet.
 */
static SCM
guile_sock_getfunction (svz_socket_t *sock, char *func)
{
  svz_hash_t *gsock;
  SCM proc;

  if (sock == NULL || guile_sock == NULL)
    return SCM_UNDEFINED;

  if (svz_sock_find (sock->id, sock->version) != sock)
    return SCM_UNDEFINED;

  if ((gsock = svz_hash_get (guile_sock, svz_itoa (sock->id))) == NULL)
    return SCM_UNDEFINED;

  if ((proc = (SCM) SVZ_PTR2NUM (svz_hash_get (gsock, func))) == 0)
    return SCM_UNDEFINED;
  
  return proc;
}

/*
 * Associate the given guile procedure @var{proc} hereby named @var{func}
 * with the socket structure @var{sock}. The function returns the previously
 * set procedure if there is such a.
 */
static SCM
guile_sock_setfunction (svz_socket_t *sock, char *func, SCM proc)
{
  svz_hash_t *gsock;
  SCM oldproc;

  if (sock == NULL || func == NULL)
    return SCM_UNDEFINED;

  if ((gsock = svz_hash_get (guile_sock, svz_itoa (sock->id))) == NULL)
    {
      gsock = svz_hash_create (4);
      svz_hash_put (guile_sock, svz_itoa (sock->id), gsock);
    }

  /* Put guile procedure into socket hash and protect it. Removes old
     guile procedure and unprotects it. */
  scm_protect_object (proc);
  oldproc = (SCM) SVZ_PTR2NUM (svz_hash_put (gsock, func, SVZ_NUM2PTR (proc)));
  if (oldproc == 0)
    return SCM_UNDEFINED;

  scm_unprotect_object (oldproc);
  return oldproc;
}

/*
 * Return an integer. If the given guile cell @var{value} is not such an 
 * integer the routine return the default value @var{def}.
 */
static int
guile_integer (SCM value, int def)
{
  if (gh_exact_p (value))
    return gh_scm2int (value);
  return def;
}

/*
 * This procedure can be used to schedule Serveez for shutdown within Guile.
 * Serveez will shutdown all network connections and terminate after the next
 * event loop. You should use this instead of issuing @code{(quit)}.
 */
#define FUNC_NAME "serveez-nuke"
SCM
guile_nuke_happened (void)
{
  svz_nuke_happened = 1;
  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

/*
 * Controls the use of exceptions handlers for the Guile procedure calls
 * of Guile server callbacks. If the optional argument @var{enable} set to 
 * @code{#t} exception handling is enabled and if set to @code{#f} 
 * exception handling is disabled. The procedure always returns the 
 * current value of this behaviour.
 */
#define FUNC_NAME "serveez-exceptions"
SCM
guile_access_exceptions (SCM enable)
{
  SCM value = gh_bool2scm (guile_use_exceptions);
  int n;

  if (!gh_eq_p (enable, SCM_UNDEFINED))
    {
      if (guile_to_boolean (enable, &n))
	guile_error ("%s: Invalid boolean value", FUNC_NAME);
      else 
	guile_use_exceptions = n;
    }
  return value;
}
#undef FUNC_NAME

/*
 * The @code{guile_call()} function puts the procedure to call including
 * the arguments to it into a single scheme cell passed to this function
 * in @var{data}. The functions unpacks this cell and applies it to
 * @code{scm_apply()}.
 * By convention the @var{data} argument cell consists of three items chained
 * like this: @code{(procedure first-argument (remaining-argument-list))}
 */
static SCM
guile_call_body (SCM data)
{
  return scm_apply (gh_car (data), 
		    gh_car (gh_cdr (data)), gh_cdr (gh_cdr (data)));
}

/*
 * This is the exception handler for calls by @code{guile_call()}. Prints
 * the procedure (passed in @var{data}), the name of the exception and the
 * error message if possible.
 */
static SCM
guile_call_handler (SCM data, SCM tag, SCM args)
{
  char *str = guile_to_string (tag);

  scm_puts ("exception in ", scm_current_error_port ());
  scm_display (data, scm_current_error_port ());
  scm_puts (" due to `", scm_current_error_port ());
  scm_puts (str, scm_current_error_port ());
  scm_puts ("'\n", scm_current_error_port ());
  scm_puts ("guile-error: ", scm_current_error_port ());

  /* on quit/exit */
  if (gh_null_p (args))
    {
      scm_display (tag, scm_current_error_port ());
      scm_puts ("\n", scm_current_error_port ());
      return SCM_BOOL_F;
    }

  if (!gh_eq_p (gh_car (args), SCM_BOOL_F))
    {
      scm_display (gh_car (args), scm_current_error_port ());
      scm_puts (": ", scm_current_error_port ());
    }
  scm_display_error_message (gh_car (gh_cdr (args)), 
			     gh_car (gh_cdr (gh_cdr (args))), 
			     scm_current_error_port ());
  return SCM_BOOL_F;
}

/*
 * The following function takes an arbitrary number of arguments (specified
 * in @var{args}) passed to @code{scm_apply()} calling the guile procedure 
 * @var{code}. The function catches exceptions occurring in the procedure 
 * @var{code}. On success (no exception) the routine returns the value 
 * returned by @code{scm_apply()} otherwise @code{SCM_BOOL_F}.
 */
static SCM
guile_call (SCM code, int args, ...)
{
  va_list list;
  SCM body_data, handler_data;
  SCM arg = SCM_EOL, arglist = SCM_EOL, ret;

  /* Setup arg and arglist correctly for use with scm_apply(). */
  va_start (list, args);
  if (args > 0)
    {
      arg = va_arg (list, SCM);
      while (--args)
	arglist = gh_cons (va_arg (list, SCM), arglist);
      arglist = gh_cons (gh_reverse (arglist), SCM_EOL);
    }
  va_end (list);

  /* Put both arguments and the procedure together into a single argument
     for the catch body. */
  body_data = gh_cons (code, gh_cons (arg, arglist));
  handler_data = code;

  /* Use exception handling if requested. */
  if (guile_use_exceptions)
    {
      ret = gh_catch (SCM_BOOL_T,
		      (scm_catch_body_t) guile_call_body, 
		      (void *) body_data, 
		      (scm_catch_handler_t) guile_call_handler, 
		      (void *) handler_data);
    }
  else
    {
      ret = guile_call_body (body_data);
    }

  return ret;
}

/* Wrapper function for the global initialization of a server type. */
static int
guile_func_global_init (svz_servertype_t *stype)
{
  SCM global_init = guile_servertype_getfunction (stype, "global-init");
  SCM ret;

  if (!gh_eq_p (global_init, SCM_UNDEFINED))
    {
      ret = guile_call (global_init, 1, MAKE_SMOB (svz_servertype, stype));
      return guile_integer (ret, -1);
    }
  return 0;
}

/* Wrapper function for the initialization of a server instance. */
static int
guile_func_init (svz_server_t *server)
{
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM init = guile_servertype_getfunction (stype, "init");
  SCM ret;

  if (!gh_eq_p (init, SCM_UNDEFINED))
    {
      ret = guile_call (init, 1, MAKE_SMOB (svz_server, server));
      return guile_integer (ret, -1);
    }
  return 0;
}

/* Wrapper routine for protocol detection of a server instance. */
static int
guile_func_detect_proto (svz_server_t *server, svz_socket_t *sock)
{
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM detect_proto = guile_servertype_getfunction (stype, "detect-proto");
  SCM ret;

  if (!gh_eq_p (detect_proto, SCM_UNDEFINED))
    {
      ret = guile_call (detect_proto, 2, MAKE_SMOB (svz_server, server), 
			MAKE_SMOB (svz_socket, sock));
      return guile_integer (ret, 0);
    }
  return 0;
}

/* Destroys the given hash containing the guile procedure callbacks
   associated with a socket structure. */
static void
guile_sock_clear_callbacks (svz_hash_t *callbacks)
{
  int n;
  SCM *proc;

  svz_hash_foreach_value (callbacks, proc, n)
    scm_unprotect_object (proc[n]);
  svz_hash_destroy (callbacks);
}

/* Free the socket boundary if set by guile. */
static void
guile_sock_clear_boundary (svz_socket_t *sock)
{
  if (sock->boundary)
    {
      scm_must_free (sock->boundary);
      sock->boundary = NULL;
    }
  sock->boundary_size = 0;
}

/* Wrapper for the socket disconnected callback. Used here in order to
   delete the additional guile callbacks associated with the disconnected
   socket structure. */
static int
guile_func_disconnected_socket (svz_socket_t *sock)
{
  SCM ret, disconnected = guile_sock_getfunction (sock, "disconnected");
  int retval = -1;
  svz_hash_t *gsock;

  /* First call the guile callback if necessary. */
  if (!gh_eq_p (disconnected, SCM_UNDEFINED))
    {
      ret = guile_call (disconnected, 1, MAKE_SMOB (svz_socket, sock));
      retval = guile_integer (ret, -1);
    }

  /* Delete all the associated guile callbacks and unprotect these. */
  if ((gsock = svz_hash_delete (guile_sock, svz_itoa (sock->id))) != NULL)
    guile_sock_clear_callbacks (gsock);

  /* Release associated guile object is necessary. */
  if (sock->data != NULL)
    scm_unprotect_object ((SCM) SVZ_PTR2NUM (sock->data));

  /* Free the socket boundary if set by guile. */
  guile_sock_clear_boundary (sock);

  return retval;
}

/* Wrapper function for the socket connection after successful detection. */
static int
guile_func_connect_socket (svz_server_t *server, svz_socket_t *sock)
{
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM connect_socket = guile_servertype_getfunction (stype, "connect-socket");
  SCM ret;

  /* Setup this function for later use. */
  sock->disconnected_socket = guile_func_disconnected_socket;

  if (!gh_eq_p (connect_socket, SCM_UNDEFINED))
    {
      ret = guile_call (connect_socket, 2, MAKE_SMOB (svz_server, server), 
			MAKE_SMOB (svz_socket, sock));
      return guile_integer (ret, 0);
    }
  return 0;
}

/* Wrapper for the finalization of a server instance. */
static int
guile_func_finalize (svz_server_t *server)
{
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM ret, finalize = guile_servertype_getfunction (stype, "finalize");
  int i, retval = 0;
  svz_hash_t *state;
  void **object;

  if (!gh_eq_p (finalize, SCM_UNDEFINED))
    {
      ret = guile_call (finalize, 1, MAKE_SMOB (svz_server, server));
      retval = guile_integer (ret, -1);
    }

  /* Release associated guile server state objects is necessary. */
  if ((state = server->data) != NULL)
    {
      svz_hash_foreach_value (state, object, i)
	scm_unprotect_object ((SCM) SVZ_PTR2NUM (object[i]));
      svz_hash_destroy (state);
      server->data = NULL;
    }

  return retval;
}

/* Wrapper routine for the global finalization of a server type. */
static int
guile_func_global_finalize (svz_servertype_t *stype)
{
  SCM ret, global_finalize;
  global_finalize = guile_servertype_getfunction (stype, "global-finalize");

  if (!gh_eq_p (global_finalize, SCM_UNDEFINED))
    {
      ret = guile_call (global_finalize, 1, MAKE_SMOB (svz_servertype, stype));
      return guile_integer (ret, -1);
    }
  return 0;
}

/* Wrapper for the client info callback. */
static char *
guile_func_info_client (svz_server_t *server, svz_socket_t *sock)
{
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM info_client = guile_servertype_getfunction (stype, "info-client");
  SCM ret;

  if (!gh_eq_p (info_client, SCM_UNDEFINED))
    {
      ret = guile_call (info_client, 2, MAKE_SMOB (svz_server, server),
			MAKE_SMOB (svz_socket, sock));
      return guile_to_string (ret);
    }
  return NULL;
}

/* Wrapper for the server info callback. */
static char *
guile_func_info_server (svz_server_t *server)
{
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM info_server = guile_servertype_getfunction (stype, "info-server");
  SCM ret;

  if (!gh_eq_p (info_server, SCM_UNDEFINED))
    {
      ret = guile_call (info_server, 1, MAKE_SMOB (svz_server, server));
      return guile_to_string (ret);
    }
  return NULL;
}

/* Wrapper for the server notifier callback. */
static int
guile_func_notify (svz_server_t *server)
{
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM ret, notify = guile_servertype_getfunction (stype, "notify");

  if (!gh_eq_p (notify, SCM_UNDEFINED))
    {
      ret = guile_call (notify, 1, MAKE_SMOB (svz_server, server));
      return guile_integer (ret, -1);
    }
  return -1;
}

/* Wrapper for the socket check request callback. */
static int
guile_func_check_request (svz_socket_t *sock)
{
  SCM ret, check_request;
  check_request = guile_sock_getfunction (sock, "check-request");

  if (!gh_eq_p (check_request, SCM_UNDEFINED))
    {
      ret = guile_call (check_request, 1, MAKE_SMOB (svz_socket, sock));
      return guile_integer (ret, -1);
    }
  return -1;
}

/* Wrapper for the socket handle request callback. The function searches for
   both the servertype specific and socket specific procedure. */
static int
guile_func_handle_request (svz_socket_t *sock, char *request, int len)
{
  svz_server_t *server;
  svz_servertype_t *stype;
  SCM ret, handle_request;
  handle_request = guile_sock_getfunction (sock, "handle-request");

  if (gh_eq_p (handle_request, SCM_UNDEFINED))
    {
      server = svz_server_find (sock->cfg);
      stype = svz_servertype_find (server);
      handle_request = guile_servertype_getfunction (stype, "handle-request");
    }

  if (!gh_eq_p (handle_request, SCM_UNDEFINED))
    {
      ret = guile_call (handle_request, 3, MAKE_SMOB (svz_socket, sock), 
			guile_data_to_bin (request, len), gh_int2scm (len));
      return guile_integer (ret, -1);
    }
  return -1;
}

/* Set the @code{handle-request} member of the socket structure @var{sock} 
   to the Guile procedure @var{proc}. The procedure returns the previously
   set handler if there is any. */
#define FUNC_NAME "svz:sock:handle-request"
MAKE_SOCK_CALLBACK (handle_request, "handle-request")
#undef FUNC_NAME

/* Set the @code{check-request} member of the socket structure @var{sock} 
   to the Guile procedure @var{proc}. Returns the previously handler if 
   there is any. */
#define FUNC_NAME "svz:sock:check-request"
MAKE_SOCK_CALLBACK (check_request, "check-request")
#undef FUNC_NAME

/* Setup the packet boundary of the socket @var{sock}. The given string value
   @var{boundary} can contain any kind of data. If you pass a exact number 
   value the socket is setup to parse fixed sized packets. For instance you 
   can setup Serveez to pass your @code{handle-request} procedure text lines 
   by calling @code{(svz:sock:boundary sock "\\n")}. */
#define FUNC_NAME "svz:sock:boundary"
static SCM
guile_sock_boundary (SCM sock, SCM boundary)
{
  svz_socket_t *xsock;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  SCM_ASSERT_TYPE (gh_exact_p (boundary) || gh_string_p (boundary), 
		   boundary, SCM_ARG2, FUNC_NAME, "string or exact");

  /* Release previously set boundaries. */
  guile_sock_clear_boundary (xsock);

  /* Setup for fixed sized packets. */
  if (gh_exact_p (boundary))
    {
      xsock->boundary = NULL;
      xsock->boundary_size = gh_scm2int (boundary);
    }
  /* Handle packet delimiters. */
  else
    {
      xsock->boundary = gh_scm2chars (boundary, NULL);
      xsock->boundary_size = gh_scm2int (scm_string_length (boundary));
    }
  xsock->check_request = svz_sock_check_request;

  return SCM_BOOL_T;
}
#undef FUNC_NAME

/* Set or unset the flood protection bit of the given socket @var{sock}.
   Returns the previous value of this bit (#t or #f). The @var{flag}
   argument must be either boolean or an exact number and is optional. */
#define FUNC_NAME "svz:sock:floodprotect"
static SCM
guile_sock_floodprotect (SCM sock, SCM flag)
{
  svz_socket_t *xsock;
  int flags;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  flags = xsock->flags;
  if (!gh_eq_p (flag, SCM_UNDEFINED))
    {
      SCM_ASSERT_TYPE (gh_boolean_p (flag) || gh_exact_p (flag), 
		       flag, SCM_ARG2, FUNC_NAME, "boolean or exact");
      if ((gh_boolean_p (flag) && gh_scm2bool (flag) != 0) ||
	  (gh_exact_p (flag) && gh_scm2int (flag) != 0))
	xsock->flags &= ~SOCK_FLAG_NOFLOOD;
      else
	xsock->flags |= SOCK_FLAG_NOFLOOD;
    }
  return (flags & SOCK_FLAG_NOFLOOD) ? SCM_BOOL_F : SCM_BOOL_T;
}
#undef FUNC_NAME

/* Write the string buffer @var{buffer} to the socket @var{sock}. The
   procedure accepts binary smobs too. Return @code{#t} on success and 
   @code{#f} on failure. */
#define FUNC_NAME "svz:sock:print"
static SCM
guile_sock_print (SCM sock, SCM buffer)
{
  svz_socket_t *xsock;
  char *buf;
  int len, ret = -1;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  SCM_ASSERT_TYPE (gh_string_p (buffer) || guile_bin_check (buffer), 
		   buffer, SCM_ARG2, FUNC_NAME, "string or binary");

  if (gh_string_p (buffer))
    {
      buf = (char *) SCM_VELTS (buffer);
      len = gh_scm2int (scm_string_length (buffer));
    }
  else
    {
      buf = guile_bin_to_data (buffer, &len);
    }

  /* Depending on the protcol type use different kind of senders. */
  if (xsock->proto & (PROTO_TCP | PROTO_PIPE))
    ret = svz_sock_write (xsock, buf, len);
  else if (xsock->proto & PROTO_UDP)
    ret = svz_udp_write (xsock, buf, len);
  else if (xsock->proto & PROTO_ICMP)
    ret = svz_icmp_write (xsock, buf, len);
  
  if (ret == -1)
    {
      svz_sock_schedule_for_shutdown (xsock);
      return SCM_BOOL_F;
    }
  return SCM_BOOL_T;
}
#undef FUNC_NAME

/* Associate any kind of data (any Guile object) given in the argument
   @var{data} with the socket @var{sock}. The @var{data} argument is
   optional. The procedure always returns a previously stored value or an 
   empty list. */
#define FUNC_NAME "svz:sock:data"
SCM
guile_sock_data (SCM sock, SCM data)
{
  svz_socket_t *xsock;
  SCM ret = SCM_EOL;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);

  /* Save return value here. */
  if (xsock->data != NULL)
    ret = (SCM) SVZ_PTR2NUM (xsock->data);

  /* Replace associated guile cell and unprotect previously stored cell
     if necessary. */
  if (!gh_eq_p (data, SCM_UNDEFINED))
    {
      if (xsock->data != NULL)
	scm_unprotect_object (ret);
      xsock->data = SVZ_NUM2PTR (data);
      scm_protect_object (data);
    }
  return ret;
}
#undef FUNC_NAME

/*
 * Convert the given value at @var{address} of the the type @var{type} into
 * a scheme cell.
 */
static SCM
guile_config_convert (void *address, int type)
{
  svz_portcfg_t *port;
  SCM ret = SCM_EOL;

  switch (type)
    {
    case SVZ_ITEM_INT:
      ret = gh_int2scm (*(int *) address);
      break;
    case SVZ_ITEM_INTARRAY:
      ret = guile_intarray_to_guile (*(svz_array_t **) address);
      break;
    case SVZ_ITEM_STR:
      ret = gh_str02scm (*(char **) address);
      break;
    case SVZ_ITEM_STRARRAY:
      ret = guile_strarray_to_guile (*(svz_array_t **) address);
      break;
    case SVZ_ITEM_HASH:
      ret = guile_hash_to_guile (*(svz_hash_t **) address);
      break;
    case SVZ_ITEM_PORTCFG:
      if ((port = *(svz_portcfg_t **) address) != NULL)
	ret = gh_str02scm (port->name);
      break;
    case SVZ_ITEM_BOOL:
      ret = gh_bool2scm (*(int *) address);
      break;
    }
  return ret;
}

/* Checks if the given Guile object @var{smob} in position @var{arg} is a 
   server or socket and throws an exception if not. Otherwise it saves the
   server in the variable @var{var}. */
#define CHECK_SERVER_SMOB_ARG(smob, arg, var)                                \
  do {                                                                       \
    SCM_ASSERT_TYPE (CHECK_SMOB (svz_server, smob) ||                        \
		     CHECK_SMOB (svz_socket, smob), smob, arg,               \
		     FUNC_NAME, "svz-server or svz-socket");                 \
    var = CHECK_SMOB (svz_server, smob) ? GET_SMOB (svz_server, smob) :      \
      svz_server_find (((svz_socket_t *) GET_SMOB (svz_socket, smob))->cfg); \
  } while (0)

/* This procedure returns the configuration item specified by @var{key} of
   the given server instance @var{server}. You can pass this function a
   socket too. In this case the procedure will lookup the appropriate server
   instance itself. If the given string @var{key} is invalid (not defined 
   in the configuration alist in @code{(define-servertype!)}) then it returns
   an empty list. */
#define FUNC_NAME "svz:server:config-ref"
SCM
guile_server_config_ref (SCM server, SCM key)
{
  SCM ret = SCM_EOL;
  svz_servertype_t *stype;
  svz_server_t *xserver;
  int i;
  void *cfg, *address;
  char *str;

  CHECK_SERVER_SMOB_ARG (server, SCM_ARG1, xserver);
  SCM_ASSERT_TYPE (gh_string_p (key), key, SCM_ARG2, FUNC_NAME, "string");

  str = guile_to_string (key);
  stype = svz_servertype_find (xserver);
  cfg = xserver->cfg;

  for (i = 0; stype->items[i].type != SVZ_ITEM_END; i++)
    {
      if (strcmp (stype->items[i].name, str) == 0)
	{
	  address = (void *) ((unsigned long) cfg + 
			      (unsigned long) stype->items[i].address - 
			      (unsigned long) stype->prototype_start);
	  ret = guile_config_convert (address, stype->items[i].type);
	  break;
	}
    }
  scm_must_free (str);
  return ret;
}
#undef FUNC_NAME

/* Returns the Guile object associated with the string value @var{key} which
   needs to be set via @code{(svz:server:state-set!)} previously. Otherwise
   the return value is an empty list. The given @var{server} argument must be
   either a valid @code{#<svz-server>} object or a @code{#<svz-socket>}. */
#define FUNC_NAME "svz:server:state-ref"
SCM
guile_server_state_ref (SCM server, SCM key)
{
  SCM ret = SCM_EOL;
  svz_server_t *xserver;
  char *str;
  void *val;
  svz_hash_t *hash;

  CHECK_SERVER_SMOB_ARG (server, SCM_ARG1, xserver);
  SCM_ASSERT_TYPE (gh_string_p (key), key, SCM_ARG2, FUNC_NAME, "string");
  str = guile_to_string (key);

  if ((hash = xserver->data) != NULL)
    if ((val = svz_hash_get (hash, str)) != NULL)
      ret = (SCM) SVZ_PTR2NUM (val);
  return ret;
}
#undef FUNC_NAME

/* Associates the Guile object @var{value} with the string @var{key}. The
   given @var{server} argument can be both, a @code{#<svz-server>} or a 
   @code{#<svz-socket>}. Returns the previously associated object or an
   empty list if there was no such association. This procedure is useful 
   for server instance state savings. */
#define FUNC_NAME "svz:server:state-set!"
SCM
guile_server_state_set_x (SCM server, SCM key, SCM value)
{
  SCM ret = SCM_EOL;
  svz_server_t *xserver;
  char *str;
  void *val;
  svz_hash_t *hash;

  CHECK_SERVER_SMOB_ARG (server, SCM_ARG1, xserver);
  SCM_ASSERT_TYPE (gh_string_p (key), key, SCM_ARG2, FUNC_NAME, "string");
  str = guile_to_string (key);

  if ((hash = xserver->data) == NULL)
    {
      hash = svz_hash_create (4);
      xserver->data = hash;
    }
  if ((val = svz_hash_put (hash, str, SVZ_NUM2PTR (value))) != NULL)
    {
      ret = (SCM) SVZ_PTR2NUM (val);
      scm_unprotect_object (ret);
    }
  scm_protect_object (value);
  return ret;
}
#undef FUNC_NAME

/* Converts the @var{server} instance's state into a Guile hash. 
   Returns an empty list if there is no such state yet. */
#define FUNC_NAME "svz:server:state->hash"
SCM
guile_server_state_to_hash (SCM server)
{
  SCM hash = SCM_EOL;
  svz_hash_t *data;
  svz_server_t *xserver;
  int i;
  char **key;

  CHECK_SERVER_SMOB_ARG (server, SCM_ARG1, xserver);
  if ((data = xserver->data) != NULL)
    {
      hash = gh_make_vector (gh_int2scm (svz_hash_size (data)), SCM_EOL);
      svz_hash_foreach_key (data, key, i)
	scm_hash_set_x (hash, gh_str02scm (key[i]),
			(SCM) SVZ_PTR2NUM (svz_hash_get (data, key[i])));
    }
  return hash;
}
#undef FUNC_NAME

/*
 * Returns the length of a configuration item type, updates the configuration
 * item structure @var{item} and increases the @var{size} value if the
 * text representation @var{str} fits one of the item types understood by
 * Serveez. Returns zero if there is no such type.
 */
static int
guile_servertype_config_type (char *str, svz_key_value_pair_t *item, int *size)
{
  int n;
  struct {
    char *key;
    int size;
    int type;
  }
  config_types[] = {
    { "integer", sizeof (int), SVZ_ITEM_INT },
    { "intarray", sizeof (svz_array_t *), SVZ_ITEM_INTARRAY },
    { "string", sizeof (char *), SVZ_ITEM_STR },
    { "strarray", sizeof (svz_array_t *), SVZ_ITEM_STRARRAY },
    { "hash", sizeof (svz_hash_t *), SVZ_ITEM_HASH },
    { "portcfg", sizeof (svz_portcfg_t *), SVZ_ITEM_PORTCFG },
    { "boolean", sizeof (int), SVZ_ITEM_BOOL },
    { NULL, 0, -1 }
  };

  for (n = 0; config_types[n].key != NULL; n++)
    {
      if (strcmp (str, config_types[n].key) == 0)
	{
	  item->type = config_types[n].type;
	  *size += config_types[n].size;
	  return config_types[n].size;
	}
    }
  return 0;
}

/*
 * Release the default configuration items of a servertype defined
 * in Guile. This is necessary because each of these items is dynamically
 * allocated if defined in Guile.
 */
static void
guile_servertype_config_free (svz_servertype_t *server)
{
  int n;

  for (n = 0; server->items[n].type != SVZ_ITEM_END; n++)
    svz_free (server->items[n].name);
  svz_config_free (server, server->prototype_start);
  svz_free (server->items);
}

#if ENABLE_DEBUG
/*
 * Debug helper: Display a text representation of the configuration items
 * of a guile servertype.
 */
static void
guile_servertype_config_print (svz_servertype_t *server)
{
  int n, i;
  svz_array_t *array;
  svz_hash_t *hash;
  svz_portcfg_t *port;
  char **key;

  fprintf (stderr, "Configuration of `%s':\n", server->prefix);
  for (n = 0; server->items[n].type != SVZ_ITEM_END; n++)
    {
      fprintf (stderr, " * %s `%s' is %sdefaultable\n", 
	       SVZ_ITEM_TEXT (server->items[n].type),
	       server->items[n].name, server->items[n].defaultable ?
	       "" : "not ");
      if (server->items[n].defaultable)
	{
	  fprintf (stderr, "   Default value: ");
	  switch (server->items[n].type)
	    {
	    case SVZ_ITEM_INT:
	      fprintf (stderr, "%d", *(int *) server->items[n].address);
	      break; 
	    case SVZ_ITEM_INTARRAY:
	      array = *(svz_array_t **) server->items[n].address;
	      fprintf (stderr, "( ");
	      for (i = 0; i < (int) svz_array_size (array); i++)
		fprintf (stderr, "%ld ", (long) svz_array_get (array, i));
	      fprintf (stderr, ")");
	      break;
	    case SVZ_ITEM_STR:
	      fprintf (stderr, "%s", *(char **) server->items[n].address);
	      break;
	    case SVZ_ITEM_STRARRAY:
	      array = *(svz_array_t **) server->items[n].address;
	      fprintf (stderr, "( ");
	      for (i = 0; i < (int) svz_array_size (array); i++)
		fprintf (stderr, "`%s' ", (char *) svz_array_get (array, i));
	      fprintf (stderr, ")");
	      break;
	    case SVZ_ITEM_HASH:
	      hash = *(svz_hash_t **) server->items[n].address;
	      fprintf (stderr, "( ");
	      svz_hash_foreach_key (hash, key, i)
		fprintf (stderr, "(%s => %s) ", key[i], 
			 (char *) svz_hash_get (hash, key[i]));
	      fprintf (stderr, ")");
	      break;
	    case SVZ_ITEM_PORTCFG:
	      port = *(svz_portcfg_t **) server->items[n].address;
	      svz_portcfg_print (port, stderr);
	      break;
	    case SVZ_ITEM_BOOL:
	      fprintf (stderr, "%d", *(int *) server->items[n].address);
	      break;
	  }
	  fprintf (stderr, " at %p\n", server->items[n].address);
	}
    }
}
#endif /* ENABLE_DEBUG */

/*
 * Obtain a default value from the scheme cell @var{value}. The configuration
 * item type is specified by @var{type}. The default value is stored then at
 * @var{address}. Returns zero on success.
 */
static int
guile_servertype_config_default (svz_servertype_t *server, SCM value, 
				 void *address, int len, int type, char *key)
{
  int err = 0, n;
  char *str, *txt;
  svz_array_t *array;
  svz_hash_t *hash;
  svz_portcfg_t *port, *dup;

  switch (type)
    {
      /* Integer. */
    case SVZ_ITEM_INT:
      if (guile_to_integer (value, &n) != 0)
	{
	  guile_error ("%s: Invalid integer value for `%s'", 
		       server->prefix, key);
	  err = -1;
	}
      else
	memcpy (address, &n, len);
      break;

      /* Array of integers. */
    case SVZ_ITEM_INTARRAY:
      if ((array = guile_to_intarray (value, key)) == NULL)
	err = -1;
      else
	memcpy (address, &array, len);
      break;

      /* Character string. */
    case SVZ_ITEM_STR:
      if ((str = guile_to_string (value)) == NULL)
	{
	  guile_error ("%s: Invalid string value for `%s'", 
		       server->prefix, key);
	  err = -1;
	}
      else
	{
	  txt = svz_strdup (str);
	  memcpy (address, &txt, len);
	  scm_must_free (str);
	}
      break;

      /* Array of character strings. */
    case SVZ_ITEM_STRARRAY:
      if ((array = guile_to_strarray (value, key)) == NULL)
	err = -1;
      else
	memcpy (address, &array, len);
      break;

      /* Hash. */
    case SVZ_ITEM_HASH:
      if ((hash = guile_to_hash (value, key)) == NULL)
	err = -1;
      else
	memcpy (address, &hash, len);
      break;

      /* Port configuration. */
    case SVZ_ITEM_PORTCFG:
      if ((str = guile_to_string (value)) == NULL)
	{
	  guile_error ("%s: Invalid string value for `%s'",
		       server->prefix, key);
	  err = -1;
	}
      else if ((port = svz_portcfg_get (str)) == NULL)
	{
	  guile_error ("%s: No such port configuration: `%s'", 
		       server->prefix, str);
	  scm_must_free (str);
	  err = -1;
	}
      else
	{
	  scm_must_free (str);
	  dup = svz_portcfg_dup (port);
	  memcpy (address, &dup, len);
	}
      break;

      /* Boolean value. */
    case SVZ_ITEM_BOOL:
      if (guile_to_boolean (value, &n) != 0)
	{
	  guile_error ("%s: Invalid boolean value for `%s'", 
		       server->prefix, key);
	  err = -1;
	}
      else
	memcpy (address, &n, sizeof (int));
      break;

      /* Invalid type. */
    default:
      err = -1;
    }
  return err;
}

/*
 * Parse the configuration of the server type @var{server} stored in the
 * scheme cell @var{cfg}.
 */
static int
guile_servertype_config (svz_servertype_t *server, SCM cfg)
{
  int def, n, err = 0;
  char *txt, **key;
  svz_hash_t *options = NULL;
  svz_key_value_pair_t item;
  svz_key_value_pair_t *items = NULL;
  char *prototype = NULL;
  int size = 0, len;

  txt = svz_malloc (256);
  svz_snprintf (txt, 256, "parsing configuration of `%s'", server->prefix);

  /* Check if the configuration alist is given or not. */
  if (gh_eq_p (cfg, SCM_UNSPECIFIED))
    {
      guile_error ("Missing servertype `configuration' for `%s'", 
		   server->prefix);
      FAIL ();
    }

  /* Try parsing this alist is valid. */
  if (NULL == (options = guile_to_optionhash (cfg, txt, 0)))
    FAIL (); /* Message already emitted. */

  /* Check the servertype configuration definition for duplicates. */
  err |= optionhash_validate (options, 1, "configuration", server->prefix);
  
  /* Now check all configuration items. */
  svz_hash_foreach_key (options, key, n)
    {
      SCM list = optionhash_get (options, key[n]);
      SCM value;
      char *str;

      /* Each configuration item must be a scheme list with three elements. */
      if (!gh_list_p (list) || gh_length (list) != 3)
	{
	  guile_error ("Invalid definition for `%s' %s", key[n], txt);
	  err = -1;
	  continue;
	}
      
      /* Assign address offset. */
      item.address = SVZ_NUM2PTR (size);

      /* First appears the type of item. */
      value = gh_car (list);
      if ((str = guile_to_string (value)) == NULL)
	{
	  guile_error ("Invalid type definition for `%s' %s", key[n], txt);
	  err = -1;
	  continue;
	}
      else if ((len = guile_servertype_config_type (str, &item, &size)) == 0)
	{
	  guile_error ("Invalid type for `%s' %s", key[n], txt);
	  err = -1;
	  continue;
	}

      /* Then appears a boolean value specifying if the configuration 
	 item is defaultable or not. */
      list = gh_cdr (list);
      value = gh_car (list);
      if (guile_to_boolean (value, &def) != 0)
	{
	  guile_error ("Invalid defaultable value for `%s' %s", key[n], txt);
	  err = -1;
	  continue;
	}
      else if (def)
	item.defaultable = SVZ_ITEM_DEFAULTABLE;
      else
	item.defaultable = SVZ_ITEM_NOTDEFAULTABLE;

      /* Finally the default value itself. */
      list = gh_cdr (list);
      value = gh_car (list);
      prototype = svz_realloc (prototype, size);
      memset (prototype + size - len, 0, len);
      if (item.defaultable == SVZ_ITEM_DEFAULTABLE)
	{
	  err |= guile_servertype_config_default (server, value, 
						  prototype + size - len,
						  len, item.type, key[n]);
	}

      /* Increase the number of configuration items. */
      item.name = svz_strdup (key[n]);
      items = svz_realloc (items, sizeof (svz_key_value_pair_t) * (n + 1));
      memcpy (&items[n], &item, sizeof (svz_key_value_pair_t));
    }

  /* Append the last configuration item identifying the end of the
     configuration item list. */
  n = svz_hash_size (options);
  items = svz_realloc (items, sizeof (svz_key_value_pair_t) * (n + 1));
  item.type = SVZ_ITEM_END;
  item.address = NULL;
  item.defaultable = 0;
  item.name = NULL;
  memcpy (&items[n], &item, sizeof (svz_key_value_pair_t));

  /* Adjust the address values of the configuration items and assign
     all gathered information to the given servertype. */
  for (n = 0; n < svz_hash_size (options); n++)
    items[n].address = (void *) ((unsigned long) items[n].address + 
      (unsigned long) prototype);
  server->prototype_start = prototype;
  server->prototype_size = size;
  server->items = items;

#if 0
  guile_servertype_config_print (server);
#endif

 out:
  optionhash_destroy (options);
  svz_free (txt);
  return err;
}

/*
 * Guile server definition: This procedure takes one argument containing
 * the information about a new server type. If everything works fine you
 * have a freshly registered server type afterwards. Return #t on success.
 */
#define FUNC_NAME "define-servertype!"
SCM
guile_define_servertype (SCM args)
{
  int n, err = 0;
  char *txt;
  svz_hash_t *options;
  SCM proc;
  svz_servertype_t *server;
  svz_hash_t *functions;

  server = svz_calloc (sizeof (svz_servertype_t));
  txt = svz_malloc (256);
  svz_snprintf (txt, 256, "defining servertype");

  if (NULL == (options = guile_to_optionhash (args, txt, 0)))
    FAIL (); /* Message already emitted. */

  /* Obtain the servertype prefix variable (Mandatory). */
  if (optionhash_extract_string (options, "prefix", 0, NULL, 
				 &server->prefix, txt) != 0)
    FAIL ();
  svz_snprintf (txt, 256, "defining servertype `%s'", server->prefix);
  
  /* Check the servertype definition once. */
  err |= optionhash_validate (options, 1, "servertype", server->prefix);
  
  /* Get the description of the server type. */
  err |= optionhash_extract_string (options, "description", 0, NULL, 
				    &server->description, txt);

  /* Set the procedures. */
  functions = svz_hash_create (4);
  for (n = 0; guile_functions[n] != NULL; n++)
    {
      err |= optionhash_extract_proc (options, guile_functions[n],
				      1, SCM_UNDEFINED, &proc, txt);
      svz_hash_put (functions, guile_functions[n], SVZ_NUM2PTR (proc));
      if (!gh_eq_p (proc, SCM_UNDEFINED))
	scm_protect_object (proc);
    }

  /* Check the configuration items for this servertype. */
  err |= guile_servertype_config (server, 
				  optionhash_get (options, "configuration"));

  if (!err)
    {
      server->global_init = guile_func_global_init;
      server->init = guile_func_init;
      server->detect_proto = guile_func_detect_proto;
      server->connect_socket = guile_func_connect_socket;
      server->finalize = guile_func_finalize;
      server->global_finalize = guile_func_global_finalize;
      server->info_client = guile_func_info_client;
      server->info_server = guile_func_info_server;
      server->notify = guile_func_notify;
      server->handle_request = guile_func_handle_request;
      guile_servertype_add (server, functions);
    }
  else
    {
      svz_free (server);
      svz_hash_destroy (functions);
    }

 out:
  optionhash_destroy (options);
  svz_free (txt);
  return err ? SCM_BOOL_F : SCM_BOOL_T;
}
#undef FUNC_NAME

#include "guile-api.c"

/*
 * Initialization of the guile server module. Should be run before calling
 * @code{gh_eval_file()}. It registers some new guile procedures and creates
 * some static data.
 */
void
guile_server_init (void)
{
  if (guile_server || guile_sock)
    return;

  guile_server = svz_hash_create (4);
  guile_sock = svz_hash_create (4);

  gh_new_procedure ("define-servertype!", 
		    guile_define_servertype, 1, 0, 0);
  gh_new_procedure ("svz:sock:boundary", 
		    guile_sock_boundary, 2, 0, 0);
  gh_new_procedure ("svz:sock:floodprotect", 
		    guile_sock_floodprotect, 1, 1, 0);
  gh_new_procedure ("svz:sock:print", 
		    guile_sock_print, 2, 0, 0);
  gh_new_procedure ("svz:sock:data", 
		    guile_sock_data, 1, 1, 0);
  gh_new_procedure ("svz:server:config-ref", 
		    guile_server_config_ref, 2, 0, 0);
  gh_new_procedure ("svz:server:state-ref",
		    guile_server_state_ref, 2, 0, 0);
  gh_new_procedure ("svz:server:state-set!",
		    guile_server_state_set_x, 3, 0, 0);
  gh_new_procedure ("svz:server:state->hash",
		    guile_server_state_to_hash, 1, 0, 0);
  gh_new_procedure ("serveez-exceptions",
		    guile_access_exceptions, 0, 1, 0);
  gh_new_procedure ("serveez-nuke",
		    guile_nuke_happened, 0, 0, 0);
  DEFINE_SOCK_CALLBACK ("svz:sock:handle-request", handle_request);
  DEFINE_SOCK_CALLBACK ("svz:sock:check-request", check_request);

  /* Initialize the guile SMOB things. Previously defined via 
     MAKE_SMOB_DEFINITION (). */
  INIT_SMOB (svz_socket);
  INIT_SMOB (svz_server);
  INIT_SMOB (svz_servertype);
  
  /* Setup garbage collector marking functions. */
  INIT_SMOB_MARKER (svz_socket);
  INIT_SMOB_MARKER (svz_server);
  INIT_SMOB_MARKER (svz_servertype);

  guile_bin_init ();
  guile_api_init ();
}

/* Destroys the server type hash containing all callbacks associated with
   the returned server type. */
static svz_servertype_t *
guile_servertype_clear_callbacks (svz_hash_t *callbacks)
{
  svz_servertype_t *server;
  SCM *proc;
  int n;

  server = svz_hash_get (callbacks, "server-type");
  svz_hash_foreach_value (callbacks, proc, n)
    {
      if (proc[n] != (SCM) server && !gh_eq_p (proc[n], SCM_UNDEFINED))
	scm_unprotect_object (proc[n]);
    }
  svz_hash_destroy (callbacks);
  return server;
}

/*
 * This function should be called before shutting down the core library in
 * order to avoid memory leaks. It releases the server types defined with
 * guile.
 */
void
guile_server_finalize (void)
{
  char **prefix, **id;
  int i;
  svz_servertype_t *stype;
  svz_hash_t *functions;

  guile_api_finalize ();

  if (guile_server)
    {
      svz_hash_foreach_key (guile_server, prefix, i)
	{
	  functions = svz_hash_get (guile_server, prefix[i]);
	  stype = guile_servertype_clear_callbacks (functions);
	  svz_free (stype->prefix);
	  svz_free (stype->description);
	  guile_servertype_config_free (stype);
	  svz_free (stype);
	}
      svz_hash_destroy (guile_server);
      guile_server = NULL;
    }

  if (guile_sock)
    {
      svz_hash_foreach_key (guile_sock, id, i)
	{
	  functions = svz_hash_get (guile_sock, id[i]);
	  guile_sock_clear_callbacks (functions);
	}
      svz_hash_destroy (guile_sock);
      guile_sock = NULL;
    }
}

#else /* not ENABLE_GUILE_SERVER */

static int have_guile_server = 0;

#endif /* ENABLE_GUILE_SERVER */
