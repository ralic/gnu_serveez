/*
 * irc-server.h - IRC server header definitions
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
 * $Id: irc-server.h,v 1.5 2000/07/15 11:44:17 ela Exp $
 *
 */

#ifndef __IRC_SERVER_H__
#define __IRC_SERVER_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE

/*
 * These functions are exported from this IRC server module.
 */
#ifndef __STDC__
int irc_parse_line ();
#else
int irc_parse_line (char *line, char *fmt, ...);
#endif
void irc_delete_servers (irc_config_t *cfg);
void irc_connect_servers (irc_config_t *cfg);

#endif /* __IRC_SERVER_H__ */
