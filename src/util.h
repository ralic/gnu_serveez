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
 * $Id: util.h,v 1.4 2000/06/12 23:06:05 raimi Exp $
 *
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>

#if HAVE_SYS_UTSNAME_H
# include <sys/utsname.h>
#endif

#ifndef NDEBUG
# include <assert.h>
#endif

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

extern int os_version;

#endif /* __MINGW32__ */

void log_printf (int level, const char *format, ...);
void set_log_file (FILE *file);
int dump_request (FILE *out, char * action, int from, char * req, int len);
char *util_inet_ntoa(unsigned long ip);
char *util_itoa (unsigned int);
unsigned int util_atoi (char *);
void list_local_interfaces(void);

/* char pointer to integer cast, needed for aligned Machines (IRIX, Solaris) */
#define INT32(p) (*p + (*(p+1)<<8) + (*(p+2)<<16) + (*(p+3)<<24))
#define INT16(p) (*p + (*(p+1)<<8))

#define UINT32(p) ((unsigned char)*p + \
                  ((unsigned char)*(p+1)<<8) + \
                  ((unsigned char)*(p+2)<<16) + \
                  ((unsigned char)*(p+3)<<24))
#define UINT16(p) ((unsigned char)*p + \
                  ((unsigned char)*(p+1)<<8))

#define READ  0 /* read pipe */
#define WRITE 1 /* write pipe */

#ifdef __MINGW32__
# define ENV_BLOCK_TYPE char *
# define INVALID_HANDLE NULL
# define LEAST_WAIT_OBJECT 1
# define SOCK_UNAVAILABLE WSAEWOULDBLOCK
#else
# define ENV_BLOCK_TYPE char **
# define INVALID_HANDLE -1
# define SOCK_UNAVAILABLE EAGAIN
#endif

char *get_version(void);

#ifdef __MINGW32__
/*
 * Some words to Win32.
 *
 * To use the vanilla Win32 winsock, you just need to #define Win32_Winsock 
 * and #include "windows.h" at the top of your source file(s). You'll also
 * want to add -lwsock32 to the compiler's command line so you link against 
 * libwsock32.a. 
 *
 * How do I make the console window go away?
 * The default during compilation is to produce a console application. If you 
 * are writing a GUI program, you should either compile with -mwindows as
 * explained above, or add the string "-Wl,--subsystem,windows" to the GCC 
 * commandline. 
 *
 * How can I find out which dlls are needed by an executable?
 * objdump -p provides this information. 
 *
 * What preprocessor do I need to know about?
 * We use _WIN32 to signify access to the Win32 API and __CYGWIN__ for access 
 * to the Cygwin environment provided by the dll. 
 * We chose _WIN32 because this is what Microsoft defines in VC++ and we 
 * thought it would be a good idea for compatibility with VC++ code to
 * follow their example. We use _MFC_VER to indicate code that should be 
 * compiled with VC++. 
 */

/*
 * Why we do not use pipes for coservers.
 *
 * Windows differentiates between sockets and filedescriptors,
 * that's why you can not select() filedescriptors.
 * Please close() the pipe's descriptors via _close() and not 
 * socketclose(), because this will fail.
 */

/*
 * The C run-time libraries have a preset limit for the number of files that
 * can be open at any one time. The limit for applications that link with the
 * single-thread static library (LIBC.LIB) is 64 file handles or 20 file
 * streams. Applications that link with either the static or dynamic
 * multithread library (LIBCMT.LIB or MSVCRT.LIB and MSVCRT.DLL), have a limit
 * of 256 file handles or 40 file streams. Attempting to open more than the
 * maximum number of file handles or file streams causes program failure.
 *
 * i know mingw uses the msvcrt.dll and hence should have the above limitation
 * but what about cygwin which uses glibc? Just curious ... i dont think there
 * is any operating system limitation may be some TLS issue from Microsoft or
 * some other stuff like that..
 *
 * Ah, as far as I know, one of the big limitations of winsock is that
 * the SOCKET type is *not* equivalent to file descriptor unlike that
 * using BSD or POSIX sockets. That's one of the major reasons for using
 * a separate data type, SOCKET, as opposed to int, typical type of a
 * file descriptor. This implies that you cannot mix SOCKETs and stdio,
 * sorry. This is the case when you use -mno-cygwin.
 *
 * socket handles are NOT file handles only under win9x. Under WNT they ARE
 * file handles.
 *
 * Actually They are regular file handles, just like any other.
 * There is a bug in all 9x/kernel32 libc/msv/crtdll interface
 * implementations GetFileType() returns TYPE_UNKNOWN for handles to
 * sockets. Since this is AFAIK the only unknown type there is, you know you 
 * have a socket handle;-)
 * there is a fix in the more recent perl distrib's
 * that you can use as a general solution.
 * -loldnames -lperlcrt -lmsvcrt will get you TYPE_CHAR
 * for socket handles. that are put into an fd with _open_osfhandle()
 * also fixes several other nasty bugs in the MS libcXXX.
 *
 */

char *GetErrorMessage(int);
extern int last_errno;

#define MESSAGE_BUF_SIZE 256

#endif /* __MINGW32__ */

const char * util_hstrerror (void);

/*
 * Definition of sys-dependent routines.
 */

#ifdef __MINGW32__

#define CLOSE_SOCKET(sock) closesocket(sock)
#define CLOSE_HANDLE(handle) (CloseHandle(handle) ? 0 : -1)
#define SYS_ERROR GetErrorMessage(GetLastError())
#define NET_ERROR GetErrorMessage(WSAGetLastError())
#define H_NET_ERROR GetErrorMessage(WSAGetLastError())
#define WRITE_PIPE(pipe, buffer, count) write(pipe, buffer, count)
#define READ_PIPE(pipe, buffer, count) read(pipe, buffer, count)

#else /* Unices here. */

#define CLOSE_SOCKET(sock) close(sock)
#define CLOSE_HANDLE(handle) close(handle)
#define SYS_ERROR strerror(errno)
#define NET_ERROR strerror(errno)
#define H_NET_ERROR util_hstrerror()
#define WRITE_PIPE(pipe, buffer, count) write(pipe, buffer, count)
#define READ_PIPE(pipe, buffer, count) read(pipe, buffer, count)
#define last_errno errno

#endif

#endif /* not __UTIL_H__ */
