/*
 * irc-event-5.c - IRC events -- User-based queries
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
 * $Id: irc-event-5.c,v 1.2 2000/06/12 23:06:06 raimi Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_IRC_PROTO

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "socket.h"
#include "alloc.h"
#include "util.h"
#include "server-core.h"
#include "irc-core/irc-core.h"
#include "irc-proto.h"
#include "irc-event.h"

/*         Command: WHOWAS
 *      Parameters: <nickname> [<count> [<server>]]
 * Numeric Replies: ERR_NONICKNAMEGIVEN ERR_WASNOSUCHNICK
 *                  RPL_WHOWASUSER      RPL_WHOISSERVER
 *                  RPL_ENDOFWHOWAS
 */
int
irc_whowas_callback (socket_t sock, 
		     irc_client_t *client,
		     irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_history_t *cl;
  char *nick, *server;
  int n, i, found;

  /* check if there is a nick given */
  if(request->paras < 1)
    {
      irc_printf(sock, ":%s %03d %s " ERR_NONICKNAMEGIVEN_TEXT "\n",
		 cfg->host, ERR_NONICKNAMEGIVEN, client->nick);
    }

  nick = request->para[0];
  n = atoi(request->para[1]);

  /* is there a server para given ? */
  if(request->paras > 2)
    {
      server = request->para[2];
    }

  /* look through the history list */
  cl = NULL;
  i = 0;
  found = 0;
  while((cl = irc_find_nick_history(cl, nick)) && (i < n || n <= 0))
    {
      found = 1;
      irc_printf(sock, ":%s %03d %s " RPL_WHOWASUSER_TEXT "\n",
		 cfg->host, RPL_WHOWASUSER, client->nick,
		 cl->nick, cl->user, cl->host, cl->real);
      i++;
    }
  /* did you found a nick in the history ? */
  if(found)
    {
      irc_printf(sock, ":%s %03d %s " RPL_ENDOFWHOWAS_TEXT "\n",
		 cfg->host, RPL_ENDOFWHOWAS, client->nick, nick);
    }
  else
    {
      irc_printf(sock, ":%s %03d %s " ERR_WASNOSUCHNICK_TEXT "\n",
		 cfg->host, ERR_WASNOSUCHNICK, client->nick, nick);
    }

  return 0;
}

/*
 * Check if a certain client is visible to another.
 */
int
is_client_visible (irc_client_t *client,  /* who wants to know about */
		   irc_client_t *rclient) /* this client */
{
  irc_channel_t *channel;
  int n;

  /* invisible ? */
  if(rclient->flag & UMODE_INVISIBLE)
    return 0;

  /* 
   * Visible, but not in a public (no secret or private) channel ? 
   * The client is also visible if they share a channel.
   */
  for(n=0; n<rclient->channels; n++)
    {
      channel = irc_find_channel(rclient->channel[n]);

      /* public channel ? */
      if(!(channel->flag & (MODE_SECRET | MODE_PRIVATE)))
	return -1;
      else
	{
	  /* no, but do they share it ? */
	  if(is_client_in_channel(NULL, client, channel) != -1)
	    return -1;
	}
    }
  return -1;
}

/*
 * Send a user WHOIS reply.
 */
void
send_user_infos (socket_t sock,        /* the socket for the client to send */
		 irc_client_t *client, /* reply client */
		 irc_client_t *cl)     /* infos about this client */
{
  irc_config_t *cfg = sock->cfg;
  irc_channel_t *channel;
  socket_t xsock;
  char text[MAX_MSG_LEN];
  int n, i;

  if(!is_client_visible(client, cl)) return;

  irc_printf(sock, ":%s %03d %s " RPL_WHOISUSER_TEXT "\n",
	     cfg->host, RPL_WHOISUSER, client->nick,
	     cl->nick, cl->user, cl->host, cl->real);
	  
  irc_printf(sock, ":%s %03d %s " RPL_WHOISSERVER_TEXT "\n",
	     cfg->host, RPL_WHOISSERVER, client->nick,
	     cl->nick, cl->server, "place server info here");
	  
  /* operator ? */
  if(cl->flag & UMODE_OPERATOR)
    irc_printf(sock, ":%s %03d %s " RPL_WHOISOPERATOR_TEXT "\n",
	       cfg->host, RPL_WHOISOPERATOR, client->nick,
	       cl->nick);
	  
  /* idle seconds */
  xsock = find_sock_by_id(cl->id);
  irc_printf(sock, ":%s %03d %s " RPL_WHOISIDLE_TEXT "\n",
	     cfg->host, RPL_WHOISIDLE, client->nick,
	     cl->nick, time(NULL) - xsock->last_send, cl->since);

  /* build channel list */
  for(text[0]=0, i=0; i<cl->channels; i++)
    {
      channel = irc_find_channel(cl->channel[i]);
      n = is_client_in_channel(NULL, cl, channel);
      if(channel->cflag[n] & MODE_OPERATOR)
	strcat(text, "@");
      else if(channel->cflag[n] & MODE_VOICE)
	strcat(text, "+");
      strcat(text, cl->channel[i]);
      strcat(text, " ");
    }

  /* send channel list */
  irc_printf(sock, ":%s %03d %s " RPL_WHOISCHANNELS_TEXT "\n",
	     cfg->host, RPL_WHOISCHANNELS, client->nick,
	     cl->nick, text);
}

/*
 *         Command: WHOIS
 *      Parameters: [<server>] <nickmask>[,<nickmask>[,...]]
 * Numeric Replies: ERR_NOSUCHSERVER   ERR_NONICKNAMEGIVEN
 *                  RPL_WHOISUSER      RPL_WHOISCHANNELS
 *                  RPL_WHOISCHANNELS  RPL_WHOISSERVER
 *                  RPL_AWAY           RPL_WHOISOPERATOR
 *                  RPL_WHOISIDLE      ERR_NOSUCHNICK
 *                  RPL_ENDOFWHOIS
 */
int
irc_whois_callback (socket_t sock, 
		    irc_client_t *client,
		    irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  char *nick, *chan;
  int n;

  /* enough paras ? */
  if(check_paras(sock, client, cfg, request, 1))
    return 0;

  /* server Mask ? */
  if(string_regex(cfg->host, request->para[0]))
    irc_parse_target(request, 1);

  /* go through all targets */
  for(n=0; n<request->targets; n++)
    {
      nick = request->target[n].nick;
      chan = request->target[n].channel;

      /* nick Mask ? */
      if(strstr(nick, "*") || strstr(nick, "?"))
	{
	  cl = NULL;
	  while((cl = irc_regex_nick(cl, nick)))
	    {
	      /* is this client away ? */
	      if(is_client_absent(cl, client))
		continue;
	      send_user_infos (sock, client, cl);
	    }
	}
      /* is target a plain nick ? */
      else if((cl = irc_find_nick(nick)))
	{
	  /* is this client away ? */
	  if(is_client_absent(cl, client)) continue;
	  send_user_infos (sock, client, cl);
	}

      irc_printf(sock, ":%s %03d %s " RPL_ENDOFWHOIS_TEXT "\n",
		 cfg->host, RPL_ENDOFWHOIS, client->nick,
		 cl->nick);
    }
  return 0;
}

/*
 * Send a single WHO reply.
 */
void
send_client_infos (socket_t sock,          /* this client's socket */
		   irc_client_t *client,   /* this client */
		   irc_client_t *cl,       /* reply this client's info */
		   irc_channel_t *channel) /* channel 'cl' is in */
{
  irc_config_t *cfg = sock->cfg;
  char text[MAX_MSG_LEN];
  char *flag = "";
  int n;

  n = is_client_in_channel(NULL, cl, channel);
  if(channel->cflag[n] & MODE_OPERATOR)
    flag = "@";
  else if(channel->cflag[n] & MODE_VOICE)
    flag = "+";

  sprintf(text, "%s %s %s %s %s %c %s%s :%d %s",
	  channel->name, cl->user, cl->host, cl->server, cl->nick, 
	  cl->flag & UMODE_AWAY ? 'G' : 'H',
	  cl->flag & UMODE_OPERATOR ? "*" : "",
	  flag, 0, cl->real);
  
  irc_printf(sock, ":%s %03d %s %s\n",
	     cfg->host, RPL_WHOREPLY, client->nick, text);

}

/* 
 *         Command: WHO
 *      Parameters: [<name> [<o>]]
 * Numeric Replies: ERR_NOSUCHSERVER
 *                  RPL_WHOREPLY     RPL_ENDOFWHO
 */
int
irc_who_callback (socket_t sock, 
		  irc_client_t *client,
		  irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  irc_channel_t *channel;
  char *name;
  int n;

  if(!request->paras) name = "*";
  else                name = request->para[0];

  /* find all Matching channels */
  channel = NULL;
  while((channel = irc_regex_channel(channel, name)))
    {
      for(n=0; n<channel->clients; n++)
	{
	  cl = irc_find_nick(channel->client[n]);
	  send_client_infos(sock, client, cl, channel);
	}
      irc_printf(sock, ":%s %03d %s %s :%s\n",
		 cfg->host, RPL_ENDOFWHO, client->nick,
		 channel->name, "End of /WHO list.");
    }
  if(channel)
    return 0;
  
  /* find all Matching nicks */
  cl = NULL;
  while((cl = irc_regex_nick(cl, name)))
    {
      for(n=0; n<cl->channels; n++)
	{
	  channel = irc_find_channel(cl->channel[n]);
	  send_client_infos(sock, client, cl, channel);
	}
      irc_printf(sock, ":%s %03d %s %s :%s\n",
		 cfg->host, RPL_ENDOFWHO, client->nick,
		 cl->nick, "End of /WHO list.");
    }

  
  return 0;
}

#else /* not ENABLE_IRC_PROTO */

int irc_event_5_dummy; /* Shutup compiler warnings. */

#endif /* not ENABLE_IRC_PROTO */
