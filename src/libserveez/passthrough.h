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
 * $Id: passthrough.h,v 1.2 2001/07/30 10:15:25 ela Exp $
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

__BEGIN_DECLS

SERVEEZ_API int svz_sock_process __P ((svz_socket_t *, 
				       char *, char **, char **));
SERVEEZ_API int svz_process_check_executable __P ((char *, char **));
SERVEEZ_API int svz_process_check_access __P ((char *));
SERVEEZ_API int svz_envblock_add __P ((svz_envblock_t *, char *, ...));
SERVEEZ_API svz_envblock_t *svz_envblock_create __P ((void));
SERVEEZ_API int svz_envblock_default __P ((svz_envblock_t *));
SERVEEZ_API int svz_envblock_free __P ((svz_envblock_t *));
SERVEEZ_API void svz_envblock_destroy __P ((svz_envblock_t *));
SERVEEZ_API svz_envp_t svz_envblock_get __P ((svz_envblock_t *));

__END_DECLS

#endif /* __PASSTHROUGH_H__ */
