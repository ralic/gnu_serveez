/*
 * option.c - getopt function implementation
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
 * $Id: option.c,v 1.5 2000/09/22 18:39:52 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif 

#define _GNU_SOURCE
#include <stdio.h>

#include "option.h"

#ifndef HAVE_GETOPT
/*
 * Lousy implementation of getopt().
 * Does not understand neither optind, opterr nor optopt
 * Only good for parsing simple small-option command lines on
 * stupid systems like Win32. No error checking !
 */
char *optarg;
int optind;
int opterr;
int optopt;

int 
getopt (int argc, char * const argv[], const char *optstring) 
{
  static int i = 1;
  int n;

  while (i < argc) 
    {
      if (argv[i][0] == '-') 
	{
	  n = 0;
	  /* go through all option characters */
	  while (optstring[n]) 
	    {
	      if (optstring[n] == argv[i][1])
		{
		  i++;
		  if (optstring[n+1] == ':')
		    {
		      optarg = argv[i];
		      i++;
		    }
		  else
		    {
		      optarg = NULL;
		    }
		  return optstring[n];
		}
	      n++;
	    }
	}
      i++;
    }
  i = 1;
  return EOF;
}

#else /* not HAVE_GETOPT */

int option_dummy_variable;

#endif /* HAVE_GETOPT */
