/*
 * server-socket.h - server socket definitions and declarations
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
 * $Id: server-socket.h,v 1.1 2001/01/28 03:26:55 ela Exp $
 *
 */

#ifndef __SERVER_SOCKET_H__
#define __SERVER_SOCKET_H__ 1

#include "libserveez/defines.h"

__BEGIN_DECLS

/*
 * Create a listening server socket. PORTCFG is the port to bind the 
 * server socket to. Return a null pointer on errors.
 */
SERVEEZ_API socket_t server_create __P ((portcfg_t *cfg));

/*
 * Something happened on the a server socket, most probably a client 
 * connection which we will normally accept.
 */
SERVEEZ_API int server_accept_socket __P ((socket_t sock));
SERVEEZ_API int server_accept_pipe __P ((socket_t sock));

#ifdef __MINGW32__
/*
 * This routine has to be called once before you could use any of the
 * Winsock32 API functions under Win32.
 */
SERVEEZ_API int net_startup __P ((void));

/*
 * Shutdown the Winsock32 API.
 */
SERVEEZ_API int net_cleanup __P ((void));

#endif /* __MINGW32__ */

__END_DECLS

#endif /* not __SERVER_SOCKET_H__ */
