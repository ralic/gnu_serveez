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
 * $Id: boot.c,v 1.4 2001/01/31 12:30:14 ela Exp $
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
#include "libserveez/boot.h"

/*
 * The configuration structure of the core library.
 */
svz_config_t svz_config;

/*
 * Setup some static strings.
 */
char *svz_library = "serveez";
char *svz_version = __serveez_version;
char *svz_build = __serveez_timestamp;

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
}

/*
 * Finalization of the core library.
 */
void
svz_halt (void)
{
}
