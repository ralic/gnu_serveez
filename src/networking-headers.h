/* networking-headers.h --- <netinet/in.h> et al
 *
 * Copyright (C) 2011 Thien-Thi Nguyen
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NETWORKING_HEADERS_H__
#define __NETWORKING_HEADERS_H__ 1

#ifdef __MINGW32__
# include <winsock2.h>
#else
# include <netinet/in.h>
#endif

#endif  /* !defined __NETWORKING_HEADERS_H__ */
