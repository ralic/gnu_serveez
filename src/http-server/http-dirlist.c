/*
 * http-dirlist.c - http protocol directory listing
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
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
 * $Id: http-dirlist.c,v 1.8 2000/07/25 16:24:27 ela Exp $
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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifndef __MINGW32__
# if HAVE_DIRENT_H
#  include <dirent.h>
#  define NAMLEN(dirent) strlen((dirent)->d_name)
# else
#  define dirent direct
#  define NAMLEN(dirent) (dirent)->d_namlen
#  if HAVE_SYS_NDIR_H
#   include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#   include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#   include <ndir.h>
#  endif
# endif
# define FILENAME(de) (de)->d_name
#else 
# include <windows.h>
# define FILENAME(de) (de).cFileName
#endif /* not __MINGW32__ */

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "snprintf.h"
#include "alloc.h"
#include "util.h"
#include "http-proto.h"
#include "http-core.h"
#include "http-dirlist.h"

/* Size of last buffer allocated. */
int http_dirlist_size = 0;

/*
 * Convert a given filename to an apropiate http request URI.
 * This is necessary because of special characters within
 * these.
 */
static char *
http_create_uri (char *file)
{
  static char uri[DIRLIST_SPACE_NAME];
  char *p, *dst;

  p = file;
  dst = uri;
  
  /* go throughout the filename */
  while (*p)
    {
      /* check if the current character is valid */
      while (*p && 
	     ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
	      (*p >= '0' && *p <= '9') || 
	      *p == ' ' || *p == '/' || *p == '.' || *p == '_'))
	*dst++ = *p++;
      if (!*p) break;
      *dst++ = '%';
      sprintf (dst, "%02X", (unsigned char)*p++);
      dst += 2;
    }
  *dst = '\0';
  return uri;
}

/*
 * Return a buffer to a http directory listing refering to DIRNAME
 * and being part of the document root DOCROOT. Do not to forget to
 * xfree() the return buffer. Return NULL on errors.
 */
char *
http_dirlist (char *dirname, char *docroot) 
{
  char *dirdata = NULL;
  int data_size = 0;
  char *relpath = NULL;
  int i = 0;
  struct stat buf;
  char filename[DIRLIST_SPACE_NAME];
  char entrystr[DIRLIST_SPACE_ENTRY];
  char *timestr = NULL;
  char postamble[DIRLIST_SPACE_POST];
  int files = 0;
#ifndef __MINGW32__
  DIR *dir;
  struct dirent *de = NULL;
#else
  WIN32_FIND_DATA de;
  HANDLE dir;
#endif

  /* Initialze data fields */
  memset (filename, 0, DIRLIST_SPACE_NAME);
  memset (entrystr, 0, DIRLIST_SPACE_ENTRY);
  memset (postamble, 0, DIRLIST_SPACE_POST);

  /* Remove trailing slash of dirname */
  if (strlen (dirname) != 1 && 
      (dirname[strlen (dirname) - 1] == '/' ||
       dirname[strlen (dirname) - 1] == '\\')) 
    {
      dirname[strlen(dirname) - 1] = 0;
    }

  /* Open the directory */
#ifdef __MINGW32__
  strcpy (filename, dirname);
  if (filename[strlen (filename) - 1] == '/' ||
      filename[strlen (filename) - 1] == '\\')
    strcat (filename, "*");
  else
    strcat (filename, "/*");
      
  if ((dir = FindFirstFile (filename, &de)) == INVALID_HANDLE_VALUE)
#else
  if ((dir = opendir (dirname)) == NULL)
#endif
    {
      return NULL;
    }

  dirdata = xmalloc (DIRLIST_SPACE);
  data_size = DIRLIST_SPACE;

  /* Calculate relative path */
  i = 0;
  while (dirname[i] == docroot[i] && docroot[i] != 0) i++;
  relpath = &dirname[i];
  if (!strcmp (relpath, "/"))
    {
      relpath++;
      dirname++;
    }

  /* Output preamble */
  while (-1 == snprintf (dirdata, data_size,
			 "%sContent-Type: text/html\r\n\r\n"
			 "<html><head>\n"
			 "<title>Directory listing of %s/</title></head>\n"
			 "<body bgcolor=white text=black link=blue>\n"
			 "<h1>Directory listing of %s/</h1>\n"
			 "<hr noshade>\n"
			 "<pre>\n",
			 HTTP_OK, relpath, relpath)) 
    {
      dirdata = xrealloc (dirdata, data_size + DIRLIST_SPACE_GROW);
      data_size += DIRLIST_SPACE_GROW;
    }

  /* Iterate directory */
#ifndef __MINGW32__
  while (NULL != (de = readdir (dir)))
#else
  do
#endif
    {
      /* Create fully qualified filename */
      snprintf (filename, DIRLIST_SPACE_NAME - 1, "%s/%s", 
		dirname, FILENAME (de));
      files++;
#if 0
      printf ("stat (%s)\n", filename);
#endif

      /* Stat the given file */
      if (-1 == stat (filename, &buf)) 
	{
	  /* Something is wrong with this file... */
	  snprintf (entrystr, DIRLIST_SPACE_ENTRY - 1,
		    "<font color=red>%s -- %s</font>\n",
		    FILENAME (de), SYS_ERROR);
	} 
      else 
	{
	  /* Get filetime and remove trailing newline */
	  if (localtime (&buf.st_mtime))
	    timestr = asctime (localtime (&buf.st_mtime));
	  else
	    {
	      buf.st_mtime = 0;
	      timestr = asctime (localtime (&buf.st_mtime));
	    }
	  if (timestr[strlen (timestr) - 1] == '\n') 
	    {
	      timestr[strlen (timestr) - 1] = 0;
	    }

	  /* Emit beautiful description */
	  if (S_ISDIR (buf.st_mode)) 
	    {
	      /* This is a directory... */
	      snprintf (entrystr, DIRLIST_SPACE_ENTRY - 1,
			"<img border=0 src=internal-gopher-menu> "
			"<a href=\"%s/\">%-40s</a> "
			"&lt;directory&gt; "
			"%s\n",
			http_create_uri (FILENAME (de)), 
			FILENAME (de), timestr);
	    } 
	  else 
	    {
	      /* Let's treat this as a normal file */
	      snprintf (entrystr, DIRLIST_SPACE_ENTRY - 1,
			"<img border=0 src=internal-gopher-text> "
			"<a href=\"%s\">%-40s</a> "
			"<b>%11d</b> "
			"%s\n",
			http_create_uri (FILENAME (de)), 
			FILENAME (de), (int) buf.st_size, timestr);
	    }
	}

      /* Append this entry's data to output buffer */
      while (data_size - strlen (dirdata) < strlen (entrystr) + 1)
	{
	  dirdata = xrealloc (dirdata, data_size + DIRLIST_SPACE_GROW);
	  data_size += DIRLIST_SPACE_GROW;
	}
      strcat (dirdata, entrystr);
    }
#ifdef __MINGW32__
  while (FindNextFile (dir, &de));
#endif

  /* Output postamble */
  snprintf (postamble, DIRLIST_SPACE_POST - 1,
	    "\n</pre><hr noshade>\n"
	    "%d entries\n</body>\n</html>",
	    files);

  if (data_size - strlen (dirdata) < strlen (postamble) + 1) 
    {
      dirdata = xrealloc (dirdata, data_size + strlen (postamble) + 1);
      data_size += strlen (postamble) + 1;
    }
  strcat (dirdata, postamble);

  /* Remember buffer size for caller */
  http_dirlist_size = data_size;

  /* Close the directory */
#ifndef __MINGW32__
  closedir (dir);
#else
  FindClose (dir);
#endif
  
  return dirdata;
}

#else /* ENABLE_HTTP_PROTO */
 
int http_dirlist_dummy_variable; /* Silence compiler */

#endif /* not ENABLE_HTTP_PROTO */
