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
 * $Id: nut-transfer.c,v 1.12 2000/09/12 22:14:17 ela Exp $
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
# include <sys/types.h>
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
#include "snprintf.h"
#include "socket.h"
#include "connect.h"
#include "server.h"
#include "gnutella.h"
#include "nut-core.h"
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
  char *p = sock->recv_buffer, *length;
  nut_transfer_t *transfer = sock->data;

  /* check if got at least the first part of the HTTP header */
  if (fill >= len && !memcmp (sock->recv_buffer, NUT_GET_OK, len))
    {
      /* find the end of the HTTP header (twice a CR/LF) */
      while (p < sock->recv_buffer + (fill - 3) && 
	     memcmp (p, NUT_SEPERATOR, 4))
	p++;
      
      /* did we get all the header information ? */
      if (p < sock->recv_buffer + (fill - 3) && !memcmp (p, NUT_SEPERATOR, 4))
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: download header received\n");
#endif

	  len = p - sock->recv_buffer + 1;
	  length = nut_parse_property (sock->recv_buffer, len, NUT_LENGTH);
	  if (length == NULL)
	    {
#if ENABLE_DEBUG
	      log_printf (LOG_DEBUG, "nut: no content length given\n");
#endif
	      return -1;
	    }

	  /* 
	   * check if the announced file length in the search reply
	   * corresponds to the content length of this HTTP header
	   */
	  sock->userflags |= NUT_FLAG_HDR;
	  transfer->size = util_atoi (length);
	  xfree (length);
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

  /* second check if the file matches the original search patterns */
  if (cfg->search)
    {
      for (n = 0; cfg->search[n]; n++)
	{
	  if (nut_string_regex (savefile, cfg->search[n]))
	    break;
	}
      if (!cfg->search[n])
	{
	  log_printf (LOG_NOTICE, "nut: no search pattern for %s\n", savefile);
	  xfree (file);
	  return -1;
	}
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
      sock_printf (xsock, NUT_GET "%d/%s " NUT_HTTP "1.0\r\n",
		   record->index, savefile);
      sock_printf (xsock, NUT_AGENT);
      sock_printf (xsock, NUT_RANGE ": bytes=0-\r\n");
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
      xfree (entry->path);
      xfree (entry);
    }
  cfg->db_files = 0;
  cfg->db_size = 0;
}

/*
 * Add a further file to our database.
 */
void
nut_add_database (nut_config_t *cfg, char *path, char *file, off_t size)
{
  nut_file_t *entry;

  entry = xmalloc (sizeof (nut_file_t));
  entry->file = xstrdup (file);
  entry->path = xstrdup (path);
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
 * NULL. If FILE is NULL we just search for the the given INDEX.
 */
nut_file_t *
nut_get_database (nut_config_t *cfg, char *file, unsigned index)
{
  nut_file_t *entry = NULL;

  for (entry = cfg->database; entry; entry = entry->next)
    {
      if (entry->index == index)
	if (file == NULL || !strcmp (entry->file, file))
	  return entry;
    }

  return entry;
}

/*
 * This routine will re-read the share directory. The routine itself is
 * recursive. Thus be careful ! It cannot check for recursion loops, yet.
 */
void
nut_read_database_r (nut_config_t *cfg, char *dirname, int depth)
{
  char *path;
  static struct stat buf;
  static char filename[NUT_PATH_SIZE];
#ifndef __MINGW32__
  DIR *dir;
  static struct dirent *de = NULL;
#else
  HANDLE dir;
  static WIN32_FIND_DATA de;
#endif

  /* first call */
  if (!depth) 
    {
      nut_destroy_database (cfg);
    }

  /* check recursion condition */
  if (depth < NUT_PATH_DEPTH)
    {
      depth++;

      /* open the directory */
#ifdef __MINGW32__
      if (snprintf (filename, NUT_PATH_SIZE - 1, "%s/*", dirname) == -1)
	return;
      
      if ((dir = FindFirstFile (filename, &de)) != INVALID_HANDLE_VALUE)
#else
      if ((dir = opendir (dirname)) != NULL)
#endif
	{
	  /* iterate directory */
#ifndef __MINGW32__
	  while (NULL != (de = readdir (dir)))
#else
	  do
#endif
	    {
	      if (snprintf (filename, NUT_PATH_SIZE - 1,
			    "%s/%s", dirname, FILENAME) == -1)
		continue;

	      /* stat the given file */
	      if (stat (filename, &buf) != -1)
		{
		  /* add valid files to database */
		  if (S_ISREG (buf.st_mode) && buf.st_size > 0)
		    {
		      nut_add_database (cfg, dirname, FILENAME, buf.st_size);
		    }
		  /* recurse into directories */
		  else if (S_ISDIR (buf.st_mode) && FILENAME[0] != '.')
		    {
		      path = xstrdup (filename);
		      nut_read_database_r (cfg, path, depth);
		      xfree (path);
		    }
		}
	    }
#ifdef __MINGW32__
	  while (FindNextFile (dir, &de));
#endif
	  closedir (dir);
	}
    }
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
  char *end, *file, *f, *hdr;
  nut_file_t *entry;

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
      len = p - sock->recv_buffer + 1;
      p = sock->recv_buffer + strlen (NUT_GET);
      end = sock->recv_buffer + len;
      while (p < end && *p >= '0' && *p <= '9')
	{
	  index *= 10;
	  index += *p - '0';
	  p++;
	}
      /* parsed file index */
      if (p >= end || *p != '/') return -1;
      f = ++p;
      while (p < end && *p != '\r' && *p != '\n') p++;

      /* got actual header property field */
      hdr = p + 2;
      len -= hdr - sock->recv_buffer;

      while (p > f && memcmp (p, " " NUT_HTTP, strlen (NUT_HTTP) + 1)) p--;
      if (p <= f) return -1;
      /* parsed file itself */
      fill = p - f;
      file = xmalloc (fill + 1);
      memcpy (file, f, fill);
      file[fill] = '\0';

      /* here we parse all the header properties */

      /* find file in database */
      if ((entry = nut_get_database (sock->cfg, file, index)) == NULL)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: no such file: %s, %u\n", file, index);
#endif
	  xfree (file);
	  return -1;
	}
      len = end - sock->recv_buffer + 3;
      sock_reduce_recv (sock, len);
      xfree (file);

      /* disable connection timeout */
      sock->idle_func = NULL;
      sock->userflags |= NUT_FLAG_HDR;

      if (nut_init_upload (sock, entry) == -1)
	return -1;
    }

  return 0;
}

/*
 * If the gnutella header has been successfully parsed this routine
 * initializes the file upload.
 */
int
nut_init_upload (socket_t sock, nut_file_t *entry)
{
  char *file;
  nut_config_t *cfg = sock->cfg;
  struct stat buf;
  int fd;
  nut_transfer_t *transfer;

  /* create filename */
  file = xmalloc (strlen (entry->path) + strlen (entry->file) + 2);
  sprintf (file, "%s/%s", entry->path, entry->file);
  
  /* check file */
  if (stat (file, &buf) == -1 || !S_ISREG (buf.st_mode) || buf.st_size <= 0)
    {
      log_printf (LOG_ERROR, "nut: invalid file: %s %s\n", file);
      xfree (file);
      return -1;
    }
  
  /* open the file for reading */
#ifdef __MINGW32__
  if ((fd = open (file, O_RDONLY | O_BINARY)) == -1)
#else
  if ((fd = open (file, O_RDONLY | O_NONBLOCK)) == -1)
#endif
    {
      log_printf (LOG_ERROR, "nut: open: %s\n", SYS_ERROR);
      xfree (file);
      return -1;
    }

  sock_printf (sock, NUT_GET_OK  NUT_AGENT);
  sock_printf (sock, NUT_CONTENT ": application/binary\r\n");
  sock_printf (sock, NUT_LENGTH ": %d\r\n", buf.st_size);
  sock_printf (sock, "\r\n");

  sock->file_desc = fd;
  sock->read_socket = nut_file_read;
  sock->write_socket = nut_file_write;
  sock->disconnected_socket = nut_disconnect_upload;
  sock->flags |= SOCK_FLAG_FILE;
  cfg->uploads++;

  transfer = xmalloc (sizeof (nut_transfer_t));
  memset (transfer, 0, sizeof (nut_transfer_t));
  transfer->size = buf.st_size;
  transfer->original_size = buf.st_size;
  transfer->start = time (NULL);
  sock->data = transfer;

  xfree (file);
  return 0;
}

/*
 * Disconnection callback for gnutella uploads.
 */
int
nut_disconnect_upload (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;
  nut_transfer_t *transfer = sock->data;

  /* decrement amount of concurrent uploads */
  cfg->uploads--;

  /* finally close the received file */
  if (close (sock->file_desc) == -1)
    log_printf (LOG_ERROR, "nut: close: %s\n", SYS_ERROR);

  /* free the transfer data */
  if (transfer)
    {
      xfree (transfer);
      sock->data = NULL;
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
  int num_read;
  int do_read;
  nut_transfer_t *transfer = sock->data;

  do_read = sock->send_buffer_size - sock->send_buffer_fill;

  /* 
   * This means the send buffer is currently full, we have to 
   * wait until some data has been send via the socket.
   */
  if (do_read <= 0)
    {
      return 0;
    }

  /*
   * Try to read as much data as possible from the file.
   */
  num_read = read (sock->file_desc,
                   sock->send_buffer + sock->send_buffer_fill,
                   do_read);

  /* Read error occured. */
  if (num_read < 0)
    {
      log_printf (LOG_ERROR, "nut: read: %s\n", SYS_ERROR);
      return -1;
    }

  /* Bogus file. File size from stat() was not true. */
  if (num_read == 0 && transfer->size != 0)
    {
      return -1;
    }

  /* Data has been read or EOF reached, set the appropiate flags. */
  sock->send_buffer_fill += num_read;
  transfer->size -= num_read;

  /* Read all file data ? */
  if (transfer->size <= 0)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: file successfully read\n");
#endif
      /* 
       * no further read()s from the file descriptor, signalling 
       * the writers there will not be additional data from now on
       */
      sock->read_socket = default_read;
      sock->flags &= ~SOCK_FLAG_FILE;
    }

  return 0;
}

/*
 * This function is the upload callback for the gnutella server. It 
 * throttles its network output to a configured value.
 */
int
nut_file_write (socket_t sock)
{
  int num_written, do_write;
  nut_transfer_t *transfer = sock->data;
  nut_config_t *cfg = sock->cfg;
  time_t t = time (NULL);

  /* throttle the network output */
  num_written = transfer->original_size - transfer->size;
  if (num_written / (t - transfer->start + 1) > cfg->speed * 1024 / 8)
    {
      sock->unavailable = t + RELAX_FD_TIME;
      return 0;
    }

  /* 
   * Write as many bytes as possible, remember how many
   * were actually sent.
   */
  do_write = (sock->send_buffer_fill > SOCK_MAX_WRITE) 
    ? SOCK_MAX_WRITE : sock->send_buffer_fill;
    
  num_written = send (sock->sock_desc, sock->send_buffer, do_write, 0);

  /* some data has been written */
  if (num_written > 0)
    {
      sock->last_send = t;

      if (sock->send_buffer_fill > num_written)
        {
          memmove (sock->send_buffer, 
                   sock->send_buffer + num_written,
                   sock->send_buffer_fill - num_written);
        }
      sock->send_buffer_fill -= num_written;
    }

  /* write error occured */
  else if (num_written < 0)
    {
      log_printf (LOG_ERROR, "nut: send: %s\n", NET_ERROR);
      if (last_errno == SOCK_UNAVAILABLE)
        {
          sock->unavailable = t + RELAX_FD_TIME;
          num_written = 0;
        }
    }

  if (sock->send_buffer_fill == 0 && transfer->size <= 0)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "nut: file successfully sent\n");
#endif
      return -1;
    }

  /*
   * Return a non-zero value if an error occured.
   */
  return (num_written < 0) ? -1 : 0;
}

#else /* ENABLE_GNUTELLA */

int nut_transfer_dummy;	/* Shut compiler warnings up. */

#endif /* not ENABLE_GNUTELLA */
