/*
 * dns.h - DNS lookup coserver header definitions
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
 * $Id: dns.h,v 1.1 2001/01/28 13:24:38 ela Exp $
 *
 */

#ifndef __DNS_H__
#define __DNS_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE

/*
 * Proceed a single DNS lookup.
 */
char * dns_handle_request (char *inbuf);

#endif /* __DNS_H__ */
