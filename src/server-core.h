/*
 * server-core.h - server management definition
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
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
 *
 * $Id: server-core.h,v 1.6 2000/08/16 01:06:11 ela Exp $
 *
 */

#ifndef __SERVER_CORE_H__
#define __SERVER_CORE_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <time.h>
#include "socket.h"

/*
 * SERVER_CHILD_DIED is set to a non-zero value whenever the server
 * receives a SIGCHLD signal.
 */
extern HANDLE server_child_died;

/*
 * This is the pointer to the head of the list of sockets, which are
 * handled by the server loop.
 */
extern socket_t socket_root;

/* 
 * This holds the time on which the next call to handle_periodic_tasks()
 * should occur.
 */
extern time_t next_notify_time;

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

/*
 * Goes through all socket and shuts invalid ones down.
 */
void check_bogus_sockets (void);

/*
 * This routine gets called once a second and is supposed to
 * perform any task that has to get scheduled periodically.
 * It checks all sockets' timers and calls their timer functions
 * when necessary.
 */
int handle_periodic_tasks (void);

#endif /* not __SERVER_CORE_H__ */
