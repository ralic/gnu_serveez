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
 * $Id: awcs-proto.c,v 1.6 2000/06/18 22:13:03 raimi Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_AWCS_PROTO

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifdef __MINGW32__
# include <winsock.h>
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
  &awcs_config,          /* the instance configuration */
  sizeof (awcs_config),  /* sizeof the instance configuration */
  awcs_config_prototype  /* configuration defintion for libsizzle */
};

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
  
  if (unlink (cfg->fsport->inpipe) == -1)
    log_printf (LOG_ERROR, "unlink: %s\n", SYS_ERROR);
  if (unlink (cfg->fsport->outpipe) == -1)
    log_printf (LOG_ERROR, "unlink: %s\n", SYS_ERROR);

  hash_destroy (cfg->clients);
  
  return 0;
}

#if ENABLE_REVERSE_LOOKUP
/*
 * Gets called when a nslookup coserver has resolved a IP address
 * for socket SOCK to name and has been identified as an aWCS client.
 */
int
awcs_nslookup_done (socket_t sock, char *host)
{
  awcs_config_t *cfg = sock->cfg;

  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "awcs_nslookup_done: no master server\n");
#endif
      return -1;
    }

  if (host)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "sending resolved ip to master\n");
#endif

      if (sock_printf (cfg->server, "%04d %d %04d %s%c",
		       cfg->server->socket_id,
		       STATUS_NSLOOKUP,
		       sock->socket_id,
		       host, '\0'))
	{
	  log_printf (LOG_FATAL, "master write error\n");
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
awcs_ident_done (socket_t sock, char *user)
{
  awcs_config_t *cfg = sock->cfg;

  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "awcs_ident_done: no master server\n");
#endif
      return -1;
    }

  if (user)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "sending identified client to master\n");
#endif

      if (sock_printf (cfg->server, "%04d %d %04d %s%c",
		       cfg->server->socket_id,
		       STATUS_IDENT,
		       sock->socket_id,
		       user, '\0'))
	{
	  log_printf (LOG_FATAL, "master write error\n");
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
status_connected (socket_t sock)
{
  unsigned short port;
  unsigned addr;
  awcs_config_t *cfg = sock->cfg;

  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "status_connected: no master server\n");
#endif
      return -1;
    }

  addr = sock->remote_addr;
  port = sock->remote_port;

#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "sending connect on socket id %d to master\n",
	      sock->socket_id);
#endif
  if (sock_printf (cfg->server, "%04d %d %04d %d.%d.%d.%d:%u%c",
		   cfg->server->socket_id,
		   STATUS_CONNECT,
		   sock->socket_id,
		   (addr >> 24) & 0xff, (addr >> 16) & 0xff, 
		   (addr >> 8) & 0xff, (addr) & 0xff, port, '\0'))
    {
      log_printf (LOG_FATAL, "master write error\n");
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
      coserver_reverse (addr, (coserver_handle_result_t) awcs_nslookup_done,
			sock);
      coserver_ident (sock, (coserver_handle_result_t) awcs_ident_done,
		      sock);
    }

  return 0;
}

/*
 * Send a status message to the master server
 * telling him that client `client' has disconnected. `reason'
 * is an error code telling how the client got disconnected.
 */
static int
status_disconnected (socket_t sock, int reason)
{
  awcs_config_t *cfg = sock->cfg;

  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "status_disconnected: no master server\n");
#endif
      return -1;
    }

#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "socket %d buffers: r[%d] s[%d]\n",
	      sock->sock_desc, 
	      sock->recv_buffer_fill, 
	      sock->send_buffer_fill); 
  log_printf (LOG_DEBUG, "sending disconnect on socket %d to master\n",
	      sock->sock_desc);
#endif /* ENABLE_DEBUG */

  if (sock_printf (cfg->server, "%04d %d %04d %d%c", 
		   cfg->server->socket_id,
		   STATUS_DISCONNECT,
		   sock->socket_id, 
		   reason, '\0'))
    {
      log_printf (LOG_FATAL, "master write error\n");
      sock_schedule_for_shutdown (cfg->server);
      cfg->server = NULL;
      awcs_disconnect_clients (cfg);
      return -1;
    }
  
  return 0;
}

/*
 * Send a status message to the master server
 * telling him that client `client' has been kicked. `reason'
 * is an error code telling why the client got kicked.
 */
static int
status_kicked (socket_t sock, int reason)
{
  awcs_config_t *cfg = sock->cfg;
  
  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "status_kicked: no master server\n");
#endif
      return -1;
    }

  if (sock_printf (cfg->server, "%04d %d %04d %d%c",
		   cfg->server->socket_id,
		   STATUS_KICK,
		   sock->socket_id, 
		   reason, '\0'))
    {
      log_printf (LOG_FATAL, "master write error\n");
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
status_alive (awcs_config_t *cfg)
{
  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "status_alive: no master server\n");
#endif /* ENABLE_DEBUG */
      return -1;
    }

  if (sock_printf (cfg->server, "%04d %d 42%c",
		   cfg->server->socket_id,
		   STATUS_ALIVE, '\0'))
    {
      log_printf (LOG_FATAL, "master write error\n");
      sock_schedule_for_shutdown (cfg->server);
      cfg->server = NULL;
      awcs_disconnect_clients (cfg);
      return -1;
    }
  
  return 0;
}

static int
status_notify (awcs_config_t *cfg) 
{
  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "status_notify: no master server\n");
#endif /* ENABLE_DEBUG */
      return -1;
    }

  if (sock_printf (cfg->server, "%04d %d 42%c",
		   cfg->server->socket_id,
		   STATUS_NOTIFY, '\0'))
    {
      log_printf (LOG_FATAL, "master write error\n");
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
process_broadcast (awcs_config_t *cfg, char *cmd, int cmd_len)
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
      xfree (sock);
    }
  return 0;
}

/*
 * Send message `cmd' to all clients
 * in the client list at the start of `cmd'.
 */
static int
process_multicast (char *cmd, int cmd_len)
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

      if ((sock = find_sock_by_id (address)) != NULL)
	{
	  if (sock_write (sock, msg, cmd_len))
	    {
	      sock_schedule_for_shutdown (sock);
	    }
	}

      cmd += 4;
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
process_status (awcs_config_t *cfg, char *cmd, int cmd_len)
{
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "awcs: sending status message\n");
#endif /* ENABLE_DEBUG */
  status_alive (cfg);

  return 0;
}

/*
 * Kick all clients in the client list at the
 * beginning of `cmd'. the rest of `cmd' is the kicking reason.
 */
static int
process_kick (char *cmd, int cmd_len)
{
  socket_t sock;
  int address;

  for (;;)
    {
      address = util_atoi (cmd);

      sock = find_sock_by_id (address);
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

      cmd += 4;
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
process_floodcmd (char *cmd, int cmd_len, int flag)
{
  socket_t sock;
  int address;

  for (;;)
    {
      address = util_atoi (cmd);
      
      if ((sock = find_sock_by_id (address)) != NULL)
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
      
      cmd += 4;
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
handle_master_request (awcs_config_t *cfg, char *request, int request_len)
{
  if (request_len <= 0)
    return -1;

  request_len--;
  switch (*request++)
    {
    case '0':
      request++;
      request_len--;
      process_broadcast (cfg, request, request_len);
      break;
    case '1':
      request++;
      request_len--;
      process_multicast (request, request_len);
      break;
    case '2':
      request++;
      request_len--;
      process_status (cfg, request, request_len);
      break;
    case '3':
      request++;
      request_len--;
      process_kick (request, request_len);
      break;
    case '4':
      request++;
      request_len--;
      process_floodcmd (request, request_len, 0);
      break;
    case '5':
      request++;
      request_len--;
      process_floodcmd (request, request_len, 1);
      break;
    case '6':
      /*
       * The following code should not be executed anymore,
       * thus the call to abort() below.
       */
      log_printf (LOG_NOTICE, 
		  "awcs: skipping '6' ...\n");
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
static void
awcs_disconnect_clients (awcs_config_t *cfg)
{
  socket_t *sock;
  int n;

  if ((sock = (socket_t *)hash_values (cfg->clients)) != NULL)
    {
      for (n = 0; n < hash_size (cfg->clients); n++)
	{
	  sock_schedule_for_shutdown (sock[n]);
	}
      xfree (sock);
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
  dump_request(stdout, "handling", sock->sock_desc, request, request_len);
#endif

  if (!cfg->server)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "awcs_handle_request: no master server\n");
#endif
      awcs_disconnect_clients (cfg);
      return -1;
    }

  if (sock == cfg->server)
    {
      handle_master_request (cfg, request, request_len);
    }
  else
    {
      if (!(ret = sock_printf (cfg->server, "%04d ", sock->socket_id)))
	{
	  ret = sock_write (cfg->server, request, request_len);
	}
      if (ret)
	{
	  log_printf (LOG_FATAL, "master write error\n");
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
#if 0
	  log_printf(LOG_DEBUG, "socket = %d packetsize = %d\n%s", 
		     sock->sock_desc, p-packet, packet);
#endif
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
		      "master not present, cannot connect socket %d\n",
		      sock->sock_desc);
#endif
	}
      if (cfg->server && cfg->master)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG,
		      "master is present, cannot connect socket %d\n",
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
  sock->kicked_socket = awcs_kicked_socket;

  if (cfg->master)
    {
#if ENABLE_DEBUG
      if (sock->flags & SOCK_FLAG_PIPE)
	{
	  log_printf (LOG_NOTICE, "master server connected on pipe (%d-%d)\n",
		      sock->pipe_desc[READ], sock->pipe_desc[WRITE]);
	}
      else
	{
	  log_printf (LOG_NOTICE, "master server connected on socket %d\n",
		      sock->sock_desc);
	}
#endif /* ENABLE_DEBUG */
      cfg->server = sock;
      sock->idle_func = awcs_idle_func;
      sock->idle_counter = 3;
      sock->flags |= SOCK_FLAG_NOFLOOD;
      log_printf (LOG_NOTICE,
		  "resizing master server buffers: %d, %d\n",
		  MASTER_SEND_BUFSIZE, MASTER_RECV_BUFSIZE);
      sock_resize_buffers (sock, MASTER_SEND_BUFSIZE, MASTER_RECV_BUFSIZE);
      sock->flags |= SOCK_FLAG_INITED;
    }
  else
    {
      char key[5];
      sprintf (key, "%04d", sock->socket_id);
      hash_put (cfg->clients, key, sock);
    }

  /*
   * Tell the master server about the connection (even itself).
   */
  status_connected (sock);
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
  char key[5];
  
  if (sock == cfg->server)
    {
      log_printf (LOG_ERROR, "lost master server\n");
      cfg->server = NULL;
      awcs_disconnect_clients (cfg);
    }
  else
    {
      status_disconnected (sock, 1);
      sprintf (key, "%04d", sock->socket_id);
      hash_delete (cfg->clients, key);
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
    status_kicked (sock, KICK_FLOODING);
  else
    status_kicked (sock, KICK_CRAWLING);

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
      status_notify (cfg);
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
  log_printf (LOG_NOTICE, "awcs: in awcs_detect_proto\n");
  dump_request (stdout, "buf", sock->sock_desc,
		sock->recv_buffer, sock->recv_buffer_fill);
#endif /* 0 */

  if (sock->recv_buffer_fill >= MASTER_DETECTION && 
      !memcmp (sock->recv_buffer, AWCS_MASTER, MASTER_DETECTION))
    {
      cfg->master = 1;
      log_printf (LOG_NOTICE, "awcs master detected\n");
      len = MASTER_DETECTION;
    }
  else if (sock->recv_buffer_fill >= CLIENT_DETECTION && 
	   !memcmp (sock->recv_buffer, AWCS_CLIENT, CLIENT_DETECTION))
    {
      cfg->master = 0;
      log_printf (LOG_NOTICE, "awcs client detected\n");
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
