/*
 * irc-event-2.c - IRC events -- Channel operations
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
 * $Id: irc-event-2.c,v 1.2 2000/06/12 23:06:06 raimi Exp $
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

/*
 *         Command: PART
 *      Parameters: <channel>{,<channel>}
 * Numeric Replies: ERR_NEEDMOREPARAMS ERR_NOSUCHCHANNEL
 *                  ERR_NOTONCHANNEL
 */
int
irc_part_callback (socket_t sock, 
		   irc_client_t *client,
		   irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  irc_channel_t *channel;
  socket_t xsock;
  int n;

  /* do you have enough paras ? */
  if(check_paras(sock, client, cfg, request, 1))
    return 0;

  /* go through all targets in the first para */
  for(n=0; n<request->targets; n++)
    {
      /* does the channel exist ? */
      if(!(channel = irc_find_channel(request->target[n].channel)))
	{
	  irc_printf(sock, ":%s %03d %s " ERR_NOSUCHCHANNEL_TEXT "\n",
		     cfg->host, ERR_NOSUCHCHANNEL, client->nick,
		     request->target[n].channel);
	  return 0;
	}

      /* yes, look if the client is in it */
      if(is_client_in_channel(sock, client, channel) == -1)
	return 0;

      /* send back the PART to all channel clients */
      for(n=0; n<channel->clients; n++)
	{
	  cl = irc_find_nick(channel->client[n]);
	  xsock = find_sock_by_id(cl->id);
	  irc_printf(xsock, ":%s!%s@%s PART %s :%s\n",
		     client->nick, client->user, client->host, 
		     channel->name, request->para[1]);
	}
      
      del_client_of_channel (cfg, client, channel->name);
    }
  return 0;
}

/*
 * This function returns no zero value if a given client matches
 * a given channel ban entry.
 */
int
is_client_banned (irc_client_t *client, irc_ban_t *ban)
{
  /* does the nick Match ? */
  if(!string_regex(client->nick, ban->nick)) return 0;

  /* does the user Match ? */
  if(!string_regex(client->user, ban->user)) return 0;

  /* does the host Match ? */
  if(!string_regex(client->host, ban->host)) return 0;

  return -1;
}

/*
 * Send the channel topic to an IRC client.
 */
void
send_channel_topic (socket_t sock,
		    irc_client_t *client,
		    irc_channel_t *channel)
{
  irc_config_t *cfg = sock->cfg;

  /* send topic if there is one */
  if(channel->topic[0] != 0)
    {
      irc_printf(sock, ":%s %03d %s %s :%s\n",
		 cfg->host, RPL_TOPIC, client->nick,
		 channel->name, channel->topic);

      /* send topic date and nick (this is not part of the RFC) */
      irc_printf(sock, ":%s %03d %s %s %s %d\n",
		 cfg->host, RPL_TOPICSET, client->nick, channel->name, 
		 channel->topic_by, channel->topic_since);
    }
  /* no topic set yet */
  else
    {
      irc_printf(sock, ":%s %03d %s " RPL_NOTOPIC_TEXT "\n",
		 cfg->host, RPL_NOTOPIC, client->nick, channel->name);
    }
}

/*
 * Send a channels and its users to a specified client.
 */
void
send_channel_users (socket_t sock,
		    irc_client_t *client, 
		    irc_channel_t *channel)
{
  irc_config_t *cfg = sock->cfg;

  char nicklist[MAX_MSG_LEN];
  irc_client_t *cl;
  int n;

  /* prebuild the nicklist */
  for(nicklist[0]=0, n=0; n<channel->clients; n++)
    {
      /* check if the client is visible or not */
      cl = irc_find_nick(channel->client[n]);
      if(!(cl->flag & UMODE_INVISIBLE) &&
	 is_client_in_channel(NULL, client, channel) != -1)
	{
	  if(channel->cflag[n] & MODE_OPERATOR) 
	    strcat(nicklist, "@");
	  else if(channel->cflag[n] & MODE_VOICE) 
	    strcat(nicklist, "+");
	  strcat(nicklist, channel->client[n]);
	  strcat(nicklist, " ");
	}
    }

  /* send channel info */
  irc_printf(sock, ":%s %03d %s %c %s :%s\n",
	     cfg->host, RPL_NAMREPLY, client->nick,
	     channel->flag & (MODE_PRIVATE | MODE_SECRET) ? '*' : '=',
	     channel->name, nicklist);
}

/*
 *         Command: JOIN
 *      Parameters: <channel>{,<channel>} [<key>{,<key>}]
 * Numeric Replies: ERR_NEEDMOREPARAMS ERR_BANNEDFROMCHAN
 *                  ERR_INVITEONLYCHAN ERR_BADCHANNELKEY
 *                  ERR_CHANNELISFULL  ERR_BADCHANMASK
 *                  ERR_NOSUCHCHANNEL  ERR_TOOMANYCHANNELS
 *                  RPL_TOPIC
 */
int
irc_join_callback (socket_t sock, 
		   irc_client_t *client,
		   irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  irc_channel_t *channel;
  socket_t xsock;
  char *chan;
  int n, i;

  /* do you have enough paras ? */
  if(check_paras(sock, client, cfg, request, 1))
    return 0;

  /* go through all targets in the first para */
  for(n=0; n<request->targets; n++)
    {
      chan = request->target[n].channel;

      /* does the channel already exists ? */
      if((channel = irc_find_channel(chan)))
	{
	  /* is a key set for this channel and is the given one ok ? */
	  if(channel->flag & MODE_KEY &&
	     strcmp(get_para_target(request->para[1], n), channel->key))
	    {
	      irc_printf(sock, 
			 ":%s %03d %s " ERR_BADCHANNELKEY_TEXT "\n",
			 cfg->host, ERR_BADCHANNELKEY, client->nick,
			 channel->name);
	      return 0;
	    }
	  /* invite only ? */
	  if(channel->flag & MODE_INVITE)
	    {
	      /* find the nick in the ivite list of the channel */
	      for(i=0; i<channel->invites; i++)
		if(channel->invite[i] == client->nick)
		  break;

	      /* not in this list ! */
	      if(i == channel->invites)
		{
		  irc_printf(sock, 
			     ":%s %03d %s " ERR_INVITEONLYCHAN_TEXT "\n",
			     cfg->host, ERR_INVITEONLYCHAN, client->nick,
			     channel->name);
		  return 0;
		}
	      /* clear this invite entry */
	      else
		{
		  channel->invite[i] = channel->invite[channel->invites - 1];
		  channel->invites--;
		}
	    }
	  /* is channel full ? */
	  if(channel->flag & MODE_ULIMIT && 
	     channel->clients >= channel->users)
	    {
	      irc_printf(sock, 
			 ":%s %03d %s " ERR_CHANNELISFULL_TEXT "\n",
			 cfg->host, ERR_CHANNELISFULL, client->nick,
			 channel->name);
	      return 0;
	    }
	  /* is this client banned ? */
	  for(i=0; i<channel->bans; i++)
	    {
	      if(is_client_banned(client, channel->ban[i]))
		{
		  irc_printf(sock, 
			     ":%s %03d %s " ERR_BANNEDFROMCHAN_TEXT "\n",
			     cfg->host, ERR_BANNEDFROMCHAN, client->nick,
			     channel->name);
		  return 0;
		}
	    }
	}

      /* done, do not deny this channel ! */
      add_client_to_channel (cfg, client, chan);
      channel = irc_find_channel(chan);

      /* send back the JOIN to all channel clients */
      for(i=0; i<channel->clients; i++)
	{
	  cl = irc_find_nick(channel->client[i]);
	  xsock = find_sock_by_id(cl->id);
	  irc_printf(xsock, ":%s!%s@%s JOIN :%s\n",
		     client->nick, client->user, client->host, chan);
	}

      /* send topic */
      send_channel_topic (sock, client, channel);

      /* send creation date and nick (this is not part of the RFC) */
      irc_printf(sock, ":%s %03d %s %s %d\n",
		 cfg->host, RPL_CHANCREATED, client->nick,
		 channel->name, channel->since);

      /* send nick list */
      send_channel_users(sock, client, channel);
      irc_printf (sock, ":%s %03d %s %s %s\n",
		  cfg->host, RPL_ENDOFNAMES, client->nick,
		  channel->name, ":End of /NAMES list");
    }

  return 0;
}

/*
 * Get a ban Mask string by a channel's ban entry.
 */
char *
get_ban_string (irc_ban_t *ban)
{
  static char text[MAX_MSG_LEN] = "";

  if(ban->nick[0]) 
    {
      strcat(text, ban->nick);
      strcat(text, "!");
    }
  if(ban->user[0])
    {
      strcat(text, ban->user);
      strcat(text, "@");
    }
  strcat(text, ban->host);
  
  return text;
}

/*
 * Check if a given client is channel operator in its channel and
 * send an error about this if neccessary. The function returns a non
 * zero value if the client is an operator otherwise zero.
 */
int
is_client_operator (socket_t sock, 
		    irc_client_t *client,
		    irc_channel_t *channel,
		    int flag)
{
  irc_config_t *cfg = sock->cfg;

  if(!(flag & MODE_OPERATOR))
    {
      irc_printf(sock, ":%s %03d %s " ERR_CHANOPRIVSNEEDED_TEXT "\n",
		 cfg->host, ERR_CHANOPRIVSNEEDED, 
		 client->nick, channel->name);
      return 0;
    }
  return -1;
}

/*
 * Set or unset a channel flag for a user in the channel.
 */
int
set_client_flag (irc_client_t *client,   /* client changing the flag */
		 socket_t sock,          /* this clients connection */
		 int cflag,              /* this clients flags */
		 char *nick,             /* the nick the Mode is for */
		 irc_channel_t *channel, /* Mode change for this channel */
		 int flag,               /* flag to be set / unset */
		 char set)               /* set / unset */
{
  irc_client_t *cl;
  socket_t xsock;
  int i, n;
  unsigned l;
  static char *Modes = CHANNEL_MODES;
  char Mode = ' ';

  /* find Mode character */
  for(l=0; l<strlen(Modes); l++)
    if(flag & (1 << l))
      {
	Mode = Modes[l];
	break;
      }
  
  if(!is_client_operator(sock, client, channel, cflag))
    return 0;

  /* find nick in channel */
  for(i=0; i<channel->clients; i++)
    if(!strcmp(channel->client[i], nick))
      {
	if(set) channel->cflag[i] |= flag;
	else    channel->cflag[i] &= ~flag;
	break;
      }

  /* no such nick in channel ! */
  if(i == channel->clients)
    {
      irc_printf(sock, "%03d " ERR_NOSUCHNICK_TEXT "\n",
		 ERR_NOSUCHNICK, nick);
      return 0;
    }

  /* propagate Mode change to channel users */
  for(n=0; n<channel->clients; n++)
    {
      cl = irc_find_nick(channel->client[n]);
      xsock = find_sock_by_id(cl->id);
      irc_printf(xsock, ":%s!%s@%s MODE %s %c%c %s\n",
		 client->nick, client->user, client->host, channel->name, 
		 set ? '+' : '-', Mode, channel->client[i]);
    }

  return 0;
}

/*
 * Set or unset a flag for a user by itself.
 */
int
set_user_flag (irc_client_t *client,   /* client changing the flag */
	       socket_t sock,          /* this clients connection */
	       int flag,               /* flag to be set / unset */
	       char set)               /* set / unset */
{
  irc_config_t *cfg = sock->cfg;
  unsigned l;
  static char *Modes = USER_MODES;
  char Mode = ' ';

  /* find Mode character */
  for(l=0; l<strlen(Modes); l++)
    if(flag & (1 << l))
      {
	Mode = Modes[l];
	break;
      }
  
  /* is this client able to change the flag */
  if(set)
    {
      if(flag & UMODE_INVISIBLE && !(client->flag & UMODE_INVISIBLE))
	cfg->invisibles++;
      
      /* this flag cannot be set by anyone ! */
      if(!(flag & UMODE_OPERATOR)) 
	client->flag |= flag;
      else
	return 0;
    }
  else
    {
      if(flag & UMODE_INVISIBLE && client->flag & UMODE_INVISIBLE)
	cfg->invisibles--;

      client->flag &= ~flag;
    }

  /* propagate Mode change to this users */
  irc_printf(sock, ":%s!%s@%s MODE %s %c%c\n",
	     client->nick, client->user, client->host,
	     client->nick, set ? '+' : '-', Mode);
  
  return 0;
}

/*
 * Set or unset a channel flag.
 */
int
set_channel_flag (irc_client_t *client,   /* client changing the flag */
		  socket_t sock,          /* this clients connection */
		  int cflag,              /* this clients flags */
		  irc_channel_t *channel, /* Mode change for this channel */
		  int flag,               /* flag to be set / unset */
		  char set)               /* set / unset */
{
  irc_client_t *cl;
  socket_t xsock;
  int n;
  unsigned l;
  char *Modes = CHANNEL_MODES;
  char Mode = ' ';

  /* find Mode character */
  for(l=0; l<strlen(Modes); l++)
    if(flag & (1 << l))
      {
	Mode = Modes[l];
	break;
      }
  
  if(!is_client_operator(sock, client, channel, cflag))
    return 0;

  if(set) channel->flag |= flag;
  else    channel->flag &= ~flag;

  /* propagate Mode change to channel users */
  for(n=0; n<channel->clients; n++)
    {
      cl = irc_find_nick(channel->client[n]);
      xsock = find_sock_by_id(cl->id);
      irc_printf(xsock, ":%s!%s@%s MODE %s %c%c\n",
		 client->nick, client->user, client->host, channel->name, 
		 set ? '+' : '-', Mode);
    }

  return 0;
}

/*
 * This function creates a ban entry.
 */
irc_ban_t *
irc_create_ban (irc_client_t *client, char *request, int len)
{
  irc_ban_t *ban;
  char *p;
  int n, size = 0;

  /* reserve and initialize buffer space */
  ban = xmalloc(sizeof(irc_ban_t));
  memset(ban, 0, sizeof(irc_ban_t));
  ban->since = time(NULL);
  sprintf(ban->by, "%s!%s@%s", client->nick, client->user, client->host);

  p = request;

  n = 0;
  while(size < len && *p != '!' && *p != '@')
    {
      ban->nick[n] = *p;
      ban->host[n] = *p;
      ban->user[n] = *p;
      n++;
      size++;
      p++;
    }
  /* nick parsed */
  if(*p == '!')
    {
      memset(ban->host, 0, MAX_NAME_LEN);
      memset(ban->user, 0, MAX_NAME_LEN);
      p++;
      size++;
      n = 0;
      while(size < len && *p != '@')
	{
	  ban->user[n] = *p;
	  n++;
	  size++;
	  p++;
	}
      /* user parsed */
      if(*p == '@')
	{
	  p++;
	  size++;
	  n = 0;
	  while(size < len)
	    {
	      ban->host[n] = *p;
	      n++;
	      size++;
	      p++;
	    }
	}
    }
  /* here user parsed */
  else if(*p == '@')
    {
      memset(ban->nick, 0, MAX_NAME_LEN);
      memset(ban->host, 0, MAX_NAME_LEN);
      p++;
      size++;
      n = 0;
      while(size < len)
	{
	  ban->host[n] = *p;
	  n++;
	  size++;
	  p++;
	}
    }
  /* parsed just a host */
  else
    {
      memset(ban->nick, 0, MAX_NAME_LEN);
      memset(ban->user, 0, MAX_NAME_LEN);
    }

  return ban;
}

/*
 * Set or unset a channel specific flag by arg.
 */
int
set_channel_arg (irc_client_t *client,   /* client changing the flag */
		 socket_t sock,          /* this clients connection */
		 int cflag,              /* this clients flags */
		 irc_channel_t *channel, /* Mode change for this channel */
		 int flag,               /* flag to be set / unset */
		 char *arg,              /* the arg */
		 char set)               /* set / unset */
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  socket_t xsock;
  int n;
  unsigned l;
  char *Modes = CHANNEL_MODES;
  char Mode = ' ';
  char ban[MAX_NAME_LEN];

  /* enough paras ? */
  if(set && arg[0] == 0)
    {
      irc_printf(sock, ":%s %03d %s " ERR_NEEDMOREPARAMS_TEXT "\n",
		 cfg->host, ERR_NEEDMOREPARAMS, 
		 client->nick, "MODE");
      return 0;
    }

  /* find Mode character */
  for(l=0; l<strlen(Modes); l++)
    if(flag & (1 << l))
      {
	Mode = Modes[l];
	break;
      }
  
  if(!is_client_operator(sock, client, channel, cflag))
    return 0;

  /* set / unset the actual flag */
  if(set) 
    {
      switch(flag)
	{
	case MODE_KEY:
	  /* key is set before... */
	  if(channel->flag & flag)
	    {
	      irc_printf(sock, ":%s %03d %s " ERR_KEYSET_TEXT "\n",
			 cfg->host, ERR_KEYSET, client->nick,
			 channel->name);
	      return 0;
	    }
	  strcpy(channel->key, arg);
	  channel->flag |= flag;
	  break;
	case MODE_ULIMIT:
	  channel->users = atoi(arg);
	  channel->flag |= flag;
	  break;
	case MODE_BAN:
	  n = channel->bans;
	  channel->ban[n] = irc_create_ban(client, arg, strlen(arg));
	  channel->bans++;
	  break;
	}
    }
  /* unset flag */
  else
    {
      /* look if you can delete a ban */
      if(flag & MODE_BAN)
	{
	  for(n=0; n<channel->bans; n++)
	    {
	      sprintf(ban, "%s!%s@%s", channel->ban[n]->nick,
		      channel->ban[n]->user, channel->ban[n]->host);
	      if(!strcmp(arg, ban))
		{
		  channel->ban[n] = channel->ban[channel->bans - 1];
		  xfree(channel->ban[channel->bans - 1]);
		  channel->bans--;
		  break;
		}
	    }
	}
      else
	{
	  channel->flag &= ~flag;
	}
    }

  /* propagate Mode change to channel users */
  for(n=0; n<channel->clients; n++)
    {
      cl = irc_find_nick(channel->client[n]);
      xsock = find_sock_by_id(cl->id);
      irc_printf(xsock, ":%s!%s@%s MODE %s %c%c%s%s\n",
		 client->nick, client->user, client->host,
		 channel->name, 
		 set ? '+' : '-', Mode, 
		 set ? " " : "", set ? arg : "");
    }

  return 0;
}

/*
 * Get a Mode string by user Modes.
 */
char *
get_client_flags (irc_client_t *client)
{
  static char Mode[MAX_MSG_LEN];
  static char *Modes = USER_MODES;
  unsigned n, i;

  memset(Mode, 0, MAX_MSG_LEN);

  Mode[0] = '+';

  for(i=n=0; n<strlen(Modes); n++)
    {
      if(client->flag & (1 << n))
	Mode[++i] = Modes[n];
    }

  return Mode;
}

/*
 * Get a flag string by a channel's Modes.
 */
char *
get_channel_flags (irc_channel_t *channel)
{
  static char Mode[MAX_MODE_LEN];
  static char arg[MAX_MODE_LEN];
  static char *Modes = CHANNEL_MODES;
  unsigned n, i;

  memset(Mode, 0, MAX_MODE_LEN);
  Mode[0] = '+';

  /* go through all possible Mode flags (characters) */
  for(i=n=0; n<strlen(Modes); n++)
    {
      if(channel->flag & (1 << n))
	Mode[++i] = Modes[n];
    }

  /* add flag para if neccessary */
  if(channel->flag & MODE_ULIMIT)
    {
      sprintf(arg, " %d", channel->users);
      strcat(Mode, arg);
    }

  return Mode;
}

/* 
 *         Command: MODE
 *   Channel modes:
 *      Parameters: <channel> {[+|-]|o|p|s|i|t|n|b|v} [<limit>] [<user>]
 *                  [<ban mask>]
 *      User modes:
 *      Parameters: <nickname> {[+|-]|i|w|s|o}
 * Numeric Replies: ERR_NEEDMOREPARAMS   RPL_CHANNELMODEIS
 *                  ERR_CHANOPRIVSNEEDED ERR_NOSUCHNICK
 *                  ERR_NOTONCHANNEL     ERR_KEYSET
 *                  RPL_BANLIST          RPL_ENDOFBANLIST
 *                  ERR_UNKNOWNMODE      ERR_NOSUCHCHANNEL
 *                  ERR_USERSDONTMATCH   RPL_UMODEIS
 *                  ERR_UMODEUNKNOWNFLAG
 */

#define FLAG_SET   1
#define FLAG_UNSET 0

int
irc_mode_callback (socket_t sock, 
		   irc_client_t *client,
		   irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  irc_channel_t *channel;
  char *chan, *nick, *req;
  char flag;
  int cflag;
  int n, para;

  /* enough paras, but at least one ? */
  if(check_paras(sock, client, cfg, request, 1))
    return 0;

  chan = nick = request->para[0];

  /* is target channel ? */
  if((channel = irc_find_channel(chan)))
    {
      /* this is a request only ? */
      if(request->paras < 2)
	{
	  irc_printf(sock, ":%s %03d %s %s %s\n", cfg->host,
		     RPL_CHANNELMODEIS,  client->nick, channel->name,
		     get_channel_flags(channel));
	  return 0;
	}

      /* find client in the channel first */
      if((n = is_client_in_channel(sock, client, channel)) == -1)
	return 0;
      cflag = channel->cflag[n];

      /* go through all the Mode string and current arg is # 2 */
      req = request->para[1]; 
      para = 2;
      flag = FLAG_SET;

      while(*req)
	{
	  switch(*req)
	    {
	    case '+':
	      flag = FLAG_SET;
	      break;
	    case '-':
	      flag = FLAG_UNSET;
	      break;
	      /* set or get ban Mask */
	    case 'b':
	      /* if no args, then reply the bans */
	      if(para >= request->paras)
		{
		  for(n=0; n<channel->bans; n++)
		    {
		      irc_printf(sock, ":%s %03d %s %s %s %s %d\n", 
				 cfg->host,
				 RPL_BANLIST, client->nick, channel->name,
				 get_ban_string(channel->ban[n]),
				 channel->ban[n]->by,
				 channel->ban[n]->since);
		    }
		  irc_printf(sock, ":%s %03d %s " RPL_ENDOFBANLIST_TEXT "\n", 
			     cfg->host, RPL_ENDOFBANLIST, client->nick, 
			     channel->name);
		}
	      /* yes there are args, so set or unset the ban */
	      else
		{
		  set_channel_arg(client, sock, cflag, channel, MODE_BAN, 
				  request->para[para], flag);
		  para++;
		}
	      break;
	      /* channel operator flag */
	    case 'o':
	      set_client_flag(client, sock, cflag, request->para[para],
			      channel, MODE_OPERATOR, flag);
	      para++;
	      break;
	      /* voice flag for this channel */
	    case 'v':
	      set_client_flag(client, sock, cflag, request->para[para],
			      channel, MODE_VOICE, flag);
	      para++;
	      break;
	      /* private flag of this channel */
	    case 'p':
	      set_channel_flag(client, sock, cflag, 
			       channel, MODE_PRIVATE, flag);
	      break;
	      /* secret flag */
	    case 's':
	      set_channel_flag(client, sock, cflag, 
			       channel, MODE_SECRET, flag);
	      break;
	      /* invite only flag */
	    case 'i':
	      set_channel_flag(client, sock, cflag, 
			       channel, MODE_INVITE, flag);
	      break;
	      /* topic setable by operator only */
	    case 't':
	      set_channel_flag(client, sock, cflag, 
			       channel, MODE_TOPIC, flag);
	      break;
	      /* no Message of outside flag */
	    case 'n':
	      set_channel_flag(client, sock, cflag, 
			       channel, MODE_MESSAGE, flag);
	      break;
	      /* Moderated channel */
	    case 'm':
	      set_channel_flag(client, sock, cflag, 
			       channel, MODE_MODERATE, flag);
	      break;
	      /* set the users limitation flag (taking one arg) */
	    case 'l':
	      set_channel_arg(client, sock, cflag, channel, MODE_ULIMIT, 
			      request->para[para], flag);
	      if(flag) para++;
	      break;
	      /* set a key for this channel */
	    case 'k':
	      set_channel_arg(client, sock, cflag, channel, MODE_KEY, 
			      request->para[para], flag);
	      if(flag) para++;
	      break;
	      /* unknow Mode flag */
	    default:
	      irc_printf(sock, ":%s %03d %s " ERR_UNKNOWNMODE_TEXT "\n",
			 cfg->host, ERR_UNKNOWNMODE, client->nick, *req);
	      break;
	    }
	  req++;
	}
    }
  
  /* is target nick ? */
  else if((cl = irc_find_nick(nick)))
    {
      /* get and set user flag only by itself ! */
      if(cl != client)
	{
	  irc_printf(sock, ":%s %03d %s " ERR_USERSDONTMATCH_TEXT "\n",
		     cfg->host, ERR_USERSDONTMATCH, client->nick);
	  return 0;
	}
      /* this is a request only ? */
      if(request->paras < 2)
	{
	  irc_printf(sock, ":%s %03d %s %s %s\n", cfg->host,
		     RPL_UMODEIS,  client->nick, client->nick,
		     get_client_flags(client));
	  return 0;
	}

      /* go through all the Mode string */
      req = request->para[1]; 
      flag = FLAG_SET;

      while(*req)
	{
	  switch(*req)
	    {
	    case '+':
	      flag = FLAG_SET;
	      break;
	    case '-':
	      flag = FLAG_UNSET;
	      break;
	      /* invisible flag */
	    case 'i':
	      set_user_flag(client, sock, UMODE_INVISIBLE, flag);
	      break;
	      /* get server Messages */
	    case 's':
	      set_user_flag(client, sock, UMODE_SERVER, flag);
	      break;
	      /* get operator Messages */
	    case 'w':
	      set_user_flag(client, sock, UMODE_WALLOP, flag);
	      break;
	      /* just deop yourself ! */
	    case 'o':
	      set_user_flag(client, sock, UMODE_OPERATOR, flag);
	      break;
	      /* unknow Mode flag */
	    default:
	      irc_printf(sock, ":%s %03d %s " ERR_UNKNOWNMODE_TEXT "\n",
			 cfg->host, ERR_UNKNOWNMODE, client->nick, *req);
	      break;
	    }
	  req++;
	}
    }

  return 0;
}

/*
 *         Command: TOPIC
 *      Parameters: <channel> [<topic>]
 * Numeric Replies: ERR_NEEDMOREPARAMS   ERR_NOTONCHANNEL
 *                  RPL_NOTOPIC          RPL_TOPIC
 *                  ERR_CHANOPRIVSNEEDED
 */
int
irc_topic_callback (socket_t sock, 
		    irc_client_t *client,
		    irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  irc_channel_t *channel;
  socket_t xsock;
  int n, i;

  /* enough paras ? */
  if(check_paras(sock, client, cfg, request, 1))
    return 0;

  /* get the channel target */
  if((channel = irc_find_channel(request->target[0].channel)))
    {
      if((n = is_client_in_channel(sock, client, channel)) != -1)
	{
	  /* just send TOPIC back */
	  if(request->paras < 2)
	    {
	      send_channel_topic (sock, client, channel);
	    }
	  /* check if client can set the TOPIC */
	  else
	    {
	      if((channel->flag & MODE_TOPIC) &&
		 !is_client_operator(sock, client, channel, channel->cflag[n]))
		return 0;

	      /* change the topic */
	      strcpy(channel->topic, request->para[1]);
	      strcpy(channel->topic_by, client->nick);
	      channel->topic_since = time(NULL);

	      /* send topic to all clients in channel */
	      for(i=0; i<channel->clients; i++)
		{
		  cl = irc_find_nick(channel->client[i]);
		  xsock = find_sock_by_id(cl->id);
		  irc_printf(xsock, ":%s!%s@%s TOPIC %s :%s\n",
			     client->nick, client->user, client->host, 
			     channel->name, channel->topic);
		}
	    }
	}
    }
  return 0;
}

/*
 *    Command: NAMES
 * Parameters: [<channel>{,<channel>}]
 *   Numerics: RPL_NAMREPLY  RPL_ENDOFNAMES
 */
int
irc_names_callback (socket_t sock, 
		    irc_client_t *client,
		    irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  irc_channel_t *ch;
  static char text[MAX_MSG_LEN];
  int n;

  /* list all channels and users ? */
  if(request->paras < 1)
    {
      /* go through all channels */
      for(ch = irc_channel_list; ch; ch = ch->next)
	{
	  /* 
	   * you can't see secret and private channels, 
	   * except you are in it
	   */
	  if(is_client_in_channel(NULL, client, ch) == -1 &&
	     (ch->flag & (MODE_SECRET | MODE_PRIVATE)))
	    continue;

	  /* send a line */
	  send_channel_users (sock, client, ch);
	}

      /* 
       * List all users not being in channels at all,
       * being visible, and are not in public channels.
       * Public channels are not secret or private or channels
       * clients share.
       */
      for(text[0] = 0, cl = irc_client_list; cl; cl = cl->next)
	{
	  /* visible ? */
	  if(!(cl->flag & UMODE_INVISIBLE))
	    {
	      /* go through all channels of the client */
	      for(n=0; n<cl->channels; n++)
		{
		  ch = irc_find_channel(cl->channel[n]);
		  /* is it public or a shared channel ? */
		  if(!(ch->flag & (MODE_SECRET | MODE_PRIVATE)) ||
		     is_client_in_channel(NULL, client, ch) != -1)
		    break;
		}
	      /* is the client in no channel or in no shared or public ? */
	      if(n == cl->channels)
		{
		  if(cl->flag & UMODE_OPERATOR)
		    strcat(text, "@");
		  strcat(text, cl->nick);
		  strcat(text, " ");
		}
	    }
	}
      /* send the (*) reply */
      irc_printf(sock, ":%s %03d %s * * :%s\n",
		 cfg->host, RPL_NAMREPLY, client->nick, text);

      /* send endo of list */
      irc_printf(sock, ":%s %03d %s " RPL_ENDOFNAMES_TEXT "\n",
		 cfg->host, RPL_ENDOFNAMES, client->nick, "");
    }
  /* list only specified channels */
  else
    {
      for(n=0; n<request->targets; n++)
	{
	  if((ch = irc_find_channel(request->target[n].channel)))
	    {
	      if(is_client_in_channel(NULL, client, ch) == -1 &&
		 (ch->flag & (MODE_SECRET | MODE_PRIVATE)))
		continue;
	      send_channel_users(sock, client, ch);
	    }
	  irc_printf(sock, ":%s %03d %s " RPL_ENDOFNAMES_TEXT "\n",
		     cfg->host, RPL_ENDOFNAMES, client->nick, 
		     ch->name);
	}
    }

  return 0;
}

/*
 * Send a part of a channel list.
 */
void
send_channel_list (socket_t sock, 
		   irc_client_t *client,
		   irc_channel_t *channel)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  int n;
  int visibles;

  /* hide secret channels */
  if(channel->flag & MODE_SECRET)
    return;

  /* hide private channels you're not in */
  if(channel->flag & MODE_PRIVATE &&
     is_client_in_channel(NULL, client, channel) == -1)
    return;

  /* count visible clients in the channel */
  for(visibles=0, n=0; n<channel->clients; n++)
    {
      cl = irc_find_nick(channel->client[n]);
      if(!(cl->flag & UMODE_INVISIBLE))
	visibles++;
    }

  /* send channel info */
  irc_printf(sock, ":%s %03d %s %s" RPL_LIST_TEXT "\n", 
	     cfg->host, RPL_LIST, client->nick, 
	     channel->flag & MODE_PRIVATE ? "* " : "",
	     channel->name, visibles, channel->topic);

}

/* 
 *         Command: LIST
 *      Parameters: [<channel>{,<channel>} [<server>]]
 * Numeric Replies: ERR_NOSUCHSERVER RPL_LISTSTART
 *                  RPL_LIST         RPL_LISTEND
 */
int
irc_list_callback (socket_t sock, 
		   irc_client_t *client,
		   irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_channel_t *ch;
  int n;

  irc_printf(sock, ":%s %03d %s " RPL_LISTSTART_TEXT "\n", 
	     cfg->host, RPL_LISTSTART, client->nick);

  /* list all channels */
  if(request->paras < 1)
    {
      for(ch = irc_channel_list; ch; ch=ch->next)
	{
	  send_channel_list (sock, client, ch);
	}
    }
  /* list specified channels */
  else
    {
      for(n=0; n<request->targets; n++)
	{
	  if((ch = irc_find_channel(request->target[n].channel)))
	    {
	      send_channel_list (sock, client, ch);
	    }
	}
    }

  irc_printf(sock, ":%s %03d " RPL_LISTEND_TEXT "\n", 
	     cfg->host, RPL_LISTEND);

  return 0;
}

/*
 *         Command: INVITE
 *      Parameters: <nickname> <channel>
 * Numeric Replies: ERR_NEEDMOREPARAMS   ERR_NOSUCHNICK
 *                  ERR_NOTONCHANNEL     ERR_USERONCHANNEL
 *                  ERR_CHANOPRIVSNEEDED
 *                  RPL_INVITING         RPL_AWAY
 */
int
irc_invite_callback (socket_t sock, 
		     irc_client_t *client, 
		     irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  irc_channel_t *ch;
  socket_t xsock;
  char *nick, *channel;
  int n, i;

  /* enough paras ? */
  if(check_paras(sock, client, cfg, request, 2))
    return 0;

  nick = request->para[0];
  channel = request->para[1];

  /* does the nick exist ? */
  if(!(cl = irc_find_nick(nick)))
    {
      irc_printf(sock, ":%s %03d %s " ERR_NOSUCHNICK_TEXT "\n",
		 cfg->host, ERR_NOSUCHNICK, client->nick, nick);
      return 0;
    }
  /* does the channel exist ? */
  if(!(ch = irc_find_channel(channel)))
    {
      irc_printf(sock, ":%s %03d %s " ERR_NOSUCHNICK_TEXT "\n",
		 cfg->host, ERR_NOSUCHNICK, client->nick, channel);
      return 0;
    }

  /* is the inviter in channel ? */
  if((n = is_client_in_channel(sock, client, ch)) == -1)
    return 0;

  /* is the user already in channel ? */
  if((i = is_client_in_channel(NULL, cl, ch)) != -1)
    {
      irc_printf(sock, ":%s %03d %s " ERR_USERONCHANNEL_TEXT "\n",
		 cfg->host, ERR_USERONCHANNEL, client->nick,
		 nick, channel);
      return 0;
    }
  /* are you operator in this channel / can you invite ? */
  if(!is_client_operator(sock, client, ch, ch->cflag[n]))
    return 0;

  /* is this client away ? */
  if(is_client_absent(cl, client)) 
    return 0;

  /* send the invite Message */
  xsock = find_sock_by_id(cl->id);
  irc_printf(xsock, ":%s!%s@%s INVITE %s :%s\n",
	     client->nick, client->user, client->host, cl->nick, ch->name);

  /* fill in the invited nick into the channel */
  n = ch->invites;
  ch->invite[n] = cl->nick;
  ch->invites++;

  return 0;
}

/*
 *         Command: KICK
 *      Parameters: <channel> <user> [<comment>]
 * Numeric Replies: ERR_NEEDMOREPARAMS  ERR_NOSUCHCHANNEL
 *                  ERR_BADCHANMASK     ERR_CHANOPRIVSNEEDED
 *                  ERR_NOTONCHANNEL
 */
int
irc_kick_callback (socket_t sock, 
		   irc_client_t *client,
		   irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  irc_client_t *victim;
  irc_channel_t *channel;
  socket_t xsock;
  int i, n;

  /* enough paras ? */
  if(check_paras(sock, client, cfg, request, 2))
    return 0;

  /* go through all targets (channels) */
  for(n=0; n<request->targets; n++)
    {
      /* does the requested channel exist ? */
      if(!(channel = irc_find_channel(request->target[n].channel)))
	{
	  irc_printf(sock, ":%s %03d %s " ERR_NOSUCHCHANNEL_TEXT "\n",
		     cfg->host, ERR_NOSUCHCHANNEL, client->nick,
		     request->target[n].channel);
	  continue;
	}

      /* yes there is such a channel, are you in it ? */
      if((i = is_client_in_channel(sock, client, channel)) == -1)
	continue;

      /* are you a channel operator ? */
      if(!is_client_operator(sock, client, channel, channel->cflag[i]))
	continue;
      
      /* kick the requested user */
      if(!(victim = irc_find_nick(get_para_target(request->para[1], n))))
	continue;
      
      if(is_client_in_channel(NULL, victim, channel) == -1)
	continue;

      for(i=0; i<channel->clients; i++)
	{
	  cl = irc_find_nick(channel->client[i]);

	  xsock = find_sock_by_id(cl->id);
	  irc_printf(xsock, ":%s!%s@%s KICK %s %s :%s\n",
		     client->nick, client->user, 
		     client->host, channel->name, victim->nick,
		     request->para[2]);
	}
      
      del_client_of_channel (cfg, victim, channel->name);
    }

  return 0;
}

#else /* not ENABLE_IRC_PROTO */

int irc_event_2_dummy; /* Shutup compiler warnings. */

#endif /* not ENABLE_IRC_PROTO */
