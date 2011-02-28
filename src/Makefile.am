## Process this file with automake to produce Makefile.in
#
# src/Makefile.am
#
# Automake input file.
#
# Copyright (C) 2011 Thien-Thi Nguyen
# Copyright (C) 2000, 2001, 2002 Stefan Jahn <stefan@lkcc.org>
# Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
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

include $(top_srcdir)/build-aux/common.mk

#
# Installation of the autoconf macro used to locate Serveez
#
aclocaldir = $(datadir)/aclocal
aclocal_DATA = serveez.m4

# Yet another script
bin_SCRIPTS = serveez-config
pkgdata_DATA = serveez.scm

# Perplexingly, Automake does not automatically distribute these.
EXTRA_DIST = $(aclocal_DATA) $(pkgdata_DATA)

#
# Add your server subdirectory here
#
SUBDIRS = libserveez foo-server guile-server $(SERVERDIRS)

#
# Add your library here
#
PROTOLIBS = foo-server/libfoo.a $(SERVERLIBS)

bin_PROGRAMS = serveez

include_HEADERS = libserveez.h

serveez_SOURCES = \
	cpp-tricks.h le-u32-hash.h timidity.h \
	gi.c gi.h \
	serveez.c serveez.h \
	guile.c guile.h \
	cfgfile.c cfgfile.h \
	option.c option.h \
	guile-server.c guile-server.h \
	guile-bin.c guile-bin.h

EXTRA_serveez_SOURCES = guile-api.c guile-api.h

serveez_DEPENDENCIES = $(PROTOLIBS) \
	libserveez/libserveez.la serveez.cfg

serveez_LDADD = $(resource_OBJECTS) $(PROTOLIBS) \
	libserveez/libserveez.la $(SERVEEZ_LIBS)

serveez_LDFLAGS = $(GUILE_LDFLAGS) $(SERVEEZ_LDFLAGS)
AM_CPPFLAGS += $(GUILE_CFLAGS)

CLEANFILES += serveez.cfg

i = "$(DESTDIR)$(includedir)/libserveez"

install-data-hook:
	$(SED) '/^#define [^_]/!d' svzconfig.h > $(i)/TMP0
	cd $(i); mv -f defines.h TMP1; \
	$(SED) -e '/#include "svzconfig.h"/{' \
	       -e ' s|.*|/* Extracted configuration.  */|' \
	       -e ' r TMP0' \
	       -e '}' TMP1 > defines.h; \
	rm -f TMP0 TMP1

dist-hook:
	rm -f $(distdir)/svzconfig.h

serveez.cfg: $(top_srcdir)/data/serveez.cfg
	$(LN_S) $< $@

# woe

EXTRA_DIST += svzconfig.h.serveez config.h.serveez version.h.serveez

resource_OBJECTS = $(resource_FILES:.rc=.ro)
serveez_DEPENDENCIES += $(resource_OBJECTS)
# We don't do this to avoid disturbing the linker command-line order (above).
# serveez_LDADD += $(resource_OBJECTS)

SUFFIXES = .rc .ro

.rc.ro:
	$(WINDRES) -i $< -o $@

svzconfig.h.serveez: svzconfig.h
	cp $< $@

config.h.serveez: config.h
	cp $< $@

version.h.serveez: version.h
	cp $< $@

stems = $(bin_PROGRAMS)
woeap = $(stems:%=%.ap)
woedsp = $(stems:%=%.dsp)

EXTRA_DIST += serveez.rc $(woeap) $(woedsp)
CLEANFILES += *.ro
MAINTAINERCLEANFILES += $(woedsp)
