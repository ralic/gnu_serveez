## Process this file with automake to produce Makefile.in
#
# test/Makefile.am
#
# Automake input file for the test directory.
#
# Copyright (C) 2000, 2001, 2002, 2003 Stefan Jahn <stefan@lkcc.org>
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

TESTS = vector-test array-test hash-test spvec-test

EXTRA_DIST = \
	prog-test.cfg test-suite.scm binary-test.scm server-test.scm \
	guile-server-test.scm

noinst_PROGRAMS = hash-test spvec-test array-test vector-test codec-test \
	prog-test

hash_test_SOURCES = hash-test.c test.c test.h
spvec_test_SOURCES = spvec-test.c test.c test.h
array_test_SOURCES = array-test.c test.c test.h
vector_test_SOURCES = vector-test.c test.c test.h
codec_test_SOURCES = codec-test.c
prog_test_SOURCES = prog-test.c

LDADD = -L../src/libserveez -lserveez

# woe

stems = $(noinst_PROGRAMS:%-test=%)
woeap = $(stems:%=%.ap)
woedsp = $(stems:%=%.dsp)

MAINTAINERCLEANFILES += $(woedsp)
EXTRA_DIST += $(woedsp) $(woeap)
