# GNUmakefile

# Copyright (C) 2013, 2014 Thien-Thi Nguyen
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

#-----------------------------------------------------------------------
# Texinfo 5.x is written in Perl, and is much slower than
# previous versions.  So "make all" is painful for the impatient.
# The target ‘ALL’ omits ‘doc/’ for that reason.
# Also, ‘ALL’ omits ‘test/’ and ‘CHECK’ omits ‘src/’ as
# there is "nothing to be done" in those circumstances.

include Makefile

ALL:
	@$(MAKE) -C src all

CHECK:
	@$(MAKE) -C test check

AC: ALL CHECK

# GNUmakefile ends here
