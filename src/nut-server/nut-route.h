/*
 * nut-route.h - gnutella routing definitions
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: nut-route.h,v 1.1 2000/09/02 15:48:14 ela Exp $
 *
 */

#ifndef __NUT_ROUTE_H__
#define __NUT_ROUTE_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

int nut_route (socket_t sock, nut_header_t *hdr, void *packet);


#endif /* __NUT_ROUTE_H__ */
