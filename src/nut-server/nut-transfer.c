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
 * $Id: nut-transfer.c,v 1.9 2000/09/08 07:45:18 ela Exp $
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

# if HAVE_DIRENT_H
#  include <dirent.h>
#  define NAMLEN(dirent) strlen((dirent)->d_name)
# else
#  define dirent direct
#  define NAMLEN(dirent) (dirent)->d_namlen
#  if HAVE_SYS_NDIR_H
#   include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#   include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#   include <ndir.h>
#  endif
# endif
#endif /* not __MINGW32__ */

#ifdef __MINGW32__
# include <windows.h>
# include <winsock.h>
# include <io.h>
#endif

#if HAVE_SYS_DIRENT_H
# include <sys/dirent.h>
#endif

#ifndef __MINGW32__
# define FILENAME de->d_name
#else 
# define FILENAME de.cFileName
# define closedir(dir) FindClose (dir)
#endif

#include "alloc.h"
#include "util.h"
#include "socket.h"
#include "connect.h"
#include "server.h"
#include "gnutella.h"
#include "nut-transfer.h"

/*
 * Check if a given search pattern matches a filename. Return non-zero 
 * on succes and zero otherwise.
 */
static int
nut_string_regex (char *text, char *regex)
{
  char *p, *token, *str, *reg;
  
  /* first check if text tokens are in text */
  if (!strchr (regex, '*') && !strchr (regex, '?'))
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
      if (!token) return -1;
      return 0;
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
			  "nut: transfer sizes differ (%u!=%u)\n",
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
  nut_transfer_t *transfer = sock->data;

  /* decrement amount of concurrent downloads */
  cfg->dnloads--;

  /* finally close the received file */
  if (close (sock->file_desc) == -1)
    log_printf (LOG_ERROR, "nut: close: %s\n", SYS_ERROR);

  /* free the transfer data */
  if (transfer)
    {
      /* if the transfer was really aborted we remove the downloaded file */
      if (transfer->size > 0 || !(sock->userflags & NUT_FLAG_HDR))
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: downloading `%s' aborted\n",
		      transfer->file);
#endif
	  if (unlink (transfer->file) == -1)
	    log_printf (LOG_ERROR, "nut: unlink: %s\n", SYS_ERROR);
	}
      xfree (transfer->file);
      xfree (transfer);
      sock->data = NULL;
    }

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
  if (!nut_string_regex (savefile, cfg->search))
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
      transfer->file = xstrdup (file);
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

/*
 * Destroy database.
 */
void
nut_destroy_database (nut_config_t *cfg)
{
  nut_file_t *entry;
  
  while ((entry = cfg->database) != NULL)
    {
      cfg->database = entry->next;
      xfree (entry->file);
      xfree (entry);
    }
  cfg->db_files = 0;
  cfg->db_size = 0;
}

/*
 * Add a further file to our database.
 */
void
nut_add_database (nut_config_t *cfg, char *file, off_t size)
{
  nut_file_t *entry;

  entry = xmalloc (sizeof (nut_file_t));
  entry->file = xstrdup (file);
  entry->size = size;
  entry->index = cfg->db_files;
  entry->next = cfg->database;
  cfg->database = entry;
  cfg->db_files++;
  cfg->db_size += size;
}

/*
 * Find a given search pattern within the database. Start to find it
 * at the given ENTRY. If it is NULL we start at the very beginning.
 */
nut_file_t *
nut_find_database (nut_config_t *cfg, nut_file_t *entry, char *search)
{
  if (entry == NULL)
    entry = cfg->database;
  else
    entry = entry->next;

  while (entry)
    {
      if (nut_string_regex (entry->file, search))
	return entry;
      entry = entry->next;
    }
  return NULL;
}

/*
 * This routine gets a gnutella database entry from a given FILE and
 * its appropiate INDEX. If no matching file has been found then return
 * NULL.
 */
nut_file_t *
nut_get_database (nut_config_t *cfg, char *file, unsigned index)
{
  nut_file_t *entry;

  for (entry = cfg->database; entry; entry = entry->next)
    {
      if (entry->index == index && !strcmp (entry->file, file))
	return entry;
    }

  return entry;
}

/*
 * This routine will re-read the share directory.
 */
int
nut_read_database (nut_config_t *cfg, char *dirname)
{
  int i = 0;
  struct stat buf;
  int files = 0;
  char filename[1024];
#ifndef __MINGW32__
  DIR *dir;
  struct dirent *de = NULL;
#else
  WIN32_FIND_DATA de;
  HANDLE dir;
#endif

  /* free previous database */
  nut_destroy_database (cfg);

  /* open the directory */
#ifdef __MINGW32__
  sprintf (filename, "%s/*", dirname);
      
  if ((dir = FindFirstFile (filename, &de)) == INVALID_HANDLE_VALUE)
#else
  if ((dir = opendir (dirname)) == NULL)
#endif
    {
      return files;
    }

  /* iterate directory */
#ifndef __MINGW32__
  while (NULL != (de = readdir (dir)))
#else
  do
#endif
    {
      sprintf (filename, "%s/%s", dirname, FILENAME);

      /* stat the given file */
      if (stat (filename, &buf) != -1 && 
	  S_ISREG (buf.st_mode) && buf.st_size > 0)
        {
	  nut_add_database (cfg, FILENAME, buf.st_size);
	  files++;
        } 
    }
#ifdef __MINGW32__
  while (FindNextFile (dir, &de));
#endif

  closedir (dir);
  return files;
}

/*
 * This routine checks for a valid http header for gnutella upload 
 * requests.
 */
int
nut_check_upload (socket_t sock)
{
  char *p = sock->recv_buffer;
  int len, fill = strlen (NUT_GET);
  unsigned index = 0;

  /* enough receive buffer fill ? */
  if (sock->recv_buffer_fill < fill)
    return 0;

  /* initial gnutella request detection */
  if (memcmp (p, NUT_GET, fill))
    return -1;

  /* parse whole upload header and find the end of the HTTP header */
  fill = sock->recv_buffer_fill;
  while (p < sock->recv_buffer + (fill - 3) && memcmp (p, NUT_SEPERATOR, 4))
    p++;

  if (p < sock->recv_buffer + (fill - 3) && !memcmp (p, NUT_SEPERATOR, 4))
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: upload header received\n");
#endif
      /* parse first (GET) line */

      /* here we parse all the header properties */
    }

  return 0;
}

/*
 * Default gnutella file reader. It is the sock->read_socket callback for
 * file uploads.
 */
int
nut_file_read (socket_t sock)
{
  return 0;
}

/*
 * This function is the upload callback for the gnutella server. It 
 * throttles its network output to a configured value.
 */
int
nut_file_write (socket_t sock)
{
  return 0;
}

#else /* ENABLE_GNUTELLA */

int nut_transfer_dummy;	/* Shut compiler warnings up. */

#endif /* not ENABLE_GNUTELLA */
