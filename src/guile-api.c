/*
 * guile-api.c - export additional Serveez functionality to Guile
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: guile-api.c,v 1.2 2001/11/01 17:03:52 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_GUILE_SERVER

#define _GNU_SOURCE

#if GUILE_SOURCE
# include <libguile/gh.h>
#else
# include <guile/gh.h>
#endif

#include "libserveez.h"

/* Establishes a network connection to the given @var{host} [:@var{port}].
   If @var{proto} equals @code{PROTO_ICMP} the @var{port} argument is
   ignored. Valid identifiers for @var{proto} are @code{PROTO_TCP},
   @code{PROTO_UDP} and @code{PROTO_ICMP}. The @var{host} argument must be 
   either a string in dotted decimal form or a exact number in network byte 
   order. The @var{port} argument must be a exact number in the range from 
   0 to 65535. Returns a valid @code{svz-socket} or @code{#f} on failure. */
#define FUNC_NAME "svz:sock:connect"
SCM
guile_sock_connect (SCM host, SCM proto, SCM port)
{
  svz_socket_t *sock;
  unsigned long xhost;
  unsigned short xport = 0;
  int p, xproto;
  char *str;
  struct sockaddr_in addr;
  SCM ret = SCM_BOOL_F;

  SCM_ASSERT_TYPE (gh_exact_p (host) || gh_string_p (host), 
                   host, SCM_ARG1, FUNC_NAME, "string or exact");
  SCM_ASSERT_TYPE (gh_exact_p (proto),
                   proto, SCM_ARG2, FUNC_NAME, "exact");

  /* Extract host to connect to. */
  if (gh_exact_p (host))
    xhost = gh_scm2int (host);
  else
    {
      str = guile_to_string (host);
      if (svz_inet_aton (str, &addr) == -1)
	{
	  guile_error ("%s: IP address in dotted decimals expected",
		       FUNC_NAME);
	  scm_must_free (str);
	  return ret;
	}
      scm_must_free (str);
      xhost = addr.sin_addr.s_addr;
    }

  /* Extract protocol to use. */
  xproto = gh_scm2int (proto);

  /* Find out about given port. */
  if (!gh_eq_p (port, SCM_UNDEFINED))
    {
      SCM_ASSERT_TYPE (gh_exact_p (port),
		       port, SCM_ARG3, FUNC_NAME, "exact");
      p = gh_scm2int (port);
      if (p < 0 || p > 65535)
	scm_out_of_range_pos (FUNC_NAME, port, SCM_ARG3);
      xport = htons ((unsigned short) p);
    }

  /* Depending on the requested protocol; create different kinds of
     socket structures. */
  switch (xproto)
    {
    case PROTO_TCP:
      sock = svz_tcp_connect (xhost, xport);
      break;
    case PROTO_UDP:
      sock = svz_udp_connect (xhost, xport);
      break;
    case PROTO_ICMP:
      sock = svz_icmp_connect (xhost, xport, ICMP_SERVEEZ);
      break;
    default:
      scm_out_of_range_pos (FUNC_NAME, proto, SCM_ARG2);
    }

  if (sock == NULL)
    return ret;

  sock->disconnected_socket = guile_func_disconnected_socket;
  return MAKE_SMOB (svz_socket, sock);
}
#undef FUNC_NAME

/* Set the @code{disconnected_socket} member of the socket structure 
   @var{sock} to the guile procedure @var{proc}.  The given callback
   runs whenever the socket is lost for some external reason. The procedure 
   returns the previously set handler if there is any. */
#define FUNC_NAME "svz:sock:disconnected"
MAKE_SOCK_CALLBACK (disconnected_socket, "disconnected")
#undef FUNC_NAME

/* Initialize the API function calls supported by Guile. */
void
guile_api_init (void)
{
  gh_define ("PROTO_TCP", gh_int2scm (PROTO_TCP));
  gh_define ("PROTO_UDP", gh_int2scm (PROTO_UDP));
  gh_define ("PROTO_ICMP", gh_int2scm (PROTO_ICMP));
  gh_new_procedure ("svz:sock:connect", guile_sock_connect, 2, 1, 0);

  DEFINE_SOCK_CALLBACK ("svz:sock:disconnected", disconnected_socket);
}

/* Finalize the API functions. */
void
guile_api_finalize (void)
{
}

#endif /* ENABLE_GUILE_SERVER */
