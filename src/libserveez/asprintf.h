/*
 * asprintf.h - (v)asprintf function interface
 *
 * Copyright (C) 2002 Andreas Rottmann <a.rottmann@gmx.at>
 * Copyright (C) 2002 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: asprintf.h,v 1.1 2002/12/05 16:57:56 ela Exp $
 *
 */

#ifndef __ASPRINTF_H__
#define __ASPRINTF_H__ 1

#include "libserveez/defines.h"

__BEGIN_DECLS

SERVEEZ_API int svz_asprintf __PARAMS ((char **, const char *, ...));
SERVEEZ_API int svz_vasprintf __PARAMS ((char **, const char *, va_list));

__END_DECLS

#endif /* not __ASPRINTF_H__ */
