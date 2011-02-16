dnl -*-autoconf-*-
dnl
dnl configure.ac
dnl
dnl Autoconf input file.
dnl
dnl Copyright (C) 2011 Thien-Thi Nguyen
dnl Copyright (C) 2000, 2001, 2002, 2003, 2004 Stefan Jahn <stefan@lkcc.org>
dnl Copyright (C) 2000, 2001 Raimund Jacob <raimi@lkcc.org>
dnl Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
dnl
dnl This is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 3, or (at your option)
dnl any later version.
dnl
dnl This software is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this package.  If not, see <http://www.gnu.org/licenses/>.
dnl

dnl ----------------------------------------------------------------------

dnl
dnl Initialize autoconf and automake
dnl
AC_PREREQ([2.64])
AC_INIT([GNU Serveez],[0.1.7],[bug-serveez@gnu.org])
AC_CONFIG_HEADERS([src/config.h src/svzconfig.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([1.10 no-define])

dnl
dnl Determine if maintainer portions of the Makefiles should be included.
dnl
AM_MAINTAINER_MODE

dnl ----------------------------------------------------------------------

dnl
dnl Check for a proper C compiler.
dnl
AC_PROG_CC

AC_C_BIGENDIAN

AC_CHECK_SIZEOF([long], 4)
AC_CHECK_SIZEOF([void *], 4)
AS_IF([test $ac_cv_sizeof_long -lt $ac_cv_sizeof_void_p],
[AC_MSG_ERROR([A 'void *' value does not fit into a 'long' value.])])

dnl
dnl Locate required external software.
dnl
AC_PROG_INSTALL
AC_PROG_SED
AC_PROG_AWK
AC_CHECK_PROG([AR],[ar],[ar],[:])
AC_CHECK_PROG([WINDRES],[windres],[windres],[:])

dnl ----------------------------------------------------------------------

AS_IF([grep "$host" $srcdir/data/PLATFORMS 1>/dev/null 2>&1],[:],
[AC_MSG_RESULT([
  $PACKAGE_NAME has never been compiled or run on this platform.
  Please send an email to <dev-serveez@gnu.org> including your compiler
  version and whether it worked correctly or not.  '$host'
  will then be added to the PLATFORMS file included in this package.
])])

dnl ----------------------------------------------------------------------

AS_CASE([$host],
 [mipsel-unknown-linux-gnu | mips-unknown-linux-gnu],[NOUNDEFINED=],
 [NOUNDEFINED='-no-undefined'])
AC_SUBST([NOUNDEFINED])

dnl ----------------------------------------------------------------------

EXTRALIBS=""
CPPEXTRAFLAGS=""
SERVERLIBS=""
SERVERDIRS=""
SERVEEZ_LIBS=""
SERVEEZ_LDFLAGS=""

AC_SUBST([CPPEXTRAFLAGS])
AC_SUBST([CPPFLAGS])
AC_SUBST([SERVERLIBS])
AC_SUBST([SERVERDIRS])
AC_SUBST([SERVEEZ_LIBS])
AC_SUBST([SERVEEZ_LDFLAGS])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether compilation warnings are desired.
dnl

SVZ_FLAG([whether compiler warnings are enabled],
         [no],[warn],[Turn on most compiler warnings],[
  save_cflags="$CFLAGS"
  warn_cflags=""
  warnflags="-W -fullwarn -pedantic"

  dnl Only make these tests if we are using GNU C
  AS_IF([SVZ_Y([GCC])],[
    warnflags="$warnflags -Wall -Wcast-align -Wstrict-prototypes \
                          -Wformat -Wno-unused -Wno-long-long"
  ])

  dnl Test this compiler flag under Linux only.
  AS_CASE([$host_os],[linux*],[warnflags="$warnflags -ansi"])

  for flag in $warnflags; do
    CFLAGS="$flag"
    AC_MSG_CHECKING([whether $CC accepts $CFLAGS])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
      [warn_cflags="$warn_cflags $CFLAGS"
      AC_MSG_RESULT([yes])],[AC_MSG_RESULT([no])])
  done
  AS_UNSET([warnflags])

  CFLAGS="$save_cflags"
  CPPEXTRAFLAGS="$warn_cflags"
  AS_UNSET([warn_cflags])
  AS_UNSET([save_cflags])
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether compilation optimizations are desired.
dnl

SVZ_FLAG([whether to enable optimizations],
         [no],[opt],[Turn on compiler optimizations],[
  save_cflags="$CFLAGS"
  opt_cflags=""
  opt_ldflags=""

  dnl Only make these tests if we are using GNU C
  AS_IF([SVZ_Y([GCC])],[
    optflags="-O2 -fomit-frame-pointer -fstrength-reduce -funroll-loops \
              -finline-functions -fexpensive-optimizations \
	      -fcaller-saves -frerun-loop-opt -foptimize-register-move \
	      -ffunction-cse -fpeephole -momit-leaf-frame-pointer"
    [arch=`eval echo "$host_cpu" | sed 's/i\([0-9]\)86/\1/g'`
    test "$arch" = "$host_cpu" && arch="5"
    test `expr $arch` -gt 4 && optflags="$optflags -fschedule-insns2"]
    AS_UNSET([arch])
    for flag in $optflags; do
      CFLAGS="$flag"
      AC_MSG_CHECKING([whether $CC accepts $CFLAGS])
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
        [opt_cflags="$opt_cflags $CFLAGS"
        AC_MSG_RESULT([yes])],[AC_MSG_RESULT([no])])
    done
    AS_UNSET([optflags])
    CFLAGS="-m486"
    AC_MSG_CHECKING([whether $CC accepts $CFLAGS])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
      [opt_is486="yes"; AC_MSG_RESULT([yes])],
      [opt_is486="no";  AC_MSG_RESULT([no])])
    CFLAGS="-march=pentiumpro"
    AC_MSG_CHECKING([whether $CC accepts $CFLAGS])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
      [opt_isPPro="yes"; AC_MSG_RESULT([yes])],
      [opt_isPPro="no";  AC_MSG_RESULT([no])])
  ],[
    dnl Other compiler than GNU C
    optflags="-O3"
    for flag in $optflags; do
      CFLAGS="$flag"
      AC_MSG_CHECKING([whether $CC accepts $CFLAGS])
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
        [opt_cflags="$opt_cflags $CFLAGS"
        AC_MSG_RESULT([yes])],[AC_MSG_RESULT([no])])
    done
    AS_UNSET([optflags])
  ])

  dnl Check for additional optimization compiler flags.
  SVZ_WITH([""],[opt],[FLAGS],[Additional optimization (comma-separated) FLAGS],
           [""],[""],[`echo "$withval" | sed 's/,/ /g'`])
  optflags="$with_opt"
  for flag in $optflags; do
    CFLAGS="$flag"
    AC_MSG_CHECKING([whether $CC accepts $CFLAGS])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
      [opt_cflags="$opt_cflags $CFLAGS"
      AC_MSG_RESULT([yes])],[AC_MSG_RESULT([no])])
  done
  AS_UNSET([optflags])

  LDFLAGS="-s"
  AC_MSG_CHECKING([whether $CC accepts $LDFLAGS])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[opt_ldflags="$opt_ldflags $LDFLAGS"
    AC_MSG_RESULT([yes])],[AC_MSG_RESULT([no])])

  AS_IF([SVZ_Y([opt_isPPro])],
   [SVZ_FLAG([whether to enable i686 optimizations],
             [no],[ppro],[Turn on i686 optimizations],[
    opt_cflags="$opt_cflags -march=pentiumpro"
    opt_isPPro=done])])

  AS_IF([SVZ_Y([opt_is486]) && test xdone != x"$opt_isPPro"],
   [SVZ_FLAG([whether to enable i486 optimizations],
             [yes],[486],[Turn on i486 optimizations],[
    opt_cflags="$opt_cflags -m486"])])

  LDFLAGS="$opt_ldflags"
  CPPEXTRAFLAGS="$CPPEXTRAFLAGS $opt_cflags"
  CFLAGS=""
  AS_UNSET([save_cflags])
  AS_UNSET([opt_cflags])
  AS_UNSET([opt_ldflags])
])

AC_SUBST([CFLAGS])
AC_SUBST([LDFLAGS])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether debug output should be included.
dnl

SVZ_FLAG([whether to enable debug output],
         [yes],[debug],[Include debug output],[
AC_DEFINE([SVZ_ENABLE_DEBUG], 1,
  [Define if debug output should be supported.])
],[
AC_DEFINE([NDEBUG], 1,
  [Define if debug code should be suppressed.])
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether we try to list local interfaces.
dnl

SVZ_FLAG([whether to enable list of local interfaces],
         [yes],[iflist],[List local interfaces],[
AC_DEFINE([ENABLE_IFLIST], 1,
  [Define if we are able to list local network interfaces.])
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether poll loop should be supported.
dnl

SVZ_FLAG([whether to enable poll loop],
         [yes],[poll],[Include poll(2) server loop],[
AC_DEFINE([ENABLE_POLL], 1,
  [Define if poll(2) should be supported if possible.])
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether ‘sendfile’ should be supported.
dnl

SVZ_FLAG([whether to use sendfile(2) if possible],
         [yes],[sendfile],[Include sendfile(2) support],[
AC_DEFINE([ENABLE_SENDFILE], 1,
  [Define if sendfile(2) should be supported if possible.])
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether some thread safety functionality should be included.
dnl

SVZ_FLAG([whether to include thread safety functionality],
         [yes],[thread-safety],[Enable some thread-safety functionality])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether the SNTP server should be included.
dnl

SVZ_FLAG([whether to enable the SNTP server],
         [yes],[sntp-proto],[Include the SNTP server],[
AC_DEFINE([ENABLE_SNTP_PROTO], 1,
  [Define if the SNTP server should be included.])
SERVERLIBS="$SERVERLIBS sntp-server/libsntp.a"
SERVERDIRS="$SERVERDIRS sntp-server"
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether the Gnutella server should be included.
dnl

SVZ_FLAG([whether to enable the Gnutella spider],
         [yes],[gnutella],[Include the Gnutella spider],[
AC_DEFINE([ENABLE_GNUTELLA], 1,
  [Define if the Gnutella spider should be included.])
SERVERLIBS="$SERVERLIBS nut-server/libnut.a"
SERVERDIRS="$SERVERDIRS nut-server"
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether the Tunnel server should be included.
dnl

SVZ_FLAG([whether to enable the port forwarder],
         [yes],[tunnel],[Include port forwarder],[
AC_DEFINE([ENABLE_TUNNEL], 1,
  [Define if the port forwarder should be included.])
SERVERLIBS="$SERVERLIBS tunnel-server/libtunnel.a"
SERVERDIRS="$SERVERDIRS tunnel-server"
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether the control protocol should be included.
dnl

SVZ_FLAG([whether to enable support for the control protocol],
         [yes],[control-proto],[Include the control protocol],[
AC_DEFINE([ENABLE_CONTROL_PROTO], 1,
  [Define if the control protocol should be supported.])
SERVERLIBS="$SERVERLIBS ctrl-server/libctrl.a"
SERVERDIRS="$SERVERDIRS ctrl-server"
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether the IRC protocol should be included.
dnl

SVZ_FLAG([whether to enable support for the IRC protocol],
         [yes],[irc-proto],[Include the IRC protocol],[
AC_DEFINE([ENABLE_IRC_PROTO], 1,
  [Define if the IRC protocol should be supported.])
SERVERLIBS="$SERVERLIBS irc-server/libircserver.a irc-core/libirccore.a"
SERVERDIRS="$SERVERDIRS irc-server irc-core"
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether the IRC protocol "TimeStamp" extension should be included.
dnl

SVZ_FLAG([whether to enable TimeStamp for the IRC protocol],
         [yes],[irc-ts],[Include TimeStamp in IRC protocol],[
AC_DEFINE([ENABLE_TIMESTAMP], 1,
  [Define if the IRC protocol TimeStamp extension should be supported.])
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether the password encryption should be included.
dnl

SVZ_FLAG([whether to enable password encryption],
         [no],[crypt],[Include password encryption])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether the aWCS protocol should be included.
dnl

SVZ_FLAG([whether to enable support for the aWCS protocol],
         [yes],[awcs-proto],[Include the aWCS protocol],[
AC_DEFINE([ENABLE_AWCS_PROTO], 1,
  [Define if the aWCS protocol should be supported.])
SERVERLIBS="$SERVERLIBS awcs-server/libawcs.a"
SERVERDIRS="$SERVERDIRS awcs-server"
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether the HTTP protocol should be included.
dnl

SVZ_FLAG([whether to enable support for the HTTP protocol],
         [yes],[http-proto],[Include the HTTP protocol],[
AC_DEFINE([ENABLE_HTTP_PROTO], 1,
  [Define if the HTTP protocol should be supported.])
SERVERLIBS="$SERVERLIBS http-server/libhttp.a"
SERVERDIRS="$SERVERDIRS http-server"
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether the Fake Ident Server should be included.
dnl

SVZ_FLAG([whether to enable the fake ident server],
         [yes],[fakeident],[Include the fake ident server],[
AC_DEFINE([ENABLE_FAKEIDENT], 1,
  [Define if the Fake Ident Server should be included.])
SERVERLIBS="$SERVERLIBS fakeident-server/libfakeident.a"
SERVERDIRS="$SERVERDIRS fakeident-server"
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether the passthrough server should be included.
dnl

SVZ_FLAG([whether to enable the passthrough server],
         [yes],[passthrough],[Include the passthrough server],[
AC_DEFINE([ENABLE_PROG_SERVER], 1,
  [Define if the program passthrough server should be included.])
SERVERLIBS="$SERVERLIBS prog-server/libprog.a"
SERVERDIRS="$SERVERDIRS prog-server"
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether the flood protection should be included.
dnl

SVZ_FLAG([whether to enable support for flood protection],
         [yes],[flood],[Include flood protection],[
AC_DEFINE([SVZ_ENABLE_FLOOD_PROTECTION], 1,
  [Define if flood protection should be supported.])
])

dnl ----------------------------------------------------------------------

dnl
dnl Check for additional searchpath for MinGW32 headers and libraries.
dnl

SVZ_WITH([/usr/local/mingw],[mingw],[DIR],[MinGW32 files are in DIR],
         [no],[/usr/local/mingw],["$withval"])
MINGDIR="$with_mingw"

dnl
dnl Check if this is Win32 (CYGWIN/MINGW32).
dnl

AS_CASE([$host_os],
[*mingw32*],[MINGW32=yes CYGWIN=no],
[*cygwin*],[MINGW32=no CYGWIN=yes])
AC_EXEEXT
AC_OBJEXT

resource_FILES=""
AS_IF([SVZ_Y([CYGWIN]) || SVZ_Y([MINGW32])],[
  AC_DEFINE([FD_SETSIZE], 4096, [Make CygWin / MinGW32 use large FD sets.])
  AC_DEFINE([WIN32_LEAN_AND_MEAN], 1, [Define for faster code generation.])
  AC_CHECK_LIB([advapi32],[main],[EXTRALIBS="$EXTRALIBS -ladvapi32"])
  AC_CHECK_LIB([shell32],[main],[EXTRALIBS="$EXTRALIBS -lshell32"])
  AC_CHECK_LIB([user32],[main],[EXTRALIBS="$EXTRALIBS -luser32"])
  resource_FILES="serveez.rc"
  AS_IF([test xno != x"$MINGDIR" && SVZ_NOT_Y([MINGW32])],[
    CFLAGS="$CFLAGS -mno-cygwin -I${MINGDIR}/include"
    LDFLAGS="$LDFLAGS -mno-cygwin -L${MINGDIR}/lib"
    AC_CHECK_LIB([ws2_32],[main],[EXTRALIBS="$EXTRALIBS -lws2_32"])
    AC_CHECK_LIB([mswsock],[main],[EXTRALIBS="$EXTRALIBS -lmswsock"])
    AC_MSG_RESULT([using MinGW32 headers and libraries in ${MINGDIR}])
  ])
  AS_IF([SVZ_Y([MINGW32])],[
    AC_CHECK_LIB([ws2_32],[main],[EXTRALIBS="$EXTRALIBS -lws2_32"])
    AC_CHECK_LIB([mswsock],[main],[EXTRALIBS="$EXTRALIBS -lmswsock"])
  ])
])
AC_SUBST([resource_FILES])

dnl ----------------------------------------------------------------------

dnl
dnl Checking for libtool support.
dnl

LT_PREREQ([1.9b])dnl (first libtool to have ‘LT_INIT’)
LT_INIT([win32-dll])

dnl ----------------------------------------------------------------------

dnl
dnl Check Guile installation.
dnl

SVZ_GUILE
SVZ_GUILE_CHECK

dnl ----------------------------------------------------------------------

dnl
dnl Check if guile servers should be included.
dnl

SVZ_FLAG([whether to include guile server support],
         [yes],[guile-server],[Include guile servers],[
AC_DEFINE([ENABLE_GUILE_SERVER], 1,
  [Define if guile servers should be supported.])
])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether to include support for named pipes under Linux.
dnl

AC_CHECK_FUNCS([mkfifo mknod sendfile])

dnl
dnl Create additional CFLAGS for MinGW32 and CygWin.
dnl

SERVEEZ_CFLAGS=""
AS_IF([SVZ_Y([CYGWIN]) || SVZ_Y([MINGW32])],
[AS_IF([SVZ_Y([enable_shared])],
  [SERVEEZ_CFLAGS="-D__SERVEEZ_IMPORT__"])])
AC_SUBST([SERVEEZ_CFLAGS])

dnl ----------------------------------------------------------------------

dnl
dnl Check whether to include support for heap counters.
dnl

SVZ_FLAG([whether to enable heap counters],
         [yes],[heap-count],[Include support for heap counters],[
AC_DEFINE([ENABLE_HEAP_COUNT], 1,
  [Define if heap counters should be enabled.])
])

dnl ----------------------------------------------------------------------

dnl
dnl Check for system header files.
dnl

AC_HEADER_STDC

dnl
dnl Checking for ‘getopt’ and ‘getopt_long’ functions and headers.
dnl

save_LIBS="$LIBS"
AC_SEARCH_LIBS([getopt],[iberty],
[SVZ_HAVE_FUNC_MAYBE_IN_LIB([getopt],[getoptlib])])
AC_SEARCH_LIBS([getopt_long],[iberty],
[SVZ_HAVE_FUNC_MAYBE_IN_LIB([getopt_long],[getopt_longlib])])
LIBS="$save_LIBS"

dnl
dnl We might declare getopt_long ourselves if the headers do not.
dnl

AS_IF([test xno != x"$ac_cv_search_getopt_long"],[
  AC_CHECK_DECLS([getopt_long], [], [], [[#include <getopt.h>]])])

AS_IF([test "$getoptlib" || test "$getopt_longlib"],
[AS_CASE([$host_os],
         [mingw*],[LIBS="$LIBS $getoptlib"],
         [SERVEEZ_LIBS="$SERVEEZ_LIBS $getoptlib"])])
AS_UNSET([getoptlib])
AS_UNSET([getopt_longlib])

dnl
dnl Check if libtool copes with SunOS/Solaris.  GNU libtool 1.4.2 and less
dnl fail to create self-contained shared libraries.  This is due to the gcc
dnl - native linker combination.  The below macro adds the -liberty path
dnl and -lgcc to the linker line if necessary.
dnl

SVZ_LIBTOOL_SOLARIS

dnl
dnl Checking for timezone and daylight variables (FreeBSD and NetBSD
dnl do not provide).
dnl

AC_CACHE_CHECK([for timezone and daylight variables],[svz_cv_var_timezone],
[AC_LINK_IFELSE(
   [AC_LANG_PROGRAM(
     [[extern long int timezone; extern int daylight;]],
     [[long int t = timezone; int d = daylight; return (d + t);]])],
   [svz_cv_var_timezone=yes],
   [svz_cv_var_timezone=no])])

AS_IF([SVZ_Y([svz_cv_var_timezone])],
[AC_DEFINE([HAVE_TIMEZONE], 1,
  [Define if you have timezone and daylight.])])

dnl
dnl ‘bzero’ is defined in <strings.h> on AIX, used by ‘FD_ZERO’
dnl There is no <wait.h> on AIX, but <sys/wait.h>.
dnl

AC_CHECK_HEADERS_ONCE([wait.h strings.h])
AC_HEADER_SYS_WAIT

AC_CHECK_FUNCS([strcasecmp],,[AC_CHECK_FUNCS([stricmp])])
AC_CHECK_FUNCS([strncasecmp],,[AC_CHECK_FUNCS([strnicmp])])

AC_CHECK_FUNCS([getrlimit getdtablesize getpwnam seteuid setegid geteuid \
  getegid gettimeofday shl_load NSAddImage])
AC_CHECK_HEADERS_ONCE([
  stdio.h unistd.h sys/time.h sys/poll.h pwd.h varargs.h
  getopt.h dlfcn.h sys/sockio.h sys/resource.h sys/sendfile.h sys/uio.h
  ws2tcpip.h dirent.h sys/dirent.h direct.h dl.h dld.h grp.h
  mach-o/dyld.h zlib.h bzlib.h rpc/rpcent.h rpc/rpc.h rpc/pmap_clnt.h
  rpc/pmap_prot.h rpc/clnt_soc.h sys/ioctl.h pthread.h floss.h
])
AC_HEADER_DIRENT

dnl
dnl Serveez core library internals (svzconfig.h).
dnl
AS_IF([SVZ_Y([ac_cv_header_varargs_h])],
[AC_DEFINE([SVZ_HAVE_VARARGS_H], 1,
  [Define if you have the <varargs.h> header file.])])
AS_IF([SVZ_Y([ac_cv_header_pthread_h])],
[AC_DEFINE([SVZ_HAVE_PTHREAD_H], 1,
  [Define if you have the <pthread.h> header file.])])

dnl
dnl Some installations need <sys/types.h> before <netinet/tcp.h>.
dnl
AC_CACHE_CHECK([for netinet/tcp.h],[svz_cv_header_netinet_tcp_h],
[AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
    #include <sys/types.h>
    #include <netinet/tcp.h>
]])],
  [svz_cv_header_netinet_tcp_h=yes],
  [svz_cv_header_netinet_tcp_h=no])])

AS_IF([SVZ_Y([svz_cv_header_netinet_tcp_h])],
[AC_DEFINE([HAVE_NETINET_TCP_H], 1,
  [Define if you have the <netinet/tcp.h> header file.])])

dnl
dnl Check if ‘alphasort’ is declared.  Also check for ‘alphasort’,
dnl ‘strsignal’ and ‘scandir’ in -liberty.
dnl
save_LIBS="$LIBS"
LIBS="$LIBS $SERVEEZ_LIBS $EXTRALIBS"
AC_CHECK_FUNCS([alphasort scandir strsignal])
LIBS="$save_LIBS"
AS_UNSET([save_LIBS])

AS_IF([SVZ_Y([ac_cv_header_dirent_h])],
[AC_CHECK_DECLS([alphasort], [], [], [[#include <dirent.h>]])])

LIBSERVEEZLIBS=""

dnl
dnl Checking for shared library loading interface.
dnl

save_LIBS="$LIBS"
AC_SEARCH_LIBS([dlopen],[dl svld],
[SVZ_HAVE_FUNC_MAYBE_IN_LIB([dlopen],[LIBSERVEEZLIBS])],
[AC_CHECK_LIB([dld],[dld_link],
  [AC_DEFINE([HAVE_DLD_LINK], 1,
    [Define if you have the dld_link function.])
 LIBSERVEEZLIBS="$LIBSERVEEZLIBS -ldld"])])
LIBS="$save_LIBS"

dnl
dnl Checking for additional codec libraries.
dnl

CODECLIBS=""

AC_CHECK_LIB([z],[zlibVersion],[
  AC_DEFINE([HAVE_ZLIB], 1, [Define if you have the zlib library.])
  CODECLIBS="$CODECLIBS -lz"
])

have_bz2=no
AC_CHECK_LIB([bz2],[bzCompress],[have_bz2=yes],
[AC_CHECK_LIB([bz2],[BZ2_bzCompress],[have_bz2=bz2_prefix])])
AS_IF([test no != $have_bz2],[
  AC_DEFINE([HAVE_BZ2LIB], 1, [Define if you have the bz2 library.])
  CODECLIBS="$CODECLIBS -lbz2"
  AS_IF([test bz2_prefix = $have_bz2],[
    AC_DEFINE([HAVE_BZ2LIB_PREFIX], 1,
      [Define if you have a prefixed bz2 library (>= v1.0)])
  ])
])

dnl
dnl Check for thread libraries.
dnl

have_threads=no
AS_IF([SVZ_Y([enable_thread_safety])],[
  AS_IF([SVZ_Y([MINGW32])],
   [have_threads=yes],
   [dnl POSIX
    AC_CHECK_LIB([pthread],[pthread_create],[
      have_threads=yes
      LIBSERVEEZLIBS="$LIBSERVEEZLIBS -lpthread"],
      dnl FreeBSD
      [AC_CHECK_LIB([c_r],[pthread_create],[
        have_threads=yes
        LIBSERVEEZLIBS="$LIBSERVEEZLIBS -lc_r"])])])])
AS_IF([SVZ_Y([have_threads])],
[AC_DEFINE([SVZ_HAVE_THREADS], 1,
  [Define if you have a thread library.])])
AS_UNSET([have_threads])

AC_SUBST([LIBSERVEEZLIBS])
AC_SUBST([CODECLIBS])

dnl ----------------------------------------------------------------------

dnl Check for 'uname' interface.

AC_CHECK_HEADERS_ONCE([sys/utsname.h])
AC_CHECK_FUNCS([uname])

dnl ----------------------------------------------------------------------

dnl Check for the 'kstat' interface on Solaris.

AC_CHECK_LIB([kstat],[kstat_open],[
  AC_DEFINE([HAVE_LIBKSTAT], 1,
    [Define if Solaris provides the kstat library.])
  EXTRALIBS="$EXTRALIBS -lkstat"
])

dnl Check for the 'pstat' interface on HP-Unix.

AC_CHECK_HEADERS_ONCE([sys/pstat.h])
AS_IF([SVZ_Y([ac_cv_header_sys_pstat_h])],
[AC_CHECK_DECL([pstat_getdynamic],
 [AC_DEFINE([HAVE_PSTAT], 1,
   [Define if HP-Unix provides the pstat interface.])],
 [], [[#include <sys/pstat.h>]])])

dnl Check for the 'sysget' interface on IRIX.

AC_CHECK_HEADERS_ONCE([sys/sysget.h])
AS_IF([SVZ_Y([ac_cv_header_sys_sysget_h])],
[AC_CHECK_DECL([sysget],
 [AC_DEFINE([HAVE_SYSGET], 1,
   [Define if IRIX provides the sysget interface.])],
 [], [[#include <sys/sysget.h>]])])

dnl Check for the '/proc/stat' file under Linux.

AC_CACHE_CHECK([whether /proc/stat exists],[svz_cv_sys_proc_stat],
[AS_IF([grep 'cpu  ' /proc/stat >/dev/null 2>&1],
       [svz_cv_sys_proc_stat=yes],
       [svz_cv_sys_proc_stat=no])])

AS_IF([SVZ_Y([svz_cv_sys_proc_stat])],
[AS_CASE([$host_os],
 [linux*],[AC_DEFINE([HAVE_PROC_STAT], 1,
            [Define if Linux supports the /proc/stat file.])],
 [AC_MSG_WARN([disabled '/proc/stat' since '$host_os' is not 'linux'])])])

dnl Check for the 'host_statistics' interface on MacOS.

AC_CHECK_FUNCS([host_statistics])

dnl ----------------------------------------------------------------------

dnl
dnl Check for existence of some useful functions.
dnl
AC_CHECK_FUNCS([vsprintf],,
[AC_MSG_ERROR([cannot compile without vsprintf])])

dnl
dnl Some Unices (e.g. Dec Unix) do not implement ‘snprintf’.
dnl FIXME: This will all be handled by gnulib, someday.
dnl
AC_CHECK_FUNCS([snprintf],,
[AC_MSG_ERROR([cannot compile without snprintf])])
AC_CHECK_FUNCS([vsnprintf],,
[AC_MSG_ERROR([cannot compile without vsnprintf])])

dnl
dnl Check whether extra net libraries are required.
dnl

SVZ_EXTRALIBS_MAYBE([socketpair],[socket])

dnl
dnl Check for Solaris nsl and resolv library.
dnl

SVZ_EXTRALIBS_MAYBE([gethostbyaddr],[nsl])
SVZ_EXTRALIBS_MAYBE([inet_aton],[resolv])

dnl
dnl Check for broken inet_ntoa ()
dnl
dnl  It is not really that ‘inet_ntoa’ is broken. Some (old) gcc versions
dnl  on IRIX handle small structs wrong.  As of gcc 2.95 this should be
dnl  fixed but we do not really know.  To work around we implement ‘inet_ntoa’
dnl  on our own if it does not work.
dnl
AC_CACHE_CHECK([whether inet_ntoa is broken],[svz_cv_func_inet_ntoa_broken],
[AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <stdio.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    main () {
	struct in_addr ip;
	ip.s_addr = htonl (0x12345678);
	if (strcmp (inet_ntoa (ip), "18.52.86.120"))
	    exit (0);
	exit (1);}
]])],
  [svz_cv_func_inet_ntoa_broken=yes],
  [svz_cv_func_inet_ntoa_broken=no],
  [svz_cv_func_inet_ntoa_broken=no])])

AS_IF([SVZ_Y([svz_cv_func_inet_ntoa_broken])],
[AC_DEFINE([BROKEN_INET_NTOA], 1,
  [Define if inet_ntoa function is broken.])])

dnl
dnl Check for typedef socklen_t in <sys/socket.h>.
dnl
AC_CHECK_TYPE([socklen_t], [],
 [AC_DEFINE([socklen_t], int,
   [Define to 'int' if <sys/socket.h> does not define.])],
 [[#include <sys/socket.h>]])

dnl
dnl Check for the typedef SOCKET and HANDLE in <winsock2.h>.
dnl
socket_defined="int"
handle_defined="int"

AS_IF([test no != "$MINGDIR"],
[AC_CHECK_HEADER([winsock2.h],[
  socket_defined="SOCKET"
  handle_defined="HANDLE"
  AC_DEFINE([Win32_Winsock], 1,
   [Define if you are using Windows Socket-API (not CYGWIN).])
  AC_DEFINE([HAVE_WINSOCK2_H], 1,
   [Define if you have the <winsock2.h>])])])

AC_DEFINE_UNQUOTED([svz_t_socket],[$socket_defined],
  [Define to 'int' if <winsock2.h> does not define 'SOCKET'.])

AC_DEFINE_UNQUOTED([svz_t_handle],[$handle_defined],
  [Define to 'int' if <winsock2.h> does not define 'HANDLE'.])

AS_UNSET([socket_defined])
AS_UNSET([handle_defined])

AC_CHECK_HEADERS([mswsock.h], [], [],[
#if HAVE_WINSOCK2_H
# include <winsock2.h>
#endif
])

AC_TYPE_SIZE_T
AC_TYPE_OFF_T
AC_TYPE_MODE_T

dnl
dnl Checking for ‘crypt’ function.
dnl

MKPASSWORD=""
AS_IF([SVZ_Y([enable_crypt])],[
  save_LIBS="$LIBS"
  AC_SEARCH_LIBS([crypt],[descrypt crypt],
   [SVZ_HAVE_FUNC_MAYBE_IN_LIB([crypt],[EXTRALIBS],[SVZ_])
    MKPASSWORD=mkpassword])
  LIBS="$save_LIBS"
])
AC_SUBST([MKPASSWORD])

dnl
dnl Check for ‘hstrerror’, ‘h_errno’ and ‘strsignal’ functions.
dnl

AC_CHECK_DECLS([strsignal], [], [], [[
    #define _GNU_SOURCE
    #define __EXTENSIONS__
    #include <string.h>
]])

AC_CHECK_FUNCS([hstrerror],,
[AC_CHECK_LIB([resolv],[hstrerror],
 [AC_DEFINE([HAVE_HSTRERROR], 1,
   [Define if you have the hstrerror function.])])])

AS_IF([SVZ_Y([ac_cv_func_hstrerror]) || SVZ_Y([ac_cv_lib_resolv_hstrerror])],
[AC_CHECK_DECLS([hstrerror, h_errno], [], [], [[#include <netdb.h>]])])

dnl
dnl Check for more functionality used in Serveez.
dnl
AC_CHECK_FUNCS([times poll waitpid])

dnl
dnl Checks for RPC services.
dnl
SVZ_EXTRALIBS_MAYBE([getrpcent],[nsl])dnl libnsl for HP-UX.
save_LIBS="$LIBS"
LIBS="$LIBS $EXTRALIBS"
AC_CHECK_FUNCS([getrpcbyname getrpcbynumber setrpcent endrpcent \
  pmap_set pmap_unset pmap_getmaps get_myaddress])
LIBS="$save_LIBS"
AS_UNSET([save_LIBS])

AC_CHECK_HEADERS_ONCE([netdb.h])
AC_CHECK_DECLS([setrpcent, endrpcent], [], [], [[
     #ifdef HAVE_NETDB_H
     # include <netdb.h>
     #endif
     #ifdef HAVE_RPC_RPCENT_H
     # include <rpc/rpcent.h>
     #endif
]])

dnl
dnl Find a working signed keyword.
dnl
AC_CACHE_CHECK([for working signed],[svz_cv_c_signed],
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <stdio.h>]],
    [[signed char have_signed = (signed char) 0;]])],
  [svz_cv_c_signed=yes
   signed_defined="signed"],
  [svz_cv_c_signed=no
   signed_defined=""])])

AC_DEFINE_UNQUOTED([svz_c_signed],[$signed_defined],
  [Define to empty if the keyword does not work.])

AS_UNSET([signed_defined])

dnl
dnl Checking for RPM builder version
dnl
RPM=":"
AS_IF([SVZ_Y([USE_MAINTAINER_MODE])],[
AC_MSG_CHECKING([for working rpm])
[version=`eval rpm --version 2>/dev/null | sed 's/[^0-9\.]//g'`]
AS_IF([test x = x"$version"],[AC_MSG_RESULT([missing])],
[AS_CASE([$version],
 [[3.* | [4-9].*]],[
  AC_MSG_RESULT([$version >= 3.0.0])
  RPM="rpm"
  RPM3="yes"],
 [AC_MSG_RESULT([$version < 3.0.0])
  RPM="rpm"
  RPM3="no"])])
AS_UNSET([version])
])
AC_SUBST([RPM])
AC_SUBST([RPM3])

LIBS="$LIBS $EXTRALIBS"
CPPFLAGS=$CPPEXTRAFLAGS

TIMESTAMP=`date '+%a %b %_d %H:%M:%S %Y' 2>/dev/null`
AS_IF([test x = x"$TIMSTAMP"],[
    dnl On HP-UX each item is padded with spaces by default.
    TIMESTAMP=`date '+%a %b %d %H:%M:%S %Y' 2>/dev/null`
])
AC_SUBST([TIMESTAMP])

dnl For GNU/Linux Debian packaging only
TIMESTAMP_RFC822=`date -R 2>/dev/null`
AC_SUBST([TIMESTAMP_RFC822])

REDHAT_TOPDIR=/opt
AC_SUBST([REDHAT_TOPDIR])

dnl
dnl Setup configuration variables.
dnl
SERVEEZ_CONFIG_CFLAGS="-I$prefix/include"
AS_IF([test x != "x$SERVEEZ_CFLAGS"],
[SERVEEZ_CONFIG_CFLAGS="$SERVEEZ_CONFIG_CFLAGS $SERVEEZ_CFLAGS"])
AS_IF([SVZ_Y([enable_shared])],
[SERVEEZ_CONFIG_LDFLAGS="-L$prefix/lib -lserveez"],
[SERVEEZ_CONFIG_LDFLAGS="$prefix/lib/libserveez.a"])
AC_SUBST([SERVEEZ_CONFIG_CFLAGS])
AC_SUBST([SERVEEZ_CONFIG_LDFLAGS])

dnl
dnl Output files.
dnl
AC_CONFIG_FILES([
    Makefile src/Makefile src/libserveez/Makefile
    src/version.h
    src/libserveez/coserver/Makefile
    src/libserveez/codec/Makefile
    src/serveez-config
    src/awcs-server/Makefile
    src/ctrl-server/Makefile
    src/http-server/Makefile
    src/irc-core/Makefile
    src/irc-server/Makefile
    src/foo-server/Makefile
    src/sntp-server/Makefile
    src/nut-server/Makefile
    src/tunnel-server/Makefile
    src/fakeident-server/Makefile
    src/prog-server/Makefile
    src/guile-server/Makefile
    doc/Makefile
    data/Makefile
    test/Makefile
    build-aux/redhat.sh
    build-aux/debian.sh
])

dnl
dnl Print results.
dnl
AC_MSG_RESULT([])
AC_MSG_RESULT([	$PACKAGE version $VERSION configured successfully.])
AC_MSG_RESULT([])

AC_OUTPUT

dnl configure.ac ends here
