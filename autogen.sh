#! /bin/sh
# autogen.sh
#
# Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
# Copyright (C) 2001, 2002, 2003 Stefan Jahn <stefan@lkcc.org>
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this package.  If not, see <http://www.gnu.org/licenses/>.

#-----------------------------------------------------------------------
# Usage: ./autogen.sh [--woe32]
#
# Optional arg ‘--woe32’ means also create .dsw and .dsp files.
# (Normally, they are created on "make dist" only.)
#
# Prerequisite tools:
# - GNU Autoconf 2.62
# - GNU Libtool 1.5
# - GNU Automake 1.10
# - GNU Texinfo 4.11
# These are minimum versions; later versions are probably ok.
for tool in autoconf libtool automake makeinfo ; do
    echo using: $($tool --version | sed 1q)
done

here=`pwd`
cd `dirname $0`

# let automake find this automatic created file
if ! test -f doc/serveez-api.texi; then
cat <<EOF > doc/serveez-api.texi
@setfilename serveez-api.info
@include version1.texi
EOF
info_touched="yes"
fi

# Invoke all the auto* tools.
autoreconf --verbose --force --install --symlink --warnings=all

# patching libtool 1.5 code for MinGW32 build
echo -n "Patching configure... "
mv -f configure configure.x
cat configure.x | sed 's/x86 DLL/x86 DLL|\^x86 archive static/' > configure
chmod +x configure
rm -f configure.x
echo "done."

# reschedule this file for building
if test x"$info_touched" = xyes ; then rm -f doc/serveez-api.texi; fi

# Life is usually full enough of woe, but one can always opt for more.
test x"$1" = x--woe32 \
    && make -f Makefile.am woe32-project-files srcdir=.

# We used to run configure here, but that's not really part of
# the bootstrap proper.  However, a nice reminder hurts no one.
echo 'You can run ./configure now (use option --help for more info).'
