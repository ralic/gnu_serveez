/*
 * windoze.c - windows port implementations
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: windoze.c,v 1.3 2000/10/25 07:54:06 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif 

#ifdef __MINGW32__

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock.h>
#include <shellapi.h>
#include <windowsx.h>

#include <util.h>
#include <socket.h>
#include <serveez.h>
#include <server-core.h>
#include <windoze.h>

static DWORD windoze_daemon_id = 0;
static HANDLE windoze_daemon_handle = NULL;
static BOOL windoze_run = FALSE;
static HICON windoze_icon = NULL;
static char windoze_tooltip[128];

/*
 * Main windows thread where the window manager can pass message to.
 */
DWORD WINAPI 
windoze_thread (char *prog)
{
  HWND hwnd;      /* window handle */
  MSG msg;        /* message structure */
  WNDCLASS class; /* window class */
  ATOM atom;      /* window manager atom */
  int count = 0;  /* notify interval counter */
  
  /* load appropriate icon */
  windoze_icon = (HICON) LoadImage (NULL, SERVEEZ_ICON_FILE, IMAGE_ICON,
				    0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
  if (windoze_icon == NULL)
    {
      log_printf (LOG_ERROR, "LoadImage: %s\n", SYS_ERROR);
      windoze_icon = LoadIcon (NULL, IDI_WINLOGO);
    }

  /* setup window class */
  memset (&class, 0, sizeof (class));
  class.lpfnWndProc = windoze_dialog;
  class.hInstance = GetModuleHandle (prog);
  class.lpszClassName = SERVEEZ_CLASS;

  /* register window class */
  if (!(atom = RegisterClass (&class)))
    {
      log_printf (LOG_ERROR, "RegisterClass: %s\n", SYS_ERROR);
      ExitThread (0);
    }

  /* create new main window */
  hwnd = CreateWindow (SERVEEZ_CLASS, NULL, 0, 0, 0, 0 ,0,
		       NULL, NULL, GetModuleHandle (prog), NULL);

  if (hwnd == NULL)
    {
      log_printf (LOG_ERROR, "CreateWindow: %s\n", SYS_ERROR);
      ExitThread (0);
    }

  /* add icon to task bar */
  windoze_notify_add (hwnd, SERVEEZ_ICON_ID);

  /* start message loop */
  windoze_run = TRUE;
  while (windoze_run)
    {
      if (PeekMessage (&msg, hwnd, 0, 0, PM_REMOVE))
	{
	  windoze_dialog (msg.hwnd, msg.message, msg.wParam, msg.lParam);
	}
      Sleep (50);

      /* modify tooltip regularly */
      if ((count += 50) >= 1000)
	{
	  windoze_notify_set (hwnd, SERVEEZ_ICON_ID);
	  count = 0;
	}
    }
  windoze_run = FALSE;

  /* delete icon from taskbar */
  windoze_notify_del (hwnd, SERVEEZ_ICON_ID);

  /* destroy window and window class */
  if (!DestroyWindow (hwnd))
    {
      log_printf (LOG_ERROR, "DestroyWindow: %s\n", SYS_ERROR);
    }
  if (!UnregisterClass (SERVEEZ_CLASS, GetModuleHandle (prog)))
    {
      log_printf (LOG_ERROR, "UnRegisterClass: %s\n", SYS_ERROR);
    }

  return 0;
}

/*
 * Modify the windows taskbar.
 */
static BOOL 
windoze_set_taskbar (HWND hwnd, DWORD msg, UINT id, HICON icon, PSTR tip)
{
  NOTIFYICONDATA tnd;

  /* setup taskbar icon */
  tnd.cbSize = sizeof (NOTIFYICONDATA);
  tnd.hWnd = hwnd;
  tnd.uID = id;
  tnd.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  tnd.uCallbackMessage = WM_SERVEEZ_NOTIFYICON;
  tnd.hIcon = icon;

  if (tip)
    {
      strncpy (tnd.szTip, tip, sizeof (tnd.szTip));
    }
  else
    {
      tnd.szTip[0] = '\0';
    }
  
  return Shell_NotifyIcon (msg, &tnd);;
}

/*
 * Draw the serveez icon within the taskbar.
 */
static LRESULT 
windoze_draw_icon (LPDRAWITEMSTRUCT lpdi)
{
  DrawIconEx (lpdi->hDC, lpdi->rcItem.left, lpdi->rcItem.top, windoze_icon,
	      16, 16, 0, NULL, DI_NORMAL);

  return TRUE;
}

/*
 * Delete something from the taskbar.
 */
void 
windoze_notify_del (HWND hwnd, UINT id)
{
  windoze_set_taskbar (hwnd, NIM_DELETE, id, NULL, NULL);
}

/*
 * Add something to the taskbar.
 */
void 
windoze_notify_add (HWND hwnd, UINT id)
{
  sprintf (windoze_tooltip, "%s %s", 
	   serveez_config.program_name,
	   serveez_config.version_string);

  windoze_set_taskbar (hwnd, NIM_ADD, id, windoze_icon, windoze_tooltip);
}

/*
 * Modify what's within the taskbar.
 */
void 
windoze_notify_set (HWND hwnd, UINT id)
{
  sprintf (windoze_tooltip, "%s %s (%d connections)", 
	   serveez_config.program_name,
	   serveez_config.version_string,
	   connected_sockets);

  windoze_set_taskbar (hwnd, NIM_MODIFY, id, windoze_icon, windoze_tooltip);
}

/*
 * Dialog callback procedure.
 */
BOOL CALLBACK 
windoze_dialog (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  switch (msg)
    {
    case WM_DRAWITEM:
      return (windoze_draw_icon ((LPDRAWITEMSTRUCT) lparam));
      break;

    case WM_DESTROY:
      windoze_notify_del (hwnd, SERVEEZ_ICON_ID);
      server_nuke_happened = 1;
      break;

    case WM_COMMAND:
      break;

    case WM_SERVEEZ_NOTIFYICON:
      switch (lparam)
	{
	case WM_LBUTTONDOWN:
	  break;

	case WM_RBUTTONDOWN:
	  server_nuke_happened = 1;
	  break;
	  
	default:
	  break;
	}
      break;
      
    default:
      break;
    }
  
  return DefWindowProc (hwnd, msg, wparam, lparam);
}

/*
 * Start the windows daemon thread and detach from the current console.
 */
int
windoze_start_daemon (char *prog)
{
  /* start message loop */
  if ((windoze_daemon_handle = CreateThread (
        NULL, 
	0, 
	windoze_thread, 
	prog, 
	0, 
	&windoze_daemon_id)) == NULL)
    {
      log_printf (LOG_ERROR, "CreateThread: %s\n", SYS_ERROR);
      return -1;
    }

  /* detach program from console */
  if (!FreeConsole ())
    {
      log_printf (LOG_ERROR, "FreeConsole: %s\n", SYS_ERROR);
      return -1;
    }

  return 0;
}

/*
 * Stop the windows daemon thread.
 */
int
windoze_stop_daemon (void)
{
  DWORD ret;

  /* daemon thread still running ? */
  if (!windoze_run)
    {
      return 0;
    }

  /* signal termination */
  windoze_run = FALSE;
  PostQuitMessage (0);

  /* wait until daemon thread terminated */
  ret = WaitForSingleObject (windoze_daemon_handle, INFINITE);
  if (ret != WAIT_OBJECT_0)
    {
      log_printf (LOG_ERROR, "WaitForSingleObject: %s\n", SYS_ERROR);
      return -1;
    }

  /* close thread handle */
  if (!CloseHandle (windoze_daemon_handle))
    {
      log_printf (LOG_ERROR, "CloseHandle: %s\n", SYS_ERROR);
      return -1;
    }
  return 0;
}

/*
 * Read and write an unsigned integer value from and to the 
 * Windows Registry Database.
 */
unsigned
windoze_get_reg_unsigned (HKEY key, char *subkey, 
			  char *subsubkey, unsigned def)
{
  unsigned value;
  DWORD size, type;
  HKEY reg;

  if (RegOpenKeyEx (key, subkey, 0, KEY_QUERY_VALUE, &reg) != ERROR_SUCCESS)
    {
      log_printf (LOG_ERROR, "RegOpenKeyEx: %s\n", SYS_ERROR);
      return def;
    }

  size = sizeof (DWORD);
  type = REG_DWORD;
  if (RegQueryValueEx (reg, subsubkey, NULL, &type, 
		       (BYTE *) &value, &size) != ERROR_SUCCESS)
    {
      log_printf (LOG_ERROR, "RegQueryValueEx: %s\n", SYS_ERROR);
      value = def;
    }

  if (RegCloseKey (reg) != ERROR_SUCCESS)
    {
      log_printf (LOG_ERROR, "RegCloseKey: %s\n", SYS_ERROR);
    }
  return value;
}

void
windoze_set_reg_unsigned (HKEY key, char *subkey, 
			  char *subsubkey, unsigned value)
{
  DWORD size, type;
  HKEY reg;

  if (RegOpenKeyEx (key, subkey, 0, KEY_SET_VALUE, &reg) != ERROR_SUCCESS)
    {
      log_printf (LOG_ERROR, "RegOpenKeyEx: %s\n", SYS_ERROR);
      return;
    }

  size = sizeof (DWORD);
  type = REG_DWORD;
  if (RegSetValueEx (reg, subsubkey, 0, type, 
		     (BYTE *) &value, size) != ERROR_SUCCESS)
    {
      log_printf (LOG_ERROR, "RegSetValueEx: %s\n", SYS_ERROR);
    }

  if (RegCloseKey (reg) != ERROR_SUCCESS)
    {
      log_printf (LOG_ERROR, "RegCloseKey: %s\n", SYS_ERROR);
    }
}

/*
 * Read and write a string value from and to the Windows Registry Database.
 */
char *
windoze_get_reg_string (HKEY key, char *subkey, char *subsubkey, char *def)
{
  static char value[128];
  DWORD size, type;
  HKEY reg;

  if (RegOpenKeyEx (key, subkey, 0, KEY_QUERY_VALUE, &reg) != ERROR_SUCCESS)
    {
      log_printf (LOG_ERROR, "RegOpenKeyEx: %s\n", SYS_ERROR);
      return def;
    }

  size = sizeof (value);
  type = REG_SZ;
  if (RegQueryValueEx (reg, subsubkey, NULL, &type, 
		       (BYTE *) value, &size) != ERROR_SUCCESS)
    {
      log_printf (LOG_ERROR, "RegQueryValueEx: %s\n", SYS_ERROR);
      strcpy (value, def);
    }

  if (RegCloseKey (reg) != ERROR_SUCCESS)
    {
      log_printf (LOG_ERROR, "RegCloseKey: %s\n", SYS_ERROR);
    }
  return value;
}

void
windoze_set_reg_string (HKEY key, char *subkey, char *subsubkey, char *value)
{
  DWORD size, type;
  HKEY reg;

  if (RegOpenKeyEx (key, subkey, 0, KEY_SET_VALUE, &reg) != ERROR_SUCCESS)
    {
      log_printf (LOG_ERROR, "RegOpenKeyEx: %s\n", SYS_ERROR);
      return;
    }

  size = strlen (value);
  type = REG_SZ;
  if (RegSetValueEx (reg, subsubkey, 0, type, 
		     (BYTE *) value, size) != ERROR_SUCCESS)
    {
      log_printf (LOG_ERROR, "RegSetValueEx: %s\n", SYS_ERROR);
    }

  if (RegCloseKey (reg) != ERROR_SUCCESS)
    {
      log_printf (LOG_ERROR, "RegCloseKey: %s\n", SYS_ERROR);
    }
}

#else /* __MINGW32__ */

int windoze_dummy;      /* Shut compiler warnings up. */

#endif /* not __MINGW32__ */
