/*
 * serveez.h - serveez interface
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
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
 * $Id: serveez.h,v 1.5 2000/09/11 00:07:35 raimi Exp $
 *
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
  /* program name */
  char *program_name;
  /* version string of program */
  char *version_string;
  /* biuld string of program */
  char *build_string;
  /* programs password */
  char *server_password;
  /* defines how many clients are allowed to connect */
  SOCKET max_sockets;
  /* when was the program started */
  time_t start_time;
}  
serveez_config_t;

extern serveez_config_t serveez_config;

/* exported from util.c because it is a central point */
extern int have_debug;
extern int have_win32;

#endif /* __SERVEEZ_H__ */
