/*
 * pipe-socket.c - pipes in socket structures
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
 * $Id: pipe-socket.c,v 1.1 2001/01/28 03:26:55 ela Exp $
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
#include <sys/stat.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "libserveez/alloc.h"
#include "libserveez/util.h"
#include "libserveez/socket.h"
#include "libserveez/server-core.h"
#include "libserveez/pipe-socket.h"

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
      if (sock->referrer)
	{
#ifdef __MINGW32__

	  /* just disconnect client pipes */
	  if (!DisconnectNamedPipe (sock->pipe_desc[READ]))
	    log_printf (LOG_ERROR, "DisconnectNamedPipe: %s\n", SYS_ERROR);
	  if (!DisconnectNamedPipe (sock->pipe_desc[WRITE]))
	    log_printf (LOG_ERROR, "DisconnectNamedPipe: %s\n", SYS_ERROR);

	  /* reinitialize the overlapped structure */
	  if (svz_os_version >= WinNT4x)
	    {
	      memset (sock->overlap[READ], 0, sizeof (OVERLAPPED));
	      memset (sock->overlap[WRITE], 0, sizeof (OVERLAPPED));
	      sock->overlap[READ] = NULL;
	      sock->overlap[WRITE] = NULL;
	    }

#else /* not __MINGW32__ */

	  /* close sending pipe */
	  if (sock->pipe_desc[WRITE] != INVALID_HANDLE)
	    if (closehandle (sock->pipe_desc[WRITE]) < 0)
	      log_printf (LOG_ERROR, "close: %s\n", SYS_ERROR);

	  /* FIXME: reset receiving pipe ??? */

#endif /* not __MINGW32__ */

	  /* restart listening pipe server socket */
	  sock->referrer->flags &= ~SOCK_FLAG_INITED;
	  sock->referrer->referrer = NULL;
	}
      else
	{
	  /* close both pipes */
	  if (sock->pipe_desc[READ] != INVALID_HANDLE)
	    if (closehandle (sock->pipe_desc[READ]) < 0)
	      log_printf (LOG_ERROR, "close: %s\n", SYS_ERROR);
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
      if (sock->referrer)
	sock->referrer->referrer = NULL;

#ifndef __MINGW32__

      /* close listening pipe */
      if (sock->pipe_desc[READ] != INVALID_HANDLE)
	if (closehandle (sock->pipe_desc[READ]) < 0)
	  log_printf (LOG_ERROR, "close: %s\n", SYS_ERROR);

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
		  sock->recv_pipe);
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
      if (errno == EAGAIN)
	return 0;
      return -1;
    }
#endif /* not __MINGW32__ */

  /* Some data has been read from the pipe. */
  else if (num_read > 0)
    {
      sock->last_recv = time (NULL);

#if ENABLE_FLOOD_PROTECTION
      if (sock_default_flood_protect (sock, num_read))
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
  /* Named pipes in Win32 cannot transfer more than 64KB at once. */
  if (do_write > 64 * 1024) do_write = 64 * 1024;

  if (!WriteFile (sock->pipe_desc[WRITE], sock->send_buffer, 
		  do_write, (DWORD *) &num_written, sock->overlap[WRITE]))
    {
      if (GetLastError () != ERROR_IO_PENDING)
	{
	  log_printf (LOG_ERROR, "pipe: WriteFile: %s\n", SYS_ERROR);
	  return -1;
	}

      /* 
       * Data bytes have been stored in system's cache. Now we are
       * checking if pending write operation has been completed.
       */
      if (!GetOverlappedResult (sock->pipe_desc[WRITE], 
				sock->overlap[WRITE], 
				(DWORD *) &num_written, FALSE))
	{
	  if (GetLastError () != ERROR_IO_INCOMPLETE)
	    {
	      log_printf (LOG_ERROR, "pipe: GetOverlappedResult: %s\n", 
			  SYS_ERROR);
	      return -1;
	    }
	}
      num_written = do_write;
    }
#else /* not __MINGW32__ */
  if ((num_written = write (sock->pipe_desc[WRITE], 
			    sock->send_buffer, 
			    do_write)) == -1)
    {
      log_printf (LOG_ERROR, "pipe: write: %s\n", SYS_ERROR);
      if (svz_errno == SOCK_UNAVAILABLE)
	{
	  sock->unavailable = time (NULL) + RELAX_FD_TIME;
	  num_written = 0;
	}
    }
#endif /* not __MINGW32__ */

  /* Some data has been successfully written to the pipe. */
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

/*
 * This routine creates a pipe connection socket structure to a named
 * pipe. Return NULL on errors.
 */
socket_t
pipe_connect (char *inpipe, char *outpipe)
{
  socket_t sock;
  HANDLE recv_pipe, send_pipe;
#ifndef __MINGW32__
  struct stat buf;
#endif
  
  /* create socket structure */
  if ((sock = sock_alloc ()) == NULL)
    {
      return NULL;
    }

#ifndef __MINGW32__
  sock->recv_pipe = svz_malloc (strlen (inpipe) + 1);
  strcpy (sock->recv_pipe, inpipe);
  sock->send_pipe = svz_malloc (strlen (outpipe) + 1);
  strcpy (sock->send_pipe, outpipe);
#else /* __MINGW32__ */
  sock->recv_pipe = svz_malloc (strlen (inpipe) + 10);
  sprintf (sock->recv_pipe, "\\\\.\\pipe\\%s", inpipe);
  sock->send_pipe = svz_malloc (strlen (outpipe) + 10);
  sprintf (sock->send_pipe, "\\\\.\\pipe\\%s", outpipe);
#endif /* __MINGW32__ */

#ifndef __MINGW32__
  /* is receive pipe such a ? */
  if (stat (sock->recv_pipe, &buf) == -1 || !S_ISFIFO (buf.st_mode))
    {
      log_printf (LOG_ERROR, "pipe: no such pipe: %s\n", sock->recv_pipe);
      sock_free (sock);
      return NULL;
    }

  /* is send pipe such a ? */
  if (stat (sock->send_pipe, &buf) == -1 || !S_ISFIFO (buf.st_mode))
    {
      log_printf (LOG_ERROR, "pipe: no such pipe: %s\n", sock->send_pipe);
      sock_free (sock);
      return NULL;
    }

  /* try opening receiving pipe for reading */
  if ((recv_pipe = open (sock->recv_pipe, O_RDONLY | O_NONBLOCK)) == -1)
    {
      log_printf (LOG_ERROR, "pipe: open: %s\n", SYS_ERROR);
      sock_free (sock);
      return NULL;
    }

  /* try opening sending pipe for writing */
  if ((send_pipe = open (sock->send_pipe, O_WRONLY | O_NONBLOCK)) == -1)
    {
      log_printf (LOG_ERROR, "pipe: open: %s\n", SYS_ERROR);
      close (recv_pipe);
      sock_free (sock);
      return NULL;
    }

  /* set send pipe to non-blocking mode */
  if (fcntl (send_pipe, F_SETFL, O_NONBLOCK) < 0)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", SYS_ERROR);
      close (recv_pipe);
      close (send_pipe);
      sock_free (sock);
      return NULL;
    }

#else /* __MINGW32__ */

  /* try opening receiving pipe */
  if ((recv_pipe = CreateFile (sock->recv_pipe, GENERIC_READ, 0,
			       NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 
			       NULL)) == INVALID_HANDLE_VALUE)
    {
      log_printf (LOG_ERROR, "pipe: CreateFile: %s\n", SYS_ERROR);
      sock_free (sock);
      return NULL;
    }

  /* try opening sending pipe */
  if ((send_pipe = CreateFile (sock->send_pipe, GENERIC_WRITE, 0,
			       NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 
			       NULL)) == INVALID_HANDLE_VALUE)
    {
      log_printf (LOG_ERROR, "pipe: CreateFile: %s\n", SYS_ERROR);
      DisconnectNamedPipe (recv_pipe);
      CloseHandle (recv_pipe);
      sock_free (sock);
      return NULL;
    }

  /* initialize the overlap structure on WinNT systems */
  if (svz_os_version >= WinNT4x)
    {
      sock->overlap[READ] = svz_malloc (sizeof (OVERLAPPED));
      memset (sock->overlap[READ], 0, sizeof (OVERLAPPED));
      sock->overlap[WRITE] = svz_malloc (sizeof (OVERLAPPED));
      memset (sock->overlap[WRITE], 0, sizeof (OVERLAPPED));
    }

#endif /* __MINGW32__ */

  /* modify socket structure and assign some callbacks */
  sock_unique_id (sock);
  sock->pipe_desc[READ] = recv_pipe;
  sock->pipe_desc[WRITE] = send_pipe;
  sock->flags |= (SOCK_FLAG_PIPE | SOCK_FLAG_CONNECTED);
  sock_enqueue (sock);

  sock->read_socket = pipe_read;
  sock->write_socket = pipe_write;

  sock_connections++;
  return sock;
}

/*
 * Create a socket structure for listening pipe sockets. Open the reading
 * end of such a connection. Return either zero or non-zero on errors.
 */
int
pipe_listener (socket_t server_sock)
{
#if HAVE_MKFIFO || HAVE_MKNOD

# ifdef HAVE_MKFIFO
#  define MKFIFO(path, mode) mkfifo (path, mode)
#  define MKFIFO_FUNC "mkfifo"
# else
#  define MKFIFO(path, mode) mknod (path, mode | S_IFIFO, 0)
#  define MKFIFO_FUNC "mknod"
# endif

  struct stat buf;
  mode_t mask;
#endif

#if defined (HAVE_MKFIFO) || defined (HAVE_MKNOD) || defined (__MINGW32__)
  HANDLE recv_pipe, send_pipe;

  /*
   * Pipe requested via port configuration ?
   */
  if (!server_sock->recv_pipe || !server_sock->send_pipe)
    return -1;
#endif

#if HAVE_MKFIFO || HAVE_MKNOD
  /* Save old permissions and set new. */
  mask = umask (0);

  /* 
   * Test if both of the named pipes have been created yet. 
   * If not then create them locally.
   */
  if (stat (server_sock->recv_pipe, &buf) == -1)
    {
      if (MKFIFO (server_sock->recv_pipe, 0666) != 0)
        {
          log_printf (LOG_ERROR, "pipe: " MKFIFO_FUNC ": %s\n", SYS_ERROR);
	  umask (mask);
          return -1;
        }
      if (stat (server_sock->recv_pipe, &buf) == -1 || !S_ISFIFO (buf.st_mode))
	{
          log_printf (LOG_ERROR, 
		      "pipe: stat: " MKFIFO_FUNC "() did not create a fifo\n");
	  umask (mask);
          return -1;
	}
    }

  if (stat (server_sock->send_pipe, &buf) == -1)
    {
      if (MKFIFO (server_sock->send_pipe, 0666) != 0)
        {
          log_printf (LOG_ERROR, "pipe: " MKFIFO_FUNC ": %s\n", SYS_ERROR);
	  umask (mask);
          return -1;
        }
      if (stat (server_sock->send_pipe, &buf) == -1 || !S_ISFIFO (buf.st_mode))
	{
          log_printf (LOG_ERROR, 
		      "pipe: stat: " MKFIFO_FUNC "() did not create a fifo\n");
	  umask (mask);
          return -1;
	}
    }

  /* reassign old umask permissions */
  umask (mask);

  /* Try opening the server's read pipe. Should always be possible. */
  if ((recv_pipe = open (server_sock->recv_pipe, O_NONBLOCK|O_RDONLY)) == -1)
    {
      log_printf (LOG_ERROR, "pipe: open: %s\n", SYS_ERROR);
      return -1;
    }
  /* Check if the file descriptor is a pipe. */
  if (fstat (recv_pipe, &buf) == -1 || !S_ISFIFO (buf.st_mode))
    {
      log_printf (LOG_ERROR, 
		  "pipe: fstat: " MKFIFO_FUNC "() did not create a fifo\n");
      close (recv_pipe);
      return -1;
    }
  server_sock->pipe_desc[READ] = recv_pipe;
  server_sock->flags |= SOCK_FLAG_RECV_PIPE;

#elif defined (__MINGW32__) /* not (HAVE_MKFIFO || HAVE_MKNOD) */

  /*
   * Create both of the named pipes and put the handles into
   * the server socket structure.
   */
  recv_pipe = CreateNamedPipe (
    server_sock->recv_pipe,                     /* path */
    PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, /* receive + overlapped */
    PIPE_READMODE_BYTE | PIPE_NOWAIT,           /* binary + non-blocking */
    1,                                          /* one instance only */
    0, 0,                                       /* default buffer sizes */
    100,                                        /* timeout in ms */
    NULL);                                      /* no security */

  if (recv_pipe == INVALID_HANDLE_VALUE || !recv_pipe)
    {
      log_printf (LOG_ERROR, "pipe: CreateNamedPipe: %s\n", SYS_ERROR);
      return -1;
    }
  server_sock->pipe_desc[READ] = recv_pipe;
  server_sock->flags |= SOCK_FLAG_RECV_PIPE;

  send_pipe = CreateNamedPipe (
    server_sock->send_pipe,                      /* path */
    PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED, /* send + overlapped */
    PIPE_TYPE_BYTE | PIPE_NOWAIT,                /* bin + non-blocking */
    1,                                           /* one instance only */
    0, 0,                                        /* default buffer sizes */
    100,                                         /* timeout in ms */
    NULL);                                       /* no security */
      
  if (send_pipe == INVALID_HANDLE_VALUE || !send_pipe)
    {
      log_printf (LOG_ERROR, "pipe: CreateNamedPipe: %s\n", SYS_ERROR);
      return -1;
    }
  server_sock->pipe_desc[WRITE] = send_pipe;
  server_sock->flags |= SOCK_FLAG_SEND_PIPE;

  /*
   * Initialize the overlapped structures for this server socket. Each
   * client connected gets it passed.
   */
  if (svz_os_version >= WinNT4x)
    {
      server_sock->overlap[READ] = svz_malloc (sizeof (OVERLAPPED));
      memset (server_sock->overlap[READ], 0, sizeof (OVERLAPPED));
      server_sock->overlap[WRITE] = svz_malloc (sizeof (OVERLAPPED));
      memset (server_sock->overlap[WRITE], 0, sizeof (OVERLAPPED));
    }

#else /* not __MINGW32__ */

  return -1;

#endif /* neither HAVE_MKFIFO nor __MINGW32__ */

#if defined (HAVE_MKFIFO) || defined (HAVE_MKNOD) || defined (__MINGW32__)

  return 0;

#endif /* HAVE_MKFIFO or __MINGW32__ */
}