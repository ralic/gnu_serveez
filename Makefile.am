## Process this file with automake to produce Makefile.in
#
# Makefile.am
#
# Automake input file.
#
# Copyright (C) 2011 Thien-Thi Nguyen
# Copyright (C) 2000, 2001, 2002 Stefan Jahn <stefan@lkcc.org>
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
#

ACLOCAL_AMFLAGS = -I m4

include $(top_srcdir)/build-aux/common.mk

SUBDIRS = src data test doc

EXTRA_DIST = BUGS autogen.sh README-alpha README-cross

CLEANFILES += *.rpm *.deb *.changes build-stamp
MAINTAINERCLEANFILES += aclocal.m4 config.h.in configure \
	stamp-h.in stamp-h[0-9].in
DISTCLEANFILES = config.cache config.log

uninstall-hook:
	 rmdir "$(DESTDIR)$(pkgdatadir)"

# woe

EXTRA_DIST += README-Win32 serveez.dsw serveez.ap \
	build-aux/vcgen.bat build-aux/autodsp

MAINTAINERCLEANFILES += serveez.dsw

woe32-project-files:
	@echo Creating woe32 project files...
	cd $(srcdir) ; perl build-aux/autodsp

# This rule triggers creation of .dsw and .dsp files on "make dist".
serveez.dsw: woe32-project-files

no-woe32:
	cd $(srcdir) ; find . -name '*.ds[wp]' -delete
	find . -name '*.serveez' -delete
