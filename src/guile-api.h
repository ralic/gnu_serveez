/*
 * guile-api.h - compatibility and miscellaneous guile functionality
 *
 * Copyright (C) 2011 Thien-Thi Nguyen
 * Copyright (C) 2001, 2002, 2003 Stefan Jahn <stefan@lkcc.org>
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

#ifndef __GUILE_API_H__
#define __GUILE_API_H__ 1

/* Define this macro if Guile 1.7.x or better is in use.  */
#if defined (SCM_MINOR_VERSION) && (SCM_MINOR_VERSION >= 7) && \
    defined (SCM_MAJOR_VERSION) && (SCM_MAJOR_VERSION >= 1)
#define SCM_VERSION_17X 1
#endif

/* Define this macro if Guile 1.5.x or better is in use.  */
#if defined (SCM_MINOR_VERSION) && (SCM_MINOR_VERSION >= 5) && \
    defined (SCM_MAJOR_VERSION) && (SCM_MAJOR_VERSION >= 1)
#define SCM_VERSION_15X 1
#endif

#ifndef SCM_VERSION_15X
#define scm_t_bits long
typedef scm_catch_body_t scm_t_catch_body;
typedef scm_catch_handler_t scm_t_catch_handler;
#endif

/* Some definitions for backward compatibility with Guile 1.3.4.  */
#ifndef SCM_ASSERT_TYPE
#define SCM_ASSERT_TYPE(_cond, _arg, _pos, _subr, _msg) \
    SCM_ASSERT (_cond, _arg, _pos, _subr)
#define scm_wrong_type_arg_msg(_subr, _pos, _bad, _msg) \
    scm_wrong_type_arg (_subr, _pos, _bad)
#define scm_out_of_range_pos(_subr, _bad, _pos) \
    scm_out_of_range (_subr, _bad)
#endif /* not SCM_ASSERT_TYPE */

/* Redefinition of the string and symbol predicates because they segfault
   for Guile 1.3.4 and prior version when passing immediate values.  */
#ifdef SCM_STRINGP
#undef SCM_STRINGP
#endif
#define SCM_STRINGP(obj) SCM_NFALSEP (scm_string_p (obj))
#ifdef SCM_SYMBOLP
#undef SCM_SYMBOLP
#endif
#define SCM_SYMBOLP(obj) SCM_NFALSEP (scm_symbol_p (obj))

/* Compatibility definitions for various Guile versions.  These definitions
   are mainly due to the fact that the gh interface is deprecated in newer
   versions.  */
#ifndef SCM_STRING_UCHARS
#define SCM_STRING_UCHARS(obj) ((unsigned char *) SCM_VELTS (obj))
#endif
#ifndef SCM_STRING_CHARS
#define SCM_STRING_CHARS(obj) ((char *) SCM_VELTS (obj))
#endif
#ifndef SCM_PROCEDUREP
#define SCM_PROCEDUREP(obj) SCM_NFALSEP (scm_procedure_p (obj))
#endif
#ifndef SCM_EXACTP
#define SCM_EXACTP(obj) SCM_NFALSEP (scm_exact_p (obj))
#endif
#ifndef SCM_POSITIVEP
#define SCM_POSITIVEP(obj) SCM_NFALSEP (scm_positive_p (obj))
#endif
#ifndef SCM_NEGATIVEP
#define SCM_NEGATIVEP(obj) SCM_NFALSEP (scm_negative_p (obj))
#endif
#ifndef SCM_PAIRP
#define SCM_PAIRP(obj) SCM_NFALSEP (scm_pair_p (obj))
#endif
#ifndef SCM_LISTP
#define SCM_LISTP(obj) SCM_NFALSEP (scm_list_p (obj))
#endif
#ifndef SCM_BOOLP
#define SCM_BOOLP(obj) SCM_NFALSEP (scm_boolean_p (obj))
#endif
#ifndef SCM_BOOL
#define SCM_BOOL(x) ((x) ? SCM_BOOL_T : SCM_BOOL_F)
#endif
#ifndef SCM_EQ_P
#define SCM_EQ_P(x, y) SCM_NFALSEP (scm_eq_p (x, y))
#endif
#ifndef SCM_CHARP
#define SCM_CHARP(obj) SCM_NFALSEP (scm_char_p (obj))
#endif
#ifndef SCM_CHAR
#define SCM_CHAR(x) SCM_ICHR (x)
#endif
#ifndef SCM_MAKE_CHAR
#define SCM_MAKE_CHAR(x) SCM_MAKICHR (x)
#endif
#ifndef SCM_WRITABLE_VELTS
#define SCM_WRITABLE_VELTS(x) SCM_VELTS(x)
#endif
#ifndef SCM_VERSION_15X
#define scm_mem2string(str, len) gh_str2scm (str, len)
#endif
#ifndef SCM_VERSION_15X
#define scm_primitive_eval_x(expr) scm_eval_x (expr)
#endif
#ifndef SCM_VERSION_15X
#define scm_c_define(name, val) gh_define (name, val)
#endif
#ifndef scm_c_free
#define scm_c_free(p) scm_must_free (p)
#endif
#ifndef SCM_VERSION_15X
#define scm_c_primitive_load(file) \
    scm_primitive_load (gi_string2scm (file))
#endif
#ifndef SCM_VERSION_15X
#define scm_gc_protect_object(obj) scm_protect_object (obj)
#endif
#ifndef SCM_VERSION_15X
#define scm_gc_unprotect_object(obj) scm_unprotect_object (obj)
#endif
#ifndef SCM_VERSION_17X
#define scm_gc_malloc(len, name) scm_must_malloc (len, name)
#define scm_gc_free(mem, len, name) scm_must_free (mem)
#define scm_gc_realloc(mem, olen, nlen, name) \
    scm_must_realloc (mem, olen, nlen, name)
#endif
#ifndef SCM_VERSION_17X
#define scm_c_symbol2str(obj, str, lenp) gh_symbol2newstr (obj, lenp)
#endif
#ifndef SCM_OUT_OF_RANGE
#define SCM_OUT_OF_RANGE(pos, arg) \
    scm_out_of_range_pos (FUNC_NAME, arg, SCM_MAKINUM (pos))
#endif

/* Return an integer.  If the given Guile cell @var{obj} is not an
   integer, the routine returns the default value @var{def}.  */
#define guile_integer(pos, obj, def) \
    ((SCM_EXACTP (obj)) ? (gi_scm2int (obj)) : (def))

/* This macro creates a new concatenated symbol for the C type ‘ctype’,
   useful for generating smob-related function/variable names.  */
#define __CTYPE(pre,ctype,post)    pre ## _ ## ctype ## _ ## post

/* Compatibility macros for Guile 1.3 version.  Also defines the macro
   HAVE_OLD_SMOBS which indicates a different smob implementation.  */
#ifndef SCM_NEWSMOB
#define SCM_NEWSMOB(value, tag, data) do {                         \
    SCM_NEWCELL (value);                                           \
    SCM_SETCDR (value, data); SCM_SETCAR (value, tag); } while (0)
#endif
#ifndef SCM_RETURN_NEWSMOB
#define SCM_RETURN_NEWSMOB(tag, data) do { \
    SCM value;                             \
    SCM_NEWSMOB (value, tag, data);        \
    return value; } while (0)
#endif
#ifndef SCM_SMOB_DATA
#define SCM_SMOB_DATA(data) SCM_CDR (data)
#define HAVE_OLD_SMOBS 1
#endif
#ifndef SCM_FPORT_FDES
#define SCM_FPORT_FDES(port) fileno ((FILE *) SCM_STREAM (port))
#endif

#endif /* not __GUILE_API_H__ */
