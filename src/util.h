/*
 * src/util.h - utility function interface
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
 * $Id: util.h,v 1.22 2000/09/26 18:08:52 ela Exp $
 *
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

#if HAVE_SYS_UTSNAME_H
# include <sys/utsname.h>
#endif

/* declare crypt interface if necessary */
#if ENABLE_CRYPT && HAVE_CRYPT
extern char *crypt (const char *key, const char *salt);
extern char *getpass (const char *prompt);
#endif

typedef unsigned char byte;

/* 
 * level of server's verbosity:
 * 0 - only fatal error messages
 * 1 - error messages
 * 2 - warnings
 * 3 - informational messages
 * 4 - debugging output
 * levels always imply numerically lesser levels
 */
#define LOG_FATAL     0
#define LOG_ERROR     1
#define LOG_WARNING   2
#define LOG_NOTICE    3
#define LOG_DEBUG     4

extern int verbosity;

#ifdef __MINGW32__

/*
 * The variable os_cersion could be used to differentiate between
 * some Windows32 versions. GetVersion() as to be called somewhere.
 */
#define Win32s  0
#define Win95   1
#define Win98   2
#define WinNT3x 3
#define WinNT4x 4
#define Win2k   5
#define WinME   6

extern int os_version;

#endif /* __MINGW32__ */

#ifndef __STDC__
void log_printf ();
#else
void log_printf (int level, const char *format, ...);
#endif
void log_set_file (FILE *file);

int util_hexdump (FILE *out, char *, int, char *, int, int);
char *util_inet_ntoa (unsigned long ip);
char *util_itoa (unsigned int);
unsigned int util_atoi (char *);
int util_strcasecmp (const char *str1, const char *str2);
int util_strncasecmp (const char *str1, const char *str2, size_t n);
int util_openfiles (void);
char *util_time (time_t t);
char *util_tolower (char *str);
char *util_version (void);

/* char pointer to integer cast, needed for aligned Machines (IRIX, Solaris) */
#define INT32(p) ((unsigned char)*p + \
                 ((unsigned char)*(p+1)<<8) + \
                 ((unsigned char)*(p+2)<<16) + \
                 ((signed char)*(p+3)<<24))
#define INT16(p) ((unsigned char)*p + \
                 ((signed char)*(p+1)<<8))

#define UINT32(p) ((unsigned char)*p + \
                  ((unsigned char)*(p+1)<<8) + \
                  ((unsigned char)*(p+2)<<16) + \
                  ((unsigned char)*(p+3)<<24))
#define UINT16(p) ((unsigned char)*p + \
                  ((unsigned char)*(p+1)<<8))

#ifdef __MINGW32__
# define ENV_BLOCK_TYPE char *
# define INVALID_HANDLE NULL
# define LEAST_WAIT_OBJECT 1
# define SOCK_UNAVAILABLE WSAEWOULDBLOCK
# define SOCK_INPROGRESS WSAEINPROGRESS
#else
# define ENV_BLOCK_TYPE char **
# define INVALID_HANDLE -1
# define SOCK_UNAVAILABLE EAGAIN
# define SOCK_INPROGRESS EINPROGRESS
#endif

#ifdef __MINGW32__

char *GetErrorMessage (int);
extern int last_errno;

#define MESSAGE_BUF_SIZE 256
#define WINSOCK_VERSION  0x0202 /* This is version 2.02 */

#define dup(handle) _dup(handle)
#define dup2(handle1, handle2) _dup2(handle1, handle2)

#endif /* __MINGW32__ */

const char * util_hstrerror (void);

/*
 * Definition of sys-dependent routines.
 */

#ifdef __MINGW32__

#define closehandle(handle) (CloseHandle (handle) ? 0 : -1)
#define SYS_ERROR GetErrorMessage (GetLastError ())
#define NET_ERROR GetErrorMessage (WSAGetLastError ())
#define H_NET_ERROR GetErrorMessage (WSAGetLastError ())
#define getcwd(buf, size) (GetCurrentDirectory (size, buf) ? buf : NULL)
#define chdir(path) (SetCurrentDirectory (path) ? 0 : -1)

/* 
 * This little modification is necessary for the native Win32 compiler.
 * We do have these macros defined in the MinGW32 and Cygwin headers
 * but not within the native Win32 headers.
 */
#ifndef S_ISDIR
# define S_ISDIR(Mode) ((Mode) & S_IFDIR)
# define S_ISCHR(Mode) ((Mode) & S_IFCHR)
# define S_ISREG(Mode) ((Mode) & S_IFREG)
#endif /* not S_ISDIR */

#else /* Unices here. */

#define closesocket(sock) close (sock)
#define closehandle(handle) close (handle)
#define SYS_ERROR strerror (errno)
#define NET_ERROR strerror (errno)
#define H_NET_ERROR util_hstrerror ()
#define last_errno errno

#endif

#endif /* not __UTIL_H__ */
