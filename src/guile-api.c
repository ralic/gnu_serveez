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
 * $Id: guile-api.c,v 1.15 2001/12/27 18:27:51 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_GUILE_SERVER

#define _GNU_SOURCE
#include <string.h>
#include <errno.h>

#ifndef __MINGW32__
# include <sys/socket.h>
# include <netdb.h>
# include <netinet/in.h>
#endif

#if HAVE_RPC_RPCENT_H
# include <rpc/rpcent.h>
#endif
#if HAVE_RPC_RPC_H
# include <rpc/rpc.h>
#endif
#if HAVE_RPC_PMAP_CLNT_H
# include <rpc/pmap_clnt.h>
#endif

#if GUILE_SOURCE
# include <libguile/gh.h>
#else
# include <guile/gh.h>
#endif

#include "libserveez.h"

/* Validate network port range. */
#define VALIDATE_NETPORT(port, cell, arg) do {       \
  (port) = SCM_NUM2LONG (arg, cell);                 \
  if ((port) < 0 || (port) >= 65536)                 \
    scm_out_of_range_pos (FUNC_NAME, (cell), (arg)); \
  } while (0)


/* Converts the given hostname @var{host} into a Internet address in host
   byte order and stores it into @var{addr}. Returns zero on success. This
   is a blocking operation. */
static int
guile_resolve (char *host, unsigned long *addr)
{
  struct hostent *ent;

  if ((ent = gethostbyname (host)) == NULL)
    return -1;
  if (ent->h_addrtype == AF_INET)
    {
      memcpy (addr, ent->h_addr_list[0], ent->h_length);
      return 0;
    }
  return -1;
}

/* Establishes a network connection to the given @var{host} [ :@var{port} ].
   If @var{proto} equals @code{PROTO_ICMP} the @var{port} argument is
   ignored. Valid identifiers for @var{proto} are @code{PROTO_TCP},
   @code{PROTO_UDP} and @code{PROTO_ICMP}. The @var{host} argument must be 
   either a string in dotted decimal form, a valid hostname or a exact number
   in host byte order. When giving a hostname this operation might be
   blocking. The @var{port} argument must be a exact number in the range from 
   0 to 65535, also in host byte order. Returns a valid @code{#<svz-socket>} 
   or @code{#f} on failure. */
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

  SCM_ASSERT_TYPE (SCM_EXACTP (host) || SCM_STRINGP (host), 
                   host, SCM_ARG1, FUNC_NAME, "string or exact");
  SCM_ASSERT_TYPE (SCM_EXACTP (proto),
                   proto, SCM_ARG2, FUNC_NAME, "exact");

  /* Extract host to connect to. */
  if (SCM_EXACTP (host))
    xhost = htonl ((unsigned long) SCM_NUM2INT (SCM_ARG1, host));
  else
    {
      str = guile_to_string (host);
      if (svz_inet_aton (str, &addr) == -1)
	{
	  if (guile_resolve (str, &xhost) == -1)
	    {
	      guile_error ("%s: IP in dotted decimals or hostname expected", 
			   FUNC_NAME);
	      scm_c_free (str);
	      return ret;
	    }
	}
      else
	xhost = addr.sin_addr.s_addr;
      scm_c_free (str);
    }

  /* Extract protocol to use. */
  xproto = SCM_NUM2INT (SCM_ARG2, proto);

  /* Find out about given port. */
  if (!SCM_UNBNDP (port))
    {
      SCM_ASSERT_TYPE (SCM_EXACTP (port),
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
  SCM_ASSERT_TYPE (SCM_EXACTP (address),
		   address, SCM_ARG1, FUNC_NAME, "exact");
  str = svz_inet_ntoa (SCM_NUM2ULONG (SCM_ARG1, address));
  return scm_makfrom0str (str);
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

  SCM_ASSERT_TYPE (SCM_STRINGP (address),
		   address, SCM_ARG1, FUNC_NAME, "string");
  str = guile_to_string (address);
  if (svz_inet_aton (str, &addr) == -1)
    {
      guile_error ("%s: IP address in dotted decimals expected", FUNC_NAME);
      scm_c_free (str);
      return SCM_BOOL_F;
    }
  scm_c_free (str);
  return scm_ulong2num (addr.sin_addr.s_addr);
}
#undef FUNC_NAME

/* The @code{(svz:ntohl)} function converts the 32 bit long integer
   @var{netlong} from network byte order to host byte order. */
#define FUNC_NAME "svz:ntohl"
static SCM
guile_svz_ntohl (SCM netlong)
{
  SCM_ASSERT_TYPE (SCM_EXACTP (netlong),
		   netlong, SCM_ARG1, FUNC_NAME, "exact");
  return scm_ulong2num (ntohl (SCM_NUM2ULONG (SCM_ARG1, netlong)));
}
#undef FUNC_NAME

/* The @code{(svz:htonl)} function converts the 32 bit long integer
   @var{hostlong} from host byte order to network byte order. */
#define FUNC_NAME "svz:htonl"
static SCM
guile_svz_htonl (SCM hostlong)
{
  SCM_ASSERT_TYPE (SCM_EXACTP (hostlong),
		   hostlong, SCM_ARG1, FUNC_NAME, "exact");
  return scm_ulong2num (htonl (SCM_NUM2ULONG (SCM_ARG1, hostlong)));
}
#undef FUNC_NAME

/* The @code{(svz:ntohs)} function converts the 16 bit short integer 
   @var{netshort} from network byte order to host byte order. */
#define FUNC_NAME "svz:ntohs"
static SCM
guile_svz_ntohs (SCM netshort)
{
  long i;
  SCM_ASSERT_TYPE (SCM_EXACTP (netshort),
		   netshort, SCM_ARG1, FUNC_NAME, "exact");
  VALIDATE_NETPORT (i, netshort, SCM_ARG1);
  return scm_int2num (ntohs ((unsigned short) i));
}
#undef FUNC_NAME

/* The @code{(svz:htons)} function converts the 16 bit short integer 
   @var{hostshort} from host byte order to network byte order. */
#define FUNC_NAME "svz:htons"
static SCM
guile_svz_htons (SCM hostshort)
{
  long i;
  SCM_ASSERT_TYPE (SCM_EXACTP (hostshort),
		   hostshort, SCM_ARG1, FUNC_NAME, "exact");
  VALIDATE_NETPORT (i, hostshort, SCM_ARG1);
  return scm_int2num (htons ((unsigned short) i));
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
  if (!SCM_UNBNDP (length))
    {
      SCM_ASSERT_TYPE (SCM_EXACTP (length), 
		       length, SCM_ARG2, FUNC_NAME, "exact");
      len = SCM_NUM2INT (SCM_ARG2, length);
      if (len < 0 || len > xsock->recv_buffer_fill)
	scm_out_of_range_pos (FUNC_NAME, length, SCM_ARG2);
    }
  else
    {
      len = xsock->recv_buffer_fill;
    }
  svz_sock_reduce_recv (xsock, len);
  return scm_int2num (len);
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
  pair = scm_cons (scm_ulong2num (xsock->remote_addr), 
		   scm_int2num ((int) xsock->remote_port));
  if (!SCM_UNBNDP (address))
    {
      SCM_ASSERT_TYPE (SCM_PAIRP (address) && SCM_EXACTP (SCM_CAR (address))
		       && SCM_EXACTP (SCM_CDR (address)), address, SCM_ARG2, 
		       FUNC_NAME, "pair of exact");
      VALIDATE_NETPORT (port, SCM_CDR (address), SCM_ARG2);
      xsock->remote_addr = SCM_NUM2ULONG (SCM_ARG2, SCM_CAR (address));
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
  pair = scm_cons (scm_ulong2num (xsock->local_addr), 
		   scm_int2num ((int) xsock->local_port));
  if (!SCM_UNBNDP (address))
    {
      SCM_ASSERT_TYPE (SCM_PAIRP (address) && SCM_EXACTP (SCM_CAR (address))
		       && SCM_EXACTP (SCM_CDR (address)), address, SCM_ARG2, 
		       FUNC_NAME, "pair of exact");
      VALIDATE_NETPORT (port, SCM_CDR (address), SCM_ARG2);
      xsock->local_addr = SCM_NUM2ULONG (SCM_ARG2, SCM_CAR (address));
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
  if (!SCM_UNBNDP (parent))
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
  if (!SCM_UNBNDP (referrer))
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
  if (!SCM_UNBNDP (server))
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

/* Turns the Nagle algorithm for the TCP socket @var{sock} on or off depending
   on the optional @var{enable} argument. Returns the previous state of this
   flag (@code{#f} if Nagle is active, @code{#t} otherwise). By default this 
   flag is switched off. This socket option is useful when dealing with small
   packet transfer in order to disable unnecessary delays. */
#define FUNC_NAME "svz:sock:no-delay"
static SCM
guile_sock_no_delay (SCM sock, SCM enable)
{
  svz_socket_t *xsock;
  int old = 0, set = 0;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  if (xsock->proto & PROTO_TCP)
    {
      if (!SCM_UNBNDP (enable))
	{
	  SCM_ASSERT_TYPE (SCM_BOOLP (enable) || SCM_EXACTP (enable), 
			   enable, SCM_ARG2, FUNC_NAME, "boolean or exact");
	  if ((SCM_BOOLP (enable) && SCM_NFALSEP (enable) != 0) ||
	      (SCM_EXACTP (enable) && SCM_NUM2INT (SCM_ARG2, enable) != 0))
	    set = 1;
	}
      if (svz_tcp_nodelay (xsock->sock_desc, set, &old) < 0)
	old = 0;
      else if (SCM_UNBNDP (enable))
	svz_tcp_nodelay (xsock->sock_desc, old, NULL);
    }
  return SCM_BOOL (old);
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

/* Sets the @code{kicked-socket} callback of the given socket structure
   @var{sock} to the Guile procedure @var{proc} and returns any previously 
   set procedure.  This callback gets called whenever the socket gets 
   closed by Serveez intentionally.  */
#define FUNC_NAME "svz:sock:kicked"
MAKE_SOCK_CALLBACK (kicked_socket, "kicked")
#undef FUNC_NAME

/* This procedure sets the @code{idle} callback of the socket structure
   @var{sock} to the Guile procedure @var{proc}. It returns any previously
   set procedure. The callback is run by the periodic task scheduler when the
   @code{idle-counter} of the socket struture drops to zero. If this counter
   is not zero it gets decremented once a second. The @code{idle}
   callback can reset @code{idle-counter} to some value and thus can 
   re-schedule itself for a later task. */
#define FUNC_NAME "svz:sock:idle"
MAKE_SOCK_CALLBACK (idle_func, "idle")
#undef FUNC_NAME

/* This functions returns the socket structure's @var{sock} current value 
   of the @code{idle-counter}. If the optional argument @var{counter} is
   given the function sets the @code{idle-counter}. Please have a look at the
   @code{(svz:sock:idle)} procedure for the exact meaning of this value. */
#define FUNC_NAME "svz:sock:idle-counter"
static SCM
guile_sock_idle_counter (SCM sock, SCM counter)
{
  svz_socket_t *xsock;
  int ocounter;

  CHECK_SMOB_ARG (svz_socket, sock, SCM_ARG1, "svz-socket", xsock);
  ocounter = xsock->idle_counter;
  if (!SCM_UNBNDP (counter))
    {
      SCM_ASSERT_TYPE (SCM_EXACTP (counter), 
		       counter, SCM_ARG2, FUNC_NAME, "exact");
      xsock->idle_counter = SCM_NUM2INT (SCM_ARG2, counter);
    }
  return scm_int2num (ocounter);
}
#undef FUNC_NAME

/* Returns a list of listening @code{#<svz-socket>} smobs to which the 
   given server instance @var{server} is currently bound to or an empty list
   if there is no such binding yet. */
#define FUNC_NAME "svz:server:listeners"
static SCM
guile_server_listeners (SCM server)
{
  svz_server_t *xserver = NULL;
  svz_array_t *listeners;
  char *str;
  unsigned long i;
  SCM list = SCM_EOL;

  /* Server instance name given ? */
  if ((str = guile_to_string (server)) != NULL)
    {
      xserver = svz_server_get (str);
      scm_c_free (str);
    }
  /* Maybe server smob given. */
  if (xserver == NULL)
    {
      CHECK_SMOB_ARG (svz_server, server, SCM_ARG1, "svz-server or string", 
		      xserver);
    }

  /* Create a list of socket smobs for the server. */
  if ((listeners = svz_server_listeners (xserver)) != NULL)
    {
      for (i = 0; i < svz_array_size (listeners); i++)
	list = scm_cons (MAKE_SMOB (svz_socket, (svz_socket_t *)
				    svz_array_get (listeners, i)),
			 list);
      svz_array_destroy (listeners);
    }
  return scm_reverse (list);
}
#undef FUNC_NAME

#if HAVE_GETRPCENT || HAVE_GETRPCBYNAME || HAVE_GETRPCBYNUMBER
static SCM
scm_return_rpcentry (struct rpcent *entry)
{
  SCM ans;
  SCM *ve;

  ans = scm_c_make_vector (3, SCM_UNSPECIFIED);
  ve = SCM_VELTS (ans);
  ve[0] = scm_mem2string (entry->r_name, strlen (entry->r_name));
  ve[1] = scm_makfromstrs (-1, entry->r_aliases);
  ve[2] = scm_ulong2num ((unsigned long) entry->r_number);
  return ans;
}

/* @defunx getrpcent
   @defunx getrpcbyname name
   @defunx getrpcbynumber number
   Lookup a network rpc service by name or by service number, and 
   return a network rpc service object.  The @code{(getrpc)} procedure 
   will take either a rpc service name or number as its first argument; 
   if given no arguments, it behaves like @code{(getrpcent)}. */
#define FUNC_NAME "getrpc"
static SCM
scm_getrpc (SCM arg)
{
  struct rpcent *entry = NULL;

#if HAVE_GETRPCENT
  if (SCM_UNBNDP (arg))
    {
      if ((entry = getrpcent ()) == NULL)
	return SCM_BOOL_F;
      return scm_return_rpcentry (entry);
    }
#endif /* HAVE_GETRPCENT */
#if HAVE_GETRPCBYNAME
  if (SCM_STRINGP (arg))
    {
      entry = getrpcbyname (SCM_STRING_CHARS (arg));
    }
  else
#endif /* HAVE_GETRPCBYNAME */
#if HAVE_GETRPCBYNUMBER
    {
      SCM_ASSERT_TYPE (SCM_INUMP (arg), arg, SCM_ARG1, FUNC_NAME, "INUMP");
      entry = getrpcbynumber (SCM_INUM (arg));
    }
#endif /* #if HAVE_GETRPCBYNUMBER */

  if (!entry)
    scm_syserror_msg (FUNC_NAME, "no such rpc service ~A", 
		      scm_listify (arg, SCM_UNDEFINED), errno);
  return scm_return_rpcentry (entry);
}
#undef FUNC_NAME
#endif /* HAVE_GETRPCENT || HAVE_GETRPCBYNAME || HAVE_GETRPCBYNUMBER */

#ifndef DECLARED_SETRPCENT
extern void setrpcent (int);
#endif /* DECLARED_SETRPCENT */
#ifndef DECLARED_ENDRPCENT
extern void endrpcent (void);
#endif /* DECLARED_ENDRPCENT */

#if HAVE_SETRPCENT && HAVE_ENDRPCENT
/* @defunx setrpcent stayopen
   @defunx endrpcent
   The @code{(setrpc)} procedure opens and rewinds the file @file{/etc/rpc}.
   If the @var{stayopen} flag is non-zero, the net data base will not be 
   closed after each call to @code{(getrpc)}.  If @var{stayopen} is omitted, 
   this is equivalent to @code{(endrpcent)}.  Otherwise it is equivalent to 
   @code{(setrpcent stayopen)}. */
#define FUNC_NAME "setrpc"
static SCM
scm_setrpc (SCM stayopen)
{
  if (SCM_UNBNDP (stayopen))
    endrpcent ();
  else
    setrpcent (!SCM_FALSEP (stayopen));
  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME
#endif /* HAVE_SETRPCENT && HAVE_ENDRPCENT */

#if HAVE_PMAP_SET && HAVE_PMAP_UNSET
/* A user interface to the portmap service, which establishes a mapping
   between the triple [@var{prognum},@var{versnum},@var{protocol}] and 
   @var{port} on the machine's portmap service. The value of @var{protocol}
   is most likely @code{IPPROTO_UDP} or @code{IPPROTO_TCP}.
   If the user omits @var{protocol} and @var{port} the procedure destroys
   all mapping between the triple [@var{prognum},@var{versnum},*] and ports
   on the machine's portmap service. */
#define FUNC_NAME "portmap"
SCM
scm_portmap (SCM prognum, SCM versnum, SCM protocol, SCM port)
{
  SCM_ASSERT_TYPE (SCM_INUMP (prognum), prognum, SCM_ARG1, 
		   FUNC_NAME, "INUMP");
  SCM_ASSERT_TYPE (SCM_INUMP (versnum), prognum, SCM_ARG2, 
		   FUNC_NAME, "INUMP");

  if (SCM_UNBNDP (protocol) && SCM_UNBNDP (port))
    {
      if (!pmap_unset (SCM_INUM (prognum), SCM_INUM (versnum)))
	scm_syserror_msg (FUNC_NAME, "~A: pmap_unset ~A ~A",
			  scm_listify (scm_makfrom0str (strerror (errno)),
				       prognum, versnum, SCM_UNDEFINED), 
			  errno);
    }
  else
    {
      SCM_ASSERT_TYPE (SCM_INUMP (protocol), protocol, SCM_ARG3, 
		       FUNC_NAME, "INUMP");
      SCM_ASSERT_TYPE (SCM_INUMP (port), port, SCM_ARG4, 
		       FUNC_NAME, "INUMP");

      if (!pmap_set (SCM_INUM (prognum), SCM_INUM (versnum), 
		     SCM_INUM (protocol), (unsigned short) SCM_INUM (port)))
	scm_syserror_msg (FUNC_NAME, "~A: pmap_set ~A ~A ~A ~A",
			  scm_listify (scm_makfrom0str (strerror (errno)),
				       prognum, versnum, protocol, port, 
				       SCM_UNDEFINED), errno);
    }
  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME
#endif /* HAVE_PMAP_SET && HAVE_PMAP_UNSET */

/* Initialize the API function calls supported by Guile. */
void
guile_api_init (void)
{
#if HAVE_GETRPCENT || HAVE_GETRPCBYNAME || HAVE_GETRPCBYNUMBER
  scm_c_define_gsubr ("getrpc", 0, 1, 0, scm_getrpc);
#endif
#if HAVE_SETRPCENT && HAVE_ENDRPCENT
  scm_c_define_gsubr ("setrpc", 0, 1, 0, scm_setrpc);
#endif
#if HAVE_PMAP_SET && HAVE_PMAP_UNSET
  scm_c_define_gsubr ("portmap", 2, 2, 0, scm_portmap);
  scm_c_define ("IPPROTO_UDP", scm_int2num (IPPROTO_UDP));
  scm_c_define ("IPPROTO_TCP", scm_int2num (IPPROTO_TCP));
#endif
  
  scm_c_define ("PROTO_TCP", scm_int2num (PROTO_TCP));
  scm_c_define ("PROTO_UDP", scm_int2num (PROTO_UDP));
  scm_c_define ("PROTO_ICMP", scm_int2num (PROTO_ICMP));
  scm_c_define ("KICK_FLOOD", scm_int2num (0));
  scm_c_define ("KICK_QUEUE", scm_int2num (1));
  scm_c_define_gsubr ("svz:sock:connect", 2, 1, 0, guile_sock_connect);
  scm_c_define_gsubr ("svz:htons", 1, 0, 0, guile_svz_htons);
  scm_c_define_gsubr ("svz:ntohs", 1, 0, 0, guile_svz_ntohs);
  scm_c_define_gsubr ("svz:htonl", 1, 0, 0, guile_svz_htonl);
  scm_c_define_gsubr ("svz:ntohl", 1, 0, 0, guile_svz_ntohl);
  scm_c_define_gsubr ("svz:inet-aton", 1, 0, 0, guile_svz_inet_aton);
  scm_c_define_gsubr ("svz:inet-ntoa", 1, 0, 0, guile_svz_inet_ntoa);
  scm_c_define_gsubr ("svz:sock:parent", 1, 1, 0, guile_sock_parent);
  scm_c_define_gsubr ("svz:sock:referrer", 1, 1, 0, guile_sock_referrer);
  scm_c_define_gsubr ("svz:sock:server", 1, 1, 0, guile_sock_server);
  scm_c_define_gsubr ("svz:sock:final-print", 1, 0, 0, guile_sock_final_print);
  scm_c_define_gsubr ("svz:sock:no-delay", 1, 1, 0, guile_sock_no_delay);
  scm_c_define_gsubr ("svz:sock?", 1, 0, 0, guile_sock_p);
  scm_c_define_gsubr ("svz:server?", 1, 0, 0, guile_server_p);
  scm_c_define_gsubr ("svz:server:listeners", 1, 0, 0, guile_server_listeners);
  scm_c_define_gsubr ("svz:sock:receive-buffer", 
		      1, 0, 0, guile_sock_receive_buffer);
  scm_c_define_gsubr ("svz:sock:receive-buffer-reduce",
		      1, 1, 0, guile_sock_receive_buffer_reduce);
  scm_c_define_gsubr ("svz:sock:remote-address",
		      1, 1, 0, guile_sock_remote_address);
  scm_c_define_gsubr ("svz:sock:local-address",
		      1, 1, 0, guile_sock_local_address);
  scm_c_define_gsubr ("svz:sock:idle-counter",
		      1, 1, 0, guile_sock_idle_counter);

  DEFINE_SOCK_CALLBACK ("svz:sock:disconnected",disconnected_socket);
  DEFINE_SOCK_CALLBACK ("svz:sock:kicked",kicked_socket);
  DEFINE_SOCK_CALLBACK ("svz:sock:idle",idle_func);
}

/* Finalize the API functions. */
void
guile_api_finalize (void)
{
}

#endif /* ENABLE_GUILE_SERVER */
