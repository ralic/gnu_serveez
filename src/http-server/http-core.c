/*
 * http-core.c - http core functionality
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
 * $Id: http-core.c,v 1.1 2000/07/26 21:09:43 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_HTTP_PROTO

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef __MINGW32__
# include <io.h>
#endif

#include "util.h"
#include "alloc.h"
#include "hash.h"
#include "http-proto.h"
#include "http-core.h"

/*
 * In Win32 OS's both of these defines are necessary for portability.
 */
#if defined (__CYGWIN__) || defined (__MINGW32__)
# define timezone _timezone
# define daylight _daylight
#endif

/*
 * This function is used to re-initialize a HTTP connection for
 * Keep-Alive connections. Return -1 if it is not 'Keep'able.
 */
int
http_keep_alive (socket_t sock)
{
  if (sock->userflags & HTTP_FLAG_KEEP)
    {
      http_free_socket (sock);

      sock->userflags &= ~HTTP_FLAG; 
      sock->read_socket = default_read;
      sock->check_request = http_check_request;
      sock->write_socket = http_default_write;
      sock->send_buffer_fill = 0;
      sock->idle_func = http_idle;
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "http: keeping connection alive\n");
#endif
      return 0;
    }
  return -1;
}

/*
 * This function is used to check if the connection in SOCK is a
 * Keep-Alive connection and sends the apropiate HTTP header property.
 */
void
http_check_keepalive (socket_t sock)
{
  http_socket_t *http = sock->data;
  http_config_t *cfg = sock->cfg;

  if ((sock->userflags & HTTP_FLAG_KEEP) && http->keepalive > 0)
    {
      sock->idle_counter = cfg->timeout;
      sock_printf (sock, "Connection: Keep-Alive\r\n");
      sock_printf (sock, "Keep-Alive: timeout=%d, max=%d\r\n", 
		   sock->idle_counter, cfg->keepalive);
      http->keepalive--;
    }
  /* tell HTTP/1.1 clients that the connection is closed after delivery */
  else
    {
      sock->userflags &= ~HTTP_FLAG_KEEP;
      sock_printf (sock, "Connection: close\r\n");
    }
}

/*
 * Produce a ASCTIME date without the trailing '\n' from a given time_t.
 */
char *
http_asc_date (time_t t)
{
  static char asc[64];
  struct tm * gm_time;

  gm_time = gmtime (&t);
  strftime (asc, 64, "%a, %d %b %Y %H:%M:%S GMT", gm_time);

  return asc;
}

/*
 * Extract a date information from a given string and return a 
 * UTC time (time_t) as time() does.
 */
time_t
http_parse_date (char *date)
{
  struct tm parse_time;
  int n;
  char _month[4];
  char _wkday[10];
  time_t ret;

  static char month[12][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  switch (date[3])
    {
      /* ASCTIME-Date */
    case ' ':
      sscanf (date, "%3s %3s %2d %02d:%02d:%02d %04d",
	      _wkday,
	      _month,
	      &parse_time.tm_mday,
	      &parse_time.tm_hour, 
	      &parse_time.tm_min,
	      &parse_time.tm_sec,
	      &parse_time.tm_year);
      
      break;
      /* RFC1123-Date */
    case ',':
      sscanf (date, "%3s, %02d %3s %04d %02d:%02d:%02d GMT", 
	      _wkday,
	      &parse_time.tm_mday,
	      _month,
	      &parse_time.tm_year,
	      &parse_time.tm_hour, 
	      &parse_time.tm_min,
	      &parse_time.tm_sec);

      break;
      /* RFC850-Date */
    default:
      sscanf (date, "%s, %02d-%3s-%02d %02d:%02d:%02d GMT", 
	      _wkday,
	      &parse_time.tm_mday,
	      _month,
	      &parse_time.tm_year,
	      &parse_time.tm_hour, 
	      &parse_time.tm_min,
	      &parse_time.tm_sec);

      parse_time.tm_mon += parse_time.tm_mon >= 70 ? 1900 : 2000;

      break;
    }
    
  /* find the month identifier */
  for (n = 0; n < 12; n++)
    if (!memcmp (_month, month[n], 3))
      parse_time.tm_mon = n;

  parse_time.tm_isdst = daylight;
  parse_time.tm_year -= 1900;
  ret = mktime (&parse_time);
  ret -= timezone;
  if (daylight > 0) ret += 3600;
  return ret;
}

/*
 * Parse part of the receive buffer for HTTP request properties
 * and store it in the socket structure SOCK. Return the amount of
 * properties found in the request.
 */
int
http_parse_property (socket_t sock, char *request, char *end)
{
  int properties, n;
  char *p;
  http_socket_t *http;

  /* get the http socket structure */
  http = sock->data;

  /* reserve data space for the http properties */
  http->property = xmalloc (MAX_HTTP_PROPERTIES * 2 * sizeof(char *));
  properties = 0;
  n = 0;

  /* find out properties if necessary */
  while (INT16 (request) != CRLF && 
	 properties < MAX_HTTP_PROPERTIES - 1)
    {
      /* get property entity identifier */
      p = request;
      while (*p != ':' && p < end) p++;
      if (p == end) break;
      http->property[n] = xmalloc (p-request+1);
      strncpy (http->property[n], request, p-request);
      http->property[n][p-request] = 0;
      n++;
      request = p+2;

      /* get property entitiy body */
      while (INT16 (p) != CRLF && p < end) p++;
      if (p == end || p <= request) break;
      http->property[n] = xmalloc (p-request+1);
      strncpy (http->property[n], request, p-request);
      http->property[n][p-request] = 0;
      n++;
      properties++;
      request = p+2;

#if 0
      printf ("http header: {%s} = {%s}\n", 
	      http->property[n-2], http->property[n-1]);
#endif
    }

  request += 2;
  http->property[n] = NULL;

  return properties;
}

/*
 * Find a given property entity in the HTTP request properties.
 * Return a NULL pointer if not found.
 */
char *
http_find_property (http_socket_t *http, char *key)
{
  int n;

  /* search through all the http properties */
  n = 0;
  while (http->property[n])
    {
      if (!util_strcasecmp (http->property[n], key))
	{
	  return http->property[n+1];
	}
      n += 2;
    }
  return NULL;
}

#define ASC_TO_HEX(c)                             \
  if (c >= '0' && c <= '9') c -= '0';             \
  else if (c >= 'a' && c <= 'f') c -= ('a' - 10); \
  else if (c >= 'A' && c <= 'F') c -= ('A' - 10);

/*
 * Convert hexadecimal encoded characters within the URI. This is 
 * necessary for some special characters. The URI is a Uniform Resource 
 * Identifier meaning the requested file.
 */
void
http_process_uri (char *uri)
{
  char *p;
  char h, l;

  /* Test if there is any occurence of the special character encoding. */
  while ((p = strchr (uri, '%')) != NULL)
    {
      if ((h = *(p+1)) && (l = *(p+2)))
	{
	  /* Convert to byte. */
	  ASC_TO_HEX (h);
	  ASC_TO_HEX (l);
	  *p = (h << 4) | l;

	  /* Copy rest of URI. */
	  uri = ++p;
	  while (*(p+2)) *p++ = *(p+2);
	  *p = '\0';
	}
      else break;
    }
}

/*
 * This function frees all the content type definitions.
 */
void
http_free_types (http_config_t *cfg)
{
  char **type;
  int n;

  if (cfg->types != NULL)
    {
      if ((type = (char **)hash_values (cfg->types)) != NULL)
	{
	  for (n = 0; n < hash_size (cfg->types); n++)
	    {
	      xfree (type[n]);
	    }
	  hash_xfree (type);
	}
      hash_destroy (cfg->types);
      cfg->types = NULL;
    }
}

/*
 * This routine gets all available content types from a given
 * file which should have kind of "/etc/mime.types"s format.
 */
#define TYPES_LINE_SIZE 1024

int
http_read_types (http_config_t *cfg)
{
  FILE *f;
  char *line;
  char *p, *end;
  char *content;
  char *suffix;
  char *content_type;

  /* create the content type hash table if neccessary */
  if (cfg->types == NULL)
    {
      cfg->types = hash_create (4);
    }

  /* try open the file */
  if ((f = fopen (cfg->type_file, "rt")) == NULL)
    {
      log_printf (LOG_ERROR, "fopen: %s\n", SYS_ERROR);
      return -1;
    }

  line = xmalloc (TYPES_LINE_SIZE);

  /* read all lines within the file */
  while ((fgets (line, TYPES_LINE_SIZE, f)) != NULL)
    {
      /* delete all trailing newline characters */
      p = line + strlen (line) - 1;
      while (p != line && (*p == '\r' || *p == '\n')) p--;
      if (p == line) continue;
      *(p+1) = 0;

      p = line;
      end = line + strlen (line);

      /* parse content type */
      content = line;
      while (p < end && (*p != ' ' && *p != '\t')) p++;
      *p++ = 0;

      /* parse all file suffixes associated with this content type */
      while (p < end)
	{
	  while (p < end && (*p == ' ' || *p == '\t')) p++;
	  if (p == end) break;
	  suffix = p;
	  while (p < end && (*p != ' ' && *p != '\t')) p++;
	  *p++ = 0;
	  if (strlen (suffix))
	    {
	      /* 
	       * add the given content type to the hash if it does not
	       * contain it already
	       */
	      if (!hash_get (cfg->types, suffix))
		{
		  content_type = xmalloc (strlen (content) + 1);
		  strcpy (content_type, content);
		  hash_put (cfg->types, suffix, content_type);
		}
	    }
	}
    }
  fclose (f);
  xfree (line);
  return 0;
}

/*
 * This routine delivers a valid content type for a given file.
 * It falls back to the socket's http configuration default content
 * type if the suffix could not be found.
 */
char *
http_find_content_type (socket_t sock, char *file)
{
  http_config_t *cfg = sock->cfg;
  char *suffix = file + strlen(file) - 1;
  char *type;

  /* parse file back until a trailing '.' */
  while (suffix > file && *suffix != '.') suffix--;
  if (suffix != file) suffix++;

  /* find this file suffix in the content type hash */
  if ((type = hash_get (cfg->types, suffix)) != NULL)
    {
      return type;
    }

  return cfg->default_type;
}

/*
 * This routine converts a relative file/path name into an
 * absolute file/path name. The given argument will be reallocated
 * if neccessary.
 */
#define MAX_DIR_LEN 1024

char *
http_absolute_file (char *file)
{
  char *savedir;
  char *p;
  char *savefile;
  char *dir;
  int have_path = 0;

  /* find any path seperator in the file */
  p = file + strlen (file) - 1;
  while (p > file && *p != '/' && *p != '\\') p--;
  if (*p == '/' || *p == '\\')
    {
      have_path = 1;
      p++;
    }

  /* save the filename within a buffer */
  savefile = xmalloc (strlen (p) + 1);
  strcpy (savefile, p);

  /* get current work directory */
  savedir = xmalloc (MAX_DIR_LEN);
  if ((getcwd (savedir, MAX_DIR_LEN)) == NULL)
    {
      log_printf (LOG_ERROR, "getcwd: %s\n", SYS_ERROR);
      xfree (savefile);
      xfree (savedir);
      return file;
    }
  
  /* 
   * If there was no path seperator in the filename then just concate
   * current work directory and filename.
   */
  if (!have_path)
    {
      strcat (savedir, "/");
      strcat (savedir, savefile);
      savedir = xrealloc (savedir, strlen (savedir) + 1);
      xfree (file);
      return savedir;
    }
  
  /* change to give directory (absolute or relative)  */
  *p = 0;
  if (chdir (file) == -1)
    {
      *p = '/';
      log_printf (LOG_ERROR, "chdir: %s\n", SYS_ERROR);
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "cannot change dir: %s\n", file);
#endif
      xfree (savefile);
      xfree (savedir);
      return file;
    }
  *p = '/';

  /* get now the current work directory */
  dir = xmalloc (MAX_DIR_LEN);
  if ((getcwd (dir, MAX_DIR_LEN)) == NULL)
    {
      log_printf (LOG_ERROR, "getcwd: %s\n", SYS_ERROR);
      xfree (dir);
      xfree (savefile);
      xfree (savedir);
      return file;
    }

  /* concate new work directory with given filename */
  strcat (dir, "/");
  strcat (dir, savefile);
  dir = xrealloc (dir, strlen (dir) + 1);
  xfree (savefile);
  xfree (file);

  /* change back to the original work directory */
  chdir (savedir);
  xfree (savedir);
  return dir;
}

#else /* ENABLE_HTTP_PROTO */

int http_core_dummy; /* Shut compiler warnings up. */

#endif /* not ENABLE_HTTP_PROTO */
