/* gi.h --- buffer against libguile interface churn
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

#ifndef __GI_H__
#define __GI_H__ 1

extern SCM gi_gc_protect (SCM object);
extern SCM gi_gc_unprotect (SCM object);

extern SCM gi_nstring2scm (size_t len, char const *s);
extern SCM gi_string2scm (char const * s);
extern SCM gi_symbol2scm (char const * name);
extern SCM gi_integer2scm (long int);
extern SCM gi_nnint2scm (unsigned long int n);

extern SCM gi_list_3 (SCM a1, SCM a2, SCM a3);
extern SCM gi_list_5 (SCM a1, SCM a2, SCM a3, SCM a4, SCM a5);

extern SCM gi_n_vector (size_t len, SCM fill);

extern SCM gi_eval_string (char const *);

extern SCM gi_lookup (char const *);

extern long gi_scm2long (SCM number);
extern unsigned long gi_scm2ulong (SCM number);

size_t gi_string_length (SCM string);

#define STRING_OR_SYMBOL_P(obj)                 \
  (SCM_STRINGP (obj) || SCM_SYMBOLP (obj))

extern int gi_get_xrep (char *buf, size_t len, SCM symbol_or_string);

#define GI_GET_XREP(buf,obj)                    \
  gi_get_xrep (buf, sizeof buf, obj)

#define GI_GET_XREP_MAYBE(buf,obj)              \
  (STRING_OR_SYMBOL_P (obj)                     \
   && 0 < GI_GET_XREP (buf, obj))

#endif  /* !defined __GI_H__ */

/* gi.h ends here */
