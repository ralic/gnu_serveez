/*
 * src/util.c - utility function implementation
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
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
 * $Id: util.c,v 1.18 2000/08/19 10:57:34 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#if HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef __MINGW32__
# include <netdb.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "snprintf.h"
#include "alloc.h"
#include "util.h"
#include "serveez.h"

/* 
 * Level of server's verbosity:
 * 0 - only fatal error messages
 * 1 - error messages
 * 2 - warnings
 * 3 - informational messages
 * 4 - debugging output
 * levels always imply numerically lesser levels
 */
int verbosity = LOG_DEBUG;

char log_level[][16] =
{
  "fatal",
  "error",
  "warning",
  "notice",
  "debug"
};

/*
 * This is the file all log messages are written to.  Change it with a
 * call to set_log_file().  By default, all log messages are written
 * to STDERR.
 */
static FILE * log_file = NULL;

/*
 * Print a message to the log system.
 */
void
log_printf (int level, const char *format, ...)
{
  va_list args;
  time_t tm;
  struct tm * t;

  if (level > verbosity || log_file == NULL)
    return;

  tm = time (NULL);
  t = localtime (&tm);
  fprintf (log_file, "[%4d/%02d/%02d %02d:%02d:%02d] %s: ",
	   t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
	   t->tm_hour, t->tm_min, t->tm_sec,
	   log_level[level]);
  va_start (args, format);
  vfprintf (log_file, format, args);
  va_end (args);
}

/*
 * Set the file stream FILE to the log file all messages are printed
 * to.
 */
void 
set_log_file (FILE * file)
{
  log_file = file;
}

/*
 * Dump a request buffer to a FILE stream.
 */
#define MAX_DUMP_LINE 16 /* bytes per line */
int
util_hexdump (FILE *out,    /* output FILE stream */
	      char *action, /* hex dump description */
	      int from,     /* who created the dumped data */
	      char *buffer, /* the buffer to dump */
	      int len,      /* length of that buffer */
	      int max)      /* maximum amount of bytes to dump (0 = all) */
{
  int row, col, x, max_col;

  if (!max) max = len;
  if (max > len) max = len;
  max_col = max / MAX_DUMP_LINE;
  if ((max % MAX_DUMP_LINE) != 0) max_col++;
    
  fprintf (out, "%s [ FROM:0x%08X SIZE:%d ]\n", action, (unsigned)from, len);

  for (x = row = 0; row < max_col && x < max; row++)
    {
      /* print hexdump */
      fprintf (out, "%04X   ", x);
      for (col = 0; col < MAX_DUMP_LINE; col++, x++)
	{
	  if (x < max)
	    fprintf (out, "%02X ", (unsigned char)buffer[x]);
	  else
	    fprintf (out, "   ");
	}
      /* print character representation */
      x -= MAX_DUMP_LINE;
      fprintf (out, "  ");
      for (col = 0; col < MAX_DUMP_LINE && x < max; col++, x++)
	{
	  fprintf (out, "%c", buffer[x] >= ' ' ? buffer[x] : '.');
	}
      fprintf (out, "\n");
    }

  fflush (out);
  return 0;
}

/*
 * This is the hstrerror() wrapper, depending on config.h ...
 */
const char *
util_hstrerror (void)
{
#if HAVE_HSTRERROR
# if HAVE_H_ERRNO
  return hstrerror (h_errno);
# else
  return hstrerror (errno);
# endif
#else /* not HAVE_HSTRERROR */
# if HAVE_H_ERRNO
  return strerror (h_errno);
# else
  return strerror (errno);
# endif
#endif /* not HAVE_HSTRERROR */
}

/*
 * These are the system dependent case insensitive string compares.
 */
int
util_strcasecmp (const char *str1, const char *str2)
{
#if defined HAVE_STRCASECMP
  return strcasecmp (str1, str2);
#elif defined HAVE_STRICMP
  return stricmp (str1, str2);
#else
  const char *p1 = str1;
  const char *p2 = str2;
  unsigned char c1, c2;

  if (p1 == p2) return 0;

  do
    {
      c1 = isupper (*p1) ? tolower (*p1) : *p1;
      c2 = isupper (*p2) ? tolower (*p2) : *p2;
      if (c1 == '\0') break;
      ++p1;
      ++p2;
    }
  while (c1 == c2);

  return c1 - c2;
#endif
}

int
util_strncasecmp (const char *str1, const char *str2, size_t n)
{
#if defined HAVE_STRNCASECMP
  return strncasecmp (str1, str2, n);
#elif defined HAVE_STRNICMP
  return strnicmp (str1, str2, n);
#else
  const char *p1 = str1;
  const char *p2 = str2;
  unsigned char c1, c2;

  if (p1 == p2) return 0;

  do
    {
      c1 = isupper (*p1) ? tolower (*p1) : *p1;
      c2 = isupper (*p2) ? tolower (*p2) : *p2;
      if (c1 == '\0') break;
      ++p1;
      ++p2;
    }
  while (c1 == c2 && --n > 0);

  return c1 - c2;
#endif
}

#ifdef __MINGW32__
/*
 * This variable contains the last error occured if it was
 * detected and printed. Needed for the "Resource unavailable".
 */
int last_errno = 0;

/*
 * There is no ErrorMessage-System for Sockets in Win32. That
 * is why we do it by hand.
 */
char *
GetWSAErrorMessage (int error)
{
  static char message[MESSAGE_BUF_SIZE];

  switch (error)
    {
    case WSAEACCES:
      return "Permission denied.";
    case WSAEADDRINUSE:
      return "Address already in use.";
    case WSAEADDRNOTAVAIL:
      return "Cannot assign requested address.";
    case WSAEAFNOSUPPORT:
      return "Address family not supported by protocol family.";
    case WSAEALREADY:
      return "Operation already in progress.";
    case WSAECONNABORTED:
      return "Software caused connection abort.";
    case WSAECONNREFUSED:
      return "Connection refused.";
    case WSAECONNRESET:
      return "Connection reset by peer.";
    case WSAEDESTADDRREQ:
      return "Destination address required.";
    case WSAEFAULT:
      return "Bad address.";
    case WSAEHOSTDOWN:
      return "Host is down.";
    case WSAEHOSTUNREACH:
      return "No route to host.";
    case WSAEINPROGRESS:
      return "Operation now in progress.";
    case WSAEINTR:
      return "Interrupted function call.";
    case WSAEINVAL:
      return "Invalid argument.";
    case WSAEISCONN:
      return "Socket is already connected.";
    case WSAEMFILE:
      return "Too many open files.";
    case WSAEMSGSIZE:
      return "Message too long.";
    case WSAENETDOWN:
      return "Network is down.";
    case WSAENETRESET:
      return "Network dropped connection on reset.";
    case WSAENETUNREACH:
      return "Network is unreachable.";
    case WSAENOBUFS:
      return "No buffer space available.";
    case WSAENOPROTOOPT:
      return "Bad protocol option.";
    case WSAENOTCONN:
      return "Socket is not connected.";
    case WSAENOTSOCK:
      return "Socket operation on non-socket.";
    case WSAEOPNOTSUPP:
      return "Operation not supported.";
    case WSAEPFNOSUPPORT:
      return "Protocol family not supported.";
    case WSAEPROCLIM:
      return "Too many processes.";
    case WSAEPROTONOSUPPORT:
      return "Protocol not supported.";
    case WSAEPROTOTYPE:
      return "Protocol wrong type for socket.";
    case WSAESHUTDOWN:
      return "Cannot send after socket shutdown.";
    case WSAESOCKTNOSUPPORT:
      return "Socket type not supported.";
    case WSAETIMEDOUT:
      return "Connection timed out.";
    case WSAEWOULDBLOCK:
      return "Resource temporarily unavailable.";
    case WSAHOST_NOT_FOUND:
      return "Host not found.";
    case WSANOTINITIALISED:
      return "Successful WSAStartup not yet performed.";
    case WSANO_DATA:
      return "Valid name, no data record of requested type.";
    case WSANO_RECOVERY:
      return "This is a non-recoverable error.";
    case WSASYSNOTREADY:
      return "Network subsystem is unavailable.";
    case WSATRY_AGAIN:
      return "Non-authoritative host not found.";
    case WSAVERNOTSUPPORTED:
      return "WINSOCK.DLL version out of range.";
    case WSAEDISCON:
      return "Graceful shutdown in progress.";
#if HAVE_WSOCK_EXT /* Not defined in Win95, but WinNT. */
    case WSA_INVALID_HANDLE:
      return "Specified event object handle is invalid.";
    case WSA_INVALID_PARAMETER:
      return "One or more parameters are invalid.";
    case WSAINVALIDPROCTABLE:
      return "Invalid procedure table from service provider.";
    case WSAINVALIDPROVIDER:
      return "Invalid service provider version number.";
    case WSA_IO_PENDING:
      return "Overlapped operations will complete later."; 
    case WSA_IO_INCOMPLETE:
      return "Overlapped I/O event object not in signaled state.";
    case WSA_NOT_ENOUGH_MEMORY:
      return "Insufficient memory available.";
    case WSAPROVIDERFAILEDINIT:
      return "Unable to initialize a service provider.";
    case WSASYSCALLFAILURE:
      return "System call failure.";
    case WSA_OPERATION_ABORTED:
      return "Overlapped operation aborted.";
#endif
    default:
      sprintf (message, "Error code %d.", error);
      break;
    }
  return message;
}

/*
 * Routine which forms a valid error message under Win32. It might either
 * use the GetLastError() or WSAGetLastError() in order to get a valid
 * error code.
 */
char *
GetErrorMessage (int nr)
{
  static char message[MESSAGE_BUF_SIZE];
  LPTSTR error;

  /* save the last error */
  last_errno = nr;

  /* return a net error if necessary */
  if (nr >= WSABASEERR)
    return GetWSAErrorMessage (nr);
  
  /* 
   * if the error is not valid (GetLastError returned zero)
   * fall back to the errno variable of the usual crtdll.
   */
  if (!nr)
    nr = errno;

  /* return a sys error */
  if (0 == FormatMessage ( 
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_ARGUMENT_ARRAY,
    NULL,
    nr,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (char *)&error,
    0,
    NULL))
    {
      sprintf (message, "FormatMessage: error code %d", GetLastError ());
      return message;
    }
  
  strcpy (message, error);
  message[strlen (message) - 2] = 0;
  LocalFree (error);
  return message;
}

int os_version = 0;

#endif /* __MINGW32__ */

/*
 * This routine is for detecting the os version of 
 * Win32 and all Unices. It saves its result in the variable os_version.
 *
 * 0 - Windows 3.x
 * 1 - Windows 95
 * 2 - Windows 98
 * 3 - Windows NT 3.x
 * 4 - Windows NT 4.x
 * 5 - Windows 2000
 * 6 - Windows ME
 */

char *
get_version (void)
{
  static char os[256] = ""; /* contains the os string */

#ifdef __MINGW32__
  static char ver[][6] = 
  { " 32s", " 95", " 98", " NT", " NT", " 2000", " ME" };
  OSVERSIONINFO osver;
#elif HAVE_SYS_UTSNAME_H
  struct utsname buf;
#endif

  if (os[0]) return os;
  
#ifdef __MINGW32__ /* Windows */

  osver.dwOSVersionInfoSize = sizeof (osver);
  if (!GetVersionEx (&osver))
    {
      log_printf (LOG_ERROR, "GetVersionEx: %s\n", SYS_ERROR);
      sprintf (os, "unknown Windows");
    }
  else
    {
      switch (osver.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_NT: /* NT od Windows 2000 */
	  if (osver.dwMajorVersion == 4)
	    os_version = WinNT4x;
	  else if (osver.dwMajorVersion <= 3)
	    os_version = WinNT3x;
	  else if (osver.dwMajorVersion == 5)
	    os_version = Win2k;
	  break;

	case VER_PLATFORM_WIN32_WINDOWS: /* Win95 or Win98 */
	  if ((osver.dwMajorVersion > 4) || 
	      ((osver.dwMajorVersion == 4) && (osver.dwMinorVersion > 0)))
	    {
	      if (osver.dwMinorVersion >= 90)
		os_version = WinME;
	      else
		os_version = Win98;
	    }
	  else
	    os_version = Win95;
	  break;

	case VER_PLATFORM_WIN32s: /* Windows 3.x */
	  os_version = Win32s;
	  break;
	}

      sprintf (os, "Windows%s %d.%02d %s%s(Build %d)",
	       ver[os_version], 
	       osver.dwMajorVersion, 
	       osver.dwMinorVersion,
	       osver.szCSDVersion,
	       osver.szCSDVersion[0] ? " " : "",
	       osver.dwBuildNumber & 0xFFFF);
    }
#else /* Unices */

#if HAVE_UNAME
  if (uname (&buf) == -1)
    {
      sprintf (os, "uname: %s", SYS_ERROR);
    }
  else
    {
      sprintf (os, "%s %s on %s", buf.sysname, buf.release, buf.machine);
    }
#endif

#endif
  return os;
}

/*
 * Converts an unsigned integer to its string representation returning
 * a pointer to an internal buffer, so copy the result.
 */
char *
util_itoa (unsigned int i)
{
  static char buffer[32];
  char *p = buffer + sizeof (buffer) - 1;

  *p = '\0';
  do
    {
      p--;
      *p = (i % 10) + '0';
    }
  while ((i /= 10) != 0);
  return p;
}

/*
 * Converts the given ip address to the dotted decimal representation.
 * The string is a statically allocated buffer, please copy result.
 * The given ip address MUST be in network byte order.
 */
char *
util_inet_ntoa (unsigned long ip)
{
  struct in_addr addr;

  addr.s_addr = ip;
  return inet_ntoa (addr);
  /*
  static char addr[16];

  sprintf (addr, "%lu.%lu.%lu.%lu", (ip >> 24) & 0xff, (ip >> 16) & 0xff,
	   (ip >> 8) & 0xff, ip & 0xff);
  return addr;
  */
}

/*
 * Converts a given string to an unsigned integer.
 */
unsigned int
util_atoi (char *str)
{
  unsigned int i = 0;

  while (*str >= '0' && *str <= '9')
    {
      i *= 10;
      i += *str - '0';
      str++;
    }
  return i;
}

/*
 * This routine checks for the current and maximum limit of open files
 * of the current process.
 */
int
util_openfiles (void)
{
#if HAVE_GETRLIMIT
# ifndef RLIMIT_NOFILE
#  define RLIMIT_NOFILE RLIMIT_OFILE
# endif
  struct rlimit rlim;

  if (getrlimit (RLIMIT_NOFILE, &rlim) == -1)
    {
      log_printf (LOG_ERROR, "getrlimit: %s\n", SYS_ERROR);
      return -1;
    }
  log_printf (LOG_NOTICE, "current open file limit: %d/%d\n", 
	      rlim.rlim_cur,  rlim.rlim_max);

  if (rlim.rlim_max < serveez_config.max_sockets || 
      rlim.rlim_cur < serveez_config.max_sockets)
    {
      rlim.rlim_max = serveez_config.max_sockets;
      rlim.rlim_cur = serveez_config.max_sockets;

      if (setrlimit (RLIMIT_NOFILE, &rlim) == -1)
	{
	  log_printf (LOG_ERROR, "setrlimit: %s\n", SYS_ERROR);
	  return -1;
	}
      getrlimit (RLIMIT_NOFILE, &rlim);
      log_printf (LOG_NOTICE, "open file limit set to: %d/%d\n",
		  rlim.rlim_cur,  rlim.rlim_max);
    }
#endif /* HAVE_GETRLIMIT */
  return 0;
}

/* Runtime checkable flags for sizzle and code */

#if defined(__MINGW32__) || defined(__CYGWIN__)
int have_win32 = 1;
#else
int have_win32 = 0;
#endif

#ifdef ENABLE_FLOOD_PROTECTION
int have_floodprotect = 1;
#else
int have_floodprotect = 0;
#endif

#ifdef ENABLE_DEBUG
int have_debug = 1;
#else
int have_debug = 0;
#endif
