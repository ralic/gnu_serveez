/*
 * foo-proto.h - example server header
 *
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
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
 * $Id: foo-proto.h,v 1.5 2000/08/02 09:45:14 ela Exp $
 *
 */

#ifndef __FOO_PROTO_H__
#define __FOO_PROTO_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include "hash.h"
#include "server.h"
#include "socket.h"

/*
 * Protocol server specific configuration.
 */
struct foo_config
{
  int dummy;
  char **messages;
  char *reply;
  int *ports;
  int bar;
  struct portcfg *port;
  hash_t **assoc;
};

/*
 * Basic server callback definitions.
 */
int foo_detect_proto (void *cfg, socket_t sock);
int foo_connect_socket (void *cfg, socket_t sock);
int foo_init (struct server *server);
int foo_global_init (void);
int foo_finalize (struct server *server);
int foo_global_finalize (void);
char *foo_info_server (struct server *server);

/*
 * This server's definition.
 */
extern struct server_definition foo_server_definition;

#endif /* __FOO_PROTO_H__ */
