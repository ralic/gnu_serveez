/*
 * pipe-socket.c - pipes in socket structures
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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "socket.h"
#include "util.h"
#include "pipe-socket.h"

/*
 * This function is for checking if a given socket is a valid pipe
 * socket (checking both pipes). Return non-zero on errors.
 */
int
pipe_valid (socket_t sock)
{
  if (sock->flags & SOCK_FLAG_PIPE &&
      sock->flags & SOCK_FLAG_CONNECTED)
    {
      if (sock->pipe_desc[READ] != INVALID_HANDLE && 
	  sock->pipe_desc[WRITE] != INVALID_HANDLE)
	{
	  return 0;
	}
      return -1;
    }
  return 0;
}

/*
 * This function is the default disconnetion routine for pipe socket
 * structures. This is called via sock->disconnected_socket(). Return
 * no-zero on errors.
 */
int
pipe_disconnected (socket_t sock)
{
#if ENABLE_DEBUG
  if (sock->flags & SOCK_FLAG_CONNECTED)
    {
      log_printf (LOG_DEBUG, "pipe (%d-%d) disconnected\n",
		  sock->pipe_desc[READ], sock->pipe_desc[WRITE]);
    }
  else if (sock->flags & SOCK_FLAG_LISTENING)
    {
      log_printf (LOG_DEBUG, "pipe listener (%s) destroyed\n",
		  sock->send_pipe);
    }
  else
    {
      log_printf (LOG_DEBUG, "invalid pipe id %d disconnected\n",
		  sock->socket_id);
    }
#endif

  return 0;
}

/*
 * The pipe_read() function reads as much data as available on a readable
 * pipe descriptor. Return a non-zero value on errors.
 */
int
pipe_read (socket_t sock)
{
  int num_read;
  int ret;
  int do_read;
  int desc;

  desc = (int) sock->pipe_desc[READ];
  do_read = sock->recv_buffer_size - sock->recv_buffer_fill;

  num_read = READ_PIPE (desc, sock->recv_buffer + sock->recv_buffer_fill,
			do_read);

  /* error occured while reading */
  if (num_read < 0)
    {
      log_printf (LOG_ERROR, "pipe read: %s\n", SYS_ERROR);
      return -1;
    }
  /* some data has been read */
  else if (num_read > 0)
    {
      sock->last_recv = time (NULL);
      sock->recv_buffer_fill += num_read;

      if (sock->check_request)
	{
	  if ((ret = sock->check_request(sock)) != 0)
	    return ret;
	}
    }
  /* the socket was selected but there is no data */
  else
    {
      log_printf (LOG_ERROR, "pipe read: no data on pipe %d\n", desc);
      return -1;
    }
  
  return 0;
}

/*
 * This pipe_write() writes as much data as possible into a writeable
 * pipe descriptor. It returns a non-zero value on errors.
 */
int
pipe_write (socket_t sock)
{
  int num_written;
  int do_write;
  int desc;

  desc = (int) sock->pipe_desc[WRITE];
  do_write = sock->send_buffer_fill;
  num_written = WRITE_PIPE (desc, sock->send_buffer, do_write);

  /* some data has been written */
  if (num_written > 0)
    {
      sock->last_send = time (NULL);
      if (sock->send_buffer_fill > num_written)
	{
	  memmove (sock->send_buffer, 
		   sock->send_buffer + num_written,
		   sock->send_buffer_fill - num_written);
	}
      sock->send_buffer_fill -= num_written;
    }
  /* error occured while writing */
  else if (num_written < 0)
    {
      log_printf (LOG_ERROR, "pipe write: %s\n", SYS_ERROR);
      if (last_errno == SOCK_UNAVAILABLE)
	{
	  sock->unavailable = time(NULL) + RELAX_FD_TIME;
	  num_written = 0;
	}
    }

  return (num_written < 0) ? -1 : 0;
}

#ifndef __MINGW32__

/*
 * Create a socket structure by the two file descriptors recv_fd and
 * send_fd. This is used by coservers only, yet. Return NULL on errors.
 */
socket_t
pipe_create (int recv_fd, int send_fd)
{
  socket_t sock;

  if (fcntl (recv_fd, F_SETFL, O_NONBLOCK) < 0)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", SYS_ERROR);
      return NULL;
    }

  if (fcntl (send_fd, F_SETFL, O_NONBLOCK) < 0)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", SYS_ERROR);
      return NULL;
    }

  if ((sock = sock_alloc ()) != NULL)
    {
      sock_unique_id (sock);
      sock->disconnected_socket = pipe_disconnected;
      sock->pipe_desc[READ] = recv_fd;
      sock->pipe_desc[WRITE] = send_fd;
      sock->flags |= (SOCK_FLAG_PIPE | SOCK_FLAG_CONNECTED);
    }

  return sock;
}

#endif /* __MINGW32__ */

/*
 * Create a (non blocking) pipe. Differs in Win32 and Unices.
 * Return a non-zero value on errors.
 */
int
create_pipe (HANDLE pipe_desc[2])
{
#ifdef __MINGW32__
  SECURITY_ATTRIBUTES sa = { sizeof (SECURITY_ATTRIBUTES), 
			     NULL,    /* NULL security descriptor */
			     TRUE };  /* Inherit handles */

  if (!CreatePipe (&pipe_desc[READ], &pipe_desc[WRITE], &sa, 0))
    {
      log_printf (LOG_ERROR, "CreatePipe: %s\n", SYS_ERROR);
      return -1;
    }
#else
  if (pipe (pipe_desc) == -1)
    {
      log_printf (LOG_ERROR, "pipe: %s\n", SYS_ERROR);
      return -1;
    }

  /* Make both ends of the pipe non blocking. */

  /* 
   * FIXME: Maybe cgi pipes MUST be blocking for *very* fast
   *        outputs because thay cannot handle the EAGAIN error.
   */

  if (fcntl (pipe_desc[READ], F_SETFL, O_NONBLOCK) == -1)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", SYS_ERROR);
      return -1;
    }
  if (fcntl (pipe_desc[WRITE], F_SETFL, O_NONBLOCK) == -1)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", SYS_ERROR);
      return -1;
    }
#endif
  return 0;
}
