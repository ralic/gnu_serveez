/*
 * http-cgi.c - http cgi implementation
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
 * $Id: http-cgi.c,v 1.24 2000/10/25 07:54:06 ela Exp $
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
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <signal.h>

#if HAVE_STRINGS_H
# include <strings.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
# include <io.h>
# include <shellapi.h>
#endif

#ifndef __MINGW32__
# include <netinet/in.h>
#endif

#include "snprintf.h"
#include "util.h"
#include "pipe-socket.h"
#include "alloc.h"
#include "serveez.h"
#include "http-proto.h"
#include "http-core.h"
#include "http-cgi.h"

/*
 * Extended disconnect_socket callback for CGIs. Handling CGI related
 * topics and afterwards we process the normal http disconnection
 * functionality.
 */
int
http_cgi_disconnect (socket_t sock)
{
  http_socket_t *http = sock->data;

  /* flush CGI output if necessary */
  if (sock->flags & SOCK_FLAG_PIPE && sock->send_buffer_fill > 0)
    if (sock->write_socket)
      sock->write_socket (sock);

  /* close both of the CGI pipes if necessary */
  if (sock->pipe_desc[READ] != INVALID_HANDLE)
    {
      if (closehandle (sock->pipe_desc[READ]) == -1)
	log_printf (LOG_ERROR, "close: %s\n", SYS_ERROR);
      sock->pipe_desc[READ] = INVALID_HANDLE;
      sock->flags &= ~SOCK_FLAG_RECV_PIPE;
    }
  if (sock->pipe_desc[WRITE] != INVALID_HANDLE)
    {
      if (closehandle (sock->pipe_desc[WRITE]) == -1)
	log_printf (LOG_ERROR, "close: %s\n", SYS_ERROR);
      sock->pipe_desc[WRITE] = INVALID_HANDLE;
      sock->flags &= ~SOCK_FLAG_SEND_PIPE;
    }

#ifdef __MINGW32__
  /* 
   * Close the process handle if necessary, but only in the Windows-Port ! 
   */
  if (http->pid != INVALID_HANDLE)
    {
      if (!TerminateProcess (http->pid, 0))
	log_printf (LOG_ERROR, "TerminateProcess: %s\n", SYS_ERROR);
      if (closehandle (http->pid) == -1)
	log_printf (LOG_ERROR, "CloseHandle: %s\n", SYS_ERROR);
      http->pid = INVALID_HANDLE;
    }
#else /* not __MINGW32__ */
  /*
   * Try killing the cgi script.
   */
  if (http->pid != INVALID_HANDLE)
    {
      if (kill (http->pid, SIGKILL) == -1)
	log_printf (LOG_ERROR, "kill: %s\n", SYS_ERROR);
      http->pid = INVALID_HANDLE;
    }
#endif /* not __MINGW32__ */

  return http_disconnect (sock);
}

/*
 * The http cgi reader gets data from the stdout of a cgi
 * program and stores the data into the send buffer of
 * the socket structure. We set the HTTP_FLAG_DONE flag
 * to indicate there was no more data.
 */
int
http_cgi_read (socket_t sock)
{
  int do_read;
  int num_read;
  http_socket_t *http = sock->data;

  /* read as much space is left in the buffer */
  do_read = sock->send_buffer_size - sock->send_buffer_fill;
  if (do_read <= 0) 
    {
      return 0;
    }

#ifdef __MINGW32__
  /* check how many bytes could be read from the cgi pipe */
  if (!PeekNamedPipe (sock->pipe_desc[READ], NULL, 0, 
		      NULL, (DWORD *) &num_read, NULL))
    {
      log_printf (LOG_ERROR, "cgi: PeekNamedPipe: %s\n", SYS_ERROR);
      return -1;
    }

  /* adjust number of bytes to read */
  if (do_read > num_read) do_read = num_read;

  /* really read from pipe */
  if (!ReadFile (sock->pipe_desc[READ],
		 sock->send_buffer + sock->send_buffer_fill,
		 do_read, (DWORD *) &num_read, NULL))
    {
      log_printf (LOG_ERROR, "cgi: ReadFile: %s\n", SYS_ERROR);
      num_read = -1;
    }
#else /* not __MINGW32__ */
  if ((num_read = read (sock->pipe_desc[READ],
			sock->send_buffer + sock->send_buffer_fill,
			do_read)) == -1)
    {
      log_printf (LOG_ERROR, "cgi: read: %s\n", SYS_ERROR);
      num_read = -1;
    }
#endif /* not __MINGW32__ */

  /* data has been read */
  else if (num_read > 0)
    {
      sock->send_buffer_fill += num_read;
      return 0;
    }

#ifdef __MINGW32__
  /*
   * because pipes cannot be select()ed it can happen that there is no
   * data within the receiving pipe, but the cgi has not yet terminated
   */
  if (num_read == 0 && http->pid != INVALID_HANDLE)
    {
      return 0;
    }
#endif /* __MINGW32__ */

  /* no data has been received */
  sock->userflags |= HTTP_FLAG_DONE;
  if (sock->send_buffer_fill == 0)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "cgi: data successfully received and resent\n");
#endif
      sock->userflags &= ~HTTP_FLAG_CGI;
      sock->flags &= ~SOCK_FLAG_RECV_PIPE;
      return -1;
    }
  
  return 0;
}

/*
 * HTTP_CGI_WRITE pipes all read data from the http socket connection 
 * into the cgi stdin. This is necessary for the so called post method.
 * It directly reads from the RECV_BUFFER of the socket structure.
 */
int
http_cgi_write (socket_t sock)
{
  int do_write;
  int num_written;
  http_socket_t *http = sock->data;

  /* 
   * Write as many bytes as possible, remember how many
   * were actually sent. Do not write more than the content
   * length of the post data.
   */
  do_write = sock->recv_buffer_fill;
  if (do_write > http->contentlength)
    do_write = http->contentlength;

#ifdef __MINGW32__
  if (!WriteFile (sock->pipe_desc[WRITE], sock->recv_buffer, 
		  do_write, (DWORD *) &num_written, NULL))
    {
      log_printf (LOG_ERROR, "cgi: WriteFile: %s\n", SYS_ERROR);
      num_written = -1;
    }
#else
  if ((num_written = write (sock->pipe_desc[WRITE], 
			    sock->recv_buffer, 
			    do_write)) == -1)
    {
      log_printf (LOG_ERROR, "cgi: write: %s\n", SYS_ERROR);
    }
#endif

  /* data has been successfully sent */
  if (num_written > 0)
    {
      sock->last_send = time (NULL);

      /*
       * Shuffle the data in the output buffer around, so that
       * new data can get stuffed into it.
       */
      if (sock->recv_buffer_fill > num_written)
	{
	  memmove (sock->recv_buffer, 
		   sock->recv_buffer + num_written,
		   sock->recv_buffer_fill - num_written);
	}
      sock->recv_buffer_fill -= num_written;
      http->contentlength -= num_written;
    }

  /* 
   * If we have written all data to the CGI stdin, we can now start
   * reading from the CGI's stdout and write again to the http
   * connection.
   */
  if (http->contentlength <= 0)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "cgi: post data sent to cgi\n");
#endif
      sock->userflags &= ~HTTP_FLAG_POST;
      sock->flags &= ~SOCK_FLAG_SEND_PIPE;
      sock->userflags |= HTTP_FLAG_CGI;
      sock->flags |= SOCK_FLAG_RECV_PIPE;
      sock->read_socket = http_cgi_read;
      sock->write_socket = http_default_write;
    }

  /*
   * Return a non-zero value if an error occurred.
   */
  return (num_written < 0) ? -1 : 0;
}

/*
 * Insert a further variable into the environment block. How
 * we really do it depends on the system we compile with.
 */
static void
http_insert_env (ENV_BLOCK_TYPE env, /* the block to add the variable to */
		 int *length,        /* pointer to the current length of it */
		 char *fmt,          /* format string */
		 ...)                /* arguments for the format string */
{
  va_list args;

  va_start (args, fmt);
#ifdef __MINGW32__
  /* Windows: */
  if (*length >= ENV_BLOCK_SIZE)
    {
      log_printf (LOG_WARNING, "cgi: env block size %d reached\n", 
		  ENV_BLOCK_SIZE);
    }
  else
    {
      vsnprintf (&env[*length], ENV_BLOCK_SIZE - (*length), fmt, args);
      *length += strlen (&env[*length]) + 1;
    }
#else
  /* Unices: */
  if (*length >= ENV_ENTRIES-1)
    {
      log_printf (LOG_WARNING, "cgi: all env entries %d filled\n", 
		  ENV_ENTRIES);
    }
  else
    {
      env[*length] = xmalloc (ENV_LENGTH);
      vsnprintf (env[*length], ENV_LENGTH, fmt, args);
      (*length)++;
    }
#endif
  va_end (args);
}

/*
 * Create the environment block for a CGI script. Depending on the
 * system the environment is a field of null terminated char pointers
 * (for Unices) followed by a null pointer or one char pointer where
 * the variables a separated by zeros and the block is terminated
 * by a further zero. It returns either the amount of defined variables
 * or the size of the block in bytes.
 */
static int
http_create_cgi_envp (socket_t sock,      /* socket for this request */
		      ENV_BLOCK_TYPE env, /* env block */
		      char *script,       /* the cgi script's filename */
		      int type)           /* the cgi type */
{
  http_socket_t *http;
  http_config_t *cfg = sock->cfg;

  /* request type identifiers */
  static char request_type[2][5] = { "POST", "GET" };

  /* 
   * This data field is needed for the conversion of http request 
   * properties into environment variables.
   */
  static struct 
  { 
    char *property; /* property identifier */
    char *env;      /* variable identifier */
  }
  env_var[] =
  {
    { "Content-length",  "CONTENT_LENGTH"       },
    { "Content-type",    "CONTENT_TYPE"         },
    { "Accept",          "HTTP_ACCEPT"          },
    { "Referer",         "HTTP_REFERER"         },
    { "User-Agent",      "HTTP_USER_AGENT"      },
    { "Host",            "HTTP_HOST"            },
    { "Connection",      "HTTP_CONNECTION"      },
    { "Accept-Encoding", "HTTP_ACCEPT_ENCODING" },
    { "Accept-Language", "HTTP_ACCEPT_LANGUAGE" },
    { "Accept-Charset",  "HTTP_ACCEPT_CHARSET"  },
    { NULL, NULL }
  };

  unsigned n; 
  int size = 0;
  int c;

  /* get http socket structure */
  http = sock->data;

  /* convert some http request properties into environment variables */
  if (http->property)
    for (c = 0; env_var[c].property; c++)
      for (n = 0; http->property[n]; n+=2)
	if (!util_strcasecmp (http->property[n], env_var[c].property))
	  {
	    http_insert_env (env, &size, "%s=%s", 
			     env_var[c].env, http->property[n+1]);
	    break;
	  }

  /* 
   * set up some more environment variables which might be 
   * necessary for the cgi script
   */
  http_insert_env (env, &size, "SERVER_NAME=%s", 
		   util_inet_ntoa (sock->local_addr));
  http_insert_env (env, &size, "SERVER_PORT=%u", ntohs (sock->local_port));
  http_insert_env (env, &size, "REMOTE_ADDR=%s", 
		   util_inet_ntoa (sock->remote_addr));
  http_insert_env (env, &size, "REMOTE_PORT=%u", ntohs (sock->remote_port));
  http_insert_env (env, &size, "SCRIPT_NAME=%s%s", cfg->cgiurl, script);
  http_insert_env (env, &size, "GATEWAY_INTERFACE=%s", CGI_VERSION);
  http_insert_env (env, &size, "SERVER_PROTOCOL=%s", HTTP_VERSION);
  http_insert_env (env, &size, "SERVER_SOFTWARE=%s/%s", 
		   serveez_config.program_name, 
		   serveez_config.version_string);
  http_insert_env (env, &size, "REQUEST_METHOD=%s", request_type[type]);

#ifdef __MINGW32__
  /* now copy the original environment block */
  for (n = 0; environ[n]; n++)
    http_insert_env (env, &size, "%s", environ[n]);

  env[size] = 0;
#else
  env[size] = NULL;
#endif

  return size;
}

/*
 * Check the http option (the URL) for a cgi request. This routine
 * parses the text of the request and delivers the real file to be
 * invoked. This function makes sure that the cgi script file exists
 * and is executable. On success it delivers a pointer which must be
 * xfree()ed after use.
 */
char *
http_check_cgi (socket_t sock, char *request)
{
#ifndef __MINGW32__
  struct stat buf;
#endif
  char *file;
  int fd;
  int size;
  char *p;
  char *saverequest;
  int len;
  http_config_t *cfg = sock->cfg;

  /* check if the request is a real cgi request */
  if (strstr (request, cfg->cgiurl) != request)
    {
      return HTTP_NO_CGI;
    }

  /* 
   * skip the CGI url and concate the script file itself, then
   * check for trailing '?' which is the starting character for
   * GET variables.
   */

  /* store the request in a local variable */
  len = strlen (request) + 1 - strlen (cfg->cgiurl);
  saverequest = xmalloc (len);
  strcpy (saverequest, request + strlen (cfg->cgiurl));

  /* find the actual URL */
  p = saverequest;
  while (*p != '?' && *p != 0) p++;
  *p = 0;

  size = strlen (cfg->cgidir) + len;
  file = xmalloc (size);
  sprintf (file, "%s%s", cfg->cgidir, saverequest);

  /* test if the file really exists and close it again */
  if ((fd = open (file, O_RDONLY)) == -1)
    {
      log_printf (LOG_ERROR, "cgi: open: %s (%s)\n", SYS_ERROR, file);
      xfree (file);
      xfree (saverequest);
      return NULL;
    }

#ifndef __MINGW32__
  /* test the file being an executable */
  if (fstat (fd, &buf) == -1)
    {
      log_printf (LOG_ERROR, "cgi: fstat: %s\n", SYS_ERROR);
      close (fd);
      xfree (file);
      xfree (saverequest);
      return NULL;
    }

  if (!(buf.st_mode & S_IFREG) ||
      !(buf.st_mode & S_IXUSR) ||
      !(buf.st_mode & S_IRUSR))  
    {
      log_printf (LOG_ERROR, "cgi: no executable: %s\n", file);
      close (fd);
      xfree (file);
      xfree (saverequest);
      return NULL;
    }
#endif
  if (close (fd) == -1)
    log_printf (LOG_ERROR, "cgi: close: %s\n", SYS_ERROR);

  /* return a pointer referring to the actual plain cgi file */
  strcpy (file, saverequest);
  file = xrealloc (file, strlen (file) + 1);
  xfree (saverequest);
  return file;
}

/*
 * Prepare the invocation of a cgi script which means to change to 
 * the referred directory and the creation of a valid environment
 * block. Return a NULL pointer on errors or a pointer to the full
 * cgi file (including the path). This MUST be freed afterwards.
 */
char *
http_pre_exec (socket_t sock,       /* socket structure */
	       ENV_BLOCK_TYPE envp, /* environment block to be filled */
	       char *file,          /* plain executable name */
	       char *request,       /* original http request */
	       int type)            /* POST or GET ? */
{
  char *cgidir;
  char *cgifile;
  int size;
  char *p;
  http_config_t *cfg = sock->cfg;

  /* change into the CGI directory temporarily */
  if (chdir (cfg->cgidir) == -1)
    {
      log_printf (LOG_ERROR, "cgi: chdir: %s\n", SYS_ERROR);
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "cgi: cannot change dir: %s\n", cfg->cgidir);
#endif
      return NULL;
    }

  /* reserve buffer for the directory */
  cgidir = xmalloc (MAX_CGI_DIR_LEN);

  /* get the current directory and concate the cgifile */
  if (getcwd (cgidir, MAX_CGI_DIR_LEN) == NULL)
    {
      log_printf (LOG_ERROR, "cgi: getcwd: %s\n", SYS_ERROR);
      xfree (cgidir);
      return NULL;
    }
  
  /* put the directory and file together */
  cgifile = xmalloc (strlen (cgidir) + strlen (file) + 1);
  sprintf (cgifile, "%s%s", cgidir, file);
  xfree (cgidir);

  /* create the environment block for the CGI script */
  size = http_create_cgi_envp (sock, envp, file, type);

  /* put the QUERY_STRING into the env variables if necessary */
  if (type == GET_METHOD)
    {
      p = request;
      while (*p != '?' && *p != 0) p++;
      http_insert_env (envp, &size, "QUERY_STRING=%s", *p ? p+1 : "");
#ifdef __MINGW32__
      envp[size] = 0;
#else
      envp[size] = NULL;
#endif
    }

  return cgifile;
}

/*
 * Write an initial HTTP response header to the socket SOCK
 * right after the the actual CGI script has been invoked.
 */
int
http_cgi_accepted (socket_t sock)
{
  return sock_printf (sock, HTTP_ACCEPTED);
}

/*
 * The following function will free all the cgi application 
 * associations.
 */
void
http_free_cgi_apps (http_config_t *cfg)
{
  char **app;
  int n;

  if (*(cfg->cgiapps))
    {
      if ((app = (char **) hash_values (*(cfg->cgiapps))) != NULL)
	{
	  for (n = 0; n < hash_size (*(cfg->cgiapps)); n++)
	    {
	      xfree (app[n]);
	    }
	  hash_xfree (app);
	}
      hash_destroy (*(cfg->cgiapps));
      *(cfg->cgiapps) = NULL;
    }
}

/*
 * This routine generates some standard cgi associations.
 */
#define DEFAULT_CGIAPP "default"
void
http_gen_cgi_apps (http_config_t *cfg)
{
  /* create the cgi association hash table if necessary */
  if (*(cfg->cgiapps) == NULL)
    {
      *(cfg->cgiapps) = hash_create (4);
    }

  /* the associations need to be in the hash to be executed at all */
  hash_put (*(cfg->cgiapps), "exe", xstrdup (DEFAULT_CGIAPP));
  hash_put (*(cfg->cgiapps), "com", xstrdup (DEFAULT_CGIAPP));
  hash_put (*(cfg->cgiapps), "bat", xstrdup (DEFAULT_CGIAPP));
}

/*
 * Invoke a cgi script. In Unices we fork() us and in Win32 we
 * CreateProcess().
 */
int
http_cgi_exec (socket_t sock,  /* the socket structure */
	       HANDLE in,      /* here the cgi reads from or NULL if GET */
	       HANDLE out,     /* here the cgi writes to */
	       char *file,     /* cgi script file */
	       char *request,  /* original request (needed for GET) */
	       int type)       /* request type (POST or GET) */
{
  HANDLE pid;    /* the pid from fork() or the process handle in Win32 */
  char *cgifile; /* path including the name of the cgi script */
  http_socket_t *http;
  http_config_t *cfg = sock->cfg;

#ifdef __MINGW32__
  STARTUPINFO StartupInfo;         /* store here the inherited handles */
  PROCESS_INFORMATION ProcessInfo; /* where we get the process handle from */
  char *savedir;                   /* save the original directory */
  char *envp;
  char *suffix, *p;
  char *cgiapp;
#else
  char *envp[ENV_ENTRIES];
  char *argv[2];
  struct stat buf;
#endif

  /* Assign local CGI disconnection routine. */
  sock->disconnected_socket = http_cgi_disconnect;

#ifdef __MINGW32__
  /* 
   * Clean the StartupInfo, use the stdio handles, and store the
   * pipe handles there if necessary (depends on type).
   */
  memset (&StartupInfo, 0, sizeof (StartupInfo));
  StartupInfo.cb = sizeof (StartupInfo);
  StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  StartupInfo.hStdOutput = out;
  if (type == POST_METHOD) StartupInfo.hStdInput = in;

  /* reserve buffer space for the environment block */
  envp = xmalloc (ENV_BLOCK_SIZE);
  savedir = xmalloc (MAX_CGI_DIR_LEN);

  /* save the current directory */
  getcwd (savedir, MAX_CGI_DIR_LEN);

  if ((cgifile = http_pre_exec (sock, envp, file, request, type)) == NULL)
    {
      sock_printf (sock, HTTP_INTERNAL_ERROR "\r\n");
      http_error_response (sock, 500);
      sock->userflags |= HTTP_FLAG_DONE;
      chdir (savedir);
      xfree (envp);
      xfree (savedir);
      return -1;
    }

  /* find a cgi interpreter if possible */
  p = cgifile + strlen (cgifile) - 1;
  while (p != cgifile && *p != '.') p--;
  suffix = p + 1;

  if ((p = hash_get (*(cfg->cgiapps), util_tolower (suffix))) != NULL)
    {
      if (strcmp (p, DEFAULT_CGIAPP))
	{
	  cgiapp = xmalloc (strlen (cgifile) + strlen (p) + 2);
	  sprintf (cgiapp, "%s %s", p, cgifile);
	  xfree (cgifile);
	  cgifile = cgiapp;
	}
    }
  /* not a valid file extension */
  else
    {
      /* find an appropriate system association */
      cgiapp = xmalloc (MAX_PATH);
      if (FindExecutable (cgifile, NULL, cgiapp) <= (HINSTANCE) 32)
	{
	  log_printf (LOG_ERROR, "FindExecutable: %s\n", SYS_ERROR);
	}
#if ENABLE_DEBUG
      /* if this is enabled you could learn about the system */
      else
	{
	  log_printf (LOG_DEBUG, "FindExecutable: %s\n", cgiapp);
	}
#endif
      xfree (cgiapp);

      /* print some error message */
      sock_printf (sock, HTTP_ACCESS_DENIED "\r\n");
      http_error_response (sock, 403);
      sock->userflags |= HTTP_FLAG_DONE;
      chdir (savedir);
      xfree (cgifile);
      xfree (envp);
      xfree (savedir);
      return -1;
    }

  /* send http header response */
  if (http_cgi_accepted (sock) == -1)
    {
      sock->userflags |= HTTP_FLAG_DONE;
      chdir (savedir);
      xfree (cgifile);
      xfree (envp);
      xfree (savedir);
      return -1;
    }

  /* create the process here */
  if (!CreateProcess (NULL,                /* ApplicationName */
		      cgifile,             /* CommandLine */
		      NULL,                /* ProcessAttributes */
		      NULL,                /* ThreadAttributes */
		      TRUE,                /* InheritHandles */
		      DETACHED_PROCESS,    /* CreationFlags */
		      envp,                /* Environment */
		      NULL,                /* CurrentDirectory */
		      &StartupInfo,
		      &ProcessInfo))
    {
      log_printf (LOG_ERROR, "cgi: CreateProcess: %s\n", SYS_ERROR);
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "cgi: cannot execute: %s\n", cgifile);
#endif
      sock_printf (sock, "\r\n");
      sock->userflags |= HTTP_FLAG_DONE;
      chdir (savedir);
      xfree (cgifile);
      xfree (envp);
      xfree (savedir);
      return -1;
    }
  
  /* reenter the actual directory and free reserved space */
  chdir (savedir);
  xfree (cgifile);
  xfree (envp);
  xfree (savedir);
  pid = ProcessInfo.hProcess;

#ifdef ENABLE_DEBUG
  log_printf (LOG_DEBUG, "http: cgi %s got pid 0x%08X\n", 
	      file+1, ProcessInfo.dwProcessId);
#endif

#else /* not __MINGW32__ */

  /* fork us here */
  if ((pid = fork ()) == 0)
    {
      /* ------ child process here ------ */

      /* create environment block */
      if ((cgifile = http_pre_exec (sock, envp, file, request, type)) == NULL)
	{
	  exit (0);
	}

      /* make the output blocking */
      if (fcntl (out, F_SETFL, ~O_NONBLOCK) == -1)
	{
	  log_printf (LOG_ERROR, "cgi: fcntl: %s\n", SYS_ERROR);
	  exit (0);
	}

      /* duplicate the receiving pipe descriptor to stdout */
      if (dup2 (out, 1) != 1)
	{
	  log_printf (LOG_ERROR, "cgi: dup2: %s\n", SYS_ERROR);
	  exit (0);
	}

      if (type == POST_METHOD)
	{
	  /* make the input blocking */
	  if (fcntl (in, F_SETFL, ~O_NONBLOCK) == -1)
	    {
	      log_printf (LOG_ERROR, "cgi: fcntl: %s\n", SYS_ERROR);
	      exit (0);
	    }

	  /* duplicate the sending pipe descriptor to stdin */
	  if (dup2 (in, 0) != 0)
	    {
	      log_printf (LOG_ERROR, "cgi: dup2: %s\n", SYS_ERROR);
	      exit (0);
	    }
	}

      /* get the cgi scripts permissions */
      if (stat (cgifile, &buf) == -1)
	{
	  log_printf (LOG_ERROR, "cgi: stat: %s\n", SYS_ERROR);
	  exit (0);
	}

      /* set the appropriate user permissions */
      if (setgid (buf.st_gid) == -1)
	{
	  log_printf (LOG_ERROR, "cgi: setgid: %s\n", SYS_ERROR);
	  exit (0);
	}
      if (setuid (buf.st_uid) == -1)
	{
	  log_printf (LOG_ERROR, "cgi: setuid: %s\n", SYS_ERROR);
	  exit (0);
	}

      /* create the argv[] and envp[] pointers */
      argv[0] = cgifile;
      argv[1] = NULL;

      /* 
       * Execute the CGI script itself here. This will overwrite the 
       * current process.
       */
      if (execve (cgifile, argv, envp) == -1)
	{
	  log_printf (LOG_ERROR, "cgi: execve: %s\n", SYS_ERROR);
	  exit (0);
	}
    }
  else if (pid == -1)
    {
      /* ------ error forking new process ------ */
      log_printf (LOG_ERROR, "cgi: fork: %s\n", SYS_ERROR);
      sock_printf (sock, HTTP_BAD_REQUEST "\r\n");
      http_error_response (sock, 400);
      sock->userflags |= HTTP_FLAG_DONE;
      return -1;
    }

  /* ------ still current (parent) process here ------ */

#ifdef ENABLE_DEBUG
  log_printf (LOG_DEBUG, "http: cgi %s got pid %d\n", file + 1, pid);
#endif

  /* send http header response */
  if (http_cgi_accepted (sock) == -1)
    {
      sock->userflags |= HTTP_FLAG_DONE;
      return -1;
    }

#endif

  /* save the process id */
  http = sock->data;
  http->pid = pid;

  /* close the inherited http data handles */
  if (closehandle (out) == -1)
    {
      log_printf (LOG_ERROR, "cgi: close: %s\n", SYS_ERROR);
    }
  if (type == POST_METHOD)
    {
      /* close the reading end of the pipe for the post data */
      if (closehandle (in) == -1)
	{
	  log_printf (LOG_ERROR, "cgi: close: %s\n", SYS_ERROR);
	}
    }

  return 0;
}

/*
 * The http POST request response.
 */
int
http_post_response (socket_t sock, char *request, int flags)
{
  char *file;
  char *length;
  HANDLE s2cgi[2];
  HANDLE cgi2s[2];
  http_socket_t *http;

  /* get http socket structure */
  http = sock->data;

  /* is this a valid POST request ? */
  file = http_check_cgi(sock, request);
  if (file == NULL || file == HTTP_NO_CGI)
    {
      sock_printf (sock, HTTP_INTERNAL_ERROR "\r\n");
      http_error_response (sock, 500);
      sock->userflags |= HTTP_FLAG_DONE;
      return -1;
    }

  /* create a pair of pipes for the cgi script process */
  if (pipe_create_pair (cgi2s) == -1)
    {
      sock_printf (sock, HTTP_INTERNAL_ERROR "\r\n");
      http_error_response (sock, 500);
      sock->userflags |= HTTP_FLAG_DONE;
      xfree (file);
      return -1;
    }
  if (pipe_create_pair (s2cgi) == -1)
    {
      sock_printf (sock, HTTP_INTERNAL_ERROR "\r\n");
      http_error_response (sock, 500);
      sock->userflags |= HTTP_FLAG_DONE;
      xfree (file);
      return -1;
    }

  /* get the content length from the header information */
  if ((length = http_find_property (http, "Content-length")) == NULL)
    {
      sock_printf (sock, HTTP_BAD_REQUEST "\r\n");
      http_error_response (sock, 411);
      sock->userflags |= HTTP_FLAG_DONE;
      xfree (file);
      return -1;
    }
  http->contentlength = util_atoi (length);

  /* prepare everything for the cgi pipe handling */
  sock->pipe_desc[WRITE] = s2cgi[WRITE];
  sock->pipe_desc[READ] = cgi2s[READ];

  /* execute the cgi script in FILE */
  if (http_cgi_exec (sock, s2cgi[READ], cgi2s[WRITE], 
		     file, request, POST_METHOD))
    {
      /* some error occurred here */
      sock->read_socket = default_read;
      sock->write_socket = http_default_write;
      xfree (file);
      return -1;
    }

  sock->write_socket = http_cgi_write;
  sock->flags |= SOCK_FLAG_SEND_PIPE;
  sock->userflags |= HTTP_FLAG_POST;

  xfree (file);
  return 0;
}

#else /* ENABLE_HTTP_PROTO */

int http_cgi_dummy; /* Shut up compiler warnings. */

#endif /* not ENABLE_HTTP_PROTO */
