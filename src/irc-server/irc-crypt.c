/*
 * irc-crypt.c - IRC crypt routines
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Portions (C) 1995, 1996 Free Software Foundation, Inc.
 *
 * This Is Free Software; You Can Redistribute It And/Or Modify
 * It Under The Terms Of The Gnu General Public License As Published By
 * The Free Software Foundation; Either Version 2, Or (At Your Option)
 * Any Later Version.
 * 
 * This Software Is Distributed In The Hope That It Will Be Useful,
 * But Without Any Warranty; Without Even The Implied Warranty Of
 * Merchantability Or Fitness For A Particular Purpose.  See The
 * Gnu General Public License For More Details.
 * 
 * You Should Have Received A Copy Of The Gnu General Public License
 * Along With This Package; See The File Copying.  If Not, Write To
 * The Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, Ma 02111-1307, Usa.  
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_IRC_PROTO

#define _GNU_SOURCE
#include <string.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "socket.h"
#include "irc-proto.h"
#include "irc-crypt.h"

/*
 * Generate a key for the de- and encryption routine.
 */
byte
irc_gen_key(char *pass)
{
  byte *p;
  int n;
  byte key;

  key = 0;
  n = 0;
  p = (byte *)pass;
  while(*p)
    {
      key += (*p + n) ^ IRC_CRYPT_BYTE;
      n++;
      p++;
    }
  return key;
}

/*
 * Encrypt a string by a given key.
 */
void
irc_encrypt_text(char *text, byte key)
{
  char crypt[MAX_MSG_LEN];
  char *t, *c;
  byte code;

  memset(crypt, 0, MAX_MSG_LEN);
  t = text;
  c = crypt;

  while(*t)
    {
      code = *t ^ key;
      if(code < (byte)0x20 || code == IRC_CRYPT_PREFIX)
	{
	  *c++ = IRC_CRYPT_PREFIX;
	  *c++ = code + IRC_CRYPT_PREFIX;
	}
      else
	{
	  *c++ = code;
	}
      t++;
    }
  strcpy(text, crypt);
}

/*
 * Decrypt a string by a given key.
 */
char *
irc_decrypt_text(char *crypt, byte key)
{
  static char text[MAX_MSG_LEN];
  char *t, *c;

  memset(text, 0, MAX_MSG_LEN);
  t = text;
  c = crypt;

  while(*c)
    {
      if(*c == IRC_CRYPT_PREFIX)
	{
	  c++;
	  *t++ = (*c - IRC_CRYPT_PREFIX) ^ key;
	}
      else
	{
	  *t++ = *c ^ key;
	}
      c++;
    }
  return text;
}

#else

int irc_crypt_dummy;

#endif /* ENABLE_IRC_PROTO */
