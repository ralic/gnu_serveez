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

TESTS = vector-test array-test hash-test spvec-test

EXTRA_DIST = hash.dsp vector.dsp array.dsp spvec.dsp

noinst_PROGRAMS = hash-test spvec-test array-test vector-test
hash_test_SOURCES = hash-test.c test.c test.h alloc.c hash.c
spvec_test_SOURCES = spvec-test.c test.c test.h alloc.c sparsevec.c
array_test_SOURCES = array-test.c test.c test.h alloc.c array.c
vector_test_SOURCES = vector-test.c test.c test.h alloc.c vector.c

AM_CFLAGS = -I$(top_srcdir)/src

%.c: $(top_srcdir)/src/libserveez/%.c
	cp -p $< $(srcdir)/$@

distclean-local:
	rm -f *\~ *.orig *.rej

dist-hook:
	rm -f $(distdir)/alloc.c $(distdir)/hash.c $(distdir)/sparsevec.c \
		$(distdir)/array.c $(distdir)/vector.c

CLEANFILES = alloc.c hash.c sparsevec.c array.c vector.c

MAINTAINERCLEANFILES = Makefile.in

SUFFIXES = .c

