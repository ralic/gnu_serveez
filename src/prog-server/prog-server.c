/*
 * prog-server.c - passthrough server implementation
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2001 Raimund Jacob <raimi@lkcc.org>
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
 * $Id: prog-server.c,v 1.2 2001/11/21 14:15:45 raimi Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_PROG_SERVER

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "libserveez.h"
#include "prog-server.h"

/*
 * Default configuration definition.
 */
prog_config_t prog_config = 
{
  NULL,
  NULL,
  NULL,
  NULL,
  1
};

/*
 * Defining configuration file associations by key-value-pairs.
 */
svz_key_value_pair_t prog_config_prototype [] = 
{
  SVZ_REGISTER_STR ("binary", prog_config.bin, SVZ_ITEM_NOTDEFAULTABLE),
  SVZ_REGISTER_STR ("directory", prog_config.dir, SVZ_ITEM_DEFAULTABLE),
  SVZ_REGISTER_STR ("user", prog_config.user, SVZ_ITEM_DEFAULTABLE),
  SVZ_REGISTER_STRARRAY ("argv", prog_config.argv, SVZ_ITEM_DEFAULTABLE),
  SVZ_REGISTER_BOOL ("do-fork", prog_config.fork, SVZ_ITEM_DEFAULTABLE),
  SVZ_REGISTER_END ()
};

/*
 * Definition of this server.
 */
svz_servertype_t prog_server_definition =
{
  "program passthrough server",
  "prog",
  prog_global_init,
  prog_init,
  prog_detect_proto,
  prog_connect_socket,
  prog_finalize,
  prog_global_finalize,
  prog_info_client,
  prog_info_server,
  prog_notify,
  prog_handle_request,
  SVZ_DEFINE_CONFIG (prog_config, prog_config_prototype)
};

int 
prog_handle_request (svz_socket_t *sock, char *request, int len)
{
  prog_config_t *cfg = sock->cfg;

  return 0;
}

int
prog_check_request (svz_socket_t *sock)
{
  return 0;
}

int
prog_detect_proto (svz_server_t *server, svz_socket_t *sock)
{
  /* prevent anything from being read from socket */
  sock->read_socket = NULL;

  return -1;
}

int
prog_connect_socket (svz_server_t *server, svz_socket_t *sock)
{
  prog_config_t *cfg = server->cfg;
  char **argv = (char**) svz_array_values (cfg->argv);

  sock->check_request = prog_check_request;
  if (svz_sock_process (sock, cfg->bin, cfg->dir, argv, NULL,
			cfg->fork ? SVZ_PROCESS_FORK :
			SVZ_PROCESS_SHUFFLE) < 0 )
    {
      svz_log (LOG_ERROR, "prog: Cannot execute %s\n", cfg->bin);
      svz_free (argv);
      return -1;
    }

  svz_free (argv);
  if (cfg->fork)
    {
      /* Just close() this end of socket, not shutdown().
       * fork() makes the socket available in the child process. When we
       * shutdown() it here, it dies in the child, too. When we just close()
       * it, it still works in the child.
       */ 
      sock->flags |= SOCK_FLAG_NOSHUTDOWN;
      return -1;
    }
  else
    {
      /* re-enable reading of socket, FIXME: tcp? */
      sock->read_socket = svz_tcp_read_socket;
      /* FIXME: take care of failing child */
    }
  return 0;
}

int
prog_global_init (svz_servertype_t *server)
{
  return 0;
}

int
prog_global_finalize (svz_servertype_t *server)
{
  return 0;
}

int
prog_finalize (svz_server_t *server)
{
  prog_config_t *cfg = server->cfg;

  return 0;
}

int
prog_init (svz_server_t *server)
{
  prog_config_t *cfg = server->cfg;

  if (cfg->argv == NULL)
    {
      cfg->argv = svz_array_create(1);
      svz_array_add (cfg->argv, NULL);
    }

  return 0;
}

int
prog_notify (svz_server_t *server)
{
  prog_config_t *cfg = server->cfg;

  return 0;
}

char *
prog_info_client (svz_server_t *server, svz_socket_t *sock)
{
  prog_config_t *cfg = server->cfg;

  return NULL;
}

char *
prog_info_server (svz_server_t *server)
{
  prog_config_t *cfg = server->cfg;

  return NULL;
}

#else /* not ENABLE_PROG_SERVER */

static int have_prog = 0;

#endif /* not ENABLE_PROG_SERVER */
