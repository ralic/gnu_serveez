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
 * $Id: guile-api.c,v 1.6 2001/11/10 17:45:11 ela Exp $
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

/* Validate network port range. */
#define VALIDATE_NETPORT(port, cell, arg) do {       \
  (port) = gh_scm2long (cell);                       \
  if ((port) < 0 || (port) >= 65536)                 \
    scm_out_of_range_pos (FUNC_NAME, (cell), (arg)); \
  } while (0)

/* Establishes a network connection to the given @var{host} [:@var{port}].
   If @var{proto} equals @code{PROTO_ICMP} the @var{port} argument is
   ignored. Valid identifiers for @var{proto} are @code{PROTO_TCP},
   @code{PROTO_UDP} and @code{PROTO_ICMP}. The @var{host} argument must be 
   either a string in dotted decimal form or a exact number in network byte 
   order. The @var{port} argument must be a exact number in the range from 
   0 to 65535. Returns a valid @code{#<svz-socket>} or @code{#f} on 
   failure. */
#define FUNC_NAME "svz:sock:connect"
SCM
guile_sock_connect (SCM host, SCM proto, SCM port)
{
  svz_socket_t *sock;
  unsigned long xhost;
  unsigned short xport = 0;
  long p;
  int xproto;
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
      VALIDATE_NETPORT (p, port, SCM_ARG3);
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

/* The @code{(svz:inet-ntoa)} function converts the Internet host address 
   @var{address} given in network byte order to a string in standard
   numbers-and-dots notation. */
#define FUNC_NAME "svz:inet-ntoa"
static SCM
guile_svz_inet_ntoa (SCM address)
{
  char *str;
  SCM_ASSERT_TYPE (gh_exact_p (address),
		   address, SCM_ARG1, FUNC_NAME, "exact");
  str = svz_inet_ntoa (gh_scm2ulong (address));
  return gh_str02scm (str);
}
#undef FUNC_NAME

/* Converts the Internet host address @var{address} from the standard 
   numbers-and-dots notation into binary data in network byte order.
   The @code{(svz:inet-aton)} function returns @code{#f} if the address is 
   invalid. */
#define FUNC_NAME "svz:inet-aton"
static SCM
guile_svz_inet_aton (SCM address)
{
  struct sockaddr_in addr;
  char *str;

  SCM_ASSERT_TYPE (gh_string_p (address),
		   address, SCM_ARG1, FUNC_NAME, "string");
  str = guile_to_string (address);
  if (svz_inet_aton (str, &addr) == -1)
    {
      guile_error ("%s: IP address in dotted decimals expected", FUNC_NAME);
      scm_must_free (str);
      return SCM_BOOL_F;
    }
  scm_must_free (str);
  return gh_ulong2scm (addr.sin_addr.s_addr);
}
#undef FUNC_NAME

/* The @code{(svz:ntohl)} function converts the 32 bit long integer
   @var{netlong} from network byte order to host byte order. */
#define FUNC_NAME "svz:ntohl"
static SCM
guile_svz_ntohl (SCM netlong)
{
  SCM_ASSERT_TYPE (gh_exact_p (netlong),
		   netlong, SCM_ARG1, FUNC_NAME, "exact");
  return gh_ulong2scm (ntohl (gh_scm2ulong (netlong)));
}
#undef FUNC_NAME

/* The @code{(svz:htonl)} function converts the 32 bit long integer
   @var{hostlong} from host byte order to network byte order. */
#define FUNC_NAME "svz:htonl"
static SCM
guile_svz_htonl (SCM hostlong)
{
  SCM_ASSERT_TYPE (gh_exact_p (hostlong),
		   hostlong, SCM_ARG1, FUNC_NAME, "exact");
  return gh_ulong2scm (htonl (gh_scm2ulong (hostlong)));
}
#undef FUNC_NAME

/* The @code{(svz:ntohs)} function converts the 16 bit short integer 
   @var{netshort} from network byte order to host byte order. */
#define FUNC_NAME "svz:ntohs"
static SCM
guile_svz_ntohs (SCM netshort)
{
  long i;
  SCM_ASSERT_TYPE (gh_exact_p (netshort),
		   netshort, SCM_ARG1, FUNC_NAME, "exact");
  VALIDATE_NETPORT (i, netshort, SCM_ARG1);
  return gh_int2scm (ntohs ((unsigned short) i));
}
#undef FUNC_NAME

/* The @code{(svz:htons)} function converts the 16 bit short integer 
   @var{hostshort} from host byte order to network byte order. */
#define FUNC_NAME "svz:htons"
static SCM
guile_svz_htons (SCM hostshort)
{
  long i;
  SCM_ASSERT_TYPE (gh_exact_p (hostshort),
		   hostshort, SCM_ARG1, FUNC_NAME, "exact");
  VALIDATE_NETPORT (i, hostshort, SCM_ARG1);
  return gh_int2scm (htons ((unsigned short) i));
}
#undef FUNC_NAME

/* Return the receive buffer of the socket @var{sock} as a binary smob. */
#define FUNC_NAME "svz:sock:receive-buffer"
static SCM
guile_sock_receive_buffer (SCM sock)
{
  svz_socket_t *xsock;
  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  return guile_data_to_bin (xsock->recv_buffer, xsock->recv_buffer_fill);
}
#undef FUNC_NAME

/* Dequeue @var{length} bytes from the receive buffer of the socket 
   @var{sock} which must be a valid @code{#<svz-socket>}. If the user omits
   the optional @var{length} argument all of the data in the receive buffer
   gets dequeued. Returns the number of bytes actually shuffled away. */
#define FUNC_NAME "svz:sock:receive-buffer-reduce"
static SCM
guile_sock_receive_buffer_reduce (SCM sock, SCM length)
{
  svz_socket_t *xsock;
  int len;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);

  /* Check if second length argument is given. */
  if (!gh_eq_p (length, SCM_UNDEFINED))
    {
      SCM_ASSERT_TYPE (gh_exact_p (length), 
		       length, SCM_ARG2, FUNC_NAME, "exact");
      len = gh_scm2int (length);
      if (len < 0 || len > xsock->recv_buffer_fill)
	scm_out_of_range_pos (FUNC_NAME, length, SCM_ARG2);
    }
  else
    {
      len = xsock->recv_buffer_fill;
    }
  svz_sock_reduce_recv (xsock, len);
  return gh_int2scm (len);
}
#undef FUNC_NAME

/* This procedure returns the current remote address as a pair like 
   @code{(host . port)} with both entries in network byte order. If you pass
   the optional argument @var{address} you can set the remote address of 
   the socket @var{sock}. */
#define FUNC_NAME "svz:sock:remote-address"
static SCM
guile_sock_remote_address (SCM sock, SCM address)
{
  svz_socket_t *xsock;
  long port;
  SCM pair;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  pair = gh_cons (gh_ulong2scm (xsock->remote_addr), 
		  gh_int2scm ((int) xsock->remote_port));
  if (!gh_eq_p (address, SCM_UNDEFINED))
    {
      SCM_ASSERT_TYPE (gh_pair_p (address) && gh_exact_p (gh_car (address))
		       && gh_exact_p (gh_cdr (address)), address, SCM_ARG2, 
		       FUNC_NAME, "pair of exact");
      VALIDATE_NETPORT (port, gh_cdr (address), SCM_ARG2);
      xsock->remote_addr = gh_scm2ulong (gh_car (address));
      xsock->remote_port = (unsigned short) port;
    }
  return pair;
}
#undef FUNC_NAME

/* This procedure returns the current local address as a pair like 
   @code{(host . port)} with both entries in network byte order. If you pass
   the optional argument @var{address} you can set the local address of 
   the socket @var{sock}. */
#define FUNC_NAME "svz:sock:local-address"
static SCM
guile_sock_local_address (SCM sock, SCM address)
{
  svz_socket_t *xsock;
  long port;
  SCM pair;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  pair = gh_cons (gh_ulong2scm (xsock->local_addr), 
		  gh_int2scm ((int) xsock->local_port));
  if (!gh_eq_p (address, SCM_UNDEFINED))
    {
      SCM_ASSERT_TYPE (gh_pair_p (address) && gh_exact_p (gh_car (address))
		       && gh_exact_p (gh_cdr (address)), address, SCM_ARG2, 
		       FUNC_NAME, "pair of exact");
      VALIDATE_NETPORT (port, gh_cdr (address), SCM_ARG2);
      xsock->local_addr = gh_scm2ulong (gh_car (address));
      xsock->local_port = (unsigned short) port;
    }
  return pair;
}
#undef FUNC_NAME

/* Return the given sockets @var{sock} parent and optionally set it to the
   socket @var{parent}. The procedure returns either a valid 
   @code{#<svz-socket>} object or an empty list. */
#define FUNC_NAME "svz:sock:parent"
static SCM
guile_sock_parent (SCM sock, SCM parent)
{
  SCM oparent = SCM_EOL;
  svz_socket_t *xsock, *xparent;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  if ((xparent = svz_sock_getparent (xsock)) != NULL)
    oparent = MAKE_SMOB (svz_socket, xparent);
  if (!gh_eq_p (parent, SCM_UNDEFINED))
    {
      CHECK_SMOB_ARG (svz_socket, parent, SCM_ARG2, "svz-socket", xparent);
      svz_sock_setparent (xsock, xparent);
    }
  return oparent;
}
#undef FUNC_NAME

/* Return the given sockets @var{sock} referrer and optionally set it to the
   socket @var{referrer}. The procedure returns either a valid 
   @code{#<svz-socket>} or an empty list. */
#define FUNC_NAME "svz:sock:referrer"
static SCM
guile_sock_referrer (SCM sock, SCM referrer)
{
  SCM oreferrer = SCM_EOL;
  svz_socket_t *xsock, *xreferrer;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  if ((xreferrer = svz_sock_getreferrer (xsock)) != NULL)
    oreferrer = MAKE_SMOB (svz_socket, xreferrer);
  if (!gh_eq_p (referrer, SCM_UNDEFINED))
    {
      CHECK_SMOB_ARG (svz_socket, referrer, SCM_ARG2, "svz-socket", xreferrer);
      svz_sock_setreferrer (xsock, xreferrer);
    }
  return oreferrer;
}
#undef FUNC_NAME

/* This procedure returns the @code{#<svz-server>} object associated with the
   given argument @var{sock}. The optional argument @var{server} can be used
   to redefine this association and must be a valid @code{#<svz-server>}
   object. For a usual socket callback like @code{connect-socket} or
   @code{handle-request} the association is already in place. But for sockets
   created by @code{(svz:sock:connect)} you can use it in order to make the
   returned socket object part of a server. */
#define FUNC_NAME "svz:sock:server"
static SCM
guile_sock_server (SCM sock, SCM server)
{
  SCM oserver = SCM_EOL;
  svz_socket_t *xsock;
  svz_server_t *xserver;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  if ((xserver = svz_server_find (xsock->cfg)) != NULL)
    oserver = MAKE_SMOB (svz_server, xserver);
  if (!gh_eq_p (server, SCM_UNDEFINED))
    {
      CHECK_SMOB_ARG (svz_server, server, SCM_ARG2, "svz-server", xserver);
      xsock->cfg = xserver->cfg;
    }
  return oserver;
}
#undef FUNC_NAME

/* This procedure schedules the socket @var{sock} for shutdown after all data
   within the send buffer queue has been send. The user should issue this
   procedure call right *before* the last call to @code{(svz:sock:print)}. */
#define FUNC_NAME "svz:sock:final-print"
static SCM
guile_sock_final_print (SCM sock)
{
  svz_socket_t *xsock;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  xsock->flags |= SOCK_FLAG_FINAL_WRITE;
  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

/* Returns @code{#t} if the given cell @var{sock} is an instance of a valid
   @code{#<svz-socket>}, otherwise @code{#f}. */
#define FUNC_NAME "svz:sock?"
static SCM
guile_sock_p (SCM sock)
{
  return CHECK_SMOB (svz_socket, sock) ? SCM_BOOL_T : SCM_BOOL_F;
}
#undef FUNC_NAME

/* Returns @code{#t} if the given cell @var{server} is an instance of a valid
   @code{#<svz-server>}, otherwise @code{#f}. */
#define FUNC_NAME "svz:server?"
static SCM
guile_server_p (SCM server)
{
  return CHECK_SMOB (svz_server, server) ? SCM_BOOL_T : SCM_BOOL_F;
}
#undef FUNC_NAME

/* Set the @code{disconnected-socket} member of the socket structure 
   @var{sock} to the Guile procedure @var{proc}. The given callback
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
  gh_new_procedure ("svz:htons", guile_svz_htons, 1, 0, 0);
  gh_new_procedure ("svz:ntohs", guile_svz_ntohs, 1, 0, 0);
  gh_new_procedure ("svz:htonl", guile_svz_htonl, 1, 0, 0);
  gh_new_procedure ("svz:ntohl", guile_svz_ntohl, 1, 0, 0);
  gh_new_procedure ("svz:inet-aton", guile_svz_inet_aton, 1, 0, 0);
  gh_new_procedure ("svz:inet-ntoa", guile_svz_inet_ntoa, 1, 0, 0);
  gh_new_procedure ("svz:sock:parent", guile_sock_parent, 1, 1, 0);
  gh_new_procedure ("svz:sock:referrer", guile_sock_referrer, 1, 1, 0);
  gh_new_procedure ("svz:sock:server", guile_sock_server, 1, 1, 0);
  gh_new_procedure ("svz:sock:final-print", guile_sock_final_print, 1, 0, 0);
  gh_new_procedure ("svz:sock?", guile_sock_p, 1, 0, 0);
  gh_new_procedure ("svz:server?", guile_server_p, 1, 0, 0);
  gh_new_procedure ("svz:sock:receive-buffer", 
		    guile_sock_receive_buffer, 1, 0, 0);
  gh_new_procedure ("svz:sock:receive-buffer-reduce",
		    guile_sock_receive_buffer_reduce, 1, 1, 0);
  gh_new_procedure ("svz:sock:remote-address",
		    guile_sock_remote_address, 1, 1, 0);
  gh_new_procedure ("svz:sock:local-address",
		    guile_sock_local_address, 1, 1, 0);

  DEFINE_SOCK_CALLBACK ("svz:sock:disconnected", disconnected_socket);
}

/* Finalize the API functions. */
void
guile_api_finalize (void)
{
}

#endif /* ENABLE_GUILE_SERVER */
