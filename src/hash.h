/*
 * src/hash.h - hash function interface
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: hash.h,v 1.3 2000/07/25 16:24:26 ela Exp $
 *
 */

#ifndef __HASH_H__
#define __HASH_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define HASH_SHRINK 4
#define HASH_EXPAND 8
#define HASH_SHRINK_LIMIT(hash) (hash->nodes >> 2)
#define HASH_EXPAND_LIMIT(hash) ((hash->nodes >> 1) + (hash->nodes >> 2))
#define HASH_MIN_SIZE 4
#define HASH_NODE(code, hash) (code & (hash->nodes - 1))

/*
 * This is the basic structure of a hash entry consisting of its
 * key, the actual value stored in the hash table and the hash code
 * of the key.
 */
typedef struct
{
  unsigned long code;
  char *key;
  void *value;
}
hash_entry_t;

/*
 * The hash table consists of different hash nodes. This contains the
 * node's size and the entry array.
 */
typedef struct
{
  int size;
  hash_entry_t *entry;
}
hash_bucket_t;

/*
 * This structure keeps information on a specific hash table.
 */
typedef struct
{
  int nodes;                       /* number of nodes in the table */
  int fill;                        /* number of filled nodes */
  int keys;                        /* number of stored keys */
  int (* equals) (char *, char *); /* key string equality callback */
  unsigned long (* code) (char *); /* hash code calculation callback */
  hash_bucket_t *table;            /* hash table */
}
hash_t;

/*
 * Basic hash functions.
 */
hash_t *hash_create (int size);
void hash_destroy (hash_t *hash);
void hash_clear (hash_t *hash);
void *hash_delete (hash_t *hash, char *key);
void hash_put (hash_t *hash, char *key, void *value);
void *hash_get (hash_t *hash, char *key);
void **hash_values (hash_t *hash);
char **hash_keys (hash_t *hash);
int hash_size (hash_t *hash);
int hash_capacity (hash_t *hash);
char *hash_contains (hash_t *hash, void *value);
void hash_analyse (hash_t *hash);

#if DEBUG_MEMORY_LEAKS
# include <stdlib.h>
# define hash_xfree(ptr) free (ptr)
#else
# define hash_xfree(ptr) xfree (ptr);
#endif

#endif /* not __HASH_H__ */
