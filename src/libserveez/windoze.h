/*
 * windoze.h - windows port interface
 *
 * Copyright (C) 2011 Thien-Thi Nguyen
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __WINDOZE_H__
#define __WINDOZE_H__ 1

/* begin svzint */
#include "libserveez/defines.h"
/* end svzint */

#ifdef __MINGW32__

__BEGIN_DECLS

SERVEEZ_API int svz_windoze_start_daemon (char *);
SERVEEZ_API int svz_windoze_stop_daemon (void);
SERVEEZ_API WCHAR *svz_windoze_asc2uni (CHAR *asc);
SERVEEZ_API CHAR *svz_windoze_uni2asc (WCHAR *unicode);
SBO unsigned svz_windoze_get_reg_unsigned (HKEY, char *, char *, unsigned);
SBO void svz_windoze_set_reg_unsigned (HKEY, char *, char *, unsigned);
SBO char *svz_windoze_get_reg_string (HKEY, char *, char *, char *);
SBO void svz_windoze_set_reg_string (HKEY, char *, char *, char *);

__END_DECLS

/*
 * This little modification is necessary for the native Win32 compiler.
 * We do have these macros defined in the MinGW32 and Cygwin headers
 * but not within the native Win32 headers.
 */
#ifndef S_ISDIR
# ifndef S_IFBLK
#  define S_IFBLK 0x3000
# endif
# define S_ISDIR(Mode) (((Mode) & S_IFMT) == S_IFDIR)
# define S_ISCHR(Mode) (((Mode) & S_IFMT) == S_IFCHR)
# define S_ISREG(Mode) (((Mode) & S_IFMT) == S_IFREG)
# define S_ISBLK(Mode) (((Mode) & S_IFMT) == S_IFBLK)
#endif /* not S_ISDIR */

#endif  /* __MINGW32__ */

#endif /* not __WINDOZE_H__ */
