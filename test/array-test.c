/*
 * test/array-test.c - array tests
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
 * $Id: array-test.c,v 1.2 2001/03/11 13:06:19 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERVEEZ_API
#include "libserveez/alloc.h"
#include "libserveez/array.h"
#include "test.h"

/* general array test defines */
#define REPEAT 10000
#define test(error) \
  if (error) {      \
    test_failed (); \
    result++;       \
  } else {          \
    test_ok ();     \
  }                 \

/*
 * Main entry point for array tests.
 */
int
main (int argc, char **argv)
{
  int result = 0;
  svz_array_t *array;
  long n, error;
  char *text;

  test_init ();
  test_print ("array function test suite\n");
  
  /* array creation */
  error = 0;
  test_print ("    create: ");
  if ((array = svz_array_create (0)) == NULL)
    error++;
  if (svz_array_size (array) != 0)
    error++;
  test (error);

  /* add and get functions */
  test_print ("       add: ");
  for (n = 0; n < REPEAT; n++)
    svz_array_add (array, (void *) (n + 1));
  test (svz_array_size (array) != REPEAT);

  test_print ("       get: ");
  for (error = n = 0; n < REPEAT; n++)
    if (svz_array_get (array, n) != (void *) (n + 1))
      error++;
  test (error);
  
  test_print ("       set: ");
  for (error = n = 0; n < REPEAT; n++)
    if (svz_array_set (array, n, (void *) (REPEAT - n)) != (void *) (n + 1))
      error++;
  test (error);

  test_print ("    delete: ");
  for (error = n = 0; n < REPEAT; n++)
    if (svz_array_del (array, 0) != (void *) (REPEAT - n))
      error++;
  if (svz_array_size (array) != 0)
    error++;
  for (n = 0; n < REPEAT; n++)
    svz_array_add (array, (void *) n);
  for (n = REPEAT - 1; n >= 0; n--)
    if (svz_array_del (array, n) != (void *) n)
      error++;
  if (svz_array_size (array) != 0)
    error++;
  test (error);

  test_print ("     clear: ");
  for (n = 0; n < REPEAT; n++)
    svz_array_add (array, (void *) n);
  svz_array_clear (array);
  test (svz_array_size (array) != 0);

  /* check the `contains' function */
  test_print ("  contains: ");
  error = 0;
  if (svz_array_contains (array, (void *) 0))
    error++;
  for (n = 0; n < REPEAT; n++)
    {
      svz_array_add (array, (void *) n);
      if (svz_array_contains (array, (void *) n) != 1)
	error++;
    }
  for (n = 0; n < REPEAT; n++)
    {
      svz_array_set (array, n, (void *) 0);
      if (svz_array_contains (array, (void *) 0) != (unsigned long) n + 1)
	error++;
    }
  test (error);

  /* check the `index' function */
  test_print ("     index: ");
  error = 0;
  if (svz_array_idx (array, (void *) 0) != 0)
    error++;
  for (n = 0; n < REPEAT; n++)
    {
      if (svz_array_idx (array, (void *) (n + 1)) != (unsigned long) -1)
	error++;
      svz_array_set (array, n, (void *) (n + 1));
      if (svz_array_idx (array, (void *) (n + 1)) != (unsigned long) n)
	error++;
    }
  test (error);

  /* check the `insert' function */
  test_print ("    insert: ");
  error = 0;
  svz_array_clear (array);
  for (n = 0; n < REPEAT; n++)
    if (svz_array_ins (array, 0, (void *) n) != 0)
      error++;
  if (svz_array_size (array) != REPEAT)
    error++;
  for (n = 0; n < REPEAT; n++)
    if (svz_array_get (array, n) != (void *) (REPEAT - n - 1))
      error++;
  svz_array_clear (array);
  for (n = 0; n < REPEAT; n++)
    if (svz_array_ins (array, n, (void *) n) != (unsigned long) n)
      error++;
  if (svz_array_size (array) != REPEAT)
    error++;
  for (n = 0; n < REPEAT; n++)
    if (svz_array_get (array, n) != (void *) n)
      error++;
  test (error);

  test_print ("   destroy: ");
  svz_array_destroy (array);
  test_ok ();

#if ENABLE_DEBUG
  /* is heap ok ? */
  test_print ("      heap: ");
  test (svz_allocated_bytes || svz_allocated_blocks);
#endif /* ENABLE_DEBUG */

  return result;
}
