/*
 * awcs-proto.c - aWCS protocol implementation
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
 * $Id: awcs-proto.c,v 1.20 2000/09/12 22:14:16 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_AWCS_PROTO

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#ifndef __MINGW32__
# include <netinet/in.h>
#endif

#include "util.h"
#include "socket.h"
#include "pipe-socket.h"
#include "alloc.h"
#include "server-core.h"
#include "coserver/coserver.h"
#include "awcs-proto.h"
#include "server.h"
#include "server-socket.h"

/*
 * The aWCS port configuration.
 */
portcfg_t awcs_net_port = 
{
  PROTO_TCP,   /* prefered protocol type */
  42424,       /* prefered port */
  "127.0.0.1", /* local ip */
  NULL,        /* calculated later */
  NULL,        /* inpipe */
  NULL         /* outpipe */
};

portcfg_t awcs_fs_port = 
{
  PROTO_PIPE,  /* prefered protocol type */
  0,           /* prefered port */
  NULL,        /* local ip */
  NULL,
  ".aWCSrecv", /* inpipe */
  ".aWCSsend"  /* outpipe */
};

/*
 * The aWCS server instance configuration.
 */
awcs_config_t awcs_config =
{
  &awcs_net_port, /* current network port configuration */
  &awcs_fs_port,  /* current filesystem port configuration */
  NULL,           /* aWCS Master server */
  0,              /* Was Master server detected ? */
  NULL            /* aWCS clients user base hash */
};

/*
 * Definition of the configuration items delivered by libsizzle.
 */
key_value_pair_t awcs_config_prototype [] =
{
  REGISTER_PORTCFG ("netport", awcs_config.netport, DEFAULTABLE),
  REGISTER_PORTCFG ("fsport", awcs_config.fsport, DEFAULTABLE),
  REGISTER_END ()
};

/*
 * The aWCS server definition.
 */
server_definition_t awcs_server_definition =
{
  "aWCS server",         /* server description */
  "aWCS",                /* server prefix used in the config file "aWCS?" */
  NULL,                  /* global initialization */
  awcs_init,             /* server instance initialization */
  awcs_detect_proto,     /* protocol detection routine */
  awcs_connect_socket,   /* callback when detected */
  awcs_finalize,         /* server instance finalization */
  NULL,                  /* global finalization */
  NULL,                  /* client info */
  NULL,                  /* server info */
  NULL,                  /* server timer */
  NULL,                  /* handle request callback */
  &awcs_config,          /* the instance configuration */
  sizeof (awcs_config),  /* sizeof the instance configuration */
  awcs_config_prototype  /* configuration definition for libsizzle */
};

/*
 * These 3 (three) routines are for modifying the client hash key
 * processing. Because we are using unique IDs for identifying aWCS 
 * clients it is not necessary to have character strings here.
 */
static unsigned
awcs_hash_keylen (char *id)
{
  return 4;
}

static int 
awcs_hash_equals (char *id1, char *id2)
{
  return memcmp (id1, id2, 4);
}
 
static unsigned long 
awcs_hash_code (char *id)
{
  unsigned long code = UINT32 (id);
  return code;
}

/*
 * Local aWCS server instance initialization routine.
 */
int
awcs_init (server_t *server)
{
  awcs_config_t *cfg = server->cfg;

  /* initialize server instance */
  cfg->master = 0;
  cfg->clients = hash_create (4);
  cfg->clients->code = awcs_hash_code;
  cfg->clients->keylen = awcs_hash_keylen;
  cfg->clients->equals = awcs_hash_equals;
  cfg->server = NULL;

  /* bind the port configuration */
  server_bind (server, cfg->netport);
  server_bind (server, cfg->fsport);

  return 0;
}

/*
 * Local aWCS server instance finalizer.
 */
int
awcs_finalize (server_t *server)
{
  awcs_config_t *cfg = server->cfg;
  
  hash_destroy (cfg->clients);
  
  return 0;
}

#if ENABLE_REVERSE_LOOKUP
/*
 * Gets called when a nslookup coserver has resolved a IP address
 * for socket SOCK to name and has been identified as an aWCS client.
 */
int
awcs_nslookup_done (char *host, int id, int version)
{
  awcs_config_t *cfg;
  socket_t sock = sock_find_id (id);

  if (host && sock && sock->version == version)
    {
      cfg = sock->cfg;

      if (!cfg->server)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "awcs: no master server (nslookup_done)\n");
#endif
	  return -1;
	}

#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "awcs: sending resolved ip to master\n");
#endif

      if (sock_printf (cfg->server, AWCS_ID_FMT " %d " AWCS_ID_FMT " %s%c",
		       cfg->server->id,
		       STATUS_NSLOOKUP,
		       sock->id,
		       host, '\0'))
	{
	  log_printf (LOG_FATAL, "awcs: master write error\n");
	  sock_schedule_for_shutdown (cfg->server);
	  cfg->server = NULL;
	  awcs_disconnect_clients (cfg);
	  return -1;
	}
    }

  return 0;
}
#endif

#if ENABLE_IDENT
/*
 * Gets called when a ident coserver has resolved a IP address
 * for socket SOCK to name and has been identified as an aWCS client.
 */
int
awcs_ident_done (char *user, int id, int version)
{
  socket_t sock = sock_find_id (id);
  awcs_config_t *cfg;

  if (user && sock && sock->version == version)
    {
      cfg = sock->cfg;

      if (!cfg->server)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "awcs: no master server (ident_done)\n");
#endif
	  return -1;
	}

#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "awcs: sending identified client to master\n");
#endif

      if (sock_printf (cfg->server, AWCS_ID_FMT " %d " AWCS_ID_FMT " %s%c",
		       cfg->server->id,
		       STATUS_IDENT,
		       sock->id,
		       user, '\0'))
	{
	  log_printf (LOG_FATAL, "awcs: master write error\n");
	  sock_schedule_for_shutdown (cfg->server);
	  cfg->server = NULL;
	  awcs_disconnect_clients (cfg);
	  return -1;
	}
    }

  return 0;
}
#endif

/*
 * This is called when a valid aWCS client has been connected.
 * The routine reports this to the Master server and invokes
 * a reverse nslookup and a ident request.
 */
static int
awcs_status_connected (socket_t sock)
{
  unsigned short port;
  unsigned long addr;
  awcs_config_t *cfg = sock->cfg;

  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "awcs: no master server (status_connected)\n");
#endif
      return -1;
    }

  addr = sock->remote_addr;
  port = sock->remote_port;

#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "awcs: sending connect on socket id %d to master\n",
	      sock->id);
#endif

  if (sock_printf (cfg->server, AWCS_ID_FMT " %d " AWCS_ID_FMT " %s:%u%c",
		   cfg->server->id,
		   STATUS_CONNECT,
		   sock->id,
		   util_inet_ntoa (addr),
		   htons (port), '\0'))
    {
      log_printf (LOG_FATAL, "awcs: master write error\n");
      sock_schedule_for_shutdown (cfg->server);
      cfg->server = NULL;
      awcs_disconnect_clients (cfg);
      return -1;
    }

  /*
   * Initiate a reverse nslookup and ident request for a real client.
   */
  if (sock != cfg->server)
    {
      coserver_reverse (addr, awcs_nslookup_done, sock->id, sock->version);
      coserver_ident (sock, awcs_ident_done, sock->id, sock->version);
    }

  return 0;
}

/*
 * Send a status message to the master server
 * telling it that client `client' has disconnected. `reason'
 * is an error code telling how the client got disconnected.
 */
static int
awcs_status_disconnected (socket_t sock, int reason)
{
  awcs_config_t *cfg = sock->cfg;

  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "awcs: no master server (status_disconnected)\n");
#endif
      return -1;
    }

#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "awcs: sending disconnect on socket %d to master\n",
	      sock->sock_desc);
#endif /* ENABLE_DEBUG */

  if (sock_printf (cfg->server, AWCS_ID_FMT " %d " AWCS_ID_FMT " %d%c", 
		   cfg->server->id,
		   STATUS_DISCONNECT,
		   sock->id, 
		   reason, '\0'))
    {
      log_printf (LOG_FATAL, "awcs: master write error\n");
      sock_schedule_for_shutdown (cfg->server);
      cfg->server = NULL;
      awcs_disconnect_clients (cfg);
      return -1;
    }
  
  return 0;
}

/*
 * Send a status message to the master server
 * telling it that client `client' has been kicked. `reason'
 * is an error code telling why the client got kicked.
 */
static int
awcs_status_kicked (socket_t sock, int reason)
{
  awcs_config_t *cfg = sock->cfg;
  
  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "awcs: no master server (status_kicked)\n");
#endif
      return -1;
    }

  if (sock_printf (cfg->server, AWCS_ID_FMT " %d " AWCS_ID_FMT " %d%c",
		   cfg->server->id,
		   STATUS_KICK,
		   sock->id, 
		   reason, '\0'))
    {
      log_printf (LOG_FATAL, "awcs: master write error\n");
      sock_schedule_for_shutdown (cfg->server);
      cfg->server = NULL;
      awcs_disconnect_clients (cfg);
      return -1;
    }

  return 0;
}

/*
 * Send a message to the master server indicating this server
 * is still alive.
 */
static int
awcs_status_alive (awcs_config_t *cfg)
{
  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "awcs: no master server (status_alive)\n");
#endif /* ENABLE_DEBUG */
      return -1;
    }

  if (sock_printf (cfg->server, AWCS_ID_FMT " %d 42%c",
		   cfg->server->id,
		   STATUS_ALIVE, '\0'))
    {
      log_printf (LOG_FATAL, "awcs: master write error\n");
      sock_schedule_for_shutdown (cfg->server);
      cfg->server = NULL;
      awcs_disconnect_clients (cfg);
      return -1;
    }
  
  return 0;
}

static int
awcs_status_notify (awcs_config_t *cfg) 
{
  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "awcs: no master server (status_notify)\n");
#endif /* ENABLE_DEBUG */
      return -1;
    }

  if (sock_printf (cfg->server, AWCS_ID_FMT " %d 42%c",
		   cfg->server->id,
		   STATUS_NOTIFY, '\0'))
    {
      log_printf (LOG_FATAL, "awcs: master write error\n");
      sock_schedule_for_shutdown (cfg->server);
      cfg->server = NULL;
      awcs_disconnect_clients (cfg);
      return -1;
    }

  return 0;
}

/*
 * Broadcast message `cmd' to all connected aWCS clients.
 */
static int
awcs_process_broadcast (awcs_config_t *cfg, char *cmd, int cmd_len)
{
  socket_t *sock;
  int n;
  
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "awcs: broadcasting\n");
#endif /* ENABLE_DEBUG */

  if ((sock = (socket_t *)hash_values (cfg->clients)) != NULL)
    {
      for (n = 0; n < hash_size (cfg->clients); n++)
	{
	  if (sock_write (sock[n], cmd, cmd_len))
	    {
	      sock_schedule_for_shutdown (sock[n]);
	    }
	}
      hash_xfree (sock);
    }
  return 0;
}

/*
 * Send message `cmd' to all clients
 * in the client list at the start of `cmd'.
 */
static int
awcs_process_multicast (awcs_config_t *cfg, char *cmd, int cmd_len)
{
  char *msg;
  int address;
  socket_t sock;

  msg = cmd;

  while (*msg != ' ')
    {
      msg++;
      cmd_len--;
    }
  msg++;
  cmd_len--;

  for (;;)
    {
      address = util_atoi (cmd);
      sock = (socket_t) hash_get (cfg->clients, (char *) &address);

      if (sock)
	{
	  if (sock_write (sock, msg, cmd_len))
	    {
	      sock_schedule_for_shutdown (sock);
	    }
	}
#if ENABLE_DEBUG
      else
	{
	  log_printf (LOG_DEBUG, 
		      "awcs: master sent invalid id (multicast %d)\n", 
		      address);
	}
#endif /* ENABLE_DEBUG */

      cmd += AWCS_ID_SIZE;
      if (!*cmd || *cmd == ' ')
	break;
      cmd++;
    }
  return 0;
}

/*
 * Process a status request.
 */
static int
awcs_process_status (awcs_config_t *cfg, char *cmd, int cmd_len)
{
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "awcs: sending status message\n");
#endif /* ENABLE_DEBUG */
  awcs_status_alive (cfg);

  return 0;
}

/*
 * Kick all clients in the client list at the
 * beginning of `cmd'. the rest of `cmd' is the kicking reason.
 */
static int
awcs_process_kick (awcs_config_t *cfg, char *cmd, int cmd_len)
{
  socket_t sock;
  int address;

  for (;;)
    {
      address = util_atoi (cmd);
      sock = (socket_t) hash_get (cfg->clients, (char *) &address);

      if (sock)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "awcs: kicking socket %d\n",
		      sock->sock_desc);
#endif /* ENABLE_DEBUG */
	  /*
	   * This is a hack.  We set the handler to NULL to be sure
	   * that the master server will not get a KICKED status
	   * message for a kick he initiated.
	   */
	  sock->kicked_socket = NULL;
	  sock_schedule_for_shutdown (sock);
	}
#if ENABLE_DEBUG
      else
	{
	  log_printf (LOG_DEBUG, 
		      "awcs: master sent invalid id (kick %d)\n",
		      address);
	}
#endif /* ENABLE_DEBUG */

      cmd += AWCS_ID_SIZE;
      if (!*cmd || *cmd == ' ')
	break;
      cmd++;
    }

  return 0;
}

/*
 * Turn off flood protection for clients listed
 * in command if flag is false, turn it on otherwise.
 */
static int
awcs_process_floodcmd (awcs_config_t *cfg, char *cmd, int cmd_len, int flag)
{
  socket_t sock;
  int address;

  for (;;)
    {
      address = util_atoi (cmd);
      sock = (socket_t) hash_get (cfg->clients, (char *) &address);

      if (sock)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "awcs: switching flood "
		      "control for socket %d %s\n",
		      sock->sock_desc, flag ? "on" : "off");
#endif /* ENABLE_DEBUG */
	  if (flag)
	    sock->flags &= ~SOCK_FLAG_NOFLOOD;
	  else
	    sock->flags |= SOCK_FLAG_NOFLOOD;
	}
#if ENABLE_DEBUG
      else
	{
	  log_printf (LOG_DEBUG, 
		      "awcs: master sent invalid id (floodcmd %d)\n",
		      address);
	}
#endif /* ENABLE_DEBUG */
      
      cmd += AWCS_ID_SIZE;
      if (!*cmd || *cmd == ' ')
	break;
      cmd++;
    }

  return 0;
}

/*
 * Handle a request from the master server.  Return a non-zero value
 * on error.
 */
static int
awcs_handle_master_request (awcs_config_t *cfg, char *request, int request_len)
{
  if (request_len <= 0)
    return -1;

#if 0
  util_hexdump (stdout, "master request", cfg->server->sock_desc,
		request, request_len, 0);
#endif

  request_len--;
  switch (*request++)
    {
    case '0':
      request++;
      request_len--;
      awcs_process_broadcast (cfg, request, request_len);
      break;
    case '1':
      request++;
      request_len--;
      awcs_process_multicast (cfg, request, request_len);
      break;
    case '2':
      request++;
      request_len--;
      awcs_process_status (cfg, request, request_len);
      break;
    case '3':
      request++;
      request_len--;
      awcs_process_kick (cfg, request, request_len);
      break;
    case '4':
      request++;
      request_len--;
      awcs_process_floodcmd (cfg, request, request_len, 0);
      break;
    case '5':
      request++;
      request_len--;
      awcs_process_floodcmd (cfg, request, request_len, 1);
      break;
    case '6':
      /* The following code should not be executed anymore. */
      log_printf (LOG_NOTICE, "awcs: skipping '6' ...\n");
      break;
    default:
      log_printf (LOG_ERROR, "awcs: bad master server request\n");
      break;
    }
  return 0;
}

/*
 * Schedule all aWCS clients for shutdown. Call this if the
 * connection to the master server has been lost.
 */
void
awcs_disconnect_clients (awcs_config_t *cfg)
{
  socket_t *sock;
  int n;

  if ((sock = (socket_t *) hash_values (cfg->clients)) != NULL)
    {
      for (n = 0; n < hash_size (cfg->clients); n++)
	{
	  sock_schedule_for_shutdown (sock[n]);
	}
      hash_xfree (sock);
    }
}

/*
 * Handle the request REQUEST of length REQUEST_LEN from socket
 * SOCK.  Return a non-zero value on error.
 */
static int
awcs_handle_request (socket_t sock, char *request, int request_len)
{
  int ret;
  awcs_config_t *cfg = sock->cfg;

#if 0
  util_hexdump (stdout, "awcs request", sock->sock_desc, 
		request, request_len, 1000);
#endif

  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "awcs: no master server (awcs_handle_request)\n");
#endif
      awcs_disconnect_clients (cfg);
      return -1;
    }

  if (sock == cfg->server)
    {
      awcs_handle_master_request (cfg, request, request_len);
    }
  else
    {
      ret = sock_printf (cfg->server, AWCS_ID_FMT " ", sock->id);
      if (ret == 0)
	{
	  ret = sock_write (cfg->server, request, request_len);
	}
      if (ret)
	{
	  log_printf (LOG_FATAL, "awcs: master write error\n");
	  sock_schedule_for_shutdown (cfg->server);
	  cfg->server = NULL;
	  awcs_disconnect_clients (cfg);
	  return -1;
	}
    }
  return 0;
}

/*
 * Checks whether a complete request has been accumulated in socket
 * SOCK's receive queue.  If yes, then the
 * request gets handled and removed from the queue.
 */
int
awcs_check_request (socket_t sock)
{
  int retval = 0;
  int request_len = 0;
  char * p, * packet;

  p = sock->recv_buffer;
  packet = p;

  do
    {
      while (p < sock->recv_buffer + sock->recv_buffer_fill && *p != '\0')
        p++;

      if (*p == '\0' && p < sock->recv_buffer + sock->recv_buffer_fill)
        {
          p++;
          request_len += (p - packet);
	  retval = awcs_handle_request(sock, packet, p - packet);
          packet = p;
        }
    }
  while (p < sock->recv_buffer + sock->recv_buffer_fill);
  
  if (request_len > 0)
    {
      memmove(sock->recv_buffer, packet,
              sock->recv_buffer_fill - request_len);
    }
  sock->recv_buffer_fill -= request_len;

  return 0;
}

/*
 * Gets called when a client has connected as socket SOCK and has been
 * identified as an aWCS client. MASTER ist set if it was a master
 * server detection.
 */
int
awcs_connect_socket (void *config, socket_t sock)
{
  awcs_config_t *cfg = config;

  /*
   * Don't allow clients without master server or multiple master servers.
   */
  if ((!cfg->server && !cfg->master) || (cfg->server && cfg->master))
    {
      if (!cfg->server && !cfg->master)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG,
		      "awcs: master not present, cannot connect socket %d\n",
		      sock->sock_desc);
#endif
	}
      if (cfg->server && cfg->master)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG,
		      "awcs: master is present, cannot connect socket %d\n",
		      sock->sock_desc);
#endif
	}

      return -1;
    }

#if ENABLE_DEBUG
  if (sock->flags & SOCK_FLAG_PIPE)
    {
      log_printf (LOG_DEBUG, "awcs: connection on pipe (%d-%d)\n",
		  sock->pipe_desc[READ], sock->pipe_desc[WRITE]);
    }
  else 
    {
      log_printf (LOG_DEBUG, "awcs: connection on socket %d\n",
		  sock->sock_desc);
    }
#endif /* ENABLE_DEBUG */

  sock->disconnected_socket = awcs_disconnected_socket;
  sock->check_request = awcs_check_request;

  if (cfg->master)
    {
#if ENABLE_DEBUG
      if (sock->flags & SOCK_FLAG_PIPE)
	{
	  log_printf (LOG_NOTICE, 
		      "awcs: master server connected on pipe (%d-%d)\n",
		      sock->pipe_desc[READ], sock->pipe_desc[WRITE]);
	}
      else
	{
	  log_printf (LOG_NOTICE, 
		      "awcs: master server connected on socket %d\n",
		      sock->sock_desc);
	}
#endif /* ENABLE_DEBUG */
      cfg->server = sock;
      sock->idle_func = awcs_idle_func;
      sock->idle_counter = 3;
      sock->flags |= (SOCK_FLAG_NOFLOOD | SOCK_FLAG_PRIORITY);
      log_printf (LOG_NOTICE,
		  "awcs: resizing master buffers: %d, %d\n",
		  MASTER_SEND_BUFSIZE, MASTER_RECV_BUFSIZE);
      sock_resize_buffers (sock, MASTER_SEND_BUFSIZE, MASTER_RECV_BUFSIZE);
      sock->flags |= SOCK_FLAG_INITED;
    }
  else
    {
      hash_put (cfg->clients, (char *) &sock->id, sock);
      sock->kicked_socket = awcs_kicked_socket;
    }

  /*
   * Tell the master server about the connection (even itself).
   */
  awcs_status_connected (sock);
  return 0;
}

/*
 * Gets called when the socket SOCK had disconnected.  All clients are
 * kicked when the socket was the master server, otherwise a diconnection
 * message is sent to the master server.
 */
int
awcs_disconnected_socket (socket_t sock)
{
  awcs_config_t *cfg = sock->cfg;
  
  if (sock == cfg->server)
    {
      log_printf (LOG_ERROR, "awcs: lost master server\n");
      cfg->server = NULL;
      awcs_disconnect_clients (cfg);
    }
  else
    {
      awcs_status_disconnected (sock, 1);
      hash_delete (cfg->clients, (char *) &sock->id);
    }

  return 0;
}

/*
 * Gets called when the socket SOCK got kicked.  REASON is true when
 * a output buffer overflow occured and false, when it was an input
 * buffer overflow or flooding.
 */
int
awcs_kicked_socket (socket_t sock, int reason)
{
  if (!reason)
    awcs_status_kicked (sock, KICK_FLOODING);
  else
    awcs_status_kicked (sock, KICK_CRAWLING);

  return 0;
}

/*
 * The socket for the master server gets set this function as the timer
 * function.  The function sends an idle message to the master server
 * and resets its timeout counter.
 */
int
awcs_idle_func (socket_t sock)
{
  awcs_config_t *cfg = sock->cfg;

  if (sock == cfg->server)
    {
      sock->idle_counter = 3;
      awcs_status_notify (cfg);
    }
  return 0;
}

/*
 * This function gets called for new sockets which are not yet
 * identified.  It returns a non-zero value when the contents in
 * the receive buffer looks like an aWCS identification request.
 */
int
awcs_detect_proto (void *config, socket_t sock)
{
  awcs_config_t *cfg = config;
  int len = 0;

#if 0
  util_hexdump (stdout, "detecting awcs", sock->sock_desc,
		sock->recv_buffer, sock->recv_buffer_fill, 0);
#endif

  if (sock->recv_buffer_fill >= MASTER_DETECTION && 
      !memcmp (sock->recv_buffer, AWCS_MASTER, MASTER_DETECTION))
    {
      cfg->master = 1;
      log_printf (LOG_NOTICE, "awcs: master detected\n");
      len = MASTER_DETECTION;
    }
  else if (sock->recv_buffer_fill >= CLIENT_DETECTION && 
	   !memcmp (sock->recv_buffer, AWCS_CLIENT, CLIENT_DETECTION))
    {
      cfg->master = 0;
      log_printf (LOG_NOTICE, "awcs: client detected\n");
      len = CLIENT_DETECTION;
    }

  if (len == 0) return 0;

  if (sock->recv_buffer_fill > len)
    {
      memmove (sock->recv_buffer, 
	       sock->recv_buffer + len, 
	       sock->recv_buffer_fill - len);
    }
  sock->recv_buffer_fill -= len;
  return len;
}

int have_awcs = 1;

#else /* ENABLE_AWCS_PROTO */

int have_awcs = 0;	/* Shut compiler warnings up,
			   make runtime checking possible */

#endif /* not ENABLE_AWCS_PROTO */
