/*
 * guile.c - interface to guile core library
 *
 * Copyright (C) 2001 Raimund Jacob <raimi@lkcc.org>
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
 * $Id: guile.c,v 1.26 2001/06/08 15:37:37 ela Exp $
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
#if GUILE_SOURCE
# include <libguile/gh.h>
#else
# include <guile/gh.h>
#endif

#include "libserveez.h"

/* FAIL breaks to the labe `out' and sets an error condition. */
#define FAIL() do { err = -1; goto out; } while(0)

/*
 * Global error flag that indicating failure of one of the parsing 
 * functions.
 */
static int guile_global_error = 0;

/* 
 * Global variable containing the current load port in exceptions. 
 * FIXME: Where should I aquire it ? In each function ?
 */
static SCM guile_load_port = 0;

/*
 * What is an 'option-hash' ?
 * We build up that data structure from a scheme pairlist. The pairlist has 
 * to be an alist which is a key => value mapping. We read that mapping and 
 * construct a @code{svz_hash_t} from it. The values of this hash are 
 * pointers to @code{guile_value_t} structures. The @code{guile_value_t} 
 * structure contains a @code{defined} field which counts the number of 
 * occurences of the key. Use @code{optionhash_validate} to make sure it 
 * is 1 for each key. The @code{use} field is to make sure that each key 
 * was needed exactly once. Use @code{optionhash_validate} again to find 
 * out which ones were not needed.
 */

/*
 * Used as value in option-hashes.
 */
typedef struct guile_value
{
  SCM value;     /* the scheme value itself, invalid when defined != 1 */
  int defined;   /* the number of definitions, 1 to be valid */
  int use;       /* how often was it looked up in the hash, 1 to be valid */
}
guile_value_t;

/*
 * Create a guile value structure with the given @var{value}. Initializes 
 * the usage counter to zero. The define counter is set to 1.
 */
static guile_value_t *
guile_value_create (SCM value)
{
  guile_value_t *v = svz_malloc (sizeof (guile_value_t));
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
  guile_value_t **value;
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
report_error (const char *format, ...)
{
  va_list args;
  /* FIXME: Why is this port undefined in guile exceptions ? */
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

/*
 * Converts @code{SCM} into @code{char *} no matter if it is string or 
 * symbol. Returns @code{NULL} if it was neither. The new string must be 
 * explicitly @code{free()}d.
 */
#define guile2str(scm)                                        \
  (gh_null_p (scm) ? NULL :                                   \
  (gh_string_p (scm) ? gh_scm2newstr (scm, NULL) :            \
  (gh_symbol_p (scm) ? gh_symbol2newstr (scm, NULL) : NULL)))

/*
 * Validate the values of an option-hash. Returns the number of errors.
 * what = 0 : check if all 'use' fields are 1
 * what = 1 : check if all 'defined' fields are 1
 * type     : what kind of thing the option-hash belongs to
 * name     : current variable name (specifying the alist)
 */
static int
optionhash_validate (svz_hash_t *hash, int what, char *type, char *name)
{
  int errors = 0, i;
  char **keys;
  char *key;
  guile_value_t *value;

  /* Go through each key in the hash. */
  svz_hash_foreach_key (hash, keys, i)
    {
      key = keys[i];
      value = (guile_value_t *) svz_hash_get (hash, key);

      switch (what)
	{
	  /* Check definition counter. */
	case 1:
	  if (value->defined != 1)
	    {
	      errors++;
	      report_error ("Multiple definitions of `%s' in %s `%s'",
			    key, type, name);
	    }
	  break;
	  /* Check usage counter. */
	case 0:
	  if (value->use == 0)
	    {
	      errors++;
	      report_error ("Unused variable `%s' in %s `%s'",
			    key, type, name);
	    }
	  break;
	}
    }

  return errors;
}

/*
 * Get a scheme value from an option-hash and increment its 'use' field.
 * Returns SCM_UNSPECIFIED when @var{key} was not found.
 */
static SCM
optionhash_get (svz_hash_t *hash, char *key)
{
  guile_value_t *val = svz_hash_get (hash, key);

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
 * Hash keys are the key names. Hash values are pointers to guile_value_t 
 * structures. If @var{dounpack} is set the pairlist's car is used instead
 * of the pairlist itself. You have to unpack when no "." is in front of
 * the pairlist definitiion (in scheme code). Some please explain the "." to
 * Ela and Raimi...
 */
static svz_hash_t *
guile2optionhash (SCM pairlist, char *txt, int dounpack)
{
  svz_hash_t *hash = svz_hash_create (10);
  guile_value_t *old_value;
  guile_value_t *new_value;
  int err = 0;

  /* Unpack if requested, ignore if null already (null == not existent). */
  if (dounpack && !gh_null_p (pairlist))
    pairlist = gh_car (pairlist);

  for ( ; gh_pair_p (pairlist); pairlist = gh_cdr (pairlist))
    {
      SCM pair = gh_car (pairlist);
      SCM key, val;
      char *str = NULL;

      /* The car must be another pair which contains key and value. */
      if (!gh_pair_p (pair))
	{
	  report_error ("Not a pair %s", txt);
	  err = 1;
	  break;
	}
      key = gh_car (pair);
      val = gh_cdr (pair);

      if (NULL == (str = guile2str (key)))
	{
	  /* Unknown key type, must be string or symbol. */
	  report_error ("Key must be string or symbol %s", txt);
	  err = 1;
	  break;
	}

      /* Remember key and free it. */
      new_value = guile_value_create (val);
      if (NULL != (old_value = svz_hash_get (hash, str)))
	{
	  /* Multiple definition, let caller croak about that error. */
	  new_value->defined += old_value->defined;
	  svz_free_and_zero (old_value);
	}
      svz_hash_put (hash, str, (void *) new_value);
      free (str);
    }

  /* Pairlist must be gh_null_p() now or that was not a good pairlist. */
  if (!err && !gh_null_p (pairlist))
    {
      report_error ("Invalid pairlist %s", txt);
      err = 1;
    }

  if (err)
    {
      svz_hash_destroy (hash);
      return NULL;
    }

  return hash;
}

/*
 * Parse an integer value from a scheme cell. Returns zero when successful.
 * Stores the integer value where @var{target} points to. Does not emit 
 * error messages.
 */
static int
guile2int (SCM scm, int *target)
{
  int err = 0;
  char *str = NULL, *endp;

  /* Usual guile number. */
  if (gh_number_p (scm))
    {
      *target = gh_scm2int (scm);
    }
  /* Try string (or even symbol) to integer conversion. */
  else if (NULL != (str = guile2str (scm)))
    {
      errno = 0;
      *target = strtol (str, &endp, 10);
      if (*endp != '\0' || errno == ERANGE)
	err = 1;
      free (str);
    }
  /* No chance. */
  else
    {
      err = 2;
    }
  return err;
}

/*
 * Parse a boolean value from a scheme cell. We consider integers and #t/#f
 * as boolean boolean values. Represented as integers internally. Returns
 * zero when successful. Stores the boolean/integer where @var{target} points
 * to. Does not emit error messages.
 */
static int
guile2boolean (SCM scm, int *target)
{
  int i;
  int err = 0;
  char *str;

  /* Usual guile boolean. */
  if (gh_boolean_p (scm))
    {
      i = gh_scm2bool (scm);
      *target = (i == 0 ? 0 : 1);
    }
  /* Try with the integer converter. */
  else if (guile2int (scm, &i) == 0)
    {
      *target = (i == 0 ? 0 : 1);
    }
  /* Neither integer nor boolean, try text conversion. */
  else if ((str = guile2str (scm)) != NULL)
    {
      if (!svz_strcasecmp (str, "yes") ||
	  !svz_strcasecmp (str, "on") ||
	  !svz_strcasecmp (str, "true"))
	*target = 1;
      else if (!svz_strcasecmp (str, "no") ||
	       !svz_strcasecmp (str, "off") ||
	       !svz_strcasecmp (str, "false"))
	*target = 0;
      else
	err = 1;
    }
  else
    {
      err = 2;
    }
  return err;
}

/*
 * Convert the given scheme cell @var{list} which needs to be a valid guile
 * list into an array of duplicated strings. Returns @code{NULL} if it is not
 * a valid guile list. Print an error message if one of the list's elements
 * is not a string. The additional argument @var{func} should be the name of 
 * the calling function.
 */
static svz_array_t *
guile2strarray (SCM list, char *func)
{
  svz_array_t *array;
  int i;
  char *str;

  /* Check if the given scheme cell is a valid list. */
  if (!gh_list_p (list))
    return NULL;

  /* Iterate over the list and build up the array of strings. */
  array = svz_array_create (gh_length (list));
  for (i = 0; gh_pair_p (list); list = gh_cdr (list), i++)
    {
      if ((str = guile2str (gh_car (list))) == NULL)
	{
	  report_error ("%s: String expected in position %d", func, i);
	  guile_global_error = -1;
	  continue;
	}
      svz_array_add (array, svz_strdup (str));
      free (str);
    }

  /* Check the size of the resulting string array. */
  if (svz_array_size (array) == 0)
    {
      svz_array_destroy (array);
      array = NULL;
    }
  return array;
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
			char *txt)          /* appended to error    */
{
  int err = 0;
  SCM hvalue = optionhash_get (hash, key);

  /* Is there such an interger in the option-hash ? */
  if (SCM_UNSPECIFIED == hvalue)
    {
      /* Nothing in hash, try to use default. */
      if (hasdef)
	*target = defvar;
      else
	{
	  report_error ("No default value for integer `%s' %s", key, txt);
	  err = 1;
	}
    }
  /* Convert the integer value. */
  else if (guile2int (hvalue, target))
    {
      report_error ("Invalid integer value for `%s' %s", key, txt);
      err = 1;
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
			   char *txt)        /* appended to error     */
{
  SCM hvalue = optionhash_get (hash, key);
  int err = 0;
  char *str = NULL;

  /* Is there such a string in the option-hash ? */
  if (SCM_UNSPECIFIED == hvalue)
    {
      /* Nothing in hash, try to use default. */
      if (hasdef)
	*target = svz_strdup (defvar);
      else
	{
	  report_error ("No default value for string `%s' %s", key, txt);
	  err = 1;
	}
    }
  else
    {
      /* Try geting the character string. */
      if (NULL == (str = guile2str (hvalue)))
	{
	  report_error ("Invalid string value for `%s' %s", key, txt);
	  err = 1;
	}
      else
	{
	  *target = svz_strdup (str);
	  free (str);
	}
    }
  return err;
}

/* Before callback for configuring a server. */
static int
optionhash_cb_before (char *server, void *arg)
{
  svz_hash_t *options = arg;
  if (0 == optionhash_validate (options, 1, "server", server))
    return SVZ_ITEM_OK;
  return SVZ_ITEM_FAILED;
}

/* Integer callback for configuring a server. */
static int
optionhash_cb_integer (char *server, void *arg, char *key, int *target,
		       int hasdef, int def)
{
  svz_hash_t *options = arg;
  SCM hvalue = optionhash_get (options, key);
  
  if (SCM_UNSPECIFIED == hvalue)
    {
      if (hasdef)
	return SVZ_ITEM_DEFAULT_ERRMSG;

      report_error ("%s: You have to define an integer called `%s'",
		    server, key);
      return SVZ_ITEM_FAILED;
    }

  if (guile2int (hvalue, target))
    {
      report_error ("%s: Invalid integer value for `%s'", server, key);
      return SVZ_ITEM_FAILED;
    }

  return SVZ_ITEM_OK;
}

/* Boolean callback for configuring a server. */
static int
optionhash_cb_boolean (char *server, void *arg, char *key, int *target,
		       int hasdef, int def)
{
  svz_hash_t *options = arg;
  SCM hvalue = optionhash_get (options, key);

  if (SCM_UNSPECIFIED == hvalue)
    {
      if (hasdef)
	return SVZ_ITEM_DEFAULT_ERRMSG;

      report_error ("%s: You have to define a boolean called `%s'",
		    server, key);
      return SVZ_ITEM_FAILED;
    }

  if (guile2boolean (hvalue, target))
    {
      report_error ("%s: Invalid boolean value for `%s'", server, key);
      return SVZ_ITEM_FAILED;
    }

  return SVZ_ITEM_OK;
}

/* Integer array callback for configuring a server. */
static int
optionhash_cb_intarray (char *server, void *arg, char *key,
			svz_array_t **target, int hasdef,
			svz_array_t *def)
{
  svz_hash_t *options = arg;
  SCM hvalue = optionhash_get (options, key);

  /* Is that integer array defined in the option-hash ? */
  if (SCM_UNSPECIFIED == hvalue)
    {
      if (hasdef)
	return SVZ_ITEM_DEFAULT_ERRMSG;

      report_error ("%s: You have to define a integer array called `%s'",
		    server, key);
      return SVZ_ITEM_FAILED;
    }
  /* Yes, start parsing it. */
  else
    {
      int err = 0, i, val;
      svz_array_t *array;

      /* Is it a valid list ? */
      if (!gh_list_p (hvalue))
	{
	  report_error ("%s: Integer array `%s' is not a valid list",
			server, key);
	  return SVZ_ITEM_FAILED;
	}

      /* Iterate over the list and build up the array of integers. */
      array = svz_array_create (0);
      for (i = 0; gh_pair_p (hvalue); hvalue = gh_cdr (hvalue), i++)
	{
	  if (guile2int (gh_car (hvalue), &val))
	    {
	      report_error ("%s: Invalid integer element #%d in "
			    "integer array `%s'", server, i, key);
	      err = -1;
	      continue;
	    }
	  svz_array_add (array, (void *) val);
	}

      if (err)
	{
	  /* Free the integer array so far. */
	  svz_array_destroy (array);
	  return SVZ_ITEM_FAILED;
	}

      /* Yippie, have it. */
      *target = array;
    }
  return SVZ_ITEM_OK;
}

/* String callback for configuring a server. */
static int
optionhash_cb_string (char *server, void *arg, char *key, 
		      char **target, int hasdef, char *def)
{
  svz_hash_t *options = arg;
  SCM hvalue = optionhash_get (options, key);
  char *str;

  if (SCM_UNSPECIFIED == hvalue)
    {
      if (hasdef)
	return SVZ_ITEM_DEFAULT_ERRMSG;

      report_error ("%s: You have to define a string called `%s'",
		    server, key);
      return SVZ_ITEM_FAILED;
    }

  if (NULL == (str = guile2str (hvalue)))
    {
      report_error ("%s: Invalid string value for `%s'", server, key);
      return SVZ_ITEM_FAILED;
    }

  *target = svz_strdup (str);
  free (str);
  return SVZ_ITEM_OK;
}

/* String array callback for configuring a server. */
static int
optionhash_cb_strarray (char *server, void *arg, char *key,
			svz_array_t **target, int hasdef,
			svz_array_t *def)
{
  svz_hash_t *options = arg;
  SCM hvalue = optionhash_get (options, key);

  if (SCM_UNSPECIFIED == hvalue)
    {
      if (hasdef)
	return SVZ_ITEM_DEFAULT_ERRMSG;

      report_error ("%s: You have to define a string array called `%s'",
		    server, key);
      return SVZ_ITEM_FAILED;
    }
  else
    {
      int err = 0, i;
      svz_array_t *array;
      char *str;

      /* Check if this is really a list. */
      if (!gh_list_p (hvalue))
	{
	  report_error ("%s: String array `%s' is not a valid list",
			server, key);
	  return SVZ_ITEM_FAILED;
	}

      /* Iterate over the list and build up the array of strings. */
      array = svz_array_create (0);
      for (i = 0; gh_pair_p (hvalue); hvalue = gh_cdr (hvalue), i++)
	{
	  if (NULL == (str = guile2str (gh_car (hvalue))))
	    {
	      report_error ("%s: Invalid string element #%d in "
			    "string array `%s'", server, i, key);
	      err = -1;
	      continue;
	    }
	  svz_array_add (array, svz_strdup (str));
	  free (str);
	}

      /* Free the string array so far. */
      if (err)
	{
	  svz_array_foreach (array, str, i)
	    svz_free (str);
	  svz_array_destroy (array);
	  return SVZ_ITEM_FAILED;
	}

      /* Okay, that's fine so far. */
      *target = array;
    }
  return SVZ_ITEM_OK;
}

/* Hash callback for configuring a server. */
static int
optionhash_cb_hash (char *server, void *arg, char *key,
		    svz_hash_t **target, int hasdef,
		    svz_hash_t *def)
{
  svz_hash_t *options = arg;
  SCM hvalue = optionhash_get (options, key);

  if (SCM_UNSPECIFIED == hvalue)
    {
      if (hasdef)
	return SVZ_ITEM_DEFAULT_ERRMSG;

      report_error ("%s: You have to define a hash (alist) called `%s'",
		    server, key);
      return SVZ_ITEM_FAILED;
    }
  else
    {
      int err = 0, i;
      svz_hash_t *hash;

      /* Unpack again... Whysoever. */
      if (!gh_list_p (hvalue))
	{
	  err = -1;
	  report_error ("%s: Not a valid list for hash (alist) `%s'",
			server, key);
	  return SVZ_ITEM_FAILED;
	}

      /* Iterate the alist. */
      hash = svz_hash_create (7);
      for (i = 0; gh_pair_p (hvalue); hvalue = gh_cdr (hvalue), i++)
	{
	  SCM k, v, pair = gh_car (hvalue);
	  char *str, *keystr, *valstr;

	  if (!gh_pair_p (pair))
	    {
	      err = -1;
	      report_error ("%s: Element #%d of hash (alist) `%s' "
			    "is not a pair", server, i, key);
	      continue;
	    }
	  k = gh_car (pair);
	  v = gh_cdr (pair);
	     
	  /* Obtain key character string. */
	  if (NULL == (str = guile2str (k)))
	    {
	      err = -1;
	      report_error ("%s: Element #%d of hash (alist) `%s' "
			    "has no valid key (string required)",
			    server, i, key);
	      keystr = NULL;
	    }
	  else
	    {
	      keystr = svz_strdup (str);
	      free (str);
	    }

	  /* Obtain value character string. */
	  if (NULL == (str = guile2str (v)))
	    {
	      err = -1;
	      report_error ("%s: Element #%d of hash (alist) `%s' "
			    "has no valid value (string required)",
			    server, i, key);
	      valstr = NULL;
	    }
	  else
	    {
	      valstr = svz_strdup (str);
	      free (str);
	    }

	  /* Add to hash if key and value look good. */
	  if (keystr != NULL && valstr != NULL)
	    {
	      svz_hash_put (hash, keystr, valstr);
	      svz_free (keystr);
	    }
	}

      /* Free the values, keys are freed be hash destructor. */
      if (err)
	{
	  char **values;
	  svz_hash_foreach_value (hash, values, i)
	    svz_free (values[i]);
	  svz_hash_destroy (hash);
	  return SVZ_ITEM_FAILED;
	}

      /* We call that a hash, finally. */
      *target = hash;
    }
  return SVZ_ITEM_OK;
}

/* Port configuration callback for configuring a server. */
static int
optionhash_cb_portcfg (char *server, void *arg, char *key,
		       svz_portcfg_t **target, int hasdef,
		       svz_portcfg_t *def)
{
  svz_hash_t *options = arg;
  SCM hvalue = optionhash_get (options, key);
  svz_portcfg_t *port;
  char *str;

  /* Is the requested port configuration defined ? */
  if (hvalue == SCM_UNSPECIFIED)
    {
      if (hasdef)
	return SVZ_ITEM_DEFAULT_ERRMSG;

      report_error ("%s: Port configuration `%s' required", server, key);
      return SVZ_ITEM_FAILED;
    }

  /* Convert Scheme cell into string. */
  if ((str = guile2str (hvalue)) == NULL)
    {
      report_error ("%s: Invalid string or symbol value for `%s'",
		    server, key);
      return SVZ_ITEM_FAILED;
    }

  /* Check existence of this named port configuration . */
  if ((port = svz_portcfg_get (str)) == NULL)
    {
      report_error ("%s: No such port configuration: `%s'", server, str);
      free (str);
      return SVZ_ITEM_FAILED;
    }

  /* Duplicate this port configuration. */
  free (str);
  *target = svz_portcfg_dup (port);
  return SVZ_ITEM_OK;
}

/* After callback for configuring a server. */
static int
optionhash_cb_after (char *server, void *arg)
{
  svz_hash_t *options = arg;
  if (0 == optionhash_validate (options, 0, "server", server))
    return SVZ_ITEM_OK;
  return SVZ_ITEM_FAILED;
}

/*
 * Extract a full pipe from an option hash. Returns zero if it worked.
 */
static int
optionhash_extract_pipe (svz_hash_t *hash,
			 char *key,        /* the key to find      */
			 svz_pipe_t *pipe, /* where to put it      */
			 char *txt)        /* appended to error    */
{
  int err = 0;

  err |= optionhash_validate (hash, 1, "pipe", key);
  err |= optionhash_extract_string (hash, PORTCFG_NAME, 0, NULL,
				    &(pipe->name), txt);
  err |= optionhash_extract_string (hash, PORTCFG_USER, 1, NULL,
				    &(pipe->user), txt);
  err |= optionhash_extract_string (hash, PORTCFG_GROUP, 1, NULL,
				    &(pipe->group), txt);
  err |= optionhash_extract_int (hash, PORTCFG_UID, 1, 0,
				 (int *) &(pipe->uid), txt);
  err |= optionhash_extract_int (hash, PORTCFG_GID, 1, 0,
				 (int *) &(pipe->gid), txt);
  err |= optionhash_extract_int (hash, PORTCFG_PERMS, 1, 0,
				 (int *) &(pipe->perm), txt);
  err |= optionhash_validate (hash, 0, "pipe", key);
  return err;
}

/*
 * GUile server definition. Use two arguments:
 * First is a (unique) server name of the form "type-something" where
 * "type" is the shortname of a servertype. Second is the optionhash that
 * is special for the server. Uses library to configure the individual 
 * options. Emits error messages (to stderr). Returns #t when server got 
 * defined, #f in case of any error.
 */
#define FUNC_NAME "define-server!"
SCM
guile_define_server (SCM name, SCM args)
{
  int err = 0;
  char *servername;
  char *servertype = NULL, *p = NULL;
  svz_hash_t *options = NULL;
  svz_servertype_t *stype;
  svz_server_t *server = NULL;
  char *txt = NULL;

  /* Configure callbacks for the `svz_server_configure()' thing. */
  svz_server_config_t configure = {
    optionhash_cb_before,   /* before */
    optionhash_cb_integer,  /* integers */
    optionhash_cb_boolean,  /* boolean */
    optionhash_cb_intarray, /* integer arrays */
    optionhash_cb_string,   /* strings */
    optionhash_cb_strarray, /* string arrays */
    optionhash_cb_hash,     /* hashes */
    optionhash_cb_portcfg,  /* port configurations */
    optionhash_cb_after     /* after */
  };

  /* Check if the given server name is valid. */
  if (NULL == (servername = guile2str (name)))
    {
      report_error ("Invalid server name");
      FAIL ();
    }

  txt = svz_malloc (256);
  svz_snprintf (txt, 256, "defining server `%s'", servername);
    
  /* Extract options. */
  if (NULL == (options = guile2optionhash (args, txt, 1)))
    FAIL (); /* Message already emitted. */

  /* Seperate server description. */
  p = servertype = svz_strdup (servername);
  while (*p && *p != '-')
    p++;

  /* Extract server type and sanity check. */
  if (*p == '-' && *(p+1) != '\0')
    *p = '\0';
  else
    {
      report_error ("Not a valid server name: `%s'");
      FAIL ();
    }

  /* Find the definition by lookup with dynamic loading. */
  if (NULL == (stype = svz_servertype_get (servertype, 1)))
    {
      report_error ("No such server type: `%s'", servertype);
      FAIL ();
    }

  /* Instantiate and configure this server. */
  server = svz_server_instantiate (stype, servername);
  server->cfg = svz_server_configure (stype,
				      servername,
				      (void *) options,
				      &configure);
  if (NULL == server->cfg)
    {
      svz_server_free (server);
      FAIL (); /* Messages emitted by callbacks. */
    }

  /* Add server if configuration is ok, no error yet. */
  if (!err)
    {
      if ((server = svz_server_add (server)) != NULL)
	{
	  report_error ("Duplicate server definition: `%s'", servername);
	  svz_server_free (server);
	  FAIL ();
	}
    }
  else
    {
      svz_server_free (server);
      FAIL ();
    }

 out:
  svz_free (txt);
  svz_free (servertype);
  free (servername);
  optionhash_destroy (options);
  guile_global_error |= err;
  return err ? SCM_BOOL_T : SCM_BOOL_F;
}
#undef FUNC_NAME

/*
 * Port configuration definition. Use two arguments:
 * First is a (unique) name for the port configuration. Second is an 
 * optionhash for various settings. Returns #t when definition worked, 
 * #f when it did not. Emits error messages (to stderr).
 */
#define FUNC_NAME "define-port!"
static SCM
guile_define_port (SCM name, SCM args)
{
  int err = 0;
  svz_portcfg_t *prev = NULL;
  svz_portcfg_t *cfg = svz_portcfg_create ();
  svz_hash_t *options = NULL;
  char *portname, *proto = NULL, *txt;

  /* Check validity of first argument. */
  if ((portname  = guile2str (name)) == NULL)
    {
      report_error ("First argument to `" FUNC_NAME "' "
		    "needs to be string or symbol");
      FAIL ();
    }

  txt = svz_malloc (256);
  svz_snprintf (txt, 256, "defining port `%s'", portname);

  if (NULL == (options = guile2optionhash (args, txt, 1)))
    FAIL (); /* Message already emitted. */

  /* Every key defined only once ? */
  if (0 != optionhash_validate (options, 1, "port", portname))
    err = -1;

  /* Find out what protocol this portcfg will be about. */
  if (NULL == (proto = guile2str (optionhash_get (options, PORTCFG_PROTO))))
    {
      report_error ("Port `%s' requires a \"" PORTCFG_PROTO "\" field", 
		    portname);
      FAIL ();
    }

  /* Is that TCP ? */
  if (!strcmp (proto, PORTCFG_TCP))
    {
      int port;
      cfg->proto = PROTO_TCP;
      err |= optionhash_extract_int (options, PORTCFG_PORT, 0, 0, &port, txt);
      cfg->tcp_port = (short) port;
      err |= optionhash_extract_int (options, PORTCFG_BACKLOG, 1, 0, 
				     &(cfg->tcp_backlog), txt);
      err |= optionhash_extract_string (options, PORTCFG_IP, 1, PORTCFG_NOIP,
					&(cfg->tcp_ipaddr), txt);
    }
  /* Maybe UDP ? */
  else if (!strcmp (proto, PORTCFG_UDP))
    {
      int port;
      cfg->proto = PROTO_UDP;
      err |= optionhash_extract_int (options, PORTCFG_PORT, 0, 0, &port, txt);
      cfg->udp_port = (short) port;
      err |= optionhash_extract_string (options, PORTCFG_IP, 1, PORTCFG_NOIP,
					&(cfg->udp_ipaddr), txt);
    }
  /* Maybe ICMP ? */
  else if (!strcmp (proto, PORTCFG_ICMP))
    {
      int type;
      cfg->proto = PROTO_ICMP;
      err |= optionhash_extract_string (options, PORTCFG_IP, 1, PORTCFG_NOIP,
					&(cfg->icmp_ipaddr), txt);
      err |= optionhash_extract_int (options, PORTCFG_TYPE, 1, ICMP_SERVEEZ, 
				     &type, txt);
      if (type & ~0xff)
	{
	  report_error ("ICMP type `%s' requires a byte (0..255) %s", 
			PORTCFG_TYPE, txt);
	  err = -1;
	}
      cfg->icmp_type = (unsigned char) (type & 0xff);
    }
  /* Maybe RAW ? */
  else if (!strcmp (proto, PORTCFG_RAW))
    {
      cfg->proto = PROTO_RAW;
      err |= optionhash_extract_string (options, PORTCFG_IP, 1, PORTCFG_NOIP,
					&(cfg->raw_ipaddr), txt);
    }
  /* Finally a PIPE ? */
  else if (!strcmp (proto, PORTCFG_PIPE))
    {
      svz_hash_t *poptions;
      SCM p;
      char *str;
      cfg->proto = PROTO_PIPE;

      /* Handle receiving pipe. */
      svz_snprintf (txt, 256, "defining pipe `%s'", PORTCFG_RECV);

      /* Check if it is a plain string. */ 
      p = optionhash_get (options, PORTCFG_RECV);
      if ((str = guile2str (p)) != NULL)
	{
	  cfg->pipe_recv.name = svz_strdup (str);
	  free (str);
	}
      /* Create local optionhash for receiving pipe direction. */
      else if (p == SCM_UNSPECIFIED)
	{
	  report_error ("%s: You have to define a pipe called `%s'",
			portname, PORTCFG_RECV);
	  err = -1;
	}
      else if ((poptions = guile2optionhash (p, txt, 0)) == NULL)
	{
	  err = -1; /* Message already emitted. */
	}
      else
	{
	  err |= optionhash_extract_pipe (poptions, PORTCFG_RECV, 
					  &(cfg->pipe_recv), txt);
	  optionhash_destroy (poptions);
	}

      /* Try getting send pipe. */
      svz_snprintf (txt, 256, "defining pipe `%s'", PORTCFG_SEND);

      /* Check plain string. */
      p = optionhash_get (options, PORTCFG_SEND);
      if ((str = guile2str (p)) != NULL)
	{
	  cfg->pipe_send.name = svz_strdup (str);
	  free (str);
	}
      else if (p == SCM_UNSPECIFIED)
	{
	  report_error ("%s: You have to define a pipe called `%s'",
			portname, PORTCFG_SEND);
	  err = -1;
	}
      else if ((poptions = guile2optionhash (p, txt, 0)) == NULL)
	{
	  err = -1; /* Message already emitted. */
	}
      else
	{
	  err |= optionhash_extract_pipe (poptions, PORTCFG_SEND, 
					  &(cfg->pipe_send), txt);
	  optionhash_destroy (poptions);
	}
    }
  else
    {
      report_error ("Invalid \"" PORTCFG_PROTO "\" field `%s' in port `%s'",
		    proto, portname);
      FAIL ();
    }
  free (proto);

  /* Access the send and receive buffer sizes. */
  err |= optionhash_extract_int (options, PORTCFG_SEND_BUFSIZE, 1, 0,
				 &(cfg->send_buffer_size), txt);
  err |= optionhash_extract_int (options, PORTCFG_RECV_BUFSIZE, 1, 0,
				 &(cfg->recv_buffer_size), txt);
  
  /* Aquire the connect frequency. */
  if (cfg->proto & PROTO_TCP)
    err |= optionhash_extract_int (options, PORTCFG_FREQ, 1, 0,
				   &(cfg->connect_freq), txt);

  /* Obtain the access lists "allow" and "deny". */
  if (!(cfg->proto & PROTO_PIPE))
    {
      cfg->deny = guile2strarray (optionhash_get (options, PORTCFG_DENY),
				  FUNC_NAME);
      cfg->allow = guile2strarray (optionhash_get (options, PORTCFG_ALLOW),
				   FUNC_NAME);
    }

  svz_free (txt);

  /* Check for unused keys in input. */
  if (0 != optionhash_validate (options, 0, "port", portname))
    FAIL (); /* Message already emitted. */

  /* Now remember the name and add that port configuration. */
  cfg->name = svz_strdup (portname);
  err |= svz_portcfg_mkaddr (cfg);

  if (err)
    FAIL();

  if ((prev = svz_portcfg_add (portname, cfg)) != cfg)
    {
      /* We've overwritten something. Report and dispose. */
      report_error ("Duplicate definition of port `%s'", portname);
      err = -1;
      svz_portcfg_destroy (prev);
    }

 out:
  if (err)
    svz_portcfg_destroy (cfg);
  free (portname);
  optionhash_destroy (options);
  guile_global_error |= err;
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
      report_error ("%s: No such port: %s", FUNC_NAME, portname);
      error++;
    }

  /* Get one of the servers in the list. */
  if ((s = svz_server_get (servername)) == NULL)
    {
      report_error ("%s: No such server: %s", FUNC_NAME, servername);
      error++;
    }
  
  /* Bind a given server instance to a port configuration. */
  if (s != NULL && p != NULL)
    {
      if (svz_server_bind (s, p) < 0)
	error++;
    }

  guile_global_error |= error;
  return error ? SCM_BOOL_F : SCM_BOOL_T;
}
#undef FUNC_NAME

/*
 * Converts the given array of strings @var{array} into a guile list.
 */
static SCM
strarray2guile (svz_array_t *array)
{
  SCM list;
  unsigned long i;
  
  /* Check validity of the give string array. */
  if (array == NULL)
    return SCM_UNDEFINED;

  /* Go through all the strings and add these to a guile list. */
  for (list = SCM_EOL, i = 0; i < svz_array_size (array); i++)
    list = gh_cons (gh_str02scm ((char *) svz_array_get (array, i)), list);
  return gh_reverse (list);
}

/*
 * Make the list of local interfaces accessable for guile. Returns the
 * local interfaces as a list of ip addresses in dotted decimal form. If
 * another list is given in @var{args} it should contain the new list of
 * local interfaces.
 */
#define FUNC_NAME "serveez-interfaces"
SCM
guile_access_interfaces (SCM args)
{
  svz_interface_t *ifc;
  int n;
  SCM list;
  char *str, description[64];
  struct sockaddr_in addr;
  svz_array_t *array;

  /* First create a array of strings containing the ip addresses of each
     local network interface and put them into a guile list. */
  array = svz_array_create (0);
  svz_interface_foreach (ifc, n)
    svz_array_add (array, svz_strdup (svz_inet_ntoa (ifc->ipaddr)));
  list = strarray2guile (array);
  if (array)
    {
      svz_array_foreach (array, str, n)
	svz_free (str);
      svz_array_destroy (array);
    }

  /* Is there an argument given to the guile function ? */
  if (args != SCM_UNDEFINED)
    {
      svz_interface_free ();
      if ((array = guile2strarray (args, FUNC_NAME)) != NULL)
	{
	  svz_array_foreach (array, str, n)
	    {
	      if (svz_inet_aton (str, &addr) == -1)
		{
		  report_error ("%s: IP address in dotted decimals expected",
				FUNC_NAME);
		  guile_global_error = -1;
		  continue;
		}
	      sprintf (description, "guile interface %d", n);
	      svz_interface_add (n, description, addr.sin_addr.s_addr);
	    }
	  svz_array_foreach (array, str, n)
	    svz_free (str);
	  svz_array_destroy (array);
	}
    }

  return list;
}
#undef FUNC_NAME

/*
 * Make the search path for the serveez core library accessable for guile.
 * Returns a list a each path as previously defined. Can override the current
 * definition of this load path. The load path is used to tell serveez where
 * it can find additional server modules.
 */
#define FUNC_NAME "serveez-load-path"
SCM
guile_access_loadpath (SCM args)
{
  SCM list;
  svz_array_t *paths = svz_dynload_path_get ();
  int n;
  char *str;

  /* Create a guile list containing each search path. */
  list = strarray2guile (paths);
  if (paths)
    {
      svz_array_foreach (paths, str, n)
	svz_free (str);
      svz_array_destroy (paths);
    }
  
  /* Set the load path if argument is given. */
  if (args != SCM_UNDEFINED)
    {
      if ((paths = guile2strarray (args, FUNC_NAME)) != NULL)
	svz_dynload_path_set (paths);
    }
  return list;
}
#undef FUNC_NAME

/*
 * Create an accessor function to read and write a C int variable.
 */
#define MAKE_INT_ACCESSOR(cfunc, cvar)                       \
static SCM cfunc (SCM args) {                                \
  SCM value = gh_int2scm (cvar); int n;                      \
  if (args != SCM_UNDEFINED){                                \
    if (guile2int (args, &n)) {                              \
      report_error ("%s: Invalid integer value", FUNC_NAME); \
      guile_global_error = -1;                               \
    } else { cvar = n; } }                                   \
  return value;                                              \
}

/*
 * Create an accessor function to read and write a C string variable.
 */
#define MAKE_STRING_ACCESSOR(cfunc, cvar)                    \
static SCM cfunc (SCM args) {                                \
  SCM value = gh_str02scm (cvar); char *str;                 \
  if (args != SCM_UNDEFINED){                                \
    if (NULL == (str = guile2str (args))) {                  \
      report_error ("%s: Invalid string value", FUNC_NAME);  \
      guile_global_error = -1;                               \
    } else {                                                 \
      svz_free (cvar);                                       \
      cvar = svz_strdup (str);                               \
      free (str);                                            \
    } }                                                      \
  return value;                                              \
}

/* Accessor for the 'verbosity' global setting. */
#define FUNC_NAME "serveez-verbosity"
MAKE_INT_ACCESSOR (guile_access_verbosity, svz_config.verbosity)
#undef FUNC_NAME

/* Accessor for the 'max sockets' global setting. */
#define FUNC_NAME "serveez-maxsockets"
MAKE_INT_ACCESSOR (guile_access_maxsockets, svz_config.max_sockets)
#undef FUNC_NAME

/* Accessor for the 'password' global setting. */
#define FUNC_NAME "serveez-passwd"
MAKE_STRING_ACCESSOR (guile_access_passwd, svz_config.password)
#undef FUNC_NAME

/*
 * Initialize Guile. Make certain variables and functions defined above
 * available to Guile.
 */
static void
guile_init (void)
{
  /* define some variables */
  gh_define ("serveez-version", gh_str02scm (svz_version));
  gh_define ("guile-version", scm_version ());
  gh_define ("have-debug", gh_bool2scm (svz_have_debug));
  gh_define ("have-Win32", gh_bool2scm (svz_have_Win32));
  gh_define ("have-floodprotect", gh_bool2scm (svz_have_floodprotect));

  /* export accessors for global variables (read/write capable) */
  gh_new_procedure ("serveez-verbosity", guile_access_verbosity, 0, 1, 0);
  gh_new_procedure ("serveez-maxsockets", guile_access_maxsockets, 0, 1, 0);
  gh_new_procedure ("serveez-passwd", guile_access_passwd, 0, 1, 0);
  gh_new_procedure ("serveez-interfaces", guile_access_interfaces, 0, 1, 0);
  gh_new_procedure ("serveez-load-path", guile_access_loadpath, 0, 1, 0);

  /* export some new procedures */
  /* FIXME: change it to "x, 1, 0)" instead of "x, 0, 1)". requires change
   *        of prototypes of the implementation..
   */
  gh_new_procedure ("define-port!", guile_define_port, 1, 0, 1);
  gh_new_procedure ("define-server!", guile_define_server, 1, 0, 1);
  gh_new_procedure ("bind-server!", guile_bind_server, 2, 0, 1);
}

/*
 * Exception handler for guile. It is called if the evaluation of the given
 * file failed.
 */
static SCM 
guile_exception (void *data, SCM tag, SCM args)
{
  /* FIXME: current-load-port is not defined in this state. Why ? */
  
  /* tag contains internal exception name
   * scm_display (tag, scm_current_error_port ());
   */
  scm_display (gh_str02scm ("guile-error: ") , scm_current_error_port ());

  /* on quit/exit */
  if (gh_null_p (args))
    {
      scm_display (tag, scm_current_error_port ());
      scm_display (gh_str02scm ("\n"), scm_current_error_port ());
      return SCM_BOOL_F;
    }

  if (SCM_BOOL_F != gh_car (args))
    {
      scm_display (gh_car (args), scm_current_error_port ());
      scm_display (gh_str02scm (": "), scm_current_error_port ());
    }
  scm_simple_format (scm_current_error_port (),
		     gh_car (gh_cdr (args)),
		     gh_car (gh_cdr (gh_cdr (args))));
  scm_display (gh_str02scm ("\n"), scm_current_error_port ());
  return SCM_BOOL_F;
}

/*
 * Get server settings from the file @var{cfgfile} and instantiate servers 
 * as needed. Return non-zero on errors.
 */
int
guile_load_config (char *cfgfile)
{
  guile_global_error = 0;
  guile_init ();

  if (gh_eval_file_with_catch (cfgfile, guile_exception) == SCM_BOOL_F)
    guile_global_error = -1; 

  return guile_global_error ? -1 : 0;
}
