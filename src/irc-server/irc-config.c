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
 * $Id: irc-config.c,v 1.2 2000/07/19 20:07:08 ela Exp $
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

#define MAX_TMP_ARRAY  4
#define MAX_TMP_STRLEN 256
#define PARSE_TILL_AT(str) \
  p = str; \
  while (*p && *p != '@') p++; \
  if (*p) { *p = '\0'; p++; }

void
irc_parse_config_lines (irc_config_t *cfg)
{
  int n;
  irc_class_t *class;
  irc_user_t *user;
  irc_oper_t *oper;
  irc_kill_t *kill;
  char *line, *p;
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
	  if (5 != irc_parse_line (line, "Y:%d:%d:%d:%d:%d",
				   &class->nr, &class->ping_freq, 
				   &class->connect_freq, &class->max_links, 
				   &class->sendq_size))
	    {
	      log_printf (LOG_ERROR, "irc: invalid Y line: %s\n", line);
	      xfree (class);
	    }
	  else
	    {
	      class->links = 0;
	      class->line = line;
	      class->next = cfg->classes;
	      cfg->classes = class;
	    }
	}
    }

  /* parse user authorization lines */
  if (cfg->ILine)
    {
      for (n = 0, line = cfg->ILine[n]; line; line = cfg->ILine[++n])
	{
	  user = xmalloc (sizeof (irc_user_t));
	  if (5 != irc_parse_line (line, "I:%s:%s:%s:%s:%d",
				   tmp[0], tmp[1], tmp[2], tmp[3],
				   &user->class))
	    {
	      log_printf (LOG_ERROR, "irc: invalid I line: %s\n", line);
	      xfree (user);
	    }
	  else
	    {
	      user->line = line;
	      PARSE_TILL_AT (tmp[0]);
	      user->user_ip = xstrdup (tmp[0]);
	      user->ip = xstrdup (p);
	      user->password = xstrdup (tmp[1]);
	      PARSE_TILL_AT (tmp[2]);
	      user->user_host = xstrdup (tmp[2]);
	      user->host = xstrdup (p);
	      if (!user->password)
		user->password = xstrdup (tmp[3]);
	      user->next = cfg->user_auth;
	      cfg->user_auth = user;
	    }
	}
    }

  /* parse operator authorization lines (local and netwotk wide) */
  if (cfg->OLine)
    {
      for (n = 0, line = cfg->OLine[n]; line; line = cfg->OLine[++n])
	{
	  oper = xmalloc (sizeof (irc_oper_t));
	  if (4 != irc_parse_line (line, "O:%s:%s:%s::%d",
				   tmp[0], tmp[1], tmp[2],
				   &oper->class))
	    {
	      log_printf (LOG_ERROR, "irc: invalid O line: %s\n", line);
	      xfree (oper);
	    }
	  else
	    {
	      oper->line = line;
	      PARSE_TILL_AT (tmp[0]);
	      oper->user = xstrdup (tmp[0]);
	      oper->host = xstrdup (p);
	      oper->password = xstrdup (tmp[1]);
	      oper->nick = xstrdup (tmp[2]);
	      oper->local = 0;
	      oper->next = cfg->operator_auth;
	      cfg->operator_auth = oper;
	    }
	}
    }

  if (cfg->oLine)
    {
      for (n = 0, line = cfg->oLine[n]; line; line = cfg->oLine[++n])
	{
	  oper = xmalloc (sizeof (irc_oper_t));
	  if (4 != irc_parse_line (line, "O:%s:%s:%s::%d",
				   tmp[0], tmp[1], tmp[2],
				   &oper->class))
	    {
	      log_printf (LOG_ERROR, "irc: invalid o line: %s\n", line);
	      xfree (oper);
	    }
	  else
	    {
	      oper->line = line;
	      PARSE_TILL_AT (tmp[0]);
	      oper->user = xstrdup (tmp[0]);
	      oper->host = xstrdup (p);
	      oper->password = xstrdup (tmp[1]);
	      oper->nick = xstrdup (tmp[2]);
	      oper->local = 1;
	      oper->next = cfg->operator_auth;
	      cfg->operator_auth = oper;
	    }
	}
    }

  /* parse banned clients */
  if (cfg->KLine)
    {
      for (n = 0, line = cfg->KLine[n]; line; line = cfg->KLine[++n])
	{
	  kill = xmalloc (sizeof (irc_kill_t));
	  if (4 != irc_parse_line (line, "O:%s:%d-%d:%s",
				   tmp[0], &kill->start, &kill->end,
				   tmp[1]))
	    {
	      log_printf (LOG_ERROR, "irc: invalid K line: %s\n", line);
	      xfree (kill);
	    }
	  else
	    {
	      kill->line = line;
	      kill->host = xstrdup (tmp[0]);
	      kill->user = xstrdup (tmp[1]);
	      kill->next = cfg->banned;
	      cfg->banned = kill;
	    }
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
 * The following routine checks whether it is still possible to connnect
 * via a given connection class number. It returns zero on success or if
 * there no such connection class, otherwise non-zero.
 */
static int
irc_check_class (irc_config_t *cfg, int class_nr)
{
  irc_class_t *class;
  
  for (class = cfg->classes; class; class = class->next)
    {
      if (class->nr == class_nr)
	{
	  if (class->links++ < class->max_links)
	    return 0;
	  else
	    {
#if ENABLE_DEBUG
	      log_printf (LOG_DEBUG, "irc: %d/%d links reached in class %d\n",
			  class->links, class->max_links, class->nr);
#endif
	      return -1;
	    }
	}
    }
  return 0;
}

/*
 * This functions goes through all the K lines to check whether a given
 * client is unwanted. Return non-zero if so.
 */
static int
irc_client_killed (irc_client_t *client, irc_config_t *cfg)
{
  irc_kill_t *kill;
  time_t t;
  struct tm *tm;
  int ts;

  for (kill = cfg->banned; kill; kill = kill->next)
    {
      if (irc_string_regex (client->user, kill->user) &&
	  irc_string_regex (client->host, kill->host))
	{
	  t = time (NULL);
	  tm = localtime (&t);
	  ts = (tm->tm_hour + 1) * 100 + tm->tm_min;
	  if (ts >= kill->start && ts <= kill->end)
	    {
#if ENABLE_DEBUG
	      log_printf (LOG_DEBUG, "irc: %s@%s is K lined: %s\n",
			  client->user, client->host, kill->line);
	      return -1;
#endif
	    }
	}
    }
  return 0;
}

/*
 * This routine checks whether a given client is wanted to connect.
 * If it is not return zero, otherwise non-zero.
 */
int
irc_client_valid (irc_client_t *client, irc_config_t *cfg)
{
  irc_user_t *user;

  /* first we check whether this client is within the K lines */
  if (irc_client_killed (client, cfg))
    {
      irc_printf (client->sock, "%s %03d %s " ERR_YOUREBANNEDCREEP_TEXT "\n",
		  cfg->host, ERR_YOUREBANNEDCREEP, client->nick);
      return 0;
    }

  /* have a look at the user authorization list (I lines) */
  for (user = cfg->user_auth; user; user = user->next)
    {
      if ((irc_string_regex (client->user, user->user_ip) &&
	   irc_string_regex (util_inet_ntoa (client->sock->remote_addr), 
			     user->ip)) ||
	  (irc_string_regex (client->user, user->user_host) &&
	   irc_string_regex (client->host, user->host)))
	{
	  /* test the given password for that I line */
	  if (user->password)
	    {
	      if (strcmp (user->password, client->pass))
		{
		  irc_printf (client->sock, ":%s %03d %s " 
			      ERR_PASSWDMISMATCH_TEXT "\n",
			      cfg->host, ERR_PASSWDMISMATCH, client->nick);
		  return 0;
		}
	    }

	  /* now we have a look at the connection classes */
	  if (irc_check_class (cfg, user->class))
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
