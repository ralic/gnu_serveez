/*
 * server-core.h - server management definition
 *
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
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
 */

#ifndef __SERVER_CORE_H__
#define __SERVER_CORE_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "socket.h"

/*
 * This is the pointer to the head of the list of sockets, which are
 * handled by the server loop.
 */
extern socket_t socket_root;

/*
 * Main server loop.  Handle all signals, incoming connections and
 * listening  server sockets.
 */
int sock_server_loop (void);

/*
 * Return the socket structure for the socket ID or NULL
 * if no such socket exists.
 */
socket_t find_sock_by_id (int id);

/*
 * Mark socket SOCK as killed.  That means that no operations except
 * disconnecting and freeing are allowed anymore.  All marked sockets
 * will be deleted once the server loop is through.  Note that this
 * function calls SOCK's disconnect handler if defined.
 */
int sock_schedule_for_shutdown (socket_t sock);

/*
 * Enqueue the socket SOCK into the list of sockets handled by
 * the server loop.
 */
int sock_enqueue (socket_t sock);

/*
 * Remove the socket SOCK from the list of sockets handled by
 * the server loop.
 */
int sock_dequeue (socket_t sock);

int pipe_read (socket_t sock);
int pipe_write (socket_t sock);

#ifdef __MINGW32__

#define WINSOCK_VERSION 0x0202 /* This is version 2.02. */
#define PIPE_BUF_SIZE   1024   /* Pipe buffer size. */

#define pipe(phandle) _pipe(phandle, PIPE_BUF_SIZE, O_BINARY)
#define dup(handle) _dup(handle)
#define dup2(handle1, handle2) _dup2(handle1, handle2)

#endif

#endif /* not __SERVER_H__ */
