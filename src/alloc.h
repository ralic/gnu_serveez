/*
 * alloc.h - memory allocation module declarations
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.  
 *
 * $Id: alloc.h,v 1.3 2000/06/16 15:36:15 ela Exp $
 *
 */

#ifndef __ALLOC_H__
#define __ALLOC_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif 

#if ENABLE_DEBUG
extern unsigned allocated_bytes;
extern unsigned allocated_blocks;
#endif

/*
 * xmalloc() - allocate `size' of memory and return a pointer to it
 */
void * xmalloc (unsigned size);

/*
 * xrealloc() - change the size of a xmalloc()'ed block of memory.
 *	`size' is the new size of the block, `old_size' is the
 *	current size. `ptr' is the pointer returned by xmalloc() or
 *	NULL.
 */
void * xrealloc (void * ptr, unsigned size);

/*
 * xfree() - free a block of xmalloc()'ed or xrealloc()'ed
 *	memory. `size' is only used to calculate the amount of
 *	memory which got x{m,re}alloc()'ed but not xfree()'ed
 */
void xfree (void * ptr);

/*
 * The xp-functions allocate memory which is not planned to free again
 *
 */
void * xpmalloc(unsigned);
void * xprealloc(void *, unsigned);
char * xpstrdup(char *);

#endif /* not __ALLOC_H__ */
