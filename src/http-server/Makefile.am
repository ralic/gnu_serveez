# Small and fast minimal webserver
#
# src/http-server/Makefile.am
#
# Automake input file.
#
# Copyright (C) 2000 Ela & Raimi
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

noinst_LIBRARIES = libhttp.a

AM_CPPFLAGS = -I..

CLEANFILES = *~

libhttp_a_SOURCES = \
	http-cache.c http-cache.h \
	http-cgi.c http-cgi.h \
	http-dirlist.c http-dirlist.h \
	http-proto.c http-proto.h \
	http-core.c http-core.h
