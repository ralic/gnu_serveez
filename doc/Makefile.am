## Process this file with automake to produce Makefile.in
# 
# doc/Makefile.am
#
# Automake input file for the documentation directory.
#
# Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
# Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
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

AUTOMAKE_OPTIONS = no-texinfo.tex
info_TEXINFOS = serveez.texi serveez-api.texi
man_MANS = serveez.1
EXTRA_DIST = $(man_MANS) index.html serveez.en.html \
	logo.jpg logosmall.jpg serveez-api.texi.in
CLEANFILES = *.info *.tgz *~ *.info-[0-9] stamp-vti* \
	serveez-api.texi $(SED_FILE)
MAINTAINERCLEANFILES = Makefile.in
PACK_FILES = ./*.html ../src/ChangeLog ../README ../README-Win32 \
	../TODO ../BUGS ../NEWS *.jpg
PACK_DIR = ./pack
DOC_DIRS = $(top_srcdir)/src/libserveez
DOC_FILES = icmp-socket.c raw-socket.c udp-socket.c pipe-socket.c \
  tcp-socket.c alloc.c sparsevec.c hash.c util.c vector.c socket.c \
  server-core.c server-loop.c server-socket.c boot.c interface.c \
  dynload.c windoze.c snprintf.c server.c core.c array.c hash.h
SED_FILE = serveez-api.sed
HTML_DIR = html_chapter

# local dependencies

distclean-local:
	rm -f *\~ *.orig *.rej

all-local: serveez.html serveez-api.html

dist-hook:
	rm -f $(distdir)/*.info $(distdir)/*.info-[0-9] $(distdir)/stamp-vti*

pack: serveez.html serveez-api.html
	test ! -d $(PACK_DIR) || rm -rf  $(PACK_DIR)
	@mkdir $(PACK_DIR)
	for file in $(PACK_FILES); do \
		cp -pf $$file $(PACK_DIR) >/dev/null 2>&1; done;
	(cd $(PACK_DIR) && \
		tar -czf ../doc_`date +%Y%m%d_%H%M`.tgz . >/dev/null 2>&1)
	@rm -rf $(PACK_DIR)

TEXI2HTML = texi2html
SED = sed

serveez-api.texi: serveez-api.texi.in
	rm -f $(SED_FILE)
	for dir in $(DOC_DIRS); do \
	  for file in $(DOC_FILES); do \
	    if test -r $$dir/$$file; then \
	      $(AWK) -f serveez-api.awk $$dir/$$file >> $(SED_FILE); fi \
	  done \
	done
	$(SED) -f $(SED_FILE) < $< > $@

%.html: %.texi
	$(TEXI2HTML) -subdir $(HTML_DIR) -split_chapter $<

%.info: %.texi
	$(MAKEINFO) -o $(srcdir)/$@ -I $(srcdir) --no-split $<

SUFFIXES = .html .info