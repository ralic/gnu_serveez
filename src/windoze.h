/*
 * windoze.h - windows port interface
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: windoze.h,v 1.5 2001/01/03 13:00:21 ela Exp $
 *
 */

#ifndef __WINDOZE_H__
#define __WINDOZE_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef __MINGW32__

/* definitions for Win95..WinME */
#define MaxSocketKey       HKEY_LOCAL_MACHINE
#define MaxSocketSubKey    "System\\CurrentControlSet\\Services\\VxD\\MSTCP"
#define MaxSocketSubSubKey "MaxConnections"

/* window definitions */
#define WM_SERVEEZ_NOTIFYICON (WM_APP + 100)
#define SERVEEZ_ICON_ID       (1001)
#define SERVEEZ_CLASS         "serveez"
#define SERVEEZ_ICON_FILE     "serveez1.ico"

/* exported functions */
DWORD WINAPI windoze_thread (char *);
void windoze_notify_del (HWND, UINT);
void windoze_notify_add (HWND, UINT);
void windoze_notify_set (HWND, UINT);
LRESULT CALLBACK windoze_dialog (HWND, UINT, WPARAM, LPARAM);
int windoze_start_daemon (char *prog);
int windoze_stop_daemon (void);
WCHAR *windoze_asc2uni (CHAR *asc);
CHAR *windoze_uni2asc (WCHAR *unicode);

/* registry functions */
unsigned windoze_get_reg_unsigned (HKEY, char *, char *, unsigned);
void windoze_set_reg_unsigned (HKEY, char *, char *, unsigned);
char *windoze_get_reg_string (HKEY, char *, char *, char *);
void windoze_set_reg_string (HKEY, char *, char *, char *);

#endif /* __MINGW32__ */

#endif /* __WINDOZE_H__ */
