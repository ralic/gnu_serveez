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
 * $Id: server-core.h,v 1.3 2001/02/28 21:51:19 raimi Exp $
 *
 */

#ifndef __SERVER_CORE_H__
#define __SERVER_CORE_H__ 1

#include <time.h>
#include "libserveez/defines.h"
#include "libserveez/socket.h"

SERVEEZ_API extern int server_nuke_happened;
SERVEEZ_API extern HANDLE server_child_died;
SERVEEZ_API extern time_t server_notify;

SERVEEZ_API extern socket_t sock_root;
SERVEEZ_API extern socket_t sock_last;

__BEGIN_DECLS

SERVEEZ_API socket_t sock_find __P ((int id, int version));
SERVEEZ_API int sock_schedule_for_shutdown __P ((socket_t sock));
SERVEEZ_API int sock_enqueue __P ((socket_t sock));
SERVEEZ_API int sock_dequeue __P ((socket_t sock));
SERVEEZ_API void sock_shutdown_all __P ((void));

SERVEEZ_API void server_check_bogus __P ((void));
SERVEEZ_API int server_periodic_tasks __P ((void));
SERVEEZ_API int server_loop __P ((void));
SERVEEZ_API void server_loop_one __P ((void));
SERVEEZ_API void server_signal_up __P ((void));
SERVEEZ_API void server_signal_dn __P ((void));
SERVEEZ_API RETSIGTYPE server_signal_handler __P ((int sig));

SERVEEZ_API int svz_fd_nonblock __P ((int fd));
SERVEEZ_API int svz_fd_cloexec __P ((int fd));

__END_DECLS

#endif /* not __SERVER_CORE_H__ */
