/*
 * http-core.h - http core definitions
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
 * $Id: http-core.h,v 1.1 2000/07/26 21:09:43 ela Exp $
 *
 */

#ifndef __HTTP_CORE_H__
#define __HTTP_CORE_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <time.h>

#include "socket.h"
#include "http-cache.h"

/*
 * This structure is used to process a http connection. It will be stored
 * within the original socket structure (sock->data).
 */
typedef struct http_socket http_socket_t;

struct http_socket
{
  http_cache_t *cache;   /* a http file cache structure */
  char **property;       /* property list of a http request */
  int contentlength;     /* the content length for the cgi pipe */
  int filelength;        /* content length for the http file */
  int keepalive;         /* how many requests left for a connection */
  off_t fileoffset;      /* file offset used by sendfile */
  HANDLE pid;            /* the pid of the cgi (process handle) */
};

/* Some definitions. */
#define HTTP_MAJOR_VERSION  1          /* accepted MajorVersion */
#define MAJOR_VERSION       0          /* MajorVersion index */
#define MINOR_VERSION       1          /* MinorVersion index */
#define MAX_HTTP_PROPERTIES 32         /* all http properties */
#define CRLF                0x0A0D     /* \r\n */
#define CRLF2               0x0A0D0A0D /* \r\n\r\n */
#define HTTP_REQUESTS       8          /* number of known request types */
#define HTTP_TIMEOUT        15         /* default timeout value */
#define HTTP_MAXKEEPALIVE   10         /* number of requests per connection */

#define HTTP_VERSION "HTTP/1.0"        /* the current HTTP protocol version */

/* HTTP resonse header definitions */
#define HTTP_OK              HTTP_VERSION " 200 OK\r\n"
#define HTTP_ACCEPTED        HTTP_VERSION " 202 Accepted\r\n"
#define HTTP_RELOCATE        HTTP_VERSION " 302 Temporary Relocation\r\n"
#define HTTP_NOT_MODIFIED    HTTP_VERSION " 304 Not Modified\r\n"
#define HTTP_BAD_REQUEST     HTTP_VERSION " 400 Bad Request\r\n"
#define HTTP_ACCESS_DENIED   HTTP_VERSION " 403 Forbidden\r\n"
#define HTTP_FILE_NOT_FOUND  HTTP_VERSION " 404 Not Found\r\n"
#define HTTP_INTERNAL_ERROR  HTTP_VERSION " 500 Internal Server Error\r\n"
#define HTTP_NOT_IMPLEMENTED HTTP_VERSION " 501 Not Implemented\r\n"

#define HTTP_FLAG_CACHE    0x0001 /* use cache if possible */
#define HTTP_FLAG_NOFILE   0x0002 /* do not send content, but header */
#define HTTP_FLAG_SIMPLE   0x0004 /* HTTP/0.9 simple GET */     
#define HTTP_FLAG_DONE     0x0008 /* http request done */
#define HTTP_FLAG_POST     0x0010 /* http cgi pipe posting data */
#define HTTP_FLAG_CGI      0x0020 /* http cgi pipe getting data */
#define HTTP_FLAG_KEEP     0x0040 /* keep alive connection */
#define HTTP_FLAG_SENDFILE 0x0080 /* use sendfile for HTTP requests */

/* all of the additional http flags */
#define HTTP_FLAG (HTTP_FLAG_DONE      | \
                   HTTP_FLAG_POST      | \
                   HTTP_FLAG_CGI       | \
                   HTTP_FLAG_CACHE     | \
                   HTTP_FLAG_KEEP      | \
                   HTTP_FLAG_SENDFILE)

/* exported http core functions */
int http_keep_alive (socket_t sock);
void http_check_keepalive (socket_t sock);

int http_read_types (http_config_t *cfg);
void http_free_types (http_config_t *cfg);
char *http_find_content_type (socket_t sock, char *file);

int http_parse_property (socket_t sock, char *request, char *end);
char *http_find_property (http_socket_t *sock, char *key);

void http_process_uri (char *uri);

time_t http_parse_date (char *date);
char *http_asc_date (time_t t);


#endif /* __HTTP_CORE_H__ */
