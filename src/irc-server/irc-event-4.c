/*
 * irc-event-4.c - IRC events -- Sending messages
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Portions (C) 1995, 1996 Free Software Foundation, Inc.
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
#include "util.h"
#include "server-core.h"
#include "irc-core/irc-core.h"
#include "irc-proto.h"
#include "irc-crypt.h"
#include "irc-event.h"

/*
 *         Command: PRIVMSG
 *      Parameters: <receiver>{,<receiver>} <text to be sent>
 * Numeric Replies: ERR_NORECIPIENT      ERR_NOTEXTTOSEND
 *                  ERR_CANNOTSENDTOCHAN ERR_NOTOPLEVEL
 *                  ERR_WILDTOPLEVEL     ERR_TOOMANYTARGETS
 *                  ERR_NOSUCHNICK
 *                  RPL_AWAY
 */
int
irc_priv_callback (socket_t sock, 
		   irc_client_t *client,
		   irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  irc_channel_t *channel;
  socket_t xsock;
  static char text[MAX_MSG_LEN];
  int n, i;

  /* enough paras ? */
  if(check_paras(sock, client, cfg, request, 2))
    return 0;

  /* concate the text itself */
  for(text[0]=0, n=1; n<request->paras; n++)
    strcat(text, request->para[n]);

  /* no text ? */
  if(!text[0])
    {
      irc_printf(sock, ":%s %03d %s " ERR_NOTEXTTOSEND_TEXT "\n",
		 cfg->host, ERR_NOTEXTTOSEND, client->nick);
      return 0;
    }

  /* go through all targets (receivers) */
  for(n=0; n<request->targets; n++)
    {
      /* is receiver nick ? */
      if((cl = irc_find_nick(request->target[n].nick)))
	{
	  /* is this client away ? */
	  if(is_client_absent(cl, client)) continue;

	  /* crypted ? */
	  if(client->flag & UMODE_PASS)
	    irc_encrypt_text(text, client->key);

	  xsock = find_sock_by_id(cl->id);
	  irc_printf(xsock, ":%s!%s@%s PRIVMSG %s :%s\n",
		     client->nick, client->user, client->host,
		     cl->nick, (cl->flag & UMODE_PASS) ?
		     irc_decrypt_text(text, cl->key) : text);
	}

      /* is receiver a channel ? */
      else if((channel = irc_find_channel(request->target[n].channel)))
	{
	  i = is_client_in_channel(sock, client, channel);
	  
	  /* no Messages of outside at these channels ! */
	  if(channel->flag & MODE_MESSAGE && i == -1)
	    {
	      irc_printf(sock, 
			 ":%s %03d %s " ERR_CANNOTSENDTOCHAN_TEXT "\n",
			 cfg->host, ERR_CANNOTSENDTOCHAN, client->nick,
			 channel->name);
	      continue;
	    }

	  /* do you have voice in a Moderated channel ? */
	  if(channel->flag & MODE_MODERATE &&
	     !(channel->cflag[i] & (MODE_VOICE | MODE_OPERATOR)))
	    {
	      irc_printf(sock, 
			 ":%s %03d %s " ERR_CANNOTSENDTOCHAN_TEXT "\n",
			 cfg->host, ERR_CANNOTSENDTOCHAN, client->nick,
			 channel->name);
	      continue;
	    }
	  
	  /* crypted ? */
	  if(client->flag & UMODE_PASS)
	    irc_encrypt_text(text, client->key);

	  /* tell all clients in this channel about */
	  for(i=0; i<channel->clients; i++)
	    {
	      cl = irc_find_nick(channel->client[i]);
	      if(cl->nick != client->nick)
		{
		  xsock = find_sock_by_id(cl->id);
		  irc_printf(xsock, ":%s!%s@%s PRIVMSG %s :%s\n",
			     client->nick, client->user, client->host,
			     channel->name, (cl->flag & UMODE_PASS) ?
			     irc_decrypt_text(text, cl->key) : text);
		}
	    }
	}
      /* no real target found */
      else
	{
	  if(request->target[n].channel[0])
	    sprintf(text, "%s", request->target[n].channel);
	  else if(request->target[n].nick[0])
	    sprintf(text, "%s", request->target[n].nick);
	  else if(request->target[n].mask[0])
	    sprintf(text, "%s", request->target[n].nick);
	  else
	    sprintf(text, "%s@%s", request->target[n].user,
		    request->target[n].host);

	  irc_printf(sock, ":%s %03d %s " ERR_NOSUCHNICK_TEXT "\n",
		     cfg->host, ERR_NOSUCHNICK, client->nick, text);
	}
    }
  return 0;
}

/*
 *         Command: NOTICE
 *      Parameters: <nickname> <text>
 * Numeric Replies: ERR_NORECIPIENT      ERR_NOTEXTTOSEND
 *                  ERR_CANNOTSENDTOCHAN ERR_NOTOPLEVEL
 *                  ERR_WILDTOPLEVEL     ERR_TOOMANYTARGETS
 *                  ERR_NOSUCHNICK
 *                  RPL_AWAY
 */
int
irc_note_callback (socket_t sock, 
		   irc_client_t *client,
		   irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  socket_t xsock;
  static char text[MAX_MSG_LEN];
  int n;

  /* enough paras ? */
  if(check_paras(sock, client, cfg, request, 2))
    return 0;
  
  /* concate the text */
  for(text[0]=0, n=1; n<request->paras; n++)
    strcat(text, request->para[n]);
  
  /* no text ? */
  if(!text[0])
    {
      irc_printf(sock, ":%s %03d %s " ERR_NOTEXTTOSEND_TEXT "\n",
		 cfg->host, ERR_NOTEXTTOSEND, client->nick);
      return 0;
    }

  /* is target a nick ? */
  if((cl = irc_find_nick(request->target[0].nick)))
     {
       /* is this client away ? */
       if(is_client_absent(cl, client)) return 0;

       /* crypted ? */
       if(client->flag & UMODE_PASS) irc_encrypt_text(text, client->key);

       if(cl->flag & UMODE_PASS) irc_decrypt_text(text, cl->key);

       xsock = find_sock_by_id(cl->id);
       irc_printf(xsock, ":%s!%s@%s NOTICE %s :%s\n",
		  client->nick, client->user, client->host,
		  cl->nick, text);
     }

  return 0;
}

#else /* not ENABLE_IRC_PROTO */

int irc_event_4_dummy; /* Shutup compiler warnings. */

#endif /* not ENABLE_IRC_PROTO */
