/*
 * socket.h - socket management definition
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: socket.h,v 1.32 2001/01/24 15:55:28 ela Exp $
 *
 */

#ifndef __SOCKET_H__
#define __SOCKET_H__ 1

#include <internal.h>

#include <time.h>

/* This is how many Id's can exist. It MUST be a 2^X and less than 10000. */
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

#define SOCK_FLAG_INIT        0x00000000 /* Value for initializing. */
#define SOCK_FLAG_INBUF       0x00000001 /* Outbuf is allocated. */
#define SOCK_FLAG_OUTBUF      0x00000002 /* Inbuf is allocated. */
#define SOCK_FLAG_CONNECTED   0x00000004 /* Socket is connected. */
#define SOCK_FLAG_LISTENING   0x00000008 /* Socket is listening. */
#define SOCK_FLAG_KILLED      0x00000010 /* Socket will be shut down soon. */
#define SOCK_FLAG_NOFLOOD     0x00000020 /* Flood protection off. */
#define SOCK_FLAG_INITED      0x00000040 /* Socket was initialized. */
#define SOCK_FLAG_ENQUEUED    0x00000080 /* Socket is on socket queue. */
#define SOCK_FLAG_RECV_PIPE   0x00000100 /* Receiving pipe is active. */
#define SOCK_FLAG_SEND_PIPE   0x00000200 /* Sending pipe is active. */
#define SOCK_FLAG_FILE        0x00000400 /* Socket is no socket, but file. */
#define SOCK_FLAG_COSERVER    0x00000800 /* Socket is a Co-Server */
#define SOCK_FLAG_SOCK        0x00001000 /* Socket is a plain socket. */
/* Socket is no socket, but pipe. */
#define SOCK_FLAG_PIPE \
  ( SOCK_FLAG_RECV_PIPE | \
    SOCK_FLAG_SEND_PIPE )
#define SOCK_FLAG_CONNECTING  0x00002000 /* Socket is still connecting */
#define SOCK_FLAG_PRIORITY    0x00004000 /* Enqueue socket preferred. */
#define SOCK_FLAG_FIXED       0x00008000 /* Dedicated UDP connection. */

#define SOCK_FLAG_FINAL_WRITE 0x00010000 /* Disconnect as soon as send 
					    queue is empty. */

#define VSNPRINTF_BUF_SIZE 2048 /* Size of the vsnprintf() buffer */

typedef struct socket * socket_t;
typedef struct socket socket_data_t;

struct socket
{
  socket_t next;		/* Next socket in chain. */
  socket_t prev;		/* Previous socket in chain. */
  int id;		        /* Unique ID for this socket. */
  int version;                  /* Socket version */

  int proto;                    /* Server/Protocol flag. */
  int flags;			/* One of the SOCK_FLAG_* flags above. */
  int userflags;                /* Can be used for protocol specific flags. */
  SOCKET sock_desc;		/* Socket descriptor. */
  int file_desc;		/* Used for files descriptors. */
  HANDLE pipe_desc[2];          /* Used for the pipes and coservers. */

#ifdef __MINGW32__
  LPOVERLAPPED overlap[2];      /* the overlap info for WinNT */
#endif /* not __MINGW32__ */

  char *recv_pipe;              /* File of the receive pipe. */
  char *send_pipe;              /* File of the send pipe. */
  socket_t referrer;            /* Referring socket structure. */

  char *boundary;               /* Packet boundary. */
  int boundary_size;            /* Packet boundary length */

  /* The following items always MUST be in network byte order. */
  unsigned short remote_port;	/* Port number of remote end. */
  unsigned long remote_addr;	/* IP address of remote end. */
  unsigned short local_port;	/* Port number of local end. */
  unsigned long local_addr;	/* IP address of local end. */

  char *send_buffer;		/* Buffer for outbound data. */
  char *recv_buffer;		/* Buffer for inbound data. */
  int send_buffer_size;		/* Size of SEND_BUFFER. */
  int recv_buffer_size;		/* Size of RECV_BUFFER. */
  int send_buffer_fill;		/* Valid bytes in SEND_BUFFER. */
  int recv_buffer_fill;		/* Valid bytes in RECV_BUFFER. */

  unsigned short sequence;      /* Currently received sequence. */
  unsigned short send_seq;      /* Send stream sequence number. */
  unsigned short recv_seq;      /* Receive stream sequence number. */

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
   * CONNECTED_SOCKET gets called whenever the socket is finally 
   * connected.
   */
  int (* connected_socket) (socket_t sock);

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
  int flood_points;		/* Accumulated flood points. */
  int flood_limit;		/* Limit of the above before kicking. */
#endif

  /* 
   * Set to non-zero time() value if the the socket is temporarily
   * unavailable (EAGAIN). This is why we use O_NONBLOCK socket
   * descriptors.
   */
  int unavailable;              

  /*
   * Miscellaneous field. Listener keeps array of server instances here.
   * This array is NULL terminated.
   */
  void *data;

  /*
   * When the final protocol detection in DEFAULT_DETECT_PROTO
   * has been done CFG should get the actual configuration hash.
   */
  void *cfg;
};

__BEGIN_DECLS

/*
 * SOCK_LOOKUP_TABLE is used to speed up references to socket
 * structures by socket's id.
 */
extern socket_t sock_lookup_table[SOCKET_MAX_IDS];

/*
 * Count the number of currently connected sockets.
 */
SERVEEZ_API extern SOCKET sock_connections;

/*
 * Check if a given socket is still valid. Return non-zero if it is
 * not.
 */
SERVEEZ_API int sock_valid __P ((socket_t sock));

/*
 * Allocate a structure of type socket_t and initialize
 * its fields.
 */
SERVEEZ_API socket_t sock_alloc __P ((void));

/*
 * Free the socket structure SOCK.  Return a non-zero value on error.
 */
SERVEEZ_API int sock_free __P ((socket_t sock));

/*
 * Create a socket structure from the file descriptor FD.  Return NULL
 * on error.
 */
SERVEEZ_API socket_t sock_create __P ((int fd));

/*
 * Disconnect the socket SOCK from the network and calls the
 * disconnect function for the socket if set.  Return a non-zero
 * value on error.
 */
SERVEEZ_API int sock_disconnect __P ((socket_t sock));

/*
 * Write LEN bytes from the memory location pointed to by BUF
 * to the output buffer of the socket SOCK.  Also try to flush the
 * buffer to the network socket of SOCK if possible.  Return a non-zero
 * value on error, which normally means a buffer overflow.
 */
SERVEEZ_API int sock_write __P ((socket_t sock, char *buf, int len));

/*
 * Print a formatted string on the socket SOCK.  FMT is the printf()-
 * style format string, which describes how to format the optional
 * arguments.  See the printf(3) manual page for details.
 */
SERVEEZ_API int sock_printf __P ((socket_t sock, const char *fmt, ...));

/*
 * Resize the send and receive buffers for the socket SOCK.  SEND_BUF_SIZE
 * is the new size for the send buffer, RECV_BUF_SIZE for the receive
 * buffer.  Note that data may be lost when the buffers shrink.
 */
SERVEEZ_API int sock_resize_buffers __P ((socket_t sock, int, int));

/*
 * Get local and remote addresses/ports of socket SOCK and intern them
 * into the socket structure.
 */
SERVEEZ_API int sock_intern_connection_info __P ((socket_t sock));

/*
 * Get and clear the pending socket error of a given socket. Print
 * the result to the log file.
 */
SERVEEZ_API int sock_error_info __P ((socket_t sock));

/*
 * Calculate unique socket structure id and assign a version 
 * for a given SOCK. The version is for validating socket structures.
 * It is currently used in the coserver callbacks.
 */
SERVEEZ_API int sock_unique_id __P ((socket_t sock));

/*
 * Lots of default callbacks.
 */
SERVEEZ_API int sock_default_write __P ((socket_t sock));
SERVEEZ_API int sock_default_read __P ((socket_t sock));
SERVEEZ_API int sock_default_detect_proto __P ((socket_t sock));
SERVEEZ_API int sock_default_check_request __P ((socket_t sock));
SERVEEZ_API int sock_default_idle_func __P ((socket_t sock));
#if ENABLE_FLOOD_PROTECTION
SERVEEZ_API int sock_default_flood_protect __P ((socket_t sock, int));
#endif

__END_DECLS

/*
 * Shorten the receive buffer of SOCK by len bytes.
 */
#define sock_reduce_recv(sock, len)                      \
  if (len && sock->recv_buffer_fill > len) {             \
    memmove (sock->recv_buffer, sock->recv_buffer + len, \
             sock->recv_buffer_fill - len);              \
  }                                                      \
  sock->recv_buffer_fill -= len;

#endif /* not __SOCKET_H__ */
