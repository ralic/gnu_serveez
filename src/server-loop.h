/*
 * server-loop.h - server loop definition
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: server-loop.h,v 1.3 2000/09/20 08:29:14 ela Exp $
 *
 */

#ifndef __SERVER_LOOP_H__
#define __SERVER_LOOP_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

/*
 * Check the server and client sockets for incoming connections 
 * and data, and process outgoing data.
 */
int server_check_sockets_select (void);
#if HAVE_POLL && ENABLE_POLL
int server_check_sockets_poll (void);
#endif
#ifdef __MINGW32__
int server_check_sockets_MinGW (void);
#endif

#if HAVE_POLL && ENABLE_POLL
# define server_check_sockets() server_check_sockets_poll ()
#elif defined (__MINGW32__)
# define server_check_sockets() server_check_sockets_MinGW ()
#else
# define server_check_sockets() server_check_sockets_select ()
#endif

#endif /* not __SERVER_LOOP_H__ */
