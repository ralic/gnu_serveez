/*
 * foo-proto.c - Example server
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.  
 */

#include <stdio.h>

#include "util.h"
#include "hash.h"
#include "alloc.h"
#include "foo-proto.h"
#include "server.h"
#include "server-socket.h"

struct portcfg some_default_port = {
  PROTO_TCP,
  42421,
  NULL,
  NULL
};

int some_default_intarray[] = {
  4,
  1,
  2,
  3
};

char *some_default_strarray[] = {
  "Hello",
  "I",
  "am",
  "a",
  "default",
  "string",
  "array",
  NULL
};

hash_t *some_default_hash = NULL;

/*
 * Demonstrate how our private cfg looks like and provide
 * default values...
 */
struct foo_config mycfg = 
{
  -42,
  some_default_strarray,
  "Default reply",
  some_default_intarray,
  42,
  &some_default_port,
  &some_default_hash
};

/*
 * Prototype of our configuration
 */
struct name_value_pair foo_config_prototype[] = 
{
  REGISTER_INT("bar", mycfg.bar, NOTDEFAULTABLE),
  REGISTER_STR("reply", mycfg.reply, DEFAULTABLE),
  REGISTER_STRARRAY("messages", mycfg.messages, DEFAULTABLE),
  REGISTER_INTARRAY("ports", mycfg.ports, DEFAULTABLE),
  REGISTER_HASH("assoc", mycfg.assoc, DEFAULTABLE),
  REGISTER_PORTCFG("port", mycfg.port, DEFAULTABLE),
  REGISTER_END()
};

/*
 * Definition of this server
 */
struct serverdefinition foo_serverdefinition =
{
  "Foo example server",
  "foo",
  foo_global_init,
  foo_init,
  foo_detect_proto,
  foo_connect_socket,
  &mycfg,
  sizeof(mycfg),
  foo_config_prototype
};

int foo_handle_request(socket_t sock, int len, struct foo_config* cfg)
{
  return sock_printf (sock, "%s: %d\r\n", cfg->reply, len);
}


int
foo_check_request(socket_t sock)
{
  /* One request is a line ending in \r\n
   */
  char *p = sock->recv_buffer;
  int len;
  struct foo_config *cfg = (struct foo_config*) sock->cfg;
  int r = 0;

  while (p < sock->recv_buffer + sock->recv_buffer_fill &&
	 *p != '\n')
    p++;

  len = p - sock->recv_buffer;
  if (*p == '\n') {
    r = foo_handle_request(sock, len, cfg);
    sock_reduce_recv(sock, len + 1);
    
  }

  return r;
}

int
foo_detect_proto(void *cfg, socket_t sock)
{
  /* see if the stream starts with our identification string */
  if ( sock->recv_buffer_fill >= 5 &&
       !memcmp(sock->recv_buffer, "foo\r\n", 5) ) {

    /* it's us: forget the id string and signal success */
    sock_reduce_recv (sock, 5);
    return -1;
  }

  /* not us... */
  return 0;
}

int
foo_connect_socket(void *acfg, socket_t sock)
{
  struct foo_config *cfg = (struct foo_config *)acfg;
  int i;
  int r;
  sock->check_request = foo_check_request;
  log_printf(LOG_NOTICE, "Foo client detected\n");
  
  if (cfg->messages) {
    for (i = 0; cfg->messages[i]; i++)
      {
	r = sock_printf(sock, "%s\r\n", cfg->messages[i]);
	if (r)
	  return r;
      }
  }

  return 0;
}

int
foo_global_init(void)
{
  some_default_hash = hash_create(4);
  hash_put(some_default_hash, "grass", "green");
  hash_put(some_default_hash, "cow", "milk");
  hash_put(some_default_hash, "sun", "light");
  hash_put(some_default_hash, "moon", "tide");
  hash_put(some_default_hash, "gnu", "good");
  return 0;
}

int
foo_init(struct server *server)
{
  struct foo_config *c = (struct foo_config*)server->cfg;
  char **s = c->messages;
  int *j = c->ports;
  int i;
  char **keys;
  hash_t *h;

  printf("Config %s:\n reply = %s\n bar = %d\n",
	 server->name, c->reply, c->bar);

  if ( s != NULL) {
    for ( i = 0; s[i] != NULL; i++)
      {
	printf (" messages[%d] = %s\n", i, s[i]);
      }
  } else {
    printf(" messages = NULL\n");
  }

  if (j != NULL) {
    for ( i = 1; i < j[0]; i++)
      {
	printf (" ports[%d] = %d\n", i, j[i]);
      }
  } else {
    printf(" ports = NULL\n");
  }
  
  h = *(c->assoc);
  if ( h != NULL) {
    keys = hash_keys (h);

    for ( i = 0; i < h->keys; i++)
      {
	printf (" assoc[%d]: `%s' => `%s'\n",
		i, keys[i], (char*)hash_get(h, keys[i]));
      }

    xfree (keys);
  } else {
    printf(" *assoc = NULL\n");
  }

  printf (" Binding on port %d\n", c->port->port);

  if ( c->port->proto != PROTO_TCP ) {
    fprintf (stderr, "Foo server can handle TCP only!\n");
    return -1;
  }

  server_bind (server, c->port);

  return 0;
}
