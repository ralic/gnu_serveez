/*
 * http-proto.h - http protocol header file
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Portions (C) 1995, 1996 Free Software Foundation, Inc.
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
 */

#ifndef __HTTP_PROTO_H__
#define __HTTP_PROTO_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include "socket.h"
#include "http-cache.h"

/*
 * This is the http server configuration structure
 * filled in the init_http_server() routine.
 */
typedef struct
{
  char *indexfile;    /* the standard index file */
  char *docs;         /* http document root */
  char *cgiurl;       /* cgi url (this is for its detection) */
  char *cgidir;       /* cgi directory where all cgi scripts are located */
  int cachesize;      /* maximum cache file size */
  int timeout;        /* timeout in seconds for keep-alive connections */
  int keepalive;      /* maximum amount of requests on a connection */
  char *default_type; /* the default content type */
  char *type_file;    /* content type file (e.g "/etc/mime.types") */
} 
http_config_t;

/* The configuration itself. */
extern http_config_t http_config;

/*
 * This structure is used to process a HTTP connection. It will be stored
 * within the original socket structure.
 */
typedef struct http_socket http_socket_t;

struct http_socket
{
  http_cache_t *cache;   /* a http file cache structure */
  char **property;       /* property list of a http request */
  int contentlength;     /* the content length for the cgi pipe */
  int filelength;        /* content length for the http file */
  int keepalive;         /* how many requests left for a connection */
  HANDLE pid;            /* the pid of the cgi (process handle) */

#ifdef __MINGW32__
  OVERLAPPED overlap[2]; /* the overlap info for WinNT */
#endif
};

/* Some definitions. */
#define HTTP_MAJOR_VERSION  1          /* accepted MajorVersion */
#define MAJOR_VERSION       0          /* MajorVersion index */
#define MINOR_VERSION       1          /* MinorVersion index */
#define MAX_HTTP_PROPERTIES 32         /* all HTTP properties */
#define CRLF                0x0A0D     /* \r\n */
#define CRLF2               0x0A0D0A0D /* \r\n\r\n */
#define HTTP_REQUESTS       8          /* all kinds of requests */
#define DEFAULT_CONTENT     0          /* the default content type */
#define HTTP_TIMEOUT        15
#define HTTP_MAXKEEPALIVE   10

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

/* basic protocol functions */
int http_check_request(socket_t sock);
int http_detect_proto(socket_t sock);
int http_default_write(socket_t sock);
int http_disconnect(socket_t sock);
int http_connect (socket_t sock);

char *http_find_property(http_socket_t *sock, char *key);
int http_keep_alive(socket_t sock);
void http_check_keepalive(socket_t sock);
int http_read_types (char *file);
void http_free_content_types (void);

/* HTTP response functions including their flags */
int http_get_response(socket_t sock, char *request, int flags);
int http_head_response(socket_t sock, char *request, int flags);
int http_default_response(socket_t sock, char *request, int flags);

#define HTTP_FLAG_CACHE  0x0001 /* use cache if possible */
#define HTTP_FLAG_NOFILE 0x0002 /* do not send content, but header */
#define HTTP_FLAG_SIMPLE 0x0004 /* HTTP/0.9 simple GET */     

#endif /* __HTTP_PROTO_H__ */
