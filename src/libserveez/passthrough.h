/*
 * passthrough.h - pass through declarations
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: passthrough.h,v 1.7 2001/11/21 21:37:42 ela Exp $
 *
 */

#ifndef __PASSTHROUGH_H__
#define __PASSTHROUGH_H__ 1

#define _GNU_SOURCE

#include "libserveez/defines.h"

#ifdef __MINGW32__
typedef char * svz_envp_t;
#else
typedef char ** svz_envp_t;
#endif

/* Structure containing a system independent environment. */
typedef struct
{
  int size;
  char **entry;
  char *block;
}
svz_envblock_t;

/* Internally used to pass lots of arguments. */
typedef struct
{
  svz_socket_t *sock;
  char *bin;
  char *dir;
  char **argv;
  svz_envblock_t *envp;
  char *user;
  char *app;
  HANDLE in, out;
}
svz_process_t;

/* Definition for the @var{flag} argument of @code{svz_sock_process()}. */
#define SVZ_PROCESS_FORK    1
#define SVZ_PROCESS_SHUFFLE 2

/* Definitions for the @var{user} argument of @code{svz_sock_process()}. */
#define SVZ_PROCESS_NONE  ((char *) 0L)
#define SVZ_PROCESS_OWNER ((char *) ~0L)

__BEGIN_DECLS

SERVEEZ_API int svz_sock_process __P ((svz_socket_t *, char *, char *, 
				       char **, svz_envblock_t *, int, 
				       char *));
SERVEEZ_API int svz_process_fork __P ((svz_process_t *));
SERVEEZ_API int svz_process_shuffle __P ((svz_process_t *));
SERVEEZ_API int svz_process_create_child __P ((svz_process_t *));
SERVEEZ_API int svz_process_send_pipe __P ((svz_socket_t *));
SERVEEZ_API int svz_process_recv_pipe __P ((svz_socket_t *));
SERVEEZ_API int svz_process_check_executable __P ((char *, char **));
SERVEEZ_API int svz_process_check_access __P ((char *, char *));
SERVEEZ_API int svz_envblock_add __P ((svz_envblock_t *, char *, ...));
SERVEEZ_API svz_envblock_t *svz_envblock_create __P ((void));
SERVEEZ_API int svz_envblock_default __P ((svz_envblock_t *));
SERVEEZ_API int svz_envblock_free __P ((svz_envblock_t *));
SERVEEZ_API void svz_envblock_destroy __P ((svz_envblock_t *));
SERVEEZ_API svz_envp_t svz_envblock_get __P ((svz_envblock_t *));

__END_DECLS

#endif /* __PASSTHROUGH_H__ */
