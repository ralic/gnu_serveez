#! /bin/sh
#
# autogen.sh
#
# Run this script to re-generate all maintainer-generated files.
#
# Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this package; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.  
#

here=`pwd`
cd `dirname $0`

echo -n "Creating aclocal.m4... "
aclocal
echo "done."
echo -n "Creating config.h.in... "
autoheader
echo "done."
echo -n "Creating Makefile.in(s)... "
automake
echo "done."
echo -n "Creating configure... "
autoconf
echo "done."

#
# run configure, maybe with parameters recorded in config.status
#
if [ -r config.status ]; then
  CMD=`awk '/^#.*\/?configure .*/ { $1 = ""; print; exit }' < config.status`
else
  CMD=`dirname $0`/configure --enable-maintainer-mode
fi
echo "Running $CMD $@ ..."
$CMD "$@"
