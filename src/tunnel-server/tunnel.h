/*
 * tunnel.h - port forward definition header
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
 * $Id: tunnel.h,v 1.1 2000/10/12 10:19:45 ela Exp $
 *
 */

#ifndef __TUNNEL_H__
#define __TUNNEL_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include "socket.h"
#include "hash.h"
#include "server.h"

/*
 * Tunnel server configuration structure.
 */
typedef struct
{
  portcfg_t *source;  /* the source port to forward from */
  portcfg_t *target;  /* target port to forward to */
  hash_t *src_client; /* the source client socket hash */
  hash_t *tgt_client; /* target client hash */
}
tnl_config_t;

/*
 * Basic server callback definitions.
 */
int tnl_init (server_t *server);
int tnl_global_init (void);
int tnl_finalize (server_t *server);
int tnl_global_finalize (void);

/*
 * This server's definition.
 */
extern server_definition_t tnl_server_definition;

#endif /* __TUNNEL_H__ */



