## Process this file with automake to produce Makefile.in
#
# doc/Makefile.am
#
# Automake input file for the documentation directory.
#
# Copyright (C) 2011 Thien-Thi Nguyen
# Copyright (C) 2000, 2001, 2002, 2003 Stefan Jahn <stefan@lkcc.org>
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

AM_MAKEINFOFLAGS = --split-size=999999

info_TEXINFOS = serveez.texi
serveez_TEXINFOS = serveez-api.texi guile-api.texi guile-boot.texi fdl.texi
man_MANS = serveez.1 serveez-config.1
EXTRA_DIST = $(man_MANS) index.html serveez.en.html \
	logo.jpg logosmall.jpg mandel.jpg
CLEANFILES += stamp-vti*
DISTCLEANFILES = stamp-*

srcsrcd = $(top_srcdir)/src

libserveez_scanroot = .scan/libserveez
libserveez_root = $(srcsrcd)/libserveez
libserveez_impl = \
  alloc.c alloc.h \
  array.c array.h \
  binding.c \
  boot.c boot.h \
  cfg.c cfg.h \
  core.c \
  dynload.c \
  hash.c hash.h \
  icmp-socket.c \
  interface.c interface.h \
  passthrough.c passthrough.h \
  pipe-socket.c \
  portcfg.c portcfg.h \
  raw-socket.c \
  server-core.c server-core.h \
  server-loop.c \
  server-socket.c \
  server.c server.h \
  socket.c socket.h \
  tcp-socket.c \
  udp-socket.c \
  util.c util.h \
  windoze.c \
  codec/codec.c \
  coserver/coserver.c coserver/coserver.h

guile_impl = \
  guile-api.c \
  guile-bin.c \
  guile-server.c \
  guile.c
guile_from = $(guile_impl:%=$(srcsrcd)/%)

MANUAL_FILES = \
  $(top_srcdir)/README-Win32 \
  $(top_srcdir)/BUGS $(top_srcdir)/NEWS $(top_srcdir)/README-cross

gen = serveez-api.texi

MAINTAINERCLEANFILES += $(gen)
EXTRA_DIST += $(gen)

maintainer-clean-local:
	rm -rf .scan

clean-local:
	rm -rf stage

serveez.info: blurb.texi
EXTRA_DIST += blurb.texi

libserveez_from = $(libserveez_impl:%=$(libserveez_root)/%)

scm-snippets: $(srcsrcd)/guile-boot.scm
	$(gx) tsar -v -l scheme -f $@ -c utf-8 update $?

c-snippets: $(guile_from)
	$(gx) c-tsar -v -l scheme -f $@ -c utf-8 \
	  update $^ -- -I$(srcsrcd) $(GUILE_CFLAGS)

c-c-snippets: $(libserveez_from)
	$(gx) s3as -o $@ $(libserveez_from)

snippets: scm-snippets c-snippets c-c-snippets
	$(gx) tsar -f $@ -c utf-8 concat $^

guile-boot.texi: snippets
guile-api.texi: snippets
serveez-api.texi: snippets

BUILT_SOURCES = snippets guile-boot.texi
EXTRA_DIST += $(BUILT_SOURCES) guile-boot.texh

.texh.texi:
	$(gx) tsin -f snippets -c utf-8 $<

genhtml = serveez.html

sednow = $(SED) "s/LAST-MODIFIED/$$(date -u +'%Y-%m-%d %T UTC')/"

stage: $(genhtml)
	rm -rf $@
	m=$@/manual; $(MKDIR_P) $$m;				\
	for d in $(genhtml);					\
	do cp -rp $$d $$m/$$(basename $$d .html); done;		\
	cp -p logosmall.jpg mandel.jpg $(MANUAL_FILES) $$m;	\
	$(SED) -e '/^>BLURB</{'					\
	       -e ' s/.*//'					\
	       -e ' r blurb.texi'				\
	       -e '}'						\
	  $(top_srcdir)/README | $(SED) '/^@c/d' > $$m/README;	\
	$(sednow) index.html > $$m/index.html;			\
	cp -p logo.jpg $@;					\
	$(sednow) serveez.en.html > $@/serveez.en.html
