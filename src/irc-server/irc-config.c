/*
 * irc-config.c - IRC server configuration routines
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
 * $Id: irc-config.c,v 1.1 2000/07/19 14:12:34 ela Exp $
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

#include "alloc.h"
#include "util.h"
#include "irc-proto.h"
#include "irc-server.h"
#include "irc-config.h"

/*
 * This routine will parse all of the x-lines of the IRC server
 * configuration and store the information within the apropiate
 * lists.
 */

#define MAX_TMP_ARRAY  6
#define MAX_TMP_STRLEN 256

void
irc_parse_config_lines (irc_config_t *cfg)
{
  int n;
  irc_class_t *class;
  irc_user_t *user;
  irc_oper_t *oper;
  irc_kill_t *kill;
  char *line;
  char *tmp[MAX_TMP_ARRAY];
  
  /* reserve some buffer space */
  for (n = 0; n < MAX_TMP_ARRAY; n++)
    tmp[n] = xmalloc (MAX_TMP_STRLEN);

  /* parse connection classes */
  if (cfg->YLine)
    {
      for (n = 0, line = cfg->YLine[n]; line; line = cfg->YLine[++n])
	{
	  class = xmalloc (sizeof (irc_class_t));
	  irc_parse_line (line, "Y:%d:%d:%d:%d:%d",
			  &class->nr, &class->ping_freq, 
			  &class->connect_freq, &class->links, 
			  &class->sendq_size);
	  class->line = line;
	  class->next = cfg->classes;
	  cfg->classes = class;
	}
    }

  /* parse user authorization lines */
  if (cfg->ILine)
    {
      for (n = 0, line = cfg->ILine[n]; line; line = cfg->ILine[++n])
	{
	  user = xmalloc (sizeof (irc_user_t));
	  irc_parse_line (line, "I:%s@%s:%s:%s@%s:%s:%d",
			  tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5],
			  &user->class);
	  user->line = line;
	  user->user_ip = xstrdup (tmp[0]);
	  user->ip = xstrdup (tmp[1]);
	  user->password = xstrdup (tmp[2]);
	  user->user_host = xstrdup (tmp[3]);
	  user->host = xstrdup (tmp[4]);
	  if (!user->password)
	    user->password = xstrdup (tmp[5]);
	  user->next = cfg->user_auth;
	  cfg->user_auth = user;
	}
    }

  /* parse operator authorization lines (local and netwotk wide) */
  if (cfg->OLine)
    {
      for (n = 0, line = cfg->OLine[n]; line; line = cfg->OLine[++n])
	{
	  oper = xmalloc (sizeof (irc_oper_t));
	  irc_parse_line (line, "O:%s@%s:%s:%s::%d",
			  tmp[0], tmp[1], tmp[2], tmp[3],
			  &oper->class);
	  oper->line = line;
	  oper->user = xstrdup (tmp[0]);
	  oper->host = xstrdup (tmp[1]);
	  oper->password = xstrdup (tmp[2]);
	  oper->nick = xstrdup (tmp[3]);
	  oper->local = 0;
	  oper->next = cfg->operator_auth;
	  cfg->operator_auth = oper;
	}
    }

  if (cfg->oLine)
    {
      for (n = 0, line = cfg->oLine[n]; line; line = cfg->oLine[++n])
	{
	  oper = xmalloc (sizeof (irc_oper_t));
	  irc_parse_line (line, "O:%s@%s:%s:%s::%d",
			  tmp[0], tmp[1], tmp[2], tmp[3],
			  &oper->class);
	  oper->line = line;
	  oper->user = xstrdup (tmp[0]);
	  oper->host = xstrdup (tmp[1]);
	  oper->password = xstrdup (tmp[2]);
	  oper->nick = xstrdup (tmp[3]);
	  oper->local = 1;
	  oper->next = cfg->operator_auth;
	  cfg->operator_auth = oper;
	}
    }

  /* parse banned clients */
  if (cfg->KLine)
    {
      for (n = 0, line = cfg->KLine[n]; line; line = cfg->KLine[++n])
	{
	  kill = xmalloc (sizeof (irc_kill_t));
	  irc_parse_line (line, "O:%s:%d-%d:%s",
			  tmp[0], &kill->start, &kill->end,
			  tmp[1]);
	  kill->line = line;
	  kill->host = xstrdup (tmp[0]);
	  kill->user = xstrdup (tmp[1]);
	  kill->next = cfg->banned;
	  cfg->banned = kill;
	}
    }

  /* free the previously allocated buffer space */
  for (n = 0; n < MAX_TMP_ARRAY; n++)
    xfree (tmp[n]);
}

/*
 * Free all the stuff reserved in the routine above.
 */
void
irc_free_config_lines (irc_config_t *cfg)
{
  irc_user_t *user;
  irc_class_t *class;
  irc_oper_t *oper;
  irc_kill_t *kill;

  /* free connection classes */
  while ((class = cfg->classes) != NULL)
    {
      cfg->classes = class->next;
      xfree (class);
    }

  /* free user authorization list */
  while ((user = cfg->user_auth) != NULL)
    {
      cfg->user_auth = user->next;
      if (user->user_ip) xfree (user->user_ip);
      if (user->ip) xfree (user->ip);
      if (user->user_host) xfree (user->user_host);
      if (user->host) xfree (user->host);
      if (user->password) xfree (user->password);
      xfree (user);
    }

  /* free operator authorization list */
  while ((oper = cfg->operator_auth) != NULL)
    {
      cfg->operator_auth = oper->next;
      if (oper->nick) xfree (oper->nick);
      if (oper->user) xfree (oper->user);
      if (oper->host) xfree (oper->host);
      if (oper->password) xfree (oper->password);
      xfree (oper);
    }

  /* free banned user list */
  while ((kill = cfg->banned) != NULL)
    {
      cfg->banned = kill->next;
      if (kill->user) xfree (kill->user);
      if (kill->host) xfree (kill->host);
      xfree (kill);
    }
}

/*
 * This routine checks whether a given client is wanted to connect.
 * If it is not return zero, otherwise non-zero.
 */
int
irc_client_valid (irc_client_t *client, irc_config_t *cfg)
{
  irc_user_t *user;

  for (user = cfg->user_auth; user; user = user->next)
    {
      if ((irc_string_regex (client->user, user->user_ip) &&
	   irc_string_regex (util_inet_ntoa (client->sock->remote_addr), 
			     user->ip)) ||
	  (irc_string_regex (client->user, user->user_host) &&
	   irc_string_regex (client->host, user->host)))
	{
	  if (user->password)
	    if (strcmp (user->password, client->pass))
	      continue;
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "irc: valid client: %s\n", user->line);
#endif
	  return 1;
	}
    }
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "irc: not a valid client (%s@%s)\n",
	      client->user, client->host);
#endif

  return 0;
}

#else /* ENABLE_IRC_PROTO */

int irc_config_dummy;

#endif /* ENABLE_IRC_PROTO */
