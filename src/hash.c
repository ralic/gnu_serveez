/*
 * src/hash.c - hash table functions
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
 * $Id: hash.c,v 1.9 2000/08/18 14:14:47 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "util.h"
#include "hash.h"

#if DEBUG_MEMORY_LEAKS
# define xfree(ptr) free (ptr)
# define xmalloc(size) malloc (size)
# define xrealloc(ptr, size) realloc (ptr, size)
#endif

/*
 * Calculate the hashcode for a given key. This is the standard callback
 * for any newly created hash.
 */
unsigned long
hash_code (char *key)
{
  unsigned long code = 0;
  char *p = key;

  assert (key);

  while (*p)
    {
      code = (code << 1) ^ *p;
      p++;
    }
  return code;
}

/*
 * This is the default callback for any newly created hash for determining
 * two keys being equal. Return zero if both strings are equal, otherwise
 * non-zero.
 */
int
hash_key_equals (char *key1, char *key2)
{
  char *p1, *p2;

  assert (key1 && key2);

  if (key1 == key2) return 0;
  
  p1 = key1;
  p2 = key2;

  while (*p1 && *p2)
    {
      if (*p1 != *p2) return -1;
      p1++;
      p2++;
    }

  if(!*p1 && !*p2) return 0;
  return -1;
}

/*
 * Create a new hash table with an initial capacity. Return a non-zero 
 * pointer to the newly created hash.
 */
hash_t *
hash_create (int size)
{
  int n;
  hash_t *hash;

  /* set initial hash table size to a binary value */
  for (n = size, size = 1; n != 1; n >>= 1)  size <<= 1;
  if (size < HASH_MIN_SIZE) size = HASH_MIN_SIZE;

  /* allocate space for the hash itself */
  hash = xmalloc (sizeof (hash_t));
  hash->nodes = size;
  hash->fill = 0;
  hash->keys = 0;
  hash->code = hash_code;
  hash->equals = hash_key_equals;

  /* allocate space for the hash table and initialize it */
  hash->table = xmalloc (sizeof (hash_bucket_t) * size);
  for (n = 0; n < size; n++)
    {
      hash->table[n].size = 0;
      hash->table[n].entry = NULL;
    }

  return hash;
}

/*
 * Destroy an existing hash. Therefore we xfree() all keys within the hash,
 * the hash table and the hash itself. The values keep untouched. So you
 * might want to xfree() them yourself.
 */
void
hash_destroy (hash_t *hash)
{
  int n, e;
  hash_bucket_t *node;

  for (n = 0; n < hash->nodes; n++)
    {
      node = &hash->table[n];
      if (node->size)
	{
	  for (e = 0; e < node->size; e++)
	    {
	      xfree (node->entry[e].key);
	    }
	  xfree (node->entry);
	}
    }
  xfree (hash->table);
  xfree (hash);
}

/*
 * Clear the hash table of a given hash. Afterwards it does not contains any
 * key. In contradiction to hash_destroy() this functions does not destroy
 * the hash itself. But shrinks it to a minimal size.
 */
void
hash_clear (hash_t *hash)
{
  hash_bucket_t *node;
  int n, e;

  /* go through all nodes of the table and delete its entries */
  for (n = 0; n < hash->nodes; n++)
    {
      node = &hash->table[n];
      if (node->size)
	{
	  for (e = 0; e < node->size; e++)
	    {
	      xfree (node->entry[e].key);
	    }
	  xfree (node->entry);
	  node->entry = NULL;
	  node->size = 0;
	}
    }

  /* reinitialize the hash table */
  hash->nodes = HASH_MIN_SIZE;
  hash->fill = 0;
  hash->keys = 0;
  hash->table = xrealloc (hash->table, sizeof (hash_bucket_t) * hash->nodes);
}

/*
 * Rehash a given hash. Double its size and expand the hashcodes or half 
 * its size and shrink the hashcodes if these would be placed somewhere 
 * else.
 */
void
hash_rehash (hash_t *hash, int type)
{
  int n, e;
  hash_bucket_t *node, *next_node;

#if 0
  hash_analyse (hash);
#endif /* ENABLE_DEBUG */

  if (type == HASH_EXPAND)
    {
      /*
       * Reallocate and initialize the hash table itself.
       */
      hash->nodes <<= 1;
      hash->table = xrealloc (hash->table, 
			      sizeof (hash_bucket_t) * hash->nodes);
      for (n = hash->nodes >> 1; n < hash->nodes; n++)
	{
	  hash->table[n].size = 0;
	  hash->table[n].entry = NULL;
	}

      /*
       * Go through all hash table entries and check if it is necessary
       * to relocate them.
       */
      for (n = 0; n < (hash->nodes >> 1); n++)
	{
	  node = &hash->table[n];
	  for (e = 0; e < node->size; e++)
	    {
	      if ((unsigned long)n != HASH_NODE(node->entry[e].code, hash))
		{
		  /* copy this entry to the far entry */
		  next_node = 
		    &hash->table[HASH_NODE(node->entry[e].code, hash)];
		  next_node->entry = xrealloc (next_node->entry,
					       (next_node->size + 1) *
					       sizeof (hash_entry_t));
		  next_node->entry[next_node->size] = node->entry[e];
		  next_node->size++;
		  if (next_node->size == 1) hash->fill++;
	      
		  /* delete this entry */
		  node->size--;
		  if (node->size == 0)
		    {
		      xfree (node->entry);
		      node->entry = NULL;
		      hash->fill--;
		    }
		  else
		    {
		      node->entry[e] = node->entry[node->size];
		      node->entry = xrealloc (node->entry,
					      node->size *
					      sizeof (hash_entry_t));
		    }
		  e--;
		}
	    }
	}
    }
  else if (type == HASH_SHRINK && hash->nodes > HASH_MIN_SIZE)
    {
      hash->nodes >>= 1;
      for (n = hash->nodes; n < hash->nodes << 1; n++)
	{
	  node = &hash->table[n];
	  if (node->size)
	    {
	      for (e = 0; e < node->size; e++)
		{
		  next_node = 
		    &hash->table[HASH_NODE (node->entry[e].code, hash)];
		  next_node->entry = xrealloc (next_node->entry,
					       (next_node->size + 1) *
					       sizeof (hash_entry_t));
		  next_node->entry[next_node->size] = node->entry[e];
		  next_node->size++;
		  if (next_node->size == 1) hash->fill++;
		}
	      xfree (node->entry);
	    }
	  hash->fill--;
	}
      hash->table = xrealloc (hash->table, 
			      sizeof (hash_bucket_t) * hash->nodes);
    }

#if 0
  hash_analyse (hash);
#endif /* ENABLE_DEBUG */
}

/*
 * This function adds a new element consisting of KEY and VALUE to an
 * existing hash. If the hash is 75% filled it gets rehashed (size will
 * be doubled). When the key already exists then the value just gets
 * replaced.
 */
void
hash_put (hash_t *hash, char *key, void *value)
{
  unsigned long code = 0;
  int e;
  hash_entry_t *entry;
  hash_bucket_t *node;

  code = hash->code (key);

  /* Check if the key is already stored. If so replace the value. */
  node = &hash->table[HASH_NODE(code, hash)];
  for (e = 0; e < node->size; e++)
    {
      if (node->entry[e].code == code &&
	  hash->equals (node->entry[e].key, key) == 0)
	{
	  node->entry[e].value = value;
	  return;
	}
    }

  /* Reallocate this node. */
  node = &hash->table[HASH_NODE(code, hash)];
  node->entry = xrealloc (node->entry, 
			  sizeof (hash_entry_t) * (node->size + 1));

  /* Fill this entry. */
  entry = &node->entry[node->size];
  entry->key = xmalloc (strlen (key) + 1);
  strcpy (entry->key, key);
  entry->value = value;
  entry->code = code;
  node->size++;
  hash->keys++;

  /* 75% filled ? */
  if (node->size == 1)
    {
      hash->fill++;
      if (hash->fill > HASH_EXPAND_LIMIT(hash))
	{
	  hash_rehash (hash, HASH_EXPAND);
	}
    }
}

/*
 * Delete an existing hash entry accessed via a given key. Return NULL if
 * the key has not been found within the hash, otherwise the VALUE.
 */
void *
hash_delete (hash_t *hash, char *key)
{
  int n;
  unsigned long code;
  hash_bucket_t *node;
  void *value;

  code = hash->code (key);
  node = &hash->table[HASH_NODE(code, hash)];
  
  for (n=0; n<node->size; n++)
    {
      if (node->entry[n].code == code && 
	  hash->equals(node->entry[n].key, key) == 0)
	{
	  value = node->entry[n].value;
	  node->size--;
	  xfree (node->entry[n].key);
	  if (node->size)
	    {
	      node->entry[n] = node->entry[node->size];
	      node->entry = xrealloc (node->entry, 
				      sizeof (hash_entry_t) * node->size);
	    }
	  else
	    {
	      xfree (node->entry);
	      node->entry = NULL;
	      hash->fill--;
	      if (hash->fill < HASH_SHRINK_LIMIT(hash))
		{
		  hash_rehash (hash, HASH_SHRINK);
		}
	    }
	  hash->keys--;
	  return value;
	}
    }

  return NULL;
}

/*
 * Hash lookup. Find a VALUE for a given KEY. Return NULL if the key has
 * not been found within the hash table.
 */
void *
hash_get (hash_t *hash, char *key)
{
  int n;
  unsigned long code;
  hash_bucket_t *node;

  code = hash->code (key);
  node = &hash->table[HASH_NODE(code, hash)];
  
  for (n = 0; n < node->size; n++)
    {
      if (node->entry[n].code == code && 
	  hash->equals (node->entry[n].key, key) == 0)
	{
	  return node->entry[n].value;
	}
    }

  return NULL;
}

/*
 * This function delivers all values within a hash table. It returns NULL 
 * if there were no values in the hash. You MUST hash_xfree() a non-NULL 
 * return value.
 */
void **
hash_values (hash_t *hash)
{
  void **values;
  hash_bucket_t *node;
  int n, e, keys;

  if (hash->keys == 0)
    return NULL;

  values = xmalloc (sizeof (void *) * hash->keys);

  for (keys = 0, n = 0; n < hash->nodes; n++)
    {
      node = &hash->table[n];
      for (e = 0; e < node->size; e++)
	{
	  values[keys++] = node->entry[e].value;
	  if (keys == hash->keys)
	    return values;
	}
    }
  return values;
}

/*
 * This function delivers all keys within a hash table. It returns NULL 
 * if there were no keys in the hash. You MUST hash_xfree() a non-NULL 
 * return value.
 */
char **
hash_keys (hash_t *hash)
{
  char **values;
  hash_bucket_t *node;
  int n, e, keys;

  if (hash->keys == 0)
    return NULL;

  values = xmalloc (sizeof (void *) * hash->keys);

  for (keys = 0, n = 0; n < hash->nodes; n++)
    {
      node = &hash->table[n];
      for (e = 0; e < node->size; e++)
	{
	  values[keys++] = node->entry[e].key;
	  if (keys == hash->keys)
	    return values;
	}
    }
  return values;
}

/*
 * This routine delivers the number of keys in the hash's hash table.
 */
int
hash_size (hash_t *hash)
{
  return hash->keys;
}

/*
 * This function returns the current capacity of a given hash.
 */
int
hash_capacity (hash_t *hash)
{
  return hash->nodes;
}

/*
 * This function can be used to determine if some key maps to the value 
 * argument in this hash table. Returns the appropiate key or NULL.
 */
char *
hash_contains (hash_t *hash, void *value)
{
  hash_bucket_t *node;
  int n, e;

  for (n=0; n<hash->nodes; n++)
    {
      node = &hash->table[n];
      for (e=0; e<node->size; e++)
	{
	  if (node->entry[e].value == value)
	    return node->entry[e].key;
	}
    }
  return NULL;
}

/*
 * This routine prints all the hash table. It is for debugging purposes
 * only.
 */
void
hash_analyse (hash_t *hash)
{
  hash_bucket_t *node;
  int n, e, nodes, depth, entries;

  for (entries = 0, depth = 0, nodes = 0, n = 0; n < hash->nodes; n++)
    {
      node = &hash->table[n];
      if (node->size > 0) nodes++;
      for (e=0; e<node->size; e++)
	{
	  entries++;
#if 0
	  printf ("node %04d: entry %02d: code: %08lu value: %p key: %-20s\n",
		  n+1, e+1, node->entry[e].code, node->entry[e].value,
		  node->entry[e].key);
#endif /* 0 */
	  if (e > depth) depth = e;
	}
    }
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, 
	      "%d/%d nodes (%d), %d entries (%d), depth: %d\n",
	      nodes, hash->nodes, hash->fill, entries, hash->keys, depth+1);
#endif /* ENABLE_DEBUG */
}
