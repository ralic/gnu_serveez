/*
 * boot.h - configuration and boot declarations
 *
 * Copyright (C) 2011 Thien-Thi Nguyen
 * Copyright (C) 2001, 2003 Stefan Jahn <stefan@lkcc.org>
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

#ifndef __BOOT_H__
#define __BOOT_H__ 1

/* begin svzint */
#include "libserveez/defines.h"
/* end svzint */

/*
 * General serveez configuration structure.
 */
typedef struct
{
  /* defines how many clients are allowed to connect */
  svz_t_socket max_sockets;
  /* log level verbosity */
  int verbosity;
}
svz_config_t;

__BEGIN_DECLS

SERVEEZ_API const char * const * svz_library_features (size_t *);

/* Core library configuration.  */
SERVEEZ_API svz_config_t svz_config;

/* Exported functions.  */
SERVEEZ_API void svz_boot (char const *);
SERVEEZ_API long svz_uptime (void);
SERVEEZ_API void svz_halt (void);

__END_DECLS

#endif /* not __BOOT_H__ */
