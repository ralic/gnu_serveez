/*
 * boot.h - configuration and boot declarations
 *
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
 * $Id: boot.h,v 1.7 2001/06/06 17:40:21 raimi Exp $
 *
 */

#ifndef __BOOT_H__
#define __BOOT_H__ 1

#include "libserveez/defines.h"

/*
 * General serveez configuration structure.
 */
typedef struct
{
  /* program's password */
  char *server_password;
  /* defines how many clients are allowed to connect */
  SOCKET max_sockets;
  /* when was the program started */
  long start_time;
  /* log level verbosity */
  int verbosity;
}  
svz_config_t;

__BEGIN_DECLS

/* Core library configuration. */
SERVEEZ_API extern svz_config_t svz_config;

/* Exported functions. */
SERVEEZ_API int svz_net_startup __P ((void));
SERVEEZ_API int svz_net_cleanup __P ((void));
SERVEEZ_API void svz_init_config __P ((void));
SERVEEZ_API void svz_boot __P ((void));
SERVEEZ_API void svz_halt __P ((void));

/* Some static strings. */
SERVEEZ_API extern char *svz_library;
SERVEEZ_API extern char *svz_version;
SERVEEZ_API extern char *svz_build;

/* Exported from `boot.c' because it is a central point. */
SERVEEZ_API extern int svz_have_debug;
SERVEEZ_API extern int svz_have_Win32;
SERVEEZ_API extern int svz_have_floodprotect;

__END_DECLS

#endif /* not __BOOT_H__ */
