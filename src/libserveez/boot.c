/*
 * boot.c - configuration and boot functions
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: boot.c,v 1.8 2001/03/04 13:13:40 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif 

#define _GNU_SOURCE
#include <time.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "version.h"
#include "libserveez/util.h"
#include "libserveez/vector.h"
#include "libserveez/interface.h"
#include "libserveez/socket.h"
#include "libserveez/server.h"
#include "libserveez/dynload.h"
#include "libserveez/boot.h"

/*
 * The configuration structure of the core library.
 */
svz_config_t svz_config;

/* The symbolic name of the core library. */
char *svz_library = "serveez";
/* The version of the core library. */
char *svz_version = __serveez_version;
/* Timestamp when core library has been build. */
char *svz_build = __serveez_timestamp;

/*
 * This routine has to be called once before you could use any of the
 * serveez core library functions.
 */
int
svz_net_startup (void)
{
#ifdef __MINGW32__
  WSADATA WSAData;
 
  /* Call this once before using Winsock API. */
  if (WSAStartup (WINSOCK_VERSION, &WSAData) == SOCKET_ERROR)
    {
      log_printf (LOG_ERROR, "WSAStartup: %s\n", NET_ERROR);
      WSACleanup ();
      return 0;
    }
  
  /* Startup IP services. */
  icmp_startup ();

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
  /* Shutdown IP services. */
  icmp_cleanup ();

  /* Call this when disconnecting from Winsock API. */
  if (WSACleanup () == SOCKET_ERROR)
    {
      log_printf (LOG_ERROR, "WSACleanup: %s\n", NET_ERROR);
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
  svz_config.start_time = time (NULL);
}

/*
 * Initialization of the core library.
 */
void
svz_boot (void)
{
  svz_init_config ();
  svz_interface_collect ();
  svz_net_startup ();
  svz_dynload_init ();
}

/*
 * Finalization of the core library.
 */
void
svz_halt (void)
{
  svz_dynload_finalize ();
  svz_net_cleanup ();
  svz_interface_free ();
}
