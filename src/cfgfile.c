/*
 * cfgfile.c - configuration file implementation with help of sizzle
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: cfgfile.c,v 1.10 2000/09/11 00:07:35 raimi Exp $
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

#include "util.h"
#include "alloc.h"
#include "cfgfile.h"
#include "serveez.h"
#include "server.h"
#include "libsizzle/libsizzle.h"

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
  void *location;        /* nhere is it (int*, char** or char***) */
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

#define REG_STRARRAY(name, location, defaultable) \
{ TYPESTRARRAY, name, location, defaultable, NULL, 0, NULL, NULL }
/* String arrays cannot have default values */


/* Macro for 'compiled-in' flags */
#define REG_HAVEFLAG(name, location) \
zzz_bind_bool_variable (name, location, 1)

/*
 * Loads the configuration from the .cfg file giving all setup variables
 * their default/configured values. Returns -1 on error, caller should
 * terminate program then...
 */
int
load_config (char *cfgfilename, int argc, char **argv)
{
  int retval = 0;
  int i;
  
  /*
   * Register all configuration items here.
   */
  struct config_t configs[] =
  {
    /* global settings */
    REG_INT ("serveez-sockets", &serveez_config.max_sockets, 200, 1),
    REG_INT ("serveez-verbosity", &verbosity, 3, 1),
    REG_STRING ("serveez-pass", &serveez_config.server_password, "!", 0),
    
    { LISTEND,      NULL, NULL,  0, NULL,  0, NULL, NULL }
  };


  zzz_set_top_of_stack ((zzz_scm_t *) &cfgfilename);
  zzz_initialize ();
  zzz_set_arguments (argc - 1, argv[0], argv + 1);

  /*
   * set some information for sizzle (read-only)
   */
  zzz_bind_string_variable ("serveez-version", serveez_config.version_string,
			    0, 1);

  /*
   * register read-only boolean variables for the features in this system
   */
  REG_HAVEFLAG ("have-debug", &have_debug);
  REG_HAVEFLAG ("have-win32", &have_win32);

  for (i = 0; configs[i].type != LISTEND; i++)
    {
      switch (configs[i].type)
	{
	case TYPEINT:
	  *(int *)configs[i].location = configs[i].default_int;
	  zzz_bind_int_variable (configs[i].name, configs[i].location, 0);
	  break;

	case TYPEBOOL:
	  *(int *)configs[i].location = configs[i].default_int;
	  zzz_bind_bool_variable (configs[i].name, configs[i].location, 0);
	  break;

	case TYPESTRING:
	  configs[i].string_buffer = xmalloc (STRINGVARSIZE);
	  strncpy (configs[i].string_buffer,
		   configs[i].default_string, STRINGVARSIZE);
	  zzz_bind_string_variable (configs[i].name, configs[i].string_buffer,
				    STRINGVARSIZE, 0);
	  break;

	case TYPESTRARRAY:
	  zzz_bind_scm_variable (configs[i].name, &configs[i].scm, 0);
	  break;

	default:
	  fprintf (stderr, "Inconsistent data in cfgfile.c, aborting.\n");
	  return -1;
	}
    }

  /* Evaluate the configfile, doing nothing when file was not found */
  if (zzz_evaluate_file (zzz_interaction_environment, cfgfilename) == -2) 
    return -1;

  for (i = 0; configs[i].type != LISTEND; i++)
    {
      switch (configs[i].type)
	{
	case TYPEINT:
	  if (!configs[i].defaultable &&
	      *(int*) configs[i].location == configs[i].default_int )
	    {
	      fprintf (stderr, "Integer variable %s was not set in %s "
		       "but has no default value !\n",
		       configs[i].name, cfgfilename);
	      retval = -1;
	    }
	  break;

	case TYPEBOOL:
	  if (!configs[i].defaultable &&
	      *(int*) configs[i].location == configs[i].default_int)
	    {
	      fprintf (stderr, "Boolean variable %s was not set in %s "
		       "but has no default value !\n",
		       configs[i].name, cfgfilename);
	      retval = -1;
	    }
	  break;

    case TYPESTRING:
      if (!configs[i].defaultable &&
	  strcmp (configs[i].string_buffer, configs[i].default_string) == 0)
	{
	  fprintf (stderr, "String variable %s was not set in %s "
		   "but has no default value !\n",
		   configs[i].name, cfgfilename);
	  retval = -1;
	}
      else
	{
	  *(char**) configs[i].location =
	    xpmalloc (strlen (configs[i].string_buffer) + 1);

	  strcpy (*(char**) configs[i].location, configs[i].string_buffer);
	  xfree (configs[i].string_buffer);
	}
      break;
      
	case TYPESTRARRAY:
	  if (!configs[i].defaultable &&  configs[i].scm == NULL)
	    {
	      fprintf (stderr, "String array %s was not set in %s "
		       "but has no default value !\n",
		       configs[i].name, cfgfilename);
	      return retval = -1;
	    }
	  else 
	    {
	      int m = 0;
	      zzz_scm_t s = configs[i].scm;
	      int length;
	      char **array;

	      /* Determine list length */
	      length = zzz_list_length (s);

	      if (length <= 0)
		{
		  fprintf (stderr, "Something wrong with string array "
			   "%s in %s.\n", configs[i].name, cfgfilename);
		  retval = -1;
		  break;
		}

	      /* Allocate mem for this array */
	      *(char***) configs[i].location = xmalloc ((length + 1) *
							sizeof (char *));
	      array = *(char***) configs[i].location;

	      /* copy list elements */
	      while (cons_p (s)) 
		{
		  if (string_p (car (s)))
		    {
		      if (retval == 0) /* idle if already in error-mode */
			{
			  char *element = string_val (car (s));
			  array[m] = xmalloc (strlen (element) + 1);
			  strcpy (array[m], element);
			}
		    } 
		  else 
		    {
		      fprintf (stderr, "Element %d of %s is not a string in "
			       "%s.\n", m, configs[i].name, cfgfilename);
		      retval = -1;
		    }
		  s = cdr (s);
		  m++;
		}

	      /* Terminate the array of char* */
	      array[m] = NULL;
	    }
	  break;
	  
	default:
	  fprintf (stderr, "Inconsistent data in cfgfile.c, panik !\n");
	  return -1;
	}
    }


  /* Instantiate servers from symbol table
   */
  if (server_load_cfg (cfgfilename) < 0)
    retval = -1;

  zzz_finalize ();
  return retval;
}
