dnl
dnl Autoconf macros for configuring the Serveez package.
dnl
dnl AC_SERVEEZ([USEFLAG]) -- Locate the Serveez core library.
dnl When USEFLAG is given (optional argument) the variables SERVEEZ_CFLAGS 
dnl and SERVEEZ_LDFLAGS are set. Otherwise the macro adds these flags to 
dnl the overall linker and compiler flags produced by the ./configure script.
dnl

AC_DEFUN([AC_SERVEEZ], [
  AC_ARG_WITH(serveez,
    [  --with-serveez=DIR      serveez installation in DIR [/usr/local]],
    [case "$withval" in
     no)  SVZDIR="no" ;;
     yes) SVZDIR="/usr/local" ;;
     *)   SVZDIR="$withval" ;;
    esac],
    SVZDIR="/usr/local")

  AC_MSG_CHECKING([for serveez installation])
  if test "x$SVZDIR" != "xno"; then
    SVZDIR=`eval cd "$SVZDIR" 2>/dev/null && pwd`
    case $host_os in
    mingw*) SVZDIR=`eval cygpath -w -i "$SVZDIR"` ;;
    esac
    if test -f "$SVZDIR/lib/libserveez.so" -o \
	    -f "$SVZDIR/bin/libserveez.dll"; then
      if test "x$1" = "x"; then 
        CFLAGS="$CFLAGS -I$SVZDIR/include"
        LDFLAGS="$LDFLAGS -L$SVZDIR/lib"
        LIBS="$LIBS -lserveez"
      else
        SERVEEZ_CFLAGS="-I$SVZDIR/include"
        SERVEEZ_LDFLAGS="-L$SVZDIR/lib -lserveez"
        AC_SUBST(SERVEEZ_CFLAGS)
        AC_SUBST(SERVEEZ_LDFLAGS)
      fi
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([none])
    fi
  else
    AC_MSG_RESULT([disabled])
  fi

  unset SVZDIR
])
