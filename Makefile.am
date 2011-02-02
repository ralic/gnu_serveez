## Process this file with automake to produce Makefile.in
#
# Makefile.am
#
# Automake input file.
#
# Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
# Copyright (C) 2000, 2001, 2002 Stefan Jahn <stefan@lkcc.org>
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
#

SUBDIRS = src data test doc

EXTRA_DIST = BUGS autogen.sh README-alpha THANKS README-Win32 README-cross \
	serveez.dsw config.h.serveez version.h.serveez \
	vcgen.bat build-aux/autodsp serveez.ap

redhat: @MAINTAINER_MODE_TRUE@ dist
	@MAINTAINER_MODE_TRUE@ @sh build-aux/redhat.sh && echo "Redhat packages done."

debian: @MAINTAINER_MODE_TRUE@ dist
	@MAINTAINER_MODE_TRUE@ @sh build-aux/debian.sh && echo "Debian packages done."

CLEANFILES = *~ *.rpm *.deb *.changes build-stamp
MAINTAINERCLEANFILES = aclocal.m4 config.h.in configure Makefile.in \
	stamp-h.in stamp-h[0-9].in serveez.dsw
DISTCLEANFILES = config.cache config.log
