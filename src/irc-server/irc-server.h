/*
 * irc-server.h - "Internet Relay Chat" server header definitions
 *
 * Copyright (C) 2000 Ela
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
 */

#ifndef __IRC_SERVER_H__
#define __IRC_SERVER_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE

int irc_parse_line(char *line, char *fmt, ...);
void irc_del_server(irc_server_t *server);
void irc_delete_servers(void);
void irc_connect_servers(void);
void irc_resolve_cline(irc_config_t *cfg);
irc_server_t *irc_add_server(irc_server_t *server);

#endif /* __IRC_SERVER_H__ */
