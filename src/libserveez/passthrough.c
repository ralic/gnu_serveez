/*
 * passthrough.c - pass through connections to processes
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: passthrough.c,v 1.9 2001/11/19 21:13:01 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if !defined(__MINGW32__) && !defined(__CYGWIN__)
extern char ** environ;
#endif

#ifdef __MINGW32__
# include <winsock2.h>
# include <io.h>
# include <shellapi.h>
#endif

#include "libserveez/alloc.h"
#include "libserveez/util.h"
#include "libserveez/snprintf.h"
#include "libserveez/socket.h"
#include "libserveez/core.h"
#include "libserveez/server-core.h"
#include "libserveez/pipe-socket.h"
#include "libserveez/passthrough.h"

/*
 * This routine start a new program specified by @var{bin} passing the
 * socket descriptor in the socket structure @var{sock} to stdin and stdout.
 * The arguments and the environment of the new process can be passed by
 * @var{argv} and @var{envp}. The argument @var{flag} specifies the method
 * used to passthrough the connection. It can be either 
 * @code{SVZ_PROCESS_FORK} (pass pipes or socket directly through 
 * @code{fork()} and @code{exec()}) or @code{SVZ_PROCESS_SHUFFLE} (pass
 * socket transactions via a pair of pipes).
 */
int
svz_sock_process (svz_socket_t *sock, char *bin, 
		  char **argv, svz_envblock_t *envp, int flag)
{
  HANDLE in, out;
  char *dir = NULL, *p, *app, *file;
  int len, ret = -1;

  if (sock == NULL || bin == NULL)
    return -1;

  /* Setup descriptors depending on the type of socket structure. */
  in = out = (HANDLE) sock->sock_desc;
  if (sock->flags & SOCK_FLAG_PIPE)
    {
      in = sock->pipe_desc[READ];
      out = sock->pipe_desc[WRITE];
    }

  /* Check executable. */
  if (svz_process_check_executable (bin, &app) < 0)
    return -1;

  /* Extract the directory part of the binary. */
  p = bin + strlen (bin) - 1;
  while (p > bin && *p != '/' && *p != '\\')
    p--;
  if (p > bin)
    {
      file = svz_strdup (p + 1);
      len = p - bin;
      dir = svz_malloc (len + 1);
      memcpy (dir, bin, len);
      dir[len] = '\0';
    }
  else
    {
      file = svz_strdup (bin);
    }

  /* Depending on the given flag use different methods to passthrough
     the connection. */
  switch (flag)
    {
    case SVZ_PROCESS_FORK:
      ret = svz_process_fork (file, dir, in, out, argv, envp, app);
      break;
    case SVZ_PROCESS_SHUFFLE:
      ret = svz_process_shuffle (sock, file, dir, (SOCKET) in, argv, 
				 envp, app);
      break;
    default:
      svz_log (LOG_ERROR, "invalid flag (%d)\n", flag);
      ret = -1;
      break;
    }

  svz_free (file);
  svz_free (dir);
  return ret;
}

/*
 * Disconnection routine for a socket connection which is connected with
 * a process's stdin/stdout via the referring passthrough pipe socket
 * structure which gets also scheduled for shutdown if possible.
 */
int
svz_process_sock_disconnect (svz_socket_t *sock)
{
  svz_socket_t *xsock;

  if ((xsock = svz_sock_getreferrer (sock)) != NULL)
    {
#if ENABLE_DEBUG
      svz_log (LOG_DEBUG, "shutting down referring id %d\n", xsock->id);
#endif
      svz_sock_setreferrer (sock, NULL);
      svz_sock_setreferrer (xsock, NULL);
      svz_sock_schedule_for_shutdown (xsock);
    }
  return 0;
}

/*
 * Disconnection routine for the passthrough pipe connected with a
 * process's stdin/stdout. Schedules the referring socket connection for
 * shutdown if possible.
 */
int
svz_process_pipe_disconnect (svz_socket_t *sock)
{
  svz_socket_t *xsock;

  if ((xsock = svz_sock_getreferrer (sock)) != NULL)
    {
#if ENABLE_DEBUG
      svz_log (LOG_DEBUG, "shutting down referring id %d\n", xsock->id);
#endif
      svz_sock_setreferrer (sock, NULL);
      svz_sock_setreferrer (xsock, NULL);
      svz_sock_schedule_for_shutdown (xsock);
    }

  sock->recv_buffer = sock->send_buffer = NULL;
  sock->recv_buffer_fill = sock->recv_buffer_size = 0;
  sock->send_buffer_fill = sock->send_buffer_size = 0;
  return 0;
}

/*
 * If the argument @var{set} is zero this routine makes the receive buffer
 * of the socket structure's referrer @var{sock} the send buffer of @var{sock}
 * itself, otherwise the other way around. Return non-zero if the routine
 * failed to determine a referrer.
 */
static int
svz_process_send_update (svz_socket_t *sock, int set)
{
  svz_socket_t *xsock;

  if ((xsock = svz_sock_getreferrer (sock)) == NULL)
    return -1;

  if (set)
    {
      sock->send_buffer = xsock->recv_buffer;
      sock->send_buffer_fill = xsock->recv_buffer_fill;
      sock->send_buffer_size = xsock->recv_buffer_size;
    }
  else
    {
      xsock->recv_buffer = sock->send_buffer;
      xsock->recv_buffer_fill = sock->send_buffer_fill;
      xsock->recv_buffer_size = sock->send_buffer_size;
    }
  return 0;
}

/*
 * Pipe writer (reading end of a process's stdin). Writes as much data as
 * possible from the send buffer which is the receive buffer of the
 * referring socket structure. Returns non-zero on errors.
 */
int
svz_process_send_pipe (svz_socket_t *sock)
{
  int num_written, do_write;

  /* update send buffer depending on receive buffer of referrer */
  if (svz_process_send_update (sock, 0))
    return -1;

  /* return here if there is nothing to do */
  if ((do_write = sock->send_buffer_fill) <= 0)
    return 0;

#ifndef __MINGW32__
  if ((num_written = write ((int) sock->pipe_desc[WRITE],
			    sock->send_buffer, do_write)) == -1)
    {
      svz_log (LOG_ERROR, "write: %s\n", SYS_ERROR);
      if (svz_errno == EAGAIN)
	return 0;
    }
#else /* __MINGW32__ */
   if (!WriteFile (sock->pipe_desc[WRITE], sock->send_buffer, 
		   do_write, (DWORD *) &num_written, NULL))
    {
      svz_log (LOG_ERROR, "WriteFile: %s\n", SYS_ERROR);
      num_written = -1;
    }
#endif /* __MINGW32__ */

  else if (num_written > 0)
    {
      sock->last_send = time (NULL);
      svz_sock_reduce_send (sock, num_written);
      svz_process_send_update (sock, 1);
    }

  return (num_written >= 0) ? 0 : -1;
}

/*
 * Depending on the flag @var{set} this routine either makes the send buffer
 * of the referring socket structure of @var{sock} the receive buffer of
 * @var{sock} itself or the other way around. Returns non-zero if it failed
 * to find a referring socket.
 */
static int
svz_process_recv_update (svz_socket_t *sock, int set)
{
  svz_socket_t *xsock;

  if ((xsock = svz_sock_getreferrer (sock)) == NULL)
    return -1;

  if (set)
    {
      sock->recv_buffer = xsock->send_buffer;
      sock->recv_buffer_fill = xsock->send_buffer_fill;
      sock->recv_buffer_size = xsock->send_buffer_size;
    }
  else
    {
      xsock->send_buffer = sock->recv_buffer;
      xsock->send_buffer_fill = sock->recv_buffer_fill;
      xsock->send_buffer_size = sock->recv_buffer_size;
    }
  return 0;
}

/*
 * Pipe reader (writing end of a process's stdout). Reads as much data as
 * possible into its receive buffer which is the send buffer of the network
 * connection this passthrough pipe socket stems from.
 */
int
svz_process_recv_pipe (svz_socket_t *sock)
{
  int num_read, do_read;

  /* update receive buffer depending on send buffer of referrer */
  if (svz_process_recv_update (sock, 0))
    return -1;

  /* return here if there is nothing to do */
  if ((do_read = sock->recv_buffer_size - sock->recv_buffer_fill) <= 0)
    return 0;

#ifndef __MINGW32__
  if ((num_read = read ((int) sock->pipe_desc[READ], 
			sock->recv_buffer + sock->recv_buffer_fill, 
			do_read)) == -1)
    {
      svz_log (LOG_ERROR, "read: %s\n", SYS_ERROR);
      if (svz_errno == EAGAIN)
	return 0;
    }
#else /* __MINGW32__ */
  if (!PeekNamedPipe (sock->pipe_desc[READ], NULL, 0, 
                      NULL, (DWORD *) &num_read, NULL))
    {
      svz_log (LOG_ERROR, "PeekNamedPipe: %s\n", SYS_ERROR);
      return -1;
    }
  if (do_read > num_read)
    do_read = num_read;
  if (!ReadFile (sock->pipe_desc[READ],
                 sock->recv_buffer + sock->recv_buffer_fill,
                 do_read, (DWORD *) &num_read, NULL))
    {
      svz_log (LOG_ERROR, "ReadFile: %s\n", SYS_ERROR);
      num_read = -1;
    }
#endif /* __MINGW32__ */

  else if (num_read > 0)
    {
      sock->last_recv = time (NULL);
      svz_process_recv_update (sock, 1);
    }

  return (num_read > 0) ? 0 : -1;
}

int
svz_process_create_child (char *file, char *dir, HANDLE in, HANDLE out, 
			  char **argv, svz_envblock_t *envp, char *app)
{
  /* Change directory, make descriptors blocking, setup environment,
     set permissions, duplicate descriptors and finally execute the 
     program. */

#ifndef __MINGW32__      
  if (chdir (dir) < 0)
    {
      svz_log (LOG_ERROR, "chdir (%s): %s\n", dir, SYS_ERROR);
      return -1;
    }

  if (svz_fd_block (out) < 0 || svz_fd_block (in) < 0)
    return -1;

  if (dup2 (out, 1) != 1 || dup2 (in, 0) != 0)
    {
      svz_log (LOG_ERROR, "unable to redirect: %s\n", SYS_ERROR);
      return -1;
    }

  if (svz_process_check_access (file) < 0)
    return -1;

  /* Check the environment and create  a default one if necessary. */
  if (envp == NULL)
    {
      envp = svz_envblock_create ();
      svz_envblock_default (envp);
    }

  /* Execute the file itself here overwriting the current process. */
  argv[0] = file;
  if (execve (file, argv, svz_envblock_get (envp)) == -1)
    {
      svz_log (LOG_ERROR, "execve: %s\n", SYS_ERROR);
      return -1;
    }
  return getpid ();

#else /* __MINGW32__ */

  STARTUPINFO startup_info;
  PROCESS_INFORMATION process_info;
  char *savedir, *application;
  int pid, n;

  /* Clean the Startup-Info, use the stdio handles, and store the pipe 
     handles there if necessary. */
  memset (&startup_info, 0, sizeof (startup_info));
  startup_info.cb = sizeof (startup_info);
  startup_info.dwFlags = STARTF_USESTDHANDLES;
  startup_info.hStdOutput = out;
  startup_info.hStdInput = in;

  /* Save current directory and change into application's. */
  savedir = svz_getcwd ();
  if (chdir (dir) < 0)
    {
      svz_log (LOG_ERROR, "getcwd: %s\n", SYS_ERROR);
      svz_free (savedir);
      return -1;
    }

  /* Check the access to the file. */
  if (svz_process_check_access (file) < 0)
    {
      chdir (savedir);
      svz_free (savedir);
      return -1;
    }

  /* Create sane environment. */
  if (envp == NULL)
    {
      envp = svz_envblock_create ();
      svz_envblock_default (envp);
    }

  /* Concatenate application name. */
  if (app != NULL)
    {
      application = svz_malloc (strlen (file) + strlen (app) + 2);
      sprintf (application, "%s %s", app, file);
    }
  else
    application = svz_strdup (file);

  /* Append program arguments. */
  for (n = 1; argv[n] != NULL; n++)
    {
      application = svz_realloc (application, strlen (application) + 
				 strlen (argv[n]) + 2);
      strcat (application, " ");
      strcat (application, argv[n]);
    }

  if (!CreateProcess (NULL,                    /* application name */
                      application,             /* command line */
                      NULL,                    /* process attributes */
                      NULL,                    /* thread attributes */
                      TRUE,                    /* inherit handles */
                      DETACHED_PROCESS,        /* creation flags */
                      svz_envblock_get (envp), /* environment */
                      NULL,                    /* current directory */
                      &startup_info, &process_info))
    {
      svz_log (LOG_ERROR, "CreateProcess (%s): %s\n", application, SYS_ERROR);
      chdir (savedir);
      svz_free (savedir);
      svz_free (application);
      return -1;
    }
  
  chdir (savedir);
  svz_free (savedir);
  svz_free (application);
  pid = (int) process_info.hProcess;

#ifdef ENABLE_DEBUG
  svz_log (LOG_DEBUG, "process `%s' got pid 0x%08X\n", file, 
	   process_info.dwProcessId);
#endif
  return pid;
#endif /* __MINGW32__ */
}

/*
 * Create two pairs of pipes in order to passthrough the transactions of the 
 * given socket descriptor @var{socket}.
 */
int
svz_process_shuffle (svz_socket_t *sock, char *file, char *dir, SOCKET socket,
		     char **argv, svz_envblock_t *envp, char *app)
{
  HANDLE process_to_serveez[2];
  HANDLE serveez_to_process[2];
  HANDLE in, out;
  svz_socket_t *xsock;
  int pid;

  /* create the pairs of pipes for the process */
  if (svz_pipe_create_pair (process_to_serveez) == -1)
    return -1;
  if (svz_pipe_create_pair (serveez_to_process) == -1)
    return -1;

  /* create yet another socket structure */
  if ((xsock = svz_pipe_create (process_to_serveez[READ], 
				serveez_to_process[WRITE])) == NULL)
    {
      svz_log (LOG_ERROR, "failed to create passthrough pipe\n");
      return -1;
    }

  /* prepare everything for the pipe handling */
  xsock->disconnected_socket = svz_process_pipe_disconnect;
  sock->disconnected_socket = svz_process_sock_disconnect;
  xsock->write_socket = svz_process_send_pipe;
  xsock->read_socket = svz_process_recv_pipe;
  svz_free_and_zero (xsock->recv_buffer);
  xsock->recv_buffer_fill = xsock->recv_buffer_size = 0;
  svz_free_and_zero (xsock->send_buffer);
  xsock->send_buffer_fill = xsock->send_buffer_size = 0;
  svz_sock_setreferrer (sock, xsock);
  svz_sock_setreferrer (xsock, sock);

  /* enqueue passthrough pipe socket */
  if (svz_sock_enqueue (xsock) < 0)
    return -1;

  in = serveez_to_process[READ];
  out = process_to_serveez[WRITE];

  /* create a process and pass the left-over pipe ends to it */
#ifndef __MINGW32__
  if ((pid = fork ()) == 0)
    {
      svz_process_create_child (file, dir, in, out, argv, envp, app);
      exit (0);
    }
  else if (pid == -1)
    {
      svz_log (LOG_ERROR, "fork: %s\n", SYS_ERROR);
      return -1;
    }
#else /* __MINGW32__ */
  pid = svz_process_create_child (file, dir, in, out, argv, envp, app);
  return pid;
#endif /*  __MINGW32__ */
  return 0;
}

/*
 * Fork the current process and execute the program @var{file} in the
 * working directory @var{dir}. Pass the descriptors @var{in} and @var{out}
 * to stdin and stdout of the child process. Additional arguments to 
 * @var{file} can be passed via the NULL terminated array of strings 
 * @var{argv}. The environment is setup by @var{envp} which can be NULL to
 * enforce the environment of the parent process. Returns -1 on errors
 * and the child's process id on success.
 */
int
svz_process_fork (char *file, char *dir, HANDLE in, HANDLE out, 
		  char **argv, svz_envblock_t *envp, char *app)
{
#ifdef __MINGW32__
  svz_log (LOG_FATAL, "fork() and exec() not supported\n");
  return -1;
#else /* __MINGW32__ */
  int pid;

  /* The child process here. */
  if ((pid = fork ()) == 0)
    {
      svz_process_create_child (file, dir, in, out, argv, envp, app);
    }
  else if (pid == -1)
    {
      svz_log (LOG_ERROR, "fork: %s\n", SYS_ERROR);
      return -1;
    }

  /* The parent process: Destroy the given socket object. */
#if ENABLE_DEBUG
  svz_log (LOG_DEBUG, "process `%s' got pid %d\n", file, pid);
#endif
  return pid;
#endif /* __MINGW32__ */
}

/*
 * Check if the given binary @var{file} is an executable. If it is script 
 * the routine returns a application able to execute the script in 
 * @var{app}. Return zero on success.
 */
int
svz_process_check_executable (char *file, char **app)
{
  struct stat buf;
#ifdef __MINGW32__
  char *suffix;
#endif

  /* Check the existence of the file. */
  if (stat (file, &buf) < 0)
    {
      svz_log (LOG_ERROR, "stat: %s\n", SYS_ERROR);
      return -1;
    }

  *app = NULL;
#ifndef __MINGW32__
  if (!(buf.st_mode & S_IFREG) || !(buf.st_mode & S_IXUSR) || 
      !(buf.st_mode & S_IRUSR))
#else
  if (!(buf.st_mode & S_IFREG))
#endif
    {
      svz_log (LOG_ERROR, "no executable: %s\n", file);
      return -1;
    }

#ifdef __MINGW32__
  suffix = file + strlen (file) - 1;
  while (suffix > file && *suffix != '.')
    suffix--;
  if (suffix > file)
    suffix++;
  if (!svz_strcasecmp (suffix, "com") || !svz_strcasecmp (suffix, "exe") ||
      !svz_strcasecmp (suffix, "bat"))
    return 0;

  *app = svz_malloc (MAX_PATH);
  if (FindExecutable (file, NULL, *app) <= (HINSTANCE) 32)
    {
      svz_log (LOG_ERROR, "FindExecutable: %s\n", SYS_ERROR);
      svz_free (*app);
      *app = NULL;
      return -1;
    }
#endif /* __MINGW32__ */

  return 0;
}

/*
 * Try setting the process permissions specified by the given executable
 * file @var{file}. Return zero on success.
 */
int
svz_process_check_access (char *file)
{
  struct stat buf;

  /* get the executable permissions */
  if (stat (file, &buf) == -1)
    {
      svz_log (LOG_ERROR, "stat: %s\n", SYS_ERROR);
      return -1;
    }

#ifndef __MINGW32__
  /* set the appropriate user permissions */
  if (setgid (buf.st_gid) == -1)
    {
      svz_log (LOG_ERROR, "setgid: %s\n", SYS_ERROR);
      return -1;
    }
  if (setuid (buf.st_uid) == -1)
    {
      svz_log (LOG_ERROR, "setuid: %s\n", SYS_ERROR);
      return -1;
    }
#endif /* not __MINGW32__ */

  return 0;
}

/*
 * Insert a new environment variable into the given environment block 
 * @var{env}. The @var{format} argument is a printf()-style format string
 * describing how to format the optional arguments.
 */
int
svz_envblock_add (svz_envblock_t *env, char *format, ...)
{
  static char buffer[VSNPRINTF_BUF_SIZE];
  int n, len;
  va_list args;

  va_start (args, format);
  svz_vsnprintf (buffer, VSNPRINTF_BUF_SIZE, format, args);
  va_end (args);

  /* Check for duplicate entry. */
  len = strchr (buffer, '=') - buffer;
  for (n = 0; n < env->size; n++)
    if (!memcmp (buffer, env->entry[n], len))
      {
	svz_free (env->entry[n]);
	env->entry[n] = svz_strdup (buffer);
	return env->size;
      }

  env->size++;
  env->entry = svz_realloc (env->entry, sizeof (char *) * (env->size + 1));
  env->entry[env->size - 1] = svz_strdup (buffer);
  env->entry[env->size] = NULL;
  return env->size;
}

/*
 * Create a fresh environment block. The returned pointer can be used to pass
 * it to @code{svz_envblock_default()} and @code{svz_envblock_add()}. Its
 * size is initially set to zero.
 */
svz_envblock_t *
svz_envblock_create (void)
{
  svz_envblock_t *env;

  env = svz_malloc (sizeof (svz_envblock_t));
  memset (env, 0, sizeof (svz_envblock_t));
  return env;
}

/*
 * Fill the given environment block @var{env} with the current process'es
 * environment variables. If the environment @var{env} contained any 
 * information before these will be overridden.
 */
int
svz_envblock_default (svz_envblock_t *env)
{
  int n;

  if (env == NULL)
    return -1;
  if (env->size)
    svz_envblock_free (env);

  for (n = 0; environ[n] != NULL; n++)
    {
      env->size++;
      env->entry = svz_realloc (env->entry, 
				sizeof (char *) * (env->size + 1));
      env->entry[env->size - 1] = svz_strdup (environ[n]);
    }

  env->entry[env->size] = NULL;
  return 0;
}

#ifdef __MINGW32__
/*
 * Win9x and WinNT systems use sorted environment. That is why we will sort
 * each environment block passed to @code{CreateProcess()}. The following
 * routine is the comparison routine for the @code{qsort()} call.
 */
static int
svz_envblock_sort (const void *data1, const void *data2)
{
  char *entry1 = * (char **) data1;
  char *entry2 = * (char **) data2;
  return strcmp (entry1, entry2);
}
#endif /* __MINGW32__ */

/*
 * Unfortunately the layout of environment blocks in Unices and Windows 
 * differ. On Unices you have a NULL terminated  array of character strings
 * and on Windows systems you have a simple character string containing
 * the environment variables in the format VAR=VALUE each seperated by a
 * zero byte. The end of the list is indicated by a further zero byte.
 * The following routine converts the given environment block @var{env} 
 * into something which can be passed to @code{exeve()} (Unix) or 
 * @code{CreateProcess()} (Windows). The routine additionally sort the
 * environment block on Windows systems since it is using sorted environments.
 */
svz_envp_t
svz_envblock_get (svz_envblock_t *env)
{
  char *dir;
#ifdef __MINGW32__
  int len = 32;
  svz_envp_t block = NULL;
  int n, size;
#endif

  /* Setup the PWD environment variable correctly. */
  dir = svz_getcwd ();
  svz_envblock_add (env, "PWD=%s", dir);
  svz_free (dir);

#ifdef __MINGW32__
  if (env->block)
    svz_free_and_zero (env->block);
  qsort ((void *) env->entry, env->size, sizeof (char *), svz_envblock_sort);
  for (size = 1, n = 0; n < env->size; n++)
    {
      len = strlen (env->entry[n]) + 1;
      block = svz_realloc (block, size + len);
      memcpy (&block[size - 1], env->entry[n], len);
      size += len;
    }
  block[size] = '\0';
  env->block = block;
  return block;
#else /* !__MINGW32__ */
  return env->entry;
#endif /* !__MINGW32__ */
}

/*
 * This function releases all environment variables currently stored in the
 * given enviroment block @var{env}. The block will be as clean as returned
 * by @code{svz_envblock_create()} afterwards.
 */
int
svz_envblock_free (svz_envblock_t *env)
{
  int n;

  if (env == NULL)
    return -1;
  for (n = 0; n < env->size; n++)
    svz_free (env->entry[n]);
  if (env->block)
    svz_free_and_zero (env->block);
  svz_free_and_zero (env->entry);
  env->size = 0;
  return 0;
}

/*
 * Destroys the given environment block @var{env} completely. The @var{env}
 * argument is invalid afterwards and should therefore not be referenced then.
 */
void
svz_envblock_destroy (svz_envblock_t *env)
{
  svz_envblock_free (env);
  svz_free (env);
}
