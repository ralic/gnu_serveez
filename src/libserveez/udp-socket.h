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
 * $Id: udp-socket.h,v 1.3 2001/05/19 23:04:58 ela Exp $
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
SERVEEZ_API int svz_udp_read_socket __P ((svz_socket_t *));
SERVEEZ_API int svz_udp_write_socket __P ((svz_socket_t *));
SERVEEZ_API int svz_udp_check_request __P ((svz_socket_t *));
SERVEEZ_API svz_socket_t *svz_udp_connect __P ((unsigned long, 
						unsigned short));
SERVEEZ_API int svz_udp_write __P ((svz_socket_t *, char *, int));
SERVEEZ_API int svz_udp_printf __P ((svz_socket_t *, const char *, ...));

__END_DECLS

#endif /* not __UDP_SOCKET_H__ */
