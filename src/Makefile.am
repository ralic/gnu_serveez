## Process this file with automake to produce Makefile.in
# 
# src/Makefile.am
#
# Automake input file.
#
# Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
# Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
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

EXTRA_DIST = serveez.rc serveez.dsp svzconfig.h.in svzconfig.h.serveez \
	serveez.m4 serveez.scm echo-server.scm

#
# Installation of the autoconf macro used to locate Serveez
#
aclocaldir = $(datadir)/aclocal
aclocal_DATA = serveez.m4

# Yet another script
bin_SCRIPTS = serveez.scm echo-server.scm

#
# Add your server subdirectory here
#
SUBDIRS = libserveez foo-server $(SERVERDIRS)

#
# Add your library here
#
PROTOLIBS = foo-server/libfoo.a $(SERVERLIBS)

bin_PROGRAMS = serveez mkpassword

include_HEADERS = libserveez.h svzconfig.h

serveez_SOURCES = \
	serveez.c serveez.h \
	guile.c guile.h \
	cfgfile.c cfgfile.h \
	option.c option.h \
	guile-server.c guile-server.h

resource_OBJECTS = $(resource_FILES:.rc=.ro)

serveez_DEPENDENCIES = $(resource_OBJECTS) $(PROTOLIBS) \
	serveez.cfg $(GUILE_DEPENDENCY)

serveez_LDADD = $(resource_OBJECTS) $(PROTOLIBS) \
	libserveez/libserveez.la

serveez_LDFLAGS = $(GUILE_LDFLAGS)
INCLUDES = $(GUILE_CFLAGS) $(SERVEEZ_CFLAGS)

mkpassword_SOURCES = mkpassword.c

CLEANFILES = *~ *.ro serveez.cfg
MAINTAINERCLEANFILES = Makefile.in

dist-hook:
	rm -f $(distdir)/svzconfig.h

distclean-local:
	rm -f *\~ *.orig *.rej

serveez.cfg: $(top_srcdir)/data/serveez.cfg
	cp -p $(top_srcdir)/data/serveez.cfg $@

SUFFIXES = .rc .ro
WINDRES = windres

%.ro: %.rc
	$(WINDRES) -i $< -o $@

$(GUILE_RULE):
	@GUILE_MAKE_LTDL@
	@GUILE_MAKE_LIB@
