## Process this file with automake to produce Makefile.in
#
# doc/Makefile.am
#
# Automake input file for the documentation directory.
#
# Copyright (C) 2011-2013 Thien-Thi Nguyen
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
gen = guile-boot.texi guile-api.texi serveez-api.texi
serveez_TEXINFOS = serveez.texi $(gen) fdl.texi

man_MANS = serveez.1
if COURAGEOUS
man_MANS += serveez-config.1
endif COURAGEOUS

EXTRA_DIST = $(man_MANS) index.html serveez.en.html \
	logo.jpg logosmall.jpg mandel.jpg
CLEANFILES += stamp-vti*
DISTCLEANFILES = stamp-*

MANUAL_FILES = \
  $(top_srcdir)/BUGS $(top_srcdir)/NEWS $(top_srcdir)/README-cross

MAINTAINERCLEANFILES += $(gen)
EXTRA_DIST += $(gen) $(gen:%.texi=%.texh)

clean-local:
	rm -rf stage

serveez.info: blurb.texi
EXTRA_DIST += blurb.texi

snippets: ../src/scm.tsar ../src/c.tsar ../src/libserveez/libserveez.tsar
	$(gx) tsar -f $@ -c utf-8 concat $^

if MAINTAINER_MODE
$(gen) : snippets

.texh.texi:
	$(gx) tsin -f snippets -c utf-8 $<
endif

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
