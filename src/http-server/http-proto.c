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
 * $Id: http-proto.c,v 1.47 2000/12/03 14:37:32 ela Exp $
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#if HAVE_STRINGS_H
# include <strings.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_SYS_SENDFILE_H
# include <sys/sendfile.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
# include <io.h>
#endif

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# if HAVE_WAIT_H
#  include <wait.h>
# endif
# if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
# endif
#endif

#if HAVE_NETINET_TCP_H
# include <netinet/tcp.h>
#endif

#include "util.h"
#include "alloc.h"
#include "hash.h"
#include "socket.h"
#include "pipe-socket.h"
#include "serveez.h"
#include "server.h"
#include "server-socket.h"
#include "server-core.h"
#include "http-proto.h"
#include "http-core.h"
#include "http-cgi.h"
#include "http-dirlist.h"
#include "http-cache.h"
#include "coserver/coserver.h"

/*
 * The http port configuration.
 */
portcfg_t http_port =
{
  PROTO_TCP,  /* TCP protocol definition */
  42424,      /* preferred port */
  "*",        /* preferred local ip address */
  NULL,       /* calculated automatically later */
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
  MAX_CACHE,          /* maximum amount of cache entries */
  HTTP_TIMEOUT,       /* server shuts connection down after x seconds */
  HTTP_MAXKEEPALIVE,  /* how many files when using keep-alive */
  "text/plain",       /* standard content type */
  "/etc/mime.types",  /* standard content type file */
  NULL,               /* current content type hash */
  NULL,               /* cgi application associations */
  "root@localhost",   /* email address of server administrator */
  NULL,               /* host name of which is sent back to clients */
  "public_html",      /* appended onto a user's home (~user request) */
  0,                  /* enable reverse DNS lookups */
  "http-access.log",  /* log file name */
  NULL,               /* custom log file format string */
  NULL                /* log file stream */
};

/*
 * Definition of the configuration items processed by libsizzle (taken
 * from the config file).
 */
key_value_pair_t http_config_prototype [] =
{
  REGISTER_PORTCFG ("netport", http_config.port, DEFAULTABLE),
  REGISTER_STR ("indexfile", http_config.indexfile, DEFAULTABLE),
  REGISTER_STR ("docs", http_config.docs, DEFAULTABLE),
  REGISTER_STR ("cgi-url", http_config.cgiurl, DEFAULTABLE),
  REGISTER_STR ("cgi-dir", http_config.cgidir, DEFAULTABLE),
  REGISTER_INT ("cache-size", http_config.cachesize, DEFAULTABLE),
  REGISTER_INT ("cache-entries", http_config.cacheentries, DEFAULTABLE),
  REGISTER_INT ("timeout", http_config.timeout, DEFAULTABLE),
  REGISTER_INT ("keepalive", http_config.keepalive, DEFAULTABLE),
  REGISTER_STR ("default-type", http_config.default_type, DEFAULTABLE),
  REGISTER_STR ("type-file", http_config.type_file, DEFAULTABLE),
  REGISTER_HASH ("types", http_config.types, DEFAULTABLE),
  REGISTER_HASH ("cgi-application", http_config.cgiapps, DEFAULTABLE),
  REGISTER_STR ("admin", http_config.admin, DEFAULTABLE),
  REGISTER_STR ("host", http_config.host, DEFAULTABLE),
  REGISTER_STR ("logfile", http_config.logfile, DEFAULTABLE),
  REGISTER_STR ("logformat", http_config.logformat, DEFAULTABLE),
  REGISTER_STR ("userdir", http_config.userdir, DEFAULTABLE),
  REGISTER_INT ("nslookup", http_config.nslookup, DEFAULTABLE),
  REGISTER_END ()
};

/*
 * Definition of the http server.
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
  http_info_client,      /* client info */
  http_info_server,      /* server info */
  NULL,                  /* server timer */
  NULL,                  /* handle request callback */
  &http_config,          /* default configuration */
  sizeof (http_config),  /* size of the configuration */
  http_config_prototype  /* configuration prototypes (libsizzle) */
};

/*
 * HTTP request types, their identification string (including its length)
 * and the appropriate callback routine itself. This array is used in
 * the HTTP_HANDLE_REQUEST function.
 */
struct
{
  char *ident;                            /* identification string */
  int len;                                /* the length of this string */
  int (*response)(socket_t, char *, int); /* the callback routine */
} 
http_request [HTTP_REQUESTS] = 
{

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
 * Global http server initializer.
 */
int
http_global_init (void)
{
#ifdef __MINGW32__
  http_start_netapi ();
#endif /* __MINGW32__ */
  http_alloc_cache (MAX_CACHE);
  return 0;
}

/*
 * Global http server finalizer.
 */
int
http_global_finalize (void)
{
  http_free_cache ();
#ifdef __MINGW32__
  http_stop_netapi ();
#endif /* __MINGW32__ */
  return 0;
}

/*
 * Local http server instance initializer.
 */
int
http_init (server_t *server)
{
  int types = 0;
  char *p;
  unsigned long host;
  http_config_t *cfg = server->cfg;

  /* resolve localhost if server name is not set */
  if (!cfg->host)
    {
      host = cfg->port->localaddr->sin_addr.s_addr;
      if (host == INADDR_ANY) host = htonl (INADDR_LOOPBACK);
      coserver_reverse (host, http_localhost, cfg, NULL);
    }

  /* start http logging system */
  if (cfg->logfile)
    {
      if ((cfg->log = fopen (cfg->logfile, "at")) == NULL)
	{
	  log_printf (LOG_ERROR, "http: fopen: %s\n", SYS_ERROR);
	  log_printf (LOG_ERROR, "http: cannot open access logfile %s\n",
		      cfg->logfile);
	}
    }
  
  /* create content type hash */
  if (*(cfg->types))
    types = hash_size (*(cfg->types));
  
  if (http_read_types (cfg))
    {
      log_printf (LOG_ERROR, "http: unable to load %s\n", cfg->type_file);
    }
  log_printf (LOG_NOTICE, "http: %d+%d known content types\n",
	      types, hash_size (*(cfg->types)) - types);

  /* check user directory path, snip trailing '/' or '\' */
  if (!cfg->userdir || !strlen (cfg->userdir))
    {
      log_printf (LOG_ERROR, "http: not a valid user directory\n");
      return -1;
    }
  p = cfg->userdir + strlen (cfg->userdir) - 1;
  if (*p == '/' || *p == '\\') *p = '\0';

  /* check document root path */
  if (!strlen (cfg->docs))
    {
      log_printf (LOG_ERROR, "http: not a valid document root\n");
      return -1;
    }

  /* checking whether http doc root path ends in '/' or '\'. */
  p = cfg->docs + strlen (cfg->docs) - 1;
  if (*p == '/' || *p == '\\') *p = '\0';

  if (cfg->cacheentries > 0)
    http_alloc_cache (cfg->cacheentries);

  /* generate cgi associations */
  http_gen_cgi_apps (cfg);
  
  server_bind (server, cfg->port);
  return 0;
}

/*
 * Local http server instance finalizer.
 */
int
http_finalize (server_t *server)
{
  http_config_t *cfg = server->cfg;

  http_free_types (cfg);
  http_free_cgi_apps (cfg);
  if (cfg->log) fclose (cfg->log);

  return 0;
}

/*
 * This function frees all HTTP request properties previously reserved
 * and frees the cache structure if necessary. Nevertheless the 
 * socket structure SOCK should still be usable for keep-alive connections.
 */
void
http_free_socket (socket_t sock)
{
  http_socket_t *http = sock->data;
  int n;

  /* log this entry and free the request string */
  http_log (sock);
  if (http->request)
    {
      xfree (http->request);
      http->request = NULL;
    }
  http->timestamp = 0;
  http->response = 0;
  http->length = 0;

  /* any property at all ? */
  if (http->property)
    {
      /* go through all properties */
      n = 0;
      while (http->property[n])
	{
	  xfree (http->property[n]);
	  n++;
	}
      xfree (http->property);
      http->property = NULL;
    }

  /* decrement usage counter of the cache entry */
  if (sock->userflags & HTTP_FLAG_CACHE)
    {
      http->cache->entry->usage--;
    }

  /* is the cache entry used ? */
  if (http->cache)
    {
      xfree (http->cache);
      http->cache = NULL;
    }

  /* close the file descriptor for usual http file transfer */
  if (sock->file_desc != -1)
    {
      if (close (sock->file_desc) == -1)
	log_printf (LOG_ERROR, "close: %s\n", SYS_ERROR);
      sock->file_desc = -1;
    }
}

/*
 * Disconnects a HTTP connection. Callback routine for the
 * socket structure entry "disconnected_socket".
 */
int
http_disconnect (socket_t sock)
{
  /* get http socket structure */
  http_socket_t *http = sock->data;

  /* free the http socket structures */
  http_free_socket (sock);

  if (http)
    {
      if (http->host) xfree (http->host);
      xfree (http);
      sock->data = NULL;
    }

  return 0;
}

/*
 * This is the default idle function for http connections. It checks 
 * whether any died child was a cgi script.
 */
int
http_cgi_died (socket_t sock)
{
  http_socket_t *http = sock->data;
#ifdef __MINGW32__
  DWORD result;
#endif

  if (sock->flags & SOCK_FLAG_PIPE)
    {
#ifndef __MINGW32__
      /* Check if a died child is this cgi. */
      if (server_child_died && http->pid == server_child_died)
	{
	  log_printf (LOG_NOTICE, "cgi script pid %d died\n", 
		      (int) server_child_died);
	  server_child_died = 0;
	}
#if HAVE_WAITPID
      /* Test if the cgi is still running. */
      if (waitpid (http->pid, NULL, WNOHANG | WUNTRACED) == http->pid)
	{
	  log_printf (LOG_NOTICE, "cgi script pid %d died\n", 
		      (int) http->pid);
	  http->pid = INVALID_HANDLE;
	}
#endif /* HAVE_WAITPID */

#else /* __MINGW32__ */

      /*
       * Check if there died a process handle in Win32, this has to be
       * done regularly here because there is no SIGCHLD in Win32 !
       */
      if (http->pid != INVALID_HANDLE)
	{
	  result = WaitForSingleObject (http->pid, LEAST_WAIT_OBJECT);
	  if (result == WAIT_FAILED)
	    {
	      log_printf (LOG_ERROR, "WaitForSingleObject: %s\n", SYS_ERROR);
	    }
	  else if (result != WAIT_TIMEOUT)
	    {
	      if (closehandle (http->pid) == -1)
		log_printf (LOG_ERROR, "CloseHandle: %s\n", SYS_ERROR);
	      server_child_died = http->pid;
	      http->pid = INVALID_HANDLE;
	    }
	}
#endif /* __MINGW32__ */
    }
  
  sock->idle_counter = 1;
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

  now = time (NULL);
  if (now - sock->last_recv > cfg->timeout &&
      now - sock->last_send > cfg->timeout)
    return -1;
  sock->idle_counter = 1;

  return http_cgi_died (sock);
}

#if HAVE_SENDFILE
/*
 * Enable and disable the TCP_CORK socket option. This is useful for 
 * performance reasons when using sendfile() with a prepending header
 * to deliver a http document.
 */
static int
http_tcp_cork (SOCKET sock, int set)
{
#ifdef TCP_CORK
  int flags;

  /* get current socket options */
  if ((flags = fcntl (sock, F_GETFL)) < 0)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", NET_ERROR);
      return -1;
    }

  /* set / unset cork option */
  if (set) flags |= TCP_CORK;
  else     flags &= ~TCP_CORK;

  /* set new socket option */
  if (fcntl (sock, F_SETFL, flags) < 0)
    {
      log_printf (LOG_ERROR, "fcntl: %s\n", NET_ERROR);
      return -1;
    }

#endif /* TCP_CORK */
  return 0;
}

/*
 * This routine is using sendfile() to transport large file's content
 * to a network socket. It is replacing HTTP_DEFAULT_WRITE on systems where
 * this function is implemented. Furthermore you do not need to set
 * the READ_SOCKET callback HTTP_FILE_READ.
 */
int
http_send_file (socket_t sock)
{
  http_socket_t *http = sock->data;
  int num_written;

  /* Try sending throughout file descriptor to socket. */
  num_written = sendfile (sock->sock_desc, sock->file_desc,
			  &http->fileoffset, SOCK_MAX_WRITE);
  
  /* Some error occurred. */
  if (num_written < 0)
    {
      log_printf (LOG_ERROR, "http: sendfile: %s\n", SYS_ERROR);
      return -1;
    }

  /* Bogus file. File size from stat() was not true. */
  if (num_written == 0 && http->filelength != 0)
    {
      return -1;
    }

  /* Data has been read or EOF reached, set the appropriate flags. */
  http->filelength -= num_written;
  http->length += num_written;

  /* Read all file data ? */
  if (http->filelength <= 0)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "http: file successfully sent\n");
#endif
      /* 
       * no further read()s from the file descriptor, signaling 
       * the writers there will not be additional data from now on
       */
      sock->read_socket = default_read;
      sock->recv_buffer_fill = 0;
      sock->send_buffer_fill = 0;
      sock->write_socket = http_default_write;
      sock->userflags &= ~HTTP_FLAG_SENDFILE;
      num_written = http_keep_alive (sock);
      http_tcp_cork (sock->sock_desc, 0);
    }

  return (num_written < 0) ? -1 : 0;
}
#endif /* HAVE_SENDFILE */

/*
 * HTTP_DEFAULT_WRITE will shutdown the connection immediately when 
 * the whole response has been sent (indicated by the HTTP_FLAG_DONE
 * flag) with two exceptions. It will keep the connection if the
 * actual file is within the cache and if this is a keep-alive connection.
 */
int
http_default_write (socket_t sock)
{
  int num_written;

  /* 
   * Write as many bytes as possible, remember how many
   * were actually sent.
   */
  num_written = send (sock->sock_desc, sock->send_buffer,
		      sock->send_buffer_fill, 0);

  /* some data has been written */
  if (num_written > 0)
    {
      sock->last_send = time (NULL);

      if (sock->send_buffer_fill > num_written)
	{
	  memmove (sock->send_buffer, 
		   sock->send_buffer + num_written,
		   sock->send_buffer_fill - num_written);
	}
      sock->send_buffer_fill -= num_written;
    }

  /* write error occurred */
  else if (num_written < 0)
    {
      log_printf (LOG_ERROR, "http: send: %s\n", NET_ERROR);
      if (last_errno == SOCK_UNAVAILABLE)
	{
	  sock->unavailable = time (NULL) + RELAX_FD_TIME;
	  num_written = 0;
	}
    }

  /*
   * Check if the http response has (success)fully been sent.
   * If yes then return non-zero in order to shutdown the socket SOCK
   * and return zero if it is a keep-alive connection.
   */
  if ((sock->userflags & HTTP_FLAG_DONE) && sock->send_buffer_fill == 0)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "http: response successfully sent\n");
#endif
      num_written = http_keep_alive (sock);
    }

  /*
   * If the requested file is within the cache then start now the 
   * cache writer. Set SEND_BUFFER_FILL to something greater than zero.
   */
  if (sock->send_buffer_fill == 0)
    {
      if (sock->userflags & HTTP_FLAG_CACHE)
	{
	  sock->send_buffer_fill = 42;
	  sock->write_socket = http_cache_write;
	}
#if HAVE_SENDFILE
      else if (sock->userflags & HTTP_FLAG_SENDFILE)
	{
	  sock->send_buffer_fill = 42;
	  sock->write_socket = http_send_file;
	}
#endif /* HAVE_SENDFILE */
    }

  /*
   * Return a non-zero value if an error occurred.
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

  /* Read error occurred. */
  if (num_read < 0)
    {
      log_printf (LOG_ERROR, "http: read: %s\n", SYS_ERROR);
      return -1;
    }

  /* Bogus file. File size from stat() was not true. */
  if (num_read == 0 && http->filelength != 0)
    {
      return -1;
    }

  /* Data has been read or EOF reached, set the appropriate flags. */
  sock->send_buffer_fill += num_read;
  http->filelength -= num_read;
  http->length += num_read;

  /* Read all file data ? */
  if (http->filelength <= 0)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "http: file successfully read\n");
#endif
      /* 
       * no further read()s from the file descriptor, signaling 
       * the writers there will not be additional data from now on
       */
      sock->read_socket = default_read;
      sock->userflags |= HTTP_FLAG_DONE;
      sock->flags &= ~SOCK_FLAG_FILE;
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
	      log_printf (LOG_DEBUG, "http client detected\n");
#endif
	      return -1;
	    }
	}
    }

  return 0;
}

/*
 * When the http_detect_proto returns successfully this function must
 * be called to set all the appropriate callbacks and socket flags.
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

  /* start reverse dns lookup for logging purposes if necessary */
  if (cfg->nslookup)
    {
      coserver_reverse (sock->remote_addr, http_remotehost, 
			sock->id, sock->version);
    }

  /* 
   * set the socket flag, disable flood protection and
   * set all the callback routines
   */
  sock->flags |= SOCK_FLAG_NOFLOOD;
  sock->check_request = http_check_request;
  sock->write_socket = http_default_write;
  sock->disconnected_socket = http_disconnect;
  sock->idle_func = http_cgi_died;
  sock->idle_counter = 1;

  return 0;
}

/*
 * This routine is called from http_check_request if there was
 * seen a full HTTP request (ends with a double CRLF).
 */
int
http_handle_request (socket_t sock, int len)
{
  http_socket_t *http = sock->data;
  int n;
  char *p, *line, *end;
  char *request;
  char *uri;
  int flag;
  int version[2];
  
  line = sock->recv_buffer;
  end = sock->recv_buffer + len;
  p = line;
  flag = 0;

  /* scan the request type */
  while (*p != ' ' && p < end - 1) p++;
  if (p == end || *(p+1) != '/')
    {
      return -1;
    }
  *p = 0;
  request = xmalloc (p - line + 1);
  strcpy (request, line);
  line = p + 1;

  /* scan the URI (file), `line' points to first character */
  while (*p != '\r' && p < end) p++;
  if (p == end)
    {
      xfree (request);
      return -1;
    }

  /* scan back until beginning of HTTP version */
  while (*p != ' ' && *p) p--;

  /* is this a HTTP/0.9 request ? */
  if (!memcmp (request, "GET", 3) && memcmp (p+1, "HTTP/", 5))
    {
      flag |= HTTP_FLAG_SIMPLE;
      while (*p != '\r') p++;
      uri = xmalloc (p - line + 1);
      strncpy (uri, line, p - line);
      uri[p-line] = 0;
      line = p;
      version[MAJOR_VERSION] = 0;
      version[MINOR_VERSION] = 9;
    }
  /* no, it is a real HTTP/1.x request */
  else
    {
      if (p <= line)
	{
	  xfree (request);
	  return -1;
	}
      *p = 0;
      uri = xmalloc(p - line + 1);
      strcpy (uri, line);
      line = p + 1;
  
      /* scan the version string of the HTTP request */
      if (memcmp (line, "HTTP/", 5))
	{
	  xfree (request);
	  xfree (uri);
	  return -1;
	}
      line += 5;
      version[MAJOR_VERSION] = *line - '0';
      line += 2;
      version[MINOR_VERSION] = *line - '0';
      line++;
    }

  /* check the remaining part of the first line the version */
  if (((version[MAJOR_VERSION] != HTTP_MAJOR_VERSION ||
	version[MINOR_VERSION] > 1 ||*(line-2) != '.') && !flag) || 
      INT16 (line) != CRLF)
    {
      xfree (request);
      xfree (uri);
      return -1;
    }
  line += 2;

  /* find out properties */
  http_parse_property (sock, line, end);

  /* convert URI if necessary */
  http_process_uri (uri);

  /* assign request properties to http structure */
  http->timestamp = time (NULL);
  http->request = xmalloc (strlen (request) + strlen (uri) + 11);
  sprintf (http->request, "%s %s HTTP/%d.%d",
	   request, uri, version[MAJOR_VERSION], version[MINOR_VERSION]);

  /* find an appropriate request callback */
  for (n = 0; n < HTTP_REQUESTS; n++)
    {
      if (!memcmp (request, http_request[n].ident, http_request[n].len))
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "http: %s received\n", request);
#endif
	  http_request[n].response (sock, uri, flag);
	  break;
	}
    }

  /* Return a "404 Bad Request" if the request type is unknown. */
  if (n == HTTP_REQUESTS)
    {
      http_default_response (sock, uri, 0);
    }

  xfree (request);
  xfree (uri);
  return 0;
}

/*
 * Check in the receive buffer of socket SOCK for full
 * http request and call http_handle_request if necessary.
 */
int 
http_check_request (socket_t sock)
{
  char *p;
  int len;

  p = sock->recv_buffer;

  while (p < sock->recv_buffer + sock->recv_buffer_fill - 3 && 
	 INT32(p) != CRLF2)
    p++;
  
  if (INT32 (p) == CRLF2 && 
      p < sock->recv_buffer + sock->recv_buffer_fill - 3)
    {
      len = p - sock->recv_buffer + 4;
      if (http_handle_request (sock, len))
	return -1;

      if (sock->recv_buffer_fill > len)
	{
	  memmove (sock->recv_buffer, sock->recv_buffer + len,
		   sock->recv_buffer_fill - len);
	}
      sock->recv_buffer_fill -= len;
    }

  return 0;
}

/*
 * Server info callback for the http protocol. We are currently using 
 * it for displaying the server configuration within the control protocol.
 */
char *
http_info_server (server_t *server)
{
  http_config_t *cfg = server->cfg;
  static char info[80*12];
  
  sprintf (info,
	   " tcp port        : %d\r\n"
	   " index file      : %s\r\n"
	   " document root   : %s/\r\n"
	   " cgi url         : %s/\r\n"
	   " cgi directory   : %s/\r\n"
	   " cache file size : %d byte\r\n"
	   " cache entries   : %d files\r\n"
	   " timeout         : after %d secs\r\n"
	   " keep alive      : for %d requests\r\n"
	   " default type    : %s\r\n"
	   " type file       : %s\r\n"
	   " content types   : %d",
	   cfg->port->port,
	   cfg->indexfile,
	   cfg->docs,
	   cfg->cgiurl,
	   cfg->cgidir,
	   cfg->cachesize,
	   cfg->cacheentries,
	   cfg->timeout,
	   cfg->keepalive,
	   cfg->default_type,
	   cfg->type_file,
	   hash_size (*(cfg->types)));

  return info;
}

/*
 * Client info callback for the http protocol.
 */
char *
http_info_client (void *http_cfg, socket_t sock)
{
  http_config_t *cfg = http_cfg;
  http_socket_t *http = sock->data;
  http_cache_t *cache = http->cache;
  static char info[80*32], text[80*8];
  int n;

  sprintf (info, "This is a http client connection.\r\n\r\n");
#if HAVE_SENDFILE
  if (sock->userflags & HTTP_FLAG_SENDFILE)
    {
      sprintf (text, "  * delivering via sendfile() (offset: %lu)\r\n",
	       http->fileoffset);
      strcat (info, text);
    }
#endif /* HAVE_SENDFILE */
  if (sock->userflags & HTTP_FLAG_KEEP)
    {
      sprintf (text, 
	       "  * keeping connection alive, "
	       "%d requests and %d secs left\r\n",
	       http->keepalive, sock->idle_counter);
      strcat (info, text);
    }
  if (sock->userflags & HTTP_FLAG_CACHE)
    {
      sprintf (text, 
	       "  * sending cache entry\r\n"
	       "    file    : %s\r\n"
	       "    size    : %d of %d bytes sent\r\n"
	       "    usage   : %d\r\n"
	       "    hits    : %d\r\n"
	       "    urgency : %d\r\n"
	       "    ready   : %s\r\n"
	       "    date    : %s\r\n",
	       cache->entry->file,
	       cache->entry->size - cache->size, cache->entry->size,
	       cache->entry->usage,
	       cache->entry->hits,
	       cache->entry->urgent,
	       cache->entry->ready ? "yes" : "no",
	       http_asc_date (cache->entry->date));
      strcat (info, text);
    }
  if (sock->userflags & HTTP_FLAG_CGI)
    {
      sprintf (text, "  * sending cgi output (pid: %d)\r\n", (int) http->pid);
      strcat (info, text);
    }
  if (sock->userflags & HTTP_FLAG_POST)
    {
      sprintf (text, 
	       "  * receiving cgi input\r\n"
	       "    pid            : %d\r\n"
	       "    content-length : %d bytes left\r\n", 
	       (int) http->pid, 
	       http->contentlength);
      strcat (info, text);
    }
  sprintf (text, "  * %d bytes left of original file size\r\n",
	   http->filelength);
  strcat (info, text);

  /* append http header properties is possible */
  if (http->property)
    {
      strcat (info, "  * request property list:\r\n");
      n = 0;
      while (http->property[n])
	{
	  sprintf (text, "    %s => %s\r\n",
		   http->property[n], http->property[n+1]);
	  n += 2;
	  strcat (info, text);
	}
    }

  return info;
}

/*
 * Respond to a http GET request. This could be either a usual file
 * request or a CGI request.
 */
int
http_get_response (socket_t sock, char *request, int flags)
{
  int fd;
  int size, status, partial = 0;
  struct stat buf;
  char *cgifile, *dir, *host, *p, *file;
  time_t date;
  HANDLE cgi2s[2];
  http_cache_t *cache;
  http_socket_t *http;
  http_config_t *cfg = sock->cfg;

  /* reset current http header */
  http_reset_header ();

  /* get the http socket structure */
  http = sock->data;

  /* check if this is a cgi request */
  cgifile = http_check_cgi (sock, request);
  if (cgifile != HTTP_NO_CGI)
    {
      if (cgifile == NULL)
	{
	  sock_printf (sock, HTTP_INTERNAL_ERROR "\r\n");
	  http_error_response (sock, 500);
	  sock->userflags |= HTTP_FLAG_DONE;
	  return -1;
	}

      /* create a pipe for the cgi script process */
      if (pipe_create_pair (cgi2s) == -1)
	{
	  sock_printf (sock, HTTP_INTERNAL_ERROR "\r\n");
	  http_error_response (sock, 500);
	  sock->userflags |= HTTP_FLAG_DONE;
	  xfree (cgifile);
	  return -1;
	}

      /* execute the cgi script in CGIFILE */
      sock->userflags |= HTTP_FLAG_CGI;
      sock->flags |= SOCK_FLAG_RECV_PIPE;
      sock->read_socket = http_cgi_read;
      sock->pipe_desc[READ] = cgi2s[READ];
      if (http_cgi_exec (sock, INVALID_HANDLE, cgi2s[WRITE], 
			 cgifile, request, GET_METHOD))
	{
	  /* some error occurred here */
	  sock->read_socket = default_read;
	  xfree (cgifile);
	  return -1;
	}
      xfree (cgifile);
      return 0;
    }

  /* check for "~user" syntax here */
  if ((p = http_userdir (sock, request)) != NULL)
    {
      size = strlen (p) + strlen (cfg->indexfile) + 1;
      file = xmalloc (size);
      strcpy (file, p);
      xfree (p);
      status = 1;
    }
  /* this is a usual file request */
  else
    {
      size = 
	strlen (cfg->docs) + 
	strlen (request) + 
	strlen (cfg->indexfile) + 4;

      file = xmalloc (size);
      strcpy (file, cfg->docs);
      strcat (file, request);
      status = 0;
    }

  /* concate the IndexFile if necessary */
  if (file[strlen (file) - 1] == '/')
    {
      p = file + strlen (file);
      strcat (file, cfg->indexfile);

      /* get directory listing if there is no index file */
      if ((fd = open (file, O_RDONLY)) == -1)
	{
	  *p = '\0';
	  if ((dir = http_dirlist (file, cfg->docs, 
				   status ? request : NULL)) == NULL)
	    {
	      log_printf (LOG_ERROR, "http: dirlist: %s: %s\n", 
			  file, SYS_ERROR);
	      sock_printf (sock, HTTP_FILE_NOT_FOUND "\r\n");
	      http_error_response (sock, 404);
	      sock->userflags |= HTTP_FLAG_DONE;
	      xfree (file);
	      return -1;
	    }
	  /* send the directory listing */
	  http->response = 200;
	  http->length = strlen (dir);
	  xfree (sock->send_buffer);
	  sock->send_buffer = dir;
	  sock->send_buffer_size = http_dirlist_size;
	  sock->send_buffer_fill = strlen (dir);
	  sock->userflags |= HTTP_FLAG_DONE;
	  xfree (file);
	  return 0;
	}
      close (fd);
    }

  /* check if there are '..' in the requested file's path */
  if (strstr (request, "..") != NULL)
    {
      sock_printf (sock, HTTP_ACCESS_DENIED "\r\n");
      http_error_response (sock, 403);
      sock->userflags |= HTTP_FLAG_DONE;
      xfree (file);
      return -1;
    }

  /* get length of file and other properties */
  if (stat (file, &buf) == -1)
    {
      log_printf (LOG_ERROR, "stat: %s (%s)\n", SYS_ERROR, file);
      sock_printf (sock, HTTP_FILE_NOT_FOUND "\r\n");
      http_error_response (sock, 404);
      sock->userflags |= HTTP_FLAG_DONE;
      xfree (file);
      return -1;
    }

  /* make sure we do not send any devices or strange files */
  if (!(S_ISREG (buf.st_mode) ||
#ifdef S_ISLNK
	S_ISLNK (buf.st_mode) ||
#endif /* S_ISLNK */
	S_ISDIR (buf.st_mode)) )
    {
      log_printf (LOG_ERROR, "http: %s is not a regular file\n", file);
      sock_printf (sock, HTTP_ACCESS_DENIED "\r\n");
      http_error_response (sock, 403);
      sock->userflags |= HTTP_FLAG_DONE;
      xfree (file);
      return -1;
    }

  /* if directory then relocate to it */
  if (S_ISDIR (buf.st_mode))
    {
      host = http_find_property (http, "Host");
      http->response = 302;
      http_set_header (HTTP_RELOCATE);
      http_add_header ("Location: %s%s%s/\r\n", 
		       host ? "http://" : "", host ? host : "", request);
      http_send_header (sock);
      sock->userflags |= HTTP_FLAG_DONE;
      xfree (file);
      return 0;
    }

  /* open the file for reading */
#ifdef __MINGW32__
  if ((fd = open (file, O_RDONLY | O_BINARY)) == -1)
#else
  if ((fd = open (file, O_RDONLY | O_NONBLOCK)) == -1)
#endif
    {
      log_printf (LOG_ERROR, "open: %s\n", SYS_ERROR);
      sock_printf (sock, HTTP_FILE_NOT_FOUND "\r\n");
      http_error_response (sock, 404);
      sock->userflags |= HTTP_FLAG_DONE;
      xfree (file);
      return -1;
    }

  /* check if this it could be a Keep-Alive connection */
  if ((p = http_find_property (http, "Connection")) != NULL)
    {
      if (strstr (p, "Keep-Alive"))
	{
	  sock->userflags |= HTTP_FLAG_KEEP;
	}
    }

  /* check if this a If-Modified-Since request */
  if ((p = http_find_property (http, "If-Modified-Since")) != NULL)
    {
      date = http_parse_date (p);
      if (date >= buf.st_mtime)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "http: %s not changed\n", file);
#endif
	  http->response = 304;
	  http_set_header (HTTP_NOT_MODIFIED);
	  http_check_keepalive (sock);
	  http_send_header (sock);
	  close (fd);
	  sock->userflags |= HTTP_FLAG_DONE;
	  xfree (file);
	  return 0;
	}
    }

  /* check content range requests */
  if ((p = http_find_property (http, "Range")) != NULL)
    {
      if (http_get_range (p, &http->range) != -1)
	partial = 1;
    }
  else if ((p = http_find_property (http, "Request-Range")) != NULL)
    {
      if (http_get_range (p, &http->range) != -1)
	partial = 1;
    }

  /* check if partial content can be delivered or not */
  if (partial)
    {
      if (http->range.first <= 0 || 
	  (http->range.last && http->range.last <= http->range.first) ||
	  http->range.last >= buf.st_size ||
	  http->range.length > buf.st_size)
	partial = 0;

      http->range.length = buf.st_size;
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "http: partial content: %ld-%ld/%ld\n",
		  http->range.first, http->range.last, http->range.length);
#endif
    }

  /* send a http header to the client */
  if (!(flags & HTTP_FLAG_SIMPLE))
    {
      http->response = 200;
      http_set_header (HTTP_OK);
      http_add_header ("Content-Type: %s\r\n",
		       http_find_content_type (sock, file));
      if (buf.st_size > 0)
	http_add_header ("Content-Length: %d\r\n", buf.st_size);
      http_add_header ("Last-Modified: %s\r\n", http_asc_date (buf.st_mtime));
      http_add_header ("Accept-Ranges: bytes\r\n");
      http_check_keepalive (sock);
      http_send_header (sock);
    }

  /* just a HEAD response handled by this GET handler */
  if (flags & HTTP_FLAG_NOFILE)
    {
      close (fd);
      sock->userflags |= HTTP_FLAG_DONE;
      xfree (file);
      return 0;
    }

  /* create a cache structure for the http socket structure */
  cache = xmalloc (sizeof (http_cache_t));
  http->cache = cache;
      
  /* return the file's current cache status */
  status = http_check_cache (file, cache);

  /* is the requested file already fully in the cache ? */
  if (status == HTTP_CACHE_COMPLETE)
    {
      if (buf.st_mtime > cache->entry->date ||
	  buf.st_size != cache->entry->size)
	{
	  /* the file on disk has changed ? */
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "cache: %s has changed\n", file);
#endif
	  http_refresh_cache (cache);
	  cache->entry->date = buf.st_mtime;
	  sock->flags |= SOCK_FLAG_FILE;
	  sock->file_desc = fd;
	  http->filelength = buf.st_size;
	  sock->read_socket = http_cache_read;
	  sock->disconnected_socket = http_cache_disconnect;
	}
      else
	{
	  /* no, initialize the cache routines */
	  cache->entry->hits++;
	  cache->entry->usage++;
	  sock->userflags |= HTTP_FLAG_CACHE;
	  if (flags & HTTP_FLAG_SIMPLE)
	    {
	      sock->send_buffer_fill = 42;
	      sock->write_socket = http_cache_write;
	    }
	  close (fd);
	}
    }
  /* the file is not in the cache structures yet */
  else
    {
      sock->file_desc = fd;
      http->filelength = buf.st_size;
      sock->flags |= SOCK_FLAG_FILE;

      /* 
       * find a free slot for the new file if it is not larger
       * than a certain size and is not "partly" in the cache
       */
      if (status == HTTP_CACHE_NO && 
	  buf.st_size > 0 && buf.st_size < cfg->cachesize &&
	  http_init_cache (file, cache) != -1)
	{
	  sock->read_socket = http_cache_read;
	  sock->disconnected_socket = http_cache_disconnect;
	  cache->entry->date = buf.st_mtime;
	}
      /*
       * either the file is not cacheable or it is currently
       * going to be in the http cache (not yet cache->ready)
       */
      else
	{
#if HAVE_SENDFILE
	  sock->read_socket = NULL;
	  sock->flags &= ~SOCK_FLAG_FILE;
	  sock->userflags |= HTTP_FLAG_SENDFILE;
	  http_tcp_cork (sock->sock_desc, 1);
#else /* not HAVE_SENDFILE */
	  sock->read_socket = http_file_read;
#endif /* not HAVE_SENDFILE */
	}
    }

  xfree (file);

  return 0;
}

/*
 * Respond to a http HEAD request. This is in particular the same as
 * GET but you do not respond with the file but with the header and all
 * the file info.
 */
int
http_head_response (socket_t sock, char *request, int flags)
{
  http_get_response (sock, request, flags | HTTP_FLAG_NOFILE);
  return 0;
}

/*
 * The default http response (Bad Request).
 */
int
http_default_response (socket_t sock, char *request, int flags)
{
  sock_printf (sock, HTTP_NOT_IMPLEMENTED "\r\n");
  http_error_response (sock, 501);
  sock->userflags |= HTTP_FLAG_DONE;
  return 0;
}

int have_http = 1;

#else /* ENABLE_HTTP_PROTO */

int have_http = 0;	/* Shut compiler warnings up, remember for runtime */

#endif /* not ENABLE_HTTP_PROTO */
