/*
 * http-proto.h - http protocol header file
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
 * $Id: http-proto.h,v 1.14 2000/11/02 12:51:57 ela Exp $
 *
 */

#ifndef __HTTP_PROTO_H__
#define __HTTP_PROTO_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>

#include "socket.h"
#include "hash.h"
#include "server.h"
#include "http-cache.h"

/*
 * This is the http server configuration structure for one instance.
 */
typedef struct
{
  portcfg_t *port;    /* tcp port configuration */
  char *indexfile;    /* the standard index file */
  char *docs;         /* http document root */
  char *cgiurl;       /* cgi url (this is for its detection) */
  char *cgidir;       /* cgi directory where all cgi scripts are located */
  int cachesize;      /* maximum cache file size */
  int cacheentries;   /* maximum cache entries */
  int timeout;        /* timeout in seconds for keep-alive connections */
  int keepalive;      /* maximum amount of requests on a connection */
  char *default_type; /* the default content type */
  char *type_file;    /* content type file (e.g "/etc/mime.types") */
  hash_t **types;     /* content type hash */
  hash_t **cgiapps;   /* cgi application associations */
  char *admin;        /* email address of server administrator */
  char *host;         /* host name of which is sent back to clients */
  char *userdir;      /* appended onto a user's home (~user request) */
  int nslookup;       /* enable reverse DNS lookups */
  char *logfile;      /* log file name */
  char *logformat;    /* custom log file format string */
  FILE *log;          /* log file stream */
} 
http_config_t;

/* Export the http server definition to `server.c'. */
extern server_definition_t http_server_definition;

/* server functions */
int http_init (server_t *server);
int http_finalize (server_t *server);
int http_global_init (void);
int http_global_finalize (void);

/* basic protocol functions */
int http_detect_proto (void *cfg, socket_t sock);
int http_connect_socket (void *cfg, socket_t sock);
char *http_info_client (void *cfg, socket_t sock);
char *http_info_server (server_t *server);

/* internal protocol functions */
int http_check_request (socket_t sock);
int http_default_write (socket_t sock);
int http_disconnect (socket_t sock);
void http_free_socket (socket_t sock);
int http_idle (socket_t sock);

/* http response functions including their flags */
int http_get_response (socket_t sock, char *request, int flags);
int http_head_response (socket_t sock, char *request, int flags);
int http_default_response (socket_t sock, char *request, int flags);

#endif /* __HTTP_PROTO_H__ */
