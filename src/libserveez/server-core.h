/*
 * server-core.h - server management definition
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
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
 * $Id: server-core.h,v 1.13 2001/08/12 10:59:04 ela Exp $
 *
 */

#ifndef __SERVER_CORE_H__
#define __SERVER_CORE_H__ 1

#include "libserveez/defines.h"
#include "libserveez/socket.h"
#include "libserveez/portcfg.h"

SERVEEZ_API extern int svz_nuke_happened;
SERVEEZ_API extern HANDLE svz_child_died;
SERVEEZ_API extern long svz_notify;

SERVEEZ_API extern svz_socket_t *svz_sock_root;
SERVEEZ_API extern svz_socket_t *svz_sock_last;

/*
 * Go through each socket structure in the chained list.
 */
#define svz_sock_foreach(sock) \
  for ((sock) = svz_sock_root; (sock) != NULL; (sock) = (sock)->next)

__BEGIN_DECLS

SERVEEZ_API void svz_sock_table_create __P ((void));
SERVEEZ_API void svz_sock_table_destroy __P ((void));
SERVEEZ_API svz_socket_t *svz_sock_find __P ((int, int));
SERVEEZ_API int svz_sock_schedule_for_shutdown __P ((svz_socket_t *));
SERVEEZ_API int svz_sock_enqueue __P ((svz_socket_t *));
SERVEEZ_API int svz_sock_dequeue __P ((svz_socket_t *));
SERVEEZ_API void svz_sock_shutdown_all __P ((void));
SERVEEZ_API void svz_sock_setparent __P ((svz_socket_t *, svz_socket_t *));
SERVEEZ_API svz_socket_t *svz_sock_getparent __P ((svz_socket_t *));
SERVEEZ_API void svz_sock_setreferrer __P ((svz_socket_t *, svz_socket_t *));
SERVEEZ_API svz_socket_t *svz_sock_getreferrer __P ((svz_socket_t *));
SERVEEZ_API svz_portcfg_t *svz_sock_portcfg __P ((svz_socket_t *));
SERVEEZ_API int svz_sock_check_access __P ((svz_socket_t *, svz_socket_t *));
SERVEEZ_API int svz_sock_check_frequency __P ((svz_socket_t *, 
					       svz_socket_t *));

SERVEEZ_API void svz_executable __P ((char *));
SERVEEZ_API void svz_sock_check_bogus __P ((void));
SERVEEZ_API int svz_periodic_tasks __P ((void));
SERVEEZ_API void svz_loop_pre __P ((void));
SERVEEZ_API void svz_loop_post __P ((void));
SERVEEZ_API void svz_loop __P ((void));
SERVEEZ_API void svz_loop_one __P ((void));
SERVEEZ_API void svz_signal_up __P ((void));
SERVEEZ_API void svz_signal_dn __P ((void));
SERVEEZ_API RETSIGTYPE svz_signal_handler __P ((int));
SERVEEZ_API void svz_strsignal_init __P ((void));
SERVEEZ_API void svz_strsignal_destroy __P ((void));
SERVEEZ_API char *svz_strsignal __P ((int));

__END_DECLS

#endif /* not __SERVER_CORE_H__ */
