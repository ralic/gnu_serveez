/*
 * core.h - socket and file descriptor declarations and definitions
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
 * $Id: core.h,v 1.8 2001/06/10 21:45:40 ela Exp $
 *
 */

#ifndef __CORE_H__
#define __CORE_H__ 1

#include "libserveez/defines.h"
#include "libserveez/array.h"

#include <stdio.h>
#include <sys/stat.h>

#ifndef __MINGW32__
# include <netinet/in.h>
#endif
#ifdef __MINGW32__
# include <winsock2.h>
#endif

#ifndef __MINGW32__
svz_array_t *svz_files;
#endif

/* protocol definitions */
#define PROTO_TCP   0x00000001 /* tcp  - bidirectional, reliable */
#define PROTO_UDP   0x00000002 /* udp  - multidirectional, unreliable */
#define PROTO_PIPE  0x00000004 /* pipe - unidirectional, reliable */
#define PROTO_ICMP  0x00000008 /* icmp - multidirectional, unreliable */
#define PROTO_RAW   0x00000010 /* raw  - multidirectional, unreliable */

__BEGIN_DECLS

SERVEEZ_API int svz_fd_nonblock __P ((int));
SERVEEZ_API int svz_fd_cloexec __P ((int));
SERVEEZ_API int svz_tcp_cork __P ((SOCKET, int));
SERVEEZ_API int svz_socket_connect __P ((SOCKET, unsigned long, 
					 unsigned short));
SERVEEZ_API SOCKET svz_socket_create __P ((int));
SERVEEZ_API char *svz_inet_ntoa __P ((unsigned long));
SERVEEZ_API int svz_inet_aton __P ((char *, struct sockaddr_in *));
SERVEEZ_API int svz_sendfile __P ((int, int, long int *, unsigned int));
SERVEEZ_API int svz_open __P ((const char *, int, unsigned int));
SERVEEZ_API int svz_close __P ((int));
SERVEEZ_API int svz_fstat __P ((int, struct stat *));
SERVEEZ_API FILE *svz_fopen __P ((const char *, const char *));
SERVEEZ_API int svz_fclose __P ((FILE *));

__END_DECLS

#endif /* not __CORE_H__ */
