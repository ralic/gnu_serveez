/*
 * test/alist-test.c - array list function tests
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
 * $Id: alist-test.c,v 1.4 2000/11/10 11:24:05 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"
#include "alist.h"
#include "test.h"

#define REPEAT 10000
#define SIZE   1000
#define GAP    5
#define test(error) \
  if (error) {      \
    test_failed (); \
    result++;       \
  } else {          \
    test_ok ();     \
  }                 \

/*
 * Main entry point for array list tests.
 */
int
main (int argc, char **argv)
{
  int result = 0;
  alist_t *list;
  long n, error, i;
  char *text;
  void **values;

  test_init ();
  test_print ("array list function test suite\n");
  
  /* array list creation */
  test_print ("        create: ");
  test ((list = alist_create ()) == NULL);

  /* add and get */
  test_print ("   add and get: ");
  for (n = error = 0; n < REPEAT; n++)
    {
      alist_add (list, (void *) 0xdeadbeef);
      if (alist_get (list, n) != (void *) 0xdeadbeef)
	error++;
    }
  if (alist_size (list) != REPEAT || alist_length (list) != REPEAT)
    error++;
  test (error);

  /* contains */
  error = 0;
  test_print ("      contains: ");
  if (alist_contains (list, NULL))
    error++;
  if (alist_contains (list, (void *) 0xdeadbeef) != REPEAT)
    error++;
  alist_add (list, (void *) 0xeabceabc);
  if (alist_contains (list, (void *) 0xeabceabc) != 1)
    error++;
  test (error);

  /* searching */
  error = 0;
  test_print ("         index: ");
  if (alist_index (list, (void *) 0xdeadbeef) != 0)
    error++;
  if (alist_index (list, (void *) 0xeabceabc) != REPEAT)
    error++;
  if (alist_index (list, NULL) != -1)
    error++;
  test (error);

  /* deletion */
  error = 0;
  test_print ("        delete: ");
  if (alist_delete (list, REPEAT) != (void *) 0xeabceabc)
    error++;
  for (n = 0; n < REPEAT; n++)
    {
      if (alist_delete (list, 0) != (void *) 0xdeadbeef)
	error++;
    }
  if (alist_length (list) || alist_size (list))
    error++;
  test (error);

  /* insertion */
  error = 0;
  test_print ("        insert: ");
  for (n = 0; n < REPEAT; n++)
    {
      alist_insert (list, 0, (void *) 0xeabceabc);
      if (alist_get (list, 0) != (void *) 0xeabceabc)
	error++;
      if (alist_delete (list, 0) != (void *) 0xeabceabc)
	error++;

      alist_insert (list, n, (void *) 0xdeadbeef);
      if (alist_get (list, n) != (void *) 0xdeadbeef)
	error++;
    }
  if (alist_length (list) != REPEAT || alist_size (list) != REPEAT)
    error++;
  test (error);

  /* set (replace) and get */
  alist_clear (list);
  error = 0;
  test_print ("   set and get: ");
  for (n = 0; n < REPEAT / GAP; n++)
    {
      if (alist_set (list, n * GAP, (void *) (n * GAP)) != NULL)
	error++;
      if (alist_get (list, n * GAP) != (void *) (n * GAP))
	error++;

      for (i = GAP - 1; i > 0; i--)
	{ 
	  if (alist_set (list, n * GAP + i, (void *) ((n * GAP) + i)) != NULL)
	    error++;
	  if (alist_get (list, n * GAP + i) != (void *) ((n * GAP) + i))
	    error++;
	}
    }
  
  if (alist_length (list) != REPEAT || alist_size (list) != REPEAT)
    error++;
  for (n = 0; n < REPEAT; n++)
    {
      if (alist_get (list, n) != (void *) n)
	error++;
    }
  test (error);

  /* values */
  error = 0;
  test_print ("        values: ");
  if ((values = alist_values (list)) != NULL)
    {
      for (n = 0; n < REPEAT; n++)
	{
	  if (values[n] != (void *) n)
	    error++;
	}
      xfree (values);
    }
  else error++;
  test (error);

  /* pack */
  error = 0;
  alist_clear (list);
  test_print ("          pack: ");
  for (n = 0; n < REPEAT; n++)
    {
      if (alist_set (list, n * GAP, (void *) n) != NULL)
	error++;
    }
  if (alist_length (list) != REPEAT * GAP - GAP + 1 || 
      alist_size (list) != REPEAT)
    error++;
  alist_pack (list);
  if (alist_length (list) != REPEAT || alist_size (list) != REPEAT)
    error++;
  for (n = 0; n < REPEAT; n++)
    {
      if (alist_get (list, n) != (void *) n)
	error++;
    }
  test (error);

  /* range deletion */
  error = 0;
  test_print ("  delete range: ");
  alist_set (list, 0, (void *) 0xdeadbeef);
  for (n = 0; (unsigned) n < alist_length (list) - GAP; n++)
    {
      if (alist_delete_range (list, n, n + GAP) != GAP)
	error++;
      n += GAP;
    }
  n++;
  if (alist_size (list) != (unsigned) n || alist_length (list) != (unsigned) n)
    error++;
  for (i = GAP, n = 0; (unsigned) n < alist_length (list) - 1; n++, i++)
    {
      if (alist_get (list, n) != (void *) i)
	error++;
      if (((n + 1) % (GAP + 1)) == 0) i += GAP;
    }
  n = alist_length (list);
  if (alist_delete_range (list, 0, n) != (unsigned) n)
    error++;
  if (alist_size (list) != 0 || alist_length (list) != 0)
    error++;
  test (error);

  /* stress test */
  error = 0;
  alist_clear (list);
  test_print ("        stress: ");

  /* put any values to array until a certain size (no order) */
  while (alist_size (list) != SIZE)
    {
      n = test_value (SIZE);
      if (alist_get (list, n) != (void *) (n + SIZE))
	{
	  alist_set (list, n, (void *) (n + SIZE));
	}
    }

  /* check for final size and length */
  if (alist_size (list) != SIZE || alist_length (list) != SIZE)
    error++;
  test_print (error ? "?" : ".");

  /* check contains(), index() and get() */
  for (n = 0; n < SIZE; n++)
    {
      if (alist_contains (list, (void *) (n + SIZE)) != 1)
	error++;
      if (alist_index (list, (void *) (n + SIZE)) != n)
	error++;
      if (alist_get (list, n) != (void *) (n + SIZE))
	error++;
    }
  test_print (error ? "?" : ".");

  /* delete all values */
  for (n = 0; n < SIZE; n++)
    {
      if (alist_delete (list, 0) != (void *) (n + SIZE))
	error++;
    }

  /* check "post" size */
  if (alist_size (list) || alist_length (list))
    error++;
  test_print (error ? "?" : ".");

  /* build array insert()ing values */
  while (alist_size (list) != REPEAT)
    {
      n = test_value (REPEAT);
      alist_insert (list, n, (void *) 0xdeadbeef);
    }

  /* check size and length of array */
  if (alist_size (list) > alist_length (list))
    error++;

  /* check all values */
  if (alist_contains (list, (void *) 0xdeadbeef) != REPEAT)
    error++;
  test_print (error ? "?" : ".");

  /* save values, pack() list and check "post" get() values */
  if ((values = alist_values (list)) != NULL)
    {
      alist_pack (list);
      if (alist_size (list) != REPEAT || alist_length (list) != REPEAT)
	error++;
      for (n = 0; n < REPEAT; n++)
	{
	  if (alist_get (list, n) != values[n] || 
	      values[n] != (void *) 0xdeadbeef)
	    error++;
	}
      xfree (values);
    }
  else error++;
  test_print (error ? "?" : ".");

  /* delete each value, found by index() and check it via contains() */
  n = REPEAT;
  while (alist_size (list))
    {
      if (alist_delete (list, alist_index (list, (void *) 0xdeadbeef)) !=
	  (void *) 0xdeadbeef)
	error++;
      if (alist_contains (list, (void *) 0xdeadbeef) != (unsigned) --n)
	error++;
    }

  /* check "post" size */
  if (alist_size (list) || alist_length (list))
    error++;
  test_print (error ? "?" : ".");

  for (i = SIZE; i < SIZE + 10; i++)
    { 
      /* build array list */
      while (alist_size (list) != (unsigned) i)
	{
	  n = test_value (10 * i) + 1;
	  alist_insert (list, n, (void *) n);
	  if (alist_get (list, n) != (void *) n)
	    error++;
	}

      /* delete all values by chance */
      while (alist_size (list))
	{
	  alist_delete_range (list, test_value (i), 
			      test_value (10 * i * 5 + 1));
	}
      test_print (error ? "?" : ".");
    }
  test_print (" ");
  test (error);

  /* array list destruction */
  test_print ("       destroy: ");
  alist_destroy (list);
  test_ok ();

#if ENABLE_DEBUG
  /* is heap ok ? */
  test_print ("          heap: ");
  test (allocated_bytes || allocated_blocks);
#endif

  return result;
}
