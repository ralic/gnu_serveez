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
	logo.jpg logosmall.jpg serveez-api.texi.in serveez-api.awk
CLEANFILES = *.info *.tgz *~ *.info-[0-9] stamp-vti* \
	serveez-api.texi $(SED_FILE) $(HTML_DIR)/*.html
MAINTAINERCLEANFILES = Makefile.in
DOC_DIRS = $(top_srcdir)/src/libserveez \
  $(top_srcdir)/src/libserveez/coserver
DOC_FILES = icmp-socket.c raw-socket.c udp-socket.c pipe-socket.c \
  tcp-socket.c alloc.c sparsevec.c hash.c util.c vector.c socket.c \
  server-core.c server-loop.c server-socket.c boot.c interface.c \
  dynload.c windoze.c snprintf.c server.c core.c array.c hash.h binding.c \
  portcfg.c array.h coserver.c coserver.h vector.h portcfg.h server-core.h \
  socket.h alloc.h server.h util.h
SED_FILE = serveez-api.sed
HTML_DIR = manual
SITE_DIR = $(top_srcdir)/../serveez-site
SED = sed
MKDIR = mkdir -p
CP = cp
RM = rm
SITE_FILES = serveez.en.html index.html *.jpg
MANUAL_FILES = $(top_srcdir)/src/ChangeLog \
  $(top_srcdir)/src/libserveez/ChangeLog \
  $(top_srcdir)/README $(top_srcdir)/README-Win32 $(top_srcdir)/TODO \
  $(top_srcdir)/BUGS $(top_srcdir)/NEWS


# local dependencies

distclean-local:
	$(RM) -rf *\~ *.orig *.rej $(HTML_DIR)/

all-local: $(HTML_DIR)/serveez.html $(HTML_DIR)/serveez-api.html

dist-hook:
	$(RM) -f $(distdir)/*.info $(distdir)/*.info-[0-9] \
		$(distdir)/stamp-vti* $(distdir)/serveez-api.texi

serveez-api.texi: serveez-api.texi.in
	$(RM) -f $(SED_FILE)
	for dir in $(DOC_DIRS); do \
	  for file in $(DOC_FILES); do \
	    if test -r $$dir/$$file; then \
	      $(AWK) -f $(srcdir)/serveez-api.awk $$dir/$$file >> \
	      $(SED_FILE); fi \
	  done \
	done
	$(SED) -f $(SED_FILE) < $< > $@

site: all-local serveez.info
	test ! -d $(SITE_DIR) || $(RM) -rf $(SITE_DIR)
	@$(MKDIR) $(SITE_DIR)
	$(CP) -p serveez.en.html logo.jpg $(SITE_DIR)
	$(CP) -rp $(HTML_DIR) $(SITE_DIR)
	$(CP) -p index.html logosmall.jpg $(SITE_DIR)/$(HTML_DIR)
	$(CP) -p serveez.info $(SITE_DIR)/$(HTML_DIR)
	$(CP) -p $(MANUAL_FILES) $(SITE_DIR)/$(HTML_DIR)

$(HTML_DIR)/%.html: %.texi
	$(TEXI2HTML) -subdir $(HTML_DIR) -split_chapter $<

%.info: %.texi
	$(MAKEINFO) -o $(srcdir)/$@ -I $(srcdir) --no-split $<

SUFFIXES = .html .info
