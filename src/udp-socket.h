/*
 * udp-socket.h - udp socket header definitions
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
 * $Id: udp-socket.h,v 1.2 2000/08/02 09:45:14 ela Exp $
 *
 */

#ifndef __UDP_SOCKET_H__
#define __UDP_SOCKET_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include "socket.h"

int udp_read_socket (socket_t sock);
int udp_write_socket (socket_t sock);
int udp_check_request (socket_t sock);
socket_t udp_connect (unsigned long host, unsigned short port);

#ifndef __STDC__
int udp_printf ();
#else
int udp_printf (socket_t sock, const char * fmt, ...);
#endif

#endif /* __UDP_SOCKET_H__ */
