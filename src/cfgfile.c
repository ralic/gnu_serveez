/*
 * cfgfile.c - configuration file implementation with help of sizzle
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
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
 * $Id: cfgfile.c,v 1.15 2001/01/28 03:26:54 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <string.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "libserveez/boot.h"
#include "libserveez/util.h"
#include "libserveez/alloc.h"
#include "libserveez/hash.h"
#include "libserveez/socket.h"
#include "libserveez/server.h"
#include "sizzle.h"
#include "cfgfile.h"

/* 
 * Include headers of servers.
 */
#include "foo-server/foo-proto.h"
#if ENABLE_AWCS_PROTO
# include "awcs-server/awcs-proto.h"
#endif
#if ENABLE_HTTP_PROTO
# include "http-server/http-proto.h"
#endif
#if ENABLE_IRC_PROTO
# include "irc-server/irc-proto.h"
#endif
#if ENABLE_CONTROL_PROTO
# include "ctrl-server/control-proto.h"
#endif
#if ENABLE_SNTP_PROTO
# include "sntp-server/sntp-proto.h"
#endif
#if ENABLE_GNUTELLA
# include "nut-server/gnutella.h"
#endif
#if ENABLE_TUNNEL
# include "tunnel-server/tunnel.h"
#endif
#if ENABLE_FAKEIDENT
# include "fakeident-server/ident-proto.h"
#endif

#include <libsizzle/libsizzle.h>

/* for backward compatibility with older versions of sizzle core */
#ifndef hashtable_p
# define hashtable_p(c) vector_p(c)
#endif
#ifndef zzz_interaction_environment
# define zzz_interaction_environment zzz_toplevel_env
#endif

#define LISTEND       0
#define TYPEINT       1
#define TYPEBOOL      2
#define TYPESTRING    3
#define TYPESTRARRAY  4

/*
 * Structure of things we need to know about any variable.
 */
struct config_t 
{
  int type;              /* what kind of configitem is this ? */
  char *name;            /* name of the variable in initfile */
  void *location;        /* nhere is it (int *, char ** or char ***) */
  int defaultable;       /* is it ok to use the default? */
  char *string_buffer;   /* string buffer we give to sizzle */

  int default_int;       /* default int / boolean value */
  char *default_string;  /* default string value */

  zzz_scm_t scm;         /* private var for listusage (STRARRAY) */
};

/* Macros for registering variables */
#define REG_INT(name, location, default, defaultable) \
{ TYPEINT, name, location, defaultable, NULL, default, NULL, NULL }

#define REG_BOOL(name, location, default, defaultable) \
{ TYPEBOOL, name, location, defaultable, NULL, default, NULL, NULL }

#define REG_STRING(name, location, default, defaultable) \
{ TYPESTRING, name, location, defaultable, NULL, 0, default, NULL }

/* String arrays cannot have default values */
#define REG_STRARRAY(name, location, defaultable) \
{ TYPESTRARRAY, name, location, defaultable, NULL, 0, NULL, NULL }

#define REG_END \
{ LISTEND, NULL, NULL,  0, NULL,  0, NULL, NULL }

/* Macro for 'compiled-in' flags */
#define REG_HAVEFLAG(name, location) \
zzz_bind_bool_variable (name, location, 1)

/* Initialize static server definitions. */
void
init_server_definitions (void)
{
  server_add_definition (&foo_server_definition);
#if ENABLE_AWCS_PROTO
  server_add_definition (&awcs_server_definition);
#endif
#if ENABLE_HTTP_PROTO
  server_add_definition (&http_server_definition);
#endif
#if ENABLE_IRC_PROTO
  server_add_definition (&irc_server_definition);
#endif
#if ENABLE_CONTROL_PROTO
  server_add_definition (&ctrl_server_definition);
#endif
#if ENABLE_SNTP_PROTO
  server_add_definition (&sntp_server_definition);
#endif
#if ENABLE_GNUTELLA
  server_add_definition (&nut_server_definition);
#endif
#if ENABLE_TUNNEL
  server_add_definition (&tnl_server_definition);
#endif
#if ENABLE_FAKEIDENT
  server_add_definition (&fakeident_server_definition);
#endif
}

/*
 * Loads the configuration from the .cfg file giving all setup variables
 * their default/configured values. Returns -1 on error, caller should
 * terminate program then...
 */
int
load_config (char *cfgfile, int argc, char **argv)
{
  int retval = 0;
  int i;
  
  /* register all configuration items here */
  struct config_t configs[] =
  {
    /* global settings */
    REG_INT ("serveez-sockets", &svz_config.max_sockets, 200, 1),
    REG_INT ("serveez-verbosity", &svz_verbosity, 3, 1),
    REG_STRING ("serveez-pass", &svz_config.server_password, "!", 0),
    REG_END
  };

  /* initialize sizzle */
  zzz_set_top_of_stack ((zzz_scm_t *) &cfgfile);
  zzz_initialize ();
  zzz_set_arguments (argc - 1, argv[0], argv + 1);

  /* set some information for sizzle (read-only) */
  zzz_bind_string_variable ("serveez-version", svz_version, 0, 1);

  /* register read-only boolean variables for the features in this system */
  REG_HAVEFLAG ("have-debug", &have_debug);
  REG_HAVEFLAG ("have-win32", &have_win32);

  /* go through list of configuration items */
  for (i = 0; configs[i].type != LISTEND; i++)
    {
      switch (configs[i].type)
	{
	case TYPEINT:
	  *(int *) configs[i].location = configs[i].default_int;
	  zzz_bind_int_variable (configs[i].name, configs[i].location, 0);
	  break;

	case TYPEBOOL:
	  *(int *) configs[i].location = configs[i].default_int;
	  zzz_bind_bool_variable (configs[i].name, configs[i].location, 0);
	  break;

	case TYPESTRING:
	  configs[i].string_buffer = svz_malloc (STRINGVARSIZE);
	  strncpy (configs[i].string_buffer,
		   configs[i].default_string, STRINGVARSIZE);
	  zzz_bind_string_variable (configs[i].name, configs[i].string_buffer,
				    STRINGVARSIZE, 0);
	  break;
	  
	case TYPESTRARRAY:
	  zzz_bind_scm_variable (configs[i].name, &configs[i].scm, 0);
	  break;

	default:
	  fprintf (stderr, "inconsistent data in " __FILE__ ", aborting\n");
	  return -1;
	}
    }

  /* evaluate the configfile, doing nothing when file was not found */
  if (zzz_evaluate_file (zzz_interaction_environment, cfgfile) == -2) 
    return -1;

  /* go through list of configuration items once again */
  for (i = 0; configs[i].type != LISTEND; i++)
    {
      switch (configs[i].type)
	{
	case TYPEINT:
	  if (!configs[i].defaultable &&
	      *(int *) configs[i].location == configs[i].default_int)
	    {
	      fprintf (stderr, "%s: integer `%s' has no default value\n",
		       cfgfile, configs[i].name);
	      retval = -1;
	    }
	  break;

	case TYPEBOOL:
	  if (!configs[i].defaultable &&
	      *(int *) configs[i].location == configs[i].default_int)
	    {
	      fprintf (stderr, "%s: boolean `%s' has no default value\n",
		       cfgfile, configs[i].name);
	      retval = -1;
	    }
	  break;

	case TYPESTRING:
	  if (!configs[i].defaultable &&
	      !strcmp (configs[i].string_buffer, configs[i].default_string))
	    {
	      fprintf (stderr, "%s: string `%s' has no default value\n",
		       cfgfile, configs[i].name);
	      retval = -1;
	    }
	  else
	    {
	      *(char **) configs[i].location =
		svz_pmalloc (strlen (configs[i].string_buffer) + 1);
	      strcpy (*(char **) configs[i].location, 
		      configs[i].string_buffer);
	      svz_free (configs[i].string_buffer);
	    }
	  break;
      
	case TYPESTRARRAY:
	  if (!configs[i].defaultable &&  configs[i].scm == NULL)
	    {
	      fprintf (stderr, "%s: string array `%s' has no default value\n",
		       cfgfile, configs[i].name);
	      return retval = -1;
	    }
	  else 
	    {
	      int m = 0;
	      zzz_scm_t s = configs[i].scm;
	      int length;
	      char **array;

	      /* determine list length */
	      length = zzz_list_length (s);

	      if (length <= 0)
		{
		  fprintf (stderr, "%s: invalid string array `%s'\n",
			   cfgfile, configs[i].name);
		  retval = -1;
		  break;
		}

	      /* allocate memory for this array */
	      *(char ***) configs[i].location = svz_malloc ((length + 1) *
							    sizeof (char *));
	      array = *(char ***) configs[i].location;

	      /* copy list elements */
	      while (cons_p (s)) 
		{
		  if (string_p (car (s)))
		    {
		      if (retval == 0) /* idle if already in error-mode */
			{
			  char *element = string_val (car (s));
			  array[m] = svz_malloc (strlen (element) + 1);
			  strcpy (array[m], element);
			}
		    } 
		  else 
		    {
		      fprintf (stderr, 
			       "%s: element %d of `%s' is not a string\n",
			       cfgfile, m, configs[i].name);
		      retval = -1;
		    }
		  s = cdr (s);
		  m++;
		}

	      /* terminate the array of (char *) */
	      array[m] = NULL;
	    }
	  break;
	  
	default:
	  fprintf (stderr, "inconsistent data in " __FILE__", panik\n");
	  return -1;
	}
    }

  /* 
   * Instantiate servers from symbol table.
   */
  if (zzz_server_load_cfg (cfgfile) < 0)
    retval = -1;

  zzz_finalize ();
  return retval;
}
