/*
 * tunnel.c - port forward implementations
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
 * $Id: tunnel.c,v 1.1 2000/10/12 10:19:45 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_TUNNEL

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "util.h"
#include "hash.h"
#include "alloc.h"
#include "server.h"
#include "tunnel.h"

tnl_config_t tnl_config = 
{
  NULL, /* the source port to forward from */
  NULL, /* target port to forward to */
  NULL, /* the source client socket hash */
  NULL  /* target client hash */
};

/*
 * Defining configuration file associations with key-value-pairs.
 */
key_value_pair_t tnl_config_prototype [] = 
{
  REGISTER_PORTCFG ("source", tnl_config.source, NOTDEFAULTABLE),
  REGISTER_PORTCFG ("target", tnl_config.target, NOTDEFAULTABLE),
  REGISTER_END ()
};

/*
 * Definition of this server.
 */
server_definition_t tnl_server_definition =
{
  "tunnel server",
  "tunnel",
  tnl_global_init,
  tnl_init,
  NULL,
  NULL,
  tnl_finalize,
  tnl_global_finalize,
  NULL,
  NULL,
  NULL,
  NULL,
  &tnl_config,
  sizeof (tnl_config),
  tnl_config_prototype
};

int
tnl_global_init (void)
{
  return 0;
}

int
tnl_global_finalize (void)
{
  return 0;
}

int
tnl_finalize (server_t *server)
{
  return 0;
}

int
tnl_init (server_t *server)
{
  return 0;
}

#else /* not ENABLE_TUNNEL */

int tunnel_dummy; /* Shut compiler warnings up. */

#endif /* not ENABLE_TUNNEL */



