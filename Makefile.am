## Process this file with automake to produce Makefile.in
# 
# Makefile.am
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

SUBDIRS = src doc data test

EXTRA_DIST = BUGS autogen.sh README-alpha THANKS README-Win32 \
	serveez.dsw serveez.dsp config.h.serveez version.h.serveez \
	libsizzle.dsp config.h.sizzle version.h.sizzle

MAINTAINERCLEANFILES = aclocal.m4 config.h.in configure Makefile.in stamp-h.in

distclean-local:
	rm -f *\~
