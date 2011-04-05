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
	logo.jpg logosmall.jpg mandel.jpg \
	serveez-doc-snarf.awk
CLEANFILES += stamp-vti*
DISTCLEANFILES = stamp-*

libserveez_scanroot = .scan/libserveez
libserveez_root = $(top_srcdir)/src/libserveez
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
  vector.c vector.h \
  windoze.c \
  codec/codec.c \
  coserver/coserver.c coserver/coserver.h

guile_scanroot = .scan/guile
guile_root = $(top_srcdir)/src
guile_impl = \
  guile-api.c \
  guile-bin.c \
  guile-server.c \
  guile.c

GUILE_TXT_FILE = serveez-procedures.txt
SNARFER = $(AWK) -f $(srcdir)/serveez-doc-snarf.awk
MANUAL_FILES = \
  $(top_srcdir)/README-Win32 \
  $(top_srcdir)/BUGS $(top_srcdir)/NEWS $(top_srcdir)/README-cross
pkgdata_DATA = $(GUILE_TXT_FILE)

genstems = serveez-api guile-api
gen = $(genstems:%=%.sed) $(genstems:%=%.texi)

MAINTAINERCLEANFILES += $(gen) $(GUILE_TXT_FILE)
EXTRA_DIST += $(gen) $(genstems:%=%.texi.in)

maintainer-clean-local:
	rm -rf .scan

clean-local:
	rm -rf stage

serveez.info: blurb.texi
EXTRA_DIST += blurb.texi

# For regenerating both serveez-api.sed and guile-api.sed, we do things
# incrementally, but with some chance of fubar if careless, for those who
# ‘configure --enable-maintainer-mode’.  Otherwise, we do things en masse
# (inefficiently if few *_from files change), but w/ guaranteed safety.

serveez-api.texi: serveez-api.sed serveez-api.texi.in
	$(SED) -f $< $(srcdir)/serveez-api.texi.in > $@

libserveez_from = $(libserveez_impl:%=$(libserveez_root)/%)

serveez-api.sed: $(libserveez_from)
	@if test -d $(libserveez_scanroot);			\
	then list='$?';						\
	else list='$(libserveez_from)';				\
	fi;							\
	for f in $$list; do					\
	  frag=$$(echo $$f | sed 's|$(libserveez_root)||');	\
	  to=$(libserveez_scanroot)$$frag;			\
	  $(MKDIR_P) $$(dirname $$to);				\
	  echo updating: $$to;					\
	  $(SNARFER) $$f > $$to;				\
	done;							\
	cat $(libserveez_impl:%=$(libserveez_scanroot)/%) > $@

guile-api.texi: guile-api.sed guile-api.texi.in
	$(SED) -f $< $(srcdir)/guile-api.texi.in > $@

snippets: $(top_srcdir)/src/guile-boot.scm
	$(gx) tsar -v -l scheme -f $@ -c utf-8 update $?

guile-boot.texi: snippets

BUILT_SOURCES = snippets guile-boot.texi
EXTRA_DIST += $(BUILT_SOURCES) guile-boot.texh

.texh.texi:
	$(gx) tsin -f snippets -c utf-8 $<

guile_from = $(guile_impl:%=$(guile_root)/%)

guile-api.sed: $(guile_from)
	@if test -d $(guile_scanroot);			\
	then list='$?';					\
	else list='$(guile_from)';			\
	fi;						\
	for f in $$list; do				\
	  frag=$$(echo $$f | sed 's|$(guile_root)||');	\
	  to=$(guile_scanroot)$$frag;			\
	  $(MKDIR_P) $$(dirname $$to);			\
	  echo updating: $$to;				\
	  $(SNARFER) $$f > $$to;			\
	done;						\
	cat $(guile_impl:%=$(guile_scanroot)/%) > $@

# FIXME: We should be able to do ‘$(SNARFER) -t $^ > $@’ directly.
# We can't because serveez-doc-snarf.awk option parsing can't.
$(GUILE_TXT_FILE): $(guile_from)
	rm -f $@; for f in $(guile_from);	\
	  do $(SNARFER) $$f -t >> $@; done

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
