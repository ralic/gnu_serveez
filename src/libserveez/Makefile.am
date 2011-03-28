## Process this file with automake to produce Makefile.in
#
# src/libserveez/Makefile.am
#
# Automake input file for the serveez core library.
#
# Copyright (C) 2011 Thien-Thi Nguyen
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
#

include $(top_srcdir)/build-aux/common.mk

EXTRA_DIST = mutex.h

SUBDIRS = coserver codec

# the serveez core library rules

BUILT_SOURCES = svzpath.h

lib_LTLIBRARIES = libserveez.la

libserveez_la_LDFLAGS = -export-dynamic -version-info $(LIBSERVEEZ_INTERFACE)
libserveez_la_DEPENDENCIES = coserver/libcoserver.la codec/libcodec.la
libserveez_la_LIBADD = $(libserveez_la_DEPENDENCIES) \
  $(LIBSERVEEZLIBS)

libserveez_la_SOURCES = alloc.c util.c hash.c                             \
  tcp-socket.c pipe-socket.c udp-socket.c icmp-socket.c raw-socket.c      \
  server-core.c server-loop.c windoze.c boot.c server.c server-socket.c   \
  interface.c dynload.c vector.c core.c socket.c array.c portcfg.c        \
  binding.c passthrough.c cfg.c mutex.c

# these file are available by #include <libserveez/foo.h>

libserveezdir = $(includedir)/libserveez

libserveez_HEADERS = alloc.h util.h hash.h socket.h                        \
  tcp-socket.h pipe-socket.h udp-socket.h icmp-socket.h                    \
  server-core.h windoze.h defines.h boot.h server.h                        \
  interface.h dynload.h vector.h core.h array.h portcfg.h                  \
  binding.h passthrough.h cfg.h

uninstall-hook:
	rmdir "$(DESTDIR)$(libserveezdir)"

svzpath.h: $(top_srcdir)/config.status
	@echo "Generating svzpath.h..."
	@echo '#define SVZ_PKGDATADIR "$(pkgdatadir)"' > $@
	@echo '#define SVZ_PKGLIBDIR "$(pkglibdir)"' >> $@

noinst_HEADERS = ipdata.h iphlpapi.h raw-socket.h server-socket.h

CLEANFILES += svzpath.h

# woe

EXTRA_DIST += libserveez.dsp libserveez.ap svzpath.h.serveez

MAINTAINERCLEANFILES += libserveez.dsp

svzpath.h.serveez: svzpath.h
	cp $< $@
