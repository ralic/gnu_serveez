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
 * $Id: irc-proto.c,v 1.2 2000/06/12 23:06:06 raimi Exp $
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
#include "server-core.h"
#include "serveez.h"
#include "irc-core/irc-core.h"
#include "irc-proto.h"
#include "irc-event.h"
#include "irc-server.h"

irc_config_t   irc_config;              /* IRC configuration hash */
irc_channel_t *irc_channel_list = NULL; /* channel list root */
irc_client_t  *irc_client_list  = NULL; /* client list root */
irc_client_history_t *irc_client_history = NULL; /* client history list root */

/* 
 * The lookup table for all connected IRC clients indexed
 * by the socket id. This is used to pass the second arg
 * for the callbacks.
 */
irc_client_t *irc_client_lookup_table[SOCKET_MAX_IDS];

/*
 * Free all the buffers of an IRC x-line.
 */
void
irc_free_xline(char **xline)
{
  int n;

  if(xline)
    {
      n = 0;
      while(xline[n])
	{
	  xfree(xline[n++]);
	}
      xfree(xline);
    }
}

/*
 * Close the IRC Protocol configuration hash.
 */
void
irc_close_config (irc_config_t *cfg)
{
  int n;

  /* free the MOTD */
  for(n=0; n<cfg->MOTDs; n++)
    xfree(cfg->MOTD[n]);

  /* free configuration hash variables */
  xfree(cfg->host);
  xfree(cfg->info);
  xfree(cfg->realhost);
  irc_free_xline(cfg->CLine);

  /* free the client history */
  irc_delete_client_history();

  /* free the server IRC list */
  irc_delete_servers();
}

#define MAX_HOST_LEN 256
#define MAX_INFO_LEN 256

/*
 * Initialization of the IRC server configuration hash.
 */
void
irc_init_config (irc_config_t *cfg)
{
  char info[MAX_INFO_LEN];
  char host[MAX_HOST_LEN];
  char realhost[MAX_HOST_LEN];
  int port;

  irc_create_lcset();

  /* scan the M line */
  irc_parse_line(cfg->MLine, "M:%s:%s:%s:%d", 
		 host, realhost, info, &port);
  cfg->host = xmalloc(strlen(host) + 1);
  strcpy(cfg->host, host);
  cfg->realhost = xmalloc(strlen(realhost) + 1);
  strcpy(cfg->realhost, realhost);
  cfg->info = xmalloc(strlen(info) + 1);
  strcpy(cfg->info, info);
  cfg->port = port;
}

/*
 * Check if a certain client is in a channel. Return -1 if not and
 * return the client's position in the channel list. This is useful to
 * to detect its channel flags. If you passed a valid socket the 
 * ERR_NOTONCHANNEL reply is sent to the socket.
 */
int
is_client_in_channel (socket_t sock,          /* client's socket */
		      irc_client_t *client,   /* the client structure */
		      irc_channel_t *channel) /* the channel to search */
{
  irc_config_t *cfg;
  int n;

  /* find client in the channel, return position if found */
  for(n=0; n<channel->clients; n++)
    if(channel->client[n] == client->nick)
      return n;
  
  /* not in channel ! */
  if(sock)
    {
      cfg = sock->cfg;
      irc_printf(sock, ":%s %03d %s " ERR_NOTONCHANNEL_TEXT "\n",
		 cfg->host, ERR_NOTONCHANNEL, client->nick, channel->name);
    }
  return -1;
}

/*
 * Add a nick to a certain channel.
 */
int
add_client_to_channel (irc_config_t *cfg,
		       irc_client_t *client, 
		       char *chan)
{
  irc_channel_t *channel;
  int n;

  /* does the channel exist locally ? */
  if((channel = irc_find_channel(chan)))
    {
      /* is the nick already in the channel ? */
      for(n=0; n<channel->clients; n++)
	if(channel->client[n] == client->nick)
	  break;

      /* no, add nick to channel */
      if(n == channel->clients)
	{
	  channel->client[n] = client->nick;
	  channel->cflag[n] = 0;
	  channel->clients++;
#if ENABLE_DEBUG
	  log_printf(LOG_DEBUG, "irc: %s joined channel %s\n", 
		     client->nick, channel->name);
#endif
	  n = client->channels;
	  client->channel[n] = channel->name;
	  client->channels++;
	}
    }

  /* no, the channel does not exists, yet */
  else
    {
      /* create one and set the first client as operator */
      channel = irc_add_channel(cfg, chan);
      channel->client[0] = client->nick;
      channel->cflag[0] = MODE_OPERATOR;
      channel->clients++;
      strcpy(channel->by, client->nick);
      channel->since = time(NULL);
#if ENABLE_DEBUG
      log_printf(LOG_DEBUG, "irc: channel %s created\n", channel->name);
      log_printf(LOG_DEBUG, "irc: %s joined channel %s\n", 
		 client->nick, channel->name);
#endif
      n = client->channels;
      client->channel[n] = channel->name;
      client->channels++;
    }

  return 0;
}

/*
 * Delete a client of a given channel.
 */
int
del_client_of_channel (irc_config_t *cfg, 
		       irc_client_t *client, 
		       char *chan)
{
  irc_channel_t *channel;
  int n, i, last;

  /* does the channel exist ? */
  if((channel = irc_find_channel(chan)))
    {
      /* delete the client of this channel */
      last = channel->clients - 1;
      for(n=0; n<channel->clients; n++)
	if(channel->client[n] == client->nick)
	  {
	    channel->client[n] = channel->client[last];
	    channel->cflag[n] = channel->cflag[last];
	    channel->clients--;
#if ENABLE_DEBUG
	    log_printf(LOG_DEBUG, "irc: %s left channel %s\n",
		       client->nick, chan);
#endif
	    /* clear this channel of client's list */
	    last = client->channels - 1;
	    for(i=0; i<client->channels; i++)
	      if(client->channel[i] == channel->name)
		{
		  client->channel[i] = client->channel[last];
		  client->channels--;
		  break;
		}
	    break;
	  }
      /* no client in channel ? */
      if(channel->clients == 0)
	{
#if ENABLE_DEBUG
	  log_printf(LOG_DEBUG, "irc: channel %s destroyed\n", chan);
#endif
	  irc_delete_channel (cfg, chan);
	}
    }

  return 0;
}

/*
 * Send an error Message if not enough paras. Return non zero if
 * there are too less.
 */
int
check_paras(socket_t sock,           /* the client's socket */
	    irc_client_t *client,    /* the irc client itself */
	    irc_config_t *conf,      /* config hash */
	    irc_request_t *request,  /* the request */
	    int n)                   /* para # */
{
  if(request->paras < n)
    {
      irc_printf(sock, ":%s %03d %s " ERR_NEEDMOREPARAMS_TEXT "\n",
		 conf->host, ERR_NEEDMOREPARAMS, client->nick,
		 request->request);
      return -1;
    }
  return 0;
}

/*
 * This routine checks if a given client is away or not then
 * sends this clients away message to the sender back. Return
 * non-zero if away.
 */
int
is_client_absent (irc_client_t *client, /* requested client */
		  irc_client_t *cl)     /* the one who want to know about */
{
  irc_config_t *cfg;
  socket_t sock;

  if(client->flag & UMODE_AWAY)
    {
      sock = find_sock_by_id(cl->id);
      cfg = sock->cfg;
      irc_printf(sock, ":%s %03d %s " RPL_AWAY_TEXT "\n",
		 cfg->host, RPL_AWAY, cl->nick, client->nick,
		 client->away);
      return -1;
    }
  return 0;
}

/*
 * This routine erases a client of all channels by a quit reason,
 * then the client is deleted itself.
 */
int
del_client_of_channels (irc_config_t *cfg, 
			irc_client_t *client, 
			char *reason)
{
  irc_channel_t *channel;
  irc_client_t *cl;
  socket_t sock;
  socket_t xsock;
  int i;

  sock = find_sock_by_id (client->id);

  /* go through all channels */
  while(client->channels)
    {
      channel = irc_find_channel(client->channel[0]);

      /* tell all clients in the channel about disconnecting */
      for(i=0; i<channel->clients; i++)
	{
	  if(channel->client[i] == client->nick)
	    continue;
	  
	  cl = irc_find_nick(channel->client[i]);
	  xsock = find_sock_by_id(cl->id);
	  irc_printf(xsock, ":%s!%s@%s QUIT :%s\n",
		     client->nick, client->user, client->host, reason);
	}
	  
      /* delete this client of channel */
      del_client_of_channel(cfg, client, client->channel[0]);
    }

  /* send last error Message */
  irc_printf(sock, "ERROR :" IRC_CLOSING_LINK "\n", client->host, reason);

  /* delete this client */
  irc_delete_client (cfg, sock->socket_id);

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
  irc_client_t *client;
  
  /* is it a valid IRC connection ? */
  if((client = irc_find_client(sock->socket_id)))
    {
      del_client_of_channels(cfg, client, IRC_CONNECTION_LOST);
    }
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
  irc_client_t *client;

  client = irc_find_client(sock->socket_id);
  
  /* 
   * Shutdown this connection if the client did not respond
   * within a certain period of time.
   */
  if(client->ping > 0)
    {
      return -1;
    }

  /*
   * Ping a client connection if necessary.
   */
  if((time(NULL) - sock->last_recv) >= IRC_PING_INTERVAL)
    {
      irc_printf(sock, "PING %s\n", cfg->host);
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
  int n;
  irc_client_t *client;

  irc_parse_request(request, len);
  client = irc_find_client(sock->socket_id);

  for(n=0; irc_callback[n].request; n++)
    {
      if(!strcasecmp(irc_callback[n].request, irc_request.request))
	{
	  return 
	    irc_callback[n].func(sock, client, &irc_request);
	}
    }

  irc_printf(sock, ":%s %03d %s " ERR_UNKNOWNCOMMAND_TEXT "\n",
	     cfg->host, ERR_UNKNOWNCOMMAND, client->nick,
	     irc_request.request);

  return 0;
}

/*
 * Delete a channel from the channel list. Returns -1 if there was no
 * apropiate channel.
 */
int
irc_delete_channel (irc_config_t *cfg, char *channel)
{
  irc_channel_t *chan, *old;
  int n;

  for(chan = old = irc_channel_list; chan; old = chan, chan = chan->next)
    {
      if(!strcmp(chan->name, channel))
	{
	  old->next = chan->next;
	  if(chan == irc_channel_list) irc_channel_list = chan->next;
	  for(n=0; n<chan->bans; n++)
	    xfree(chan->ban[n]);
	  xfree(chan);
	  cfg->channels--;
	  return 0;
	}
    }
  return -1;
}

/*
 * Find a channel within the current channel list. Return NULL if
 * the channel has not been found.
 */
irc_channel_t *
irc_find_channel(char *channel)
{
  irc_channel_t *chan;

  for(chan = irc_channel_list; chan; chan = chan->next)
    {
      if(!string_equal(chan->name, channel))
	{
	  return chan;
	}
    }
  return NULL;
}

/*
 * Find a Matching channel in the current channel list. Return NULL if
 * no channel has not been found and return the first channel been found.
 * Start searching at the given channel (if NULL start at the beginning).
 */
irc_channel_t *
irc_regex_channel(irc_channel_t *ch, char *regex)
{
  irc_channel_t *channel;

  channel = ch ? ch->next : irc_channel_list;
  for(; channel; channel = channel->next)
    {
      if(string_regex(channel->name, regex))
	{
	  return channel;
	}
    }
  return NULL;
}

/*
 * Add a new channel to the channel list.
 */
irc_channel_t *
irc_add_channel (irc_config_t *cfg, char *channel)
{
  irc_channel_t *chan;

  if(irc_find_channel(channel))
    return NULL;

  chan = xmalloc(sizeof(irc_channel_t));
  memset(chan, 0, sizeof(irc_channel_t));
  strcpy(chan->name, channel);
  chan->next = irc_channel_list;
  irc_channel_list = chan;
  cfg->channels++;
  return chan;
}

/*
 * Add a client to the client history list.
 */
void
irc_add_client_history(irc_client_t *cl)
{
  irc_client_history_t *client;

  client = xmalloc(sizeof(irc_client_history_t));
  client->next = irc_client_history;
  irc_client_history = client;
  strcpy(client->nick, cl->nick);
  strcpy(client->user, cl->user);
  strcpy(client->host, cl->host);
  strcpy(client->real, cl->real);
}

/*
 * Find a nick in the history client list. Return NULL if
 * no nick has not been found and return the first client been found.
 * Start searching at the given client (if NULL start at the beginning).
 */
irc_client_history_t *
irc_find_nick_history(irc_client_history_t *cl, char *nick)
{
  irc_client_history_t *client;

  client = cl ? cl->next : irc_client_history;
  for(; client; client = client->next)
    {
      if(!string_equal(client->nick, nick))
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
irc_delete_client_history(void)
{
  irc_client_history_t *client;
  irc_client_history_t *old;

  for(client = irc_client_history; client; client = old)
    {
      old = client->next;
      xfree(client);
    }
  irc_client_history = NULL;
}

/*
 * Delete a client from the client list. Returns -1 if there was no
 * apropiate client.
 */
int
irc_delete_client (irc_config_t *cfg, int id)
{
  irc_client_t *client;
  irc_client_t *old = NULL;

  for(client = irc_client_list; client; client = client->next)
    {
      if(client->id == id)
	{
	  /* put this client into the history list */
	  irc_add_client_history(client);

	  irc_client_lookup_table[id] = NULL;
	  if(old) old->next = client->next;
	  if(client == irc_client_list) irc_client_list = client->next;
	  xfree(client);
	  cfg->clients--;
	  cfg->users--;
	  return 0;
	}
      old = client;
    }
  return -1;
}

/*
 * Find a user@host within the current client list. Return NULL if
 * no client has not been found.
 */
irc_client_t *
irc_find_userhost(char *user, char *host)
{
  irc_client_t *client;

  for(client = irc_client_list; client; client = client->next)
    {
      if(!strcmp(client->user, user) && !strcmp(client->host, host))
	{
	  return client;
	}
    }
  return NULL;
}

/*
 * Find a client within the current client list. Return NULL if
 * the client has not been found.
 */
irc_client_t *
irc_find_client(int id)
{
  return 
    irc_client_lookup_table[id];
}

/*
 * Find a nick within the current client list. Return NULL if
 * the nick has not been found.
 */
irc_client_t *
irc_find_nick(char *nick)
{
  irc_client_t *client;

  for(client = irc_client_list; client; client = client->next)
    {
      if(!string_equal(client->nick, nick))
	{
	  return client;
	}
    }
  return NULL;
}

/*
 * Find a Matching nick in the current client list. Return NULL if
 * no nick has not been found and return the first client been found.
 * Start searching at the given client (if NULL start at the beginning).
 */
irc_client_t *
irc_regex_nick(irc_client_t *cl, char *regex)
{
  irc_client_t *client;

  client = cl ? cl->next : irc_client_list;
  for(; client; client = client->next)
    {
      if(string_regex(client->nick, regex))
	{
	  return client;
	}
    }
  return NULL;
}

/*
 * Add a new client to the client list.
 */
irc_client_t *
irc_add_client (irc_config_t *cfg, int id)
{
  irc_client_t *client;

  if(irc_find_client(id))
    return NULL;

  client = xmalloc(sizeof(irc_client_t));
  memset(client, 0, sizeof(irc_client_t));
  client->id = id;
  client->next = irc_client_list;
  irc_client_list = client;
  cfg->clients++;
  cfg->users++;
  irc_client_lookup_table[id] = client;
  return client;
}

/*
 * Print a formatted string to the socket SOCK.
 */
int
irc_printf(socket_t sock, const char *fmt, ...)
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
  if (len > sizeof(buffer))
    len = sizeof(buffer);

  if((len = sock_write(sock, buffer, len)))
    {
      sock->flags |= SOCK_FLAG_KILLED;
    }
  return len;
}

int have_irc = 1;

#else

int have_irc = 0;            /* Shut up compiler, remember for runtime */

#endif /* ENABLE_IRC_PROTO */
