/*
 * http-proto.c - http protocol implementation
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
 * $Id: http-proto.c,v 1.3 2000/06/15 11:54:52 ela Exp $
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#if HAVE_STRINGS_H
# include <strings.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#ifndef __MINGW32__
# include <sys/socket.h>
#endif

#if defined(__CYGWIN__) || defined(__MINGW32__)
# define timezone _timezone
# define daylight _daylight
#endif

#include "util.h"
#include "alloc.h"
#include "hash.h"
#include "socket.h"
#include "pipe-socket.h"
#include "serveez.h"
#include "server.h"
#include "server-core.h"
#include "http-proto.h"
#include "http-cgi.h"
#include "http-dirlist.h"
#include "http-cache.h"

/*
 * The HTTP port configuration.
 */
portcfg_t http_port =
{
  PROTO_TCP,  /* TCP protocol defintion */
  42424,      /* prefered port */
  NULL,       /* no receiving (listening) pipe */
  NULL        /* no sending pipe */
};

/*
 * The HTTP server instance configuration.
 */
http_config_t http_config =
{
  &http_port,         /* port configuration */
  "index.html",       /* standard index file  for GET request */
  "../show",          /* document root */
  "/cgi-bin",         /* how cgi-requests are detected */
  "./cgibin",         /* cgi script root */
  MAX_CACHE_SIZE,     /* maximum file size to cache them */
  HTTP_TIMEOUT,       /* server shuts connection down after x seconds */
  HTTP_MAXKEEPALIVE,  /* how many files when using keep-alive */
  "text/plain",       /* standard content type */
  "/etc/mime.types",  /* standard content type file */
  NULL                /* current content type hash */
};

/*
 * Defintion of the configuration items processed by libsizzle (taken
 * from the config file).
 */
key_value_pair_t http_config_prototype [] =
{
  REGISTER_PORTCFG ("port", http_config.port, DEFAULTABLE),
  REGISTER_STR ("indexfile", http_config.indexfile, DEFAULTABLE),
  REGISTER_STR ("docs", http_config.docs, DEFAULTABLE),
  REGISTER_STR ("cgiurl", http_config.cgiurl, DEFAULTABLE),
  REGISTER_STR ("cgidir", http_config.cgidir, DEFAULTABLE),
  REGISTER_INT ("cachesize", http_config.cachesize, DEFAULTABLE),
  REGISTER_INT ("timeout", http_config.timeout, DEFAULTABLE),
  REGISTER_INT ("keepalive", http_config.keepalive, DEFAULTABLE),
  REGISTER_STR ("default-type", http_config.default_type, DEFAULTABLE),
  REGISTER_STR ("type-file", http_config.type_file, DEFAULTABLE),
  REGISTER_HASH ("types", http_config.types, DEFAULTABLE),
  REGISTER_END ()
};

/*
 * Defintion of the http server.
 */
server_definition_t http_server_definition =
{
  "http server",         /* long server description */
  "http",                /* short server description (for libsizzle) */
  http_global_init,      /* global initializer */
  http_init,             /* instance initializer */
  http_detect_proto,     /* protocol detection routine */
  http_connect_socket,   /* connection routine */
  http_finalize,         /* instance finalization routine */
  http_global_finalize,  /* global finalizer */
  &http_config,          /* default configuration */
  sizeof (http_config),  /* size of the configuration */
  http_config_prototype  /* configuration prototypes (libsizzle) */
};

/*
 * HTTP request types, their identification string (including its length)
 * and the apropiate callback routine itself. This array is used in
 * the HTTP_HANDLE_REQUEST function.
 */
struct
{
  char *ident;                            /* identification string */
  int len;                                /* the length of this string */
  int (*response)(socket_t, char *, int); /* the callback routine */
} 
http_request[HTTP_REQUESTS] = {

  { "GET",     3, http_get_response     },
  { "HEAD",    4, http_head_response    },
  { "POST",    4, http_post_response    },
  { "PUT",     3, http_default_response },
  { "OPTIONS", 7, http_default_response },
  { "DELETE",  6, http_default_response },
  { "TRACE",   5, http_default_response },
  { "CONNECT", 7, http_default_response }

};

/*
 * Content type definitions. Maybe later you could add this to the 
 * configuration hash and read it from "/etc/mime.types" !
 */
typedef struct
{
  char *suffix; /* recognition file suffix */
  char *type;   /* apropiate content-type string */
}
http_content_t;

hash_t *http_content_type = NULL;

struct
{
  char *suffix; /* regognition file suffix */
  char *type;   /* apropiate content-type string */
}
http_content[] =
{
  {"class", "application/octet-stream"},
  {"jar",   "application/octet-stream"},
  {"html",  "text/html"},
  {"htm",   "text/html"},
  {"au",    "audio/basic"},
  {"gif",   "image/gif"},
  {"jpg",   "image/jpeg"},
  {"jpeg",  "image/jpeg"},
  {"png",   "image/png"},
  {"txt",   "text/plain"},
  {"c",     "text/plain"},
  {"h",     "text/plain"},
  {"js",    "application/x-javascript"},
  {"tgz",   "application/x-gtar"},
  {"gz",    "application/x-gzip"},
  {"zip",   "application/zip"},
  {"tar",   "application/x-tar"},
  {NULL, NULL}
};

int
http_global_init (void)
{
  return 0;
}

int
http_global_finalize (void)
{
  return 0;
}

/*
 * Local http server instance initializer.
 */
int
http_init (server_t *server)
{
  http_config_t *cfg = server->cfg;
  
  http_read_types (cfg->type_file);

  log_printf (LOG_NOTICE, "http: files in %s\n", cfg->docs);
  log_printf (LOG_NOTICE, "http: %s is cgi root, accessed via %s\n",
	      cfg->cgidir, cfg->cgiurl);

  return 0;
}

/*
 * Local http server instance finalizer.
 */
int
http_finalize (server_t *server)
{
  http_free_content_types ();
  http_free_cache ();
  
  return 0;
}

/*
 * Add a content type definition to the content type hash
 * http_content_type.
 */
void
http_add_content_type (char *suffix, char *type)
{
  char *content_type;

  /* create the hash table if neccessary */
  if (http_content_type == NULL)
    {
      http_content_type = hash_create (4);
    }

  /* 
   * add the given content type to the hash if it does not
   * contain it already
   */
  if (!hash_get (http_content_type, suffix))
    {
      content_type = xmalloc (strlen (type) + 1);
      strcpy (content_type, type);
      hash_put (http_content_type, suffix, content_type);
    }
}

/*
 * This function frees all the content type definitions.
 */
void
http_free_content_types (void)
{
  char **type;
  int n;

  if (http_content_type != NULL)
    {
      if ((type = (char **)hash_values (http_content_type)) != NULL)
	{
	  for (n = 0; n < http_content_type->keys; n++)
	    {
	      xfree (type[n]);
	    }
	  xfree (type);
	}
      hash_destroy (http_content_type);
      http_content_type = NULL;
    }
}

/*
 * This function frees all HTTP request properties previously reserved
 * and frees the cache structure if neccessary. Nevertheless the 
 * socket structure SOCK should still be usable for keep-alive connections.
 */
void
http_free_socket (socket_t sock)
{
  int n;
  http_socket_t *http;

  /* first get the http socket structure */
  http = sock->data;

  /* any property at all ? */
  if(http->property)
    {
      /* go through all properties */
      n = 0;
      while(http->property[n])
	{
	  xfree(http->property[n]);
	  n++;
	}
      xfree(http->property);
      http->property = NULL;
    }

  /* is the cache entry used ? */
  if(http->cache)
    {
      xfree(http->cache);
      http->cache = NULL;
    }

  /* close the file descriptor for usual http file transfer */
  if(sock->file_desc != -1)
    {
      if(close(sock->file_desc) == -1)
	log_printf(LOG_ERROR, "close: %s\n", SYS_ERROR);
      sock->file_desc = -1;
    }

  /* close both of the CGI pipes if necessary */
  if(sock->pipe_desc[READ] != INVALID_HANDLE)
    {
      if(CLOSE_HANDLE(sock->pipe_desc[READ]) == -1)
	log_printf(LOG_ERROR, "close: %s\n", SYS_ERROR);
      sock->pipe_desc[READ] = INVALID_HANDLE;
    }
  if(sock->pipe_desc[WRITE] != INVALID_HANDLE)
    {
      if(CLOSE_HANDLE(sock->pipe_desc[WRITE]) == -1)
	log_printf(LOG_ERROR, "close: %s\n", SYS_ERROR);
      sock->pipe_desc[WRITE] = INVALID_HANDLE;
    }

#ifdef __MINGW32__
  /* 
   * close the process handle if necessary, 
   * but only in the Windows-Port ! 
   */
  if(http->pid != INVALID_HANDLE)
    {
      if(CLOSE_HANDLE(http->pid) == -1)
	log_printf(LOG_ERROR, "CloseHandle: %s\n", SYS_ERROR);
      http->pid = INVALID_HANDLE;
    }
#endif

}

/*
 * Disconnects a HTTP connection. Callback routine for the
 * socket structure entry "disconnected_socket".
 */
int
http_disconnect (socket_t sock)
{
  http_socket_t *http;

  /* get http socket structure */
  http = sock->data;

  /* free the http socket structures */
  http_free_socket(sock);
  xfree(sock->data);

  return 0;
}

/*
 * Idle function for HTTP Keep-Alive connections. It simply returns -1
 * if a certain time has elapsed and the main server loop will shutdown
 * the connection therefore.
 */
int
http_idle (socket_t sock)
{
  time_t now;
  http_config_t *cfg = sock->cfg;

  now = time(NULL);
  if(now - sock->last_recv > cfg->timeout &&
     now - sock->last_send > cfg->timeout)
    return -1;
  sock->idle_counter = 1;
  return 0;
}

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
http_check_keepalive(socket_t sock)
{
  http_socket_t *http = sock->data;
  http_config_t *cfg = sock->cfg;

  if(sock->userflags & HTTP_FLAG_KEEP && http->keepalive > 0)
    {
      sock->idle_counter = cfg->timeout;
      sock_printf(sock, "Connection: Keep-Alive\r\n");
      sock_printf(sock, "Keep-Alive: timeout=%d, max=%d\r\n", 
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
 * HTTP_DEFAULT_WRITE will shutdown the connection immediately when 
 * the whole response has been sent (indicated by the HTTP_FLAG_DONE
 * flag) with two exceptions. It will keep the connection if the
 * actual file is within the cache and if this is a keep-alive connection.
 */
int
http_default_write(socket_t sock)
{
  int num_written;

  /* 
   * Write as many bytes as possible, remember how many
   * were actually sent.
   */
  num_written = send(sock->sock_desc, sock->send_buffer,
		     sock->send_buffer_fill, 0);

  /* some data has been written */
  if(num_written > 0)
    {
      sock->last_send = time(NULL);

      if(sock->send_buffer_fill > num_written)
	{
	  memmove(sock->send_buffer, 
		  sock->send_buffer + num_written,
		  sock->send_buffer_fill - num_written);
	}
      sock->send_buffer_fill -= num_written;
    }

  /* write error occured */
  else if (num_written < 0)
    {
      log_printf(LOG_ERROR, "http: write: %s\n", NET_ERROR);
      if(last_errno == SOCK_UNAVAILABLE)
	{
	  sock->unavailable = time(NULL) + RELAX_FD_TIME;
	  num_written = 0;
	}
    }

  /*
   * Check if the http response has (success)fully been sent.
   * If yes then return non-zero in order to shutdown the socket SOCK
   * and return zero if it is a keep-alive connection.
   */
  if(sock->userflags & HTTP_FLAG_DONE && sock->send_buffer_fill == 0)
    {
#if ENABLE_DEBUG
      log_printf(LOG_DEBUG, "http: response successfully sent\n");
#endif
      num_written = http_keep_alive(sock);
    }

  /*
   * If the requested file is within the cache then start now the 
   * cache writer. Set SEND_BUFFER_FILL to something greater than zero.
   */
  if(sock->userflags & HTTP_FLAG_CACHE && sock->send_buffer_fill == 0)
    {
      sock->send_buffer_fill = 42;
      sock->write_socket = http_cache_write;
    }

  /*
   * Return a non-zero value if an error occured.
   */
  return (num_written < 0) ? -1 : 0;
}

/*
 * The HTTP_FILE_READ reads as much data from a file as possible directly
 * into the send buffer of the socket SOCK. It returns a non-zero value 
 * on read errors. When all the file has been read then the socket flag 
 * HTTP_FLAG_DONE is set.
 */
int
http_file_read (socket_t sock)
{
  int num_read;
  int do_read;
  http_socket_t *http;

  http = sock->data;
  do_read = sock->send_buffer_size - sock->send_buffer_fill;

  /* 
   * This means the send buffer is currently full, we have to 
   * wait until some data has been send via the socket.
   */
  if(do_read <= 0)
    {
      return 0;
    }

  /*
   * Try to read as much data as possible from the file.
   */
  num_read = read(sock->file_desc,
		  sock->send_buffer + sock->send_buffer_fill,
		  do_read);

  /* Read error occured. */
  if(num_read < 0)
    {
      log_printf(LOG_ERROR, "http: read: %s\n", SYS_ERROR);
      return -1;
    }

  /* Data has been read or EOF reached, set the apropiate flags. */
  sock->send_buffer_fill += num_read;
  http->filelength -= num_read;

  /* Read all file data ? */
  if(http->filelength <= 0)
    {
#if ENABLE_DEBUG
      log_printf(LOG_DEBUG, "http: file successfully read\n");
#endif
      /* 
       * no further read()s from the file descriptor, signalling 
       * the writers there will notbe additional data from now on
       */
      sock->read_socket = default_read;
      sock->userflags |= HTTP_FLAG_DONE;
      sock->userflags &= ~HTTP_FLAG_FILE;
    }

  return 0;
}

/*
 * This function gets called for new sockets which are not yet
 * identified.  It returns a non-zero value when the content in
 * the receive buffer looks like an HTTP request.
 */
int
http_detect_proto (void *cfg, socket_t sock)
{
  int n;

  /* go through all possible request types */
  for (n = 0; n < HTTP_REQUESTS; n++)
    {
      if (sock->recv_buffer_fill >= http_request[n].len)
	{
	  if (!memcmp (sock->recv_buffer, http_request[n].ident, 
		       http_request[n].len))
	    {
#if ENABLE_DEBUG
	      log_printf(LOG_DEBUG, "http client detected\n");
#endif
	      return -1;
	    }
	}
    }

  return 0;
}

/*
 * When the http_detect_proto returns successfully this function must
 * be called to set all the apropiate callbacks and socket flags.
 */
int
http_connect_socket (void *http_cfg, socket_t sock)
{
  http_socket_t *http;
  http_config_t *cfg = http_cfg;

  /*
   * initialize the http socket structure
   */
  http = xmalloc (sizeof (http_socket_t));
  memset (http, 0, sizeof (http_socket_t));
  http->pid = INVALID_HANDLE;
  http->keepalive = cfg->keepalive;
  sock->data = http;
	  
  /* 
   * set the socket flag, disable flood protection and
   * set all the callback routines
   */
  sock->userflags |= SOCK_FLAG_NOFLOOD;
  sock->check_request = http_check_request;
  sock->write_socket = http_default_write;
  sock->disconnected_socket = http_disconnect;

  return 0;
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
http_parse_date(char *date)
{
  struct tm parse_time;
  int n;
  char _month[4];
  char _wkday[10];
  time_t ret;

  static char month[12][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  switch(date[3])
    {
      /* ASCTIME-Date */
    case ' ':
      sscanf(date, "%3s %3s %2d %02d:%02d:%02d %04d",
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
      sscanf(date, "%3s, %02d %3s %04d %02d:%02d:%02d GMT", 
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
      sscanf(date, "%s, %02d-%3s-%02d %02d:%02d:%02d GMT", 
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
  for(n=0; n<12; n++)
    if(!memcmp(_month, month[n], 3))
      parse_time.tm_mon = n;

  parse_time.tm_isdst = daylight;
  parse_time.tm_year -= 1900;
  ret = mktime(&parse_time);
  ret -= timezone;
  if(daylight > 0) ret += 3600;
  return ret;
}

/*
 * Parse part of the receive buffer for HTTP request properties
 * and store it in the socket structure SOCK. Return the amount of
 * properties found in the request.
 */
int
http_parse_property(socket_t sock, char *request, char *end)
{
  int properties, n;
  char *p;
  http_socket_t *http;

  /* get the http socket structure */
  http = sock->data;

  /* reserve data space for the http properties */
  http->property = xmalloc(MAX_HTTP_PROPERTIES * 2 * sizeof(char *));
  properties = 0;
  n = 0;

  /* find out properties if necessary */
  while(INT16(request) != CRLF && 
	properties < MAX_HTTP_PROPERTIES - 1)
    {
      /* get property entity identifier */
      p = request;
      while(*p != ':' && p < end) p++;
      if(p == end) break;
      http->property[n] = xmalloc(p-request+1);
      strncpy(http->property[n], request, p-request);
      http->property[n][p-request] = 0;
      n++;
      request = p+2;

      /* get property entitiy body */
      while(INT16(p) != CRLF && p < end) p++;
      if(p == end || p <= request) break;
      http->property[n] = xmalloc(p-request+1);
      strncpy(http->property[n], request, p-request);
      http->property[n][p-request] = 0;
      n++;
      properties++;
      request = p+2;

#if 0
      printf("http header: {%s} = {%s}\n", 
	     http->property[n-2], http->property[n-1]);
#endif
    }

  request+=2;
  http->property[n] = NULL;

  return properties;
}

/*
 * Find a given property entity in the HTTP request properties.
 * Return a NULL pointer if not found.
 */
char *
http_find_property(http_socket_t *http, char *key)
{
  int n;

  /* search through all the http properties */
  n = 0;
  while(http->property[n])
    {
      if(!strcasecmp(http->property[n], key))
	{
	  return http->property[n+1];
	}
      n+=2;
    }
  return NULL;
}

/*
 * This routine is called from http_check_request if there was
 * seen a full HTTP request (ends with a double CRLF).
 */
int
http_handle_request(socket_t sock, int len)
{
  int n;
  char *p, *line, *end;
  char *request;
  char *option;
  int flag;
  int version[2];
  
  line = sock->recv_buffer;
  end = sock->recv_buffer + len;
  p = line;
  flag = 0;

  /* scan request type */
  while(*p != ' ' && p < end) p++;
  if(p == end)
    {
      return -1;
    }
  *p = 0;
  request = xmalloc(p - line + 1);
  strcpy(request, line);
  line = p + 1;

  /* scan the option (file) */
  while(*p != '\r' && p < end) p++;
  if(p == end)
    {
      xfree(request);
      return -1;
    }

  /* scan back until beginning of HTTP version */
  while(*p != ' ' && *p) p--;

  /* is this a HTTP/0.9 request ? */
  if(!memcmp(request, "GET", 3) && memcmp(p+1, "HTTP/", 5))
    {
      flag |= HTTP_FLAG_SIMPLE;
      while(*p != '\r') p++;
      option = xmalloc(p - line + 1);
      strncpy(option, line, p - line);
      option[p-line] = 0;
      line = p;
    }
  /* no, it is a real HTTP/1.x request */
  else
    {
      if(p <= line)
	{
	  xfree(request);
	  return -1;
	}
      *p = 0;
      option = xmalloc(p - line + 1);
      strcpy(option, line);
      line = p + 1;
  
      /* scan the version string of the HTTP request */
      if(memcmp(line, "HTTP/", 5))
	{
	  xfree(request);
	  xfree(option);
	  return -1;
	}
      line += 5;
      version[MAJOR_VERSION] = *line - '0';
      line += 2;
      version[MINOR_VERSION] = *line - '0';
      line++;
    }

  /* check the remaining part of the first line the version */
  if(((version[MAJOR_VERSION] != HTTP_MAJOR_VERSION ||
       version[MINOR_VERSION] > 1 ||*(line-2) != '.') && !flag) || 
     INT16(line) != CRLF)
    {
      xfree(request);
      xfree(option);
      return -1;
    }
  line+=2;

  /* find out properties */
  http_parse_property(sock, line, end);

  /* find an apropiate request callback */
  for(n=0; n<HTTP_REQUESTS; n++)
    {
      if(!strcmp(request, http_request[n].ident))
	{
#if ENABLE_DEBUG
	  log_printf(LOG_DEBUG, "http: %s received\n", request);
#endif
	  http_request[n].response(sock, option, flag);
	  break;
	}
    }

  /* Return a "404 Bad Request" if the request type is unknown. */
  if(n == HTTP_REQUESTS)
    {
      http_default_response(sock, option, 0);
    }

  xfree(request);
  xfree(option);
  return 0;
}

/*
 * Check in the receive buffer of socket SOCK for full
 * http request and call http_handle_request if necessary.
 */
int 
http_check_request(socket_t sock)
{
  char *p;
  int len;

  p = sock->recv_buffer;

  while(p < sock->recv_buffer + sock->recv_buffer_fill - 3 && 
	INT32(p) != CRLF2)
    p++;
  
  if(INT32(p) == CRLF2 && 
     p < sock->recv_buffer + sock->recv_buffer_fill - 3)
    {
      len = p - sock->recv_buffer + 4;
      if(http_handle_request(sock, len))
	return -1;

      if(sock->recv_buffer_fill > len )
	{
	  memmove(sock->recv_buffer, sock->recv_buffer + len,
		  sock->recv_buffer_fill - len);
	}
      sock->recv_buffer_fill -= len;
    }

  return 0;
}

/*
 * This routine gets all available content types from a given
 * file which should have kind of "/etc/mime.types"s format.
 */

#define TYPES_LINE_SIZE 1024

int
http_read_types (char *file)
{
  FILE *f;
  char *line;
  char *p, *end;
  char *content;
  char *suffix;

  /* try open the file */
  if ((f = fopen (file, "rt")) == NULL)
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
	      http_add_content_type (suffix, content);
	    }
	}
    }
  fclose (f);
  xfree (line);
  return 0;
}

/*
 * Get a content type from filename. We will use here something
 * similiar to the "/etc/mime.types" file.
 */
char *
get_content_type (char *file)
{
  int n = 0;
  char *type = file + strlen(file);

  while(type > file && *type != '.') type--;
  type++;

  while(http_content[n].suffix != NULL)
    {
      if(!strcasecmp(type, http_content[n].suffix))
	{
	  return http_content[n].type;
	}
      n++;
    }
  
  return http_content[DEFAULT_CONTENT].type;
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
  if ((type = hash_get (http_content_type, suffix)) != NULL)
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

/*
 * Respond to a http GET request. This could be either a usual file
 * request or a CGI request.
 */
int
http_get_response (socket_t sock, char *request, int flags)
{
  int fd;
  int size;
  char *file;
  struct stat buf;
  char *cgifile;
  char *dir;
  char *host;
  char *p;
  time_t date;
  HANDLE cgi2s[2];
  http_cache_t *cache;
  http_socket_t *http;
  http_config_t *cfg = sock->cfg;

  /* get the http socket structure */
  http = sock->data;

  /* check if this is a cgi request */
  cgifile = http_check_cgi(sock, request);
  if(cgifile != HTTP_NO_CGI)
    {
      if(cgifile == NULL)
	{
	  sock_printf(sock, HTTP_INTERNAL_ERROR "\r\n");
	  sock->userflags |= HTTP_FLAG_DONE;
	  return -1;
	}

      /* create a pipe for the cgi script process */
      if(create_pipe(cgi2s) == -1)
	{
	  sock_printf(sock, HTTP_INTERNAL_ERROR "\r\n");
	  sock->userflags |= HTTP_FLAG_DONE;
	  xfree(cgifile);
	  return -1;
	}

      /* execute the cgi script in CGIFILE */
      sock->userflags |= HTTP_FLAG_CGI;
      sock->read_socket = http_cgi_read;
      sock->pipe_desc[READ] = cgi2s[READ];
      if(http_cgi_exec(sock, INVALID_HANDLE, cgi2s[WRITE], 
		       cgifile, request, GET_METHOD))
	{
	  /* some error occured here */
	  sock->read_socket = default_read;
	  xfree(cgifile);
	  return -1;
	}
      xfree(cgifile);
      return 0;
    }

  /* this is a usual file request */
  size = 
    strlen(cfg->docs) + 
    strlen(request) + 
    strlen(cfg->indexfile) + 1;

  file = xmalloc(size);
  strcpy(file, cfg->docs);
  strcat(file, request);

  /* concate the IndexFile if necessary */
  if(file[strlen(file)-1] == '/')
    {
      strcat(file, cfg->indexfile);
      /* get directory listing if there is no index file */
      if((fd = open(file, O_RDONLY)) == -1)
	{
	  strcpy(file, cfg->docs);
	  strcat(file, request);
	  if((dir = http_dirlist(file, cfg->docs)) == NULL)
	    {
	      log_printf(LOG_ERROR, "http dirlist: %s: %s\n", 
			 file, SYS_ERROR);
	      sock_printf(sock, HTTP_FILE_NOT_FOUND "\r\n");
	      sock->userflags |= HTTP_FLAG_DONE;
	      xfree(file);
	      return -1;
	    }
	  /* send the directory listing */
	  xfree(sock->send_buffer);
	  sock->send_buffer = dir;
	  sock->send_buffer_size = http_dirlist_size;
	  sock->send_buffer_fill = strlen(dir);
	  sock->userflags |= HTTP_FLAG_DONE;
	  xfree(file);
	  return 0;
	}
      close(fd);
    }

  /* check if there are '..' in the requested file's path */
  if(strstr(request, "..") != NULL)
    {
      sock_printf(sock, HTTP_ACCESS_DENIED "\r\n");
      sock->userflags |= HTTP_FLAG_DONE;
      xfree(file);
      return -1;
    }

  /* get length of file and other properties */
  if(stat(file, &buf) == -1)
    {
      log_printf(LOG_ERROR, "stat: %s (%s)\n", SYS_ERROR, file);
      sock_printf(sock, HTTP_FILE_NOT_FOUND "\r\n");
      sock->userflags |= HTTP_FLAG_DONE;
      xfree(file);
      return -1;
    }

  /* if directory then relocate to it */
  if(S_ISDIR(buf.st_mode))
    {
      host = http_find_property(http, "Host");
      sock_printf(sock, "%sLocation: %s%s%s/\r\n\r\n", 
		  HTTP_RELOCATE, 
		  host ? "http://" : "", 
		  host ? host : "", 
		  request);
      sock->userflags |= HTTP_FLAG_DONE;
      xfree(file);
      return 0;
    }

  /* open the file for reading */
#ifdef __MINGW32__
  if((fd = open (file, O_RDONLY | O_BINARY)) == -1)
#else
  if((fd = open (file, O_RDONLY | O_NONBLOCK)) == -1)
#endif
    {
      log_printf (LOG_ERROR, "open: %s\n", SYS_ERROR);
      sock_printf (sock, HTTP_FILE_NOT_FOUND "\r\n");
      sock->userflags |= HTTP_FLAG_DONE;
      xfree (file);
      return -1;
    }

  /* check if this it could be a Keep-Alive connection */
  if((p = http_find_property(http, "Connection")) != NULL)
    {
      /*
      if(!strcasecmp(p, "Keep-Alive"))
      */
      if(strstr(p, "Keep-Alive"))
	{
	  sock->userflags |= HTTP_FLAG_KEEP;
	}
    }

  /* check if this a If-Modified-Since request */
  if((p = http_find_property(http, "If-Modified-Since")) != NULL)
    {
      date = http_parse_date(p);
      if(date >= buf.st_mtime)
	{
#if ENABLE_DEBUG
	  log_printf(LOG_DEBUG, "http: %s not changed\n", file);
#endif
	  sock_printf(sock, HTTP_NOT_MODIFIED);
	  http_check_keepalive(sock);
	  sock_printf(sock, "\r\n");

	  close(fd);
	  sock->userflags |= HTTP_FLAG_DONE;
	  xfree(file);
	  return 0;
	}
    }

  /* send a http header to the client */
  if(!(flags & HTTP_FLAG_SIMPLE))
    {
      sock_printf(sock, HTTP_OK);
      sock_printf(sock, "Content-Type: %s\r\n", get_content_type(file));
      sock_printf(sock, "Content-Length: %d\r\n", buf.st_size);
      sock_printf(sock, "Server: %s/%s\r\n", 
		  serveez_config.program_name,
		  serveez_config.version_string);
      sock_printf(sock, "Date: %s\r\n", http_asc_date (time (NULL)));
      sock_printf(sock, "Last-Modified: %s\r\n", http_asc_date (buf.st_mtime));

      http_check_keepalive(sock);

      /* request foorter */
      sock_printf(sock, "\r\n");
    }

  /* just a HEAD response handled by this GET handler */
  if(flags & HTTP_FLAG_NOFILE)
    {
      close(fd);
      sock->userflags |= HTTP_FLAG_DONE;
      xfree(file);
      return 0;
    }

  /* create a cache structure for the http socket structure */
  cache = xmalloc(sizeof(http_cache_t));
  http->cache = cache;
      
  /* is the requested file already in the cache ? */
  if(http_check_cache(file, cache) != -1)
    {
      if(buf.st_mtime > cache->entry->modified ||
	 buf.st_size != cache->entry->length)
	{
	  /* the file on disk has changed ? */
#if ENABLE_DEBUG
	  log_printf(LOG_DEBUG, "cache: %s has changed\n", file);
#endif
	  http_refresh_cache(cache);
	  cache->entry->modified = buf.st_mtime;
	  sock->userflags |= HTTP_FLAG_FILE;
	  sock->file_desc = fd;
	  http->filelength = buf.st_size;
	  sock->read_socket = http_cache_read;
	}
      else
	{
	  /* no, initialize the cache routines */
	  cache->entry->hits++;
	  cache->entry->usage++;
	  sock->userflags |= HTTP_FLAG_CACHE;
	  if(flags & HTTP_FLAG_SIMPLE)
	    {
	      sock->send_buffer_fill = 42;
	      sock->write_socket = http_cache_write;
	    }
	  close(fd);
	}
    }
  /* the fils is not in the cache structures yet */
  else
    {
      sock->file_desc = fd;
      http->filelength = buf.st_size;
      sock->userflags |= HTTP_FLAG_FILE;

      /* 
       * find a free slot for the new file if it is not larger
       * than a certain size
       */
      if(buf.st_size < cfg->cachesize &&
	 http_init_cache(file, cache) != -1)
	{
	  sock->read_socket = http_cache_read;
	  cache->entry->modified = buf.st_mtime;
	}
      else
	{
	  sock->read_socket = http_file_read;
	}
    }

  xfree(file);

  return 0;
}

/*
 * Respond to a http HEAD request. This is in particular the same as
 * GET but you do not respond with the file but with the header and all
 * the file info.
 */
int
http_head_response(socket_t sock, char *request, int flags)
{
  http_get_response(sock, request, flags | HTTP_FLAG_NOFILE);
  return 0;
}

/*
 * The default http response (Bad Request).
 */
int
http_default_response(socket_t sock, char *request, int flags)
{
  sock_printf(sock, HTTP_NOT_IMPLEMENTED "\r\n");
  sock->userflags |= HTTP_FLAG_DONE;
  return 0;
}

#ifdef __MINGW32__
/*
 * Check if there died a process handle in Win32, this has to be
 * done regularly here because there is no SIGCHLD in Win32 !
 */
int
http_check_cgi (socket_t sock)
{
  DWORD result;
  http_socket_t *http = sock->data;

  if (sock->userflags & HTTP_FLAG_CGI && http->pid != INVALID_HANDLE)
    {
      result = WaitForSingleObject (http->pid, LEAST_WAIT_OBJECT);
      if (result == WAIT_FAILED)
	{
	  log_printf (LOG_ERROR, "WaitForSingleObject: %s\n", SYS_ERROR);
	}
      else if (result != WAIT_TIMEOUT)
	{
	  if (CLOSE_HANDLE (http->pid) == -1)
	    log_printf (LOG_ERROR, "CloseHandle: %s\n", SYS_ERROR);
	  http->pid = INVALID_HANDLE;
	  server_coserver_died = http->pid;
	}
    }
  return 0;
}
#endif /* __MINGW32__ */

/* FIXME: that was if (server_coserver_died) */
int
http_find_died_cgi (void)
{
  http_socket_t *http;
  socket_t sock;

  /*
   * Go through all sockets and check whether the died PID
   * was a CGI script...
   */
  sock = socket_root; 
  while (sock)
    {
      http = sock->data;
      if (sock->userflags & HTTP_FLAG)
	{
	  if (1/*http->pid == server_coserver_died*/)
	    {
	      log_printf (LOG_NOTICE, "cgi script pid %d died\n", 
			  (int) 1/*server_coserver_died*/);
	      /*server_coserver_died = 0;*/
	      break;
	    }
	}
      sock = sock->next;
    }

  return 0;
}

int have_http = 1;

#else /* ENABLE_HTTP_PROTO */

int have_http = 0;	/* Shut compiler warnings up, remember for runtime */

#endif /* not ENABLE_HTTP_PROTO */
