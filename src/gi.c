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
#include <string.h>
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

#if V15
#define gc_protect    scm_gc_protect_object
#define gc_unprotect  scm_gc_unprotect_object
#else
#define gc_protect    scm_protect_object
#define gc_unprotect  scm_unprotect_object
#endif

SCM
gi_gc_protect (SCM obj)
{
  return gc_protect (obj);
}

SCM
gi_gc_unprotect (SCM obj)
{
  return gc_unprotect (obj);
}

#if V19
#define mem2scm(len,ptr)  scm_from_locale_stringn (ptr, len)
#define mem02scm(ptr)     scm_from_locale_string (ptr)
#elif V15
#define mem2scm(len,ptr)  scm_makfromstr (ptr, len)
#define mem02scm(ptr)     scm_makfrom0str (ptr)
#else
#define mem2scm(len,ptr)  gh_str2scm (ptr, len)
#define mem02scm(ptr)     gh_str02scm (ptr)
#endif

SCM
gi_nstring2scm (size_t len, char const *s)
{
  return mem2scm (len, s);
}

SCM
gi_string2scm (char const * s)
{
  return mem02scm (s);
}

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

#if V19
#define integer2scm  scm_from_signed_integer
#define nnint2scm    scm_from_unsigned_integer
#elif V15
#define integer2scm  scm_long2num
#define nnint2scm    scm_ulong2num
#else
#define integer2scm  gh_long2scm
#define nnint2scm    gh_ulong2scm
#endif

SCM
gi_integer2scm (long int n)
{
  return integer2scm (n);
}

SCM
gi_nnint2scm (unsigned long int n)
{
  return nnint2scm (n);
}

#if V15
#define list_3(a1,a2,a3)  scm_list_n (a1, a2, a3, SCM_UNDEFINED)
#else
#define list_3(a1,a2,a3)  scm_listify (a1, a2, a3, SCM_UNDEFINED)
#endif

SCM
gi_list_3 (SCM a1, SCM a2, SCM a3)
{
  return list_3 (a1, a2, a3);
}

#if V15
#define list_5(a1,a2,a3,a4,a5)  scm_list_n (a1, a2, a3, a4, a5, SCM_UNDEFINED)
#else
#define list_5(a1,a2,a3,a4,a5)  scm_listify (a1, a2, a3, a4, a5, SCM_UNDEFINED)
#endif

SCM
gi_list_5 (SCM a1, SCM a2, SCM a3, SCM a4, SCM a5)
{
  return list_5 (a1, a2, a3, a4, a5);
}

#if V15
#define make_vector  scm_c_make_vector
#else
#define make_vector  scm_make_vector
#endif

extern SCM
gi_n_vector (size_t len, SCM fill)
{
  return make_vector (integer2scm (len), fill);
}

/* gi.c ends here */
