/*
 * guile.c - interface to guile core library
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2001 Raimund Jacob <raimi@lkcc.org>
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
 * $Id: guile.c,v 1.19 2001/06/01 21:24:09 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <guile/gh.h>
#include <libguile.h>

#include "libserveez.h"

#define valid_port(port) ((port) > 0 && (port) < 65536)
#define FAIL() do { err = -1; goto out; } while(0)

/*
 * What is an 'optionhash' ?
 * We build up that data structure from a scheme pairlist. The pairlist has 
 * to be an alist which is a key => value mapping. We read that mapping and 
 * construct a @code{svz_hash_t} from it. The values of this hash are 
 * pointers to @code{value_t} structures. The @code{value_t} structure 
 * contains a @code{defined} field which counts the number of occurences of 
 * the key. Use @code{validate_optionhash} to make sure it is 1 for each key.
 * The @code{use} field is to make sure that each key was needed exactly once.
 * Use @code{validate_optionhash} again to find out which ones were not 
 * needed.
 */

/*
 * Used as value in option-hashes.
 */
typedef struct
{
  SCM value;     /* the scheme value itself, invalid when defined != 1 */
  int defined;   /* the number of definitions, 1 to be valid */
  int use;       /* how often was it looked up in the hash, 1 to be valid */
}
value_t;

/*
 * Create a new value_t structure with the given @var{value}. Initializes 
 * use to zero. The define counter is set to 1.
 */
static value_t *
new_value_t (SCM value)
{
  value_t *v = (value_t *) svz_malloc (sizeof (value_t));
  v->value = value;
  v->defined = 1;
  v->use = 0;
  return v;
}

/*
 * Destroy the given option-hash @var{options}.
 */
static void
optionhash_destroy (svz_hash_t *options)
{
  value_t **value;
  int n;

  if (options)
    {
      svz_hash_foreach_value (options, value, n)
	svz_free (value[n]);
      svz_hash_destroy (options);
    }
}

/*
 * Report some error at the current scheme position. Prints to stderr
 * but lets the program continue. The format string @var{format} does not 
 * need a trailing newline.
 */
static void
report_error (const char* format, ...)
{
  va_list args;
  SCM lp = scm_current_load_port ();
  char *file = SCM_PORTP (lp) ? gh_scm2newstr (SCM_FILENAME (lp), NULL) : NULL;

  fprintf (stderr, "%s:%d:%d: ", file ? file : "undefined", 
	   SCM_PORTP (lp) ? SCM_LINUM (lp) : 0, 
	   SCM_PORTP (lp) ? SCM_COL (lp) : 0);
  if (file)
    free (file);

  va_start (args, format);
  vfprintf (stderr, format, args);
  va_end (args);

  fprintf (stderr, "\n");
}

/* ********************************************************************** */

/*
 * Converts SCM to char * no matter if it is string or symbol. Returns NULL
 * when it was neither. New string must be explicitly free()d.
 */
#define guile2str(scm) \
  (gh_string_p (scm) ? gh_scm2newstr (scm, NULL) : \
  (gh_symbol_p (scm) ? gh_symbol2newstr (scm, NULL) : NULL))


/* ********************************************************************** */

/*
 * Validate the values of an option-hash. Returns the number of errors.
 * what = 0 : check if all 'use' fields are 1
 * what = 1 : check if all 'defined' fields are 1
 * type     : what kind of thing the option-hash belongs to
 * name     : current variable name
 */
static int
validate_optionhash (svz_hash_t *hash, int what, char *type, char *name)
{
  int errors = 0, i;
  char **keys;

  svz_hash_foreach_key (hash, keys, i)
    {
      char *key = keys[i];
      value_t *value = (value_t *) svz_hash_get (hash, key);

      if (what)
	{
	  if (value->defined != 1)
	    {
	      errors++;
	      report_error("`%s' is defined multiple times in %s `%s'",
			   key, type, name);
	    }
	}
      else
	{
	  if (value->use == 0)
	    {
	      errors++;
	      report_error("`%s' is defined but never used in %s `%s'",
			   key, type, name);
	    }
	}
    }

  return errors;
}

/*
 * Get a scheme value from an option-hash and increment its 'use' field.
 * Returns SCM_UNSPECIFIED when key was not found.
 */
static SCM
optionhash_get (svz_hash_t *hash, char *key)
{
  value_t *val = (value_t*) svz_hash_get (hash, key);

  if (NULL != val)
    {
      val->use++;
      return val->value;
    }
  return SCM_UNSPECIFIED;
}

/*
 * Traverse a scheme pairlist that is an associative list and build up
 * a hash from it. Emits error messages and returns NULL when it did so.
 * Hash keys are the key names. Hash values are pointers to value_t structs.
 */
static svz_hash_t *
guile2optionhash (SCM pairlist, char *msg)
{
  svz_hash_t *hash = svz_hash_create (10);
  value_t *old_value;
  value_t *new_value;
  int err = 0;

  /* have to unpack that list again... why ? */
  pairlist = gh_car (pairlist);

  for ( ; gh_pair_p (pairlist); pairlist = gh_cdr (pairlist))
    {
      SCM pair = gh_car (pairlist);
      SCM key, val;
      char *tmp = NULL;

      /* the car must be another pair which contains key and value */
      if (!gh_pair_p (pair))
	{
	  report_error ("not a pair");
	  err = 1;
	  break;
	}
      key = gh_car (pair);
      val = gh_cdr (pair);

      if (NULL == (tmp = guile2str (key)))
	{
	  /* unknown key type, must be string or symbol */
	  report_error ("Key must be string or symbol %s", msg);
	  err = 1;
	  break;
	}

      /* remember key and free it */
      new_value = new_value_t (val);
      old_value = svz_hash_get (hash, tmp);

      if (NULL != old_value)
	{
	  /* multiple definition, let caller croak about that error */
	  new_value->defined += old_value->defined;
	  svz_free_and_zero (old_value);
	}
      svz_hash_put (hash, tmp, (void *) new_value);
      free (tmp);
    }

  /* pairlist must be gh_null_p() now or that was not a good pairlist... */
  if (!err && !gh_null_p (pairlist))
    {
      report_error ("invalid pairlist");
      err = 1;
    }

  if (err)
    {
      svz_hash_destroy (hash);
      return NULL;
    }

  return hash;
}


/* ********************************************************************** */

/*
 * Parse an integer value from a scheme cell. Returns zero when successful 
 * and stores the integer value where @var{target} points to. Does not emit 
 * error messages.
 */
static int
guile2int (SCM scm, int *target)
{
  int err = 0;
  char *asstr = NULL;
  if (gh_number_p (scm))                       /* yess... we got it */
    {
      *target = gh_scm2int (scm);
    }
  else if (NULL != (asstr = guile2str (scm)))  /* try harder        */
    {
      char *endp;
      *target = strtol (asstr, &endp, 10);
      if (*endp != '\0' || errno == ERANGE)
	{
	  /* not parsable... */
	  err = 1;
	}
      free (asstr);
    }
  else                                         /* no chance         */
    {
      err = 2;
    }
  return err;
}


/*
 * Extract an integer value from an option hash. Returns zero if it worked.
 */
static int
optionhash_extract_int (svz_hash_t *hash,
			char *key,          /* the key to find      */
			int hasdef,         /* is there a default ? */
			int defvar,         /* the default          */
			int *target,        /* where to put it      */
			char *msg           /* appended to errormsg */
			)
{
  int err = 0;
  SCM hvalue = optionhash_get (hash, key);

  if (SCM_UNSPECIFIED == hvalue)
    {
      /* nothing in hash, try to use default */
      if (hasdef)
	{
	  *target = defvar;
	}
      else
	{
	  err = 1;
	  report_error ("No default value for integer `%s' %s", key, msg);
	}
    }
  else
    {
      /* convert something */
      if (guile2int (hvalue, target))
	{
	  err = 1;
	  report_error ("Invalid integer value for `%s' %s", key, msg);
	}
    }
  return err;
}

/*
 * Exctract a string value from an option hash. Returns zero if it worked.
 * The memory for the string is newly allocated, no matter where it came
 * from.
 */
static int
optionhash_extract_string (svz_hash_t *hash,
			   char *key,        /* the key to find       */
			   int hasdef,       /* if there is a default */
			   char *defvar,     /* default               */
			   char **target,    /* where to put it       */
			   char *msg         /* appended to errormsg  */
			   )
{
  int err = 0;
  char *tmp = NULL;
  SCM hvalue = optionhash_get (hash, key);

  if (SCM_UNSPECIFIED == hvalue)
    {
      /* nothing in hash, try to use default */
      if (hasdef)
	{
	  *target = svz_strdup (defvar);
	}
      else
	{
	  err = 1;
	  report_error ("No default value for string `%s' %s", key, msg);
	}
    }
  else
    {
      if (NULL == (tmp = guile2str (hvalue)))
	{
	  err = 1;
	  report_error ("`%s' must be a string value %s", key, msg);
	}
      else
	{
	  *target = svz_strdup (tmp);
	  free (tmp);
	}
    }

  return err;
}

static int
optionhash_cb_before (char *servername, void *arg)
{
  svz_hash_t *options = (svz_hash_t *) arg;
  if (0 == validate_optionhash (options, 1, "server", servername))
    return SVZ_ITEM_OK;
  return SVZ_ITEM_FAILED;
}

static int
optionhash_cb_integer (char *servername, void *arg, char *key, int *target,
		       int hasdef, int def)
{
  svz_hash_t *options = (svz_hash_t *) arg;
  return SVZ_ITEM_DEFAULT_ERRMSG;
}

static int
optionhash_cb_boolean (char *servername, void *arg, char *key, int *target,
		       int hasdef, int def)
{
  svz_hash_t *options = (svz_hash_t *) arg;
  return SVZ_ITEM_DEFAULT_ERRMSG;
}

static int
optionhash_cb_intarray (char *servername, void *arg, char *key,
			svz_array_t **target, int hasdef,
			svz_array_t *def)
{
  svz_hash_t *options = (svz_hash_t *) arg;
  return SVZ_ITEM_DEFAULT_ERRMSG;
}

static int
optionhash_cb_string (char *servername, void *arg, char *key, 
		      char **target, int hasdef, char *def)
{
  svz_hash_t *options = (svz_hash_t *) arg;
  return SVZ_ITEM_DEFAULT_ERRMSG;
}

static int
optionhash_cb_strarray (char *servername, void *arg, char *key,
			svz_array_t **target, int hasdef,
			svz_array_t *def)
{
  svz_hash_t *options = (svz_hash_t *) arg;
  return SVZ_ITEM_DEFAULT_ERRMSG;
}

static int
optionhash_cb_hash (char *servername, void *arg, char *key,
		    svz_hash_t **target, int hasdef,
		    svz_hash_t *def)
{
  svz_hash_t *options = (svz_hash_t *) arg;
  return SVZ_ITEM_DEFAULT_ERRMSG;
}

static int
optionhash_cb_portcfg (char *servername, void *arg, char *key,
		       svz_portcfg_t **target, int hasdef,
		       svz_portcfg_t *def)
{
  svz_hash_t *options = (svz_hash_t *) arg;
  return SVZ_ITEM_DEFAULT_ERRMSG;
}

static int
optionhash_cb_after (char *servername, void *arg)
{
  svz_hash_t *options = (svz_hash_t *) arg;
  if (0 == validate_optionhash (options, 0, "server", servername))
    return SVZ_ITEM_OK;
  return SVZ_ITEM_FAILED;
}


/*
 * Generic server definition. Use two arguments:
 * First is a (unique) server name of the form "type-something" where
 * "type" is the shortname of a servertype. Second is the optionhash that
 * is special for the server. Uses library to configure the individual options.
 * Emits error messags (to stderr). Returns #t when server got defined, #f
 * in case of any error.
 */
#define FUNC_NAME "define-server!"
SCM
guile_define_server (SCM name, SCM args)
{
  int err = 0;
  char *servername = guile2str (name);
  char *servertype = NULL, *p = NULL;
  svz_hash_t *options = NULL;
  svz_servertype_t *stype;
  svz_server_t *server = NULL;
  char *msg = svz_malloc (256);

  svz_server_config_t configure = {
    optionhash_cb_before,    /* before */
    optionhash_cb_integer,   /* integers */
    optionhash_cb_boolean,   /* boolean */
    optionhash_cb_intarray,  /* integer arrays */
    optionhash_cb_string,    /* strings */
    optionhash_cb_strarray,  /* string arrays */
    optionhash_cb_hash,      /* hashes */
    optionhash_cb_portcfg,   /* port configurations */
    optionhash_cb_after      /* after */
  };


  /* check if the given name is valid */
  if (NULL == servername)
    {
      report_error ("Invalid servername");
      FAIL();
    }

  svz_snprintf (msg, 256, "while defining server `%s'", servername);
    
  /* extract options */
  if (NULL == (options = guile2optionhash (args, msg)))
    FAIL(); /* message already emitted */

  /* Seperate server description. */
  p = servertype = svz_strdup (servername);
  while (*p && *p != '-')
    p++;

  /* extract server type and sanity check */
  if (*p == '-' && *(p+1) != '\0')
    {
      *p = '\0';
    }
  else
    {
      report_error ("`%s' is not a valid server name");
      FAIL();
    }

  /* find the definition by lookup with dynamic loading */
  if (NULL == (stype = svz_servertype_get (servertype, 1)))
    {
      report_error ("no such server type `%s'", servertype);
      FAIL();
    }

  /* FIXME: dupecheck ? */
  server = svz_server_instantiate (stype, servername);

  /* config anlegen server->cfg */
  server->cfg = svz_server_configure (stype,
				      servername,
				      (void *) options,
				      &configure);
  if (NULL == server->cfg)
    {
      svz_server_free (server);
      FAIL(); /* messages emitted from callbacks */
    }

  /* add server if config is ok, no error yet */
  if (!err)
    svz_server_add (server);
  else
    svz_server_free (server);

#if ENABLE_DEBUG
  svz_log (LOG_DEBUG, "defining server `%s' (%s)\n", servername, 
	   servertype);
#endif

 out:
  svz_free (msg);
  svz_free (servertype);
  free (servername);
  optionhash_destroy (options);
  return err ? SCM_BOOL_T : SCM_BOOL_F;
}
#undef FUNC_NAME


/*
 * Generic port configuration definition. Use two arguments:
 * First is a (unique) name for the prot. Second is an optionhash for various
 * settings. Returns #t when definition worked, #f when it did not. Emits
 * error messages (to stderr).
 */
#define FUNC_NAME "define-port!"
static SCM
guile_define_port (SCM symname, SCM args)
{
  int err = 0;
  svz_portcfg_t *prev = NULL;
  svz_portcfg_t *cfg = svz_portcfg_create ();
  svz_hash_t *options = NULL;
  char *portname = guile2str (symname);
  char *proto = NULL; 
  char *msg = svz_malloc (256);

  if (portname == NULL)
    {
      report_error ("first argument to " FUNC_NAME 
		    " must be string or symbol");
      FAIL();
    }

  svz_snprintf (msg, 256, "when defining port `%s'", portname);

  if (NULL == (options = guile2optionhash (args, msg)))
    FAIL();       /* message already emitted */

  /* every key defined only once ? */
  if (0 != validate_optionhash (options, 1, "port", portname))
    err = -1;

  /* find out what protocol this portcfg will be about */
  if (NULL == (proto = guile2str (optionhash_get (options, PORTCFG_PROTO))))
    {
      report_error ("port `%s' requires a \"proto\" field", portname);
      FAIL();
    }


  if (!strcmp (proto, PORTCFG_TCP))
    {
      int port;
      cfg->proto = PROTO_TCP;
      err |= optionhash_extract_int (options, PORTCFG_PORT, 0, 0, &port, msg);
      cfg->tcp_port = (short) port;
      if (!valid_port (port))
	{
	  report_error ("invalid port number %s", msg);
	  err = -1;
	}
      err |= optionhash_extract_int (options, PORTCFG_BACKLOG, 1, 0, 
				     &(cfg->tcp_backlog), msg);
      err |= optionhash_extract_string (options, PORTCFG_IP, 1, PORTCFG_NOIP,
					&(cfg->tcp_ipaddr), msg);
      err |= svz_portcfg_mkaddr (cfg);
    }
  else if (!strcmp (proto, PORTCFG_UDP))
    {
      int port;
      cfg->proto = PROTO_UDP;
      err |= optionhash_extract_int (options, PORTCFG_PORT, 0, 0, &port, msg);
      cfg->udp_port = (short) port;
      if (!valid_port (port))
	{
	  report_error ("invalid port number %s", msg);
	  err = -1;
	}
      err |= optionhash_extract_string (options, PORTCFG_IP, 1, PORTCFG_NOIP,
				 &(cfg->udp_ipaddr), msg);
      err |= svz_portcfg_mkaddr (cfg);
    }
  else if (!strcmp (proto, PORTCFG_ICMP))
    {
      int type;
      cfg->proto = PROTO_ICMP;
      err |= optionhash_extract_string (options, PORTCFG_IP, 1, PORTCFG_NOIP,
					&(cfg->icmp_ipaddr), msg);
      err |= optionhash_extract_int (options, PORTCFG_TYPE, 1, ICMP_SERVEEZ, 
				     &type, msg);
      if ((type & ~0xFF) != 0)
	{
	  report_error ("key '" PORTCFG_TYPE "' must be a byte %s", msg);
	  err = -1;
	}
      cfg->icmp_type = (char) (type & 0xFF);
      err |= svz_portcfg_mkaddr (cfg);
    }
  else if (!strcmp (proto, PORTCFG_RAW))
    {
      cfg->proto = PROTO_RAW;
      err |= optionhash_extract_string (options, PORTCFG_IP, 1, PORTCFG_NOIP,
					&(cfg->raw_ipaddr), msg);
      err |= svz_portcfg_mkaddr (cfg);
    }
  else if (!strcmp (proto, PORTCFG_PIPE))
    {
      cfg->proto = PROTO_PIPE;
      /* FIXME: implement me */
    }
  else
    {
      report_error ("invalid \"proto\" field `%s' in `%s'.",
		    proto, portname);
      FAIL();
    }
  svz_free (msg);
  free (proto);
  
  /* check for unused keys in input */
  if (0 != validate_optionhash (options, 0, "port", portname))
    FAIL(); /* message already emitted */

  /* now remember the name and add that config */
  cfg->name = svz_strdup (portname);

  if (err)
    {
      FAIL();
    }
  else
    {
      /* FIXME: remove when it works */
      svz_portcfg_print (cfg, stdout);
      prev = svz_portcfg_add (portname, cfg);
      
      if (prev != cfg)
	{
	  /* we've overwritten something. report and dispose */
	  report_error ("overwriting previous definition of port `%s'",
			portname);
	  svz_portcfg_destroy (prev);
	}
    }

 out:
  if (err)
    svz_portcfg_destroy (cfg);
  free (portname);
  optionhash_destroy (options);
  return err ? SCM_BOOL_F : SCM_BOOL_T;
}
#undef FUNC_NAME

/*
 * Generic port -> server(s) port binding ...
 */
#define FUNC_NAME "bind-server!"
SCM
guile_bind_server (SCM port, SCM server, SCM args /* FIXME: further servers */)
{
  char *portname = guile2str (port);
  char *servername = guile2str (server);
  svz_server_t *s;
  svz_portcfg_t *p;
  int error = 0;

  /* Check id there is such a port configuration. */
  if ((p = svz_portcfg_get (portname)) == NULL)
    {
      report_error ("no such port: %s", portname);
      error++;
    }

  /* Get one of the servers in the list. */
  if ((s = svz_server_get (servername)) == NULL)
    {
      report_error ("no such server: %s", servername);
      error++;
    }
  
  /* Bind a given server instance to a port configuration. */
  if (s != NULL && p != NULL)
    {
      if (svz_server_bind (s, p) < 0)
	error++;
    }

  return error ? SCM_BOOL_F : SCM_BOOL_T;
}
#undef FUNC_NAME

/*
 * Initialize Guile.
 */
static void
guile_init (void)
{
  SCM def_serv;

  /* define some variables */
  gh_define ("serveez-version", gh_str02scm (svz_version));
  gh_define ("guile-version", scm_version ());
  gh_define ("have-debug", gh_bool2scm (svz_have_debug));
  gh_define ("have-Win32", gh_bool2scm (svz_have_Win32));
  gh_define ("have-floodprotect", gh_bool2scm (svz_have_floodprotect));

  gh_define ("serveez-verbosity", gh_int2scm (svz_verbosity));
  gh_define ("serveez-sockets", gh_int2scm (svz_config.max_sockets));
  gh_define ("serveez-pass", gh_str02scm (svz_config.server_password));

  /* export some new procedures */
  def_serv = gh_new_procedure ("define-port!", guile_define_port, 1, 0, 2);
  def_serv = gh_new_procedure ("define-server!", guile_define_server, 1, 0, 2);
  def_serv = gh_new_procedure ("bind-server!", guile_bind_server, 2, 0, 3);
}

/*
 * Exception handler for guile. It is called if the evaluation of the given
 * file failed.
 */
static SCM 
guile_exception (void *data, SCM tag, SCM args)
{
  fprintf (stderr, "exception: ");
  gh_display (tag);
  fprintf (stderr, ": ");
  gh_display (args);
  gh_newline ();
  return SCM_BOOL_F;
}

/*
 * Get server settings from the file @var{cfgfile} and instantiate servers 
 * as needed. Return non-zero on errors.
 */
int
guile_load_config (char *cfgfile)
{
  guile_init ();
  if (gh_eval_file_with_catch (cfgfile, guile_exception) == SCM_BOOL_F)
    return -1;
  return 0;
}
