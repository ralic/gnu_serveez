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
 * $Id: guile-server.c,v 1.7 2001/07/13 14:54:51 ela Exp $
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
  return (void *) gh_cdr (smob);                                             \
}                                                                            \
int guile_##cfunc##_p (SCM smob) {                                           \
  return (SCM_NIMP (smob) && gh_car (smob) == guile_##cfunc##_tag) ? 1 : 0;  \
}                                                                            \
int guile_##cfunc##_print (SCM smob, SCM port, scm_print_state *state) {     \
  static char txt[256];                                                      \
  sprintf (txt, "#<%s %p>", description, (void *) gh_cdr (smob));            \
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

MAKE_SMOB_DEFINITION (svz_socket, "svz-socket")
MAKE_SMOB_DEFINITION (svz_server, "svz-server")
MAKE_SMOB_DEFINITION (svz_servertype, "svz-servertype")

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
  if (SCM_UNSPECIFIED == hvalue)
    {
      /* Nothing in hash, try to use default. */
      if (hasdef)
	*target = defvar;
      else
	{
	  report_error ("No default procedure for `%s' %s", key, txt);
	  err = 1;
	}
      return err;
    }

  /* Is that guile procedure ? */
  if (gh_procedure_p (hvalue))
    {
      *target = hvalue;
    }
  else if ((str = guile2str (hvalue)) != NULL)
    {
      if ((proc = gh_lookup (str)) != SCM_UNDEFINED && gh_procedure_p (proc))
	*target = proc;
      else
	{
	  report_error ("No such procedure `%s' for `%s' %s", str, key, txt);
	  err = 1;
	}
      scm_must_free (str);
    }
  else
    {
      report_error ("Invalid procedure for `%s' %s", key, txt);
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

  if ((proc = (SCM) ((unsigned long) svz_hash_get (gserver, func))) == 0)
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

  if ((proc = (SCM) ((unsigned long) svz_hash_get (gsock, func))) == 0)
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

  if ((oldproc = 
       ((SCM) (unsigned long) 
	svz_hash_put (gsock, func, (void *) ((unsigned long) proc)))) == 0)
    return SCM_UNDEFINED;

  return oldproc;
}

/*
 * Return a guile cell containing a unsigned long value. The given pointer
 * @var{ptr} is converted.
 * FIXME: Create unique cell types for svz_socket_t, svz_server_t and
 *        svz_servertype_t. (tags? MGrabMue?).
 */
static SCM
guile_ptr (void *ptr)
{
  return gh_ulong2scm ((unsigned long) ptr);
}

/*
 * Return an integer. If the given guile cell @var{value} is not such an 
 * integer the routine return the default value @var{def}.
 */
static int
guile_int (SCM value, int def)
{
  if (gh_number_p (value))
    return gh_scm2int (value);
  return def;
}

/* Wrapper function for the global initialization of a server type. */
static int
guile_func_global_init (svz_servertype_t *stype)
{
  SCM global_init = guile_servertype_getfunction (stype, "global-init");
  SCM ret;

  if (global_init != SCM_UNDEFINED)
    {
      ret = gh_call1 (global_init, MAKE_SMOB (svz_servertype, stype));
      return guile_int (ret, -1);
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

  if (init != SCM_UNDEFINED)
    {
      ret = gh_call1 (init, MAKE_SMOB (svz_server, server));
      return guile_int (ret, -1);
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

  if (detect_proto != SCM_UNDEFINED)
    {
      ret = gh_call2 (detect_proto, MAKE_SMOB (svz_server, server), 
		      MAKE_SMOB (svz_socket, sock));
      return guile_int (ret, 0);
    }
  return 0;
}

/* Wrapper for the socket disconnected callback. Used here in order to
   delete the additional guile callbacks associated with the disconnected
   socket structure. */
static int
guile_func_disconnected (svz_socket_t *sock)
{
  SCM ret, disconnected;
  int retval = -1;
  svz_hash_t *gsock;
  disconnected = guile_sock_getfunction (sock, "disconnected");

  /* First call the guile callback if necessary. */
  if (disconnected != SCM_UNDEFINED)
    {
      ret = gh_call1 (disconnected, MAKE_SMOB (svz_socket, sock));
      retval = guile_int (ret, -1);
    }

  /* Delete all the associated guile callbacks. */
  if ((gsock = svz_hash_delete (guile_sock, svz_itoa (sock->id))) != NULL)
    {
      svz_hash_destroy (gsock);
    }

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
  sock->disconnected_socket = guile_func_disconnected;

  if (connect_socket != SCM_UNDEFINED)
    {
      ret = gh_call2 (connect_socket, MAKE_SMOB (svz_server, server), 
		      MAKE_SMOB (svz_socket, sock));
      return guile_int (ret, 0);
    }
  return 0;
}

/* Wrapper for the finalization of a server instance. */
static int
guile_func_finalize (svz_server_t *server)
{
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM finalize = guile_servertype_getfunction (stype, "finalize");
  SCM ret;

  if (finalize != SCM_UNDEFINED)
    {
      ret = gh_call1 (finalize, MAKE_SMOB (svz_server, server));
      return guile_int (ret, -1);
    }
  return 0;
}

/* Wrappper routine for the global finalization of a server type. */
static int
guile_func_global_finalize (svz_servertype_t *stype)
{
  SCM ret, global_finalize;
  global_finalize = guile_servertype_getfunction (stype, "global-finalize");

  if (global_finalize != SCM_UNDEFINED)
    {
      ret = gh_call1 (global_finalize, MAKE_SMOB (svz_servertype, stype));
      return guile_int (ret, -1);
    }
  return 0;
}

/* Wrapper for the client info callback. */
static char *
guile_func_info_client (svz_server_t *server, svz_socket_t *sock)
{
  return NULL;
}

/* Wrapper for the server info callback. */
static char *
guile_func_info_server (svz_server_t *server)
{
  return NULL;
}

/* Wrapper for the server notifier callback. */
static int
guile_func_notify (svz_server_t *server)
{
  return -1;
}

/* Wrapper for the socket handle request callback. */
static int
guile_func_handle_request (svz_socket_t *sock, char *request, int len)
{
  SCM ret, handle_request;
  handle_request = guile_sock_getfunction (sock, "handle-request");

  if (handle_request != SCM_UNDEFINED)
    {
      ret = gh_call3 (handle_request, MAKE_SMOB (svz_socket, sock), 
		      gh_str2scm (request, len), gh_int2scm (len));
      return guile_int (ret, -1);
    }

  return -1;
}

/* Set the @code{handle_request} member of the socket structure @var{sock} 
   to the guile procedure @var{proc}. The procedure returns the previously
   set handler if there is any. */
#define FUNC_NAME "svz:sock:handle-request"
static SCM
guile_sock_handle_request (SCM sock, SCM proc)
{
  svz_socket_t *xsock;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  SCM_ASSERT_TYPE (gh_procedure_p (proc), proc, SCM_ARG2, 
		   FUNC_NAME, "procedure");

  xsock->handle_request = guile_func_handle_request;
  return guile_sock_setfunction (xsock, "handle-request", proc);
}
#undef FUNC_NAME

/* Setup the packet boundary of the socket @var{sock}. The given value
   @var{boundary} can contain any kind of data. */
#define FUNC_NAME "svz:sock:boundary"
static SCM
guile_sock_boundary (SCM sock, SCM boundary)
{
  svz_socket_t *xsock;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  SCM_ASSERT_TYPE (gh_string_p (boundary), boundary, SCM_ARG2, 
		   FUNC_NAME, "string");

  /* FIXME: leaking here ... */
  xsock->boundary = gh_scm2chars (boundary, NULL);
  xsock->boundary_size = gh_scm2int (scm_string_length (boundary));
  xsock->check_request = svz_sock_check_request;

  return SCM_BOOL_T;
}
#undef FUNC_NAME

/* Write @var{len} byte from the string buffer @var{buf} to the socket 
   @var{sock}. Return #t on success and #f on failure. */
#define FUNC_NAME "svz:sock:write"
static SCM
guile_sock_write (SCM sock, SCM buf, SCM len)
{
  svz_socket_t *xsock;
  SCM ret = SCM_BOOL_T;
  char *buffer;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  SCM_ASSERT_TYPE (gh_string_p (buf), buf, SCM_ARG2,
		   FUNC_NAME, "string");
  SCM_ASSERT_TYPE (gh_number_p (len), len, SCM_ARG3,
		   FUNC_NAME, "number");

  buffer = gh_scm2chars (buf, NULL);
  if (svz_sock_write (xsock, buffer, gh_scm2int (len)) == -1)
    {
      svz_sock_schedule_for_shutdown (xsock);
      ret = SCM_BOOL_F;
    }
  scm_must_free (buffer);
  return ret;
}
#undef FUNC_NAME

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

  if (NULL == (options = guile2optionhash (args, txt, 0)))
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
      svz_hash_put (functions, guile_functions[n], 
		    (void * ) ((unsigned long) proc));
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

/*
 * Initialization of the guile server module. Should be run before calling
 * @code{gh_eval_file}. It registeres some new guile procedures and creates
 * some static data.
 */
void
guile_server_init (void)
{
  if (guile_server || guile_sock)
    return;

  guile_server = svz_hash_create (4);
  guile_sock = svz_hash_create (4);
  gh_new_procedure ("define-servertype!", guile_define_servertype, 1, 0, 0);
  gh_new_procedure ("svz:sock:handle-request", 
		    guile_sock_handle_request, 2, 0, 0);
  gh_new_procedure ("svz:sock:boundary", guile_sock_boundary, 2, 0, 0);
  gh_new_procedure ("svz:sock:write", guile_sock_write, 3, 0, 0);

  /* Initialize the guile SMOB things. Previously defined via 
     MAKE_SMOB_DEFINITION (). */
  INIT_SMOB (svz_socket);
  INIT_SMOB (svz_server);
  INIT_SMOB (svz_servertype);
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

  if (guile_server)
    {
      svz_hash_foreach_key (guile_server, prefix, i)
	{
	  functions = svz_hash_get (guile_server, prefix[i]);
	  stype = svz_hash_get (functions, "server-type");
	  svz_hash_destroy (functions);
	  svz_free (stype->prefix);
	  svz_free (stype->description);
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
	  svz_hash_destroy (functions);
	}
      svz_hash_destroy (guile_sock);
      guile_sock = NULL;
    }
}

#else /* not ENABLE_GUILE_SERVER */

static int have_guile_server = 0;

#endif /* ENABLE_GUILE_SERVER */
