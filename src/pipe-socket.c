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
 * $Id: pipe-socket.c,v 1.9 2000/09/11 00:07:35 raimi Exp $
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
#ifndef __MINGW32__
      if (unlink (sock->recv_pipe) == -1)
	log_printf (LOG_ERROR, "unlink: %s\n", SYS_ERROR);
      if (unlink (sock->send_pipe) == -1)
	log_printf (LOG_ERROR, "unlink: %s\n", SYS_ERROR);
#else /* __MINGW32__ */
      if (sock->pipe_desc[READ] != INVALID_HANDLE)
	if (!CloseHandle (sock->pipe_desc[READ]))
	  log_printf (LOG_ERROR, "CloseHandle: %s\n", SYS_ERROR);
      if (sock->pipe_desc[WRITE] != INVALID_HANDLE)
	if (!CloseHandle (sock->pipe_desc[WRITE]))
	  log_printf (LOG_ERROR, "CloseHandle: %s\n", SYS_ERROR);
#endif /* __MINGW32__ */

      log_printf (LOG_DEBUG, "pipe listener (%s) destroyed\n",
		  sock->send_pipe);
    }
  else
    {
      log_printf (LOG_DEBUG, "invalid pipe id %d disconnected\n",
		  sock->id);
    }
#endif

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
#ifdef __MINGW32__
  LPOVERLAPPED overlap;
#endif

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
  /*
   * On Win32 systems we use ReadFile with overlapped I/O to
   * "emulate" non-blocking descriptors. We MUST set it to NULL
   * for Win95 and less versions.
   */
  overlap = (os_version >= WinNT4x) ? &sock->overlap[READ] : NULL;
  if (!ReadFile (sock->pipe_desc[READ],
		 sock->recv_buffer + sock->recv_buffer_fill,
		 do_read, (DWORD *) &num_read, overlap))
    {
      log_printf (LOG_ERROR, "pipe ReadFile: %s\n", SYS_ERROR);
      if (last_errno == ERROR_IO_PENDING)
	GetOverlappedResult (sock->pipe_desc[READ], overlap, 
			     (DWORD *) &num_read, FALSE);
      else
	return -1;
    }
#else /* not __MINGW32__ */
  if ((num_read = read (sock->pipe_desc[READ],
			sock->recv_buffer + sock->recv_buffer_fill,
			do_read)) == -1)
    {
      log_printf (LOG_ERROR, "pipe read: %s\n", SYS_ERROR);
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
  /* the socket was selected but there is no data */
  else
    {
      log_printf (LOG_ERROR, "pipe read: no data on pipe %d\n", 
		  sock->pipe_desc[READ]);
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
  int num_written, do_write;
#ifdef __MINGW32__
  LPOVERLAPPED overlap;
#endif

  /* 
   * Write as many bytes as possible, remember how many
   * were actually sent. Do not write more than the content
   * length of the post data.
   */
  do_write = sock->send_buffer_fill;

#ifdef __MINGW32__
  overlap = (os_version >= WinNT4x) ? &sock->overlap[WRITE] : NULL;
  if (!WriteFile (sock->pipe_desc[WRITE], 
		  sock->send_buffer, 
		  do_write, (DWORD *) &num_written, overlap))
    {
      log_printf (LOG_ERROR, "pipe WriteFile: %s\n", SYS_ERROR);
      if (last_errno == ERROR_IO_PENDING)
	GetOverlappedResult (sock->pipe_desc[WRITE], overlap, 
			     (DWORD *) &num_written, FALSE);
      else
	num_written = -1;
    }
#else /* not __MINGW32__ */
  if ((num_written = write (sock->pipe_desc[WRITE], 
			    sock->send_buffer, 
			    do_write)) == -1)
    {
      if (last_errno == SOCK_UNAVAILABLE)
	{
	  sock->unavailable = time(NULL) + RELAX_FD_TIME;
	  num_written = 0;
	}
      log_printf (LOG_ERROR, "pipe write: %s\n", SYS_ERROR);
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
 * send_fd. This is used by coservers only, yet. Return NULL on errors.
 */
socket_t
pipe_create (HANDLE recv_fd, HANDLE send_fd)
{
  socket_t sock;

#ifndef __MINGW32__
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
      sock->disconnected_socket = pipe_disconnected;
      sock->pipe_desc[READ] = recv_fd;
      sock->pipe_desc[WRITE] = send_fd;
      sock->flags |= (SOCK_FLAG_PIPE | SOCK_FLAG_CONNECTED);
    }

  return sock;
}

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
