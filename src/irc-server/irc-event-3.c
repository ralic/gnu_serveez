/*
 * irc-event-3.c - IRC events -- Server queries and commands
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
 * $Id: irc-event-3.c,v 1.2 2000/06/12 23:06:06 raimi Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_IRC_PROTO

#define _GNU_SOURCE
#include <string.h>
#include <time.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "socket.h"
#include "util.h"
#include "server-core.h"
#include "serveez.h"
#include "irc-core/irc-core.h"
#include "irc-proto.h"
#include "irc-event.h"

/* 
 *         Command: ADMIN
 *      Parameters: [<server>]
 * Numeric Replies: ERR_NOSUCHSERVER
 *                  RPL_ADMINME       RPL_ADMINLOC1
 *                  RPL_ADMINLOC2     RPL_ADMINEMAIL
 */
int
irc_admin_callback (socket_t sock, 
		    irc_client_t *client,
		    irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;

  /* server para is given */
  if(request->paras > 1)
    {
      /* check if the server para is valid */
      if(strcmp(request->para[1], cfg->host))
	{
	  irc_printf(sock, ":%s %03d %s " ERR_NOSUCHSERVER_TEXT "\n",
		     cfg->host, ERR_NOSUCHSERVER, client->nick,
		     request->para[1]);
	  return 0;
	}
    }
  
  irc_printf(sock, ":%s %03d %s " RPL_ADMINME_TEXT "\n",
	     cfg->host, RPL_ADMINME, client->nick, 
	     cfg->host, cfg->admininfo);
  irc_printf(sock, ":%s %03d %s " RPL_ADMINLOC1_TEXT "\n",
	     cfg->host, RPL_ADMINLOC1, client->nick, 
	     cfg->location1);
  irc_printf(sock, ":%s %03d %s " RPL_ADMINLOC2_TEXT "\n",
	     cfg->host, RPL_ADMINLOC2, client->nick, 
	     cfg->location2);
  irc_printf(sock, ":%s %03d %s " RPL_ADMINEMAIL_TEXT "\n",
	     cfg->host, RPL_ADMINEMAIL, client->nick, 
	     cfg->email);
  
  return 0;
}
/*
 *         Command: TIME
 *      Parameters: [<server>]
 * Numeric Replies: ERR_NOSUCHSERVER RPL_TIME
 */
int
irc_time_callback (socket_t sock, 
		   irc_client_t *client,
		   irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  time_t t;

  /* server para is given */
  if(request->paras > 1)
    {
      /* check if the server para is valid */
      if(strcmp(request->para[1], cfg->host))
	{
	  irc_printf(sock, ":%s %03d %s " ERR_NOSUCHSERVER_TEXT "\n",
		     cfg->host, ERR_NOSUCHSERVER, client->nick,
		     request->para[1]);
	  return 0;
	}
    }
  
  /* reply the local time */
  t = time(NULL);
  irc_printf(sock, ":%s %03d %s " RPL_TIME_TEXT,
	     cfg->host, RPL_TIME, client->nick, 
	     cfg->host, ctime(&t));

  return 0;
}

/*
 *         Command: LUSERS
 *      Parameters: 
 * Numeric Replies: RPL_LUSERCLIENT  RPL_LUSEROP
 *                  RPL_LUSERUNKNOWN RPL_LUSERCHANNELS
 *                  RPL_LUSERME
 */
int
irc_lusers_callback (socket_t sock, 
		     irc_client_t *client,
		     irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;

  /* send LUSER* replies */
  irc_printf(sock, ":%s %03d %s " RPL_LUSERCLIENT_TEXT "\n",
	     cfg->host, RPL_LUSERCLIENT, client->nick,
	     cfg->users, cfg->invisibles, cfg->servers);

  irc_printf(sock, ":%s %03d %s " RPL_LUSEROP_TEXT "\n",
	     cfg->host, RPL_LUSEROP, client->nick,
	     cfg->operators);

  /* This will end up in a non standard welcome message !
  irc_printf(sock, ":%s %03d %s " RPL_LUSERUNKNOWN_TEXT "\n",
  cfg->host, RPL_LUSERUNKNOWN, client->nick,
  cfg->unknowns);
  */

  irc_printf(sock, ":%s %03d %s " RPL_LUSERCHANNELS_TEXT "\n",
	     cfg->host, RPL_LUSERCHANNELS, client->nick,
	     cfg->channels);

  irc_printf(sock, ":%s %03d %s " RPL_LUSERME_TEXT "\n",
	     cfg->host, RPL_LUSERME, client->nick,
	     cfg->clients, cfg->servers);
  
  return 0;
}

/*
 *         Command: STATS
 *      Parameters: [<query> [<server>]]
 * Numeric Replies: ERR_NOSUCHSERVER
 *                  RPL_STATSCLINE     RPL_STATSNLINE
 *                  RPL_STATSILINE     RPL_STATSKLINE
 *                  RPL_STATSQLINE     RPL_STATSLLINE
 *                  RPL_STATSLINKINFO  RPL_STATSUPTIME
 *                  RPL_STATSCOMMANDS  RPL_STATSOLINE
 *                  RPL_STATSHLINE     RPL_ENDOFSTATS
 */
int
irc_stats_callback (socket_t sock, 
		    irc_client_t *client,
		    irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;
  char stat;
  time_t t, sec, hour, min, day;

  /* no paras given */
  if(!request->paras)
    {
      irc_printf(sock, ":%s %03d %s " RPL_ENDOFSTATS_TEXT "\n",
		 cfg->host, RPL_ENDOFSTATS, client->nick, ' ');
      return 0;
    }
  
  /* server para is given */
  if(request->paras > 1)
    {
      /* check if the server para is valid */
      if(strcmp(request->para[1], cfg->host))
	{
	  irc_printf(sock, ":%s %03d %s " ERR_NOSUCHSERVER_TEXT "\n",
		     cfg->host, ERR_NOSUCHSERVER, client->nick,
		     request->para[1]);
	  return 0;
	}
    }
  
  stat = request->para[0][0];
  switch(stat)
    { 
      /* 
       * c - servers which the server may connect to or allow 
       *     connections from
       */
    case 'c':
      break;
      /* 
       * h - returns a list of servers which are either forced to be
       *     treated as leaves or allowed to act as hubs
       */
    case 'h':
      break;
      /*
       * i - returns a list of hosts which the server allows a client
       *     to connect from
       */
    case 'i':
      break;
      /* 
       * k - returns a list of banned username/hostname combinations
       *     for that server
       */
    case 'k':
      break;
      /*
       * l - returns a list of the server's connections, showing how
       *     long each connection has been established and the traffic
       *     over that connection in bytes and messages for each
       *     direction
       */
    case 'l':
      break;
      /*
       * m - returns a list of commands supported by the server and
       *     the usage count for each if the usage count is non zero
       */
    case 'm':
      break;
      /*
       * o - returns a list of hosts from which normal clients may
       *     become operators
       */
    case 'o':
      break;
      /*
       * y - show Y (Class) lines from server's configuration file
       */
    case 'y':
      break;
      /*
       * u - returns a string showing how long the server has been up
       */
    case 'u':
      t = time(NULL) - serveez_config.start_time;
      sec = t % 60;
      t -= sec;
      min = t % 60;
      t -= min * 60;
      hour = t % 3600;
      t -= hour * 3600;
      day = t / (3600 * 24);
      irc_printf(sock, ":%s %03d %s " RPL_STATSUPTIME_TEXT "\n",
		 cfg->host, RPL_STATSUPTIME, client->nick, 
		 day, hour, min, sec);
      break;
    default:
      break;
    }

  irc_printf(sock, ":%s %03d %s " RPL_ENDOFSTATS_TEXT "\n",
	     cfg->host, RPL_ENDOFSTATS, client->nick, stat);
  return 0;
}

/*
 *         Command: VERSION
 *      Parameters: [<server>]
 * Numeric Replies: ERR_NOSUCHSERVER RPL_VERSION
 */
int
irc_version_callback (socket_t sock, 
		      irc_client_t *client,
		      irc_request_t *request)
{
  irc_config_t *cfg = sock->cfg;

  /* no paras */
  if(!request->paras)
    {
      irc_printf(sock, ":%s %03d %s " RPL_VERSION_TEXT "\n",
		 cfg->host, RPL_VERSION, client->nick,
		 serveez_config.version_string, 
		 serveez_config.program_name);
    }
  return 0;
}
		     
#else /* not ENABLE_IRC_PROTO */

int irc_event_3_dummy; /* Shutup compiler warnings. */

#endif /* not ENABLE_IRC_PROTO */
