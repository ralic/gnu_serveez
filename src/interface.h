/*
 * src/interfaces.h - network interface definitions
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
 * $Id: interface.h,v 1.2 2000/06/28 18:45:51 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

/* Export this function. */
void list_local_interfaces (void);

#ifdef _WIN32

#include "include/ipdata.h" 
#include "include/iphlpapi.h"

/*
 * Function pointer definitions for use with GetProcAddress.
 */
typedef int (__stdcall *WsControlProc) (DWORD, DWORD, LPVOID, LPDWORD,
					LPVOID, LPDWORD);

#define WSCTL_TCP_QUERY_INFORMATION 0
#define WSCTL_TCP_SET_INFORMATION   1   

/*
 * Structure for collecting IP interfaces.
 */
typedef struct
{
  DWORD index;       /* IF index */
  char *description; /* interface description */
}
ifList_t;

#endif /* not _WIN32 */
