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

TEXINFO_TEX = no-texinfo.tex

info_TEXINFOS = serveez.texi

EXTRA_DIST = awcs-proto.txt mime.types port.lst irc-MOTD.txt \
	serveez.cfg

CLEANFILES = *.info

MAINTAINERCLEANFILES = Makefile.in

distclean-local:
	rm -f *\~ *.orig *.rej

# local dependencies
