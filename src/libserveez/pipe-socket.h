/*
 * pipe-socket.h - pipes in socket structures header definitions
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
 * $Id: pipe-socket.h,v 1.3 2001/02/02 11:26:23 ela Exp $
 *
 */

#ifndef __PIPE_SOCKET_H__
#define __PIPE_SOCKET_H__ 1

#include "libserveez/defines.h"
#include "libserveez/socket.h"

#define READ  0 /* read pipe index */
#define WRITE 1 /* write pipe index */

__BEGIN_DECLS

SERVEEZ_API int pipe_valid __P ((socket_t sock));
SERVEEZ_API int pipe_read_socket __P ((socket_t sock));
SERVEEZ_API int pipe_write_socket __P ((socket_t sock));
SERVEEZ_API int pipe_disconnect __P ((socket_t sock));
SERVEEZ_API socket_t pipe_create __P ((HANDLE recv_fd, HANDLE send_fd));
SERVEEZ_API int pipe_create_pair __P ((HANDLE pipe_desc[2]));
SERVEEZ_API socket_t pipe_connect __P ((char *inpipe, char *outpipe));
SERVEEZ_API int pipe_listener __P ((socket_t server_sock));

__END_DECLS

#endif /* not __PIPE_SOCKET_H__ */
