/*
 * nut-transfer.c - gnutella file transfer implementation
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
 * $Id: nut-transfer.c,v 1.6 2000/09/03 21:28:05 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_GNUTELLA

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#ifndef __MINGW32__
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
# include <io.h>
#endif

#include "alloc.h"
#include "util.h"
#include "socket.h"
#include "connect.h"
#include "server.h"
#include "gnutella.h"
#include "nut-transfer.h"

/*
 * Check if a given search pattern matches a filename. Return zero on succes
 * and non-zero otherwise.
 */
static int
nut_string_regex (char *text, char *regex)
{
  char *p, *token, *str, *reg;
  
  /* first check if text tokens are in text */
  if (!strchr (text, '*') && !strchr (text, '?'))
    {
      str = xstrdup (text);
      reg = xstrdup (regex);
      util_tolower (str);
      util_tolower (reg);

      /* all tokens must be in the text */
      for (token = strtok (reg, " "); token; token = strtok (NULL, " "))
	{
	  if (!strstr (str, token)) break;
	}
      xfree (str);
      xfree (reg);
      if (!token) return 0;
      return -1;
    }

  /* parse until end of both strings */
  else while (*regex && *text)
    {
      /* find end of strings or '?' or '*' */
      while (*regex != '*' && *regex != '?' && 
             *regex && *text)
        {
          /* return no Match if so */
          if (tolower (*text) != tolower (*regex))
            return 0;
          text++;
          regex++;
        }
      /* one free character */
      if (*regex == '?')
        {
          if (!(*text)) return 0;
          text++;
          regex++;
        }
      /* free characters */
      else if (*regex == '*')
        {
          regex++;
          /* skip useless '?'s after '*'s */
          while (*regex == '?') regex++;
           /* skip all characters until next character in pattern found */
          while (*text && tolower (*regex) != tolower (*text)) 
            text++;
          /* next character in pattern found */
          if (*text)
            {
              /* find the last occurence of this character in the text */
              p = text + strlen (text);
              while (tolower (*p) != tolower (*text)) 
                p--;
              /* continue parsing at this character */
              text = p;
            }
        }
    }

  /* is the text longer than the regex ? */
  if (!*text && !*regex) return -1;
  return 0;
}

/*
 * Within this callback the actual file transfer is done.
 */
static int
nut_save_transfer (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;
  int fill = sock->recv_buffer_fill;
  nut_transfer_t *transfer = sock->data;
  int num_written;

  /* do we have something to write in the receive buffer ? */
  if (fill > 0)
    {
      /* write as much data as possible */
      num_written = write (sock->file_desc, sock->recv_buffer, fill);

      /* seems like an error occured */
      if (num_written < 0)
	{
	  log_printf (LOG_ERROR, "nut: write: %s\n", SYS_ERROR);
	  return -1;
	}
      
      /* crop written data from receive buffer */
      sock_reduce_recv (sock, num_written);

      /* did we get all data */
      if ((transfer->size -= num_written) <= 0)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: file successfully received\n");
#endif
	  /* yes, shutdown the connection */
	  return -1;
	}
    }
  return 0;
}

/*
 * This is the sock->check_request callback for gnutella file transfers.
 * Whenever there is data within the receive queue it will be called.
 */
static int
nut_check_transfer (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;
  int fill = sock->recv_buffer_fill;
  int len = strlen (NUT_GET_OK);
  char *p = sock->recv_buffer;
  nut_transfer_t *transfer = sock->data;

  /* check if got at least the first part of the HTTP header */
  if (fill >= len && !memcmp (sock->recv_buffer, NUT_GET_OK, len))
    {
      len = 0;

      /* find the end of the HTTP header (twice a CR/LF) */
      while (p < sock->recv_buffer + (fill - 3) && 
	     memcmp (p, NUT_SEPERATOR, 4))
	{
	  /* parse the content length field */
	  if (!memcmp (p, NUT_LENGTH, strlen (NUT_LENGTH)))
	    {
	      /* get the actual content length stored within the header */
	      p += strlen (NUT_LENGTH);
	      while (*p < '0' || *p > '9') p++;
	      while (*p >= '0' && *p <= '9')
		{
		  len *= 10;
		  len += *p - '0';
		  p++;
		}
              p--;
	    }
	  p++;
	}

      /* did we get all the header information ? */
      if (p < sock->recv_buffer + (fill - 3) && !memcmp (p, NUT_SEPERATOR, 4))
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: header received, length %d\n", len);
#endif
	  sock->userflags |= NUT_FLAG_HDR;

	  /* 
	   * check if the announced file length in the search reply
	   * corresponds to the content length of this HTTP header
	   */
	  transfer->size = len;
	  if (transfer->original_size != transfer->size)
	    {
	      log_printf (LOG_WARNING,
			  "nut: transfer sizes differ (%d!=%d)\n",
			  transfer->original_size, transfer->size);
	    }

	  /* assign the appropiate gnutella transfer callbacks */
	  sock->check_request = nut_save_transfer;
	  sock->write_socket = NULL;
	  sock->idle_func = NULL;

	  /* crop header from receive buffer */
	  len = (p - sock->recv_buffer) + 4;
	  sock_reduce_recv (sock, len);
	}
    }

  return 0;
}

/*
 * This callback is executed whenever a gnutella file transfer aborted
 * or successfully exited.
 */
static int
nut_disconnect_transfer (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;

  /* free the transfer data */
  if (sock->data)
    {
      xfree (sock->data);
      sock->data = NULL;
    }

  /* decrement amount of concurrent downloads */
  cfg->dnloads--;

  /* finally close the received file */
  if (close (sock->file_desc) == -1)
    log_printf (LOG_ERROR, "nut: close: %s\n", SYS_ERROR);

  return 0;
}

/*
 * This routine tries to connect to a foreign gnutella host in order to
 * get a certain file.
 */
int
nut_init_transfer (socket_t sock, nut_reply_t *reply, 
		   nut_record_t *record, char *savefile)
{
  nut_config_t *cfg = sock->cfg;
  socket_t xsock;
  char *file;
  struct stat buf;
  int fd;
  nut_transfer_t *transfer;
  int n = 0, pos;
  
  /* has the requested file the right file extension ? */
  if (cfg->extensions)
    {
      /* go through all file extensions */
      while (cfg->extensions[n])
	{
	  if (strlen (savefile) > strlen (cfg->extensions[n]))
	    {
	      pos = strlen (savefile) - strlen (cfg->extensions[n]);
	      if (pos < 0 ||
		  !util_strcasecmp (&savefile[pos], cfg->extensions[n]))
		break;
	    }
	  n++;
	}
      /* did the above code "break" ? */
      if (!cfg->extensions[n])
	{
	  log_printf (LOG_WARNING, "nut: not a valid extension: %s\n",
		      savefile);
	  return -1;
	}
    }

  /* first check if the requested file is not already created */
  file = xmalloc (strlen (cfg->save_path) + strlen (savefile) + 2);
  sprintf (file, "%s/%s", cfg->save_path, savefile);
  if (stat (file, &buf) != -1)
    {
      log_printf (LOG_NOTICE, "nut: %s already exists\n", savefile);
      xfree (file);
      return -1;
    }

  /* second check if the file matches the original search pattern */
  if (nut_string_regex (savefile, cfg->search))
    {
      log_printf (LOG_NOTICE, "nut: no search pattern for %s\n", savefile);
      xfree (file);
      return -1;
    }

  /* try creating local file */
#ifdef __MINGW32__
  if ((fd = open (file, O_RDWR|O_CREAT|O_BINARY, S_IREAD|S_IWRITE)) == -1)
#else
  if ((fd = open (file, O_RDWR|O_CREAT, S_IREAD|S_IWRITE)) == -1)
#endif
    {
      log_printf (LOG_ERROR, "nut: open: %s\n", SYS_ERROR);
      xfree (file);
      return -1;
    }

  /* try to connect to the host */
  if ((xsock = sock_connect (reply->ip, reply->port)) != NULL)
    {
      log_printf (LOG_NOTICE, "nut: connecting %s:%u\n",
		  util_inet_ntoa (reply->ip), ntohs (reply->port));
      cfg->dnloads++;
      xsock->cfg = cfg;
      xsock->flags |= SOCK_FLAG_NOFLOOD;
      xsock->disconnected_socket = nut_disconnect_transfer;
      xsock->check_request = nut_check_transfer;
      xsock->userflags = NUT_FLAG_HTTP;
      xsock->file_desc = fd;
      xsock->idle_func = nut_connect_timeout;
      xsock->idle_counter = NUT_CONNECT_TIMEOUT;
      transfer = xmalloc (sizeof (nut_transfer_t));
      transfer->original_size = record->size;
      xsock->data = transfer;

      /* send HTTP request to the listening gnutella host */
      sock_printf (xsock, NUT_GET "%d/%s " NUT_HTTP "\r\n",
		   record->index, savefile);
      sock_printf (xsock, NUT_AGENT);
      sock_printf (xsock, NUT_RANGE " bytes=0-\r\n");
      sock_printf (xsock, "\r\n");
      xfree (file);
      return 0;
    }

  close (fd);
  xfree (file);
  return 0;
}

#else /* ENABLE_GNUTELLA */

int nut_transfer_dummy;	/* Shut compiler warnings up. */

#endif /* not ENABLE_GNUTELLA */
