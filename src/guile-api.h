/*
 * guile-api.h - compatibility and miscellaneous guile functionality
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
 * $Id: guile-api.h,v 1.1 2001/11/19 13:31:49 ela Exp $
 *
 */

#ifndef __GUILE_API_H__
#define __GUILE_API_H__ 1

/* Some definitions for backward compatibility with Guile 1.3.4 */
#ifndef SCM_ASSERT_TYPE
#define SCM_ASSERT_TYPE(_cond, _arg, _pos, _subr, _msg) \
    SCM_ASSERT (_cond, _arg, _pos, _subr)
#define scm_wrong_type_arg_msg(_subr, _pos, _bad, _msg) \
    scm_wrong_type_arg (_subr, _pos, _bad)
#define scm_out_of_range_pos(_subr, _bad, _pos) \
    scm_out_of_range (_subr, _bad)
#endif /* not SCM_ASSERT_TYPE */

/* Compatibility definitions for various Guile versions.  These definitions
   are mainly due to the fact that the gh interface is deprecated in newer
   versions. */
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
#ifndef SCM_PAIRP
#define SCM_PAIRP(obj) SCM_NFALSEP (scm_pair_p (obj))
#endif
#ifndef scm_num2int
#define SCM_C_NUM2INT(pos, obj) gh_scm2int (obj)
#else
#define SCM_C_NUM2INT(pos, obj) scm_num2int (obj, pos, FUNC_NAME)
#endif
#ifndef scm_num2ulong
#define SCM_C_NUM2ULONG(pos, obj) gh_scm2ulong (obj)
#else
#define SCM_C_NUM2ULONG(pos, obj) scm_num2ulong (obj, pos, FUNC_NAME)
#endif
#ifndef scm_num2long
#define SCM_C_NUM2LONG(pos, obj) gh_scm2long (obj)
#else
#define SCM_C_NUM2LONG(pos, obj) scm_num2long (obj, pos, FUNC_NAME)
#endif
#ifndef scm_int2num
#define scm_int2num(x) scm_long2num ((long) (x))
#endif
#ifndef SCM_LISTP
#define SCM_LISTP(obj) SCM_NFALSEP (scm_list_p (obj))
#endif
#ifndef scm_mem2string
#define scm_mem2string(str, len) gh_str2scm (str, len)
#endif
#ifndef scm_c_define
#define scm_c_define(name, val) gh_define (name, val)
#endif
#ifndef scm_c_define_gsubr
#define scm_c_define_gsubr(name, req, opt, rst, fcn) \
    gh_new_procedure (name, fcn, req, opt, rst)
#endif
#ifndef scm_c_primitive_load
#define scm_c_primitive_load(file) \
    scm_primitive_load (scm_makfrom0str (file))
#endif

#endif /* not __GUILE_API_H__ */
