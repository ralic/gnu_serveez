## Process this file with automake to produce Makefile.in
# 
# src/Makefile.am
#
# Automake input file.
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
	foo-server

#
# Add your library here
#
LIBS = 	\
	$(SIZZLEDIR)/libsizzle/.libs/libsizzle.a \
	awcs-server/libawcs.a \
	coserver/libcoserver.a \
	ctrl-server/libctrl.a \
	http-server/libhttp.a \
	irc-server/libircserver.a \
	irc-core/libirccore.a \
	foo-server/libfoo.a


bin_PROGRAMS = serveez

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
	server-core.c server-core.h

serveez_DEPENDENCIES = $(LIBS) \
	serveez.cfg

serveez_LDADD = $(LIBS) \
	-lm

MAINTAINERCLEANFILES = Makefile.in

distclean-local:
	rm -f *\~ *.orig *.rej

$(SIZZLEDIR)/libsizzle/.libs/libsizzle.a:
	(cd $(SIZZLEDIR)/libsizzle && make)

serveez.cfg: ../doc/serveez.cfg
	cp $< $@

