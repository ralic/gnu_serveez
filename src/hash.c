/*
 * src/hash.c - hash table functions
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: hash.c,v 1.14 2001/01/24 15:55:28 ela Exp $
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
# define svz_free(ptr) svz_free_func (ptr)
# define svz_malloc(size) svz_malloc_func (size)
# define svz_realloc(ptr, size) svz_realloc_func (ptr, size)
#endif /* DEBUG_MEMORY_LEAKS */

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

  if (key1 == key2)
    return 0;
  
  p1 = key1;
  p2 = key2;

  while (*p1 && *p2)
    {
      if (*p1 != *p2)
	return -1;
      p1++;
      p2++;
    }

  if (!*p1 && !*p2)
    return 0;
  return -1;
}

/*
 * This is the default routine for determining the actual key length.
 */
unsigned
hash_key_length (char *key)
{
  unsigned len = 0;

  assert (key);
  while (*key++)
    len++;
  len++;

  return len;
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
  for (n = size, size = 1; n != 1; n >>= 1) 
    size <<= 1;
  if (size < HASH_MIN_SIZE)
    size = HASH_MIN_SIZE;

  /* allocate space for the hash itself */
  hash = svz_malloc (sizeof (hash_t));
  hash->buckets = size;
  hash->fill = 0;
  hash->keys = 0;
  hash->code = hash_code;
  hash->equals = hash_key_equals;
  hash->keylen = hash_key_length;

  /* allocate space for the hash table and initialize it */
  hash->table = svz_malloc (sizeof (hash_bucket_t) * size);
  for (n = 0; n < size; n++)
    {
      hash->table[n].size = 0;
      hash->table[n].entry = NULL;
    }

  return hash;
}

/*
 * Destroy an existing hash. Therefore we svz_free() all keys within the hash,
 * the hash table and the hash itself. The values keep untouched. So you
 * might want to svz_free() them yourself.
 */
void
hash_destroy (hash_t *hash)
{
  int n, e;
  hash_bucket_t *bucket;

  for (n = 0; n < hash->buckets; n++)
    {
      bucket = &hash->table[n];
      if (bucket->size)
	{
	  for (e = 0; e < bucket->size; e++)
	    {
	      svz_free (bucket->entry[e].key);
	    }
	  svz_free (bucket->entry);
	}
    }
  svz_free (hash->table);
  svz_free (hash);
}

/*
 * Clear the hash table of a given hash. Afterwards it does not contains any
 * key. In contradiction to hash_destroy() this functions does not destroy
 * the hash itself. But shrinks it to a minimal size.
 */
void
hash_clear (hash_t *hash)
{
  hash_bucket_t *bucket;
  int n, e;

  /* go through all buckets of the table and delete its entries */
  for (n = 0; n < hash->buckets; n++)
    {
      bucket = &hash->table[n];
      if (bucket->size)
	{
	  for (e = 0; e < bucket->size; e++)
	    {
	      svz_free (bucket->entry[e].key);
	    }
	  svz_free (bucket->entry);
	  bucket->entry = NULL;
	  bucket->size = 0;
	}
    }

  /* reinitialize the hash table */
  hash->buckets = HASH_MIN_SIZE;
  hash->fill = 0;
  hash->keys = 0;
  hash->table = svz_realloc (hash->table, 
			     sizeof (hash_bucket_t) * hash->buckets);
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
  hash_bucket_t *bucket, *next_bucket;

#if 0
  hash_analyse (hash);
#endif /* ENABLE_DEBUG */

  if (type == HASH_EXPAND)
    {
      /*
       * Reallocate and initialize the hash table itself.
       */
      hash->buckets <<= 1;
      hash->table = svz_realloc (hash->table, 
				 sizeof (hash_bucket_t) * hash->buckets);
      for (n = hash->buckets >> 1; n < hash->buckets; n++)
	{
	  hash->table[n].size = 0;
	  hash->table[n].entry = NULL;
	}

      /*
       * Go through all hash table entries and check if it is necessary
       * to relocate them.
       */
      for (n = 0; n < (hash->buckets >> 1); n++)
	{
	  bucket = &hash->table[n];
	  for (e = 0; e < bucket->size; e++)
	    {
	      if ((unsigned long) n != 
		  HASH_BUCKET (bucket->entry[e].code, hash))
		{
		  /* copy this entry to the far entry */
		  next_bucket = 
		    &hash->table[HASH_BUCKET (bucket->entry[e].code, hash)];
		  next_bucket->entry = svz_realloc (next_bucket->entry,
						    (next_bucket->size + 1) *
						    sizeof (hash_entry_t));
		  next_bucket->entry[next_bucket->size] = bucket->entry[e];
		  next_bucket->size++;
		  if (next_bucket->size == 1)
		    hash->fill++;
	      
		  /* delete this entry */
		  bucket->size--;
		  if (bucket->size == 0)
		    {
		      svz_free (bucket->entry);
		      bucket->entry = NULL;
		      hash->fill--;
		    }
		  else
		    {
		      bucket->entry[e] = bucket->entry[bucket->size];
		      bucket->entry = svz_realloc (bucket->entry,
						   bucket->size *
						   sizeof (hash_entry_t));
		    }
		  e--;
		}
	    }
	}
    }
  else if (type == HASH_SHRINK && hash->buckets > HASH_MIN_SIZE)
    {
      hash->buckets >>= 1;
      for (n = hash->buckets; n < hash->buckets << 1; n++)
	{
	  bucket = &hash->table[n];
	  if (bucket->size)
	    {
	      for (e = 0; e < bucket->size; e++)
		{
		  next_bucket = 
		    &hash->table[HASH_BUCKET (bucket->entry[e].code, hash)];
		  next_bucket->entry = svz_realloc (next_bucket->entry,
						    (next_bucket->size + 1) *
						    sizeof (hash_entry_t));
		  next_bucket->entry[next_bucket->size] = bucket->entry[e];
		  next_bucket->size++;
		  if (next_bucket->size == 1)
		    hash->fill++;
		}
	      svz_free (bucket->entry);
	    }
	  hash->fill--;
	}
      hash->table = svz_realloc (hash->table, 
				 sizeof (hash_bucket_t) * hash->buckets);
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
  hash_bucket_t *bucket;

  code = hash->code (key);

  /* Check if the key is already stored. If so replace the value. */
  bucket = &hash->table[HASH_BUCKET (code, hash)];
  for (e = 0; e < bucket->size; e++)
    {
      if (bucket->entry[e].code == code &&
	  hash->equals (bucket->entry[e].key, key) == 0)
	{
	  bucket->entry[e].value = value;
	  return;
	}
    }

  /* Reallocate this bucket. */
  bucket = &hash->table[HASH_BUCKET (code, hash)];
  bucket->entry = svz_realloc (bucket->entry, 
			       sizeof (hash_entry_t) * (bucket->size + 1));

  /* Fill this entry. */
  entry = &bucket->entry[bucket->size];
  entry->key = svz_malloc (hash->keylen (key));
  memcpy (entry->key, key, hash->keylen (key));
  entry->value = value;
  entry->code = code;
  bucket->size++;
  hash->keys++;

  /* 75% filled ? */
  if (bucket->size == 1)
    {
      hash->fill++;
      if (hash->fill > HASH_EXPAND_LIMIT (hash))
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
  hash_bucket_t *bucket;
  void *value;

  code = hash->code (key);
  bucket = &hash->table[HASH_BUCKET (code, hash)];
  
  for (n = 0; n < bucket->size; n++)
    {
      if (bucket->entry[n].code == code && 
	  hash->equals (bucket->entry[n].key, key) == 0)
	{
	  value = bucket->entry[n].value;
	  bucket->size--;
	  svz_free (bucket->entry[n].key);
	  if (bucket->size)
	    {
	      bucket->entry[n] = bucket->entry[bucket->size];
	      bucket->entry = svz_realloc (bucket->entry, 
					   sizeof (hash_entry_t) * 
					   bucket->size);
	    }
	  else
	    {
	      svz_free (bucket->entry);
	      bucket->entry = NULL;
	      hash->fill--;
	      if (hash->fill < HASH_SHRINK_LIMIT (hash))
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
  hash_bucket_t *bucket;

  code = hash->code (key);
  bucket = &hash->table[HASH_BUCKET (code, hash)];
  
  for (n = 0; n < bucket->size; n++)
    {
      if (bucket->entry[n].code == code && 
	  hash->equals (bucket->entry[n].key, key) == 0)
	{
	  return bucket->entry[n].value;
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
  hash_bucket_t *bucket;
  int n, e, keys;

  if (hash->keys == 0)
    return NULL;

  values = svz_malloc (sizeof (void *) * hash->keys);

  for (keys = 0, n = 0; n < hash->buckets; n++)
    {
      bucket = &hash->table[n];
      for (e = 0; e < bucket->size; e++)
	{
	  values[keys++] = bucket->entry[e].value;
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
  hash_bucket_t *bucket;
  int n, e, keys;

  if (hash->keys == 0)
    return NULL;

  values = svz_malloc (sizeof (void *) * hash->keys);

  for (keys = 0, n = 0; n < hash->buckets; n++)
    {
      bucket = &hash->table[n];
      for (e = 0; e < bucket->size; e++)
	{
	  values[keys++] = bucket->entry[e].key;
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
  return hash->buckets;
}

/*
 * This function can be used to determine if some key maps to the value 
 * argument in this hash table. Returns the appropriate key or NULL.
 */
char *
hash_contains (hash_t *hash, void *value)
{
  hash_bucket_t *bucket;
  int n, e;

  for (n = 0; n < hash->buckets; n++)
    {
      bucket = &hash->table[n];
      for (e = 0; e < bucket->size; e++)
	{
	  if (bucket->entry[e].value == value)
	    return bucket->entry[e].key;
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
  hash_bucket_t *bucket;
  int n, e, buckets, depth, entries;

  for (entries = 0, depth = 0, buckets = 0, n = 0; n < hash->buckets; n++)
    {
      bucket = &hash->table[n];
      if (bucket->size > 0)
	buckets++;
      for (e = 0; e < bucket->size; e++)
	{
	  entries++;
#if 0
	  printf ("bucket %04d: entry %02d: code: %08lu "
		  "value: %p key: %-20s\n",
		  n + 1, e + 1, bucket->entry[e].code, bucket->entry[e].value,
		  bucket->entry[e].key);
#endif /* 0 */
	  if (e > depth)
	    depth = e;
	}
    }
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, 
	      "%d/%d buckets (%d), %d entries (%d), depth: %d\n",
	      buckets, hash->buckets, hash->fill, 
	      entries, hash->keys, depth + 1);
#endif /* ENABLE_DEBUG */
}
