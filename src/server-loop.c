/*
 * server-loop.c - server loop implementation
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
 * $Id: server-loop.c,v 1.1 2000/08/16 01:06:11 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <sys/types.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <errno.h>
#include <string.h>
#if HAVE_SYS_POLL_H
# include <sys/poll.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "alloc.h"
#include "util.h"
#include "socket.h"
#include "pipe-socket.h"
#include "server-core.h"
#include "server-loop.h"

/*
 * Check the server and client sockets for incoming connections 
 * and data, and process outgoing data.
 */
int
check_sockets_select (void)
{
  int nfds;			/* count of file descriptors to check */
  fd_set read_fds;		/* bitmasks for file descriptors to check */
  fd_set write_fds;		/* dito */
  fd_set except_fds;		/* dito */
  struct timeval wait;		/* used for timeout in select() */
  socket_t sock;

  /*
   * Prepare the file handle sets for the select() call below.
   */
  FD_ZERO (&read_fds);
  FD_ZERO (&except_fds);
  FD_ZERO (&write_fds);
  nfds = 0;

  /*
   * Here we set the bitmaps for all clients we handle.
   */
  for (sock = socket_root; sock; sock = sock->next)
    {
      /* Put only those SOCKs into fd set not yet killed and skip files. */
      if (sock->flags & SOCK_FLAG_KILLED)
	continue;


      /* If socket is a file descriptor, then read it here. */
      if (sock->flags & SOCK_FLAG_FILE)
	{
	  if (sock->read_socket)
	    if (sock->read_socket (sock))
	      sock_schedule_for_shutdown (sock);
	}

#ifndef __MINGW32__
      /* Handle pipes. */
      if (sock->flags & SOCK_FLAG_PIPE)
	{
	  /* Do not handle listening pipes here. */
	  if (sock->flags & SOCK_FLAG_LISTENING)
	    continue;

	  /* 
	   * Put a pipe's descriptor into WRITE and EXCEPT set. Do this 
	   * if it is Unix not Win32.
	   */
	  if (sock->flags & SOCK_FLAG_SEND_PIPE)
	    {
	      FD_SET (sock->pipe_desc[WRITE], &except_fds);
	      if (sock->pipe_desc[WRITE] > nfds)
		nfds = sock->pipe_desc[WRITE];

	      if (sock->send_buffer_fill > 0)
		FD_SET (sock->pipe_desc[WRITE], &write_fds);
	    }

	  /* 
	   * Put a pipe's descriptor into READ set for getting data.
	   * Do this if it is Unix not Win32.
	   */
	  if (sock->flags & SOCK_FLAG_RECV_PIPE)
	    {
	      FD_SET (sock->pipe_desc[READ], &except_fds);
	      FD_SET (sock->pipe_desc[READ], &read_fds);
	      if (sock->pipe_desc[READ] > nfds)
		nfds = sock->pipe_desc[READ];
	    }
	}

#endif /* not __MINGW32__ */

      if (sock->flags & SOCK_FLAG_SOCK)
	{
	  /* Is the socket descriptor currently unavailable ? */
	  if (sock->unavailable)
	    {
	      if (time (NULL) >= sock->unavailable)
		sock->unavailable = 0;
	    }

	  /* Put every client's socket into EXCEPT and READ. */
	  FD_SET (sock->sock_desc, &except_fds);
	  FD_SET (sock->sock_desc, &read_fds);
	  if (sock->sock_desc > (SOCKET)nfds)
	    nfds = sock->sock_desc;

	  /* Put a socket into WRITE if necessary and possible. */
	  if (!sock->unavailable)
	    {
	      if (sock->send_buffer_fill > 0)
		FD_SET (sock->sock_desc, &write_fds);
	    }
	}
    }
  
  nfds++;

  /*
   * Adjust timeout value, so we won't wait longer than we want.
   */
  wait.tv_sec = next_notify_time - time (NULL);
  if (wait.tv_sec < 0) wait.tv_sec = 0;
  wait.tv_usec = 0;

  if ((nfds = select (nfds, &read_fds, &write_fds, &except_fds, &wait)) <= 0)
    {
      if (nfds < 0)
	{
	  log_printf (LOG_ERROR, "select: %s\n", NET_ERROR);
#ifdef __MINGW32__
	  /* FIXME: What value do we choose here ? */
	  if (last_errno != 0)
#else
	  if (errno == EBADF)
#endif
	    check_bogus_sockets ();
	  return -1;
	}
      else 
	{
	  /*
	   * select() timed out, so we can do some administrative stuff.
	   */
	  handle_periodic_tasks ();
	}
    }

  /* 
   * Go through all enqueued SOCKs and check if these have been 
   * select()ed or could be handle in any other way.
   */
  for (sock = socket_root; sock; sock = sock->next)
    {
      if (sock->flags & SOCK_FLAG_KILLED)
	continue;

      /* Handle pipes. Different in Win32 and Unices. */
      else if (sock->flags & SOCK_FLAG_PIPE)
	{
	  /* Make listening pipe servers listen. */
	  if (sock->flags & SOCK_FLAG_LISTENING)
	    {
	      if (!(sock->flags & SOCK_FLAG_INITED))
		if (sock->read_socket)
		  if (sock->read_socket (sock))
		    sock_schedule_for_shutdown (sock);
	      continue;
	    }

	  /* Handle receiving pipes. */
	  if (sock->flags & SOCK_FLAG_RECV_PIPE)
	    {
#ifndef __MINGW32__
	      if (FD_ISSET (sock->pipe_desc[READ], &except_fds))
		{
		  log_printf (LOG_ERROR, "exception on receiving pipe %d \n",
			      sock->pipe_desc[READ]);
		  sock_schedule_for_shutdown (sock);
		}
	      if (FD_ISSET (sock->pipe_desc[READ], &read_fds))
#endif /* not __MINGW32__ */
		{
		  if (sock->read_socket)
		    if (sock->read_socket (sock))
		      sock_schedule_for_shutdown (sock);
		}
	    }

	  /* Handle sending pipes. */
	  if (sock->flags & SOCK_FLAG_SEND_PIPE)
	    {
#ifndef __MINGW32__
	      if (FD_ISSET (sock->pipe_desc[WRITE], &except_fds))
		{
		  log_printf (LOG_ERROR, "exception on sending pipe %d \n",
			      sock->pipe_desc[WRITE]);
		  sock_schedule_for_shutdown (sock);
		}
	      if (FD_ISSET (sock->pipe_desc[WRITE], &write_fds))
#endif /* not __MINGW32__ */
		{
		  if (sock->write_socket)
		    if (sock->write_socket (sock))
		      sock_schedule_for_shutdown (sock);
		}
	    }
	}

      /* Handle usual sockets. Socket in the exception set ? */
      if (sock->flags & SOCK_FLAG_SOCK)
	{
	  if (FD_ISSET (sock->sock_desc, &except_fds))
	    {
	      log_printf (LOG_ERROR, "exception on socket %d\n",
			  sock->sock_desc);
	      sock_schedule_for_shutdown (sock);
	      continue;
	    }
	  
	  /* Is socket readable ? */
	  if (FD_ISSET (sock->sock_desc, &read_fds))
	    {
	      if (sock->read_socket)
		if (sock->read_socket (sock))
		  {
		    sock_schedule_for_shutdown (sock);
		    continue;
		  }
	    }
	  
	  /* Is socket writeable ? */
	  if (FD_ISSET (sock->sock_desc, &write_fds))
	    {
	      if (sock->write_socket)
		if (sock->write_socket(sock))
		  {
		    sock_schedule_for_shutdown (sock);
		    continue;
		  }
	    }
	}
    }

  /*
   * We had no chance to time out so we have to explicitly call the
   * timeout handler.
   */
  if (time (NULL) > next_notify_time)
    {
      handle_periodic_tasks ();
    }
  
  return 0;
}

#if HAVE_POLL /* configure'd */

/* re-allocate static buffers if neccessary */
#define FD_EXPAND()                                             \
  if (nfds >= max_nfds) {                                       \
    max_nfds++;                                                 \
    ufds = xprealloc (ufds, sizeof (struct pollfd) * max_nfds); \
    memset (&ufds[max_nfds-1], 0, sizeof (struct pollfd));      \
    sfds = xprealloc (sfds, sizeof (socket_t) * max_nfds);      \
    memset (&sfds[max_nfds-1], 0, sizeof (socket_t));           \
  }                                                             \

/* check for incoming data */
#define FD_POLL_IN(fd, sock)                 \
  {                                          \
    FD_EXPAND();                             \
    ufds[nfds].fd = fd;                      \
    ufds[nfds].events |= (POLLIN | POLLPRI); \
    sfds[nfds] = sock;                       \
  }                                          \

/* process outgoing data */
#define FD_POLL_OUT(fd, sock)     \
  {                               \
    FD_EXPAND();                  \
    ufds[nfds].fd = fd;           \
    ufds[nfds].events |= POLLOUT; \
    sfds[nfds] = sock;            \
  }                               \

/* clear polling buffers */
#define FD_POLL_CLR(ufds, sfds)                          \
  if (ufds) {                                            \
    memset (ufds, 0, sizeof (struct pollfd) * max_nfds); \
    memset (sfds, 0, sizeof (socket_t) * max_nfds);      \
  }                                                      \

/*
 * Same routine as the above check_sockets_select() routine. Here we are 
 * using poll() instead of select(). This might solve the builtin file 
 * descriptor limit of the (g)libc select(). This routine will NOT be
 * available under Win32.
 */
int
check_sockets_poll (void)
{
  static unsigned int max_nfds = 0;   /* maximum number of file descriptors */
  unsigned int nfds, fd;              /* number of fds */
  static struct pollfd *ufds = NULL;  /* poll fd array */
  static socket_t *sfds = NULL;       /* refering socket structures */
  int timeout;                        /* timeout in milliseconds */
  int polled;                         /* amount of polled fds */
  socket_t sock;                      /* socket structure */

  /* clear polling structures */
  nfds = 0;
  FD_POLL_CLR (ufds, sfds);

  /* go through all sockets */
  for (sock = socket_root; sock; sock = sock->next)
    {
      /* skip already killed sockets */
      if (sock->flags & SOCK_FLAG_KILLED)
	continue;

      /* process files */
      if (sock->flags & SOCK_FLAG_FILE)
	{
	  if (sock->read_socket)
	    if (sock->read_socket (sock))
	      sock_schedule_for_shutdown (sock);
	}

      /* process pipes */
      if (sock->flags & SOCK_FLAG_PIPE)
	{
	  if (sock->flags & SOCK_FLAG_LISTENING)
	    {
	      /* check for named pipe connection */
	      if (!(sock->flags & SOCK_FLAG_INITED))
		if (sock->read_socket)
		  if (sock->read_socket (sock))
		    sock_schedule_for_shutdown (sock);
	      continue;
	    }

	  /* send pipe ? */
	  if (sock->flags & SOCK_FLAG_SEND_PIPE)
	    {
	      if (sock->send_buffer_fill > 0)
		{
		  fd = sock->pipe_desc[WRITE];
		  FD_POLL_OUT (fd, sock);
		  nfds++;
		}
	    }

	  /* receive pipe ? */
	  if (sock->flags & SOCK_FLAG_RECV_PIPE)
	    {
	      fd = sock->pipe_desc[READ];
	      FD_POLL_IN (fd, sock);
	      nfds++;
	    }
	}

      /* normal bi-directional socket connection */
      if (sock->flags & SOCK_FLAG_SOCK)
	{
	  /* process lingering connection counter */
	  if (sock->unavailable)
	    {
	      if (time (NULL) >= sock->unavailable)
		sock->unavailable = 0;
	    }

	  /* poll this socket for reading and writing */
	  fd = sock->sock_desc;
	  FD_POLL_IN (fd, sock);
	  if (!sock->unavailable)
	    {
	      if (sock->send_buffer_fill > 0)
		FD_POLL_OUT (fd, sock);
	    }
	  nfds++;
	}
    }
  
  /* calculate timeout value */
  timeout = (next_notify_time - time (NULL)) * 1000;
  if (timeout < 0) timeout = 0;

  /* now poll() everything */
  if ((polled = poll (ufds, nfds, timeout)) <= 0)
    {
      if (polled < 0)
	{
	  log_printf (LOG_ERROR, "poll: %s\n", NET_ERROR);
	  if (errno == EBADF)
	    check_bogus_sockets ();
	  return -1;
	}
      else 
	{
	  handle_periodic_tasks ();
	}
    }

  /* go through all poll()ed structures */
  for (fd = 0; fd < nfds && polled != 0; fd++)
    {
      sock = sfds[fd];
      /* do not process killed connections */
      if (sock->flags & SOCK_FLAG_KILLED)
	continue;

      /* file descriptor ready for reading ? */
      if (ufds[fd].revents & (POLLIN | POLLPRI))
	{
	  polled--;
	  if (sock->read_socket)
	    if (sock->read_socket (sock))
	      {
		sock_schedule_for_shutdown (sock);
		continue;
	      }
	}
      
      /* file descriptor ready for writing */
      if (ufds[fd].revents & POLLOUT)
	{
	  polled--;
	  if (sock->write_socket)
	    if (sock->write_socket (sock))
	      {
		sock_schedule_for_shutdown (sock);
		continue;
	      }
	}

      /* file descriptor caused some error */
      if (ufds[fd].revents & (POLLERR | POLLHUP | POLLNVAL))
	{
	  polled--;
	  if (sock->flags & SOCK_FLAG_SOCK)
	    {
	      log_printf (LOG_ERROR, "exception on socket %d\n",
			  sock->sock_desc);
	      sock_schedule_for_shutdown (sock);
	    }
	  if (sock->flags & SOCK_FLAG_RECV_PIPE)
	    {
	      log_printf (LOG_ERROR, "exception on receiving pipe %d \n",
			  sock->pipe_desc[READ]);
	      sock_schedule_for_shutdown (sock);
	    }
	  if (sock->flags & SOCK_FLAG_SEND_PIPE)
	    {
	      log_printf (LOG_ERROR, "exception on sending pipe %d \n",
			  sock->pipe_desc[WRITE]);
	      sock_schedule_for_shutdown (sock);
	    }
	}
    }
  
  /* handle regular tasks ... */
  if (time (NULL) > next_notify_time)
    {
      handle_periodic_tasks ();
    }
  
  return 0;
}

#endif /* HAVE_POLL */
