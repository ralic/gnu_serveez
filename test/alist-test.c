/*
 * test/alist-test.c - array list tests
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
 * $Id: alist-test.c,v 1.1 2000/10/20 13:13:48 ela Exp $
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
  long n, error;
  char *text;
  void **values;

  test_print ("array list function test suite\n");
  
  /* array list creation */
  test_print ("      create: ");
  test ((list = alist_create ()) == NULL);

  /* add and get */
  test_print (" add and get: ");
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
  test_print ("    contains: ");
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
  test_print ("       index: ");
  if (alist_index (list, (void *) 0xdeadbeef) != 0)
    error++;
  if (alist_index (list, (void *) 0xeabceabc) != REPEAT)
    error++;
  if (alist_index (list, NULL) != -1)
    error++;
  test (error);

  /* deletion */
  error = 0;
  test_print ("      delete: ");
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
  test_print ("      insert: ");
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

  /* array list destruction */
  test_print ("     destroy: ");
  alist_destroy (list);
  test_ok ();

#if ENABLE_DEBUG
  /* is heap ok ? */
  test_print ("        heap: ");
  test (allocated_bytes || allocated_blocks);
#endif

  return result;
}
