/*
 * irc-event-1.c - IRC events -- Connection Registration
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
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "socket.h"
#include "alloc.h"
#include "util.h"
#include "server-core.h"
#include "serveez.h"
#include "irc-core/irc-core.h"
#include "irc-proto.h"
#include "irc-crypt.h"
#include "irc-event.h"

/*
 *    Command: QUIT
 * Parameters: [<Quit message>]
 */
int
irc_quit_callback (socket_t sock, 
		   irc_client_t *client,
		   irc_request_t *request)
{
  /* delete the client */
  del_client_of_channels(sock->cfg, client, request->para[0]);

  return -1;
}

/*
 *         Command: PASS
 *      Parameters: <password>
 * Numeric Replies: ERR_NEEDMOREPARAMS ERR_ALREADYREGISTRED
 */
int
irc_pass_callback (socket_t sock, 
		   irc_client_t *client,
		   irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;

  /* enough paras ? reject the client if not given */
  if(check_paras(sock, client, cfg, request, 1))
    return -1;

  strcpy(client->pass, request->para[0]);
  client->key = irc_gen_key(client->pass);
  client->flag |= UMODE_PASS;

  /* check it ! */
  if (cfg->pass)
    {
      if (strcmp (request->para[0], cfg->pass))
	{
	  irc_delete_client (cfg, sock->socket_id);
	  return -1;
	}
    }

  return 0;
}

/*
 * Send the inital Messages.
 */
void
send_init_block (socket_t sock, 
		 irc_client_t *client)
{
  irc_config_t *cfg = sock->cfg;

  /* send initial Messages */
  irc_printf(sock, ":%s %03d %s :" RPL_WELCOME_TEXT "\n",
	     cfg->host, RPL_WELCOME, client->nick, client->nick);

  irc_printf(sock, ":%s %03d %s :" RPL_YOURHOST_TEXT "\n",
	     cfg->host, RPL_YOURHOST, client->nick,
	     cfg->host, serveez_config.program_name,
	     serveez_config.version_string);

  irc_printf(sock, "NOTICE %s :*** " RPL_YOURHOST_TEXT "\n",
	     client->nick, cfg->host, serveez_config.program_name,
	     serveez_config.version_string);
  
  irc_printf(sock, ":%s %03d %s :" RPL_CREATED_TEXT "\n",
	     cfg->host, RPL_CREATED, client->nick, 
	     serveez_config.build_string);

  irc_printf(sock, ":%s %03d %s " RPL_MYINFO_TEXT "\n",
	     cfg->host, RPL_MYINFO, client->nick,
	     cfg->host, serveez_config.program_name,
	     serveez_config.version_string, USER_MODES, CHANNEL_MODES);

  /* send LUSER* replies */
  irc_lusers_callback (sock, client, NULL);

  /* send the "Message of the Day" */
  irc_motd_callback (sock, client, NULL);
}

/*
 * This function extracts a valid nick of a given text. It returns
 * the length of it, otherwise zero.
 */
int
irc_get_nick (char *nick)
{
  char *p;
  int n;

  p = nick;

  if((*p >= '0' && *p <= '9') || *p == '-')
    return 0;
  
  for(n=0; *p && n<MAX_NICK_LEN; n++, p++)
    if(!((*p >= 'A' && *p <= '~') ||  (*p >= '0' && *p <= '9') || 
	 (*p == '_') || (*p == '-')))
      break;
  *p = 0;

  return p - nick;
}

/*
 *         Command: NICK
 *      Parameters: <nickname> [ <hopcount> ]
 * Numeric Replies: ERR_NONICKNAMEGIVEN ERR_ERRONEUSNICKNAME
 *                  ERR_NICKNAMEINUSE   ERR_NICKCOLLISION
 */
int
irc_nick_callback (socket_t sock, 
		   irc_client_t *client,
		   irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *cl;
  irc_channel_t *channel;
  socket_t xsock;
  char *nick;
  int n, i;

  /* enough para's ? */
  if(request->paras < 1)
    {
      irc_printf(sock, ":%s %03d " ERR_NONICKNAMEGIVEN_TEXT "\n", 
		 cfg->host, ERR_NONICKNAMEGIVEN);
      return 0;
    }

  /* is the given nick valid ? */
  nick = request->para[0];
  if(!irc_get_nick(nick))
    {
      irc_printf(sock, ":%s %03d * " ERR_ERRONEUSNICKNAME_TEXT "\n", 
		 cfg->host, ERR_ERRONEUSNICKNAME, nick);
      return 0;
    }

  /* nick already in use ? */
  if((cl = irc_find_nick(nick)))
    {
      /* did the client tried to change to equal nicks ? then ignore */
      if(cl->id == client->id) return 0;
#if ENABLE_DEBUG
      log_printf(LOG_DEBUG, "irc: nick %s is already in use\n", cl->nick);
#endif
      irc_printf(sock, ":%s %03d * " ERR_NICKNAMEINUSE_TEXT "\n", 
		 cfg->host, ERR_NICKNAMEINUSE, cl->nick);
      return 0;
    }

  /* do you have already specified a valid nick ? */
  if(client->flag & UMODE_NICK)
    {
      irc_add_client_history(client);

#if ENABLE_DEBUG
      log_printf(LOG_DEBUG, "irc: %s changed nick to %s\n", 
		 client->nick, nick);
#endif
      /* go through all channels this client is in */
      for(n=0; n<client->channels; n++)
	{
	  /* propagate this to all clients in channel */
	  channel = irc_find_channel(client->channel[n]);
	  for(i=0; i<channel->clients; i++)
	    {
	      cl = irc_find_nick(channel->client[i]);
	      xsock = find_sock_by_id(cl->id);
	      irc_printf(xsock, ":%s!%s@%s NICK :%s\n",
			 client->nick, client->user, client->host, nick);
	    }
	}
      strcpy(client->nick, nick);
    }
  /* this is the first nick you specified ! send init block */
  else
    {
      strcpy (client->nick, nick);
      send_init_block (sock, client);
      client->flag |= UMODE_NICK;
    }

  return 0;
}

/*
 *         Command: USER
 *      Parameters: <username> <hostname> <servername> <realname>
 * Numeric Replies: ERR_NEEDMOREPARAMS ERR_ALREADYREGISTRED
 */
int
irc_user_callback (socket_t sock, 
		   irc_client_t *client,
		   irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;

  /* complete parameter list ? */
  if(check_paras(sock, client, cfg, request, 4))
    return 0;
  
  /* is this client already fully registered ? */
  if((client->flag & UMODE_REGISTERED) == UMODE_REGISTERED)
    {
      irc_printf(sock, ":%s %03d %s " ERR_ALREADYREGISTRED_TEXT "\n", 
		 cfg->host, ERR_ALREADYREGISTRED, client->nick);
      return 0;
    }

  /* store paras in client structure if not done by AUTH-callbacks */
  if(client->user[0] == 0) sprintf(client->user, "~%s", request->para[0]);
  if(client->host[0] == 0) sprintf(client->host, "%s", request->para[1]);
  if(client->server[0] == 0) sprintf(client->server, "%s", request->para[2]);
  strcpy(client->real, request->para[3]);
  client->flag |= UMODE_USER;

  return 0;
}

/*
 *         Command: MOTD
 *      Parameters: 
 * Numeric Replies: ERR_NOMOTD    RPL_MOTDSTART
 *                  RPL_ENDOFMOTD RPL_MOTD
 */
int
irc_motd_callback (socket_t sock, 
		   irc_client_t *client,
		   irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  FILE *f;
  struct stat buf;
  int n;

  /* try requesting the file */
  if(stat(cfg->MOTD_file, &buf) == -1)
    {
      log_printf(LOG_ERROR, "irc: /MOTD error: %s\n", SYS_ERROR);
      irc_printf(sock, ":%s %03d %s " ERR_NOMOTD_TEXT "\n",
		 cfg->host, ERR_NOMOTD, client->nick);
      return 0;
    }

  /* has the file been changed ? then read it */
  if(cfg->MOTD_lastModified <  buf.st_mtime)
    {
      cfg->MOTD_lastModified =  buf.st_mtime;
      if((f = fopen(cfg->MOTD_file, "r")) == NULL)
	{
	  log_printf(LOG_ERROR, "irc: /MOTD error: %s\n", SYS_ERROR);
	  return 0;
	}

      /* free the previous MOTD content */
      for(n=0; n<cfg->MOTDs; n++)
	xfree(cfg->MOTD[n]);

      /* read every line (restrict line length) */
      n = 0;
      cfg->MOTD[n] = xmalloc(MOTD_LINE_LEN);
      while(fgets(cfg->MOTD[n], MOTD_LINE_LEN, f) && n < MAX_MOTD_LINES)
	{
	  cfg->MOTD[n][strlen(cfg->MOTD[n]) - 1] = '\0';
	  n++;
	  cfg->MOTD[n] = xmalloc(MOTD_LINE_LEN);
	}
      xfree(cfg->MOTD[n]);
      cfg->MOTDs = n;
      fclose(f);
    }
  
  /* send the "Message of the Day" if neccessary */
  if(cfg->MOTDs)
    {
      /* start */
      irc_printf(sock, 
		 "NOTICE %s :*** The MOTD file was last modified at %s",
		 client->nick, ctime(&cfg->MOTD_lastModified));

      irc_printf(sock, ":%s %03d %s " RPL_MOTDSTART_TEXT "\n",
		 cfg->host, RPL_MOTDSTART, client->nick, cfg->host);

      /* go through all lines */
      for(n=0; n<cfg->MOTDs; n++)
	{
	  irc_printf(sock, ":%s %03d %s " RPL_MOTD_TEXT "\n",
		     cfg->host, RPL_MOTD, client->nick, cfg->MOTD[n]);
	}

      /* end */
      irc_printf(sock, ":%s %03d %s " RPL_ENDOFMOTD_TEXT "\n",
		 cfg->host, RPL_ENDOFMOTD, client->nick, cfg->host);
    }

  return 0;
}

#else /* not ENABLE_IRC_PROTO */

int irc_event_1_dummy; /* Shutup compiler warnings. */

#endif /* not ENABLE_IRC_PROTO */
