/*
 * udp-socket.h - udp socket header definitions
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: udp-socket.h,v 1.2 2001/02/02 11:26:24 ela Exp $
 *
 */

#ifndef __UDP_SOCKET_H__
#define __UDP_SOCKET_H__ 1

#include "libserveez/defines.h"
#include "libserveez/socket.h"

/* general defines */
#define UDP_MSG_SIZE (64 * 1024)               /* maximum size of udp packet */
#define UDP_BUF_SIZE (4 * (UDP_MSG_SIZE + 24)) /* space for 4 messages */

__BEGIN_DECLS

/* exported UDP socket functions */
SERVEEZ_API int udp_read_socket __P ((socket_t sock));
SERVEEZ_API int udp_write_socket __P ((socket_t sock));
SERVEEZ_API int udp_check_request __P ((socket_t sock));
SERVEEZ_API socket_t udp_connect __P ((unsigned long, unsigned short));
SERVEEZ_API int udp_write __P ((socket_t sock, char *buf, int length));
SERVEEZ_API int udp_printf __P ((socket_t sock, const char *fmt, ...));

__END_DECLS

#endif /* not __UDP_SOCKET_H__ */
