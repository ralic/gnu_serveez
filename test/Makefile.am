## Process this file with automake to produce Makefile.in
# 
# test/Makefile.am
#
# Automake input file for the test directory.
#
# Copyright (C)  2000 Stefan Jahn <stefan@lkcc.org>
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

TESTS = hash-test alist-test

EXTRA_DIST = hash.dsp alist.dsp

noinst_PROGRAMS = hash-test alist-test
hash_test_SOURCES = hash-test.c test.c test.h alloc.c hash.c
alist_test_SOURCES = alist-test.c test.c test.h alloc.c alist.c

AM_CFLAGS = -I../src

alloc.c: ../src/alloc.c
	ln -s $< $@

hash.c: ../src/hash.c
	ln -s $< $@

alist.c: ../src/alist.c
	ln -s $< $@

distclean-local:
	rm -f *\~ *.orig *.rej

dist-hook:
	rm -f $(distdir)/alloc.c
	rm -f $(distdir)/hash.c
	rm -f $(distdir)/alist.c

CLEANFILES = alloc.c hash.c alist.c

MAINTAINERCLEANFILES = Makefile.in
