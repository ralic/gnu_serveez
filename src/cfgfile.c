/*
 * cfgfile.c - Configurationfile implementation with help of sizzle
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
 * $Id: cfgfile.c,v 1.2 2000/06/11 21:39:17 raimi Exp $
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

#if ENABLE_IRC_PROTO
# include "irc-server/irc-proto.h"
#endif

#if ENABLE_HTTP_PROTO
# include "http-server/http-proto.h"
# include "http-server/http-cache.h"
#endif



#define LISTEND       0
#define TYPEINT       1
#define TYPEBOOL      2
#define TYPESTRING    3
#define TYPESTRARRAY  4

/*
 * Structure of things we need to know about any variable
 */
struct config_t {
  int type;              /* What kind of configitem is this ? */
  char *name;            /* Name of the variable in initfile */
  void *location;        /* Where is it (int*, char** or char***) */
  int defaultable;       /* Is it ok to use the default? */
  char *string_buffer;   /* String buffer we give to sizzle */

  int default_int;       /* default int/bool value */
  char *default_string;  /* default string value */

  zzz_scm_t scm;         /* private var for listusage (STRARRAY) */
} ;

/* Macros for registering variables */
#define REG_INT(name, location, default, defaultable) \
{ TYPEINT, name, location, defaultable, NULL, default, NULL, NULL }

#define REG_BOOL(name, location, default, defaultable) \
{ TYPEBOOL, name, location, defaultable, NULL, default, NULL, NULL }

#define REG_STRING(name, location, default, defaultable) \
{ TYPESTRING, name, location, defaultable, NULL, 0, default, NULL }

#define REG_STRARRAY(name, location, defaultable) \
{ TYPESTRARRAY, name, location, defaultable, NULL, 0, NULL, NULL }
/* Str Arrays cannot have default values */


/* Macro for 'compiled-in' flags */
#define REG_HAVEFLAG(name, location) \
zzz_bind_bool_variable(name, location, 1)

/*
 * Loads the configuration from the .cfg file giving all setup variables
 * their default/configured values. Returns -1 on error, caller should
 * terminate program then...
 */
int
load_config(char *cfgfilename, int argc, char **argv)
{
  int retval = 0;
  int i;
  
  /*
   * Register all configuration items here
   */
  struct config_t configs[] =
  {
    /* Global settings */
    REG_INT("serveez-port", &serveez_config.port, 42420, 1),
    REG_INT("serveez-sockets", &serveez_config.max_sockets, 200, 1),
    REG_INT("serveez-verbosity", &verbosity, 3, 1),
    REG_STRING("serveez-pass", &serveez_config.server_password, "!", 0),
    
    /* HTTP Server related settings */
#if ENABLE_HTTP_PROTO
    REG_STRING("http-indexfile", &http_config.indexfile, "index.html", 1),
    REG_STRING("http-docs", &http_config.docs, "../show", 1),
    REG_STRING("http-cgidir", &http_config.cgidir, "./cgibin", 1),
    REG_STRING("http-cgiurl", &http_config.cgiurl, "/cgi-bin", 1),
    REG_INT("http-cachesize", &http_config.cachesize, MAX_CACHE_SIZE, 1),
    REG_INT("http-timeout", &http_config.timeout, HTTP_TIMEOUT, 1),
    REG_INT("http-keepalive", &http_config.keepalive,
		 HTTP_MAXKEEPALIVE, 1),
#endif

    /* IRC server related settings */
#if ENABLE_IRC_PROTO
    REG_STRING("irc-MOTD-file", &irc_config.MOTD_file,
		    "../doc/irc-MOTD.txt", 1),
#if ENABLE_TIMESTAMP
    REG_INT("irc-tsdelta", &irc_config.tsdelta, 0, 1),
#endif
    REG_STRING("irc-admininfo", &irc_config.admininfo, "!", 0),
    REG_STRING("irc-email", &irc_config.email, "!", 0),
    REG_STRING("irc-location1", &irc_config.location1, "!", 0),
    REG_STRING("irc-location2", &irc_config.location2, "!", 0),
    REG_STRING("irc-M-line", &irc_config.MLine, "!", 0),
    REG_STRARRAY("irc-C-lines", &irc_config.CLine, 1),
#endif

    { LISTEND,      NULL, NULL,  0, NULL,  0, NULL, NULL }
  };


  zzz_set_top_of_stack ((zzz_scm_t *) &cfgfilename);
  zzz_initialize ();
  zzz_set_arguments (argc - 1, argv[0], argv + 1);

  /*
   * set some information for sizzle (read-only)
   */
  zzz_bind_string_variable("serveez-version", serveez_config.version_string,
			   0, 1);

  /*
   * register read-only boolean variables for the features in this system
   */
  REG_HAVEFLAG("have-debug", &have_debug);
  REG_HAVEFLAG("have-awcs", &have_awcs);
  REG_HAVEFLAG("have-irc", &have_irc);
  REG_HAVEFLAG("have-http", &have_http);
  REG_HAVEFLAG("have-ident", &have_ident);
  REG_HAVEFLAG("have-nslookup", &have_nslookup);
  REG_HAVEFLAG("have-floodprotection", &have_floodprotect);
  REG_HAVEFLAG("have-win32", &have_win32);

  for (i = 0; configs[i].type != LISTEND; i++)
    {
      switch (configs[i].type)
	{
	case TYPEINT:
	  *(int *)configs[i].location = configs[i].default_int;
	  zzz_bind_int_variable(configs[i].name, configs[i].location, 0);
	  break;

	case TYPEBOOL:
	  *(int *)configs[i].location = configs[i].default_int;
	  zzz_bind_bool_variable(configs[i].name, configs[i].location, 0);
	  break;

	case TYPESTRING:
	  configs[i].string_buffer = xmalloc(STRINGVARSIZE);
	  strncpy(configs[i].string_buffer,
		  configs[i].default_string, STRINGVARSIZE);
	  zzz_bind_string_variable(configs[i].name, configs[i].string_buffer,
				   STRINGVARSIZE, 0);
	  break;

	case TYPESTRARRAY:
	  zzz_bind_scm_variable(configs[i].name, &configs[i].scm, 0);
	  break;

	default:
	  fprintf(stderr, "Inconsistent data in cfgfile.c, aborting\n");
	  return -1;
	}
    }

  /* Evaluate the configfile, doing nothing when file was not found */
  if (zzz_evaluate_file(cfgfilename) == -2) 
    return -1;

  for (i = 0; configs[i].type != LISTEND; i++)
    {
      switch (configs[i].type)
	{
	case TYPEINT:
	  if ( !configs[i].defaultable &&
	       *(int*)configs[i].location == configs[i].default_int )
	    {
	      fprintf(stderr, "Integer variable %s was not set in %s "
		      "but has no default value!\n",
		      configs[i].name, cfgfilename);
	      retval = -1;
	    }
	  break;

	case TYPEBOOL:
	  if ( !configs[i].defaultable &&
	       *(int*)(configs[i].location) == configs[i].default_int )
	    {
	      fprintf(stderr, "Boolean variable %s was not set in %s "
		      "but has no default value!\n",
		      configs[i].name, cfgfilename);
	      retval = -1;
	    }
	  break;

    case TYPESTRING:
      if ( !configs[i].defaultable &&
	   strcmp(configs[i].string_buffer, configs[i].default_string) == 0 )
	{
	  fprintf(stderr, "String variable %s was not set in %s "
		  "but has no default value!\n",
		  configs[i].name, cfgfilename);
	  retval = -1;
	}
      else
	{
	  /* We use malloc here because this string cannot be leaked later */
	  *(char**)configs[i].location =
	    malloc(strlen(configs[i].string_buffer) + 1);
	  if ( NULL == (char**)configs[i].location )
	    {
	      fprintf(stderr, "Out of mem :-(\n");
	      return -1;
	    }

	  strcpy(*(char**)configs[i].location, configs[i].string_buffer);
	  xfree(configs[i].string_buffer);
	}
      break;
      
	case TYPESTRARRAY:
	  if (!configs[i].defaultable &&  configs[i].scm == NULL ) {
	    fprintf(stderr, "String array %s was not set in %s "
		    "but has no default value!\n",
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
	      length = zzz_list_length(s);

	      if (length <= 0)
		{
		  fprintf(stderr, "Something wrong with String array "
			  "%s in %s\n", configs[i].name, cfgfilename);
		  retval = -1;
		  break;
		}

	      /* Allocate mem for this array */
	      *(char***)configs[i].location = xmalloc((length+1)*
						      sizeof(char*));
	      array = *(char***)configs[i].location;

	      /* copy list elements */
	      while ( cons_p(s) ) {
		if ( string_p(car(s)) ) {
		  if (retval == 0) /* idle if already in errormode */
		    {
		      char *element = string_val(car(s));
		      array[m] = xmalloc(strlen(element) + 1);
		      strcpy(array[m], element);
		    }
		} else {
		  fprintf(stderr, "Element %d of %s is not a String in "
			  "%s\n", m, configs[i].name, cfgfilename);
		  retval = -1;
		}

		s = cdr(s);
		m++;
	      }

	      /* Terminate the array of char* */
	      array[m] = NULL;
	    }
	  break;
	  
	default:
	  fprintf(stderr, "Inconsistent data in cfgfile.c, panik!\n");
	  return -1;
	}
    }


  /* Instantiate servers from symbol table
   */
  if ( server_load_cfg(cfgfilename) < 0 )
    retval = -1;

  zzz_finalize ();
  return retval;
}


