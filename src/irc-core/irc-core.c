/*
 * irc-core.c - IRC core protocol functions
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
 * $Id: irc-core.c,v 1.5 2000/06/18 16:25:19 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_IRC_PROTO

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "socket.h"
#include "util.h"
#include "coserver/coserver.h"
#include "irc-core.h"
#include "irc-server/irc-proto.h"

irc_request_t irc_request; /* single IRC request */
char irc_lcset[256];       /* lower case character set */

#if ENABLE_REVERSE_LOOKUP
/*
 * Gets called when a nslookup coserver has resolved a IP address
 * for socket SOCK.
 */
int
irc_nslookup_done (socket_t sock, char *host)
{
  irc_client_t *client = sock->data;

  if (host)
    {
      client->flag |= UMODE_DNS;
      strcpy (client->host, host);
      irc_printf (sock, "NOTICE AUTH :%s\n", IRC_DNS_DONE);
      return 0;
    }
  return -1;
}
#endif /* ENABLE_REVERSE_LOOKUP */

#if ENABLE_IDENT
/*
 * Gets called when an ident coserver has got a reply
 * for socket SOCK.
 */
int
irc_ident_done (socket_t sock, char *user)
{
  irc_client_t *client = sock->data;

  if (user)
    {
      client->flag |= UMODE_IDENT;
      strcpy (client->user, user);
      irc_printf (sock, "NOTICE AUTH :%s\n", IRC_IDENT_DONE);
      return 0;
    }
  return -1;
}
#endif

/*
 * Initialization of the authentification (DNS and IDENT) for an
 * IRC client.
 */
void
irc_start_auth (socket_t sock)
{
  irc_config_t *cfg = sock->cfg;
  irc_client_t *client;
      
  /* 
   * Create and initialize a local IRC client ! This is not yet within the
   * actual client hash. 
   */
  client = irc_create_client (cfg);
  strcpy (client->server, cfg->host);
  client->since = time (NULL);
  sock->data = client;
  client->id = sock->socket_id;

  /* Set password flag, if there is not server password defined. */
  if (!cfg->pass) 
    client->flag |= UMODE_PASS;

  /* Start here the nslookup and ident lookup. */
  coserver_reverse (sock->remote_addr, 
		    (coserver_handle_result_t) irc_nslookup_done, sock);
  irc_printf (sock, "NOTICE AUTH :" IRC_DNS_INIT "\n");
      
  coserver_ident (sock, (coserver_handle_result_t) irc_ident_done, sock);
  irc_printf (sock, "NOTICE AUTH :" IRC_IDENT_INIT "\n");
}

/*
 * Detection routine for the IRC protocol. Returns no-zero if an
 * IRC connection has been detected. Otherwise zero.
 */
int
irc_detect_proto (void *cfg, socket_t sock)
{
  int ret = 0;

  if (sock->recv_buffer_fill >= 1 && 
      sock->recv_buffer[0] == ':')
    {
      ret = 1;
    }
  else if (sock->recv_buffer_fill >= 4 && 
	   (!memcmp (sock->recv_buffer, "PASS", 4) ||
	    !memcmp (sock->recv_buffer, "NICK", 4) ||
	    !memcmp (sock->recv_buffer, "USER", 4)))
    {
      ret = 4;
    }

  if (ret)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "irc protocol detected\n");
#endif
      return -1;
    }
  
  return 0;
}

/*
 * When a client connection has been identified by IRC_DETECT_PROTO
 * this routine is called to setup this socket for an IRC connection.
 */
int
irc_connect_socket (void *cfg, socket_t sock)
{
  sock->check_request = irc_check_request;
  sock->disconnected_socket = irc_disconnect;
  sock->idle_func = irc_idle;
  sock->idle_counter = IRC_PING_INTERVAL;
  irc_start_auth (sock);

  return 0;
}

/*
 * The CHECK_REQUEST looks through the receive buffer of the 
 * IRC connection for complete messages and calls then the
 * HANDLE_REQUEST function.
 */
int
irc_check_request (socket_t sock)
{
  int retval = 0;
  int request_len = 0;
  char *p, *packet;
  unsigned char *a;

  p = sock->recv_buffer;
  packet = p;

  do
    {
      while (p < sock->recv_buffer + sock->recv_buffer_fill && *p != '\n')
        p++;

      if (*p == '\n' && p < sock->recv_buffer + sock->recv_buffer_fill)
        {
          p++;
          request_len += (p - packet);
	  
#if ENABLE_DEBUG > 1
	  printf ("irc packet (%03d): '", p - packet - 1);
	  for(a = (unsigned char *)packet; (char *)a < p; a++)
	    {
	      if (*a >= ' ' && *a < 128) 
		printf ("%c", *a);
	      else 
		printf ("{0x%02x}", *a);
	    }
	  printf ("'\n");
#endif

          retval = irc_handle_request (sock, packet, 
				       p - packet - ((*(p-2) == '\r')?2:1));
          packet = p;
        }
    }
  while (p < sock->recv_buffer + sock->recv_buffer_fill && !retval);
  
  if (request_len > 0 && request_len < sock->recv_buffer_fill)
    {
      memmove (sock->recv_buffer, packet,
	       sock->recv_buffer_fill - request_len);
    }
  sock->recv_buffer_fill -= request_len;

  return retval;
}

/*
 * Parse the 'no'th string by a given para. 
 * These should be all seperated by ','.
 */
char *
get_para_target (char *para, int no)
{
  static char target[MAX_NAME_LEN];
  char *p;
  int n;

  p = para;
  for (n = 0; *p && n < no; n++)
    while(*p && *p != ',') p++;
  
  /* got a key (first or any ',' seperated) */
  if (*p == ',' || p == para)
    {
      n = 0;
      if (*p == ',') p++;
      while (*p && *p != ',')
	{
	  target[n] = *p;
	  n++;
	  p++;
	}
      target[n+1] = 0;
    }
  else
    target[0] = 0;

  return target;
}

/*
 * This routine parses a complete IRC message and fills in
 * all the information into the request structure.
 */
int
irc_parse_request (char *request, int len)
{
  char *p;
  int n, i;
  int size = 0;

  memset (&irc_request, 0, sizeof (irc_request_t));
  
  p = request;

  /* parse message origin if neccessary */
  if (*p == ':')
    {
      n = 0;
      /* get server or nick */
      while (*p != '!' && *p != '@' && *p != ' ' && size < len) 
	{
	  irc_request.server[n] = *p;
	  irc_request.nick[n] = *p;
	  size++;
	  n++;
	  p++;
	}
      /* user follows */
      if (*p == '!')
	{
	  n = 0;
	  p++;
	  size++;
	  while (*p != '@' && *p != ' ' && size < len) 
	    {
	      irc_request.user[n] = *p;
	      size++;
	      n++;
	      p++;
	    }
	}
      /* host follows */
      if (*p == '@')
	{
	  n = 0;
	  p++;
	  size++;
	  while (*p != ' ' && size < len) 
	    {
	      irc_request.host[n] = *p;
	      size++;
	      n++;
	      p++;
	    }
	}
      /* skip whitespace(s) */
      while (*p == ' ' && size < len)
	{
	  size++;
	  p++;
	}
    }

  /* no message origin, command follow */
  n = 0;
  while (*p != ' ' && size < len)
    {
      irc_request.request[n] = *p;
      size++;
      n++;
      p++;
    }
  /* get parameter(s) */
  i = 0;
  while (size < len)
    {
      /* skip whitespace(s) */
      while (*p == ' ' && size < len) 
	{
	  size++;
	  p++;
	}
      if (size == len) break;
      
      /* get next parameter */
      n = 0;
      
      /* trailing parameter ? */
      if (*p == ':')
	{
	  p++;
	  size++;
	  while (size < len)
	    {
	      irc_request.para[i][n] = *p;
	      size++;
	      n++;
	      p++;
	    }
	}
      
      /* normal parameter */
      else
	{
	  while (*p != ' ' && size < len)
	    {
	      irc_request.para[i][n] = *p;
	      size++;
	      n++;
	      p++;
	    }
	}
      i++;
    }

  if (i > 0 && irc_request.para[i-1][0] == 0) i--;
  irc_request.paras = i;
  irc_parse_target (&irc_request, 0);
  
  return 0;
}

/*
 * This function parses a one of the give request's paras
 * and stores all targets in its targets.
 */
void
irc_parse_target (irc_request_t *request, int para)
{
  int i, size, n, len;
  char *p;
  
  request->targets = 0;

  /* is there a para ? */
  if (request->paras <= para)
    return;

  /* yes, start parsing */
  i = 0;
  size = 0;
  p = request->para[para];
  len = strlen(request->para[para]);

  while (size < len)
    {
      /* local channel */
      if (*p == '&')
	{
	  n = 0;
	  while (*p != ',' && size < len)
	    {
	      request->target[i].channel[n] = *p;
	      p++;
	      size++;
	      n++;
	    }
	}
      /* mask */
      else if (*p == '$')
	{
	  n = 0;
	  while (*p != ',' && size < len)
	    {
	      request->target[i].mask[n] = *p;
	      p++;
	      size++;
	      n++;
	    }
	}
      /* channel or mask */
      else if (*p == '#')
	{
	  n = 0;
	  while (*p != ',' && size < len)
	    {
	      request->target[i].mask[n] = *p;
	      request->target[i].channel[n] = *p;
	      p++;
	      size++;
	      n++;
	    }
	}
      /* nick or user@host */
      else
	{
	  n = 0;
	  while (*p != ',' && *p != '@' && size < len)
	    {
	      request->target[i].user[n] = *p;
	      request->target[i].nick[n] = *p;
	      p++;
	      size++;
	      n++;
	    }
	  /* host */
	  if (*p == '@')
	    {
	      p++;
	      size++;
	      n = 0;
	      memset (request->target[i].nick, 0, MAX_NICK_LEN);
	      while (*p != ',' && size < len)
		{
		  request->target[i].host[n] = *p;
		  p++;
		  size++;
		  n++;
		}
	    }
	}
      if (*p == ',')
	{
	  size++;
	  p++;
	}
      i++;
    }
  request->targets = i;
}

/*
 * This routine just tries to Match 2 strings. It returns non zero
 * if it does.
 */
int
string_regex (char *text, char *regex)
{
  char *p;

  /* parse until end of both strings */
  while (*regex && *text)
    {
      /* find end of strings or '?' or '*' */
      while (*regex != '*' && *regex != '?' && 
	     *regex && *text)
	{
	  /* return no Match if so */
	  if (irc_lcset[(unsigned)*text] != irc_lcset[(unsigned)*regex])
	    return 0;
	  text++;
	  regex++;
	}
      /* one free character */
      if (*regex == '?')
	{
	  if (!(*text)) return 0;
	  text++;
	}
      /* free characters */
      else if (*regex == '*')
	{
	  /* skip useless '?'s after '*'s */
	  while (*(regex+1) == '?') regex++;
	  /* skip all characters until next character in pattern found */
	  while (*text && 
		 irc_lcset[(unsigned)*(regex+1)] != 
		 irc_lcset[(unsigned)*text]) 
	    text++;
	  /* next character in pattern found */
	  if (*text)
	    {
	      /* find the last occurence of this character in the text */
	      p = text + strlen (text);
	      while (irc_lcset[(unsigned)*p] != irc_lcset[(unsigned)*text]) 
		p--;
	      /* continue parsing at this character */
	      text = p;
	    }
	}
      regex++;
    }

  /* is the text longer than the regex ? */
  if (!*text && !*regex) return -1;
  return 0;
}

/*
 * Create the lowercase character set for string compares.
 */
void
irc_create_lcset (void)
{
  int n;
  
  for (n = 0; n < 256; n++)
    {
      irc_lcset[n] = (char) tolower (n);
    }
  irc_lcset['['] = '{';
  irc_lcset[']'] = '}';
  irc_lcset['|'] = '\\';
}

/*
 * Make a case insensitive string compare. Return 0 if both
 * strings are equal.
 */
int
string_equal (char *str1, char *str2)
{
  char *p1, *p2;

  if (str1 == str2) return 0;
  
  p1 = str1;
  p2 = str2;

  while (*p1 && *p2)
    {
      if (irc_lcset[(unsigned)*p1] != irc_lcset[(unsigned)*p2]) 
	return -1;
      p1++;
      p2++;
    }

  if (!*p1 && !*p2) return 0;
  return -1;
}

#else

int irc_core_dummy; /* shut up compiler */

#endif /* ENABLE_IRC_PROTO */
