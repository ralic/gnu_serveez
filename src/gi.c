/* gi.c --- buffer against libguile interface churn
 *
 * Copyright (C) 2011 Thien-Thi Nguyen
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <libguile.h>
#ifdef HAVE_GUILE_GH_H
# include <guile/gh.h>
#endif

#if defined SCM_MAJOR_VERSION && defined SCM_MINOR_VERSION
#if 1 == SCM_MAJOR_VERSION
#define V19  (SCM_MINOR_VERSION >= 9)
#define V17  (SCM_MINOR_VERSION >= 7)
#define V15  (SCM_MINOR_VERSION >= 5)
#endif  /* 1 == SCM_MAJOR_VERSION */
#endif  /* defined SCM_MAJOR_VERSION && defined SCM_MINOR_VERSION */

#if V19
#define symbol2scm  scm_make_symbol
#elif V15
#define symbol2scm  scm_str2symbol
#else
#define symbol2scm  gh_symbol2scm
#endif

SCM
gi_symbol2scm (char const * name)
{
  return symbol2scm (name);
}

/* gi.c ends here */
