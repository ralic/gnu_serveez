/*
 * socket.h - socket management definition
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
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
 * $Id: socket.h,v 1.2 2000/06/11 21:39:17 raimi Exp $
 *
 */

#ifndef __SOCKET_H__
#define __SOCKET_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <time.h>
#include <string.h>

/* This is how many ID's can exist. It MUST be a 2^X and less than 10000. */
#define SOCKET_MAX_IDS       8192 
/* How much data is accepted before valid detection. */
#define MAX_DETECTION_FILL     16 
/* How much time is accepted before valid detection. */
#define MAX_DETECTION_WAIT     30 
/* If a socket resource is unavailable, relax for this time in seconds. */
#define RELAX_FD_TIME           1 
/* Do not write more than SOCK_MAX_WRITE bytes to a socket at once. */
#define SOCK_MAX_WRITE       1024

#define RECV_BUF_SIZE  (1024 * 8) /* Normal receive buffer size. */
#define SEND_BUF_SIZE  (1024 * 8) /* Normal send buffer size. */

#define SOCK_FLAG_INIT        0x0000 /* Value for initializing. */
#define SOCK_FLAG_INBUF       0x0001 /* Outbuf is allocated. */
#define SOCK_FLAG_OUTBUF      0x0002 /* Inbuf is allocated. */
#define SOCK_FLAG_CONNECTED   0x0010 /* Socket is connected. */
#define SOCK_FLAG_LISTENING   0x0020 /* Socket is listening. */
#define SOCK_FLAG_AUTH        0x0100 /* Socket has been authenticated. */
#define SOCK_FLAG_KILLED      0x0200 /* Socket will be shut down soon. */
#define SOCK_FLAG_NOFLOOD     0x0400 /* Flood protection off. */
#define SOCK_FLAG_INITED      0x0800 /* Socket was initialized. */
#define SOCK_FLAG_ENQUEUED    0x1000 /* Socket is on socket queue. */

#ifdef __MINGW32__
#define SOCK_FLAG_THREAD      0x2000 /* Win32 thread. */
#else
#define SOCK_FLAG_PIPE        0x2000 /* Socket is no socket, but pipe. */
#endif

#define SOCK_FLAG_AWCS_CLIENT 0x0004 /* aWCS client detected. */

#if ENABLE_HTTP_PROTO
#define SOCK_FLAG_HTTP_CLIENT 0x0008 /* HTTP client detected. */
#define SOCK_FLAG_HTTP_DONE   0x0040 /* HTTP request done */
#define SOCK_FLAG_HTTP_POST   0x0080 /* HTTP cgi pipe posting data */
#define SOCK_FLAG_HTTP_CGI    0x4000 /* HTTP cgi pipe getting data */
#define SOCK_FLAG_HTTP_FILE   0x8000 /* HTTP file response */
#define SOCK_FLAG_HTTP_CACHE  0x00040000 /* HTTP file cache */
#define SOCK_FLAG_HTTP_KEEP   0x00200000 /* Keep-Alive connection */

/* all of the additional HTTP flags */
#define SOCK_FLAG_HTTP (SOCK_FLAG_HTTP_DONE  | \
                        SOCK_FLAG_HTTP_POST  | \
                        SOCK_FLAG_HTTP_CGI   | \
                        SOCK_FLAG_HTTP_FILE  | \
                        SOCK_FLAG_HTTP_CACHE | \
                        SOCK_FLAG_HTTP_KEEP)
#endif

#if ENABLE_CONTROL_PROTO
#define SOCK_FLAG_CTRL_CLIENT 0x00010000 /* control connection */
#define SOCK_FLAG_CTRL_PASSED 0x00020000 /* got right password */
#endif

#if ENABLE_IRC_PROTO
#define SOCK_FLAG_IRC_CLIENT  0x00080000 /* IRC connection */
#define SOCK_FLAG_IRC_SERVER  0x00100000 /* IRC server connection */
#endif

#define VSNPRINTF_BUF_SIZE 2048 /* Size of the vsnprintf() buffer */

typedef struct socket * socket_t;
typedef struct socket socket_data_t;

struct socket
{
  socket_t next;		/* Next socket in chain. */
  socket_t prev;		/* Previous socket in chain. */
  int socket_id;		/* Unique ID for this socket. */

  int sflags;                   /* Server/Protocol flag. */
  int flags;			/* One of the SOCK_FLAG_* flags above. */
  SOCKET sock_desc;		/* Socket descriptor. */
  int file_desc;		/* Used for files descriptors. */
  HANDLE pipe_desc[2];          /* Used for the pipes and coservers. */
  char *recv_pipe;              /* File of the receive pipe. */
  char *send_pipe;              /* File of the send pipe. */

  char *boundary;               /* Packet boundary. */
  int boundary_size;            /* Packet boundary length */

  unsigned short remote_port;	/* Port number of remote end. */
  unsigned remote_addr;		/* IP address of remote end. */
  char *remote_host;            /* Hostname of the remote end. */
  unsigned short local_port;	/* Port number of local end. */
  unsigned local_addr;		/* IP address of local end. */
  char *local_host;             /* Hostname of the local end. */

  char * send_buffer;		/* Buffer for outbound data. */
  char * recv_buffer;		/* Buffer for inbound data. */
  int send_buffer_size;		/* Size of SEND_BUFFER. */
  int recv_buffer_size;		/* Size of RECV_BUFFER. */
  int send_buffer_fill;		/* Valid bytes in SEND_BUFFER. */
  int recv_buffer_fill;		/* Valid bytes in RECV_BUFFER. */

  /*
   * READ_SOCKET gets called whenever data is available on the socket.
   * Normally, this is set to a default function which reads all available
   * data from the socket and feeds it to CHECK_REQUEST, but specific
   * sockets may need another policy.
   */
  int (* read_socket) (socket_t sock);

  /*
   * WRITE_SOCKET is called when data is is valid in the output buffer
   * and the socket gets available for writing.  Normally, this simply
   * writes as much data as possible to the socket and removes it from
   * the buffer.
   */
  int (* write_socket) (socket_t sock);

  /*
   * DISCONNECTED_SOCKET gets called whenever the socket is lost for
   * some external reason.
   */
  int (* disconnected_socket) (socket_t sock);

  /*
   * KICKED_SOCKET gets called whenever the socket gets closed by us.
   */
  int (* kicked_socket) (socket_t sock, int reason);

  /*
   * CHECK_REQUEST gets called whenever data was read from the socket.
   * Its purpose is to check whether a complete request was read, and
   * if it was, it should be handled and removed from the input buffer.
   */
  int (* check_request) (socket_t sock);

#if ENABLE_REVERSE_LOOKUP
  /*
   * NSLOOKUP_FUNC gets called when sock's host has been resolved.
   */
  int (* nslookup_func) (socket_t sock, char *host);
#endif

#if ENABLE_IDENT
  /*
   * IDENT_FUNC gets called when sock's user has been identified.
   */
  int (* ident_func) (socket_t sock, char *user);
#endif

#if ENABLE_HTTP_PROTO
  void * http;            /* http_socket_t entry for HTTP connections */
#endif

  /*
   * When the final protocol detection in DEFAULT_CHECK_REQUEST
   * has been done CFG should get the actual configuration hash.
   */
  void * cfg;

  /*
   * HANDLE_REQUEST gets called when the CHECK_REQUEST got
   * a valid packet.
   */
  int (* handle_request) (socket_t sock, char *request, int len);

  /*
   * IDLE_FUNC gets called from the periodic task scheduler.  Whenever
   * IDLE_COUNTER (see below) is non-zero, it is decremented and
   * IDLE_FUNC gets called when it drops to zero.  IDLE_FUNC can reset
   * IDLE_COUNTER to some value and thus can re-schedule itself for a
   * later task.
   */
  int (* idle_func) (socket_t sock);

  int idle_counter;		/* Counter for calls to IDLE_FUNC. */

  time_t last_send;		/* Timestamp of last send to socket. */
  time_t last_recv;		/* Timestamp of last receive from socket */

#if ENABLE_FLOOD_PROTECTION
  int flood_points;		/* Accumulated floodpoints. */
  int flood_limit;		/* Limit of the above before kicking. */
#endif

  /* 
   * Set to non-zero time() value if the the socket is temporarily
   * unavailable (EAGAIN). This is why we use O_NONBLOCK socket
   * descriptors.
   */
  int unavailable;              

  /*
   * Misc field. Listener keeps array of server instances here.
   * NULL terminated.
   *
   */
  void *data;
};

extern socket_t sock_lookup_table[SOCKET_MAX_IDS];
extern int socket_id;

/*
 * Count the number of currently connected sockets.
 */
extern SOCKET connected_sockets;

/*
 * Allocate a structure of type socket_t and initialize
 * its fields.
 */
socket_t sock_alloc (void);

/*
 * Free the socket structure SOCK.  Return a non-zero value on error.
 */
int sock_free (socket_t sock);

/*
 * Create a socket structure from the file descriptor FD.  Return NULL
 * on error.
 */
socket_t sock_create (int fd);

/*
 * Disconnect the socket SOCK from the network and calls the
 * disconnect function for the socket if set.  Return a non-zero
 * value on error.
 */
int sock_disconnect (socket_t sock);

/*
 * Write LEN bytes from the memory location pointed to by BUF
 * to the output buffer of the socket SOCK.  Also try to flush the
 * buffer to the network socket of SOCK if possible.  Return a non-zero
 * value on error, which normally means a buffer overflow.
 */
int sock_write (socket_t sock, char * buf, int len);

/*
 * Print a formatted string on the socket SOCK.  FMT is the printf()-
 * style format string, which describes how to format the optional
 * arguments.  See the printf(3) manual page for details.
 */
int sock_printf (socket_t sock, const char * fmt, ...);

/*
 * Resize the send and receive buffers for the socket SOCK.  SEND_BUF_SIZE
 * is the new size for the send buffer, RECV_BUF_SIZE for the receive
 * buffer.  Note that data may be lost when the buffers shrink.
 */
int sock_resize_buffers (socket_t sock, int send_buf_size, int recv_buf_size);

/*
 * Get local and remote addresses/ports of socket SOCK and intern them
 * into the socket structure.
 */
int sock_intern_connection_info (socket_t sock);

/*
 * Create a socket structure for reading and writing to a pipe.
 */
socket_t pipe_create (int read_fd, int write_fd);

int default_read (socket_t sock);
int default_detect_proto (socket_t sock);
int default_check_request (socket_t sock);


/*
 * Shorten the receive buffer of sock
 */
#define sock_reduce_recv(sock, len) \
  if (sock->recv_buffer_fill > len) { \
    memmove (sock->recv_buffer, sock->recv_buffer+len, \
             sock->recv_buffer_fill - len); \
  } \
  sock->recv_buffer_fill -= len;

#endif /* not __SOCKET_H__ */
