/*
 * ident-proto.c - fake ident server implementation
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
 * $Id: ident-proto.c,v 1.5 2001/02/02 11:26:23 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_FAKEIDENT

#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "libserveez.h"
#include "ident-proto.h"

/*
 * Default value definitions for the server configuration.
 */
struct portcfg fakeident_default_port = 
{
  PROTO_TCP,      /* we are tcp */
  113,            /* standard port to listen on */
  "*",            /* bind all local addresses */
  NULL,           /* calculated from above values later */
  NULL,           /* no inpipe for us */
  NULL            /* no outpipe for us */
};

/*
 * Configuration for this server containing the default values.
 */
struct fakeident_config fakeident_config =
{
  "UNIX",
  NULL,
  &fakeident_default_port
};

/*
 * The config prototype. Register items that can be configured.
 */
struct key_value_pair fakeident_config_prototype [] =
{
  REGISTER_STR ("systemtype", fakeident_config.systemtype, DEFAULTABLE),
  REGISTER_STR ("username", fakeident_config.username, DEFAULTABLE),
  REGISTER_PORTCFG ("port", fakeident_config.port, DEFAULTABLE),
  REGISTER_END ()
};

/*
 * Function forward declaration.
 */
int fakeident_init (struct server *server);
int fakeident_connect_socket (void *acfg, socket_t sock);
int fakeident_detect_proto (void *something, socket_t sock);
int fakeident_handle_request (socket_t sock, char *request, int len);
char *fakeident_info_server (struct server *server);

/*
 * The actual server definition.
 */
struct server_definition fakeident_server_definition = 
{
  "simple fake ident server",
  "fakeident",
  NULL,                       /* global init */
  fakeident_init,
  fakeident_detect_proto,
  fakeident_connect_socket,
  NULL,                       /* local finalize */
  NULL,                       /* global finalize */
  NULL,
  fakeident_info_server,
  NULL,
  NULL,
  &fakeident_config,
  sizeof (fakeident_config),
  fakeident_config_prototype
};

/*
 * Initialize a fakeident server instance
 */
int
fakeident_init (struct server *server)
{
  struct fakeident_config *c = server->cfg;

  /*
   * bind us to the port
   */
  server_bind (server, c->port);

  return 0;
}

/*
 * When we get a connection this callback is invoked. set up more callbacks.
 */
int
fakeident_connect_socket (void *acfg, socket_t sock)
{
  struct fakeident_config *cfg = (struct fakeident_config *) acfg;

  sock->boundary = "\n";
  sock->boundary_size = 1;
  sock->check_request = sock_check_request;
  sock->handle_request = fakeident_handle_request;

  return 0;
}

/*
 * Try to find out of that line is meant for us.
 * A valid request for us is: "number[space]*,[space]*number[space]*\r\n
 */
int
fakeident_detect_proto (void *something, socket_t sock)
{
  int retval = 0;
  char *p = sock->recv_buffer;
  char *end = sock->recv_buffer + sock->recv_buffer_fill;

  /* first a number */
  for (; p < end && isdigit (*p); p++);
  if (p == end)
    goto out;

  /* spaces */
  for (; p < end && *p == ' '; p++);
  if (p == end)
    goto out;

  /* comma */
  if (*p == ',')
    p++;
  else
    goto out;

  /* spaces */
  for (; p < end && *p == ' '; p++);
  if (p == end)
    goto out;

  /* number */
  for (; p < end && isdigit (*p); p++);
  if (p == end)
    goto out;
  
  /* spaces */
  for (; p < end && *p == ' '; p++);
  if (p == end)
    goto out;
  
  /* optional '\r' */
  if (p < end && *p == '\r')
    p++;

  /* now, if that is a '\n' we finally have it */
  if (p < end && *p == '\n')
    retval = -1;

 out:
  return retval;
}

/*
 * Handle a single request when an input line is recognized.
 */
int
fakeident_handle_request (socket_t sock, char *request, int len)
{
  struct fakeident_config *cfg = (struct fakeident_config *) sock->cfg;
  int err = 0;
  char *p = sock->recv_buffer;

  /* isolate the number pair for the reply */
  for (; *p != '\r' && *p != '\n'; p++);
  *p = '\0';

  /*
   * This is the last line we send. Let the runtime close the connection
   * when data is away.
   */
  sock->flags |= SOCK_FLAG_FINAL_WRITE;

  /* 
   * If a username is set we reply "systemtype : username"
   * or "ERROR : NO-USER" else.
   */
  if (cfg->username == NULL)
    {
      err = sock_printf (sock, "%s : ERROR : NO-USER\r\n", sock->recv_buffer);
    }
  else
    {
      err = sock_printf (sock, "%s : USERID : %s : %s\r\n", sock->recv_buffer,
			 cfg->systemtype, cfg->username);
    }

  return err;
}

/*
 * Info about server as seen in control protocol.
 */
char *
fakeident_info_server (struct server *server)
{
  struct fakeident_config *cfg = server->cfg;
  static char info[80];

  if (cfg->username == NULL)
    {
      sprintf (info, " signalling ERROR : NO-USER");
    }
  else
    {
      sprintf (info, " reporting user `%s' running system type `%s'",
	       cfg->username, cfg->systemtype);
    }
  
  return info;
}

#else /* ENABLE_FAKEIDENT */

int fakeident_dummy = 0;  /* Shut compiler warnings up. */

#endif /* ENABLE_FAKEIDENT */
