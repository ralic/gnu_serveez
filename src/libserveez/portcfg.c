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
 * $Id: portcfg.c,v 1.1 2001/04/04 14:23:14 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <assert.h>

#include "libserveez/util.h"
#include "libserveez/core.h"
#include "libserveez/portcfg.h"

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
  if ((a->type & (PROTO_TCP | PROTO_UDP | PROTO_ICMP | PROTO_RAW)) &&
      (a->type == b->type))
    {
      /* 
       * Two network ports are equal if both local port and address 
       * are equal.
       */
      switch (a->type)
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
  else if (a->type & PROTO_PIPE && a->type == b->type) 
    {
      /* 
       * Two pipe configs are equal if they use the same files.
       */
      if (!strcmp (a->pipe_recv->name, b->pipe_recv->name) && 
	  !strcmp (b->pipe_send->name, b->pipe_send->name))
	return 1;
    } 

  /* Do not even the same proto flag -> cannot be equal. */
  return 0;
}

/*
 * Add the given port configuration @var{port} associated with the name
 * @var{name} to the list of known port configurations. Return @code{NULL}
 * on errors.
 */
svz_portcfg_t *
svz_portcfg_add (char *name, svz_portcfg_t *port)
{
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
  if (svz_hash_get (svz_portcfgs, name))
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "portcfg `%s' already registered\n", name);
#endif
      return NULL;
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
  if (svz_portcfgs != NULL)
    svz_hash_destroy (svz_portcfgs);
  svz_portcfgs = NULL;
}
