/*
 * awcs-proto.h - aWCS protocol declarations
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
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
 * $Id: awcs-proto.h,v 1.4 2000/06/13 16:50:47 ela Exp $
 *
 */

#ifndef __AWCS_PROTO_H__
#define __AWCS_PROTO_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "socket.h"
#include "server.h"
#include "hash.h"

#define STATUS_CONNECT    0
#define STATUS_DISCONNECT 1
#define STATUS_KICK       2
#define STATUS_ALIVE      3
#define STATUS_NOTIFY     4
#define STATUS_NSLOOKUP   5
#define STATUS_IDENT      6

#define KICK_FLOODING 0
#define KICK_CRAWLING 1

#define MASTER_SEND_BUFSIZE (1024 * 256)
#define MASTER_RECV_BUFSIZE (1024 * 128)

#define MASTER_DETECTION 3
#define CLIENT_DETECTION 5
#define AWCS_MASTER "6 \0"
#define AWCS_CLIENT "aWCS\0"

/*
 * Local configuration of one instance of an aWCS server.
 */
typedef struct
{
  portcfg_t *netport; /* Network port configuration */
  portcfg_t *fsport;  /* Filesystem port configuration */
  socket_t server;    /* the current master server */
  int master;         /* Was Master server detected ? */
  hash_t *clients;    /* this aWCS servers user base */
}
awcs_config_t;

/*
 * The aWCS server definition. Exported to "server.h".
 */
extern server_definition_t awcs_server_definition;

/*
 * aWCS server initialization and finalization routines.
 */
int awcs_init (server_t *server);
int awcs_finalize (server_t *server);

/*
 * Exported aWCS server callbacks.
 */
int awcs_detect_proto (void *cfg, socket_t sock);
int awcs_connect_socket (void *cfg, socket_t sock);

/*
 * Local aWCS server callbacks.
 */
static void awcs_disconnect_clients (awcs_config_t *cfg);
int awcs_check_request (socket_t sock);
int awcs_disconnected_socket (socket_t sock);
int awcs_kicked_socket (socket_t sock, int reason);
int awcs_idle_func (socket_t sock);
int awcs_nslookup_done (socket_t sock, char *name);

#endif /* not __AWCS_PROTO_H__ */
