/*
 * src/dynload.c - dynamic server loading implementation
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
 * $Id: dynload.c,v 1.2 2001/01/24 15:55:28 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#if HAVE_DLFCN_H
# include <dlfcn.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "alloc.h"
#include "util.h"
#include "server.h"
#include "dynload.h"

/*
 * Load an additional server definition from a shared library. The given
 * descriptive name DESCRIPTION must be part of the library's name.
 */
server_definition_t *
dyn_load (char *description)
{
  char *lib, *def;
  void *handle;
  server_definition_t *server;

  lib = svz_malloc (strlen (description) + 8);
  sprintf (lib, "lib%s." DYNLOAD_SUFFIX, description);
#ifdef __MINGW32__
#else /* ! __MINGW32__ */
  /* open library */
  if ((handle = dlopen (lib, RTLD_NOW | RTLD_GLOBAL)) == NULL)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "dynload: unable to load library '%s'", lib);
#endif /* ENABLE_DEBUG */
      log_printf (LOG_ERROR, "dlopen: %s\n", dlerror ());
      svz_free (lib);
      return NULL;
    }
  svz_free (lib);

  /* obtain exported data */
  def = svz_malloc (strlen (description) + strlen ("_server_definition") + 1);
  sprintf (def, "%s_server_definition", description);
  if ((server = dlsym (handle, def)) == NULL)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "dynload: unable to obtain symbol '%s'", def);
#endif /* ENABLE_DEBUG */
      log_printf (LOG_ERROR, "dlsym: %s\n", dlerror ());
      svz_free (def);
      return NULL;
    }
#endif /* ! __MINGW32__ */

  return server;
}
