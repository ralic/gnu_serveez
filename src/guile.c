/*
 * guile.c - interface to guile core library
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
 * $Id: guile.c,v 1.2 2001/02/23 08:29:01 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <guile/gh.h>

#include "libserveez.h"

/*
 * Set Serveez variables accessable in Guile.
 */
int
guile_export_variables (void)
{
  gh_define ("serveez-version", gh_str02scm (svz_version));
  gh_define ("guile-version", scm_version ());

  gh_define ("have-debug", gh_bool2scm (have_debug));
  gh_define ("have-win32", gh_bool2scm (have_win32));

  gh_define ("serveez-verbosity", gh_int2scm (svz_verbosity));
  gh_define ("serveez-sockets", gh_int2scm (svz_config.max_sockets));
  gh_define ("serveez-pass", gh_str02scm (svz_config.server_password));

  return 0;
}

/*
 * Set Serveez functions accessable in Guile.
 */
int
guile_export_functions (void)
{
  return 0;
}

/*
 * Generic server definition...
 */
#define FUNC_NAME "define-server!"
SCM
guile_define_server_x (SCM type, SCM name, SCM alist)
{
  char * ctype;
  char * cname;
  SCM cpair;

  SCM_VALIDATE_STRING_COPY (1, type, ctype);
  SCM_VALIDATE_STRING_COPY (2, name, cname);
  SCM_VALIDATE_LIST (3, alist);

  while (gh_pair_p (alist))
    {
      SCM_VALIDATE_ALISTCELL_COPYSCM (3, alist, cpair);
      alist = gh_cdr (alist);
    }
  log_printf (LOG_DEBUG, "defining server `%s' of type `%s'\n",
	      cname, ctype);
  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

/*
 * Generic port configuration definition...
 */
#define FUNC_NAME "define-port!"
SCM
guile_define_port_x (SCM type, SCM name, SCM alist)
{
  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

/*
 * Generic server -> port binding...
 */
#define FUNC_NAME "bind-server!"
SCM
guile_bind_server_x (SCM type, SCM name, SCM alist)
{
  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

/*
 * Initialize Guile.
 */
static void
guile_init (void)
{
  SCM def_serv;

  guile_export_variables ();
  guile_export_functions ();

  def_serv = gh_new_procedure3_0 ("define-server!", guile_define_server_x);
  def_serv = gh_new_procedure3_0 ("define-port!", guile_define_port_x);
  def_serv = gh_new_procedure3_0 ("bind-server!", guile_bind_server_x);
}

/*
 * Get server settings from the file @var{cfgfile} and instantiate servers 
 * as needed. Return non-zero on errors.
 */
int
guile_load_cfg (char *cfgfile)
{
  gh_eval_file (cfgfile);
  return -1;
}
