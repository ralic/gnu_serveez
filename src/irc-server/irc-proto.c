/*
 * irc-proto.c - basic IRC protocol functions
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
 * $Id: irc-proto.c,v 1.6 2000/07/07 16:26:20 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_IRC_PROTO

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if HAVE_STRINGS_H
# include <strings.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "snprintf.h"
#include "socket.h"
#include "alloc.h"
#include "util.h"
#include "server.h"
#include "server-core.h"
#include "server-socket.h"
#include "serveez.h"
#include "irc-core/irc-core.h"
#include "irc-proto.h"
#include "irc-event.h"
#include "irc-server.h"

/*
 * The IRC port configuration.
 */
portcfg_t irc_port =
{
  PROTO_TCP,  /* tcp protocol only */
  42424,      /* prefered tcp port */
  "*",        /* prefered local ip address */
  NULL,       /* calculated automatically */
  NULL,       /* no receiving pipe */
  NULL        /* no sending (listening) pipe */
};

/*
 * The IRC server instance default configuration,
 */
irc_config_t irc_config =
{
  &irc_port,             /* port configuration */
  0,                     /* logged in operators */
  0,                     /* logged in users */
  0,                     /* unknown connections */
  0,                     /* invisible users */
  NULL,                  /* virtual host name */
  NULL,                  /* read server host name */
  42424,                 /* listening tcp port */
#if ENABLE_TIMESTAMP
  0,                     /* delta value to UTC */
#endif
  { NULL },              /* message of the day */
  0,                     /* message of the day lines */
  0,                     /* motd last modified date */
  "../doc/irc-MOTD.txt", /* file name of masage of the day */
  NULL,                  /* MLine */
  NULL,                  /* ALine */
  NULL,                  /* YLines */
  NULL,                  /* ILines */
  NULL,                  /* OLines */
  NULL,                  /* oLines */
  NULL,                  /* CLines */
  NULL,                  /* NLines */
  NULL,                  /* KLines */
  NULL,                  /* server password */
  NULL,                  /* server info */
  NULL,                  /* email address of maintainers */
  NULL,                  /* admininfo */
  NULL,                  /* location1 */
  NULL,                  /* location2 */
  { NULL },              /* operator host names */
  { NULL },              /* operator passwords */
  NULL,                  /* irc channel hash */
  NULL,                  /* irc client hash */
  NULL,                  /* irc server list root */
  NULL                   /* client history list root */
};

/*
 * Definition of the configuration items processed by sizzle.
 */
key_value_pair_t irc_config_prototype [] =
{
  REGISTER_PORTCFG ("port", irc_config.netport, DEFAULTABLE),
  REGISTER_STR ("MOTD-file", irc_config.MOTD_file, DEFAULTABLE),
#if ENABLE_TIMESTAMP
  REGISTER_INT ("tsdelta", irc_config.tsdelta, DEFAULTABLE),
#endif
  REGISTER_STR ("admininfo", irc_config.admininfo, NOTDEFAULTABLE),
  REGISTER_STR ("email", irc_config.email, NOTDEFAULTABLE),
  REGISTER_STR ("location1", irc_config.location1, NOTDEFAULTABLE),
  REGISTER_STR ("location2", irc_config.location2, NOTDEFAULTABLE),
  REGISTER_STR ("M-line", irc_config.MLine, NOTDEFAULTABLE),
  REGISTER_STR ("A-line", irc_config.ALine, DEFAULTABLE),
  REGISTER_STRARRAY ("Y-lines", irc_config.YLine, DEFAULTABLE),
  REGISTER_STRARRAY ("I-lines", irc_config.ILine, DEFAULTABLE),
  REGISTER_STRARRAY ("O-lines", irc_config.OLine, DEFAULTABLE),
  REGISTER_STRARRAY ("o-lines", irc_config.oLine, DEFAULTABLE),
  REGISTER_STRARRAY ("C-lines", irc_config.CLine, DEFAULTABLE),
  REGISTER_STRARRAY ("N-lines", irc_config.NLine, DEFAULTABLE),
  REGISTER_STRARRAY ("K-lines", irc_config.KLine, DEFAULTABLE),
  REGISTER_END ()
};

/*
 * Definition of the IRC server.
 */
server_definition_t irc_server_definition =
{
  "irc server",        /* long description */
  "irc",               /* short description for instanciating */
  irc_global_init,     /* global initializer */
  irc_init,            /* instance initializer */
  irc_detect_proto,    /* detection routine */
  irc_connect_socket,  /* connection routine */
  irc_finalize,        /* instance finalizer */
  irc_global_finalize, /* global finalizer */
  &irc_config,         /* default configuration */
  sizeof (irc_config), /* size of configuration */
  irc_config_prototype /* configuration prototypes */
};

/*
 * Global IRC server initializer.
 */
int
irc_global_init (void)
{
  irc_create_lcset ();
  return 0;
}

/*
 * Global IRC server finalizer.
 */
int
irc_global_finalize (void)
{
  return 0;
}

#define MAX_HOST_LEN 256
#define MAX_INFO_LEN 256

/*
 * Initialization of the IRC server instance.
 */
int
irc_init (server_t *server)
{
  irc_config_t *cfg = server->cfg;
  char info[MAX_INFO_LEN];
  char host[MAX_HOST_LEN];
  char realhost[MAX_HOST_LEN];
  int port;

  /* initialize hashes and lists */
  cfg->clients = hash_create (4);
  cfg->channels = hash_create (4);
  cfg->servers = NULL;
  cfg->history = NULL;

  /* scan the M line */
  irc_parse_line (cfg->MLine, "M:%s:%s:%s:%d", 
		  host, realhost, info, &port);
  cfg->host = xmalloc (strlen (host) + 1);
  strcpy (cfg->host, host);
  cfg->realhost = xmalloc (strlen (realhost) + 1);
  strcpy (cfg->realhost, realhost);
  cfg->info = xmalloc (strlen (info) + 1);
  strcpy (cfg->info, info);
  cfg->port = port;

  irc_connect_servers (cfg);

  server_bind (server, cfg->netport);

  return 0;
}

/*
 * IRC server instance finalizer.
 */
int
irc_finalize (server_t *server)
{
  irc_config_t *cfg = server->cfg;
  int n;

  /* free the MOTD */
  for(n = 0; n < cfg->MOTDs; n++)
    xfree (cfg->MOTD[n]);

  /* free configuration hash variables */
  xfree (cfg->host);
  xfree (cfg->info);
  xfree (cfg->realhost);

  /* free the client history */
  irc_delete_client_history (cfg);

  /* free the server IRC list */
  irc_delete_servers (cfg);

  hash_destroy (cfg->clients);
  hash_destroy (cfg->channels);

  return 0;
}

/*
 * Check if a certain client is in a channel. Return -1 if not and
 * return the client's position in the channel list. This is useful to
 * to detect its channel flags. If you passed a valid socket the 
 * ERR_NOTONCHANNEL reply is sent to the socket.
 */
int
irc_client_in_channel (socket_t sock,          /* client's socket */
		       irc_client_t *client,   /* the client structure */
		       irc_channel_t *channel) /* the channel to search */
{
  irc_config_t *cfg;
  int n;

  /* find client in the channel, return position if found */
  for (n = 0; n < channel->clients; n++)
    if (channel->client[n] == client)
      return n;
  
  /* not in channel ! */
  if (sock)
    {
      cfg = sock->cfg;
      irc_printf (sock, ":%s %03d %s " ERR_NOTONCHANNEL_TEXT "\n",
		  cfg->host, ERR_NOTONCHANNEL, client->nick, channel->name);
    }
  return -1;
}

/*
 * Add a nick to a certain channel.
 */
int
irc_join_channel (irc_config_t *cfg,
		  irc_client_t *client, 
		  char *chan)
{
  irc_channel_t *channel;
  int n;

  /* does the channel exist locally ? */
  if ((channel = irc_find_channel (cfg, chan)))
    {
      /* is the nick already in the channel ? */
      for (n = 0; n < channel->clients; n++)
	if (channel->client[n] == client)
	  break;

      /* no, add nick to channel */
      if (n == channel->clients)
	{
	  channel->client[n] = client;
	  channel->cflag[n] = 0;
	  channel->clients++;
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "irc: %s joined channel %s\n", 
		      client->nick, channel->name);
#endif
	  n = client->channels;
	  client->channel[n] = channel;
	  client->channels++;
	}
    }

  /* no, the channel does not exists, yet */
  else
    {
      /* create one and set the first client as operator */
      channel = irc_add_channel (cfg, chan);
      channel->client[0] = client;
      channel->cflag[0] = MODE_OPERATOR;
      channel->clients++;
      strcpy (channel->by, client->nick);
      channel->since = time (NULL);
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "irc: channel %s created\n", channel->name);
      log_printf (LOG_DEBUG, "irc: %s joined channel %s\n", 
		  client->nick, channel->name);
#endif
      n = client->channels;
      client->channel[n] = channel;
      client->channels++;
    }

  return 0;
}

/*
 * Delete a client of a given channel.
 */
int
irc_leave_channel (irc_config_t *cfg, 
		   irc_client_t *client, 
		   irc_channel_t *channel)
{
  int n, i, last;

  /* delete the client of this channel */
  last = channel->clients - 1;
  for (n = 0; n < channel->clients; n++)
    if (channel->client[n] == client)
      {
	channel->client[n] = channel->client[last];
	channel->cflag[n] = channel->cflag[last];
	channel->clients--;
#if ENABLE_DEBUG
	log_printf (LOG_DEBUG, "irc: %s left channel %s\n",
		    client->nick, channel->name);
#endif
	/* clear this channel of client's list */
	last = client->channels - 1;
	for (i = 0; i < client->channels; i++)
	  if (client->channel[i] == channel)
	    {
	      client->channel[i] = client->channel[last];
	      client->channels--;
	      break;
	    }
	break;
      }
  /* no client in channel ? */
  if (channel->clients == 0)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "irc: channel %s destroyed\n", channel->name);
#endif
      irc_delete_channel (cfg, channel);
    }

  return 0;
}

/*
 * Send an error message if there are not enough arguments given. 
 * Return non zero if there are less than necessary.
 */
int
irc_check_args (socket_t sock,           /* the client's socket */
		irc_client_t *client,    /* the irc client itself */
		irc_config_t *conf,      /* config hash */
		irc_request_t *request,  /* the request */
		int n)                   /* necessary arguments */
{
  if (request->paras < n)
    {
      irc_printf (sock, ":%s %03d %s " ERR_NEEDMOREPARAMS_TEXT "\n",
		  conf->host, ERR_NEEDMOREPARAMS, client->nick,
		  request->request);
      return -1;
    }
  return 0;
}

/*
 * This routine checks if a given client is away or not, then
 * sends this clients away message back to the client which requested
 * this. Return non-zero if away.
 */
int
irc_client_absent (irc_client_t *client,  /* requested client */
		   irc_client_t *rclient) /* who want to know about */
{
  irc_config_t *cfg;
  socket_t sock;

  if (client->flag & UMODE_AWAY)
    {
      sock = rclient->sock;
      cfg = sock->cfg;
      irc_printf (sock, ":%s %03d %s " RPL_AWAY_TEXT "\n",
		  cfg->host, RPL_AWAY, rclient->nick, client->nick,
		  client->away);
      return -1;
    }
  return 0;
}

/*
 * This routine erases a client from all channels by a quit reason,
 * then the client is deleted itself.
 */
int
irc_leave_all_channels (irc_config_t *cfg, 
			irc_client_t *client, 
			char *reason)
{
  irc_channel_t *channel;
  irc_client_t *cl;
  socket_t sock;
  socket_t xsock;
  int i;

  sock = client->sock;

  /* go through all channels */
  while (client->channels)
    {
      channel = client->channel[0];

      /* tell all clients in the channel about disconnecting */
      for (i = 0; i < channel->clients; i++)
	{
	  if (channel->client[i] == client)
	    continue;
	  
	  cl = channel->client[i];
	  xsock = cl->sock;
	  irc_printf (xsock, ":%s!%s@%s QUIT :%s\n",
		      client->nick, client->user, client->host, reason);
	}
	  
      /* delete this client of channel */
      irc_leave_channel (cfg, client, client->channel[0]);
    }

  /* send last error Message */
  irc_printf (sock, "ERROR :" IRC_CLOSING_LINK "\n", client->host, reason);

  /* delete this client */
  irc_delete_client (cfg, client);

  return 0;
}

/*
 * This function is the default callback if the connection of
 * an IRC client gets lost.
 */
int
irc_disconnect (socket_t sock)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *client = sock->data;
  
  /* is it a valid IRC connection ? */
  irc_leave_all_channels (cfg, client, IRC_CONNECTION_LOST);
  return 0;
}

/*
 * This is the idle callback for serveez. Here the IRC server could
 * send a PING to a IRC client.
 */
int
irc_idle (socket_t sock)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *client = sock->data;

  /* 
   * Shutdown this connection if the client did not respond
   * within a certain period of time.
   */
  if (client->ping > 0)
    {
      return -1;
    }

  /*
   * Ping a client connection if necessary.
   */
  if ((time (NULL) - sock->last_recv) >= IRC_PING_INTERVAL)
    {
      irc_printf (sock, "PING %s\n", cfg->host);
      client->ping++;
    }

  sock->idle_counter = IRC_PING_INTERVAL;
  return 0;
}

/*
 * This structure's field contains all the callback routines necessary
 * to react like an IRC server. The actual routines are defined in the
 * irc-event.h and implemented in the irc-event-?.c files.
 */
struct
{
  char *request;
  int (* func)(socket_t, irc_client_t *, irc_request_t *);
}
irc_callback[] =
{
  { "KILL",     irc_kill_callback     },
  { "ERROR",    irc_error_callback    },
  { "WHOWAS",   irc_whowas_callback   },
  { "ADMIN",    irc_admin_callback    },
  { "TIME",     irc_time_callback     },
  { "LUSERS",   irc_lusers_callback   },
  { "STATS",    irc_stats_callback    },
  { "PING",     irc_ping_callback     },
  { "PONG",     irc_pong_callback     },
  { "VERSION",  irc_version_callback  },
  { "KICK",     irc_kick_callback     },
  { "AWAY",     irc_away_callback     },
  { "WHO",      irc_who_callback      },
  { "WHOIS",    irc_whois_callback    },
  { "MOTD",     irc_motd_callback     },
  { "INVITE",   irc_invite_callback   },
  { "LIST",     irc_list_callback     },
  { "NAMES",    irc_names_callback    },
  { "NOTICE",   irc_note_callback     },
  { "TOPIC",    irc_topic_callback    },
  { "MODE",     irc_mode_callback     },
  { "PRIVMSG",  irc_priv_callback     },
  { "USERHOST", irc_userhost_callback },
  { "ISON",     irc_ison_callback     },
  { "PART",     irc_part_callback     },
  { "QUIT",     irc_quit_callback     },
  { "JOIN",     irc_join_callback     },
  { "PASS",     irc_pass_callback     },
  { "USER",     irc_user_callback     },
  { "NICK",     irc_nick_callback     },
  { NULL,       NULL                  }
};

int
irc_handle_request (socket_t sock, char *request, int len)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *client = sock->data;
  int n;

  irc_parse_request (request, len);

  for(n = 0; irc_callback[n].request; n++)
    {
      if (!strcasecmp (irc_callback[n].request, irc_request.request))
	{
	  return 
	    irc_callback[n].func (sock, client, &irc_request);
	}
    }

  irc_printf (sock, ":%s %03d %s " ERR_UNKNOWNCOMMAND_TEXT "\n",
	      cfg->host, ERR_UNKNOWNCOMMAND, client->nick,
	      irc_request.request);

  return 0;
}

/*
 * Delete a channel from the channel list. Returns -1 if there was no
 * apropiate channel.
 */
static int
irc_delete_channel (irc_config_t *cfg, irc_channel_t *channel)
{
  int n;

  if (hash_contains (cfg->channels, channel))
    {
      for (n = 0; n < channel->bans; n++)
	xfree (channel->ban[n]);
      hash_delete (cfg->channels, channel->name);
      xfree (channel);
      return 0;
    }
  return -1;
}

/*
 * Find a channel within the current channel list. Return NULL if
 * the channel has not been found.
 */
irc_channel_t *
irc_find_channel (irc_config_t *cfg, char *channel)
{
  irc_channel_t *chan;

  chan = hash_get (cfg->channels, channel);
  return chan;
}

/*
 * Find all matching channels in the current channel list. Return NULL if
 * no channel has not been found. You MUST xfree() this list if non-NULL.
 * The delivered array is NULL terminated.
 */
irc_channel_t **
irc_regex_channel (irc_config_t *cfg, char *regex)
{
  irc_channel_t **channel, **fchannel;
  int n, found, size;

  if ((channel = (irc_channel_t **)hash_values (cfg->channels)) != NULL)
    {
      size = hash_size (cfg->channels);
      fchannel = xmalloc (sizeof (irc_channel_t *) * (size + 1));
      for (found = n = 0; n < size; n++)
	{
	  if (irc_string_regex (channel[n]->name, regex))
	    {
	      fchannel[found++] = channel[n];
	    }
	}
      xfree (channel);
      fchannel[found++] = NULL;
      fchannel = xrealloc (fchannel, sizeof (irc_channel_t *) * (found));
      return fchannel;
    }
  return NULL;
}

/*
 * Add a new channel to the channel list.
 */
static irc_channel_t *
irc_add_channel (irc_config_t *cfg, char *name)
{
  irc_channel_t *channel;

  if (irc_find_channel (cfg, name))
    return NULL;

  channel = xmalloc (sizeof (irc_channel_t));
  memset (channel, 0, sizeof (irc_channel_t));
  strcpy (channel->name, name);
  hash_put (cfg->channels, name, channel);
  return channel;
}

/*
 * Add a client to the client history list.
 */
void
irc_add_client_history (irc_config_t *cfg, irc_client_t *cl)
{
  irc_client_history_t *client;

  client = xmalloc (sizeof (irc_client_history_t));
  strcpy (client->nick, cl->nick);
  strcpy (client->user, cl->user);
  strcpy (client->host, cl->host);
  strcpy (client->real, cl->real);
  
  client->next = cfg->history;
  cfg->history = client;
}

/*
 * Find a nick in the history client list. Return NULL if
 * no nick has not been found. Otherwise the first client found within
 * the history list.
 */
irc_client_history_t *
irc_find_nick_history (irc_config_t *cfg, 
		       irc_client_history_t *cl, 
		       char *nick)
{
  irc_client_history_t *client;

  client = cl ? cl->next : cfg->history;
  for(; client; client = client->next)
    {
      if(!irc_string_equal(client->nick, nick))
	{
	  return client;
	}
    }
  return NULL;
}

/*
 * Delete all the client history.
 */
void
irc_delete_client_history (irc_config_t *cfg)
{
  irc_client_history_t *client;
  irc_client_history_t *old;

  for (client = cfg->history; client; client = old)
    {
      old = client->next;
      xfree (client);
    }
  cfg->history = NULL;
}

/*
 * Delete a client from the client list. Returns -1 if there was no
 * apropiate client.
 */
int
irc_delete_client (irc_config_t *cfg, irc_client_t *client)
{
  /* put this client into the history list */
  irc_add_client_history (cfg, client);
  
  if (hash_delete (cfg->clients, client->nick) == NULL)
    return -1;

  xfree (client);
  cfg->users--;

  return 0;
}

/*
 * Find a user@host within the current client list. Return NULL if
 * no client has not been found.
 */
irc_client_t *
irc_find_userhost (irc_config_t *cfg, char *user, char *host)
{
  irc_client_t **client;
  irc_client_t *fclient;
  int n;

  if ((client = (irc_client_t **)hash_values (cfg->clients)) != NULL)
    {
      for (n = 0; n < hash_size (cfg->clients); n++)
	{
	  if (!strcmp (client[n]->user, user) && 
	      !strcmp (client[n]->host, host))
	    {
	      fclient = client[n];
	      xfree (client);
	      return fclient;
	    }
	}
      xfree (client);
    }
  return NULL;
}

/*
 * Find a nick within the current client list. Return NULL if
 * the nick has not been found.
 */
irc_client_t *
irc_find_nick (irc_config_t *cfg, char *nick)
{
  irc_client_t *client;

  if ((client = hash_get (cfg->clients, nick)) != NULL)
    {
      return client;
    }
  return NULL;
}

/*
 * Find all matching nicks in the current client list. Return NULL if
 * no nick has not been found. You MUST xfree() this array if it is
 * non-NULL. The delivered clients are NULL terminated.
 */
irc_client_t **
irc_regex_nick (irc_config_t *cfg, char *regex)
{
  irc_client_t **client, **fclient;
  int n, found, size;

  if ((client = (irc_client_t **)hash_values (cfg->clients)) != NULL)
    {
      size = hash_size (cfg->clients);
      fclient = xmalloc (sizeof (irc_client_t *) * (size + 1));
      for (found = n = 0; n < size; n++)
	{
	  if (irc_string_regex (client[n]->nick, regex))
	    {
	      fclient[found++] = client[n];
	    }
	}
      xfree (client);
      fclient[found++] = NULL;
      fclient = xrealloc (fclient, sizeof (irc_client_t *) * (found));
      return fclient;
    }
  return NULL;
}

/*
 * Add a new client to the client list.
 */
irc_client_t *
irc_add_client (irc_config_t *cfg, irc_client_t *client)
{
  if (irc_find_nick (cfg, client->nick))
    return NULL;

  hash_put (cfg->clients, client->nick, client);
  return client;
}

/*
 * Create a new IRC client structure. This will be stored within the
 * miscelleanous data field in the socket structure (sock->data).
 */
irc_client_t *
irc_create_client (irc_config_t *cfg)
{
  irc_client_t *client;

  client = xmalloc (sizeof (irc_client_t));
  memset (client, 0, sizeof (irc_client_t));
  cfg->users++;
  return client;
}

/*
 * Print a formatted string to the socket SOCK.
 */
int
irc_printf (socket_t sock, const char *fmt, ...)
{
  va_list args;
  static char buffer[VSNPRINTF_BUF_SIZE];
  unsigned len;

  if (sock->flags & SOCK_FLAG_KILLED)
    return 0;

  va_start (args, fmt);
  len = vsnprintf (buffer, VSNPRINTF_BUF_SIZE, fmt, args);
  va_end (args);

  /* Just to be sure... */
  if (len > sizeof (buffer))
    len = sizeof (buffer);

  if ((len = sock_write (sock, buffer, len)))
    {
      sock->flags |= SOCK_FLAG_KILLED;
    }
  return len;
}

int have_irc = 1;

#else /* ENABLE_IRC_PROTO */

int have_irc = 0;            /* Shut up compiler, remember for runtime */

#endif /* ENABLE_IRC_PROTO */
