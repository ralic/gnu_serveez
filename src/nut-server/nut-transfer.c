/*
 * nut-transfer.c - gnutella file transfer implementation
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
 *
 * $Id: nut-transfer.c,v 1.1 2000/08/29 10:44:03 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_GNUTELLA

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#ifndef __MINGW32__
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
# include <io.h>
#endif

#include "alloc.h"
#include "util.h"
#include "socket.h"
#include "connect.h"
#include "server.h"
#include "gnutella.h"
#include "nut-transfer.h"

/*
 * Within this callback the actual file transfer is done.
 */
static int
nut_save_transfer (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;
  int fill = sock->recv_buffer_fill;
  nut_transfer_t *transfer = sock->data;
  int num_written;

  if (fill > 0)
    {
      num_written = write (sock->file_desc, sock->recv_buffer, fill);
      if (num_written < 0)
	{
	  log_printf (LOG_ERROR, "nut: write: %s\n", SYS_ERROR);
	  return -1;
	}
      sock_reduce_recv (sock, num_written);
      transfer->size -= num_written;
      if (transfer->size <= 0)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: file successfully received\n");
#endif
	  return -1;
	}
    }
  return 0;
}

/*
 * This is the sock->check_request callback for gnutella file transfers.
 * Whenever there is data within the receive queue it will be called.
 */
static int
nut_check_transfer (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;
  int fill = sock->recv_buffer_fill;
  int len = strlen (NUT_GET_OK);
  char *p = sock->recv_buffer;
  nut_transfer_t *transfer = sock->data;

  if (fill >= len && !memcmp (sock->recv_buffer, NUT_GET_OK, len))
    {
      len = 0;
      while (p < sock->recv_buffer + (fill - 3) && 
	     memcmp (p, NUT_SEPERATOR, 4))
	{
	  if (!memcmp (p, NUT_LENGTH, strlen (NUT_LENGTH)))
	    {
	      p += 16;
	      while (*p >= '0' && *p <= '9')
		{
		  len *= 10;
		  len += *p - '0';
		  p++;
		}
	    }
	  p++;
	}
      if (p < sock->recv_buffer + (fill - 3) && 
	  memcmp (p, NUT_SEPERATOR, 4))
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "nut: header received, length %d\n", len);
#endif
	  sock->userflags |= NUT_FLAG_HDR;
	  transfer->size = len;
	  if (transfer->original_size != transfer->size)
	    {
	      log_printf (LOG_WARNING,
			  "nut: transfer sizes differ (%d!=%d)\n",
			  transfer->original_size, transfer->size);
	    }
	  sock->check_request = nut_save_transfer;
	  sock->write_socket = NULL;

	  len = (p - sock->recv_buffer) + 4;
	  sock_reduce_recv (sock, len);
	}
    }

  return 0;
}

/*
 * This callback is executed whenever a gnutella file transfer aborted.
 */
static int
nut_disconnect_transfer (socket_t sock)
{
  nut_config_t *cfg = sock->cfg;

  if (sock->data)
    xfree (sock->data);

  cfg->dnloads--;

  if (close (sock->file_desc) == -1)
    log_printf (LOG_ERROR, "nut: close: %s\n", SYS_ERROR);

  return 0;
}

/*
 * This routine tries to connect to a foreign gnutella host in order to
 * get a certain file.
 */
int
nut_init_transfer (socket_t sock, nut_reply_t *reply, nut_record_t *record)
{
  nut_config_t *cfg = sock->cfg;
  socket_t xsock;
  char *file;
  struct stat buf;
  int fd;
  nut_transfer_t *transfer;
  int n = 0, pos;
  
  /* has the requested file the right file extension ? */
  if (cfg->extensions)
    {
      /* go through all file extensions */
      while (cfg->extensions[n])
	{
	  if (strlen (record->file) > strlen (cfg->extensions[n]))
	    {
	      pos = strlen (record->file) - strlen (cfg->extensions[n]);
	      if (!util_strcasecmp (&record->file[pos], cfg->extensions[n]))
		break;
	    }
	  n++;
	}
      /* did the above code "break" ? */
      if (!cfg->extensions[n])
	{
	  log_printf (LOG_WARNING, "nut: not a valid extension: %s\n",
		      record->file);
	  return -1;
	}
    }

  /* first check if the requested file is not already created */
  file = xmalloc (strlen (cfg->save_path) + strlen (record->file) + 2);
  sprintf (file, "%s/%s", cfg->save_path, record->file);
  if (stat (file, &buf) != -1)
    {
      log_printf (LOG_NOTICE, "nut: %s already exists\n", record->file);
      xfree (file);
      return -1;
    }

  /* try creating local file */
  if ((fd = open (file, O_WRONLY | O_CREAT)) == -1)
    {
      log_printf (LOG_ERROR, "nut: open: %s\n", SYS_ERROR);
      xfree (file);
      return -1;
    }

  /* try to connect to the host */
  if ((xsock = sock_connect (reply->ip, htons (reply->port))) != NULL)
    {
      log_printf (LOG_NOTICE, "nut: connecting %s:%u\n",
		  util_inet_ntoa (reply->ip), reply->port);

      cfg->dnloads++;
      xsock->cfg = cfg;
      xsock->disconnected_socket = nut_disconnect_transfer;
      xsock->check_request = nut_check_transfer;
      xsock->userflags = NUT_FLAG_INIT;
      xsock->file_desc = fd;
      transfer = xmalloc (sizeof (nut_transfer_t));
      transfer->original_size = record->size;
      xsock->data = transfer;

      sock_printf (xsock, NUT_GET "%d/%s " NUT_HTTP "\r\n",
		   record->index, record->file);
      sock_printf (xsock, NUT_AGENT);
      sock_printf (xsock, NUT_RANGE "bytes=0-\r\n");
      sock_printf (xsock, "\r\n");
      xfree (file);
      return 0;
    }

  close (fd);
  xfree (file);
  return 0;
}

#else /* ENABLE_GNUTELLA */

int nut_transfer_dummy;	/* Shut compiler warnings up. */

#endif /* not ENABLE_GNUTELLA */
