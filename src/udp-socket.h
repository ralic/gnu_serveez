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
 * $Id: udp-socket.h,v 1.5 2000/10/28 13:03:11 ela Exp $
 *
 */

#ifndef __UDP_SOCKET_H__
#define __UDP_SOCKET_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include "socket.h"

/* general defines */
#define UDP_MSG_SIZE (64 * 1024)               /* maximum size of udp packet */
#define UDP_BUF_SIZE (4 * (UDP_MSG_SIZE + 24)) /* space for 4 messages */

/* exported functions */
int udp_read_socket (socket_t sock);
int udp_write_socket (socket_t sock);
int udp_check_request (socket_t sock);
socket_t udp_connect (unsigned long host, unsigned short port);
int udp_write (socket_t sock, char *buf, int length);

#ifndef __STDC__
int udp_printf ();
#else
int udp_printf (socket_t sock, const char * fmt, ...);
#endif

#endif /* __UDP_SOCKET_H__ */
