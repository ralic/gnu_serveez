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
 * $Id: http-cgi.c,v 1.3 2000/06/15 11:54:52 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE

#if ENABLE_HTTP_PROTO

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#if HAVE_STRINGS_H
# include <strings.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "snprintf.h"
#include "util.h"
#include "pipe-socket.h"
#include "alloc.h"
#include "serveez.h"
#include "http-proto.h"
#include "http-cgi.h"

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
  http_socket_t *http;
#ifdef __MINGW32__
  LPOVERLAPPED overlap;
#endif

  http = sock->data;

  /* read as much space is left in the buffer */
  do_read = sock->send_buffer_size - sock->send_buffer_fill;
  if (do_read <= 0) 
    {
      return 0;
    }

#ifdef __MINGW32__
  overlap = (os_version >= WinNT4x) ? &http->overlap[READ] : NULL;
  if(!ReadFile(sock->pipe_desc[READ],
	       sock->send_buffer + sock->send_buffer_fill,
	       do_read,
	       (DWORD *)&num_read,
	       overlap))
    {
      log_printf(LOG_ERROR, "cgi: ReadFile: %s\n", SYS_ERROR);
      if(last_errno == ERROR_IO_PENDING)
	GetOverlappedResult(sock->pipe_desc[READ], overlap, 
			    (DWORD *)&num_read, FALSE);
      else
	num_read = -1;
    }
#else
  if((num_read = read(sock->pipe_desc[READ],
		      sock->send_buffer + sock->send_buffer_fill,
		      do_read)) == -1)
    {
      log_printf(LOG_ERROR, "cgi: read: %s\n", SYS_ERROR);
      return -1;
    }
#endif

  /* data has been read */
  else if(num_read > 0)
    {
      sock->send_buffer_fill += num_read;
      return 0;
    }

  /* no data has been received */
  sock->userflags |= HTTP_FLAG_DONE;
  if(sock->send_buffer_fill == 0)
    {
#if ENABLE_DEBUG
      log_printf(LOG_DEBUG, "cgi: data successfully received and resent\n");
#endif
      sock->userflags &= ~HTTP_FLAG_CGI;
      return -1;
    }
  
  return 0;
}

/*
 * HTTP_CGI_WRITE pipes all read data from the http socket connection 
 * into the cgi stdin. This is neccessary for the so called post method.
 * It directly reads from the RECV_BUFFER of the socket structure.
 */
int
http_cgi_write (socket_t sock)
{
  int do_write;
  int num_written;
  http_socket_t *http;
#ifdef __MINGW32__
  LPOVERLAPPED overlap;
#endif

  /* get http socket structure */
  http = sock->data;

  /* 
   * Write as many bytes as possible, remember how many
   * were actually sent. Do not write more than the content
   * length of the post data.
   */
  do_write = sock->recv_buffer_fill;
  if(do_write > http->contentlength)
    do_write = http->contentlength;

#ifdef __MINGW32__
  overlap = (os_version >= WinNT4x) ? &http->overlap[WRITE] : NULL;
  if(!WriteFile(sock->pipe_desc[WRITE], 
		sock->recv_buffer, 
		do_write,
		(DWORD *)&num_written,
		overlap))
    {
      log_printf(LOG_ERROR, "cgi: WriteFile: %s\n", SYS_ERROR);
      if(last_errno == ERROR_IO_PENDING)
	GetOverlappedResult(sock->pipe_desc[WRITE], overlap, 
			    (DWORD *)&num_written, FALSE);
      else
	num_written = -1;
    }
#else
  if((num_written = write(sock->pipe_desc[WRITE], 
			  sock->recv_buffer, 
			  do_write)) == -1)
    {
      log_printf(LOG_ERROR, "cgi: write: %s\n", SYS_ERROR);
    }
#endif

  /* data has been successfully sent */
  if(num_written > 0)
    {
      sock->last_send = time(NULL);

      /*
       * Shuffle the data in the output buffer around, so that
       * new data can get stuffed into it.
       */
      if(sock->recv_buffer_fill > num_written)
	{
	  memmove(sock->recv_buffer, 
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
  if(http->contentlength <= 0)
    {
#if ENABLE_DEBUG
      log_printf(LOG_DEBUG, "cgi: post data sent to cgi\n");
#endif
      sock->userflags &= ~HTTP_FLAG_POST;
      sock->userflags |= HTTP_FLAG_CGI;
      sock->read_socket = http_cgi_read;
      sock->write_socket = http_default_write;
    }

  /*
   * Return a non-zero value if an error occured.
   */
  return (num_written < 0) ? -1 : 0;
}

/*
 * Insert a further variable into the environment block. How
 * we really do it depends on the system we compile with.
 */
void
insert_env(ENV_BLOCK_TYPE env, /* the block to add the variable to */
	   int *length,        /* pointer to the current length of it */
	   char *fmt,          /* format string */
	   ...)                /* arguments for the format string */
{
  va_list args;

  va_start(args, fmt);
#ifdef __MINGW32__
  /* Windows: */
  if(*length >= ENV_BLOCK_SIZE)
    {
      log_printf(LOG_WARNING, "cgi: env block size %d reached\n", 
		 ENV_BLOCK_SIZE);
    }
  else
    {
      vsnprintf(&env[*length], ENV_BLOCK_SIZE - (*length), fmt, args);
      *length += strlen(&env[*length]) + 1;
    }
#else
  /* Unices: */
  if(*length >= ENV_ENTRIES-1)
    {
      log_printf(LOG_WARNING, "cgi: all env entries %d filled\n", 
		 ENV_ENTRIES);
    }
  else
    {
      env[*length] = xmalloc(ENV_LENGTH);
      vsnprintf(env[*length], ENV_LENGTH, fmt, args);
      (*length)++;
    }
#endif
  va_end(args);
}

/*
 * Create the environment block for a CGI script. Depending on the
 * system the environment is a field of null terminated char pointers
 * (for Unices) followed by a null pointer or one char pointer where
 * the variables a seperated by zeros and the block is terminated
 * by a further zero. It returns either the amount of defined variables
 * or the size of the block in bytes.
 */
int
create_cgi_envp(socket_t sock,      /* socket structure for this request */
		ENV_BLOCK_TYPE env, /* env block */
		char *script,       /* the cgi script's filename */
		int type)           /* the cgi type */
{
  http_socket_t *http;
  http_config_t *cfg = sock->cfg;

  /* request type indentifiers */
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
  for(c=0; env_var[c].property; c++)
    for(n=0; http->property[n]; n+=2)
      if(!strcasecmp(http->property[n], env_var[c].property))
	{
	  insert_env(env, &size, "%s=%s", 
		     env_var[c].env, http->property[n+1]);
	  break;
	}

  /* 
   * set up some more environment variables which might be 
   * necessary for the cgi script
   */
  n = sock->local_addr;
  insert_env(env, &size, "SERVER_NAME=%u.%u.%u.%u", 
	     n >> 24, (n >> 16) & 0xff, (n >> 8) & 0xff, n & 0xff);
  insert_env(env, &size, "SERVER_PORT=%u", sock->local_port);
  n = sock->remote_addr;
  insert_env(env, &size, "REMOTE_ADDR=%u.%u.%u.%u", 
	     n >> 24, (n >> 16) & 0xff, (n >> 8) & 0xff, n & 0xff);
  insert_env(env, &size, "REMOTE_PORT=%u", sock->remote_port);
  insert_env(env, &size, "SCRIPT_NAME=%s%s", cfg->cgiurl, script);
  insert_env(env, &size, "GATEWAY_INTERFACE=%s", CGI_VERSION);
  insert_env(env, &size, "SERVER_PROTOCOL=%s", HTTP_VERSION);
  insert_env(env, &size, "SERVER_SOFTWARE=%s/%s", 
	     serveez_config.program_name, 
	     serveez_config.version_string);
  insert_env(env, &size, "REQUEST_METHOD=%s", request_type[type]);

#ifdef __MINGW32__
  /* now copy the original environment block */
  for(n=0; environ[n]; n++)
    insert_env(env, &size, "%s", environ[n]);

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
http_check_cgi(socket_t sock, char *request)
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
  if(strstr(request, cfg->cgiurl) != request)
    {
      return HTTP_NO_CGI;
    }

  /* 
   * skip the CGI url and concate the script file itself, then
   * check for trailing '?' which is the starting character for
   * GET variables.
   */

  /* store the request in a local variable */
  len = strlen(request) + 1 - strlen(cfg->cgiurl);
  saverequest = xmalloc(len);
  strcpy(saverequest, request + strlen(cfg->cgiurl));

  /* find the actual URL */
  p = saverequest;
  while(*p != '?' && *p != 0) p++;
  *p = 0;

  size = strlen(cfg->cgidir) + len;
  file = xmalloc(size);
  sprintf(file, "%s%s", cfg->cgidir, saverequest);

  /* test if the file really exists and close it again */
  if((fd = open(file, O_RDONLY)) == -1)
    {
      log_printf(LOG_ERROR, "cgi: open: %s (%s)\n", SYS_ERROR, file);
      xfree(file);
      xfree(saverequest);
      return NULL;
    }

#ifndef __MINGW32__
  /* test the file being an executable */
  if(fstat(fd, &buf) == -1)
    {
      log_printf(LOG_ERROR, "cgi: fstat: %s\n", SYS_ERROR);
      close(fd);
      xfree(file);
      xfree(saverequest);
      return NULL;
    }

  if(!(buf.st_mode & S_IFREG) ||
     !(buf.st_mode & S_IXUSR) ||
     !(buf.st_mode & S_IRUSR))  
    {
      log_printf(LOG_ERROR, "cgi: no executable: %s\n", file);
      close(fd);
      xfree(file);
      xfree(saverequest);
      return NULL;
    }
#endif
  if(close(fd) == -1)
    log_printf(LOG_ERROR, "cgi: close: %s\n", SYS_ERROR);

  /* return a pointer refering to the actual plain cgi file */
  strcpy(file, saverequest);
  xrealloc(file, strlen(file) + 1);
  xfree(saverequest);
  return file;
}

/*
 * Prepare the invocation of a cgi script which means to change to 
 * the refered directory and the creation of a valid environment
 * block. Return a NULL pointer on errors or a pointer to the full
 * cgi file (including the path). This MUST be freed afterwards.
 */
char *
http_pre_exec(socket_t sock,       /* socket structure */
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
      log_printf(LOG_ERROR, "cgi: chdir: %s\n", SYS_ERROR);
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "cannot change dir: %s\n", cfg->cgidir);
#endif
      return NULL;
    }

  /* reserve buffer for the directory */
  cgidir = xmalloc(MAX_CGI_DIR_LEN);

  /* get the current directory and concate the cgifile */
  if(getcwd(cgidir, MAX_CGI_DIR_LEN) == NULL)
    {
      log_printf(LOG_ERROR, "cgi: getcwd: %s\n", SYS_ERROR);
      xfree(cgidir);
      return NULL;
    }
  
  /* put the directory and file together */
  cgifile = xmalloc(strlen(cgidir) + strlen(file) + 1);
  sprintf(cgifile, "%s%s", cgidir, file);
  xfree(cgidir);

  /* create the environment block for the CGI script */
  size = create_cgi_envp(sock, envp, file, type);

  /* put the QUERY_STRING into the env variables if neccessary */
  if(type == GET_METHOD)
    {
      p = request;
      while(*p != '?' && *p != 0) p++;
      insert_env(envp, &size, "QUERY_STRING=%s", *p ? p+1 : "");
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
http_cgi_accepted(socket_t sock)
{
  return sock_printf(sock, HTTP_ACCEPTED);
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

#ifdef __MINGW32__
  STARTUPINFO StartupInfo;         /* store here the inherited handles */
  PROCESS_INFORMATION ProcessInfo; /* where we get the process handle from */
  char *savedir;                   /* save the original directory */
  char *envp;
  struct interpreter
  {
    char *suffix;
    char *execute;
  }
  cgiexe[] =
  {
    { ".exe", NULL   },
    { ".com", NULL   },
    { ".bat", NULL   },
    { ".pl",  "perl" },
    { NULL,   NULL   }
  };
  int n;
  char *p;
  char *cgiapp;
#else
  char *envp[ENV_ENTRIES];
  char *argv[2];
  struct stat buf;
#endif

#ifdef __MINGW32__
  /* 
   * Clean the StartupInfo, use the stdio handles, and store the
   * pipe handles there if neccessary (depends on type).
   */
  memset(&StartupInfo, 0, sizeof(StartupInfo));
  StartupInfo.cb = sizeof(StartupInfo);
  StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  StartupInfo.hStdOutput = out;
  if(type == POST_METHOD) StartupInfo.hStdInput = in;

  /* reserve buffer space for the environment block */
  envp = xmalloc(ENV_BLOCK_SIZE);
  savedir = xmalloc(MAX_CGI_DIR_LEN);

  /* save the current directory */
  getcwd(savedir, MAX_CGI_DIR_LEN);

  if((cgifile = http_pre_exec(sock, envp, file, request, type)) == NULL)
    {
      sock_printf(sock, HTTP_INTERNAL_ERROR "\r\n");
      sock->userflags |= HTTP_FLAG_DONE;
      chdir(savedir);
      xfree(envp);
      xfree(savedir);
      return -1;
    }

  /* send http header response */
  if(http_cgi_accepted(sock) == -1)
    {
      sock->userflags |= HTTP_FLAG_DONE;
      chdir(savedir);
      xfree(cgifile);
      xfree(envp);
      xfree(savedir);
      return -1;
    }

  /* find cgi interpreter if neccessary */
  p = cgifile + strlen (cgifile) - 1;
  while (p != cgifile && *p != '.') p--;
  n = 0;
  while (cgiexe[n].suffix)
    {
      if (!strcmp (cgiexe[n].suffix, p))
	{
	  if (cgiexe[n].execute)
	    {
	      cgiapp = xmalloc (strlen (cgifile) + 
				strlen (cgiexe[n].execute) + 2);
	      sprintf (cgiapp, "%s %s", cgiexe[n].execute, cgifile);
	      xfree (cgifile);
	      cgifile = cgiapp;
	    }
	  break;
	}
      n++;
    }

  /* create the process here */
  if(!CreateProcess(NULL,                /* ApplicationName */
		    cgifile,             /* CommandLine */
		    NULL,                /* ProcessAttributes */
		    NULL,                /* ThreadAttributes */
		    TRUE,                /* InheritHandles */
		    IDLE_PRIORITY_CLASS, /* CreationFlags */
		    envp,                /* Environment */
		    NULL,                /* CurrentDirectory */
		    &StartupInfo,
		    &ProcessInfo))
    {
      log_printf(LOG_ERROR, "cgi: CreateProcess: %s\n", SYS_ERROR);
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "cannot execute: %s\n", cgifile);
#endif
      sock_printf(sock, "\r\n");
      sock->userflags |= HTTP_FLAG_DONE;
      chdir(savedir);
      xfree(cgifile);
      xfree(envp);
      xfree(savedir);
      return -1;
    }
  
  /* reenter the actual directory and free reserved space */
  chdir(savedir);
  xfree(cgifile);
  xfree(envp);
  xfree(savedir);
  pid = ProcessInfo.hProcess;

#ifdef ENABLE_DEBUG
  log_printf(LOG_DEBUG, "http: cgi %s got pid 0x%08X\n", 
	     file+1, ProcessInfo.dwProcessId);
#endif

#else
  /* fork us here */
  if((pid = fork()) == 0)
    {
      /* create env block, etc. */
      if((cgifile = http_pre_exec(sock, envp, file, request, type)) == NULL)
	{
	  exit(0);
	}

      /* Make the output blocking ! */
      if(fcntl(out, F_SETFL, ~O_NONBLOCK) == -1)
	{
	  log_printf(LOG_ERROR, "cgi: fcntl: %s\n", SYS_ERROR);
	  exit(0);
	}

      /* duplicate the receiving pipe descriptor to stdout */
      if(dup2(out, 1) != 1)
	{
	  log_printf(LOG_ERROR, "cgi: dup2: %s\n", SYS_ERROR);
	  exit(0);
	}

      if(type == POST_METHOD)
	{

	  /* Make the input blocking ! */
	  if(fcntl(in, F_SETFL, ~O_NONBLOCK) == -1)
	    {
	      log_printf(LOG_ERROR, "cgi: fcntl: %s\n", SYS_ERROR);
	      exit(0);
	    }

	  /* duplicate the sending pipe descriptor to stdin */
	  if(dup2(in, 0) != 0)
	    {
	      log_printf(LOG_ERROR, "cgi: dup2: %s\n", SYS_ERROR);
	      exit(0);
	    }
	}

      /* set the apropiate user permissions */
      if(stat(cgifile, &buf) == -1)
	{
	  log_printf(LOG_ERROR, "cgi: stat: %s\n", SYS_ERROR);
	  exit(0);
	}
      if(setgid(buf.st_gid) == -1)
	{
	  log_printf(LOG_ERROR, "cgi: setgid: %s\n", SYS_ERROR);
	  exit(0);
	}
      if(setuid(buf.st_uid) == -1)
	{
	  log_printf(LOG_ERROR, "cgi: setuid: %s\n", SYS_ERROR);
	  exit(0);
	}

      /* create the argv[] and envp[] pointers */
      argv[0] = cgifile;
      argv[1] = NULL;

      /* execute the CGI script itself here */
      if(execve(cgifile, argv, envp) == -1)
	{
	  log_printf(LOG_ERROR, "cgi: execve: %s\n", SYS_ERROR);
	  exit(0);
	}
    }
  else if(pid == -1)
    {
      log_printf(LOG_ERROR, "cgi: fork: %s\n", SYS_ERROR);
      sock_printf(sock, HTTP_BAD_REQUEST "\r\n");
      sock->userflags |= HTTP_FLAG_DONE;
      return -1;
    }

#ifdef ENABLE_DEBUG
  log_printf(LOG_DEBUG, "http: cgi %s got pid %d\n", file + 1, pid);
#endif

  /* send http header response */
  if(http_cgi_accepted(sock) == -1)
    {
      sock->userflags |= HTTP_FLAG_DONE;
      return -1;
    }

#endif

  /* save the process id */
  http = sock->data;
  http->pid = pid;

  /* close the inherited http data handle */
  if(CLOSE_HANDLE(out) == -1)
    {
      log_printf(LOG_ERROR, "cgi: close: %s\n", SYS_ERROR);
    }
  if(type == POST_METHOD)
    {
      /* close the reading end of the pipe for the post data */
      if(CLOSE_HANDLE(in) == -1)
	{
	  log_printf(LOG_ERROR, "cgi: close: %s\n", SYS_ERROR);
	}
    }

  return 0;
}

/*
 * The http POST request response.
 */
int
http_post_response(socket_t sock, char *request, int flags)
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
  if(file == NULL || file == HTTP_NO_CGI)
    {
      sock_printf(sock, HTTP_INTERNAL_ERROR "\r\n");
      sock->userflags |= HTTP_FLAG_DONE;
      return -1;
    }

  /* create a pair of pipes for the cgi script process */
  if(create_pipe(cgi2s) == -1)
    {
      sock_printf(sock, HTTP_INTERNAL_ERROR "\r\n");
      sock->userflags |= HTTP_FLAG_DONE;
      xfree(file);
      return -1;
    }
  if(create_pipe(s2cgi) == -1)
    {
      sock_printf(sock, HTTP_INTERNAL_ERROR "\r\n");
      sock->userflags |= HTTP_FLAG_DONE;
      xfree(file);
      return -1;
    }

  /* get the content length from the header information */
  if((length = http_find_property(http, "Content-length")) == NULL)
    {
      sock_printf(sock, HTTP_BAD_REQUEST "\r\n");
      sock->userflags |= HTTP_FLAG_DONE;
      xfree(file);
      return -1;
    }
  http->contentlength = atoi(length);

  /* prepare everything for the cgi pipe handling */
  sock->pipe_desc[WRITE] = s2cgi[WRITE];
  sock->pipe_desc[READ] = cgi2s[READ];

  /* execute the cgi script in FILE */
  if(http_cgi_exec(sock, s2cgi[READ], cgi2s[WRITE], 
		   file, request, POST_METHOD))
    {
      /* some error occured here */
      sock->read_socket = default_read;
      sock->write_socket = http_default_write;
      xfree(file);
      return -1;
    }

#ifdef __MINGW32__
  memset(&http->overlap[READ], 0, sizeof(OVERLAPPED));
  memset(&http->overlap[WRITE], 0, sizeof(OVERLAPPED));
#endif
  sock->write_socket = http_cgi_write;
  sock->disconnected_socket = http_disconnect;
  sock->userflags |= HTTP_FLAG_POST;

  xfree(file);
  return 0;
}

#else /* ENABLE_HTTP_PROTO */

int cgi_dummy_variable;	/* Shutup compiler warnings. */

#endif /* not ENABLE_HTTP_PROTO */
