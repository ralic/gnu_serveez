/*
 * nut-core.h - gnutella core definitions
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
 * $Id: nut-core.h,v 1.1 2000/09/03 21:28:05 ela Exp $
 *
 */

#ifndef __NUT_CORE_H__
#define __NUT_CORE_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE

/*
 * Because the gnutella protocol is a binary protocol we need to convert
 * packets to structures and backwards.
 */

nut_header_t * nut_get_header (byte *data);
void nut_put_header (nut_header_t *hdr, byte *data);
nut_ping_reply_t * nut_get_ping_reply (byte *data);
void nut_put_ping_reply (nut_ping_reply_t *reply, byte *data);
nut_query_t * nut_get_query (byte *data);
void nut_put_query (nut_query_t *query, byte *data);
nut_record_t * nut_get_record (byte *data);
void nut_put_record (nut_record_t *record, byte *data);
nut_reply_t * nut_get_reply (byte *data);
void nut_put_reply (nut_reply_t *reply, byte *data);
nut_push_t * nut_get_push (byte *data);
void nut_put_push (nut_push_t *push, byte *data);

#endif /* __NUT_CORE_H__ */
