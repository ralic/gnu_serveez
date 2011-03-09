#! /bin/sh
# autogen.sh
#
# Copyright (C) 2011 Thien-Thi Nguyen
# Copyright (C) 2001, 2002, 2003 Stefan Jahn <stefan@lkcc.org>
# Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
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
# - GNU Autoconf 2.64
# - GNU Libtool 2.4
# - GNU Automake 1.10
# - GNU Texinfo 4.11
# - Guile-BAUX 20110309.1744.ad9085f
# These are minimum versions; later versions are probably ok.
for tool in autoconf libtool automake makeinfo guile-baux-tool ; do
    echo using: $($tool --version | sed 1q)
done

cd `dirname $0`

# Make some Guile-BAUX functionality available.
guile-baux-tool import \
    gbaux-do

# Invoke all the auto* tools.
autoreconf --verbose --force --install --symlink --warnings=all

# Life is usually full enough of woe, but one can always opt for more.
test x"$1" = x--woe32 \
    && make -f Makefile.am woe32-project-files srcdir=.

# We used to run configure here, but that's not really part of
# the bootstrap proper.  However, a nice reminder hurts no one.
echo 'You can run ./configure now (use option --help for more info).'
