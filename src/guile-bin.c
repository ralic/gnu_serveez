/*
 * guile-bin.c - binary data exchange layer for Guile servers
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
 * $Id: guile-bin.c,v 1.11 2001/11/16 09:08:22 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_GUILE_SERVER

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#if GUILE_SOURCE
# include <libguile/gh.h>
#else
# include <guile/gh.h>
#endif

#include "libserveez.h"
#include "guile-bin.h"

/*
 * Structure definition of the data the binary smob refers to.
 */
typedef struct guile_bin
{
  unsigned char *data; /* data pointer */
  int size;            /* size of the above data */ 
  int garbage;         /* if set the data pointer got allocated by
			  the smob functions */
}
guile_bin_t;

/* The smob tag. */
static long guile_bin_tag = 0;

/* Useful defines for accessing the binary smob. */
#define GET_BIN_SMOB(binary) \
  ((guile_bin_t *) ((unsigned long) SCM_SMOB_DATA (binary)))
#define CHECK_BIN_SMOB(binary) \
  (SCM_NIMP (binary) && SCM_TYP16 (binary) == guile_bin_tag)
#define CHECK_BIN_SMOB_ARG(binary, arg, var)                       \
  if (!CHECK_BIN_SMOB (binary))                                    \
    scm_wrong_type_arg_msg (FUNC_NAME, arg, binary, "svz-binary"); \
  var = GET_BIN_SMOB (binary)
#define MAKE_BIN_SMOB() \
  ((guile_bin_t *) scm_must_malloc (sizeof (guile_bin_t), "svz-binary"))

/* Smob test function: Returns @code{#t} if the given cell @var{binary} is 
   an instance of the binary smob type. */
#define FUNC_NAME "binary?"
static SCM
guile_bin_p (SCM binary)
{
  return CHECK_BIN_SMOB (binary) ? SCM_BOOL_T : SCM_BOOL_F;
}
#undef FUNC_NAME

/* Smob print function: Displays a text representation of the given
   cell @var{binary} to the output port @var{port}. */
static int
guile_bin_print (SCM binary, SCM port, scm_print_state *state)
{
  guile_bin_t *bin = GET_BIN_SMOB (binary);
  static char txt[256];

  sprintf (txt, "#<svz-binary %p, size: %d>", bin->data, bin->size);
  scm_puts (txt, port);
  return 1;
}

/* Smob free function: Releases any allocated resources used the given
   cell @var{binary}. No need to mark any referring scheme cell. Returns
   the number of bytes actually free()'d. */
static scm_sizet
guile_bin_free (SCM binary)
{
  guile_bin_t *bin = GET_BIN_SMOB (binary);
  scm_sizet size = sizeof (guile_bin_t);

  /* Free the data pointer if it has been allocated by ourselves and
     is not just a reference. */
  if (bin->garbage)
    {
      size += bin->size;
      scm_must_free ((void *) bin->data);
    }
  scm_must_free ((void *) bin);
  return size;
}

/* Smob equal function: Return #t if the both cells @var{a} and @var{b}
   are definitely or virtually equal. Otherwise return #f. */
static SCM
guile_bin_equal (SCM a, SCM b)
{
  guile_bin_t *bin1 = GET_BIN_SMOB (a);
  guile_bin_t *bin2 = GET_BIN_SMOB (b);

  if (bin1 == bin2)
    return SCM_BOOL_T;
  else if (bin1->size == bin2->size)
    {
      if (bin1->data == bin2->data)
	return SCM_BOOL_T;
      else if (memcmp (bin1->data, bin2->data, bin1->size) == 0)
	return SCM_BOOL_T;
    }
  return SCM_BOOL_F;
}

/* Converts the given string cell @var{string} into a binary smob. The data
   pointer of the binary smob is marked as garbage which must be free()'d 
   in the sweep phase of the garbage collector. */
#define FUNC_NAME "string->binary"
SCM
guile_string_to_bin (SCM string)
{
  guile_bin_t *bin;

  SCM_ASSERT_TYPE (gh_string_p (string), string, 
		   SCM_ARG1, FUNC_NAME, "string");

  bin = MAKE_BIN_SMOB ();
  bin->size = gh_scm2int (scm_string_length (string));
  bin->data = (unsigned char *) scm_must_malloc (bin->size, "svz-binary-data");
  memcpy (bin->data, SCM_STRING_CHARS (string), bin->size);
  bin->garbage = 1;

  SCM_RETURN_NEWSMOB (guile_bin_tag, bin);
}
#undef FUNC_NAME

/* Converts the given binary smob @var{binary} into a string. Returns the
   string cell itself. */
#define FUNC_NAME "binary->string"
SCM
guile_bin_to_string (SCM binary)
{
  guile_bin_t *bin;

  CHECK_BIN_SMOB_ARG (binary, SCM_ARG1, bin);
  return gh_str2scm ((char *) bin->data, bin->size);
}
#undef FUNC_NAME

/* This routine searches through the binary smob @var{binary} for the cell
   @var{needle}. The latter argument can be either a exact number, character,
   string or another binary smob. It returns @code{#f} if the needle could 
   not be found and a positive number indicates the position of the first 
   occurrence of @var{needle} in the binary smob @var{binary}. */
#define FUNC_NAME "binary-search"
SCM
guile_bin_search (SCM binary, SCM needle)
{
  SCM ret = SCM_BOOL_F;
  guile_bin_t *bin;

  CHECK_BIN_SMOB_ARG (binary, SCM_ARG1, bin);
  SCM_ASSERT (gh_string_p (needle) || gh_char_p (needle) || 
	      gh_exact_p (needle) || CHECK_BIN_SMOB (needle),
	      needle, SCM_ARG2, FUNC_NAME);

  /* Search for a pattern. */
  if (gh_string_p (needle) || CHECK_BIN_SMOB (needle))
    {
      guile_bin_t *search = NULL;
      int len;
      unsigned char *p, *end, *start;

      if (CHECK_BIN_SMOB (needle))
	search = GET_BIN_SMOB (needle);
      len = search ? search->size : gh_scm2int (scm_string_length (needle));
      p = search ? search->data : SCM_STRING_UCHARS (needle);
      start = bin->data;
      end = start + bin->size - len;

      while (start <= end)
	{
	  if (*start == *p && memcmp (start, p, len) == 0)
	    {
	      ret = gh_int2scm (start - bin->data);
	      break;
	    }
	  start++;
	} 
    }

  /* Search for a single byte. */
  else if (gh_char_p (needle) || gh_exact_p (needle))
    {
      unsigned char c;
      unsigned char *p, *end;

      c = gh_char_p (needle) ? gh_scm2char (needle) : gh_scm2int (needle);
      p = bin->data;
      end = p + bin->size;

      while (p < end)
	{
	  if (*p == c)
	    {
	      ret = gh_int2scm (p - bin->data);
	      break;
	    }
	  p++;
	}
    }
  return ret;
}
#undef FUNC_NAME

/* Set the byte at position @var{index} of the binary smob @var{binary} to
   the value given in @var{value} which can be either a character or a 
   exact number. */
#define FUNC_NAME "binary-set!"
SCM
guile_bin_set_x (SCM binary, SCM index, SCM value)
{
  guile_bin_t *bin;
  int idx;

  CHECK_BIN_SMOB_ARG (binary, SCM_ARG1, bin);
  SCM_ASSERT_TYPE (gh_exact_p (index), index, SCM_ARG2, FUNC_NAME, "exact");
  SCM_ASSERT_TYPE (gh_exact_p (value) || gh_char_p (value), 
		   value, SCM_ARG3, FUNC_NAME, "char or exact");

  /* Check the range of the index argument. */
  idx = gh_scm2int (index);
  if (idx < 0 || idx >= bin->size)
    scm_out_of_range_pos (FUNC_NAME, index, SCM_ARG2);

  bin->data[idx] = gh_scm2char (value);
  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

/* Obtain the byte at position @var{index} of the binary smob 
   @var{binary}. */
#define FUNC_NAME "binary-ref"
SCM
guile_bin_ref (SCM binary, SCM index)
{
  guile_bin_t *bin;
  int idx;

  CHECK_BIN_SMOB_ARG (binary, SCM_ARG1, bin);
  SCM_ASSERT_TYPE (gh_exact_p (index), index, SCM_ARG2, FUNC_NAME, "exact");

  /* Check the range of the index argument. */
  idx = gh_scm2int (index);
  if (idx < 0 || idx >= bin->size)
    scm_out_of_range_pos (FUNC_NAME, index, SCM_ARG2);

  return gh_char2scm (bin->data[idx]);
}
#undef FUNC_NAME

/* Return the size of the binary smob @var{binary}. */
#define FUNC_NAME "binary-length"
SCM
guile_bin_length (SCM binary)
{
  guile_bin_t *bin;

  CHECK_BIN_SMOB_ARG (binary, SCM_ARG1, bin);
  return gh_int2scm (bin->size);
}
#undef FUNC_NAME

/* Append either the binary smob or string @var{append} onto the binary
   smob @var{binary}. If @var{binary} has been a simple data pointer
   reference it is then a standalone binary smob as returned by
   @code{string->binary}. */
#define FUNC_NAME "binary-concat!"
SCM
guile_bin_concat (SCM binary, SCM append)
{
  guile_bin_t *bin, *concat = NULL;
  int len;
  unsigned char *p;
  
  CHECK_BIN_SMOB_ARG (binary, SCM_ARG1, bin);
  SCM_ASSERT (gh_string_p (append) || CHECK_BIN_SMOB (append),
	      append, SCM_ARG2, FUNC_NAME);

  if (CHECK_BIN_SMOB (append))
    concat = GET_BIN_SMOB (append);
  len = concat ? concat->size : gh_scm2int (scm_string_length (append));
  p = concat ? concat->data : SCM_STRING_UCHARS (append);

  if (bin->garbage)
    {
      bin->data = (unsigned char *) 
	scm_must_realloc ((void *) bin->data, bin->size, bin->size + len, 
			  "svz-binary-data");
    }
  else
    {
      unsigned char *odata = bin->data;
      bin->data = (unsigned char *) 
	scm_must_malloc (bin->size + len, "svz-binary-data");
      memcpy (bin->data, odata, bin->size);
    }

  memcpy (bin->data + bin->size, p, len);
  bin->size += len;
  bin->garbage = 1;
  return binary;
}
#undef FUNC_NAME

/* Create a subset binary smob from the given binary smob @var{binary}. The
   range of this subset is specified by @var{start} and @var{end} both
   inclusive (thus the resulting size is = @var{end} - @var{start} + 1). 
   With a single exception: If @var{end} is not given or specified with -1 
   the routine returns all data until the end of @var{binary}. */
#define FUNC_NAME "binary-subset"
SCM
guile_bin_subset (SCM binary, SCM start, SCM end)
{
  guile_bin_t *bin, *ret;
  int from, to;

  CHECK_BIN_SMOB_ARG (binary, SCM_ARG1, bin);
  SCM_ASSERT_TYPE (gh_exact_p (start), start, SCM_ARG2, FUNC_NAME, "exact");
  SCM_ASSERT_TYPE (gh_exact_p (end) || gh_eq_p (end, SCM_UNDEFINED), 
		   end, SCM_ARG3, FUNC_NAME, "exact");

  from = gh_scm2int (start);
  to = gh_eq_p (end, SCM_UNDEFINED) ? -1 : gh_scm2int (end);
  if (to == -1)
    to = bin->size - 1;

  /* Check the ranges of both indices. */
  if (from < 0 || from >= bin->size || from >= end)
    scm_out_of_range_pos (FUNC_NAME, from, SCM_ARG2);    
  if (to < 0 || to >= bin->size || to <= from)
    scm_out_of_range_pos (FUNC_NAME, to, SCM_ARG3);    

  ret = MAKE_BIN_SMOB ();
  ret->size = to - from + 1;
  ret->data = bin->data + from;
  ret->garbage = 0;

  SCM_RETURN_NEWSMOB (guile_bin_tag, ret);
}
#undef FUNC_NAME

/* Convert the given binary smob @var{binary} into a scheme list. The list
   is empty if the size of @var{binary} is zero. */
#define FUNC_NAME "binary->list"
SCM
guile_bin_to_list (SCM binary)
{
  guile_bin_t *bin;
  unsigned char *p;
  SCM list;

  CHECK_BIN_SMOB_ARG (binary, SCM_ARG1, bin);
  for (list = SCM_EOL, p = bin->data + bin->size; p-- > bin->data; )
    list = gh_cons (gh_ulong2scm (*p), list);
  return list;
}
#undef FUNC_NAME

/* Convert the scheme list @var{list} into a binary smob. Each of the 
   elements of @var{list} is checked for validity. */
#define FUNC_NAME "list->binary"
SCM
guile_list_to_bin (SCM list)
{
  guile_bin_t *bin;
  unsigned char *p;
  int value;

  SCM_ASSERT_TYPE (gh_list_p (list), list, SCM_ARG1, FUNC_NAME, "list");
  bin = MAKE_BIN_SMOB ();
  bin->size = gh_length (list);

  if (bin->size > 0)
    {
      p = bin->data = (unsigned char *) 
	scm_must_malloc (bin->size, "svz-binary-data");
      bin->garbage = 1;
    }
  else
    {
      bin->garbage = 0;
      bin->data = NULL;
      SCM_RETURN_NEWSMOB (guile_bin_tag, bin);
    }
	
  /* Iterate over the list and build up binary smob. */
  while (gh_pair_p (list))
    {
      if (!gh_exact_p (gh_car (list)))
	scm_out_of_range (FUNC_NAME, gh_car (list));
      value = gh_scm2int (gh_car (list));
      if (value < 0 || value > 255)
	scm_out_of_range (FUNC_NAME, gh_car (list));
      *p++ = value;
      list = gh_cdr (list);
    }

  SCM_RETURN_NEWSMOB (guile_bin_tag, bin);
}
#undef FUNC_NAME

/* Checks if the given scheme cell @var{binary} is a binary smob or not.
   Returns zero if not, otherwise non-zero. */
int
guile_bin_check (SCM binary)
{
  return CHECK_BIN_SMOB (binary) ? 1 : 0;
}

/* This routine converts any given data pointer @var{data} with a
   certain @var{size} into a binary smob. This contains then a simple
   reference to the given data pointer. */
SCM
guile_data_to_bin (void *data, int size)
{
  guile_bin_t *bin;
     
  bin = MAKE_BIN_SMOB ();
  bin->size = size;
  bin->data = data;
  bin->garbage = 0;
  SCM_RETURN_NEWSMOB (guile_bin_tag, bin);
}

/* This function converts the given binary smob @var{binary} back to a
   data pointer. If the @var{size} argument is not NULL the current size
   of the binary smob will be stored there. */
void *
guile_bin_to_data (SCM binary, int *size)
{
  guile_bin_t *bin = GET_BIN_SMOB (binary);
  if (size)
    *size = bin->size;
  return bin->data;
}

/* Initialize the binary smob with all its features. Call this function
   once at application startup and before running any scheme file 
   evaluation. */
void
guile_bin_init (void)
{
  guile_bin_tag = scm_make_smob_type ("svz-binary", 0);
  scm_set_smob_print (guile_bin_tag, guile_bin_print);
  scm_set_smob_free (guile_bin_tag, guile_bin_free);
  scm_set_smob_equalp (guile_bin_tag, guile_bin_equal);

  gh_new_procedure ("binary?", guile_bin_p, 1, 0, 0);
  gh_new_procedure ("string->binary", guile_string_to_bin, 1, 0, 0);
  gh_new_procedure ("binary->string", guile_bin_to_string, 1, 0, 0);
  gh_new_procedure ("list->binary", guile_list_to_bin, 1, 0, 0);
  gh_new_procedure ("binary->list", guile_bin_to_list, 1, 0, 0);
  gh_new_procedure ("binary-search", guile_bin_search, 2, 0, 0);
  gh_new_procedure ("binary-set!", guile_bin_set_x, 3, 0, 0);
  gh_new_procedure ("binary-ref", guile_bin_ref, 2, 0, 0);
  gh_new_procedure ("binary-length", guile_bin_length, 1, 0, 0);
  gh_new_procedure ("binary-concat!", guile_bin_concat, 2, 0, 0);
  gh_new_procedure ("binary-subset", guile_bin_subset, 2, 1, 0);
}

#else /* not ENABLE_GUILE_SERVER */

static int have_guile_bin = 0;

#endif/* ENABLE_GUILE_SERVER */
