dnl acinclude.m4 --- Autoconf macros for configuring the Serveez package.
dnl
dnl Copyright (C) 2001, 2002, 2003 Stefan Jahn <stefan@lkcc.org>
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

dnl ----------------------------------------------------------------------
dnl Common bits.

dnl SVZ_HELP_STRING(LHS,DEFAULT,BLURB)
dnl
dnl Wrap ‘AS_HELP_STRING’, expanding the right-hand-side as:
dnl   BLURB [default=DEFAULT]
dnl
AC_DEFUN([SVZ_HELP_STRING],
[AS_HELP_STRING([$1],[$3 @<:@default=$2@:>@])])dnl

dnl SVZ_FLAG(MSG,DEFAULT,NICK,BLURB[,IF-YES[,IF-NO]])
dnl
dnl Say "checking MSG"; then check for --{en,dis}able-NICK;
dnl assigning var enable_NICK the value ‘yes’ or ‘no’, or DEFAULT
dnl if not specified on the configure script command-line.  BLURB
dnl is for --help output (see ‘SVZ_HELP_STRING’).  Report the
dnl result.  If specified, do IF-YES or IF-NO for values of ‘yes’
dnl and ‘no’, respectively.
dnl
AC_DEFUN([SVZ_FLAG],[dnl
AS_VAR_PUSHDEF([VAR],[enable_$3])dnl
AC_MSG_CHECKING([$1])
AC_ARG_ENABLE([$3],
  [SVZ_HELP_STRING([--]m4_case([$2],[yes],[dis],[en])[able-$3],
                   [$2],[$4])],
  [test yes = "$VAR" || VAR=no],
  [VAR=$2])
AC_MSG_RESULT([$VAR])
m4_ifnblank([$5$6],[AS_IF([test yes = "$VAR"],[$5],[$6])])
AS_VAR_POPDEF([VAR])dnl
])dnl

dnl SVZ_WITH(DEFAULT,NICK,ARGS,BLURB,IF-NO,IF-YES,OTHERWISE)
dnl
dnl Wrap ‘AC_ARG_WITH’, setting the shell variable with_NICK to
dnl IF-NO, IF-YES, or OTHERWISE (if specified on the configure
dnl script command-line); or DEFAULT (if unspecified).  ARGS and
dnl BLURB are for --help output (see ‘SVZ_HELP_STRING’).
dnl
AC_DEFUN([SVZ_WITH],[dnl
AS_VAR_PUSHDEF([VAR],[with_$2])dnl
AC_ARG_WITH([$2],[SVZ_HELP_STRING([--with-$2]m4_ifnblank([$3],[=$3]),
                                  [$1],[$4])],
  [AS_CASE(["$VAR"],
           [no],[VAR=$5],
           [yes],[VAR=$6],
           [VAR=$7])],
  [VAR=$1])
AS_VAR_POPDEF([VAR])dnl
])dnl

dnl ----------------------------------------------------------------------
dnl
dnl SVZ_GUILE -- Locate a Guile installation.
dnl This macro sets both the variables GUILE_CFLAGS and GUILE_LDFLAGS to be
dnl passed to the compiler and linker. In a first try it uses the
dnl `guile-config' script in order to obtain these settings. Then it proceeds
dnl the `--with-guile=DIR' option of the ./configure script.
dnl
dnl SVZ_GUILE_SOURCE -- Check a source tree of Guile.
dnl Check if the user has given the `--with-guile-source=DIR' argument to
dnl ./configure script. If so, it overrides the above SVZ_GUILE macro and
dnl provides the following Makefile variables:
dnl   GUILE_SOURCE - the source directory
dnl   GUILE_CFLAGS - flags passed to the compiler
dnl   GUILE_LDFLAGS - flags passed to the linker
dnl   GUILE_DEPENDENCY - a dependency for the guile library
dnl   GUILE_RULE - name of the rule to make the guile library
dnl   GUILE_MAKE_LTDL - make command line for the above rule
dnl   GUILE_MAKE_LIB - yet another make line for that rule
dnl Please have a look at the `src/Makefile.am' file for more details how
dnl these variables are actually used to build a static guile library linked
dnl to the main binary.
dnl
dnl SVZ_GUILE_CHECK -- Checks for Guile results and exits if necessary.
dnl
dnl SVZ_LIBTOOL_SOLARIS -- Helps libtool to build on Solaris.
dnl

AC_DEFUN([SVZ_GUILE], [
  SVZ_WITH([/usr/local],[guile],[DIR],[Guile installation in DIR],
           [no],[/usr/local],["$withval"])
  GUILEDIR="$with_guile"

  AC_MSG_CHECKING([for guile installation])
  if test "x$GUILEDIR" != "xno" ; then
    GUILEDIR="`eval cd "$GUILEDIR" 2>/dev/null && pwd`"
    case $build_os in
    mingw*)
	GUILEDIR="`eval cygpath -w -i "$GUILEDIR"`"
	GUILEDIR="`echo "$GUILEDIR" | sed -e 's%\\\\%/%g'`"
	;;
    esac
    if test -f "$GUILEDIR/lib/libguile.so" -o \
      -n "`find "$GUILEDIR/lib" -name "libguile.so.*" 2>/dev/null`" -o \
      -f "$GUILEDIR/lib/libguile.dylib" -o \
      -f "$GUILEDIR/bin/libguile.dll" -o \
      -f "$GUILEDIR/bin/cygguile.dll" ; then
      GUILE_CFLAGS="-I$GUILEDIR/include"
      if test x"$CYGWIN" = xyes -o x"$MINGW32" = xyes ; then
	GUILE_CFLAGS="-D__GUILE_IMPORT__ $GUILE_CFLAGS"
      fi
      GUILE_LDFLAGS="-L$GUILEDIR/lib -lguile"
      GUILE_BUILD="yes"
      AC_MSG_RESULT([yes])
    fi
  fi

  if test "x$GUILE_BUILD" != "xyes" ; then
    guile=""
    if test "x`eval guile-config --version 2>&1 | grep version`" != "x" ; then
      guile="`eval guile-config --version 2>&1 | grep version`"
      [guile="`echo $guile | sed -e 's/[^0-9\.]//g'`"]
    fi
    if test "x$guile" != "x" ; then
      case "$guile" in
      [1.3 | 1.3.[2-9] | 1.[4-9]* | [2-9].*)]
	AC_MSG_RESULT([$guile >= 1.3])
	GUILE_BUILD="yes"
	;;
      [*)]
	AC_MSG_RESULT([$guile < 1.3])
	AC_MSG_WARN([
  GNU Guile version 1.3 or above is needed, and you do not seem to have
  it handy on your system.])
	;;
      esac
      GUILE_CFLAGS="`eval guile-config compile`"
      GUILE_LDFLAGS="`eval guile-config link`"
    else
      AC_MSG_RESULT([missing])
      GUILE_CFLAGS=""
      GUILE_LDFLAGS=""
    fi
    AS_UNSET([guile])
  fi
  AS_UNSET([GUILEDIR])
  AC_SUBST([GUILE_CFLAGS])
  AC_SUBST([GUILE_LDFLAGS])
])

AC_DEFUN([SVZ_GUILE_SOURCE], [
  SVZ_WITH([/usr/src],[guile-source],[DIR],[Guile source tree in DIR],
           [no],[/usr/src],["$withval"])
  GUILESRC="$with_guile_source"

  AC_MSG_CHECKING([for guile source tree])
  if test "x$GUILESRC" != "xno"; then
    GUILESRC="`eval cd "$GUILESRC" 2>/dev/null && pwd`"
    case $build_os in
    mingw*)
	GUILESRC="`eval cygpath -w -i "$GUILESRC"`"
	GUILESRC="`echo "$GUILESRC" | sed -e 's%\\\\%/%g'`"
	;;
    esac
    if test -f "$GUILESRC/configure" &&
       test -f "$GUILESRC/config.status" ; then
      GUILE_SOURCE="$GUILESRC"
      GUILE_CFLAGS="-I$GUILESRC -I$GUILESRC/libguile -DGUILE_SOURCE"
      GUILE_LDFLAGS="$GUILESRC/libguile/libguile.la"
      GUILE_DEPENDENCY="$GUILESRC/libguile/libguile.la"
      GUILE_RULE="$GUILESRC/libguile/libguile.la"
      GUILE_MAKE_LTDL='(cd $(GUILE_SOURCE)/libltdl && $(MAKE) libltdlc.la)'
      if test ! -f "$GUILESRC/libltdl/configure" ; then
	GUILE_MAKE_LTDL="# $GUILE_MAKE_LTDL"
      fi
      GUILE_MAKE_LIB='(cd $(GUILE_SOURCE)/libguile && $(MAKE) libguile.la)'
      AC_SUBST([GUILE_SOURCE])
      AC_SUBST([GUILE_CFLAGS])
      AC_SUBST([GUILE_LDFLAGS])
      AC_SUBST([GUILE_DEPENDENCY])
      AC_SUBST([GUILE_RULE])
      AC_SUBST([GUILE_MAKE_LTDL])
      AC_SUBST([GUILE_MAKE_LIB])
      AC_MSG_RESULT([yes])
      GUILE_BUILD="yes"
    else
      AC_MSG_RESULT([missing])
      GUILE_SOURCE="no"
    fi
  else
    AC_MSG_RESULT([disabled])
    GUILE_SOURCE="no"
  fi
  AS_UNSET([GUILESRC])
])

AC_DEFUN([SVZ_GUILE_CHECK], [
  if test "x$GUILE_BUILD" != "xyes" ; then
    AC_MSG_ERROR([
  The $PACKAGE $VERSION package requires either an installed Guile
  version or an unpacked source tarball at hand.  You can specify the
  install location by passing `--with-guile=<directory>' or the source
  location by passing `--with-guile-source=<directory>'.  Guile
  version 1.4 is preferred.])
  fi
])

AC_DEFUN([SVZ_LIBTOOL_SOLARIS], [
  if test "x$GCC" = "xyes" -a "x$enable_shared" = "xyes" ; then
    case $host_os in
    solaris*)
      LIBERTY="`gcc --print-file-name=libiberty.a`"
      LIBERTY="-L`dirname $LIBERTY 2>/dev/null`"
      SERVEEZ_LDFLAGS="$SERVEEZ_LDFLAGS $LIBERTY"
      GCCLIB="`gcc --print-libgcc-file-name`"
      GCCDIR="-L`dirname $GCCLIB 2>/dev/null`"
      GCCFILE="`basename $GCCLIB 2>/dev/null`"
      GCCFILE="-l`echo "$GCCFILE" | sed -e 's/lib\(.*\)\.a/\1/'`"
      SERVEEZ_LDFLAGS="$SERVEEZ_LDFLAGS $GCCDIR"
      SERVEEZ_LIBS="$SERVEEZ_LIBS $GCCFILE"
      AC_MSG_WARN([
  The configure script added
  '$LIBERTY $GCCDIR $GCCFILE'
  to your linker line.  This may not be what you want.  Please report
  to <bug-serveez@gnu.org> if we failed to build shared libraries
  for '$host_os'.])
      AS_UNSET([LIBERTY])
      AS_UNSET([GCCLIB])
      AS_UNSET([GCCDIR])
      AS_UNSET([GCCFILE])
      ;;
    esac
  fi
])
