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
#include "timidity.h"

#if defined SCM_MAJOR_VERSION && defined SCM_MINOR_VERSION
#if 1 == SCM_MAJOR_VERSION
#define V19  (SCM_MINOR_VERSION >= 9)
#define V17  (SCM_MINOR_VERSION >= 7)
#define V15  (SCM_MINOR_VERSION >= 5)
#endif  /* 1 == SCM_MAJOR_VERSION */
#endif  /* defined SCM_MAJOR_VERSION && defined SCM_MINOR_VERSION */

#ifdef __GNUC__
#define UNUSED  __attribute__ ((__unused__))
#else
#define UNUSED
#endif

void *
gi_malloc (size_t len, const char *name)
{
#if V17
  return scm_gc_malloc (len, name);
#else
  return scm_must_malloc (len, name);
#endif
}

void *
gi_realloc (void *mem, size_t olen, size_t nlen, const char *name)
{
#if V17
  return scm_gc_realloc (mem, olen, nlen, name);
#else
  return scm_must_realloc (mem, olen, nlen, name);
#endif
}

#if V17
#define POSSIBLY_UNUSED_GI_FREE_PARAM
#else
#define POSSIBLY_UNUSED_GI_FREE_PARAM  UNUSED
#endif

void
gi_free (void *mem,
         POSSIBLY_UNUSED_GI_FREE_PARAM size_t len,
         POSSIBLY_UNUSED_GI_FREE_PARAM const char *name)
{
#if V17
  scm_gc_free (mem, len, name);
#else
  scm_must_free (mem);
#endif
}

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

#if V15
#define eval_string  scm_c_eval_string
#else
#define eval_string  gh_eval_str
#endif

SCM
gi_eval_string (char const *string)
{
  return eval_string (string);
}

SCM
gi_lookup (char const *string)
{
  SCM rv;

#if V15
  rv = scm_sym2var (symbol2scm (string),
                    scm_current_module_lookup_closure (),
                    SCM_BOOL_F);
  rv = SCM_FALSEP (rv)
    ? SCM_UNDEFINED
    : scm_variable_ref (rv);
#else
  rv = gh_lookup (string);
#endif

  return rv;
}

int
gi_scm2int (SCM number)
{
#if V15
  return scm_to_int (number);
#else
  return gh_scm2int (number);
#endif
}

long
gi_scm2long (SCM number)
{
#if V15
  return scm_to_long (number);
#else
  return gh_scm2long (number);
#endif
}

unsigned long
gi_scm2ulong (SCM number)
{
#if V15
  return scm_to_ulong (number);
#else
  return gh_scm2ulong (number);
#endif
}

size_t
gi_string_length (SCM string)
{
  return gi_scm2ulong (scm_string_length (string));
}

int
gi_nfalsep (SCM obj)
{
#if V17
  return ! scm_is_false (obj);
#else
  return SCM_NFALSEP (obj);
#endif
}

int
gi_stringp (SCM obj)
{
#if V17
  return scm_is_string (obj);
#else
  return gi_nfalsep (scm_string_p (obj));
#endif
}

int
gi_symbolp (SCM obj)
{
#if V17
  return scm_is_symbol (obj);
#else
  return gi_nfalsep (scm_symbol_p (obj));
#endif
}

int
gi_get_xrep (char *buf, size_t len, SCM symbol_or_string)
{
  SCM obj;
  size_t actual;

  /* You're joking, right?  */
  if (! len)
    return -1;

  obj = gi_symbolp (symbol_or_string)
    ? scm_symbol_to_string (symbol_or_string)
    : symbol_or_string;
  actual = gi_string_length (obj);
  if (len < actual + 1)
    return -1;

#if V17
  {
    size_t sanity;

    scm_c_string2str (obj, buf, &sanity);
    assert (sanity == actual);
  }
#else
  {
    int sanity;
    char *stage = gh_scm2newstr (obj, &sanity);

    assert (sanity == (int) actual);
    memcpy (buf, stage, actual);
    scm_must_free (stage);
  }
#endif

  /* Murphy was an optimist.  */
  buf[actual] = '\0';
  return actual;
}

void
gi_define (const char *name, SCM value)
{
#if V15
  scm_c_define (name, value);
#else
  gh_define (name, value);
#endif
}

SCM
gi_primitive_eval (SCM form)
{
#if V15
  return scm_primitive_eval_x (form);
#else
  return scm_eval_x (form);
#endif
}

SCM
gi_primitive_load (const char *filename)
{
#if V15
  return scm_c_primitive_load (filename);
#else
  return scm_primitive_load (gi_string2scm (filename));
#endif
}

int
gi_smob_tagged_p (SCM obj, svz_smob_tag_t tag)
{
  return SCM_NIMP (obj)
    && tag == SCM_TYP16 (obj);
}

/* gi.c ends here */
