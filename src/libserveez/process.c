/*
 * process.c - pass through connections to processes
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
 * $Id: process.c,v 1.2 2001/06/29 14:23:10 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef __MINGW32__
# include <winsock2.h>
# include <io.h>
# include <shellapi.h>
#endif

#include "libserveez/alloc.h"
#include "libserveez/util.h"
#include "libserveez/socket.h"
#include "libserveez/core.h"
#include "libserveez/server-core.h"
#include "libserveez/pipe-socket.h"
#include "libserveez/process.h"

/*
 * This routine start a new program specified by @var{bin} passing the
 * socket descriptor in the socket structure @var{sock} to stdin and stdout.
 * The arguments and the environment of the new process can be passed by
 * @var{argv} and @var{envp}.
 */
int
svz_sock_process (svz_socket_t *sock, char *bin, char **argv, char **envp)
{
  HANDLE pid, in, out;
  char *dir = NULL, *p, *app, *file;
  int flag, len;

  if (sock == NULL || bin == NULL)
    return -1;

  /* Setup descriptors depending on the type of socket structure. */
  in = out = sock->sock_desc;
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

#ifndef __MINGW32__
  /* The child process here. */
  if ((pid = fork ()) == 0)
    {
      /* Change directory, make descriptors blocking, setup environment,
	 set permissions, duplicate descriptors and finally execute the 
	 program. */
      
      if (chdir (dir) < 0)
	{
	  svz_log (LOG_ERROR, "chdir (%s): %s\n", dir, SYS_ERROR);
	  exit (0);
	}

      if (svz_fd_block (out) < 0 || svz_fd_block (in) < 0)
	exit (0);

      if (dup2 (out, 1) != 1 || dup2 (in, 0) != 0)
	{
	  svz_log (LOG_ERROR, "unable to redirect: %s\n", SYS_ERROR);
	  exit (0);
	}

      if (svz_process_check_access (file) < 0)
	exit (0);

      /* Execute the file itself here overwriting the current process. */
      argv[0] = file;
      if (execve (file, argv, envp) == -1)
        {
          svz_log (LOG_ERROR, "execve: %s\n", SYS_ERROR);
          exit (0);
        }
    }
  else if (pid == -1)
    {
      svz_log (LOG_ERROR, "fork: %s\n", SYS_ERROR);
      svz_free (file);
      svz_free (dir);
      return -1;
    }

  /* The parent process: Destroy the given socket object. */
#if ENABLE_DEBUG
  svz_log (LOG_DEBUG, "process `%s' got pid %d\n", bin, pid);
#endif
  svz_free (file);
  svz_free (dir);
  return 0;

#else /* __MINGW32__ */
  return -1;
#endif /* not __MINGW32__ */
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
  char *suffix;

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
