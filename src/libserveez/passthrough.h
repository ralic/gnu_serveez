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
 * $Id: passthrough.h,v 1.9 2001/12/10 22:01:13 ela Exp $
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
  int size;     /* Number of environment entries. */
  char **entry; /* Environment entries in the format "VAR=VALUE". */
  char *block;  /* Temporary environment block. */
}
svz_envblock_t;

/* Internally used to pass lots of arguments. */
typedef struct
{
  svz_socket_t *sock;   /* Socket structure to pass through. */
  char *bin;            /* Fully qualified program name. */
  char *dir;            /* Working directory. */
  char **argv;          /* Program arguments including argv[0]. */
  svz_envblock_t *envp; /* Environment block. */
  char *user;           /* User and group. */
  char *app;            /* Additional @var{bin} interpreter application. */
  HANDLE in, out;       /* New stdin and stdout of child process. */
  int flag;             /* Passthrough method flag. */
}
svz_process_t;

/* Definition for the @var{flag} argument of @code{svz_sock_process()}. */
#define SVZ_PROCESS_FORK         1
#define SVZ_PROCESS_SHUFFLE_SOCK 2
#define SVZ_PROCESS_SHUFFLE_PIPE 3

/* Definitions for the @var{user} argument of @code{svz_sock_process()}. */
#define SVZ_PROCESS_NONE  ((char *) 0L)
#define SVZ_PROCESS_OWNER ((char *) ~0L)

/* 
 * This macro must be called once after @code{svz_boot()} for setting up the
 * @code{svz_environ} variable. It simply passes the @code{environ} variable
 * of the calling application to the underlying Serveez core API. This is
 * necessary to make the @code{svz_envblock_default()} function working 
 * correctly.
 */
#define svz_envblock_setup() do { svz_environ = environ; } while (0)

__BEGIN_DECLS

SERVEEZ_API int svz_sock_process __P ((svz_socket_t *, char *, char *, 
				       char **, svz_envblock_t *, int, 
				       char *));

SERVEEZ_API int svz_process_disconnect __P ((svz_socket_t *));
SERVEEZ_API int svz_process_disconnect_passthrough __P ((svz_socket_t *));
SERVEEZ_API int svz_process_check_request __P ((svz_socket_t *));
SERVEEZ_API int svz_process_idle __P ((svz_socket_t *));
SERVEEZ_API int svz_process_send_pipe __P ((svz_socket_t *));
SERVEEZ_API int svz_process_recv_pipe __P ((svz_socket_t *));
SERVEEZ_API int svz_process_send_socket __P ((svz_socket_t *));
SERVEEZ_API int svz_process_recv_socket __P ((svz_socket_t *));

SERVEEZ_API int svz_process_create_child __P ((svz_process_t *));
SERVEEZ_API int svz_process_shuffle __P ((svz_process_t *));
SERVEEZ_API int svz_process_fork __P ((svz_process_t *));

SERVEEZ_API int svz_process_check_executable __P ((char *, char **));
SERVEEZ_API int svz_process_split_usergroup __P ((char *, char **, char **));
SERVEEZ_API int svz_process_check_access __P ((char *, char *));

SERVEEZ_API char **svz_environ;
SERVEEZ_API svz_envblock_t *svz_envblock_create __P ((void));
SERVEEZ_API int svz_envblock_default __P ((svz_envblock_t *));
SERVEEZ_API int svz_envblock_add __P ((svz_envblock_t *, char *, ...));
SERVEEZ_API int svz_envblock_free __P ((svz_envblock_t *));
SERVEEZ_API void svz_envblock_destroy __P ((svz_envblock_t *));
SERVEEZ_API svz_envp_t svz_envblock_get __P ((svz_envblock_t *));

__END_DECLS

#endif /* __PASSTHROUGH_H__ */
