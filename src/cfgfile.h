/*
 * cfgfile.h - Configuration file definitions
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
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
 * $Id: cfgfile.h,v 1.4 2001/01/28 03:26:54 ela Exp $
 *
 */

#ifndef __CFGFILE_H__
#define __CFGFILE_H__ 1

/*
 * How large should string variables be (bufferspace given to sizzle).
 */
#define STRINGVARSIZE 1024

void init_server_definitions (void);
int load_config (char * cfgfilename, int argc, char **argv);

#endif /* not __CFGFILE_H__ */
