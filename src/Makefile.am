## Process this file with automake to produce Makefile.in
# 
# src/Makefile.am
#
# Automake input file.
#
# Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
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
# $Id: Makefile.am,v 1.16 2000/10/16 18:39:49 ela Exp $
#

#
# *NEEDED* extra header files
#

EXTRA_DIST = \
	include/ipdata.h \
	include/iphlpapi.h

#
# Add your subdirectory here
#
SUBDIRS = \
	awcs-server \
	coserver \
	ctrl-server \
	http-server \
	irc-core \
	irc-server \
	foo-server \
	q3key-server \
	nut-server \
	tunnel-server

#
# Add your library here
#
PROTOLIBS = 	\
	awcs-server/libawcs.a \
	coserver/libcoserver.a \
	ctrl-server/libctrl.a \
	http-server/libhttp.a \
	irc-server/libircserver.a \
	irc-core/libirccore.a \
	foo-server/libfoo.a \
	q3key-server/libq3key.a \
	nut-server/libnut.a \
	tunnel-server/libtunnel.a

bin_PROGRAMS = serveez mkpassword

serveez_SOURCES = \
	serveez.c serveez.h \
	alloc.c alloc.h \
	util.c 	util.h \
	socket.c socket.h \
	server.c server.h \
	connect.h connect.c \
	cfgfile.c cfgfile.h \
	option.c option.h \
	snprintf.h snprintf.c \
	server-socket.c server-socket.h \
	hash.c hash.h \
	server-core.c server-core.h \
	server-loop.c server-loop.h \
	pipe-socket.c pipe-socket.h \
	interface.c interface.h \
	udp-socket.c udp-socket.h \
	icmp-socket.c icmp-socket.h \
	windoze.c windoze.h \
	alist.c alist.h

serveez_DEPENDENCIES = $(PROTOLIBS) $(SIZZLEDEP) \
	serveez.cfg

serveez_LDADD = $(PROTOLIBS) $(SIZZLESTATIC)

mkpassword_SOURCES = mkpassword.c

MAINTAINERCLEANFILES = Makefile.in

# This rule is used when we build from a sizzle source distribution
$(SIZZLERULE):
	@echo ""
	@echo "Building libsizzle.a from source distribution"
	@echo ""
	(cd $(SIZZLEDIR)/libsizzle && make)

distclean-local:
	rm -f *\~ *.orig *.rej

serveez.cfg: ../data/serveez.cfg
	cp $< $@

