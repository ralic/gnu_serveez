/*
 * boot.c - configuration and boot functions
 *
 * Copyright (C) 2011 Thien-Thi Nguyen
 * Copyright (C) 2001, 2002, 2003 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package.  If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <time.h>
#include <sys/types.h>

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#include "version.h"
#include "libserveez/alloc.h"
#include "libserveez/util.h"
#include "libserveez/vector.h"
#include "libserveez/interface.h"
#include "libserveez/socket.h"
#include "libserveez/icmp-socket.h"
#include "libserveez/pipe-socket.h"
#include "libserveez/server.h"
#include "libserveez/dynload.h"
#include "libserveez/boot.h"
#include "libserveez/mutex.h"
#include "libserveez/server-core.h"
#include "libserveez/codec/codec.h"

/*
 * The configuration structure of the core library.
 */
svz_config_t svz_config = { NULL, 0, 0, 0 };

/* The symbolic name of the core library.  */
char *svz_library = "serveez";
/* The version of the core library.  */
char *svz_version = __serveez_version;

/* Runtime flag if this is the debug version or not.  */
#ifdef ENABLE_DEBUG
int svz_have_debug = 1;
#else
int svz_have_debug = 0;
#endif

/* Runtime checkable flags for configuration language and code if flood
   protection has been enabled or not.  */
#ifdef ENABLE_FLOOD_PROTECTION
int svz_have_floodprotect = 1;
#else
int svz_have_floodprotect = 0;
#endif

/*
 * Return a list (length saved to @var{count}) of strings
 * representing the features compiled into libserveez.
 */
const char * const *
svz_library_features (size_t *count)
{
  static const char * const features[] = {
#ifdef ENABLE_DEBUG
    "debug",
#endif
#ifdef ENABLE_HEAP_COUNT
    "heap-counters",
#endif
#ifdef ENABLE_IFLIST
    "interface-list",
#endif
#if defined ENABLE_POLL && defined HAVE_POLL
    "poll",
#endif
#if defined ENABLE_SENDFILE && defined HAVE_SENDFILE
    "sendfile",
#endif
#ifdef SVZ_HAVE_THREADS
    "threads",
#endif
#ifdef ENABLE_FLOOD_PROTECTION
    "flood-protection",
#endif
    "core"
  };

  *count = sizeof (features) / sizeof (char *);

  return features;
}

/* Extern declaration of the logging mutex.  */
svz_mutex_declare (svz_log_mutex)

/*
 * This routine has to be called once before you could use any of the
 * serveez core library functions.
 */
int
svz_net_startup (void)
{
#ifdef __MINGW32__
  WSADATA WSAData;

  /* Call this once before using Winsock API.  */
  if (WSAStartup (WINSOCK_VERSION, &WSAData) == SOCKET_ERROR)
    {
      svz_log (LOG_ERROR, "WSAStartup: %s\n", NET_ERROR);
      WSACleanup ();
      return 0;
    }

  /* Startup IP services.  */
  svz_icmp_startup ();

#endif /* __MINGW32__ */

  return 1;
}

/*
 * Shutdown the serveez core library.
 */
int
svz_net_cleanup (void)
{
#ifdef __MINGW32__
  /* Shutdown IP services.  */
  svz_icmp_cleanup ();

  /* Call this when disconnecting from Winsock API.  */
  if (WSACleanup () == SOCKET_ERROR)
    {
      svz_log (LOG_ERROR, "WSACleanup: %s\n", NET_ERROR);
      return 0;
    }

#endif /* not __MINGW32__ */

  return 1;
}

/*
 * Initialization of the configuration.
 */
void
svz_init_config (void)
{
  svz_config.start = time (NULL);
  svz_config.verbosity = LOG_DEBUG;
  svz_config.max_sockets = 100;
  svz_config.password = NULL;
}

/*
 * Initialization of the core library.
 */
void
svz_boot (void)
{
  svz_mutex_create (&svz_log_mutex);
  svz_strsignal_init ();
  svz_sock_table_create ();
  svz_signal_up ();
  svz_init_config ();
  svz_interface_collect ();
  svz_net_startup ();
  svz_pipe_startup ();
  svz_dynload_init ();
  svz_codec_init ();
  svz_config_type_init ();
}

/*
 * Finalization of the core library.
 */
void
svz_halt (void)
{
  svz_free_and_zero (svz_config.password);
  svz_portcfg_finalize ();
  svz_config_type_finalize ();
  svz_codec_finalize ();
  svz_dynload_finalize ();
  svz_pipe_cleanup ();
  svz_net_cleanup ();
  svz_interface_free ();
  svz_signal_dn ();
  svz_sock_table_destroy ();
  svz_strsignal_destroy ();
  svz_mutex_destroy (&svz_log_mutex);
}
