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
 * $Id: pipe-socket.h,v 1.8 2001/01/25 10:57:57 ela Exp $
 *
 */

#ifndef __PIPE_SOCKET_H__
#define __PIPE_SOCKET_H__ 1

#define _GNU_SOURCE
#include <internal.h>
#include "socket.h"

#define READ  0 /* read pipe index */
#define WRITE 1 /* write pipe index */

#ifdef __MINGW32__

/* pipe info structure for Win32 */
typedef struct
{
  /* critical section objects for the pipe threads */
  CRITICAL_SECTION recv_sync;
  CRITICAL_SECTION send_sync;

  /* pipe thread handles and IDs */
  HANDLE recv_thread;
  HANDLE send_thread;
  DWORD recv_tid;
  DWORD send_tid;
}
pipe_t;

#endif /* __MINGW32__ */

__BEGIN_DECLS

/*
 * This function is for checking if a given socket is a valid pipe
 * socket (checking both pipes). Return non-zero on errors.
 */
SERVEEZ_API int pipe_valid __P ((socket_t sock));

/*
 * The pipe_read() function reads as much data as available on a readable
 * pipe descriptor. Return a non-zero value on errors.
 */
SERVEEZ_API int pipe_read __P ((socket_t sock));

/*
 * This pipe_write() writes as much data as possible into a writable
 * pipe descriptor. It returns a non-zero value on errors.
 */
SERVEEZ_API int pipe_write __P ((socket_t sock));

/*
 * This function is the default disconnetion routine for pipe socket
 * structures. Return non-zero on errors.
 */
SERVEEZ_API int pipe_disconnect __P ((socket_t sock));

/*
 * Create a socket structure by the two file descriptors recv_fd and
 * send_fd. Return NULL on errors.
 */
SERVEEZ_API socket_t pipe_create __P ((HANDLE recv_fd, HANDLE send_fd));

/*
 * Create a (non blocking) pipe. Differs in Win32 and Unices.
 * Return a non-zero value on errors.
 */
SERVEEZ_API int pipe_create_pair __P ((HANDLE pipe_desc[2]));

/*
 * This routine creates a pipe connection socket structure to a named
 * pipe. Return NULL on errors.
 */
SERVEEZ_API socket_t pipe_connect __P ((char *inpipe, char *outpipe));

/*
 * Create a socket structure for listening pipe sockets. Open the reading
 * end of such a connection. Return either zero or non-zero on errors.
 */
SERVEEZ_API int pipe_listener __P ((socket_t server_sock));

__END_DECLS

#endif /* !__PIPE_SOCKET_H__ */
