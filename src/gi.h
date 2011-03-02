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

extern SCM gi_nstring2scm (size_t len, char const *s);
extern SCM gi_string2scm (char const * s);
extern SCM gi_symbol2scm (char const * name);
extern SCM gi_integer2scm (long int);

#endif  /* !defined __GI_H__ */

/* gi.h ends here */
