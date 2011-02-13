/*
 * cfgfile.c - configuration file and left overs
 *
 * Copyright (C) 2011 Thien-Thi Nguyen
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package.  If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <string.h>

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#include "libserveez.h"
#include "cfgfile.h"

/*
 * Include headers of servers.
 */
#include "foo-server/foo-proto.h"
#if ENABLE_AWCS_PROTO
# include "awcs-server/awcs-proto.h"
#endif
#if ENABLE_HTTP_PROTO
# include "http-server/http-proto.h"
#endif
#if ENABLE_IRC_PROTO
# include "irc-server/irc-proto.h"
#endif
#if ENABLE_CONTROL_PROTO
# include "ctrl-server/control-proto.h"
#endif
#if ENABLE_SNTP_PROTO
# include "sntp-server/sntp-proto.h"
#endif
#if ENABLE_GNUTELLA
# include "nut-server/gnutella.h"
#endif
#if ENABLE_TUNNEL
# include "tunnel-server/tunnel.h"
#endif
#if ENABLE_FAKEIDENT
# include "fakeident-server/ident-proto.h"
#endif
#if ENABLE_PROG_SERVER
# include "prog-server/prog-server.h"
#endif

/*
 * Initialize all static server definitions.
 */
void
init_server_definitions (void)
{
  svz_servertype_add (&foo_server_definition);
#if ENABLE_AWCS_PROTO
  svz_servertype_add (&awcs_server_definition);
#endif
#if ENABLE_HTTP_PROTO
  svz_servertype_add (&http_server_definition);
#endif
#if ENABLE_IRC_PROTO
  svz_servertype_add (&irc_server_definition);
#endif
#if ENABLE_CONTROL_PROTO
  svz_servertype_add (&ctrl_server_definition);
#endif
#if ENABLE_SNTP_PROTO
  svz_servertype_add (&sntp_server_definition);
#endif
#if ENABLE_GNUTELLA
  svz_servertype_add (&nut_server_definition);
#endif
#if ENABLE_TUNNEL
  svz_servertype_add (&tnl_server_definition);
#endif
#if ENABLE_FAKEIDENT
  svz_servertype_add (&fakeident_server_definition);
#endif
#if ENABLE_PROG_SERVER
  svz_servertype_add (&prog_server_definition);
#endif
}
