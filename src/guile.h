/*
 * guile.h - interface to guile core library declarations
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
 * $Id: guile.h,v 1.4 2001/06/27 20:38:36 ela Exp $
 *
 */

#ifndef __GUILE_H__
#define __GUILE_H__ 1

/*
 * Converts @code{SCM} into @code{char *} no matter if it is string or 
 * symbol. Returns @code{NULL} if it was neither. The new string must be 
 * explicitly @code{free()}d.
 */
#define guile2str(scm)                                        \
  (gh_null_p (scm) ? NULL :                                   \
  (gh_string_p (scm) ? gh_scm2newstr (scm, NULL) :            \
  (gh_symbol_p (scm) ? gh_symbol2newstr (scm, NULL) : NULL)))

/* FAIL breaks to the label `out' and sets an error condition. */
#define FAIL() do { err = -1; goto out; } while(0)

/* Export functions. */
int optionhash_extract_string (svz_hash_t *hash, char *key, int hasdef,
			       char *defvar, char **target, char *txt);
svz_hash_t *guile2optionhash (SCM pairlist, char *txt, int dounpack);
void optionhash_destroy (svz_hash_t *options);
void report_error (const char *format, ...);
SCM optionhash_get (svz_hash_t *hash, char *key);
int guile_load_config (char *cfgfile);

#endif /* not __GUILE_H__ */
