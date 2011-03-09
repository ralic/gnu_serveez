dnl inc.m4 --- some -*-autoconf-*- macros for configuring GNU Serveez
dnl
dnl Copyright (C) 2011 Thien-Thi Nguyen
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

dnl SVZ_Y(VAR)
dnl
dnl Expand to:
dnl  test xyes = x"$VAR"
dnl
AC_DEFUN([SVZ_Y],[test xyes = x"$]$1["])

dnl SVZ_NOT_Y(VAR)
dnl
dnl Expand to:
dnl  test xyes != x"$VAR"
dnl
AC_DEFUN([SVZ_NOT_Y],[test xyes != x"$]$1["])

dnl SVZ_HAVE_FUNC_MAYBE_IN_LIB(FUNC,VAR)
dnl
dnl First, do ‘AC_DEFINE’ on FUNC.  Next, if shell variable
dnl ac_cv_search_FUNC does not have value "none required", then
dnl append it to VAR.
dnl
AC_DEFUN([SVZ_HAVE_FUNC_MAYBE_IN_LIB],[
AC_DEFINE([HAVE_]m4_toupper([$1]), 1,
  [Define to 1 if you have the $1 function.])dnl
AS_VAR_PUSHDEF([VAR],[ac_cv_search_$1])dnl
test 'xnone required' = x"$VAR" || $2="[$]$2 $VAR"
AS_VAR_POPDEF([VAR])dnl
])

dnl SVZ_EXTRALIBS_MAYBE(FUNC,LIBRARIES)
dnl
dnl Save value of shell variable ‘LIBS’; do ‘AC_SEARCH_LIBS’ on FUNC
dnl and LIBRARIES, and (if successful) ‘SVZ_HAVE_FUNC_MAYBE_IN_LIB’
dnl on FUNC and shell variable ‘EXTRALIBS’; restore ‘LIBS’ afterwards.
dnl
AC_DEFUN([SVZ_EXTRALIBS_MAYBE],[
save_LIBS="$LIBS"
AC_SEARCH_LIBS([$1],[$2],
[SVZ_HAVE_FUNC_MAYBE_IN_LIB([$1],[EXTRALIBS])])
LIBS="$save_LIBS"
])

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
  [SVZ_Y(VAR) || VAR=no],
  [VAR=$2])
AC_MSG_RESULT([$VAR])
m4_ifnblank([$5$6],[AS_IF([SVZ_Y(VAR)],[$5],[$6])])
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
dnl SVZ_GUILE_CHECK -- Checks for Guile results and exits if necessary.
dnl

AC_DEFUN([SVZ_GUILE], [
  SVZ_WITH([/usr/local],[guile],[DIR],[Guile installation in DIR],
           [no],[/usr/local],["$withval"])
  GUILEDIR="$with_guile"

  AC_MSG_CHECKING([for guile installation])
  AS_IF([test xno != x"$GUILEDIR"],[
    GUILEDIR=`eval cd "$GUILEDIR" 2>/dev/null && pwd`
    AS_CASE([$build_os],[mingw*],[
      GUILEDIR=`eval cygpath -w -i "$GUILEDIR"`
      GUILEDIR=`echo "$GUILEDIR" | sed -e 's%\\\\%/%g'`
    ])
    AS_IF([test -f "$GUILEDIR/lib/libguile.so" \
        || test -n `find "$GUILEDIR/lib" -name 'libguile.so.*' 2>/dev/null` \
        || test -f "$GUILEDIR/lib/libguile.dylib" \
        || test -f "$GUILEDIR/bin/libguile.dll" \
        || test -f "$GUILEDIR/bin/cygguile.dll"],[
      GUILE_CFLAGS="-I$GUILEDIR/include"
      AS_IF([SVZ_Y([CYGWIN]) || SVZ_Y([MINGW32])],
            [GUILE_CFLAGS="-D__GUILE_IMPORT__ $GUILE_CFLAGS"])
      GUILE_LDFLAGS="-L$GUILEDIR/lib -lguile"
      GUILE_BUILD="yes"
      AC_MSG_RESULT([yes])
    ])
  ])

  AS_IF([SVZ_NOT_Y([GUILE_BUILD])],[
    guile=""
    AS_IF([test x != x`guile-config --version 2>&1 | grep version`],[
      guile=`guile-config --version 2>&1 | grep version`
      [guile=`echo $guile | sed -e 's/[^0-9\.]//g'`]
    ])
    AS_IF([test x != x"$guile"],[
      AS_CASE([$guile],
       [[1.3 | 1.3.[2-9] | 1.[4-9]* | [2-9].*]],
       [AC_MSG_RESULT([$guile >= 1.3])
        GUILE_BUILD="yes"],
       [AC_MSG_RESULT([$guile < 1.3])
        AC_MSG_WARN([GNU Guile version 1.3 or above is needed, and you
                     do not seem to have it handy on your system.])])
      GUILE_CFLAGS=`guile-config compile`
      GUILE_LDFLAGS=`guile-config link`
      GUILEDIR=`guile-config info prefix`
    ],[
      AC_MSG_RESULT([missing])
      GUILE_CFLAGS=""
      GUILE_LDFLAGS=""
    ])
    AS_UNSET([guile])
  ])
  AS_IF([test -f "$GUILEDIR/include/guile/gh.h"],
   [AC_DEFINE([HAVE_GUILE_GH_H], 1,
     [Define to 1 if your Guile installation includes <guile/gh.h>.])])
  AS_UNSET([GUILEDIR])
  AC_SUBST([GUILE_CFLAGS])
  AC_SUBST([GUILE_LDFLAGS])
])

AC_DEFUN([SVZ_GUILE_CHECK], [
  AS_IF([SVZ_NOT_Y([GUILE_BUILD])],[
    AC_MSG_ERROR([
  $PACKAGE_STRING requires an installed Guile. You can specify the
  install location by passing `--with-guile=<directory>'.  Guile
  version 1.4 is preferred.])])])

dnl inc.m4 ends here
