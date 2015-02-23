## Process this file with automake to produce Makefile.in
#
# src/irc-server/Makefile.am
#
# EFNet compatible IRC server.
#
# Copyright (C) 2011-2014 Thien-Thi Nguyen
# Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
# Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
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

noinst_LIBRARIES = libircserver.a

libircserver_a_SOURCES = \
	irc-proto.c irc-proto.h \
	irc-crypt.c irc-crypt.h \
	irc-event.h \
	irc-event-1.c \
	irc-event-2.c \
	irc-event-3.c \
	irc-event-4.c \
	irc-event-5.c \
	irc-event-6.c \
	irc-event-7.c \
	irc-server.h irc-server.c \
	irc-config.h irc-config.c

BUILT_SOURCES = timestamp.c

# The arguments to GNU and BSD date(1) used for parsing and formatting date
# strings are different (and conflicting).  Additionally, extracting file
# modification time and formatting it predictably are notoriously tricky to
# do portably.  mdate-sh, as part of the GNU Autotools suite, takes
# responsibility for "pretty-printing" a modification timestamp for a file,
# so it seems reasonable to delegate to it when reproducing the behavior of
# GNU date's ‘-r’ flag, which BSD date does not support.

timestamp.c: $(top_builddir)/config.status
	if date --version >/dev/null 2>&1 ; then			\
	  DATESTR=`date -r $< -u +'%Y-%m-%d %R UTC'` ;			\
	else								\
	  MDATE=`$(top_builddir)/build-aux/mdate-sh $<` ;		\
	  DATESTR=`date -jf "%d %B %Y" "$$MDATE" +'%Y-%m-%d %R UTC'` ;	\
	fi ;								\
	printf 'static char created[] = "%s";\n' "$$DATESTR" > $@
