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
 * $Id: core.h,v 1.1 2001/03/02 21:12:53 ela Exp $
 *
 */

#ifndef __CORE_H__
#define __CORE_H__ 1

#include "libserveez/defines.h"

__BEGIN_DECLS

SERVEEZ_API int svz_fd_nonblock __P ((int fd));
SERVEEZ_API int svz_fd_cloexec __P ((int fd));

__END_DECLS

#endif /* not __SERVER_CORE_H__ */
