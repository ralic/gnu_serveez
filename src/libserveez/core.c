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
 * $Id: core.c,v 1.3 2001/03/08 11:53:56 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_NETINET_TCP_H
# include <netinet/tcp.h>
#endif
#if HAVE_SYS_SENDFILE_H
# include <sys/sendfile.h>
#endif
#if defined (HAVE_SYS_UIO_H) && defined (__FreeBSD__)
# include <sys/uio.h>
#endif

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#include "libserveez/util.h"
#include "libserveez/core.h"

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

/*
 * Create a new non-blocking socket which does not get inherited on 
 * @code{exec()}. The protocol is specified by @var{proto}. Return the
 * socket descriptor or -1 on errors.
 */
SOCKET
svz_socket_create (int proto)
{
  int stype;                 /* socket type (STREAM or DGRAM or RAW) */
  int ptype;                 /* protocol type (IP or UDP or ICMP) */
  SOCKET sockfd;

  /* Assign the appropriate socket type. */
  switch (proto)
    {
    case PROTO_TCP:
      stype = SOCK_STREAM;
      ptype = IPPROTO_IP;
      break;
    case PROTO_UDP:
      stype = SOCK_DGRAM;
      ptype = IPPROTO_UDP;
      break;
    case PROTO_ICMP:
      stype = SOCK_RAW;
      ptype = IPPROTO_ICMP;
      break;
      /* This protocol is for sending packets only. The kernel filters
	 any received packets by the socket protocol (here: IPPROTO_RAW
	 which is unspecified). */
    case PROTO_RAW:
      stype = SOCK_RAW;
      ptype = IPPROTO_RAW;
      break;
    default:
      stype = SOCK_STREAM;
      ptype = IPPROTO_IP;
      break;
    }

  /* Create a socket for communication with a server. */
  if ((sockfd = socket (AF_INET, stype, ptype)) == INVALID_SOCKET)
    {
      log_printf (LOG_ERROR, "socket: %s\n", NET_ERROR);
      return -1;
    }

  /* Make the socket non-blocking. */
  if (svz_fd_nonblock (sockfd) != 0)
    {
      closesocket (sockfd);
      return -1;
    }
  
  /* Do not inherit this socket. */
  if (svz_fd_cloexec (sockfd) != 0)
    {
      closesocket (sockfd);
      return -1;
    }

  return sockfd;
}

/*
 * Connect the given socket descriptor @var{sockfd} to the host @var{host}
 * at the network port @var{port}. Return non-zero on errors.
 */
int
svz_socket_connect (SOCKET sockfd, unsigned long host, unsigned short port)
{
  struct sockaddr_in server;
  int error;

  /* Setup the server address. */
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = host;
  server.sin_port = port;
  
  /* Try to connect to the server, */
  if (connect (sockfd, (struct sockaddr *) &server, sizeof (server)) == -1)
    {
#ifdef __MINGW32__
      error = WSAGetLastError ();
#else
      error = errno;
#endif
      if (error != SOCK_INPROGRESS && error != SOCK_UNAVAILABLE)
        {
          log_printf (LOG_ERROR, "connect: %s\n", NET_ERROR);
          closesocket (sockfd);
          return -1;
        }
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "connect: %s\n", NET_ERROR);
#endif
    }
  return 0;
}

/*
 * Converts the given ip address @var{ip} to the dotted decimal 
 * representation. The string is a statically allocated buffer, please 
 * copy the result. The given ip address MUST be in network byte order.
 */
char *
svz_inet_ntoa (unsigned long ip)
{
#if !BROKEN_INET_NTOA
  struct in_addr addr;

  addr.s_addr = ip;
  return inet_ntoa (addr);

#else /* BROKEN_INET_NTOA */

  static char addr[16];

  /* 
   * Now, this is strange: IP is given in host byte order. Nevertheless
   * conversion is endian-specific. To the binary AND and SHIFT operations
   * work differently on different architectures ?
   */
  sprintf (addr, "%lu.%lu.%lu.%lu",
#if WORDS_BIGENDIAN
	   (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
#else /* Little Endian */
	   ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
#endif
  return addr;

#endif /* BROKEN_INET_NTOA */
}

/*
 * Converts the Internet host address @var{str} from the standard 
 * numbers-and-dots notation into binary data and stores it in the 
 * structure that @var{addr} points to. @code{svz_inet_aton()} returns 
 * zero if the address is valid, nonzero if not.
 */
int
svz_inet_aton (char *str, struct sockaddr_in *addr)
{
#if HAVE_INET_ATON
  if (inet_aton (str, &addr->sin_addr) == 0)
    {
      return -1;
    }
#elif defined (__MINGW32__)
  int len = sizeof (struct sockaddr_in);
  if (WSAStringToAddress (str, AF_INET, NULL, 
                          (struct sockaddr *) addr, &len) != 0)
    {
      return -1;
    }
#else /* not HAVE_INET_ATON and not __MINGW32__ */
  addr->sin_addr.s_addr = inet_addr (str);
#endif /* not HAVE_INET_ATON */
  return 0;
}

/*
 * Enable or disable the @code{TCP_CORK} socket option of the given socket
 * descriptor @var{fd}. This is useful for performance reasons when using 
 * @code{sendfile()} with any prepending or trailing data not inside the 
 * file to transmit. The function return zero on success, otherwise non-zero.
 */
int
svz_tcp_cork (SOCKET fd, int set)
{
#ifdef TCP_CORK
  int flags;

  /* get current socket options */
  if ((flags = fcntl (fd, F_GETFL)) < 0)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", NET_ERROR);
      return -1;
    }

  /* set or unset the cork option */
  flags = set ? flags | TCP_CORK : flags & ~TCP_CORK;

  /* apply new socket option */
  if (fcntl (fd, F_SETFL, flags) < 0)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", NET_ERROR);
      return -1;
    }

#endif /* TCP_CORK */
  return 0;
}

#if HAVE_SENDFILE
/*
 * This function transmits data between one file descriptor and another 
 * where @var{in_fd} is the source and @var{out_fd} the destination. The
 * @var{offset} argument is a pointer to a variable holding the input file 
 * pointer position from which reading starts. When this routine returns,
 * the @var{offset} variable will be set to the offset of the byte following 
 * the last byte that was read. @var{count} is the number of bytes to copy 
 * between file descriptors. Returns the number of bytes actually 
 * read/written or -1 on errors.
 */
int
svz_sendfile (int out_fd, int in_fd, off_t *offset, size_t count)
{
  int ret;
#ifdef __FreeBSD__
  off_t sbytes;
  ret = sendfile (in_fd, out_fd, *offset, count, NULL, &sbytes, 0);
  *offset += sbytes;
  ret = ret ? -1 : (int) sbytes;
#else
  ret = sendfile (out_fd, in_fd, offset, count);
#endif
  return ret;
}
#endif /* HAVE_SEND_FILE */
