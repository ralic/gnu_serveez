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
 * $Id: guile-server.c,v 1.3 2001/06/30 13:26:49 ela Exp $
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

/* List of functions to configure. */
static char *guile_functions[] = {
  "global-init", "init", "detect-proto", "connect-socket", "finalize",
  "global-finalize", "info-client", "info-server", "notify", 
  "handle-request", NULL };

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
 * @var{server}. If the lookupp fails this routine return SCM_UNDEFINED.
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
      ret = gh_call1 (global_init, guile_ptr (stype));
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
      ret = gh_call1 (init, guile_ptr (server));
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
      ret = gh_call2 (detect_proto, guile_ptr (server), guile_ptr (sock));
      return guile_int (ret, 0);
    }
  return 0;
}

/* Wrapper function for the socket connection after successful detection. */
static int
guile_func_connect_socket (svz_server_t *server, svz_socket_t *sock)
{
  svz_servertype_t *stype = svz_servertype_find (server);
  SCM connect_socket = guile_servertype_getfunction (stype, "connect-socket");
  SCM ret;

  if (connect_socket != SCM_UNDEFINED)
    {
      ret = gh_call2 (connect_socket, guile_ptr (server), guile_ptr (sock));
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
      ret = gh_call1 (finalize, guile_ptr (server));
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
      ret = gh_call1 (global_finalize, guile_ptr (stype));
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
  return -1;
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
  SCM value, proc;
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
  if (guile_server)
    return;

  guile_server = svz_hash_create (4);
  gh_new_procedure ("define-servertype!", guile_define_servertype, 1, 0, 0);
}

/*
 * This function should be called before shutting down the core library in
 * order to avoid memory leaks. It releases the server types defined with
 * guile.
 */
void
guile_server_finalize (void)
{
  char **prefix;
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
}

#else /* not ENABLE_GUILE_SERVER */

static int have_guile_server = 0;

#endif /* ENABLE_GUILE_SERVER */
