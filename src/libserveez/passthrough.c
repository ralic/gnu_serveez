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
 * $Id: passthrough.c,v 1.2 2001/07/30 10:15:25 ela Exp $
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
 * @var{argv} and @var{envp}.
 */
int
svz_sock_process (svz_socket_t *sock, char *bin, char **argv, char **envp)
{
  HANDLE pid, in, out;
  char *dir = NULL, *p, *app, *file;
  int len;

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
  char *buf, *dir;
  int len = 32;
#ifdef __MINGW32__
  svz_envp_t block = NULL;
  int n, size;
#endif

  /* Setup the PWD environment variable correctly. */
  buf = dir = NULL;
  while (dir == NULL)
    {
      buf = svz_realloc (buf, len);
      dir = getcwd (buf, len);
      len *= 2;
    }
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
