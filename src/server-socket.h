/*
 * server-socket.h - server socket definitions and declarations
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
 * $Id: server-socket.h,v 1.2 2000/06/11 21:39:17 raimi Exp $
 *
 */

#ifndef __SERVER_SOCKET_H__
#define __SERVER_SOCKET_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "socket.h"
#include "server.h"

/*
 * This structure is used by server_bind () to collect various server
 * instances and their port configurations.
 */
typedef struct
{
  server_t *server; /* server instance */
  portcfg_t *cfg;   /* port configuration */
}
server_binding_t;

/* protocol specific definitions */
#define SERV_FLAG_IRC         0x0004 /* IRC protocol */
#define SERV_FLAG_AWCS_MASTER 0x0008 /* AWCS protocol */
#define SERV_FLAG_AWCS_CLIENT 0x0008 /* AWCS protocol */
#define SERV_FLAG_CTRL        0x0010 /* Control protocol */
#define SERV_FLAG_HTTP        0x0020 /* HTTP protocol */

#define SERV_FLAG_UNIVERSAL   (SERV_FLAG_IRC |         \
			       SERV_FLAG_AWCS_MASTER | \
			       SERV_FLAG_AWCS_CLIENT | \
			       SERV_FLAG_CTRL |        \
			       SERV_FLAG_HTTP)

/*
 * Start all server bindings (instances of servers).
 */
int server_start (void);

/*
 * This functions binds a previouly instanciated server to a specified
 * port configuration.
 */
int server_bind (server_t *server, portcfg_t *cfg);

/*
 * Create a listening server socket. TYPE is either UDP or TCP. 
 * PROTOCOL can be one of the apropiate SERV_FLAG_* flags and
 * PORT is the port to bind the server socket to.
 * Return a null pointer on errors.
 */
socket_t server_create (portcfg_t *cfg);

/*
 * Something happened on the a server socket, most probably a client 
 * connection which we will normally accept.
 */
int server_accept_socket (socket_t sock);
int server_accept_pipe (socket_t sock);

/*
 * This routine has to be called once before you could use any of the
 * Winsock32 API functions under Win32.
 */
int net_startup (void);

/*
 * Shutdown the Winsock32 API.
 */
int net_cleanup (void);

#endif /* not __SERVER_SOCKET_H__ */
