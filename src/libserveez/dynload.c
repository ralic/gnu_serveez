/*
 * dynload.c - dynamic server loading implementation
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
 * $Id: dynload.c,v 1.6 2001/04/18 19:26:57 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#if HAVE_DL_H
# include <dl.h>
#endif
#ifdef __BEOS__
# include <kernel/image.h>
#endif
#if HAVE_DLD_H
# include <dld.h>
#endif
#if HAVE_DLFCN_H
# include <dlfcn.h>
# ifndef RTLD_GLOBAL
#  define RTLD_GLOBAL 0
# endif
#endif

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#include "libserveez/alloc.h"
#include "libserveez/util.h"
#include "libserveez/socket.h"
#include "libserveez/server.h"
#include "libserveez/dynload.h"

/* Internal list of shared libraries. */
static int dyn_libraries = 0;
static dyn_library_t *dyn_library = NULL;

/* Print the current shared library error description. */
#if HAVE_DLOPEN
# define dyn_error() dlerror ()
#else
# define dyn_error() SYS_ERROR
#endif

/* Define library prefix and suffix. */
#ifdef __MINGW32__
# define DYNLOAD_PREFIX "lib"
# define DYNLOAD_SUFFIX "dll"
#else
# define DYNLOAD_PREFIX "lib"
# define DYNLOAD_SUFFIX "so"
#endif

/*
 * Find a library handle for a given library's name @var{file} in the current
 * list of loaded shared libraries. Return @code{NULL} if there is no such 
 * thing.
 */
static dyn_library_t *
dyn_find_library (char *file)
{
  int n;

  /* go through all loaded libraries and check if there is such a */
  for (n = 0; n < dyn_libraries; n++)
    if (!strcmp (dyn_library[n].file, file))
      return &dyn_library[n];

  return NULL;
}

/*
 * Open the given library @var{file} and put it into the currently load 
 * library list. Return a valid library handle entry on success.
 */
static dyn_library_t *
dyn_load_library (char *file)
{
  int n;
  void *handle = NULL;

  /* go through all loaded libraries and check if there is such a */
  for (n = 0; n < dyn_libraries; n++)
    if (!strcmp (dyn_library[n].file, file))
      {
	dyn_library[n].ref++;
	return &dyn_library[n];
      }

  /* try open the library */
#if HAVE_DLOPEN
  handle = dlopen (file, RTLD_NOW | RTLD_GLOBAL);
#elif defined (__BEOS__)
  handle = load_add_on (file);
  if ((image_id) handle <= 0)
    handle = NULL;
#elif HAVE_DLD_LINK
  handle = dld_link (file);
#elif defined (__MINGW32__)
  handle = LoadLibrary (file);
#elif HAVE_SHL_LOAD
  handle = shl_load (file, BIND_IMMEDIATE | BIND_NONFATAL | DYNAMIC_PATH);
#endif
  
  if (handle == NULL)
    {
      log_printf (LOG_ERROR, "load: %s (%s)\n", dyn_error (), file);
      return NULL;
    }

  /* check if this handle already exists */
  for (n = 0; n < dyn_libraries; n++)
    if (dyn_library[n].handle == handle)
      {
	dyn_library[n].ref++;
	return &dyn_library[n];
      }

  /* add the shared library to the handle list */
  n = dyn_libraries;
  dyn_libraries++;
  dyn_library = svz_realloc (dyn_library, sizeof (dyn_library_t) * 
			     dyn_libraries);
  dyn_library[n].file = svz_strdup (file);
  dyn_library[n].handle = handle;
  dyn_library[n].ref = 1;
  return &dyn_library[n];
}

/*
 * Unload a given library @var{lib} if possible. Return the reference 
 * counter or zero if the library has been unloaded. Return -1 if there is 
 * no such library at all or on other errors.
 */
static int
dyn_unload_library (dyn_library_t *lib)
{
  int n, err = 0;
  void *handle;

  /* check if this library really exists */
  for (n = 0; n < dyn_libraries; n++)
    if (&dyn_library[n] == lib)
      {
	/* return the remaining reference counter */
	if (--lib->ref > 0)
	  return lib->ref;

	/* unload the library */
	handle = lib->handle;
#if HAVE_DLOPEN
	err = dlclose (handle);
#elif defined (__BEOS__)
	err = (unload_add_on ((image_id) handle) != B_OK);
#elif HAVE_DLD_LINK
	err = dld_unlink_by_file (lib->file);
#elif defined (__MINGW32__)
	err = FreeLibrary (handle);
#elif HAVE_SHL_LOAD
	err = shl_unload ((shl_t) handle);
#endif
	if (err)
	  {
	    log_printf (LOG_ERROR, "unload: %s (%s)\n", dyn_error (), 
			lib->file);
	    return -1;
	  }

	/* rearrange the library structure */
	svz_free (lib->file);
	if (--dyn_libraries > 0)
	  {
	    *lib = dyn_library[dyn_libraries];
	    svz_realloc (dyn_library, sizeof (dyn_library_t) * dyn_libraries);
	  }
	else
	  {
	    svz_free (dyn_library);
	    dyn_library = NULL;
	  }
	return 0;
      }
  return -1;
}

/*
 * Get a function or data symbol @var{symbol} from the given library 
 * @var{lib}. Return @code{NULL} on errors.
 */
static void *
dyn_load_symbol (dyn_library_t *lib, char *symbol)
{
  int n;
  void *address = NULL;

  /* check if there is such a library */
  for (n = 0; n < dyn_libraries; n++)
    if (&dyn_library[n] == lib)
      {
#if HAVE_DLOPEN
	address = dlsym (lib->handle, symbol);
#elif defined (__BEOS__)
	if (get_image_symbol ((image_id) lib->handle, symbol, 
			      B_SYMBOL_TYPE_ANY, &address) != B_OK)
	  address = NULL;
#elif HAVE_DLD_LINK
	address = dld_get_func (symbol);
#elif defined (__MINGW32__)
	*((FARPROC *) &address) = GetProcAddress (lib->handle, symbol);
#elif HAVE_SHL_LOAD
	if (shl_findsym ((shl_t *) &lib->handle,
			 symbol, TYPE_UNDEFINED, &address) != 0)
	  address = NULL;
#endif
	if (address == NULL)
	  {
	    log_printf (LOG_ERROR, "symbol: %s (%s)\n", dyn_error (), symbol);
	  }
	return address;
      }
  return NULL;
}

/*
 * Finalize the shared library interface of the core library. Unload
 * all libraries no matter if referenced or not.
 */
void
svz_dynload_finalize (void)
{
  while (dyn_libraries)
    {
      dyn_library[0].ref = 1;
      dyn_unload_library (&dyn_library[0]);
    }
}

/*
 * Initialize the shared library interface of the core library.
 */
void
svz_dynload_init (void)
{
  if (dyn_libraries)
    svz_dynload_finalize ();
  dyn_libraries = 0;
  dyn_library = NULL;
}

/* 
 * Create a file name of a shared library for a given servers 
 * descriptive name @var{description}.
 */
static char *
dyn_create_file (char *description)
{
  char *file;

  file = svz_malloc (strlen (description) + 
		     strlen (DYNLOAD_PREFIX "." DYNLOAD_SUFFIX) + 1);
  sprintf (file, DYNLOAD_PREFIX "%s." DYNLOAD_SUFFIX, description);
  return file;
}

/*
 * Create a symbol name of server definition for a given servers
 * descriptive name @var{description}.
 */
static char *
dyn_create_symbol (char *description)
{
  char *symbol;

  symbol = svz_malloc (strlen (description) + 
		       strlen ("_server_definition") + 1);
  sprintf (symbol, "%s_server_definition", description);
  return symbol;
}

/*
 * Load an additional server definition from a shared library. The given
 * descriptive name @var{description} must be part of the library's name.
 */
svz_servertype_t *
svz_server_load (char *description)
{
  char *file, *def;
  dyn_library_t *lib;
  svz_servertype_t *server;

  /* load library */
  file = dyn_create_file (description);
  if ((lib = dyn_load_library (file)) == NULL)
    {
      svz_free (file);
      return NULL;
    }
  svz_free (file);

  /* obtain exported data */
  def = dyn_create_symbol (description);
  if ((server = dyn_load_symbol (lib, def)) == NULL)
    {
      dyn_unload_library (lib);
      svz_free (def);
      return NULL;
    }
  svz_free (def);

  return server;
}

/*
 * Unload a server definition from a shared library. The given
 * descriptive name @var{description} must be part of the library's name.
 * Return the remaining reference count or -1 on errors.
 */
int
svz_server_unload (char *description)
{
  dyn_library_t *lib;
  char *file;

  /* check if there is such a library loaded */
  file = dyn_create_file (description);
  if ((lib = dyn_find_library (file)) != NULL)
    {
      /* try unloading it */
      svz_free (file);
      return dyn_unload_library (lib);
    }
  svz_free (file);

  return -1;
}
