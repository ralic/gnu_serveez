/*
 * core.c - socket and file descriptor core implementations
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: core.c,v 1.1 2001/03/02 21:12:53 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#include "libserveez/util.h"

/*
 * Set the given file descriptor to nonblocking I/O. This heavily differs
 * in Win32 and Unix. The given file descriptor @var{fd} must be a socket
 * descriptor under Win32, otherwise the function fails. Return zero on
 * success, otherwise non-zero.
 */
int
svz_fd_nonblock (int fd)
{
#ifdef __MINGW32__
  unsigned long blockMode = 1;

  if (ioctlsocket (fd, FIONBIO, &blockMode) == SOCKET_ERROR)
    {
      log_printf (LOG_ERROR, "ioctlsocket: %s\n", NET_ERROR);
      return -1;
    }
#else /* not __MINGW32__ */
  int flag;

  flag = fcntl (fd, F_GETFL);
  if (fcntl (fd, F_SETFL, flag | O_NONBLOCK) < 0)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", NET_ERROR);
      return -1;
    }
#endif /* not __MINGW32__ */

  return 0;
}

/*
 * Set the close-on-exec flag of the given file descriptor @var{fd} and
 * return zero on success. Otherwise return non-zero.
 */
int
svz_fd_cloexec (int fd)
{
#ifndef __MINGW32__

  /* 
   * ... SNIP : from the cygwin mail archives 1999/05 ...
   * The problem is in socket() call on W95 - the socket returned 
   * is non-inheritable handle (unlike NT and Unixes, where
   * sockets are inheritable). To fix the problem DuplicateHandle 
   * call is used to create inheritable handle, and original 
   * handle is closed.
   * ... SNAP ...
   *
   * Thus here is NO NEED to set the FD_CLOEXEC flag and no
   * chance anyway.
   */

  int flag;

  flag = fcntl (fd, F_GETFD);
  if ((fcntl (fd, F_SETFD, flag | FD_CLOEXEC)) < 0)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", NET_ERROR);
      return -1;
    }

#endif /* !__MINGW32__ */

  return 0;
}
