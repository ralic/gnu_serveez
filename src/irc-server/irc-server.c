/*
 * irc-server.c - IRC server connection routine
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
 * $Id: irc-server.c,v 1.3 2000/06/12 23:06:06 raimi Exp $
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
#include <errno.h>

#ifdef __MINGW32__
# include <winsock.h>
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif

#include "alloc.h"
#include "util.h"
#include "socket.h"
#include "snprintf.h"
#include "irc-proto.h"
#include "irc-event.h"
#include "irc-server.h"
#include "connect.h"
#include "coserver/coserver.h"

#define DEFAULT_PORT 6667

irc_server_t  *irc_server_list;  /* server list root */

#define MAX_HOST_LEN 256
#define MAX_PASS_LEN 256
#define MAX_NAME_LEN 256

/*
 * Parse one of the config lines in the IRC configuration.
 * This function has exactly the same syntax as sscanf() but
 * recognizes only %s and %d for string and integers. Strings
 * will be parsed until the next character in the format string.
 */
int
irc_parse_line(char *line, char *fmt, ...)
{
  va_list args;
  int *i;
  char *s;
  int ret;

  va_start(args, fmt);
  ret = 0;

  while(*fmt && *line)
    {
      /* next arg */
      if(*fmt == '%')
	{
	  fmt++;

	  /* a decimal */
	  if(*fmt == 'd')
	    {
	      i = va_arg(args, int *);
	      *i = 0;
	      fmt++;
	      while(*line && *line >= '0' && *line <= '9')
		{
		  *i *= 10;
		  *i += *line - '0';
		  line++;
		}
	    }
	  /* a string */
	  else if(*fmt == 's')
	    {
	      s = va_arg(args, char *);
	      fmt++;
	      while(*line && *line != *fmt)
		{
		  *s = *line;
		  s++;
		  line++;
		}
	      *s = 0;
	    }
	  ret++;
	}
      /* not an arg */
      else if(*fmt != *line)
	{
	  break;
	}
      fmt++;
      line++;
    }

  va_end(args);
  return ret;
}

/*
 * Go through all C lines in the IRC server configuration
 * and resolve all hosts.
 */
void
irc_resolve_cline (irc_config_t *cfg)
{
  char realhost[MAX_HOST_LEN];
  char pass[MAX_PASS_LEN];
  char host[MAX_NAME_LEN];
  int port;
  int class;
  unsigned *addr;
  struct hostent *server_host;
  irc_server_t *ircserver;
  char *cline;
  int n;
  char request[MAX_HOST_LEN];

  /* any C lines at all ? */
  if (!cfg->CLine) return;

  /* go through all C lines */
  n = 0;
  while ((cline = cfg->CLine[n++]) != NULL)
    {
      /* scan the actual C line */
      irc_parse_line (cline, "C:%s:%s:%s:%d:%d", 
		      realhost, pass, host, &port, &class);
      
#if ENABLE_DNS_LOOKUP_1
      sprintf (request, "%s\n", realhost);
      send_coserver_request (COSERVER_DNS, request);
#endif

      /* find the host by its name */
      if ((server_host = gethostbyname (realhost)) == NULL)
	{
	  log_printf (LOG_ERROR, "gethostbyname: %s\n", H_NET_ERROR);
	  log_printf (LOG_ERROR, "cannot find %s\n", realhost);
	  continue;
	}

      /* create new IRC server structure */
      ircserver = xmalloc(sizeof(irc_server_t));

      /* get the inet address in network byte order */
      if(server_host->h_addrtype == AF_INET &&
	 server_host->h_length == 4)
	{
	  ircserver->addr = UINT32 (server_host->h_addr_list[0]);
	}

      ircserver->port = htons(port);
      ircserver->id = -1;
      ircserver->realhost = xmalloc(strlen(realhost) + 1);
      strcpy(ircserver->realhost, realhost);
      ircserver->host = xmalloc(strlen(host) + 1);
      strcpy(ircserver->host, host);
      ircserver->pass = xmalloc(strlen(pass) + 1);
      strcpy(ircserver->pass, pass);
      ircserver->next = NULL;

      /* add this server to the server list */
      log_printf(LOG_NOTICE, "irc: resolved %s\n", ircserver->realhost);
      irc_add_server(ircserver);
    }
}

/*
 * Connect to a remote IRC server. Return non-zero on errors.
 */
int
irc_connect_server(char *host, irc_config_t *cfg)
{
  socket_t sock;
  irc_server_t *server;
  irc_client_t *cl;
  irc_channel_t *ch;
  char nicklist[MAX_MSG_LEN];
  int n;

  server = irc_server_list;

  while(server)
    {
      if(!strcmp(host, server->host) && server->port)
	{
	  if(!(sock = sock_connect(server->addr, server->port)))
	    {
	      return -1;
	    }
	  log_printf(LOG_NOTICE, "irc: connecting to %s\n", server->realhost);
	  server->id = sock->socket_id;
	  sock->flags |= SOCK_FLAG_IRC_SERVER;
	  sock->check_request = irc_check_request;

	  /* send initial requests introducing this server */
#ifndef ENABLE_TIMESTAMP
	  irc_printf(sock, "PASS %s\n", server->pass);
#else
	  irc_printf(sock, "PASS %s %s\n", server->pass, TS_PASS);
#endif
	  irc_printf(sock, "SERVER %s 1 :%s\n", cfg->host, cfg->info);

#if ENABLE_TIMESTAMP
	  irc_printf(sock, "SVINFO %d %d %d :%d\n",
		     TS_CURRENT, TS_MIN, 0, time(NULL) + cfg->tsdelta);
#endif

	  /* now propagate user information to this server */
	  for(cl = irc_client_list; cl; cl = cl->next)
	    {
#if ENABLE_TIMESTAMP
	      irc_printf(sock, "NICK %s %d %d %s %s %s %s :%s\n",
			 cl->nick, cl->hopcount, cl->since, 
			 get_client_flags(cl), cl->user, cl->host,
			 cl->server, "EFNet?");
#else
	      irc_printf(sock, "NICK %s\n", cl->nick);
	      irc_printf(sock, "USER %s %s %s %s\n", 
			 cl->user, cl->host, cl->server, cl->real);
	      irc_printf(sock, "MODE %s %s\n", 
			 cl->nick, get_client_flags(cl));
#endif
	    }

	  /* propagate all channel information to the server */
	  for(ch = irc_channel_list; ch; ch = ch->next)
	    {
#if ENABLE_TIMESTAMP
	      /* create nick list */
	      for(nicklist[0]=0, n=0; n<ch->clients; n++)
		{
		  if(ch->cflag[n] & MODE_OPERATOR)
		    strcat(nicklist, "@");
		  else if(ch->cflag[n] & MODE_VOICE)
		    strcat(nicklist, "+");
		  strcat(nicklist, ch->client[n]);
		  strcat(nicklist, " ");
		}
	      /* propagate one channel in one request */
	      irc_printf(sock, "SJOIN %d %s %s :%s\n",
			 ch->since, ch->name, get_channel_flags(ch),
			 nicklist);
#else
	      for(n=0; n<ch->clients; n++)
		{
		  irc_printf(sock, ":%s JOIN %s\n", ch->client[n], ch->name);
		}
	      irc_printf(sock, "MODE %s %s\n", 
			 ch->name, get_channel_flags(ch));
#endif
	    }

	  return 0;
	}
      server = server->next;
    }

  return -1;
}

/*
 * This function goes through all N lines and tries to connect to
 * all IRC servers.
 */
void
irc_connect_servers(void)
{
  irc_server_t *server;
  
  server = irc_server_list;

  while(server)
    {
      irc_connect_server(server->host, &irc_config);
      server = server->next;
    }
}

/*
 * Add an IRC server to the server list.
 */
irc_server_t *
irc_add_server(irc_server_t *server)
{
  server->next = irc_server_list;
  irc_server_list = server;
    
  return irc_server_list;
}

/*
 * Delete all IRC servers.
 */
void
irc_delete_servers(void)
{
  while(irc_server_list)
    {
      irc_del_server(irc_server_list);
    }
}

/*
 * Delete an IRC server of the current list.
 */
void
irc_del_server(irc_server_t *server)
{
  irc_server_t *srv;
  irc_server_t *prev;

  prev = srv = irc_server_list;
  while(srv)
    {
      if(srv == server)
	{
	  xfree(server->realhost);
	  xfree(server->host);
	  xfree(server->pass);
	  if(prev == srv)
	    irc_server_list = server->next;
	  else
	    prev->next = server->next;
	  xfree(server);
	  return;
	}
      prev = srv;
      srv = srv->next;
    }
}

/*
 * Find an IRC server in the current list.
 */
irc_server_t *
irc_find_server(void)
{
  return NULL;
}

#else /* not ENABLE_IRC_PROTO */

int irc_server_dummy;

#endif /* ENABLE_IRC_PROTO */
