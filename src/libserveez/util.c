/*
 * util.c - utility function implementation
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: util.c,v 1.3 2001/02/18 22:27:28 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef _AIX
# undef _NO_PROTO
# ifndef _USE_IRS
#  define _USE_IRS 1
# endif
# define _XOPEN_SOURCE_EXTENDED 1
# define _ALL_SOURCE 1
#endif /* _AIX */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#if HAVE_SYS_RESOURCE_H && !defined (__MINGW32__)
# include <sys/resource.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_STRINGS_H
# include <strings.h>
#endif

#ifndef __MINGW32__
# include <netdb.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#include "libserveez/snprintf.h"
#include "libserveez/windoze.h"
#include "libserveez/util.h"

/* 
 * Level of server's verbosity:
 * 0 - only fatal error messages
 * 1 - error messages
 * 2 - warnings
 * 3 - informational messages
 * 4 - debugging output
 * levels always imply numerically lesser levels
 */
int svz_verbosity = LOG_DEBUG;

static char log_level[][16] = {
  "fatal",
  "error",
  "warning",
  "notice",
  "debug"
};

/*
 * This is the file all log messages are written to. Change it with a
 * call to `log_set_file ()'.  By default, all log messages are written
 * to STDERR.
 */
static FILE *log_file = NULL;

/*
 * Print a message to the log system.
 */
void
log_printf (int level, const char *format, ...)
{
  va_list args;
  time_t tm;
  struct tm *t;

  if (level > svz_verbosity || log_file == NULL)
    return;

  tm = time (NULL);
  t = localtime (&tm);
  fprintf (log_file, "[%4d/%02d/%02d %02d:%02d:%02d] %s: ",
	   t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
	   t->tm_hour, t->tm_min, t->tm_sec, log_level[level]);
  va_start (args, format);
  vfprintf (log_file, format, args);
  va_end (args);
  fflush (log_file);
}

/*
 * Set the file stream FILE to the log file all messages are printed
 * to. Could also be STDOUT or STDERR.
 */
void
log_set_file (FILE * file)
{
  log_file = file;
}

/*
 * Dump a BUFFER with the length LEN to the file stream OUT. You can
 * specify a description in ACTION. The hexadecimal text representation of
 * the given buffer will be either cut at LEN or MAX. FROM is a numerical
 * identifier of the buffers creator.
 */
#define MAX_DUMP_LINE 16	/* bytes per line */
int
util_hexdump (FILE *out,	/* output FILE stream */
	      char *action,	/* hex dump description */
	      int from,		/* who created the dumped data */
	      char *buffer,	/* the buffer to dump */
	      int len,		/* length of that buffer */
	      int max)		/* maximum amount of bytes to dump (0 = all) */
{
  int row, col, x, max_col;

  if (!max)
    max = len;
  if (max > len)
    max = len;
  max_col = max / MAX_DUMP_LINE;
  if ((max % MAX_DUMP_LINE) != 0)
    max_col++;

  fprintf (out, "%s [ FROM:0x%08X SIZE:%d ]\n", action, (unsigned) from, len);

  for (x = row = 0; row < max_col && x < max; row++)
    {
      /* print hexdump */
      fprintf (out, "%04X   ", x);
      for (col = 0; col < MAX_DUMP_LINE; col++, x++)
	{
	  if (x < max)
	    fprintf (out, "%02X ", (unsigned char) buffer[x]);
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
 * This is the `hstrerror ()' wrapper function, depending on the 
 * configuration file `config.h'.
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
 * Transform the given binary data T (UTC time) to an ASCII time text
 * representation without any trailing characters.
 */
char *
util_time (time_t t)
{
  static char *asc;
  char *p;

  p = asc = ctime (&t);
  while (*p)
    p++;
  while (*p < ' ')
    *(p--) = '\0';

  return asc;
}

/*
 * Create some kind of uptime string. It tells how long the core library
 * has been running. 
 */
char *
util_uptime (time_t diff)
{
  static char text[64];
  time_t sec, min, hour, day, old;

  old = diff;
  sec = diff % 60;
  diff /= 60;
  min = diff % 60;
  diff /= 60;
  hour = diff % 24;
  diff /= 24;
  day = diff;

  if (old < 60)
    {
      sprintf (text, "%ld sec", sec);
    }
  else if (old < 60 * 60)
    {
      sprintf (text, "%ld min", min);
    }
  else if (old < 60 * 60 * 24)
    {
      sprintf (text, "%ld hours, %ld min", hour, min);
    }
  else
    {
      sprintf (text, "%ld days, %ld:%02ld", day, hour, min);
    }

  return text;
}

/*
 * Convert the given string STR to lower case text representation.
 */
char *
util_tolower (char *str)
{
  char *p = str;

  while (*p)
    {
      *p = (char) (isupper ((byte) * p) ? tolower ((byte) * p) : *p);
      p++;
    }
  return str;
}

/*
 * These are the system dependent case insensitive string compares. They
 * compare the strings STR1 and STR2 (optional up to N characters). Return
 * zero if both strings are equal.
 */
int
util_strcasecmp (const char *str1, const char *str2)
{
#if HAVE_STRCASECMP
  return strcasecmp (str1, str2);
#elif HAVE_STRICMP
  return stricmp (str1, str2);
#else
  const char *p1 = str1;
  const char *p2 = str2;
  unsigned char c1, c2;

  if (p1 == p2)
    return 0;

  do
    {
      c1 = isupper (*p1) ? tolower (*p1) : *p1;
      c2 = isupper (*p2) ? tolower (*p2) : *p2;
      if (c1 == '\0')
	break;
      ++p1;
      ++p2;
    }
  while (c1 == c2);

  return c1 - c2;
#endif /* neither HAVE_STRCASECMP nor HAVE_STRICMP */
}

int
util_strncasecmp (const char *str1, const char *str2, size_t n)
{
#if HAVE_STRNCASECMP
  return strncasecmp (str1, str2, n);
#elif HAVE_STRNICMP
  return strnicmp (str1, str2, n);
#else
  const char *p1 = str1;
  const char *p2 = str2;
  unsigned char c1, c2;

  if (p1 == p2)
    return 0;

  do
    {
      c1 = isupper (*p1) ? tolower (*p1) : *p1;
      c2 = isupper (*p2) ? tolower (*p2) : *p2;
      if (c1 == '\0')
	break;
      ++p1;
      ++p2;
    }
  while (c1 == c2 && --n > 0);

  return c1 - c2;
#endif /* neither HAVE_STRCASECMP nor HAVE_STRICMP */
}

#ifdef __MINGW32__
/*
 * This variable contains the last system or network error occurred if 
 * it was detected and printed. Needed for the "Resource unavailable" error
 * condition.
 */
int svz_errno = 0;

#define MESSAGE_BUF_SIZE 256

/*
 * There is no text representation of network (Winsock API) errors in 
 * Win32. That is why we translate it by hand.
 */
static char *
util_neterror (int error)
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
    default:
      sprintf (message, "Network error code %d.", error);
      break;
    }
  return message;
}

/*
 * Routine which forms a valid error message under Win32. It might either
 * use the `GetLastError ()' or `WSAGetLastError ()' in order to get a valid
 * error code.
 */
char *
util_syserror (int nr)
{
  static char message[MESSAGE_BUF_SIZE];
  LPTSTR error;

  /* save the last error */
  svz_errno = nr;

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
  if (0 ==
      FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
		     FORMAT_MESSAGE_FROM_SYSTEM |
		     FORMAT_MESSAGE_ARGUMENT_ARRAY, NULL, nr,
		     MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
		     (char *) &error, 0, NULL))
    {
      sprintf (message, "FormatMessage: error code %ld", GetLastError ());
      return message;
    }

  strcpy (message, error);
  message[strlen (message) - 2] = 0;
  LocalFree (error);
  return message;
}

/*
 * This variable contains the the runtime detected Win32 version. Its value
 * is setup in `util_version ()'.
 * 0 - Windows 3.x     1 - Windows 95      2 - Windows 98
 * 3 - Windows NT 3.x  4 - Windows NT 4.x  5 - Windows 2000
 * 6 - Windows ME
 */
int svz_os_version = 0;

#endif /* __MINGW32__ */

/*
 * This routine is for detecting the operating system version of Win32 
 * and all Unices at runtime. You should call it at least once at startup.
 * It saves its result in the variable `svz_os_version' and prints an
 * appropriate message.
 */
char *
util_version (void)
{
  static char os[256] = ""; /* contains the os string */

#ifdef __MINGW32__
  static char ver[][6] =
    { " 32s", " 95", " 98", " NT", " NT", " 2000", " ME" };
  OSVERSIONINFO osver;
#elif HAVE_SYS_UTSNAME_H
  struct utsname buf;
#endif

  /* detect only once */
  if (os[0])
    return os;

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
	case VER_PLATFORM_WIN32_NT: /* NT or Windows 2000 */
	  if (osver.dwMajorVersion == 4)
	    svz_os_version = WinNT4x;
	  else if (osver.dwMajorVersion <= 3)
	    svz_os_version = WinNT3x;
	  else if (osver.dwMajorVersion == 5)
	    svz_os_version = Win2k;
	  break;

	case VER_PLATFORM_WIN32_WINDOWS: /* Win95 or Win98 */
	  if ((osver.dwMajorVersion > 4) ||
	      ((osver.dwMajorVersion == 4) && (osver.dwMinorVersion > 0)))
	    {
	      if (osver.dwMinorVersion >= 90)
		svz_os_version = WinME;
	      else
		svz_os_version = Win98;
	    }
	  else
	    svz_os_version = Win95;
	  break;

	case VER_PLATFORM_WIN32s: /* Windows 3.x */
	  svz_os_version = Win32s;
	  break;
	}

      sprintf (os, "Windows%s %ld.%02ld %s%s(Build %ld)",
	       ver[svz_os_version], 
	       osver.dwMajorVersion, osver.dwMinorVersion,
	       osver.szCSDVersion, osver.szCSDVersion[0] ? " " : "",
	       osver.dwBuildNumber & 0xFFFF);
    }
#elif HAVE_UNAME /* !__MINGW32__ */
  uname (&buf);
  sprintf (os, "%s %s on %s", buf.sysname, buf.release, buf.machine);
#endif /* not HAVE_UNAME */

  return os;
}

/*
 * Converts an unsigned integer to its decimal string representation 
 * returning a pointer to an internal buffer, so copy the result.
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
      *p = (char) ((i % 10) + '0');
    }
  while ((i /= 10) != 0);
  return p;
}

/*
 * Converts a given string STR in decimal format to an unsigned integer.
 * Stops conversion on any invalid characters.
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
 * Converts the given ip address IP to the dotted decimal representation.
 * The string is a statically allocated buffer, please copy the result.
 * The given ip address MUST be in network byte order.
 */
char *
util_inet_ntoa (unsigned long ip)
{
#if !BROKEN_INET_NTOA
  struct in_addr addr;

  addr.s_addr = ip;
  return inet_ntoa (addr);

#else /* BROKEN_INET_NTOA */

  static char addr[16];

  /* 
   * Now, this is strange: IP is given in host byte order. Nevertheless
   * conversion is endian-specific. To the binary AND and SHIFT operations
   * work differently on different architectures ?
   */
  sprintf (addr, "%lu.%lu.%lu.%lu",
#if WORDS_BIGENDIAN
	   (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
#else /* Little Endian */
	   ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
#endif
  return addr;

#endif /* BROKEN_INET_NTOA */
}

/*
 * Converts the Internet host address @var{str} from the standard 
 * numbers-and-dots notation into binary data and stores it in the 
 * structure that @var{addr} points to. @code{util_inet_aton()} returns 
 * zero if the address is valid, nonzero if not.
 */
int
util_inet_aton (char *str, struct sockaddr_in *addr)
{
#if HAVE_INET_ATON
  if (inet_aton (str, &addr->sin_addr) == 0)
    {
      return -1;
    }
#elif defined (__MINGW32__)
  int len = sizeof (struct sockaddr_in);
  if (WSAStringToAddress (str, AF_INET, NULL, 
                          (struct sockaddr *) addr, &len) != 0)
    {
      return -1;
    }
#else /* not HAVE_INET_ATON and not __MINGW32__ */
  addr->sin_addr.s_addr = inet_addr (str);
#endif /* not HAVE_INET_ATON */
  return 0;
}

/*
 * This routine checks for the current and maximum limit of open files
 * of the current process. The function heavily depends on the underlying
 * platform. It tries to set the limit to the given MAX_SOCKETS amount.
 */
int
util_openfiles (int max_sockets)
{
#if HAVE_GETRLIMIT
  struct rlimit rlim;
#endif

#if HAVE_GETDTABLESIZE
  int openfiles;

  if ((openfiles = getdtablesize ()) == -1)
    {
      log_printf (LOG_ERROR, "getdtablesize: %s\n", SYS_ERROR);
      return -1;
    }
  log_printf (LOG_NOTICE, "file descriptor table size: %d\n", openfiles);
#endif /* HAVE_GETDTABLESIZE */

#if HAVE_GETRLIMIT

# ifndef RLIMIT_NOFILE
#  define RLIMIT_NOFILE RLIMIT_OFILE
# endif

  if (getrlimit (RLIMIT_NOFILE, &rlim) == -1)
    {
      log_printf (LOG_ERROR, "getrlimit: %s\n", SYS_ERROR);
      return -1;
    }
  log_printf (LOG_NOTICE, "current open file limit: %d/%d\n",
	      rlim.rlim_cur, rlim.rlim_max);

  if ((int) rlim.rlim_max < (int) max_sockets || 
      (int) rlim.rlim_cur < (int) max_sockets)
    {
      rlim.rlim_max = max_sockets;
      rlim.rlim_cur = max_sockets;

      if (setrlimit (RLIMIT_NOFILE, &rlim) == -1)
	{
	  log_printf (LOG_ERROR, "setrlimit: %s\n", SYS_ERROR);
	  return -1;
	}
      getrlimit (RLIMIT_NOFILE, &rlim);
      log_printf (LOG_NOTICE, "open file limit set to: %d/%d\n",
		  rlim.rlim_cur, rlim.rlim_max);
    }

#elif defined (__MINGW32__)	/* HAVE_GETRLIMIT */

  unsigned sockets = 100;

  if (svz_os_version == Win95 || 
      svz_os_version == Win98 || svz_os_version == WinME)
    {
      if (svz_os_version == Win95)
	sockets = windoze_get_reg_unsigned (MaxSocketKey,
					    MaxSocketSubKey,
					    MaxSocketSubSubKey, sockets);
      else
	sockets = util_atoi (windoze_get_reg_string (MaxSocketKey,
						     MaxSocketSubKey,
						     MaxSocketSubSubKey,
						     util_itoa (sockets)));

      log_printf (LOG_NOTICE, "current open file limit: %u\n", sockets);

      if (sockets < (unsigned) max_sockets)
	{
	  sockets = max_sockets;

	  if (svz_os_version == Win95)
	    windoze_set_reg_unsigned (MaxSocketKey,
				      MaxSocketSubKey,
				      MaxSocketSubSubKey, sockets);
	  else
	    windoze_set_reg_string (MaxSocketKey,
				    MaxSocketSubKey,
				    MaxSocketSubSubKey, util_itoa (sockets));

	  log_printf (LOG_NOTICE, "open file limit set to: %u\n", sockets);
	}
    }
#endif /* MINGW32__ */

  return 0;
}

/* Runtime checkable flags for sizzle and code. */

#if defined (__MINGW32__) || defined (__CYGWIN__)
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
