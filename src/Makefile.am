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
# $Id: Makefile.am,v 1.26 2001/01/24 15:55:27 ela Exp $
#

#
# *NEEDED* extra header files
#

EXTRA_DIST = \
	include/ipdata.h include/iphlpapi.h \
	serveez.rc
#
# Add your server subdirectory here
#
SUBDIRS = \
	awcs-server \
	coserver \
	ctrl-server \
	http-server \
	irc-core \
	irc-server \
	foo-server \
	sntp-server \
	nut-server \
	tunnel-server \
	fakeident-server

#
# Add your library here
#
PROTOLIBS = $(SERVERLIBS) \
	coserver/libcoserver.a \
	irc-server/libircserver.a \
	irc-core/libirccore.a \
	foo-server/libfoo.a

#
# Serveez core library
#
lib_LTLIBRARIES = libserveez.la
libserveez_la_SOURCES = \
	alloc.c alloc.h \
	util.c util.h \
	snprintf.h snprintf.c \
	hash.c hash.h \
	alist.c alist.h \
	socket.c socket.h \
	connect.h connect.c \
	pipe-socket.c pipe-socket.h \
	udp-socket.c udp-socket.h \
	icmp-socket.c icmp-socket.h \
	raw-socket.c raw-socket.h \
	server-core.c server-core.h \
	server-loop.c server-loop.h \
	windoze.c windoze.h \
	libserveez.h internal.h

libserveez_la_LDFLAGS = -export-dynamic

bin_PROGRAMS = serveez mkpassword

serveez_SOURCES = \
	dynload.c dynload.h \
	serveez.c serveez.h \
	interface.c interface.h \
	server.c server.h \
	cfgfile.c cfgfile.h \
	option.c option.h \
	server-socket.c server-socket.h

resource_OBJECTS = $(resource_FILES:.rc=.ro)

serveez_DEPENDENCIES = $(resource_OBJECTS) $(PROTOLIBS) $(SIZZLEDEP) \
	serveez.cfg

serveez_LDADD = $(resource_OBJECTS) $(PROTOLIBS) $(SIZZLESTATIC) \
	libserveez.la

mkpassword_SOURCES = mkpassword.c

MAINTAINERCLEANFILES = Makefile.in

CLEANFILES = *.ro

# This rule is used when we build from a sizzle source distribution
$(SIZZLERULE):
	@echo ""
	@echo "building sizzle core library from source distribution"
	@echo ""
	(cd $(SIZZLEDIR)/libsizzle && $(MAKE) $(AM_MAKEFLAGS))

distclean-local:
	rm -f *\~ *.orig *.rej

serveez.cfg: $(top_srcdir)/data/serveez.cfg
	cp -p $< $@

SUFFIXES = .rc .ro
WINDRES = windres

%.ro: %.rc
	$(WINDRES) -i $< -o $@
