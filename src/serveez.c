/*
 * serveez.c - main module
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: serveez.c,v 1.31 2001/04/06 15:32:35 raimi Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif 

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <guile/gh.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#include "libserveez.h"
#include "serveez.h"
#include "cfgfile.h"
#include "option.h"
#include "guile.h"

/*
 * Print program version.
 */
static void 
version (void)
{
  fprintf (stdout, "%s %s\n", svz_library, svz_version);
}

/*
 * Display program command line options.
 */
static void 
usage (void)
{
  fprintf (stdout, "Usage: serveez [OPTION]...\n\n"
#if HAVE_GETOPT_LONG
 "  -h, --help               display this help and exit\n"
 "  -V, --version            display version information and exit\n"
 "  -i, --iflist             list local network interfaces and exit\n"
 "  -f, --cfg-file=FILENAME  file to use as configuration file (serveez.cfg)\n"
 "  -v, --verbose=LEVEL      set level of verbosity\n"
 "  -l, --log-file=FILENAME  use FILENAME for logging (default is stderr)\n"
 "  -P, --password=STRING    set the password for control connections\n"
 "  -m, --max-sockets=COUNT  set the max. number of socket descriptors\n"
 "  -d, --daemon             start as daemon in background\n"
#else /* not HAVE_GETOPT_LONG */
 "  -h           display this help and exit\n"
 "  -V           display version information and exit\n"
 "  -i           list local network interfaces and exit\n"
 "  -f FILENAME  file to use as configuration file (serveez.cfg)\n"
 "  -v LEVEL     set level of verbosity\n"
 "  -l FILENAME  use FILENAME for logging (default is stderr)\n"
 "  -P STRING    set the password for control connections\n"
 "  -m COUNT     set the max. number of socket descriptors\n"
 "  -d           start as daemon in background\n"
#endif /* not HAVE_GETOPT_LONG */
 "\nReport bugs to <bug-serveez@gnu.org>.\n");
}

#if HAVE_GETOPT_LONG
/*
 * Argument array for `getopt_long ()' system call.
 */
static struct option serveez_options[] = {
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'V'},
  {"iflist", no_argument, NULL, 'i'},
  {"daemon", no_argument, NULL, 'd'},
  {"verbose", required_argument, NULL, 'v'},
  {"cfg-file", required_argument, NULL, 'f'},
  {"log-file", required_argument, NULL, 'l'},
  {"password", required_argument, NULL, 'P'},
  {"max-sockets", required_argument, NULL, 'm'},
  {NULL, 0, NULL, 0}
};
#endif /* HAVE_GETOPT_LONG */

#define SERVEEZ_OPTIONS "l:hViv:f:P:m:d"

void
guile_main (int argc, char **argv)
{
  int cli_verbosity = -1;
  int cli_sockets = -1;
  char *cli_pass = NULL;
  char *cfg_file = NULL;
  FILE *log_handle = NULL;

  guile_load_config ("guile.cfg"); 
  if (load_config (cfg_file, argc, argv) == -1)
    {
      /* 
       * Something went wrong while configuration file loading, 
       * Message output by function itself...
       */
      exit (3);
    }

  /*
   * Make command line arguments overriding the configuration file settings.
   */
  if (cli_verbosity != -1)
    svz_verbosity = cli_verbosity;

  if (cli_sockets != -1)
    svz_config.max_sockets = cli_sockets;

  if (cli_pass)
    {
      free (svz_config.server_password);
      svz_config.server_password = cli_pass;
    }

#if ENABLE_DEBUG
  log_printf (LOG_NOTICE, "serveez starting, debugging enabled\n");
#endif /* ENABLE_DEBUG */
  
  log_printf (LOG_NOTICE, "%s\n", svz_sys_version ());
  svz_openfiles (svz_config.max_sockets);
  log_printf (LOG_NOTICE, "using %d socket descriptors\n",
	      svz_config.max_sockets);

  /* 
   * Startup the internal coservers here.
   */
  if (coserver_init () == -1)
    {
      exit (4);
    }

  /*
   * Initialise server instances.
   */
  if (svz_server_init_all () == -1)
    {
      /* 
       * Something went wrong while the server initialised themselves.
       * Abort silently.
       */
      exit (6);
    }

  /*
   * Actually open the ports.
   */
  if (server_start () == -1)
    {
      exit (7);
    }
  
  server_loop ();

  /*
   * Run the finalizers.
   */
  svz_server_finalize_all ();
  svz_servertype_finalize ();

  /*
   * Disconnect the previously invoked internal coservers.
   */
  log_printf (LOG_NOTICE, "destroying internal coservers\n");
  coserver_finalize ();

  svz_halt ();

#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "%d byte(s) of memory in %d block(s) wasted\n", 
	      svz_allocated_bytes, svz_allocated_blocks);

#if DEBUG_MEMORY_LEAKS
  svz_heap ();
#endif
#endif /* ENABLE_DEBUG */

#ifdef __MINGW32__
  if (cli_daemon)
    {
      windoze_stop_daemon ();
    }
#endif

  log_printf (LOG_NOTICE, "serveez terminating\n");

  if (log_handle != stderr)
    svz_fclose (log_handle);
}

/*
 * Main entry point.
 */
int
main (int argc, char *argv[])
{
  char *log_file = NULL;
  char *cfg_file = "serveez.cfg";

  int cli_verbosity = -1;
  int cli_sockets = -1;
  char *cli_pass = NULL;
  int cli_daemon = 0;

  FILE *log_handle = NULL;

  int arg;
#if HAVE_GETOPT_LONG
  int index;
#endif

  /* initialize the configuration structure */
  svz_boot ();

#if HAVE_GETOPT_LONG
  while ((arg = getopt_long (argc, argv, SERVEEZ_OPTIONS, serveez_options,
			     &index)) != EOF)
#else
  while ((arg = getopt (argc, argv, SERVEEZ_OPTIONS)) != EOF)
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

	case 'i':
	  svz_interface_list ();
	  exit (0);
	  break;

	case 'f':
	  cfg_file = optarg;
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
	  log_file = optarg;
	  break;

	case 'P':
	  if (optarg && strlen (optarg) >= 2)
	    {
#if ENABLE_CRYPT && HAVE_CRYPT
	      cli_pass = svz_pstrdup (crypt (optarg, optarg));
#else
	      cli_pass = svz_pstrdup (optarg);
#endif
	    }
	  break;

	case 'm':
	  if (optarg)
	    cli_sockets = atoi (optarg);
	  else
	    {
	      usage ();
	      exit (1);
	    }
	  if (svz_verbosity <= 0)
	    {
	      usage ();
	      exit (1);
	    }
	  break;

	case 'd':
	  cli_daemon = 1;
	  break;

	default:
	  usage ();
	  exit (1);
	}
    }

  /*
   * Send all log messages to LOG_HANDLE.
   */
  if (log_file && log_file[0])
    log_handle = svz_fopen (log_file, "w");
  
  if (!log_handle)
    log_handle = stderr;

  log_set_file (log_handle);

  /*
   * Start as daemon, not as foreground application.
   */
  if (cli_daemon)
    {
#ifndef __MINGW32__

      int pid;

      if ((pid = fork ()) == -1)
	{
	  log_printf (LOG_ERROR, "fork: %s\n", SYS_ERROR);
	  exit (1);
	}
      else if (pid != 0)
	{
	  exit (0);
	}
      /* Close the log file if necessary. */
      if (log_handle == stderr)
	log_set_file (NULL);
      /* Close stdin, stdout and stderr. */
      close (0);
      close (1);
      close (2);

#else /* __MINGW32__ */

      if (windoze_start_daemon (argv[0]) == -1)
	exit (1);
      if (log_handle == stderr)
	log_set_file (NULL);
      closehandle (GetStdHandle (STD_INPUT_HANDLE));
      closehandle (GetStdHandle (STD_OUTPUT_HANDLE));
      closehandle (GetStdHandle (STD_ERROR_HANDLE));

#endif /* __MINGW32__ */
    }

#if 0
  /*
   * DEBUG: Show what servers we are able to run.
   */
  svz_servertype_print ();
#endif

  /*
   * Load configuration.
   */
  init_server_definitions ();

  gh_enter (argc, argv, guile_main);
  return 0;
}
