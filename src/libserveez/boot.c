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

#include "config.h"

#include <stdio.h>
#include <time.h>
#include <sys/types.h>

#ifdef __MINGW32__
# include <winsock2.h>
#endif

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
static int
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
static int
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

static void
svz__net_updn (int direction)
{
  (direction
   ? svz_net_startup
   : svz_net_cleanup)
    ();
}

/*
 * Initialization of the configuration.
 */
static void
svz_init_config (void)
{
  svz_config.start = time (NULL);
  svz_config.verbosity = LOG_DEBUG;
  svz_config.max_sockets = 100;
  svz_config.password = NULL;
}

/* These are used only in ‘svz_boot’ and ‘svz_halt’,
   so it's easier to just declare them here.  */

#define UPDN(x)  SBO void svz__ ## x ## _updn (int direction)

UPDN (log);
UPDN (strsignal);
UPDN (sock_table);
UPDN (signal);
UPDN (interface);
UPDN (pipe);
UPDN (dynload);
UPDN (codec);
UPDN (config_type);

SBO void svz_portcfg_finalize (void);

/*
 * Initialization of the core library.
 */
void
svz_boot (void)
{
#define UP(x)  svz__ ## x ## _updn (1)

  UP (log);
  UP (strsignal);
  UP (sock_table);
  UP (signal);
  svz_init_config ();
  UP (interface);
  UP (net);
  UP (pipe);
  UP (dynload);
  UP (codec);
  UP (config_type);

#undef UP
}

/*
 * Finalization of the core library.
 */
void
svz_halt (void)
{
#define DN(x)  svz__ ## x ## _updn (0)

  svz_free_and_zero (svz_config.password);
  svz_portcfg_finalize ();
  DN (config_type);
  DN (codec);
  DN (dynload);
  DN (pipe);
  DN (net);
  DN (interface);
  DN (signal);
  DN (sock_table);
  DN (strsignal);
  DN (log);

#undef DN
}
