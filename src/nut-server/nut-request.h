/*
 * nut-request.h - gnutella requests header file
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
 * $Id: nut-request.h,v 1.2 2001/01/28 03:26:55 ela Exp $
 *
 */

#ifndef __NUT_REQUEST_H__
#define __NUT_REQUEST_H__ 1

#if HAVE_CONFIG_H
# include <config.h>
#endif

/* Exported functions. */
int nut_reply (socket_t sock, nut_header_t *hdr, byte *packet);
int nut_push_request (socket_t sock, nut_header_t *hdr, byte *packet);
int nut_query (socket_t sock, nut_header_t *hdr, byte *packet);
int nut_pong (socket_t sock, nut_header_t *hdr, byte *packet);
int nut_ping (socket_t sock, nut_header_t *hdr, byte *null);

#endif /* not __NUT_REQUEST_H__ */
