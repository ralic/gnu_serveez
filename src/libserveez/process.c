/*
 * process.c - pass through socket connections to processes
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: process.c,v 1.1 2001/06/28 19:02:45 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <assert.h>

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#include "libserveez/socket.h"
#include "libserveez/process.h"

/*
 * This routine start a new program specified by @var{bin} passing the
 * socket descriptor in the socket structure @var{sock} to stdin and stdout.
 * The arguments and the environment of the new process can be passed by
 * @var{argv} and @var{envp}.
 */
int
svz_sock_process (svz_socket_t *sock, char *bin, char **argv, char **envp)
{
  /* TODO: */
  return 0;
}
