## Process this file with automake to produce Makefile.in
# 
# doc/Makefile.am
#
# Automake input file for the documentation directory.
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

AUTOMAKE_OPTIONS = no-texinfo.tex

info_TEXINFOS = serveez.texi
man_MANS = serveez.1
EXTRA_DIST = $(man_MANS) index.html
CLEANFILES = *.info serveez*.html *.tgz *~ *.info-[0-9] stamp-vti
MAINTAINERCLEANFILES = Makefile.in
PACK_FILES = ./*.html ../src/ChangeLog ../README ../README-Win32 \
	../TODO ../BUGS ../NEWS
PACK_DIR = ./pack

# local dependencies

distclean-local:
	rm -f *\~ *.orig *.rej

all-local: serveez.html serveez.dvi

dist-hook:
	rm -f $(distdir)/*.info $(distdir)/*.info-[0-9] $(distdir)/stamp-vti

pack: serveez.html
	test ! -d $(PACK_DIR) || rm -rf  $(PACK_DIR)
	@mkdir $(PACK_DIR)
	for file in $(PACK_FILES); do \
		cp -f $$file $(PACK_DIR) >/dev/null 2>&1; done;
	(cd $(PACK_DIR) && \
		tar -czf ../doc_`date +%Y%m%d_%H%M`.tgz . >/dev/null 2>&1)
	@rm -rf $(PACK_DIR)

TEXI2HTML = texi2html
SUFFIXES = .html

%.html: %.texi
	$(TEXI2HTML) -split_chapter $<

%.info: %.texi
	$(MAKEINFO) -I . --no-split $<
