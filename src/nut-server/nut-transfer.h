/*
 * nut-transfer.h - gnutella file transfer definitions
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
 * $Id: nut-transfer.h,v 1.3 2000/09/03 21:28:05 ela Exp $
 *
 */

#ifndef __NUT_TRANSFER_H__
#define __NUT_TRANSFER_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE

#include "socket.h"
#include "gnutella.h"

/* general definitions */
#define NUT_GET       "GET /get/"
#define NUT_AGENT     "User-Agent: Gnutella\r\n"
#define NUT_HTTP      "HTTP/1.0"
#define NUT_RANGE     "Content-range:"
#define NUT_LENGTH    "Content-length:"
#define NUT_GET_OK    "HTTP 200 OK\r\n"
#define NUT_SEPERATOR "\r\n\r\n"

/* gnutella transfer data structure */
typedef struct
{
  int original_size;
  int size;
}
nut_transfer_t;

int nut_init_transfer (socket_t, nut_reply_t *, nut_record_t *, char *);

#endif /* __NUT_TRANSFER_H__ */
