/*
 * test/hash-test.c - hash table tests
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
 * $Id: hash-test.c,v 1.3 2000/11/22 18:58:22 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "hash.h"
#include "test.h"

extern void hash_rehash (hash_t *, int);

#define REPEAT 10000
#define test(error) \
  if (error) {      \
    test_failed (); \
    result++;       \
  } else {          \
    test_ok ();     \
  }                 \

/*
 * Main entry point for hash tests.
 */
int
main (int argc, char **argv)
{
  int result = 0;
  hash_t *hash;
  long n, error;
  char *text;
  char **keys;
  void **values;

  test_print ("hash function test suite\n");

  /* hash creation */
  test_print ("           create: ");
  test ((hash = hash_create (4)) == NULL);

  /* hash put and get */
  test_print ("      put and get: ");
  for (error = n = 0; n < REPEAT; n++)
    {
      text = xstrdup (test_string ());
      hash_put (hash, text, (void *) 0xdeadbeef);
      if (((void *) 0xdeadbeef != hash_get (hash, text)))
	error++;
      xfree (text);
    }
  test (error);

  /* hash containing a certain value */
  test_print ("         contains: ");
  error = 0;
  if (hash_contains (hash, NULL))
    error++;
  if (hash_contains (hash, (void *) 0xeabceabc))
    error++;
  hash_put (hash, "1234567890", (void *) 0xeabceabc);
  if (strcmp ("1234567890", hash_contains (hash, (void *) 0xeabceabc)))
    error++;
  test (error);

  /* hash key deletion */
  test_print ("           delete: ");
  error = 0;
  n = hash_size (hash);
  if ((void *) 0xeabceabc != hash_delete (hash, "1234567890"))
    error++;
  if (n - 1 != hash_size (hash))
    error++;
  test (error);

  /* keys and values */
  test_print ("  keys and values: ");
  hash_clear (hash);
  error = 0;
  text = xmalloc (16);
  for (n = 0; n < REPEAT; n++)
    {
      sprintf (text, "%015lu", (unsigned long) n);
      hash_put (hash, text, (void *) n);
      if (hash_get (hash, text) != (void *) n)
	error++;
    }
  xfree (text);
  if (n != hash_size (hash))
    error++;
  values = hash_values (hash);
  keys = hash_keys (hash);
  if (keys && values)
    { 
      for (n = 0; n < REPEAT; n++)
	{
	  if (atol (keys[n]) != (long) values[n])
	    error++;
	  if (hash_get (hash, keys[n]) != values[n])
	    error++;
	  if (hash_contains (hash, values[n]) != keys[n])
	    error++;
	}
      hash_xfree (keys);
      hash_xfree (values);
    }
  else error++;
  if (hash_size (hash) != REPEAT)
    error++;
  test (error);

  /* rehashing */
  error = 0;
  test_print ("           rehash: ");
  while (hash->buckets > HASH_MIN_SIZE)
    hash_rehash (hash, HASH_SHRINK);
  while (hash->buckets < hash_size (hash) * 10)
    hash_rehash (hash, HASH_EXPAND);
  while (hash->buckets > HASH_MIN_SIZE)
    hash_rehash (hash, HASH_SHRINK);
  while (hash->buckets < hash_size (hash) * 10)
    hash_rehash (hash, HASH_EXPAND);
  text = xmalloc (16);
  for (n = 0; n < REPEAT; n++)
    {
      sprintf (text, "%015lu", (unsigned long) n);
      if (hash_get (hash, text) != (void *) n)
	error++;
      if (hash_delete (hash, text) != (void *) n)
	error++;
    }
  if (hash_size (hash))
    error++;
  xfree (text);
  test (error);
  
  /* hash clear */
  test_print ("            clear: ");
  hash_clear (hash);
  test (hash_size (hash));

  /* hash destruction */
  test_print ("          destroy: ");
  hash_destroy (hash);
  test_ok ();

#if ENABLE_DEBUG
  /* is heap ok ? */
  test_print ("             heap: ");
  test (allocated_bytes || allocated_blocks);
#endif

  return result;
}
