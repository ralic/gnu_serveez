/*
 * serveez.h - serveez interface
 *
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
 * Portions (C) 1995, 1996 Free Software Foundation, Inc.
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
 */

#ifndef __SERVEEZ_H__
#define __SERVEEZ_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <time.h>

/*
 * General serveez configuration structure.
 */
typedef struct
{
  char *program_name;
  char *version_string;
  char *build_string;
  char *server_password;
  int port;
  /*
   * This value defines how many clients are allowed to connect.
   * This is by default 200.
   */
  SOCKET max_sockets;
  time_t start_time;
}  
serveez_config_t;

extern serveez_config_t serveez_config;

/*
 * Each module - compiled in or not - defines an int variable. This is done
 * because ANSI C forbids empty .c files. So we use this int as a flag.
 * These ints can be used as read-only variables for sizzle bindings and for
 * other runtime checks.
 */

extern int have_awcs;
extern int have_floodprotect;
extern int have_http;
extern int have_irc;
extern int have_ctrl;
extern int have_ident;
extern int have_nslookup;

extern int have_debug;
extern int have_win32;

#endif /* __SERVEEZ_H__ */
