/*
 * guile-server.c - guile server modules
 *
 * Copyright (C) 2011 Thien-Thi Nguyen
 * Copyright (C) 2001, 2002, 2003, 2004 Stefan Jahn <stefan@lkcc.org>
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

#include "config.h"

#if ENABLE_GUILE_SERVER

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#if HAVE_FLOSS_H
# include <floss.h>
#endif
#if !defined HAVE_GUILE_GH_H
# include <libguile.h>
#else
# include <guile/gh.h>
#endif

#include "networking-headers.h"
#include "action.h"
#include "libserveez.h"
#include "gi.h"
#include "guile-api.h"
#include "guile.h"
#include "guile-bin.h"
#include "guile-server.h"

#define _CTYPE(ctype,x)     guile_ ## ctype ## _ ## x
#define NAME_TAG(ctype)     _CTYPE (ctype, tag)
#define NAME_PRED(ctype)    _CTYPE (ctype, p)
#define NAME_CREATE(ctype)  _CTYPE (ctype, create)
#define NAME_FREE(ctype)    _CTYPE (ctype, free)
#define NAME_PRINT(ctype)   _CTYPE (ctype, print)
#define NAME_GET(ctype)     _CTYPE (ctype, get)
#define NAME_INIT(ctype)    _CTYPE (ctype, init)

#define integer_else(obj, def)                  \
  (SCM_EXACTP (obj)                             \
   ? gi_scm2int (obj)                           \
   : def)

/* The guile server type hash.  */
static svz_hash_t *guile_server = NULL;

/* The guile socket hash.  */
static svz_hash_t *guile_sock = NULL;

/* List of procedures to configure for a server type.  */
static char *guile_functions[] = {
  "global-init", "init", "detect-proto", "connect-socket", "finalize",
  "global-finalize", "info-client", "info-server", "notify", "reset",
  "handle-request", NULL };

/* If set to zero exception handling is disabled.  */
static int guile_use_exceptions = 1;

/*
 * Depending on the smob implementation of Guile we use different functions
 * in order to create a new smob tag.  It is also necessary to apply a smob
 * `free' function for older Guile versions because it is called
 * unconditionally and has no reasonable default function.
 */

#if HAVE_OLD_SMOBS
  /* Guile 1.3 backward compatibility code.  */
# define CREATE_SMOB_TAG(ctype, description)                               \
  static scm_smobfuns guile_funs = {                                       \
    NULL, NAME_FREE (ctype),                                               \
    NAME_PRINT (ctype), NULL };                                            \
  NAME_TAG (ctype) = scm_newsmob (&guile_funs);
#else
  /* Create new smob data type and assign a printer function.  */
# define CREATE_SMOB_TAG(ctype, description)                               \
  NAME_TAG (ctype) = scm_make_smob_type (description, 0);                  \
  scm_set_smob_print (NAME_TAG (ctype), NAME_PRINT (ctype));
#endif

/*
 * Creates a Guile SMOB (small object).  The @var{ctype} specifies a base
 * name for all defined functions.  The argument @var{description} is used
 * in the printer function.  The macro creates various function to operate
 * on a SMOB:
 * a) creator - Creates a new instance.
 * b) getter  - Converts a scheme cell to the C structure.
 * c) checker - Checks if the scheme cell represents this SMOB.
 * d) printer - Used when applying (display . args) in Guile.
 * e) init    - Initialization of the SMOB type
 * f) tag     - The new scheme tag used to identify a SMOB.
 * g) free    - Run if the SMOB gets destroyed.
 */
#define MAKE_SMOB_DEFINITION(ctype, description)                             \
static scm_t_bits NAME_TAG (ctype) = 0;                                      \
static SCM NAME_CREATE (ctype) (void *data) {                                \
  SCM_RETURN_NEWSMOB (NAME_TAG (ctype), data);                               \
}                                                                            \
static void * NAME_GET (ctype) (SCM smob) {                                  \
  return (void *) SCM_SMOB_DATA (smob);                                      \
}                                                                            \
static int NAME_PRED (ctype) (SCM smob) {                                    \
  return (SCM_NIMP (smob) &&                                                 \
          SCM_TYP16 (smob) == NAME_TAG (ctype)) ? 1 : 0;                     \
}                                                                            \
static int NAME_PRINT (ctype) (SCM smob, SCM port,                           \
                               SVZ_UNUSED scm_print_state *state) {          \
  static char txt[256];                                                      \
  sprintf (txt, "#<%s %p>", description, (void *) SCM_SMOB_DATA (smob));     \
  scm_puts (txt, port);                                                      \
  return 1;                                                                  \
}                                                                            \
static size_t NAME_FREE (ctype) (SVZ_UNUSED SCM smob) {                      \
  return 0;                                                                  \
}                                                                            \
static void NAME_INIT (ctype) (void) {                                       \
  CREATE_SMOB_TAG (ctype, description);                                      \
}

/* Initializer macro for a new smob type.  */
#define INIT_SMOB(ctype) NAME_INIT (ctype) ()

/* Instantiating macro for a smob type.  */
#define MAKE_SMOB(ctype, data) NAME_CREATE (ctype) (data)

/* Checks if the given scheme cell is a smob or not.  */
#define CHECK_SMOB(ctype, smob) NAME_PRED (ctype) (smob)

/* Extracts the smob data from a given smob cell.  */
#define GET_SMOB(ctype, smob) NAME_GET (ctype) (smob)

/* Checks if the scheme cell @var{smob} is a smob and throws an error if
   not.  Otherwise the variable @var{var} receives the smob data.  */
#define CHECK_SMOB_ARG(ctype, smob, arg, description, var) do { \
  if (!CHECK_SMOB (ctype, smob))                                \
    scm_wrong_type_arg_msg (FUNC_NAME, arg, smob, description); \
  else                                                          \
    var = GET_SMOB (ctype, smob);                               \
  } while (0)

/* Finally: With the help of the above macros we create smob types for
   Serveez socket structures, servers and server types.  */
MAKE_SMOB_DEFINITION (svz_socket, "svz-socket")
MAKE_SMOB_DEFINITION (svz_server, "svz-server")
MAKE_SMOB_DEFINITION (svz_servertype, "svz-servertype")

/* This macro creates a the body of socket callback getter/setter for
   use from Scheme code.  The procedure returns any previously set
   callback or an undefined value.  */
#define SOCK_CALLBACK_BODY(func,assoc)                          \
  svz_socket_t *xsock;                                          \
                                                                \
  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1,                   \
                  "svz-socket", xsock);                         \
  if (!SCM_UNBNDP (proc))                                       \
    {                                                           \
      SCM_ASSERT_TYPE (SCM_PROCEDUREP (proc), proc, SCM_ARG2,   \
                       FUNC_NAME, "procedure");                 \
      xsock->func = guile_func_ ## func;                        \
      return guile_sock_setfunction (xsock, assoc, proc);       \
    }                                                           \
  return guile_sock_getfunction (xsock, assoc)

/*
 * Extract a guile procedure from an option hash.  Return zero on success.
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

  /* Is there such a string in the option-hash?  */
  if (SCM_EQ_P (hvalue, SCM_UNSPECIFIED))
    {
      /* Nothing in hash, try to use default.  */
      if (hasdef)
        *target = defvar;
      else
        {
          guile_error ("No default procedure for `%s' %s", key, txt);
          err = 1;
        }
      return err;
    }

  /* Is that guile procedure?  */
  if (SCM_PROCEDUREP (hvalue))
    {
      *target = hvalue;
    }
  else if (STRING_OR_SYMBOL_P (hvalue))
    {
      char str[128];

      GI_GET_XREP (str, hvalue);
      proc = gi_lookup (str);
      if (!SCM_UNBNDP (proc) && SCM_PROCEDUREP (proc))
        *target = proc;
      else
        {
          guile_error ("No such procedure `%s' for `%s' %s", str, key, txt);
          err = 1;
        }
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
 * a hash associating names with guile procedures.
 */
static void
guile_servertype_add (svz_servertype_t *server, svz_hash_t *functions)
{
  svz_hash_put (functions, "server-type", server);
  svz_hash_put (guile_server, server->prefix, functions);
  svz_servertype_add (server);
}

/*
 * Lookup a procedure named @var{func} in the list of known servertypes.
 * The returned guile procedure depends on the given server type
 * @var{server}.  If the lookup fails this routine return SCM_UNDEFINED.
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
 * @var{sock} or SCM_UNDEFINED if there is no such procedure yet.
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

/* This function is used as destruction callback for the socket and
 * servertype hashes used in the Guile servers.  */
static void
guile_unprotect (SCM proc)
{
  if (!SCM_UNBNDP (proc))
    gi_gc_unprotect (proc);
}

/*
 * Associate the given guile procedure @var{proc} hereby named @var{func}
 * with the socket structure @var{sock}.  The function returns the previously
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
      gsock = svz_hash_create (4, (svz_free_func_t) guile_unprotect);
      svz_hash_put (guile_sock, svz_itoa (sock->id), gsock);
    }

  /* Put guile procedure into socket hash and protect it.  Removes old
     guile procedure and unprotects it.  */
  gi_gc_protect (proc);
  oldproc = (SCM) SVZ_PTR2NUM (svz_hash_put (gsock, func, SVZ_NUM2PTR (proc)));
  if (oldproc == 0)
    return SCM_UNDEFINED;

  gi_gc_unprotect (oldproc);
  return oldproc;
}

SCM_DEFINE
(guile_nuke_happened,
 "serveez-nuke", 0, 0, 0,
 (void),
 doc: /***********
Shutdown all network connections and terminate after the next event
loop.  You should use this instead of calling @code{quit}.  */)
{
#define FUNC_NAME s_guile_nuke_happened
  raise (SIGQUIT);
  return SCM_UNSPECIFIED;
#undef FUNC_NAME
}

SCM_DEFINE
(guile_access_exceptions,
 "serveez-exceptions", 0, 1, 0,
 (SCM enable),
 doc: /***********
Control the use of exception handlers for the Guile procedure calls of
Guile server callbacks.  If the optional argument @var{enable} is
@code{#t}, enable exception handling; if @code{#f}, disable it.
Return the current (boolean) setting.  */)
{
#define FUNC_NAME s_guile_access_exceptions
  SCM value = SCM_BOOL (guile_use_exceptions);
  int n;

  if (!SCM_UNBNDP (enable))
    {
      if (guile_to_boolean (enable, &n))
        guile_error ("%s: Invalid boolean value", FUNC_NAME);
      else
        guile_use_exceptions = n;
    }
  return value;
#undef FUNC_NAME
}

/*
 * The @code{guile_call} function puts the procedure to call including
 * the arguments to it into a single scheme cell passed to this function
 * in @var{data}.  The functions unpacks this cell and applies it to
 * @code{scm_apply}.
 * By convention the @var{data} argument cell consists of three items chained
 * like this: @code{(procedure first-argument (remaining-argument-list))}
 */
static SCM
guile_call_body (SCM data)
{
  return scm_apply (SCM_CAR (data),
                    SCM_CAR (SCM_CDR (data)), SCM_CDR (SCM_CDR (data)));
}

/*
 * This is the exception handler for calls by @code{guile_call}.  Prints
 * the procedure (passed in @var{data}), the name of the exception and the
 * error message if possible.
 */
static SCM
guile_call_handler (SCM data, SCM tag, SCM args)
{
  SCM ep = scm_current_error_port ();

  scm_puts ("exception in ", ep);
  scm_display (data, ep);
  scm_puts (" due to `", ep);
  scm_display (tag, ep);
  scm_puts ("'\n", ep);
  scm_puts ("guile-error: ", ep);

  /* on quit/exit */
  if (SCM_NULLP (args))
    {
      scm_display (tag, ep);
      scm_puts ("\n", ep);
      return SCM_BOOL_F;
    }

  if (!SCM_FALSEP (SCM_CAR (args)))
    {
      scm_display (SCM_CAR (args), ep);
      scm_puts (": ", ep);
    }
  scm_display_error_message (SCM_CAR (SCM_CDR (args)),
                             SCM_CAR (SCM_CDR (SCM_CDR (args))),
                             ep);
  return SCM_BOOL_F;
}

/*
 * The following function takes an arbitrary number of arguments (specified
 * in @var{args}) passed to @code{scm_apply} calling the guile procedure
 * @var{code}.  The function catches exceptions occurring in the procedure
 * @var{code}.  On success (no exception) the routine returns the value
 * returned by @code{scm_apply} otherwise @code{SCM_BOOL_F}.
 */
static SCM
guile_call (SCM code, int args, ...)
{
  va_list list;
  SCM body_data, handler_data;
  SCM arg = SCM_EOL, arglist = SCM_EOL, ret;

  /* Setup arg and arglist correctly for use with ‘scm_apply’.  */
  va_start (list, args);
  if (args > 0)
    {
      arg = va_arg (list, SCM);
      while (--args)
        arglist = scm_cons (va_arg (list, SCM), arglist);
      arglist = scm_cons (scm_reverse (arglist), SCM_EOL);
    }
  va_end (list);

  /* Put both arguments and the procedure together into a single argument
     for the catch body.  */
  body_data = scm_cons (code, scm_cons (arg, arglist));
  handler_data = code;

  /* Use exception handling if requested.  */
  if (guile_use_exceptions)
    {
      ret = scm_internal_catch (SCM_BOOL_T,
                                (scm_t_catch_body) guile_call_body,
                                (void *) body_data,
                                (scm_t_catch_handler) guile_call_handler,
                                (void *) handler_data);
    }
  else
    {
      ret = guile_call_body (body_data);
    }

  return ret;
}

/* Wrapper function for the global initialization of a server type.  */
static int
guile_func_global_init (svz_servertype_t *stype)
{
#define FUNC_NAME __func__
  SCM global_init = guile_servertype_getfunction (stype, "global-init");
  SCM ret;

  if (!SCM_UNBNDP (global_init))
    {
      ret = guile_call (global_init, 1, MAKE_SMOB (svz_servertype, stype));
      return integer_else (ret, -1);
    }
  return 0;
#undef FUNC_NAME
}


/* Wrapper function for the initialization of a server instance.  */
static int
guile_func_init (svz_server_t *server)
{
#define FUNC_NAME __func__
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM init = guile_servertype_getfunction (stype, "init");
  SCM ret;

  if (!SCM_UNBNDP (init))
    {
      ret = guile_call (init, 1, MAKE_SMOB (svz_server, server));
      return integer_else (ret, -1);
    }
  return 0;
#undef FUNC_NAME

}
/* Wrapper routine for protocol detection of a server instance.  */
static int
guile_func_detect_proto (svz_server_t *server, svz_socket_t *sock)
{
#define FUNC_NAME __func__
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM detect_proto = guile_servertype_getfunction (stype, "detect-proto");
  SCM ret;

  if (!SCM_UNBNDP (detect_proto))
    {
      ret = guile_call (detect_proto, 2, MAKE_SMOB (svz_server, server),
                        MAKE_SMOB (svz_socket, sock));
      return integer_else (ret, 0);
    }
  return 0;
}
#undef FUNC_NAME

/* Free the socket boundary if set by guile.  */
static void
guile_sock_clear_boundary (svz_socket_t *sock)
{
  if (sock->boundary)
    svz_free_and_zero (sock->boundary);
  sock->boundary_size = 0;
}

/* Wrapper for the socket disconnected callback.  Used here in order to
   delete the additional guile callbacks associated with the disconnected
   socket structure.  */
static int
guile_func_disconnected_socket (svz_socket_t *sock)
{
#define FUNC_NAME __func__
  SCM ret, disconnected = guile_sock_getfunction (sock, "disconnected");
  int retval = -1;
  svz_hash_t *gsock;

  /* First call the guile callback if necessary.  */
  if (!SCM_UNBNDP (disconnected))
    {
      ret = guile_call (disconnected, 1, MAKE_SMOB (svz_socket, sock));
      retval = integer_else (ret, -1);
    }

  /* Delete all the associated guile callbacks and unprotect these.  */
  if ((gsock = svz_hash_delete (guile_sock, svz_itoa (sock->id))) != NULL)
    svz_hash_destroy (gsock);

  /* Release associated guile object is necessary.  */
  if (sock->data != NULL)
    gi_gc_unprotect ((SCM) SVZ_PTR2NUM (sock->data));

  /* Free the socket boundary if set by guile.  */
  guile_sock_clear_boundary (sock);

  return retval;
#undef FUNC_NAME

}
/* Wrapper for the kicked socket callback.  */
static int
guile_func_kicked_socket (svz_socket_t *sock, int reason)
{
#define FUNC_NAME __func__
  SCM ret, kicked = guile_sock_getfunction (sock, "kicked");

  if (!SCM_UNBNDP (kicked))
    {
      ret = guile_call (kicked, 2, MAKE_SMOB (svz_socket, sock),
                        gi_integer2scm (reason));
      return integer_else (ret, -1);
    }
  return 0;
}
#undef FUNC_NAME

/* Wrapper function for the socket connection after successful detection.  */
static int
guile_func_connect_socket (svz_server_t *server, svz_socket_t *sock)
{
#define FUNC_NAME __func__
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM connect_socket = guile_servertype_getfunction (stype, "connect-socket");
  SCM ret;

  /* Setup this function for later use.  */
  sock->disconnected_socket = guile_func_disconnected_socket;

  if (!SCM_UNBNDP (connect_socket))
    {
      ret = guile_call (connect_socket, 2, MAKE_SMOB (svz_server, server),
                        MAKE_SMOB (svz_socket, sock));
      return integer_else (ret, 0);
    }
  return 0;
#undef FUNC_NAME

}
/* Wrapper for the finalization of a server instance.  */
static int
guile_func_finalize (svz_server_t *server)
{
#define FUNC_NAME __func__
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM ret, finalize = guile_servertype_getfunction (stype, "finalize");
  int retval = 0;
  svz_hash_t *state;

  if (!SCM_UNBNDP (finalize))
    {
      ret = guile_call (finalize, 1, MAKE_SMOB (svz_server, server));
      retval = integer_else (ret, -1);
    }

  /* Release associated guile server state objects is necessary.  */
  if ((state = server->data) != NULL)
    {
      svz_hash_destroy (state);
      server->data = NULL;
    }

  return retval;
}
#undef FUNC_NAME

/* Wrapper routine for the global finalization of a server type.  */
static int
guile_func_global_finalize (svz_servertype_t *stype)
{
#define FUNC_NAME __func__
  SCM ret, global_finalize;
  global_finalize = guile_servertype_getfunction (stype, "global-finalize");

  if (!SCM_UNBNDP (global_finalize))
    {
      ret = guile_call (global_finalize, 1, MAKE_SMOB (svz_servertype, stype));
      return integer_else (ret, -1);
    }
  return 0;
#undef FUNC_NAME

}
/* Min-Max definitions.  */
#define GUILE_MIN(x, y) (((x) < (y)) ? (x) : (y))
#define GUILE_MAX(x, y) (((x) > (y)) ? (x) : (y))

/* Wrapper for the client info callback.  */
static char *
guile_func_info_client (svz_server_t *server, svz_socket_t *sock)
{
#define FUNC_NAME __func__
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM info_client = guile_servertype_getfunction (stype, "info-client");
  SCM ret;
  static char text[1024];

  if (!SCM_UNBNDP (info_client))
    {
      ret = guile_call (info_client, 2, MAKE_SMOB (svz_server, server),
                        MAKE_SMOB (svz_socket, sock));
      if (GI_GET_XREP_MAYBE (text, ret))
        return text;
    }
  return NULL;
}
#undef FUNC_NAME

/* Wrapper for the server info callback.  */
static char *
guile_func_info_server (svz_server_t *server)
{
#define FUNC_NAME __func__
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM info_server = guile_servertype_getfunction (stype, "info-server");
  SCM ret;
  static char text[1024];

  if (!SCM_UNBNDP (info_server))
    {
      ret = guile_call (info_server, 1, MAKE_SMOB (svz_server, server));
      if (GI_GET_XREP_MAYBE (text, ret))
        return text;
    }
  return NULL;
#undef FUNC_NAME

}
/* Wrapper for the server notifier callback.  */
static int
guile_func_notify (svz_server_t *server)
{
#define FUNC_NAME __func__
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM ret, notify = guile_servertype_getfunction (stype, "notify");

  if (!SCM_UNBNDP (notify))
    {
      ret = guile_call (notify, 1, MAKE_SMOB (svz_server, server));
      return integer_else (ret, -1);
    }
  return -1;
}
#undef FUNC_NAME

/* Wrapper for the server reset callback.  */
static int
guile_func_reset (svz_server_t *server)
{
#define FUNC_NAME __func__
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM ret, reset = guile_servertype_getfunction (stype, "reset");

  if (!SCM_UNBNDP (reset))
    {
      ret = guile_call (reset, 1, MAKE_SMOB (svz_server, server));
      return integer_else (ret, -1);
    }
  return -1;
#undef FUNC_NAME

}
/* Wrapper for the socket check request callback.  */
static int
guile_func_check_request (svz_socket_t *sock)
{
#define FUNC_NAME __func__
  SCM ret, check_request;
  check_request = guile_sock_getfunction (sock, "check-request");

  if (!SCM_UNBNDP (check_request))
    {
      ret = guile_call (check_request, 1, MAKE_SMOB (svz_socket, sock));
      return integer_else (ret, -1);
    }
  return -1;
}
#undef FUNC_NAME

/* Wrapper for the socket handle request callback.  The function searches for
   both the servertype specific and socket specific procedure.  */
static int
guile_func_handle_request (svz_socket_t *sock, char *request, int len)
{
#define FUNC_NAME __func__
  svz_server_t *server;
  svz_servertype_t *stype;
  SCM ret, handle_request;
  handle_request = guile_sock_getfunction (sock, "handle-request");

  if (SCM_UNBNDP (handle_request))
    {
      server = svz_server_find (sock->cfg);
      stype = svz_servertype_find (server);
      handle_request = guile_servertype_getfunction (stype, "handle-request");
    }

  if (!SCM_UNBNDP (handle_request))
    {
      ret = guile_call (handle_request, 3, MAKE_SMOB (svz_socket, sock),
                        guile_data_to_bin (request, len), gi_integer2scm (len));
      return integer_else (ret, -1);
    }
  return -1;
#undef FUNC_NAME

}
/* Wrapper for the socket idle func callback.  */
static int
guile_func_idle_func (svz_socket_t *sock)
{
#define FUNC_NAME __func__
  SCM ret, idle_func = guile_sock_getfunction (sock, "idle");

  if (!SCM_UNBNDP (idle_func))
    {
      ret = guile_call (idle_func, 1, MAKE_SMOB (svz_socket, sock));
      return integer_else (ret, -1);
    }
  return 0;
}
#undef FUNC_NAME

/* Wrapper for the socket trigger condition func callback.  */
static int
guile_func_trigger_cond (svz_socket_t *sock)
{
#define FUNC_NAME __func__
  SCM ret, trigger_cond = guile_sock_getfunction (sock, "trigger-condition");

  if (!SCM_UNBNDP (trigger_cond))
    {
      ret = guile_call (trigger_cond, 1, MAKE_SMOB (svz_socket, sock));
      return SCM_NFALSEP (ret);
    }
  return 0;
#undef FUNC_NAME

}
/* Wrapper for the socket trigger func callback.  */
static int
guile_func_trigger_func (svz_socket_t *sock)
{
#define FUNC_NAME __func__
  SCM ret, trigger_func = guile_sock_getfunction (sock, "trigger");

  if (!SCM_UNBNDP (trigger_func))
    {
      ret = guile_call (trigger_func, 1, MAKE_SMOB (svz_socket, sock));
      return integer_else (ret, -1);
    }
  return 0;
}
#undef FUNC_NAME

/* Wrapper for the socket check oob request callback.  */
static int
guile_func_check_request_oob (svz_socket_t *sock)
{
#define FUNC_NAME __func__
  SCM ret, check_request_oob;
  check_request_oob = guile_sock_getfunction (sock, "check-oob-request");

  if (!SCM_UNBNDP (check_request_oob))
    {
      ret = guile_call (check_request_oob, 2, MAKE_SMOB (svz_socket, sock),
                        gi_integer2scm (sock->oob));
      return integer_else (ret, -1);
    }
  return -1;
#undef FUNC_NAME
}

SCM_DEFINE
(guile_sock_handle_request,
 "svz:sock:handle-request", 1, 1, 0,
 (SCM sock, SCM proc),
 doc: /***********
Set the @code{handle-request} member of the socket structure @var{sock}
to @var{proc}.  Return the previously set handler if there is any.  */)
{
#define FUNC_NAME s_guile_sock_handle_request
  SOCK_CALLBACK_BODY (handle_request, "handle-request");
#undef FUNC_NAME
}

SCM_DEFINE
(guile_sock_check_request,
 "svz:sock:check-request", 1, 1, 0,
 (SCM sock, SCM proc),
 doc: /***********
Set the @code{check-request} member of the socket structure @var{sock}
to @var{proc}.  Return the previously handler if there is any.  */)
{
#define FUNC_NAME s_guile_sock_check_request
  SOCK_CALLBACK_BODY (check_request, "check-request");
#undef FUNC_NAME
}

SCM_DEFINE
(guile_sock_boundary,
 "svz:sock:boundary", 2, 0, 0,
 (SCM sock, SCM boundary),
 doc: /***********
Setup the packet boundary of the socket @var{sock}.  The given string
value @var{boundary} can contain any kind of data.  If @var{boundary}
is an exact number, set up the socket to parse fixed sized packets.
More precisely, set the @code{check-request} callback of the given
socket structure @var{sock} to an internal routine which runs the
socket's @code{handle-request} callback when it detects a
complete packet specified by @var{boundary}.

For instance, you can arrange for Serveez to pass the
@code{handle-request} procedure lines of text by calling
@code{(svz:sock:boundary sock "\n")}.  */)
{
#define FUNC_NAME s_guile_sock_boundary
  svz_socket_t *xsock;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  SCM_ASSERT_TYPE (SCM_EXACTP (boundary) || SCM_STRINGP (boundary),
                   boundary, SCM_ARG2, FUNC_NAME, "string or exact");

  /* Release previously set boundaries.  */
  guile_sock_clear_boundary (xsock);

  /* Setup for fixed sized packets.  */
  if (SCM_EXACTP (boundary))
    {
      xsock->boundary = NULL;
      xsock->boundary_size = gi_scm2int (boundary);
    }
  /* Handle packet delimiters.  */
  else
    {
      char buf[512];

      xsock->boundary_size = GI_GET_XREP (buf, boundary);
      xsock->boundary = svz_strdup (buf);
    }

  /* Only assign this callback for connection oriented protocols.  */
  if (xsock->proto & (SVZ_PROTO_TCP | SVZ_PROTO_PIPE))
    xsock->check_request = svz_sock_check_request;

  return SCM_BOOL_T;
#undef FUNC_NAME
}

SCM_DEFINE
(guile_sock_floodprotect,
 "svz:sock:floodprotect", 1, 1, 0,
 (SCM sock, SCM flag),
 doc: /***********
Set or unset the flood protection bit of the given socket @var{sock}.
Return the previous value of this bit (@code{#t} or @code{#f}).  The
@var{flag} argument must be either boolean or an exact number and is
optional.  */)
{
#define FUNC_NAME s_guile_sock_floodprotect
  svz_socket_t *xsock;
  int flags;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  flags = xsock->flags;
  if (!SCM_UNBNDP (flag))
    {
      SCM_ASSERT_TYPE (SCM_BOOLP (flag) || SCM_EXACTP (flag),
                       flag, SCM_ARG2, FUNC_NAME, "boolean or exact");
      if ((SCM_BOOLP (flag) && SCM_NFALSEP (flag) != 0) ||
          (SCM_EXACTP (flag) && gi_scm2int (flag) != 0))
        xsock->flags &= ~SVZ_SOFLG_NOFLOOD;
      else
        xsock->flags |= SVZ_SOFLG_NOFLOOD;
    }
  return (flags & SVZ_SOFLG_NOFLOOD) ? SCM_BOOL_F : SCM_BOOL_T;
#undef FUNC_NAME
}

SCM_DEFINE
(guile_sock_print,
 "svz:sock:print", 2, 0, 0,
 (SCM sock, SCM buffer),
 doc: /***********
Write @var{buffer} (string or binary smob) to the socket @var{sock}.
Return @code{#t} on success and @code{#f} on failure.  */)
{
#define FUNC_NAME s_guile_sock_print
  svz_socket_t *xsock;
  char tem[8192];
  char *buf;
  int len, ret = -1;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  SCM_ASSERT_TYPE (SCM_STRINGP (buffer) || guile_bin_check (buffer),
                   buffer, SCM_ARG2, FUNC_NAME, "string or binary");

  if (SCM_STRINGP (buffer))
    {
      len = GI_GET_XREP (tem, buffer);
      buf = tem;
    }
  else
    {
      buf = guile_bin_to_data (buffer, &len);
    }

  /* Depending on the protocol type use different kind of senders.  */
  if (xsock->proto & (SVZ_PROTO_TCP | SVZ_PROTO_PIPE))
    ret = svz_sock_write (xsock, buf, len);
  else if (xsock->proto & SVZ_PROTO_UDP)
    ret = svz_udp_write (xsock, buf, len);
  else if (xsock->proto & SVZ_PROTO_ICMP)
    ret = svz_icmp_write (xsock, buf, len);

  if (ret == -1)
    {
      svz_sock_schedule_for_shutdown (xsock);
      return SCM_BOOL_F;
    }
  return SCM_BOOL_T;
#undef FUNC_NAME
}

SCM_DEFINE
(guile_sock_data,
 "svz:sock:data", 1, 1, 0,
 (SCM sock, SCM data),
 doc: /***********
Associate any kind of data (any object) given in the argument
@var{data} with the socket @var{sock}.  The @var{data} argument is
optional.  Return a previously stored value or an empty list.
[Do not use; will be deleted; use object properties, instead.  --ttn]  */)
{
#define FUNC_NAME s_guile_sock_data
  svz_socket_t *xsock;
  SCM ret = SCM_EOL;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);

  /* Save return value here.  */
  if (xsock->data != NULL)
    ret = (SCM) SVZ_PTR2NUM (xsock->data);

  /* Replace associated guile cell and unprotect previously stored cell
     if necessary.  */
  if (!SCM_UNBNDP (data))
    {
      if (xsock->data != NULL)
        gi_gc_unprotect (ret);
      xsock->data = SVZ_NUM2PTR (data);
      gi_gc_protect (data);
    }
  return ret;
#undef FUNC_NAME
}

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
      ret = gi_integer2scm (*(int *) address);
      break;
    case SVZ_ITEM_INTARRAY:
      ret = guile_intarray_to_guile (*(svz_array_t **) address);
      break;
    case SVZ_ITEM_STR:
      ret = gi_string2scm (*(char **) address);
      break;
    case SVZ_ITEM_STRARRAY:
      ret = guile_strarray_to_guile (*(svz_array_t **) address);
      break;
    case SVZ_ITEM_HASH:
      ret = guile_hash_to_guile (*(svz_hash_t **) address);
      break;
    case SVZ_ITEM_PORTCFG:
      if ((port = *(svz_portcfg_t **) address) != NULL)
        ret = gi_string2scm (port->name);
      break;
    case SVZ_ITEM_BOOL:
      ret = SCM_BOOL (*(int *) address);
      break;
    }
  return ret;
}

/* Checks if the given Guile object @var{smob} in position @var{arg} is a
   server or socket and throws an exception if not.  Otherwise it saves the
   server in the variable @var{var}.  */
#define CHECK_SERVER_SMOB_ARG(smob, arg, var)                                \
  do {                                                                       \
    SCM_ASSERT_TYPE (CHECK_SMOB (svz_server, smob) ||                        \
                     CHECK_SMOB (svz_socket, smob), smob, arg,               \
                     FUNC_NAME, "svz-server or svz-socket");                 \
    var = CHECK_SMOB (svz_server, smob) ? GET_SMOB (svz_server, smob) :      \
      svz_server_find (((svz_socket_t *) GET_SMOB (svz_socket, smob))->cfg); \
  } while (0)

SCM_DEFINE
(guile_server_config_ref,
 "svz:server:config-ref", 2, 0, 0,
 (SCM server, SCM key),
 doc: /***********
Return the configuration item specified by @var{key} of the given server
instance @var{server}.  You can pass this procedure a socket, too, in
which case the appropriate server instance is looked up.  If the given
string @var{key} is invalid (not defined in the configuration alist in
@code{define-servertype!}), then return an empty list.  */)
{
#define FUNC_NAME s_guile_server_config_ref
  SCM ret = SCM_EOL;
  svz_servertype_t *stype;
  svz_server_t *xserver;
  int i;
  void *cfg, *address;
  char str[64];
  svz_config_prototype_t *prototype;

  CHECK_SERVER_SMOB_ARG (server, SCM_ARG1, xserver);
  SCM_ASSERT_TYPE (SCM_STRINGP (key), key, SCM_ARG2, FUNC_NAME, "string");

  GI_GET_XREP (str, key);
  stype = svz_servertype_find (xserver);
  cfg = xserver->cfg;
  prototype = &stype->config_prototype;

  for (i = 0; prototype->items[i].type != SVZ_ITEM_END; i++)
    {
      if (strcmp (prototype->items[i].name, str) == 0)
        {
          address = (void *) ((unsigned long) cfg +
                              (unsigned long) prototype->items[i].address -
                              (unsigned long) prototype->start);
          ret = guile_config_convert (address, prototype->items[i].type);
          break;
        }
    }
  return ret;
#undef FUNC_NAME
}

SCM_DEFINE
(guile_server_state_ref,
 "svz:server:state-ref", 2, 0, 0,
 (SCM server, SCM key),
 doc: /***********
Return the Guile object associated with the string value @var{key} which
needs to be have been set via @code{svz:server:state-set!} previously.
Otherwise return an empty list.  The given @var{server} argument must be
either a valid @code{#<svz-server>} object or a @code{#<svz-socket>}.
[NB: See comment in @code{svz:server:state-set!}. ---ttn]  */)
{
#define FUNC_NAME s_guile_server_state_ref
  SCM ret = SCM_EOL;
  svz_server_t *xserver;
  char str[64];
  svz_hash_t *hash;

  CHECK_SERVER_SMOB_ARG (server, SCM_ARG1, xserver);
  SCM_ASSERT_TYPE (SCM_STRINGP (key), key, SCM_ARG2, FUNC_NAME, "string");
  GI_GET_XREP (str, key);

  if ((hash = xserver->data) != NULL)
    if (svz_hash_exists (hash, str))
      ret = (SCM) SVZ_PTR2NUM (svz_hash_get (hash, str));
  return ret;
#undef FUNC_NAME
}

SCM_DEFINE
(guile_server_state_set_x,
 "svz:server:state-set!", 3, 0, 0,
 (SCM server, SCM key, SCM value),
 doc: /***********
Associate the Guile object @var{value} with the string @var{key}.  The
given @var{server} argument can be both, a @code{#<svz-server>} or a
@code{#<svz-socket>}.  Return the previously associated object or an
empty list if there was no such association.  This procedure is useful
for server instance state savings.  [Actually, it is superfluous since
Guile provides object properties and closures.  Probably this procedure
will be deleted soon. ---ttn]  */)
{
#define FUNC_NAME s_guile_server_state_set_x
  SCM ret = SCM_EOL;
  svz_server_t *xserver;
  char str[64];
  svz_hash_t *hash;

  CHECK_SERVER_SMOB_ARG (server, SCM_ARG1, xserver);
  SCM_ASSERT_TYPE (SCM_STRINGP (key), key, SCM_ARG2, FUNC_NAME, "string");
  GI_GET_XREP (str, key);

  if ((hash = xserver->data) == NULL)
    {
      hash = svz_hash_create (4, (svz_free_func_t) guile_unprotect);
      xserver->data = hash;
    }
  if (svz_hash_exists (hash, str))
    {
      ret = (SCM) SVZ_PTR2NUM (svz_hash_put (hash, str, SVZ_NUM2PTR (value)));
      gi_gc_unprotect (ret);
    }
  else
    svz_hash_put (hash, str, SVZ_NUM2PTR (value));
  gi_gc_protect (value);
  return ret;
#undef FUNC_NAME
}

static void
server_state_to_hash_internal (void *k, void *v, void *closure)
{
  SCM *hash = closure;

  scm_hash_set_x (*hash, gi_string2scm (k), (SCM) SVZ_PTR2NUM (v));
}

SCM_DEFINE
(guile_server_state_to_hash,
 "svz:server:state->hash", 1, 0, 0,
 (SCM server),
 doc: /***********
Convert the @var{server} instance's state into a Guile hash.
Return an empty list if there is no such state yet.  */)
{
#define FUNC_NAME s_guile_server_state_to_hash
  SCM hash = SCM_EOL;
  svz_hash_t *data;
  svz_server_t *xserver;

  CHECK_SERVER_SMOB_ARG (server, SCM_ARG1, xserver);
  if ((data = xserver->data) != NULL)
    {
      /* FIXME: Use "make-hash-table" analog.  */
      hash = gi_n_vector (svz_hash_size (data), SCM_EOL);
      svz_hash_foreach (server_state_to_hash_internal, data, &hash);
    }
  return hash;
#undef FUNC_NAME
}

/*
 * Returns the length of a configuration item type, updates the configuration
 * item structure @var{item} and increases the @var{size} value if the
 * text representation @var{str} fits one of the item types understood by
 * Serveez.  Returns zero if there is no such type.
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
 * in Guile.  This is necessary because each of these items is dynamically
 * allocated if defined in Guile.
 */
static void
guile_servertype_config_free (svz_servertype_t *server)
{
  int n;
  svz_config_prototype_t *prototype = &server->config_prototype;

  for (n = 0; prototype->items[n].type != SVZ_ITEM_END; n++)
    svz_free (prototype->items[n].name);
  svz_config_free (prototype, prototype->start);
  svz_free (prototype->items);
}

#if ENABLE_DEBUG
static void
print_hash_kv (void *k, void *v, SVZ_UNUSED void *closure)
{
  char *key = k;
  char *value = v;

  fprintf (stderr, "(%s => %s) ", key, value);
}


/*
 * Debug helper: Emit a printable representation of the the given
 * port configuration to the given FILE stream @var{f}.
 */
static void
portcfg_print (svz_portcfg_t *this, FILE *f)
{
  if (NULL == this)
    {
      fprintf (f, "portcfg is NULL\n");
      return;
    }

  switch (this->proto)
    {
    case SVZ_PROTO_TCP:
      fprintf (f, "portcfg `%s': TCP (%s|%s):%d\n", this->name,
               SVZ_CFG_TCP (this, ipaddr),
               svz_inet_ntoa (SVZ_CFG_TCP (this, addr).sin_addr.s_addr),
               SVZ_CFG_TCP (this, port));
      break;
    case SVZ_PROTO_UDP:
      fprintf (f, "portcfg `%s': UDP (%s|%s):%d\n", this->name,
               SVZ_CFG_UDP (this, ipaddr),
               svz_inet_ntoa (SVZ_CFG_UDP (this, addr).sin_addr.s_addr),
               SVZ_CFG_UDP (this, port));
      break;
    case SVZ_PROTO_ICMP:
      fprintf (f, "portcfg `%s': ICMP (%s|%s)\n", this->name,
               SVZ_CFG_ICMP (this, ipaddr),
               svz_inet_ntoa (SVZ_CFG_ICMP (this, addr).sin_addr.s_addr));
      break;
    case SVZ_PROTO_RAW:
      fprintf (f, "portcfg `%s': RAW (%s|%s)\n", this->name,
               SVZ_CFG_RAW (this, ipaddr),
               svz_inet_ntoa (SVZ_CFG_RAW (this, addr).sin_addr.s_addr));
      break;
    case SVZ_PROTO_PIPE:
      {
        svz_pipe_t *r = &SVZ_CFG_PIPE (this, recv);
        svz_pipe_t *s = &SVZ_CFG_PIPE (this, send);

        fprintf (f, "portcfg `%s': PIPE "
                 "(\"%s\", \"%s\" (%d), \"%s\" (%d), %04o)<->"
                 "(\"%s\", \"%s\" (%d), \"%s\" (%d), %04o)\n", this->name,
                 r->name, r->user, r->uid, r->group, r->gid, r->perm,
                 s->name, s->user, s->uid, s->group, s->gid, s->perm);
      }
      break;
    default:
      fprintf (f, "portcfg has invalid proto field %d\n", this->proto);
    }
}

/*
 * Debug helper: Display a text representation of the configuration items
 * of a guile servertype.
 */
static void
guile_servertype_config_print (svz_servertype_t *server)
{
  int n;
  size_t i;
  svz_array_t *array;
  svz_hash_t *hash;
  svz_portcfg_t *port;
  svz_config_prototype_t *prototype = &server->config_prototype;

  fprintf (stderr, "Configuration of `%s':\n", server->prefix);
  for (n = 0; prototype->items[n].type != SVZ_ITEM_END; n++)
    {
      fprintf (stderr, " * %s `%s' is %sdefaultable\n",
               SVZ_ITEM_TEXT (prototype->items[n].type),
               prototype->items[n].name, prototype->items[n].defaultable ?
               "" : "not ");
      if (prototype->items[n].defaultable)
        {
          fprintf (stderr, "   Default value: ");
          switch (prototype->items[n].type)
            {
            case SVZ_ITEM_INT:
              fprintf (stderr, "%d", *(int *) prototype->items[n].address);
              break;
            case SVZ_ITEM_INTARRAY:
              array = *(svz_array_t **) prototype->items[n].address;
              fprintf (stderr, "( ");
              for (i = 0; i < svz_array_size (array); i++)
                fprintf (stderr, "%ld ", (long) svz_array_get (array, i));
              fprintf (stderr, ")");
              break;
            case SVZ_ITEM_STR:
              fprintf (stderr, "%s", *(char **) prototype->items[n].address);
              break;
            case SVZ_ITEM_STRARRAY:
              array = *(svz_array_t **) prototype->items[n].address;
              fprintf (stderr, "( ");
              for (i = 0; i < svz_array_size (array); i++)
                fprintf (stderr, "`%s' ", (char *) svz_array_get (array, i));
              fprintf (stderr, ")");
              break;
            case SVZ_ITEM_HASH:
              hash = *(svz_hash_t **) prototype->items[n].address;
              fprintf (stderr, "( ");
              svz_hash_foreach (print_hash_kv, hash, NULL);
              fprintf (stderr, ")");
              break;
            case SVZ_ITEM_PORTCFG:
              port = *(svz_portcfg_t **) prototype->items[n].address;
              portcfg_print (port, stderr);
              break;
            case SVZ_ITEM_BOOL:
              fprintf (stderr, "%d", *(int *) prototype->items[n].address);
              break;
          }
          fprintf (stderr, " at %p\n", prototype->items[n].address);
        }
    }
}
#endif /* ENABLE_DEBUG */

/*
 * Obtain a default value from the scheme cell @var{value}.  The configuration
 * item type is specified by @var{type}.  The default value is stored then at
 * @var{address}.  Returns zero on success.
 */
static int
guile_servertype_config_default (svz_servertype_t *server, SCM value,
                                 void *address, int len, int type, char *key)
{
  int err = 0, n;
  char str[2048], *txt;
  svz_array_t *array;
  svz_hash_t *hash;
  svz_portcfg_t *port, *dup;

  switch (type)
    {
      /* Integer.  */
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

      /* Array of integers.  */
    case SVZ_ITEM_INTARRAY:
      if ((array = guile_to_intarray (value, key)) == NULL)
        err = -1;
      else
        memcpy (address, &array, len);
      break;

      /* Character string.  */
    case SVZ_ITEM_STR:
      if (! GI_GET_XREP_MAYBE (str, value))
        {
          guile_error ("%s: Invalid string value for `%s'",
                       server->prefix, key);
          err = -1;
        }
      else
        {
          txt = svz_strdup (str);
          memcpy (address, &txt, len);
        }
      break;

      /* Array of character strings.  */
    case SVZ_ITEM_STRARRAY:
      if ((array = guile_to_strarray (value, key)) == NULL)
        err = -1;
      else
        memcpy (address, &array, len);
      break;

      /* Hash.  */
    case SVZ_ITEM_HASH:
      if ((hash = guile_to_hash (value, key)) == NULL)
        err = -1;
      else
        memcpy (address, &hash, len);
      break;

      /* Port configuration.  */
    case SVZ_ITEM_PORTCFG:
      if (! GI_GET_XREP_MAYBE (str, value))
        {
          guile_error ("%s: Invalid string value for `%s'",
                       server->prefix, key);
          err = -1;
        }
      else if ((port = svz_portcfg_get (str)) == NULL)
        {
          guile_error ("%s: No such port configuration: `%s'",
                       server->prefix, str);
          err = -1;
        }
      else
        {
          dup = svz_portcfg_dup (port);
          memcpy (address, &dup, len);
        }
      break;

      /* Boolean value.  */
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

      /* Invalid type.  */
    default:
      err = -1;
    }
  return err;
}

static void
items_append (svz_key_value_pair_t **all, unsigned int i,
              svz_key_value_pair_t *one)
{
  *all = svz_realloc (*all, (1 + i) * sizeof (*one));
  /* Is this equivalent to:  *((*all)[n]) = *one;  ???  */
  memcpy (&((*all)[i]), one, sizeof (*one));
}

struct servertype_config_closure
{
  svz_servertype_t *server;
  char *action;
  svz_hash_t *options;
  int error;
  int size;
  unsigned int count;
  svz_key_value_pair_t *items;
  char *prototype;
};

static void
servertype_config_internal (void *k, SVZ_UNUSED void *v, void *closure)
{
  struct servertype_config_closure *x = closure;
  char *key = k;
  SCM list = optionhash_get (x->options, key);
  svz_key_value_pair_t item;
  SCM value;
  char str[64];
  int len, def;

  /* Each configuration item must be a scheme list with three elements.  */
  if (!SCM_LISTP (list) ||
      gi_scm2ulong (scm_length (list)) != 3)
    {
      guile_error ("Invalid definition for `%s' %s", key, x->action);
      x->error = -1;
      return;
    }

  /* Assign address offset.  */
  item.address = SVZ_NUM2PTR (x->size);

  /* First appears the type of item.  */
  value = SCM_CAR (list);
  if (! GI_GET_XREP_MAYBE (str, value))
    {
      guile_error ("Invalid type definition for `%s' %s", key, x->action);
      x->error = -1;
      return;
    }
  len = guile_servertype_config_type (str, &item, &x->size);
  if (len == 0)
    {
      guile_error ("Invalid type for `%s' %s", key, x->action);
      x->error = -1;
      return;
    }

  /* Then appears a boolean value specifying if the configuration
     item is defaultable or not.  */
  list = SCM_CDR (list);
  value = SCM_CAR (list);
  if (guile_to_boolean (value, &def) != 0)
    {
      guile_error ("Invalid defaultable value for `%s' %s", key, x->action);
      x->error = -1;
      return;
    }
  item.defaultable = def
    ? SVZ_ITEM_DEFAULTABLE
    : SVZ_ITEM_NOTDEFAULTABLE;

  /* Finally the default value itself.  */
  list = SCM_CDR (list);
  value = SCM_CAR (list);
  x->prototype = svz_realloc (x->prototype, x->size);
  memset (x->prototype + x->size - len, 0, len);
  if (item.defaultable == SVZ_ITEM_DEFAULTABLE)
    {
      x->error |= guile_servertype_config_default
        (x->server, value, x->prototype + x->size - len,
         len, item.type, key);
    }

  /* Increase the number of configuration items.  */
  item.name = svz_strdup (key);
  items_append (&x->items, x->count++, &item);
}

/*
 * Parse the configuration of the server type @var{server} stored in the
 * scheme cell @var{cfg}.
 */
static int
guile_servertype_config (svz_servertype_t *server, SCM cfg)
{
#define FUNC_NAME __func__
  unsigned int n;
  svz_key_value_pair_t item;
  char action[ACTIONBUFSIZE];
  struct servertype_config_closure x =
    {
      server, action,
      NULL,                             /* .options */
      0,                                /* .error */
      0,                                /* .size */
      0,                                /* .count */
      NULL,                             /* .items */
      NULL                              /* .prototype */
    };

#define err      (x.error)              /* Keep things tidy.  */
#define items    (x.items)
#define options  (x.options)

  DOING ("parsing configuration of `%s'", server->prefix);

  /* Check if the configuration alist is given or not.  */
  if (SCM_EQ_P (cfg, SCM_UNSPECIFIED))
    {
      guile_error ("Missing servertype `configuration' for `%s'",
                   server->prefix);
      FAIL ();
    }

  /* Try parsing this alist is valid.  */
  if (NULL == (options = guile_to_optionhash (cfg, action, 0)))
    FAIL ();                    /* Message already emitted.  */

  /* Check the servertype configuration definition for duplicates.  */
  err |= optionhash_validate (options, 1, "configuration", server->prefix);

  /* Now check all configuration items.  */
  svz_hash_foreach (servertype_config_internal, options, &x);

  /* Append the last configuration item identifying the end of the
     configuration item list.  */
  item.type = SVZ_ITEM_END;
  item.address = NULL;
  item.defaultable = 0;
  item.name = NULL;
  items_append (&items, x.count++, &item);

  /* Adjust the address values of the configuration items and assign
     all gathered information to the given servertype.  */
  for (n = 0; n < x.count; n++)
    items[n].address = (void *) ((unsigned long) items[n].address +
      (unsigned long) x.prototype);
  server->config_prototype.start = x.prototype;
  server->config_prototype.size = x.size;
#undef items        /* Unfortunately, tidy is incorrect for next line LHS.  */
  server->config_prototype.items = x.items;

#if 0
  guile_servertype_config_print (server);
#endif

 out:
  optionhash_destroy (options);
  return err;

#undef options
#undef err
#undef FUNC_NAME
}

SCM_DEFINE
(guile_define_servertype,
 "define-servertype!", 1, 0, 0,
 (SCM args),
 doc: /***********
Define a new server type based on @var{args}.  (If everything
works fine you have a freshly registered server type afterwards.)
Return @code{#t} on success.  */)
{
#define FUNC_NAME s_guile_define_servertype
  int n, err = 0;
  svz_hash_t *options;
  SCM proc;
  svz_servertype_t *server;
  svz_hash_t *functions;
  char action[ACTIONBUFSIZE];

  server = svz_calloc (sizeof (svz_servertype_t));
  DEFINING ("%s", "servertype");

  if (NULL == (options = guile_to_optionhash (args, action, 0)))
    FAIL ();                    /* Message already emitted.  */

  /* Obtain the servertype prefix variable (Mandatory).  */
  if (optionhash_extract_string (options, "prefix", 0, NULL,
                                 &server->prefix, action) != 0)
    FAIL ();
  DEFINING ("servertype `%s'", server->prefix);

  /* Check the servertype definition once.  */
  err |= optionhash_validate (options, 1, "servertype", server->prefix);

  /* Get the description of the server type.  */
  err |= optionhash_extract_string (options, "description", 0, NULL,
                                    &server->description, action);

  /* Set the procedures.  */
  functions = svz_hash_create (4, (svz_free_func_t) guile_unprotect);
  for (n = 0; guile_functions[n] != NULL; n++)
    {
      proc = SCM_UNDEFINED;
      err |= optionhash_extract_proc (options, guile_functions[n],
                                      1, SCM_UNDEFINED, &proc, action);
      svz_hash_put (functions, guile_functions[n], SVZ_NUM2PTR (proc));
      if (!SCM_UNBNDP (proc))
        gi_gc_protect (proc);
    }

  /* Check duplicate server types.  */
  if (svz_servertype_get (server->prefix, 0) != NULL)
    {
      guile_error ("Duplicate servertype definition: `%s'", server->prefix);
      err = -1;
    }
  else
    {
      /* Check the configuration items for this servertype.  */
      err |= guile_servertype_config (server,
                                      optionhash_get (options,
                                                      "configuration"));
    }

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
      server->reset = guile_func_reset;
      server->handle_request = guile_func_handle_request;
      guile_servertype_add (server, functions);
    }
  else
    {
      svz_free (server->prefix);
      if (server->description)
        svz_free (server->description);
      svz_free (server);
      svz_hash_destroy (functions);
    }

 out:
  optionhash_destroy (options);
  guile_global_error |= err;
  return err ? SCM_BOOL_F : SCM_BOOL_T;
#undef FUNC_NAME
}

/*
 * Destroys the servertype represented by the hash @var{callbacks}.  Removes
 * the servertype pointer from the hash, destroys the remaining callback
 * hash and finally frees all resources allocated by the servertype.
 */
static void
guile_servertype_destroy (svz_hash_t *callbacks)
{
  svz_servertype_t *stype;

  stype = svz_hash_delete (callbacks, "server-type");
  svz_hash_destroy (callbacks);
  svz_free (stype->prefix);
  svz_free (stype->description);
  guile_servertype_config_free (stype);
  svz_free (stype);
}

/*
 * Destroys the given socket represented by the hash @var{callbacks} and
 * destroy this hash recursively.
 */
static void
guile_sock_destroy (svz_hash_t *callbacks)
{
  svz_hash_destroy (callbacks);
}

#include "guile-api.c"

/*
 * Initialization of the guile server module.  Should be run before calling
 * @code{guile_eval_file}.  It registers some new guile procedures and
 * creates some static data.
 */
void
guile_server_init (void)
{
  if (guile_server || guile_sock)
    return;

  guile_server =
    svz_hash_create (4, (svz_free_func_t) guile_servertype_destroy);
  guile_sock =
    svz_hash_create (4, (svz_free_func_t) guile_sock_destroy);

  /* Initialize the guile SMOB things.  Previously defined via
     MAKE_SMOB_DEFINITION ().  */
  INIT_SMOB (svz_socket);
  INIT_SMOB (svz_server);
  INIT_SMOB (svz_servertype);

#include "guile-server.x"

  guile_bin_init ();
  guile_api_init ();
}

/*
 * This function should be called before shutting down the core library in
 * order to avoid memory leaks.  It releases the server types defined with
 * guile.
 */
void
guile_server_finalize (void)
{
  guile_api_finalize ();

  if (guile_server)
    {
      svz_hash_destroy (guile_server);
      guile_server = NULL;
    }

  if (guile_sock)
    {
      svz_hash_destroy (guile_sock);
      guile_sock = NULL;
    }
}

#else /* not ENABLE_GUILE_SERVER */

static int have_guile_server = 0;

#endif /* ENABLE_GUILE_SERVER */
