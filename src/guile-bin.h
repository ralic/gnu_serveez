/*
 * guile-bin.h - binary data exchange layer for guile servers
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
 * $Id: guile-bin.h,v 1.3 2001/11/16 09:08:22 ela Exp $
 *
 */

#ifndef __GUILE_BIN_H__
#define __GUILE_BIN_H__ 1

/* Some definitions for backward compatibility with Guile 1.3.4 */
#ifndef SCM_ASSERT_TYPE
#define SCM_ASSERT_TYPE(_cond, _arg, _pos, _subr, _msg) \
    SCM_ASSERT (_cond, _arg, _pos, _subr)
#define scm_wrong_type_arg_msg(_subr, _pos, _bad, _msg) \
    scm_wrong_type_arg (_subr, _pos, _bad)
#define scm_out_of_range_pos(_subr, _bad, _pos) \
    scm_out_of_range (_subr, _bad)
#endif /* not SCM_ASSERT_TYPE */

/* Compatibility definitions for various Guile versions. */
#ifndef SCM_STRING_UCHARS
#define SCM_STRING_UCHARS(obj) ((unsigned char *) SCM_VELTS (obj))
#endif
#ifndef SCM_STRING_CHARS
#define SCM_STRING_CHARS(obj) ((char *) SCM_VELTS (obj))
#endif

void guile_bin_init (void);
int guile_bin_check (SCM);
SCM guile_data_to_bin (void *, int);
void *guile_bin_to_data (SCM, int *);

#endif /* not __GUILE_BIN_H__ */
