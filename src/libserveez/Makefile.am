## Process this file with automake to produce Makefile.in
# 
# src/libserveez/Makefile.am
#
# Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
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
# $Id: Makefile.am,v 1.3 2001/01/30 11:49:57 ela Exp $
#

SUBDIRS = coserver

# the serveez core library rules

lib_LTLIBRARIES = libserveez.la

libserveez_la_LDFLAGS = -export-dynamic -no-undefined
libserveez_la_DEPENDENCIES = coserver/libcoserver.la
libserveez_la_LIBADD = coserver/libcoserver.la

libserveez_la_SOURCES = alloc.c util.c snprintf.c hash.c alist.c socket.c \
  tcp-socket.c pipe-socket.c udp-socket.c icmp-socket.c raw-socket.c      \
  server-core.c server-loop.c windoze.c boot.c server.c server-socket.c

# these file are available by #include <libserveez/foo.h>

libserveezdir = $(includedir)/libserveez

libserveez_HEADERS = alloc.h util.h snprintf.h hash.h alist.h socket.h \
  tcp-socket.h pipe-socket.h udp-socket.h icmp-socket.h raw-socket.h   \
  server-core.h server-loop.h windoze.h defines.h boot.h server.h      \
  server-socket.h

INCLUDES = -I..

CLEANFILES = *~ *.orig *.rej

MAINTAINERCLEANFILES = Makefile.in
