/*
 * portcfg.c - port configuration implementation
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
 * $Id: portcfg.c,v 1.8 2001/04/28 12:37:06 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <assert.h>
#include <string.h>
#include <sys/types.h>

#include "libserveez/alloc.h"
#include "libserveez/util.h"
#include "libserveez/core.h"
#include "libserveez/vector.h"
#include "libserveez/array.h"
#include "libserveez/portcfg.h"
#include "libserveez/interface.h"

/*
 * This hash holds all port configurations created by the configuration
 * file.
 */
static svz_hash_t *svz_portcfgs = NULL;

/*
 * Check if two given port configurations structures are equal i.e. 
 * specifying the same network port or files. Returns non-zero if @var{a} 
 * and @var{b} are equal.
 */
int
svz_portcfg_equal (svz_portcfg_t *a, svz_portcfg_t *b)
{
  if ((a->proto & (PROTO_TCP | PROTO_UDP | PROTO_ICMP | PROTO_RAW)) &&
      (a->proto == b->proto))
    {
      /* 
       * Two network ports are equal if both local port and address 
       * are equal.
       */
      switch (a->proto)
	{
	case PROTO_TCP:
	  if (a->tcp_port == b->tcp_port &&
	      !strcmp (a->tcp_ipaddr, b->tcp_ipaddr))
	    return 1;
	  break;
	case PROTO_UDP:
	  if (a->udp_port == b->udp_port &&
	      !strcmp (a->udp_ipaddr, b->udp_ipaddr))
	    return 1;
	  break;
	case PROTO_ICMP:
	  if (!strcmp (a->icmp_ipaddr, b->icmp_ipaddr))
	    return 1;
	  break;
	case PROTO_RAW:
	  if (!strcmp (a->raw_ipaddr, b->raw_ipaddr))
	    return 1;
	  break;
	}
    } 
  else if (a->proto & PROTO_PIPE && a->proto == b->proto) 
    {
      /* 
       * Two pipe configs are equal if they use the same files.
       */
      if (!strcmp (a->pipe_recv.name, b->pipe_recv.name) && 
	  !strcmp (b->pipe_send.name, b->pipe_send.name))
	return 1;
    } 

  /* Do not even the same proto flag -> cannot be equal. */
  return 0;
}

/*
 * Add the given port configuration @var{port} associated with the name
 * @var{name} to the list of known port configurations. Return @code{NULL}
 * on errors. If the return port configuration equals the given port
 * configuration  the given one has been successfully added.
 */
svz_portcfg_t *
svz_portcfg_add (char *name, svz_portcfg_t *port)
{
  svz_portcfg_t *replace;

  /* Do not handle invalid arguments. */
  if (name == NULL || port == NULL)
    return NULL;

  /* Check if the port configuration hash is inited. */
  if (svz_portcfgs == NULL)
    {
      if ((svz_portcfgs = svz_hash_create (4)) == NULL)
	return NULL;
    }

  /* Try adding a new port configuration. */
  if ((replace = svz_hash_get (svz_portcfgs, name)) != NULL)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "portcfg `%s' already registered\n", name);
#endif
      svz_hash_put (svz_portcfgs, name, port);
      return replace;
    }
  svz_hash_put (svz_portcfgs, name, port);
  return port;
}

/*
 * Remove the named port configuration identified by @var{name} from the
 * list of known port configurations. Return @code{NULL} on errors or 
 * otherwise the port configuration associated with @var{name}.
 */
svz_portcfg_t *
svz_portcfg_del (char *name)
{
  svz_portcfg_t *port;

  /* List of port configurations is empty. */
  if (svz_portcfgs == NULL || name == NULL)
    return NULL;

  /* Actually remove it from the list. */
  return svz_hash_delete (svz_portcfgs, name);
}

/*
 * This function can be used to set the character string representation
 * of a the port configuration @var{this} in dotted decimal form
 * (@var{ipaddr}). Returns zero on success, non-zero otherwise.
 */
int
svz_portcfg_set_ipaddr (svz_portcfg_t *this, char *ipaddr)
{
  if (!this || !ipaddr)
    return -1;

  switch (this->proto)
    {
    case PROTO_TCP:
      svz_free_and_zero (this->tcp_ipaddr);
      this->tcp_ipaddr = ipaddr;
      break;
    case PROTO_UDP:
      svz_free_and_zero (this->udp_ipaddr);
      this->udp_ipaddr = ipaddr;
      break;
    case PROTO_ICMP:
      svz_free_and_zero (this->icmp_ipaddr);
      this->icmp_ipaddr = ipaddr;
      break;
    case PROTO_RAW:
      svz_free_and_zero (this->raw_ipaddr);
      this->raw_ipaddr = ipaddr;
      break;
    default:
      return -1;
    }
  return 0;
}

/*
 * Expand the given port configuration @var{this} if it is a network port
 * configuration and if the network ip address is @code{INADDR_ANY}. Return
 * an array of port configurations which are copies of the given.
 */
svz_array_t *
svz_portcfg_expand (svz_portcfg_t *this)
{
  svz_array_t *ports = svz_array_create (1);
  svz_portcfg_t *port;
  struct sockaddr_in *addr;
  int n;
  ifc_entry_t *ifc;

  /* Is this a network port configuration and should it be expanded ? */
  if ((addr = svz_portcfg_addr (this)) != NULL && 
      addr->sin_addr.s_addr == INADDR_ANY)
    {
      svz_vector_foreach (svz_interfaces, ifc, n)
	{
	  port = svz_portcfg_copy (this);
	  addr = svz_portcfg_addr (port);
	  addr->sin_addr.s_addr = ifc->ipaddr;
	  svz_portcfg_set_ipaddr (port, 
				  svz_strdup (svz_inet_ntoa (ifc->ipaddr)));
	  svz_array_add (ports, port);
	}
    }
  /* No, just add the given port configuration. */
  else
    {
      port = svz_portcfg_copy (this);
      svz_array_add (ports, port);
    }
  return ports;
}  

/*
 * Make a copy of the given port configuration @var{port}. This function
 * is used in @code{svz_portcfg_expand}.
 */
svz_portcfg_t *
svz_portcfg_copy (svz_portcfg_t *port)
{
  svz_portcfg_t *copy;

  /* First plain copy. */
  copy = svz_malloc (sizeof (svz_portcfg_t));
  memcpy (copy, port, sizeof (svz_portcfg_t));
  copy->name = svz_strdup (port->name);

  /* Depending on the protocol, copy various strings. */
  switch (port->proto)
    {
    case PROTO_TCP:
      copy->tcp_ipaddr = svz_strdup (port->tcp_ipaddr);
      break;
    case PROTO_UDP:
      copy->udp_ipaddr = svz_strdup (port->udp_ipaddr);
      break;
    case PROTO_ICMP:
      copy->icmp_ipaddr = svz_strdup (port->icmp_ipaddr);
      break;
    case PROTO_RAW:
      copy->raw_ipaddr = svz_strdup (port->raw_ipaddr);
      break;
    case PROTO_PIPE:
      copy->pipe_recv.name = svz_strdup (port->pipe_recv.name);
      copy->pipe_recv.user = svz_strdup (port->pipe_recv.user);
      copy->pipe_recv.group = svz_strdup (port->pipe_recv.group);
      copy->pipe_send.name = svz_strdup (port->pipe_send.name);
      copy->pipe_send.user = svz_strdup (port->pipe_send.user);
      copy->pipe_send.group = svz_strdup (port->pipe_send.group);
      break;
    }
  return copy;
}

/*
 * This function makes the given port configuration @var{port} completely 
 * unusable. 
 */
void
svz_portcfg_destroy (svz_portcfg_t *port)
{
  if (port == NULL)
    return;

  svz_free_and_zero (port->name);
  switch (port->proto)
    {
    case PROTO_TCP:
      svz_free_and_zero (port->tcp_ipaddr);
      break;
    case PROTO_UDP:
      svz_free_and_zero (port->udp_ipaddr);
      break;
    case PROTO_ICMP:
      svz_free_and_zero (port->icmp_ipaddr);
      break;
    case PROTO_RAW:
      svz_free_and_zero (port->raw_ipaddr);
      break;
    case PROTO_PIPE:
      svz_free_and_zero (port->pipe_recv.user);
      svz_free_and_zero (port->pipe_recv.name);
      svz_free_and_zero (port->pipe_recv.group);
      svz_free_and_zero (port->pipe_send.user);
      svz_free_and_zero (port->pipe_send.name);
      svz_free_and_zero (port->pipe_send.group);
      break;
    }

  /* FIXME: What about the values ? */
  if (port->deny)
    svz_array_destroy (port->deny);
  if (port->allow)
    svz_array_destroy (port->allow);
  if (port->accepted)
    svz_hash_destroy (port->accepted);
}

/*
 * Return the port configuration assosciated with the given name @var{name}.
 * This function returns @code{NULL} on errors.
 */
svz_portcfg_t *
svz_portcfg_get (char *name)
{
  /* Do not handle invalid arguments. */
  if (name == NULL || svz_portcfgs == NULL)
    return NULL;

  return svz_hash_get (svz_portcfgs, name);
}

/*
 * Delete the list of known port configurations. This routine should 
 * definitely called from the core library's finalizer.
 */
void
svz_portcfg_finalize (void)
{
  svz_portcfg_t **port;
  int n;

  if (svz_portcfgs != NULL)
    {
      svz_hash_foreach_value (svz_portcfgs, port, n)
	{
	  svz_portcfg_destroy (port[n]);
	  svz_free (port[n]);
	}
      svz_hash_destroy (svz_portcfgs);
      svz_portcfgs = NULL;
    }
}

/*
 * Construct the @code{sockaddr_in} fields from the @code{ipaddr} field.
 * Returns zero if it worked. If it does not work the @code{ipaddr} field
 * did not consist of an ip address in dotted decimal form.
 */
int
svz_portcfg_mkaddr (svz_portcfg_t *this)
{
  int err = 0;

  switch (this->proto)
    {
    case PROTO_TCP:
      /* put ip address here */
      err = svz_inet_aton (this->tcp_ipaddr, &this->tcp_addr);
      /* this surely is internet */
      this->tcp_addr.sin_family = AF_INET;
      /* determine network port */
      this->tcp_addr.sin_port = htons (this->tcp_port);
      break;
    case PROTO_UDP:
      err = svz_inet_aton (this->udp_ipaddr, &this->udp_addr);
      this->udp_addr.sin_family = AF_INET;
      this->udp_addr.sin_port = htons (this->tcp_port);
      break;
    case PROTO_ICMP:
      err = svz_inet_aton (this->icmp_ipaddr, &this->icmp_addr);
      this->icmp_addr.sin_family = AF_INET;
      break;
    case PROTO_RAW:
      err = svz_inet_aton (this->raw_ipaddr, &this->raw_addr);
      this->raw_addr.sin_family = AF_INET;
      break;
    case PROTO_PIPE:
      err = 0;
      break;
    default:
      err = 0;
    }
  return err;
}

/*
 * Debug helper: Emit a printable representation of the the given
 * port configuration to the given FILE stream.
 */
void
svz_portcfg_print (svz_portcfg_t *this, FILE *stream)
{
  if (NULL == this)
    {
      fprintf (stream, "portcfg is NULL !\n");
      return;
    }

  switch (this->proto)
    {
    case PROTO_TCP:
      fprintf (stream, "portcfg `%s': TCP (%s|%s):%d\n", this->name,
	       this->tcp_ipaddr,
	       svz_inet_ntoa (this->tcp_addr.sin_addr.s_addr),
	       this->tcp_port);
      break;
    case PROTO_UDP:
      fprintf (stream, "portcfg `%s': UDP (%s|%s):%d\n", this->name,
	       this->udp_ipaddr,
	       svz_inet_ntoa (this->udp_addr.sin_addr.s_addr),
	       this->udp_port);
      break;
    case PROTO_ICMP:
      fprintf (stream, "portcfg `%s': ICMP (%s|%s)\n", this->name,
	       this->icmp_ipaddr,
	       svz_inet_ntoa (this->icmp_addr.sin_addr.s_addr));
      break;
    case PROTO_RAW:
      fprintf (stream, "portcfg `%s': RAW (%s|%s)\n", this->name,
	       this->raw_ipaddr,
	       svz_inet_ntoa (this->raw_addr.sin_addr.s_addr));
      break;
    case PROTO_PIPE:
      /* FIXME: implement me */
      fprintf (stream, "portcfg `%s': PIPE but unimplemented\n", this->name);
      break;
    default:
      fprintf (stream, "portcfg with invalid proto field %d\n", this->proto);
    }
}
