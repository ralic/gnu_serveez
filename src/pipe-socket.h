/*
 * pipe-socket.h - pipes in socket structures header definitions
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
 * $Id
 *
 */

#ifndef __PIPE_SOCKET_H__
#define __PIPE_SOCKET_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include "socket.h"

#define READ  0 /* read pipe index */
#define WRITE 1 /* write pipe index */

/*
 * The pipe_read() function reads as much data as available on a readable
 * pipe descriptor. Return a non-zero value on errors.
 */
int pipe_read (socket_t sock);

/*
 * This pipe_write() writes as much data as possible into a writeable
 * pipe descriptor. It returns a non-zero value on errors.
 */
int pipe_write (socket_t sock);

/*
 * This function is the default disconnetion routine for pipe socket
 * structures. This is called via sock->disconnected_socket(). Return
 * no-zero on errors.
 */
int pipe_disconnected (socket_t sock);

/*
 * Create a socket structure by the two file descriptors recv_fd and
 * send_fd. This is used by coservers only, yet. Return NULL on errors.
 */
socket_t pipe_create (int recv_fd, int send_fd);


/*
 * Create a (non blocking) pipe. Differs in Win32 and Unices.
 * Return a non-zero value on errors.
 */
int create_pipe (HANDLE pipe_desc[2]);

#endif /* __PIPE_SOCKET_H__ */
