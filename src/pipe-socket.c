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
 * $Id: pipe-socket.c,v 1.15 2000/10/08 21:14:03 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <fcntl.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "alloc.h"
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
  if (sock->flags & SOCK_FLAG_LISTENING)
    return 0;

  if (!(sock->flags & SOCK_FLAG_CONNECTED))
    return -1;

  if (sock->flags & SOCK_FLAG_RECV_PIPE)
    if (sock->pipe_desc[READ] == INVALID_HANDLE)
      return -1;

  if (sock->flags & SOCK_FLAG_SEND_PIPE)
    if (sock->pipe_desc[WRITE] == INVALID_HANDLE)
      return -1;

  return 0;
}

/*
 * This function is the default disconnection routine for pipe socket
 * structures. Return non-zero on errors.
 */
int
pipe_disconnect (socket_t sock)
{
  if (sock->flags & SOCK_FLAG_CONNECTED)
    {
      if (sock->referer)
	{
#ifdef __MINGW32__
	  /* just disconnect client pipes */
	  if (!DisconnectNamedPipe (sock->pipe_desc[READ]))
	    log_printf (LOG_ERROR, "DisconnectNamedPipe: %s\n", SYS_ERROR);
	  if (!DisconnectNamedPipe (sock->pipe_desc[WRITE]))
	    log_printf (LOG_ERROR, "DisconnectNamedPipe: %s\n", SYS_ERROR);
#endif /* not __MINGW32__ */

	  /* restart listening pipe server socket */
	  sock->referer->flags &= ~SOCK_FLAG_INITED;
	  sock->referer->referer = NULL;
	}
      else
	{
	  /* close both pipes */
	  if (sock->pipe_desc[READ] != INVALID_HANDLE)
	    if (closehandle (sock->pipe_desc[READ]) < 0)
	      log_printf(LOG_ERROR, "close: %s\n", SYS_ERROR);
	  if (sock->pipe_desc[WRITE] != INVALID_HANDLE)
	    if (closehandle (sock->pipe_desc[WRITE]) < 0)
	      log_printf (LOG_ERROR, "close: %s\n", SYS_ERROR);
	}

#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "pipe (%d-%d) disconnected\n",
		  sock->pipe_desc[READ], sock->pipe_desc[WRITE]);
#endif

      sock->pipe_desc[READ] = INVALID_HANDLE;
      sock->pipe_desc[WRITE] = INVALID_HANDLE;
    }
  
  /* prevent a pipe server's child to reinit the pipe server */
  if (sock->flags & SOCK_FLAG_LISTENING)
    {
      if (sock->referer)
	sock->referer->referer = NULL;

#ifndef __MINGW32__

      /* delete named pipes on file system */
      if (unlink (sock->recv_pipe) == -1)
	log_printf (LOG_ERROR, "unlink: %s\n", SYS_ERROR);
      if (unlink (sock->send_pipe) == -1)
	log_printf (LOG_ERROR, "unlink: %s\n", SYS_ERROR);

#else /* __MINGW32__ */

      /* disconnect and close named pipes */
      if (sock->pipe_desc[READ] != INVALID_HANDLE)
	{
	  if (!DisconnectNamedPipe (sock->pipe_desc[READ]))
	    log_printf (LOG_ERROR, "DisconnectNamedPipe: %s\n", SYS_ERROR);
	  if (!CloseHandle (sock->pipe_desc[READ]))
	    log_printf (LOG_ERROR, "CloseHandle: %s\n", SYS_ERROR);
	}
      if (sock->pipe_desc[WRITE] != INVALID_HANDLE)
	{
	  if (!DisconnectNamedPipe (sock->pipe_desc[WRITE]))
	    log_printf (LOG_ERROR, "DisconnectNamedPipe: %s\n", SYS_ERROR);
	  if (!CloseHandle (sock->pipe_desc[WRITE]))
	    log_printf (LOG_ERROR, "CloseHandle: %s\n", SYS_ERROR);
	}

#endif /* __MINGW32__ */

#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "pipe listener (%s) destroyed\n",
		  sock->send_pipe);
#endif

      sock->pipe_desc[READ] = INVALID_HANDLE;
      sock->pipe_desc[WRITE] = INVALID_HANDLE;
    }

  return 0;
}

/*
 * The pipe_read() function reads as much data as available on a readable
 * pipe descriptor or handle on Win32. Return a non-zero value on errors.
 */
int
pipe_read (socket_t sock)
{
  int num_read, do_read;

  /* 
   * Read as much space is left in the receive buffer and return 
   * zero if there is no more space.
   */
  do_read = sock->recv_buffer_size - sock->recv_buffer_fill;
  if (do_read <= 0) 
    {
      log_printf (LOG_ERROR, "receive buffer overflow on pipe %d\n", 
		  sock->pipe_desc[READ]);
      
      if (sock->kicked_socket)
	sock->kicked_socket (sock, 0);
      
      return -1;
    }

#ifdef __MINGW32__
  /* check how many bytes could be read from the cgi pipe */
  if (!PeekNamedPipe (sock->pipe_desc[READ], NULL, 0, 
		      NULL, (DWORD *) &num_read, NULL))
    {
      log_printf (LOG_ERROR, "pipe: PeekNamedPipe: %s\n", SYS_ERROR);
      return -1;
    }

  /* leave this function if there is no data within the pipe */
  if (num_read <= 0)
    return 0;

  /* adjust number of bytes to read */
  if (do_read > num_read) do_read = num_read;

  /* really read from pipe */
  if (!ReadFile (sock->pipe_desc[READ],
		 sock->recv_buffer + sock->recv_buffer_fill,
		 do_read, (DWORD *) &num_read, NULL))
    {
      log_printf (LOG_ERROR, "pipe: ReadFile: %s\n", SYS_ERROR);
      return -1;
    }
#else /* not __MINGW32__ */
  if ((num_read = read (sock->pipe_desc[READ],
			sock->recv_buffer + sock->recv_buffer_fill,
			do_read)) == -1)
    {
      log_printf (LOG_ERROR, "pipe: read: %s\n", SYS_ERROR);
      return -1;
    }
#endif /* not __MINGW32__ */

  /* Some data has been read from the pipe. */
  else if (num_read > 0)
    {
      sock->last_recv = time (NULL);

#if ENABLE_FLOOD_PROTECTION
      if (default_flood_protect (sock, num_read))
	{
	  log_printf (LOG_ERROR, "kicked pipe %d (flood)\n", 
		      sock->pipe_desc[READ]);
	  return -1;
	}
#endif /* ENABLE_FLOOD_PROTECTION */

      sock->recv_buffer_fill += num_read;

      if (sock->check_request)
	if (sock->check_request (sock))
	  return -1;
    }

#ifndef __MINGW32__
  /* the socket was selected but there is no data */
  else
    {
      log_printf (LOG_ERROR, "pipe: read: no data on pipe %d\n", 
		  sock->pipe_desc[READ]);
      return -1;
    }
#endif /* !__MINGW32__ */
  
  return 0;
}

/*
 * This pipe_write() writes as much data as possible into a writeable
 * pipe descriptor. It returns a non-zero value on errors.
 */
int
pipe_write (socket_t sock)
{
  int num_written, do_write;

  /* 
   * Write as many bytes as possible, remember how many
   * were actually sent. Do not write more than the content
   * length of the post data.
   */
  do_write = sock->send_buffer_fill;

#ifdef __MINGW32__
  if (!WriteFile (sock->pipe_desc[WRITE], sock->send_buffer, 
		  do_write, (DWORD *) &num_written, NULL))
    {
      log_printf (LOG_ERROR, "pipe: WriteFile: %s\n", SYS_ERROR);
      num_written = -1;
    }
#else /* not __MINGW32__ */
  if ((num_written = write (sock->pipe_desc[WRITE], 
			    sock->send_buffer, 
			    do_write)) == -1)
    {
      log_printf (LOG_ERROR, "pipe: write: %s\n", SYS_ERROR);
      if (last_errno == SOCK_UNAVAILABLE)
	{
	  sock->unavailable = time (NULL) + RELAX_FD_TIME;
	  num_written = 0;
	}
    }
#endif /* not __MINGW32__ */

  /* Some data has been successfully written to the pipe. */
  else if (num_written > 0)
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

  return (num_written < 0) ? -1 : 0;
}

/*
 * Create a socket structure by the two file descriptors recv_fd and
 * send_fd. Return NULL on errors.
 */
socket_t
pipe_create (HANDLE recv_fd, HANDLE send_fd)
{
  socket_t sock;

#ifndef __MINGW32__
  /* Try to set to non-blocking I/O. */
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
#endif /* not __MINGW32__ */

  if ((sock = sock_alloc ()) != NULL)
    {
      sock_unique_id (sock);
      sock->pipe_desc[READ] = recv_fd;
      sock->pipe_desc[WRITE] = send_fd;
      sock->flags |= (SOCK_FLAG_PIPE | SOCK_FLAG_CONNECTED);
    }

  return sock;
}

/*
 * Create a (non blocking) pair of pipes. This differs in Win32 and 
 * Unices. Return a non-zero value on errors.
 */
int
pipe_create_pair (HANDLE pipe_desc[2])
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
#else /* not __MINGW32__ */

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
#endif /* not __MINGW32__ */

  return 0;
}
