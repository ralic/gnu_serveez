/*
 * interfaces.h - network interface definitions
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
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
 * $Id: interface.h,v 1.1 2001/02/02 11:30:34 ela Exp $
 *
 */

#ifndef __INTERFACE_H__
#define __INTERFACE_H__ 1

#include "libserveez/defines.h"

/*
 * Structure for collecting IP interfaces.
 */
typedef struct
{
  unsigned long index;  /* interface index */
  char *description;    /* interface description */
  unsigned long ipaddr; /* its IP address */
}
ifc_entry_t;

__BEGIN_DECLS

SERVEEZ_API extern int svz_interfaces;
SERVEEZ_API extern ifc_entry_t *svz_interface;

/* Export these functions. */
SERVEEZ_API void svz_interface_list __P ((void));
SERVEEZ_API void svz_interface_collect __P ((void));
SERVEEZ_API int svz_interface_free __P ((void));
SERVEEZ_API int svz_interface_add __P ((int, char *, unsigned long));

__END_DECLS

#endif /* not __INTERFACE_H__ */
