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
 * $Id: pipe-socket.h,v 1.11 2001/09/07 10:34:51 ela Exp $
 *
 */

#ifndef __PIPE_SOCKET_H__
#define __PIPE_SOCKET_H__ 1

#include "libserveez/defines.h"
#include "libserveez/socket.h"

#define READ           0      /* read pipe index */
#define WRITE          1      /* write pipe index */
#define PIPE_MAX_WRITE 0xFFFF /* maximum amount of write transfer bytes */
#define PIPE_MAX_READ  0xFFFF /* maximum amount of read transfer bytes */

/*
 * Definition of a named pipe.
 */
typedef struct svz_pipe
{
  char *name;        /* name of named pipe */
  unsigned int perm; /* user and group permissions */
  char *user;        /* user name */
  unsigned int uid;  /* user id (calculated from user name) */
  unsigned int pgid; /* primary group id */
  char *group;       /* group name */
  unsigned int gid;  /* group id (calculated from group name) */
}
svz_pipe_t;

__BEGIN_DECLS

SERVEEZ_API int svz_pipe_startup __P ((void));
SERVEEZ_API int svz_pipe_cleanup __P ((void));
SERVEEZ_API int svz_pipe_valid __P ((svz_socket_t *));
SERVEEZ_API int svz_pipe_read_socket __P ((svz_socket_t *));
SERVEEZ_API int svz_pipe_write_socket __P ((svz_socket_t *));
SERVEEZ_API int svz_pipe_disconnect __P ((svz_socket_t *));
SERVEEZ_API svz_socket_t *svz_pipe_create __P ((HANDLE, HANDLE));
SERVEEZ_API int svz_pipe_create_pair __P ((HANDLE pipe_desc[2]));
SERVEEZ_API svz_socket_t *svz_pipe_connect __P ((svz_pipe_t *, svz_pipe_t *));
SERVEEZ_API int svz_pipe_listener __P ((svz_socket_t *, 
					svz_pipe_t *, svz_pipe_t *));
SERVEEZ_API int svz_pipe_check_user __P ((svz_pipe_t *));
SERVEEZ_API int svz_pipe_check_group __P ((svz_pipe_t *));
SERVEEZ_API svz_pipe_t *svz_pipe_alloc __P ((void));
SERVEEZ_API void svz_pipe_destroy __P ((svz_pipe_t *));

__END_DECLS

#endif /* not __PIPE_SOCKET_H__ */
