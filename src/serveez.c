/*
 * serveez.c - main module
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
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
 * $Id: serveez.c,v 1.2 2000/06/11 21:39:17 raimi Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "version.h"
#include "alloc.h"
#include "util.h"
#include "server-core.h"
#include "serveez.h"
#include "cfgfile.h"
#include "option.h"
#include "server-socket.h"
#include "coserver/coserver.h"

#if ENABLE_HTTP_PROTO
# include "http-server/http-proto.h"
# include "http-server/http-cache.h"
#endif

#if ENABLE_IRC_PROTO
# include "irc-server/irc-proto.h"
# include "irc-server/irc-server.h"
#endif

#include "server.h"

/*
 * The configurations structure of the Serveez. Defined
 * in the serveez.h.
 */
serveez_config_t serveez_config;

/*
 * Initialization of the configuration.
 */
static void
init_config (void)
{
  serveez_config.start_time = time (NULL);
  serveez_config.program_name = "serveez";
  serveez_config.version_string = __serveez_version;
  serveez_config.build_string = __serveez_timestamp;
}

static void 
version (void)
{
  fprintf (stderr, "%s %s\n", 
	   serveez_config.program_name, 
	   serveez_config.version_string);
}

static void 
usage (void)
{
  fprintf (stderr, "usage: serveez [OPTION]...\n"
	   "\n"
#if HAVE_GETOPT_LONG
 "  -h, --help               display this help and exit\n"
 "  -V, --version            display version information and exit\n"
 "  -f, --file               file to use as configuration file [serveez.cfg]\n"
 "  -p, --port=PORT          port number the server should listen on\n"
 "  -v, --verbose=LEVEL      set level of verbosity\n"
 "  -l, --log-file=FILENAME  use FILENAME for logging (default is stderr)\n"
 "  -P, --password=STRING    set the password for control connections\n"
 "  -m, --max-sockets=COUNT  set the max. number of socket descriptors\n"
#else /* not HAVE_GETOPT_LONG */
 "  -h           display this help and exit\n"
 "  -V           display version information and exit\n"
 "  -f           file to use as configuration file [serveez.cfg\n"
 "  -p PORT      port number the server should listen on\n"
 "  -v LEVEL     set level of verbosity\n"
 "  -l FILENAME  use FILENAME for logging (default is stderr)\n"
 "  -P STRING    set the password for control connections\n"
 "  -m COUNT     set the max. number of socket descriptors\n"
#endif /* not HAVE_GETOPT_LONG */
 "\n"
 "See README for reporting bugs.\n");
}

#if HAVE_GETOPT_LONG
static struct option server_options[] =
{
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'V'},
  {"verbose", required_argument, NULL, 'v'},
  {"file", required_argument, NULL, 'f'},
  {"port", required_argument, NULL, 'p'},
  {"log-file", required_argument, NULL, 'l'},
  {"password", required_argument, NULL, 'P'},
  {"max-sockets", required_argument, NULL, 'm'},
  {NULL, 0, NULL, 0}
};
#endif /* HAVE_GETOPT_LONG */

int
main (int argc, char * argv[])
{
  char * log_file_name = NULL;
  char * cfg_file_name = "serveez.cfg";

  int cli_port = -1;
  int cli_verbosity = -1;
  int cli_max_sockets = -1;
  char * cli_pass = NULL;

  FILE * log_file = NULL;
  socket_t server;

  int arg;
#if HAVE_GETOPT_LONG
  int index;
#endif

  /* initialize the configuration structure */
  init_config();

#if HAVE_GETOPT_LONG
  while ((arg = getopt_long (argc, argv, "p:hVv:f:P:m:", server_options,
			     &index)) != EOF)
#else
  while ((arg = getopt (argc, argv, "p:hVv:f:P:m:")) != EOF)
#endif
    {
      switch (arg)
	{
	case 'h':
	  usage ();
	  exit (0);
	  break;

	case 'V':
	  version ();
	  exit (0);
	  break;

	case 'f':
	  cfg_file_name = optarg;
	  break;

	case 'p':
	  cli_port = atoi (optarg);
	  break;

	case 'v':
	  if (optarg)
	    cli_verbosity = atoi (optarg);
	  else
	    cli_verbosity = LOG_DEBUG;
	  if (cli_verbosity < 0)
	    cli_verbosity = 0;
	  else if (cli_verbosity > LOG_DEBUG)
	    cli_verbosity = LOG_DEBUG;
	  break;

	case 'l':
	  log_file_name = optarg;
	  break;

	case 'P':
	  if (optarg)
	    {
	      /*
	      strncpy(serveez_config.server_password, optarg, 
		      sizeof(serveez_config.server_password));
	      serveez_config.server_password[
	        sizeof(serveez_config.server_password) - 1] = '\0';
	      */
	      cli_pass = xmalloc(strlen(optarg) + 1);
	      strcpy(cli_pass, optarg);
	    }
	  break;

	case 'm':
	  if (optarg)
	   cli_max_sockets = atoi (optarg);
	  else
	    {
	      usage ();
	      exit (1);
	    }
	  if (verbosity <= 0)
	    {
	      usage ();
	      exit (1);
	    }
	  break;

	default:
	  usage ();
	  exit (1);
	}
    }

  if (log_file_name && log_file_name[0])
    log_file = fopen(log_file_name, "w");
  
  if (!log_file)
    log_file = stderr;

  /*
   * Send all log messages to LOG_FILE.
   */
  set_log_file(log_file);


  /*
   * DEBUG: show what servers we are able to run
   */
#if 0
  server_show_definitions();
#endif

  /*
   * Load configuration
   */
  if ( load_config(cfg_file_name, argc, argv) == -1 )
    {
      /* 
       * Something went wrong while configuration file loading, 
       * message output by function itself...
       */
      return 1;
    }


  /*
   * Make command line arguments overriding the configuration file settings.
   */
  if (cli_port != -1)
    serveez_config.port = cli_port;

  if (cli_verbosity != -1)
    verbosity = cli_verbosity;

  if (cli_max_sockets != -1)
    serveez_config.max_sockets = cli_max_sockets;

  if (cli_pass)
    {
      free(serveez_config.server_password);
      serveez_config.server_password = cli_pass;
    }

  /*
   * Initialise servers globaly
   */
  if ( server_global_init() == -1 ) {
    return 2;
  }
  
  /*
   * Initialise servers
   */
  if ( server_init_all() == -1 )
    {
      /* Something went wrong while the server initialised themselfes.
       * abort solently
       */
      return 3;
    }

#if ENABLE_DEBUG
  log_printf(LOG_NOTICE, "serveez starting, debugging enabled\n");
#endif /* ENABLE_DEBUG */
  
  log_printf(LOG_NOTICE, "%s\n", get_version());
  
#if ENABLE_HTTP_PROTO
  http_read_types ("/etc/mime.types");
  log_printf(LOG_NOTICE, "http: %s is document root\n",
	     http_config.docs);
  log_printf(LOG_NOTICE, "http: %s is cgi root, accessed via %s\n",
	     http_config.cgidir, http_config.cgiurl);
#endif


#ifdef __MINGW32__
  if (!net_startup())
    return 4;
#endif /* __MINGW32__ */
  
  /* 
   * Startup the internal coservers here.
   */
  if ( coserver_init () == -1 )
    return 5;

#if ENABLE_REVERSE_LOOKUP
  create_internal_coserver (COSERVER_REVERSE_DNS);
#endif
#if ENABLE_IDENT
  create_internal_coserver (COSERVER_IDENT);
#endif
#if ENABLE_DNS_LOOKUP
  create_internal_coserver (COSERVER_DNS);
#endif

      
  /*
   * Actually open the ports
   */
  if ( server_start () == -1 )
    return 6;
  

     
#if ENABLE_IRC_PROTO
  irc_init_config(&irc_config);
  irc_resolve_cline(&irc_config);
  irc_connect_servers();
#endif /* ENABLE_IRC_PROTO */

  sock_server_loop ();

  /*
   * Disconnect the previously invoked internal coservers.
   */
  log_printf(LOG_NOTICE, "destroying internal coservers\n");
  
#if ENABLE_REVERSE_LOOKUP
  destroy_internal_coservers (COSERVER_REVERSE_DNS);
#endif
#if ENABLE_IDENT
  destroy_internal_coservers (COSERVER_IDENT);
#endif
#if ENABLE_DNS_LOOKUP
  destroy_internal_coservers (COSERVER_DNS);
#endif

  /*
   * Run the finalizers
   */
  server_finalize_all ();
  server_global_finalize ();
  coserver_finalize ();

#ifdef __MINGW32__
  net_cleanup();
#endif /* __MINGW32__ */

#if ENABLE_HTTP_PROTO
  http_free_content_types ();
  http_free_cache();
#endif
  
#if ENABLE_IRC_PROTO
  irc_close_config(&irc_config);
#endif

#if ENABLE_DEBUG
  log_printf(LOG_DEBUG, "%d byte(s) of memory in %d block(s) wasted\n", 
	     allocated_bytes, allocated_blocks);
#endif /* ENABLE_DEBUG */

  log_printf(LOG_NOTICE, "serveez terminating\n");

  if (log_file != stderr)
    fclose(log_file);

  return 0;
}
