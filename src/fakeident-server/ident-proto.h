/*
 * ident-proto.h - fake ident server
 *
 * Copyright (C) 2001 Raimund Jacob <raimi@lkcc.org>
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
 * $Id: ident-proto.h,v 1.1 2001/01/04 22:34:28 raimi Exp $
 *
 */

#ifndef __IDENT_PROTO_H__
#define __IDENT_PROTO_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include "server.h"
#include "socket.h"


/*
 * Configuration of this server
 */
struct fakeident_config
{
  char *systemtype;      /* the system type in the response, 'UNIX' */
  char *username;        /* the username responded for all requests */
  struct portcfg *port;  /* port the server is bound to             */  
};


/*
 * This server's definition
 */
extern struct server_definition fakeident_server_definition;

#endif /* __IDENT_PROTO_H__ */
