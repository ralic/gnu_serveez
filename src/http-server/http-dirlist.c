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
 * $Id: http-dirlist.c,v 1.2 2000/06/17 09:30:18 ela Exp $
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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "snprintf.h"
#include "alloc.h"
#include "util.h"
#include "http-proto.h"
#include "http-dirlist.h"

/* Size of last buffer allocated. */
int http_dirlist_size = 0;

char *
http_dirlist (char *dirname, char *docroot) 
{
  char *dirdata = NULL;
  int data_size = 0;
  char *relpath = NULL;
  int i = 0;
  DIR *dir;
  struct dirent *de = NULL;
  struct stat statbuf;
  char filename[DIRLIST_SPACE_NAME];
  char entrystr[DIRLIST_SPACE_ENTRY];
  char *timestr = NULL;
  char postamble[DIRLIST_SPACE_POST];
  int files = 0;

  /* Remove trailing slash of dirname */
  if (strlen (dirname) != 1 && 
      (dirname[strlen (dirname) - 1] == '/' ||
       dirname[strlen (dirname) - 1] == '\\')) 
    {
      dirname[strlen(dirname) - 1] = 0;
    }

  /* Open the directory */
  if ((dir = opendir (dirname)) == NULL)
    {
      return NULL;
    }

  dirdata = (char*)xmalloc (DIRLIST_SPACE);
  data_size = DIRLIST_SPACE;

  /* Calculate relative path */
  i = 0;
  while (dirname[i] == docroot[i]) i++;
  relpath = &dirname[i];

  /* Output preamble */
  while (-1 == snprintf (dirdata, data_size,
			 "%sContent-Type: text/html\r\n\r\n"
			 "<html><head>\n"
			 "<title>Directory listing of %s/</title></head>\n"
			 "<body bgcolor=white text=black link=blue>\n"
			 "<h1>Directory listing of %s/</h1>\n"
			 "<hr noshade>\n"
			 "<table border=0 cellspacing=5 cellpadding=1>\n",
			 HTTP_OK, relpath, relpath)) 
    {
      dirdata = xrealloc (dirdata, data_size + DIRLIST_SPACE_GROW);
      data_size += DIRLIST_SPACE_GROW;
    }

  /* Iterate directory */
  while (NULL != (de = readdir (dir))) 
    {
      /* Create fully qualified filename */
      snprintf (filename, DIRLIST_SPACE_NAME, "%s/%s", dirname, de->d_name);
      files++;

      /* Stat the given file */
      if (-1 == stat (filename, &statbuf)) 
	{
	  /* Something is wrong with this file... */
	  snprintf (entrystr, DIRLIST_SPACE_ENTRY,
		    "<tr><td><pre>"
		    "<font color=red><u>%s</u></font></pre></td>"
		    "<td></td>"
		    "<td><font color=red>%s</font></pre></td></tr>\n", 
		    de->d_name, SYS_ERROR);
	} 
      else 
	{
	  /* Get filetime and remove trailing newline */
	  timestr = asctime (localtime (&statbuf.st_mtime));
	  if (timestr[strlen (timestr) - 1] == '\n') 
	    {
	      timestr[strlen (timestr) - 1] = 0;
	    }

	  /* Emit beautiful description */
	  if (S_ISDIR (statbuf.st_mode)) 
	    {
	      /* This is a directory... */
	      snprintf (entrystr, DIRLIST_SPACE_ENTRY,
			"<tr><td><pre>"
			"<img border=0 src=internal-gopher-menu> "
			"<a href=\"%s/\">%s/</a></pre></td>"
			"<td><pre> &lt;directory&gt; </pre></td>"
			"<td><pre>%s</pre></td></tr>\n",
			de->d_name, de->d_name, timestr);
	    } 
	  else 
	    {
	      /* Let's treat this as a normal file */
	      snprintf (entrystr, DIRLIST_SPACE_ENTRY,
			"<tr><td><pre>"
			"<img border=0 src=internal-gopher-text> "
			"<a href=\"%s\">%s</a></pre></td>"
			"<td align=right><pre>%10d</pre></td>"
			"<td><pre>%s</pre></td></tr>\n",
			de->d_name, de->d_name, (int)statbuf.st_size, timestr);
	    }
	}

      /* Append this entry's data to output buffer */
      while (data_size - strlen (dirdata) < strlen (entrystr)) 
	{
	  dirdata = xrealloc (dirdata, data_size + DIRLIST_SPACE_GROW + 1);
	  data_size += DIRLIST_SPACE_GROW + 1;
	}

      strcat (dirdata, entrystr);
    }

  /* Output postamble */
  snprintf (postamble, DIRLIST_SPACE_POST, 
	    "</table>\n<hr noshade>\n"
	    "%d entries\n</body>\n</html>",
	    files);

  if (data_size - strlen (dirdata) < strlen (postamble)) 
    {
      dirdata = xrealloc (dirdata, data_size + strlen (postamble) + 1);
      data_size += strlen (postamble) + 1;
    }
  strcat (dirdata, postamble);

  /* Remember buffer size for caller */
  http_dirlist_size = data_size;

  /* Close the directory */
  closedir (dir);
  
  return dirdata;
}

#else /* ENABLE_HTTP_PROTO */
 
int http_dirlist_dummy_variable; /* Silence compiler */

#endif /* not ENABLE_HTTP_PROTO */
