/*
 * control-proto.c - control protocol implementation
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
 * $Id: control-proto.c,v 1.10 2000/07/01 15:43:40 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_CONTROL_PROTO

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#if HAVE_LIBKSTAT
# include <kstat.h>
# include <sys/sysinfo.h>
#elif HAVE_PSTAT
# include <sys/pstat.h>
#elif HAVE_SYSGET
# include <sys/sysget.h>
# include <sys/sysinfo.h>
#endif

#if HAVE_TIMES
# include <sys/times.h>
#endif

#include "snprintf.h"
#include "util.h"
#include "socket.h"
#include "pipe-socket.h"
#include "server-core.h"
#include "server-socket.h"
#include "server.h"
#include "serveez.h"
#include "coserver/coserver.h"
#include "control-proto.h"

#if ENABLE_HTTP_PROTO
# include "http-server/http-cache.h"
#endif

/*
 * The control port configuration.
 */
portcfg_t ctrl_port =
{
  PROTO_TCP,  /* TCP protocol defintion */
  42424,      /* prefered port */
  "*",        /* prefered local ip address */
  NULL,       /* calculated automatically later */
  NULL,       /* no receiving (listening) pipe */
  NULL        /* no sending pipe */
};

/*
 * The contrl server instance configuration.
 */
ctrl_config_t ctrl_config =
{
  &ctrl_port  /* port configuration only (yet) */
};

/*
 * Defintion of the configuration items processed by libsizzle (taken
 * from the config file).
 */
key_value_pair_t ctrl_config_prototype [] =
{
  REGISTER_PORTCFG ("netport", ctrl_config.netport, DEFAULTABLE),
  REGISTER_END ()
};

/*
 * Defintion of the control protocol server.
 */
server_definition_t ctrl_server_definition =
{
  "control protocol server", /* long server description */
  "control",                 /* short server description (for libsizzle) */
  NULL,                      /* global initializer */
  ctrl_init,                 /* instance initializer */
  ctrl_detect_proto,         /* protocol detection routine */
  ctrl_connect_socket,       /* connection routine */
  ctrl_finalize,             /* instance finalization routine */
  NULL,                      /* global finalizer */
  &ctrl_config,              /* default configuration */
  sizeof (ctrl_config),      /* size of the configuration */
  ctrl_config_prototype      /* configuration prototypes (libsizzle) */
};

/*
 * Within the CTRL_IDLE function this structure gets filled with
 * the apropiate data.
 */
cpu_state_t cpu_state;

/*
 * Server instance initializer.
 */
int
ctrl_init (server_t *server)
{
  ctrl_config_t *cfg = server->cfg;

  server_bind (server, cfg->netport);
  return 0;
}

/*
 * Server instance finalizer.
 */
int
ctrl_finalize (server_t *server)
{
  return 0;
}

/*
 * This function gets called for new sockets which are not yet
 * identified.  It returns a non-zero value when the content in
 * the receive buffer looks like the control protocol.
 */
int
ctrl_detect_proto (void *ctrlcfg, socket_t sock)
{
  ctrl_config_t *cfg = ctrlcfg;
  int ret = 0;

  /* accept both CRLF and CR */
  if (sock->recv_buffer_fill >= 2 && 
      sock->recv_buffer[0] == '\r' &&
      sock->recv_buffer[1] == '\n')
    {
      ret = 2;
    }
  else if (sock->recv_buffer_fill >= 1 && sock->recv_buffer[0] == '\n')
    {
      ret = 1;
    }

  /* control protocol detected */
  if (ret)
    {
      if (ret < sock->recv_buffer_fill)
	{
	  memmove (sock->recv_buffer, 
		   sock->recv_buffer + ret, 
		   sock->recv_buffer_fill - ret);
	}
      sock->recv_buffer_fill -= ret;
#if ENABLE_DEBUG
      log_printf(LOG_DEBUG, "control protocol detected\n");
#endif
      return -1;
    }

  return 0;
}

/*
 * When ctrl_detect_proto has identified a client connection being
 * a control protocol connection you have to call the following 
 * routine.
 */
int
ctrl_connect_socket (void *ctrlcfg, socket_t sock)
{
  ctrl_config_t *cfg = ctrlcfg;

  sock_resize_buffers (sock, sock->send_buffer_size, CTRL_RECV_BUFSIZE);
  sock->check_request = default_check_request;
  sock->handle_request = ctrl_handle_request;
  sock->boundary = CTRL_PACKET_DELIMITER;
  sock->boundary_size = CTRL_PACKET_DELIMITER_LEN;
  sock->idle_func = ctrl_idle;
  sock->idle_counter = CTRL_LOAD_UPDATE;

#if HAVE_PROC_STAT
  cpu_state.cpufile = CPU_FILE_NAME;
  cpu_state.cpuline = CPU_LINE_FORMAT;
#elif HAVE_LIBKSTAT /* not HAVE_PROC_STAT */

#else /* neither HAVE_PROC_STAT nor HAVE_LIBKSTAT */
  strcpy (cpu_state.info, CPU_FORMAT);
#endif

  cpu_state.cpuinfoline = CPU_FORMAT;

  /* send welcome message */
  sock_printf (sock, "%s", CTRL_PASSWD);

  return 0;
}

/*
 * Quit command. If you give this command the control protocol
 * connection will be immediately closed.
 */
int
ctrl_quit (socket_t sock, int flag, char *arg)
{
  return flag;
}

/*
 * Help screen. Here you will get all the available commands of the
 * control protocol. These depend on the features the current serveez
 * implements.
 */
int
ctrl_help (socket_t sock, int flag, char *arg)
{
  sock_printf (
    sock, 
    "\r\n%s",
    " available commands:\r\n"
    "   * help                - this help screen\r\n"
    "   * quit                - quit this control connection\r\n"
#if ENABLE_IDENT
    "   * restart ident       - restart the ident coserver\r\n"
#endif /* ENABLE_IDENT */
#if ENABLE_REVERSE_LOOKUP
    "   * restart reverse dns - restart reverse DNS lookup coserver\r\n"
#endif /* ENABLE_REVERSE_LOOKUP */
#if ENABLE_DNS_LOOKUP
    "   * restart dns         - restart the DNS lookup coserver\r\n"
#endif /* ENABLE_DNS_LOOKUP */
    "   * killall             - shutdown all client connections\r\n"
    "   * kill id NUM         - shutdown connection NUM\r\n"
    "   * stat                - general statistics\r\n"
    "   * stat con            - connection statistics\r\n"
    "   * stat id NUM         - NUM's connnection info\r\n"
    "   * stat all            - all statistics\r\n"
#if ENABLE_HTTP_PROTO
    "   * stat cache          - http cache statistics\r\n"
    "   * kill cache          - free all http cache entries\r\n"
#endif /* ENABLE_HTTP_PROTO */
    "\r\n");

  return flag;
}

/*
 * Snip unprintable characters from a given string. It chops leading and
 * trailing whitespaces.
 */
char *
ctrl_chop (char *arg)
{
  static char text[256];
  char *p;
  char *t;

  p = arg;
  t = text;
  while (*p == ' ') p++;
  while (*p >= ' ') *t++ = *p++;
  while (*t == ' ' && t > text) *t-- = 0;
  return text;
}

/*
 * ID's connection info.
 */
int
ctrl_stat_id (socket_t sock, int flag, char *arg)
{
  int id;
  socket_t xsock;
  char sflags[128] = "";
  char flags[128] = "";
  char proto[128] = "";

  /* find the apropiate client or server connection */
  id = atoi (arg);
  if ((xsock = find_sock_by_id (id)) == NULL)
    {
      sock_printf (sock, "no such connection: %d\r\n", id);
      return flag;
    }

  /* process connection type server flags */
  if (xsock->proto & PROTO_TCP)
    strcat (sflags, "tcp ");
  if (xsock->proto & PROTO_UDP)
    strcat (sflags, "udp ");
  if (xsock->proto & PROTO_PIPE)
    strcat (sflags, "pipe ");

  
  /* process protocol type server flags */
  /* FIXME: server depending string */
  strcat (sflags, "unknown server ");

  /* 
   * Process general socket structure's flags. Uppercase words refer
   * to set bits and lowercase to unset bits.
   */
  sprintf (flags, "%s %s %s %s %s %s %s %s %s %s %s %s %s",
	   xsock->flags & SOCK_FLAG_INBUF ?     "INBUF" : "inbuf",
	   xsock->flags & SOCK_FLAG_OUTBUF ?    "OUTBUF" : "outbuf",
	   xsock->flags & SOCK_FLAG_CONNECTED ? "CONNECT" : "connect",
	   xsock->flags & SOCK_FLAG_LISTENING ? "LISTEN" : "listen",
	   xsock->flags & SOCK_FLAG_AUTH ?      "AUTH" : "auth",
	   xsock->flags & SOCK_FLAG_KILLED ?    "KILL" : "kill",
	   xsock->flags & SOCK_FLAG_NOFLOOD ?   "flood" : "FLOOD",
	   xsock->flags & SOCK_FLAG_INITED ?    "INIT" : "init",
	   xsock->flags & SOCK_FLAG_THREAD ?    "THREAD" : "thread",
	   xsock->flags & SOCK_FLAG_PIPE ?      "PIPE" : "pipe",
	   xsock->flags & SOCK_FLAG_FILE ?      "FILE" : "file",
	   xsock->flags & SOCK_FLAG_SOCK ?      "SOCK" : "sock",
	   xsock->flags & SOCK_FLAG_ENQUEUED ?  "ENQUEUED" : "enqueued");

  /* process protocol specific flags */
  /* FIXME: protocol depending output here ! */
  strcat (proto, "unknown protocol ");

  /* print all previously collected statistics of this connection */
  sock_printf (sock, 
	       "\r\nconnection id %d statistics\r\n\r\n"
	       " sflags   : %s\r\n"
	       " flags    : %s\r\n"
	       " protocol : %s\r\n"
	       " sock fd  : %d\r\n"
	       " file fd  : %d\r\n"
	       " pipe fd  : %d (recv), %d (send)\r\n"
	       " foreign  : %d.%d.%d.%d:%d (%s)\r\n"
	       " local    : %d.%d.%d.%d:%d (%s)\r\n"
	       " sendbuf  : %d (size), %d (fill), %s (last send)\r\n"
	       " recvbuf  : %d (size), %d (fill), %s (last recv)\r\n"
	       " idle     : %d\r\n"
#if ENABLE_FLOOD_PROTECTION
	       " flood    : %d (points), %d (limit)\r\n"
#endif /* ENABLE_FLOOD_PROTECTION */
	       " avail    : %s\r\n\r\n",
	       id,
	       sflags[0] ? sflags : "none",
	       flags,
	       proto[0] ? proto : "none",
	       xsock->sock_desc,
	       xsock->file_desc,
	       xsock->pipe_desc[READ],
	       xsock->pipe_desc[WRITE],
	       (xsock->remote_addr >> 24) & 0xff,
	       (xsock->remote_addr >> 16) & 0xff,
	       (xsock->remote_addr >> 8) & 0xff,
	       xsock->remote_addr & 0xff,
	       xsock->remote_port,
	       xsock->remote_host ? xsock->remote_host : "unresolved",
	       (xsock->local_addr >> 24) & 0xff,
	       (xsock->local_addr >> 16) & 0xff,
	       (xsock->local_addr >> 8) & 0xff,
	       xsock->local_addr & 0xff,
	       xsock->local_port,
	       xsock->local_host ? xsock->local_host : "unresolved",
	       xsock->send_buffer_size,
	       xsock->send_buffer_fill,
	       ctrl_chop (ctime (&xsock->last_send)),
	       xsock->recv_buffer_size,
	       xsock->recv_buffer_fill,
	       ctrl_chop (ctime (&xsock->last_recv)),
	       xsock->idle_counter,
#if ENABLE_FLOOD_PROTECTION
	       xsock->flood_points,
	       xsock->flood_limit,
#endif /* ENABLE_FLOOD_PROTECTION */
	       xsock->unavailable ? "no" : "yes");

  return flag;
}

/*
 * General statistics.
 */	  
int
ctrl_stat (socket_t sock, int flag, char *arg)
{
  char *starttime;

  starttime = ctime (&serveez_config.start_time);
  starttime[strlen (starttime) - 1] = '\0';
  sock_printf (sock, 
	       "\r\nThis is %s version %s running since %s.\r\n", 
	       serveez_config.program_name, 
	       serveez_config.version_string,
	       starttime);
  sock_printf (sock, "Features  :"
#ifdef ENABLE_AWCS_PROTO
	       " AWCS"
#endif
#ifdef ENABLE_HTTP_PROTO
	       " HTTP"
#endif
#ifdef ENABLE_IDENT
	       " IDENT"
#endif
#ifdef ENABLE_REVERSE_LOOKUP
	       " REVERSE-DNS"
#endif
#ifdef ENABLE_DNS_LOOKUP
	       " DNS"
#endif
#ifdef ENABLE_FLOOD_PROTECTION
	       " FLOOD"
#endif
#ifdef ENABLE_DEBUG
	       " DEBUG"
#endif
#ifdef ENABLE_IRC_PROTO
	       " IRC"
#endif
#if ENABLE_CONTROL_PROTO
	       " CTRL"
#endif
#if defined(__MINGW32__) || defined(__CYGWIN__)
	       " WIN32"
#endif
	       "\r\n");

  sock_printf (sock, "Os        : %s\r\n", get_version ());
  sock_printf (sock, "Sys-Load  : %s\r\n", cpu_state.info);
  sock_printf (sock, "Proc-Load : %s\r\n", cpu_state.pinfo);
  sock_printf (sock, "%d connected sockets (hard limit is %d)\r\n",
	       connected_sockets, serveez_config.max_sockets);
  sock_printf (sock, "\r\n");

  return flag;
}

/*
 * Connection statistics.
 */
int
ctrl_stat_con (socket_t sock, int flag, char *arg)
{
  socket_t xsock;
  char *id;
  char linet[64];  
  char rinet[64];
  int n;

  sock_printf(sock, "\r\n%s", 
	      "Proto         Id  RecvQ  SendQ Sock "
	      "Local                Foreign\r\n");

  for (xsock = socket_root; xsock; xsock = xsock->next)
    {
      id = "None";
      if (xsock->proto)
	id = "Server";
      /* FIXME: protocol specific */

      n = xsock->local_addr;
      sprintf (linet, "%d.%d.%d.%d:%d", (n>>24)&0xff, 
	       (n>>16)&0xff, (n>>8)&0xff, n&0xff,
	       xsock->local_port);		  

      n = xsock->remote_addr;
      sprintf (rinet, "%d.%d.%d.%d:%d", (n>>24)&0xff,
	       (n>>16)&0xff, (n>>8)&0xff, n&0xff,
	       xsock->remote_port);
      
      sock_printf (sock, 
		   "%-11s %4d %6d %6d %4d %-20s %-20s\r\n", id,
		   xsock->socket_id, xsock->recv_buffer_fill,
		   xsock->send_buffer_fill, 
		   xsock->sock_desc,
		   linet, rinet);
    }
  sock_printf (sock, "\r\n");

  return flag;
}

#if ENABLE_HTTP_PROTO
/*
 * HTTP cache statistics.
 */
int
ctrl_stat_cache (socket_t sock, int flag, char *arg)
{
  int n, total, files;
  char *p;

  sock_printf (sock, "\r\n%s", 
	       "File                             "
	       "Size  Usage  Hits Recent Ready\r\n");

  for (files = 0, total = 0, n = 0; n < http_cache_entries; n++)
    {
      if (http_cache[n].used)
	{
	  files++;
	  total += http_cache[n].length;
	  p = http_cache[n].file;
	  p += strlen(http_cache[n].file);
	  while(*p != '/' && *p != '\\' && p != http_cache[n].file) p--;
	  if(p != http_cache[n].file) p++;
	  sock_printf(sock, "%-30s %6d %6d %5d %6d %-5s\r\n", p,
		      http_cache[n].length,
		      http_cache[n].usage,
		      http_cache[n].hits,
		      http_cache[n].recent,
		      http_cache[n].ready ? "Yes" : "No");
	}
    }
  sock_printf(sock, "\r\nTotal : %d byte in %d cache entries\r\n\r\n",
	      total, files);

  return flag;
}

/*
 * Free all HTTP cache entries.
 */
int
ctrl_kill_cache (socket_t sock, int flag, char *arg)
{
  sock_printf (sock, "%d HTTP cache entries reinitialized.\r\n",
	       http_cache_entries);
  http_free_cache ();
  http_alloc_cache (http_cache_entries);
  return flag;
}
#endif /* ENABLE_HTTP_PROTO */

/*
 * Connection statitics (short).
 */
int
ctrl_stat_all(socket_t sock, int flag, char *arg)
{
  int client, n;
  int_coserver_t *coserver;
  socket_t xsock;

  sock_printf(sock, "\r\n");
#if ENABLE_AWCS_PROTO
  client = 0;
  n = 0;
  for (xsock = socket_root; xsock; xsock = xsock->next)
    {
      /*
      if(xsock->flags & SOCK_FLAG_AWCS_CLIENT)
	client++;
      if(xsock == master_server)
	n++;
      */
    }
  sock_printf(sock, "aWCS connections: %d clients, %d master\r\n",
	      client, n);
#endif
#if ENABLE_HTTP_PROTO
  client = 0;
  for (xsock = socket_root; xsock; xsock = xsock->next)
    {
      /*
      if(xsock->flags & SOCK_FLAG_HTTP_CLIENT)
	client++;
      */
    }
  sock_printf(sock, "HTTP connections: %d clients\r\n", client);
#endif
#if ENABLE_CONTROL_PROTO
  client = 0;
  for (xsock = socket_root; xsock; xsock = xsock->next)
    {
      /*
      if(xsock->flags & SOCK_FLAG_CTRL_CLIENT)
	client++;
      */
    }
  sock_printf(sock, "Ctrl connections: %d clients\r\n", client);
#endif
#if ENABLE_IRC_PROTO
  client = 0;
  for (xsock = socket_root; xsock; xsock = xsock->next)
    {
      /*
      if(xsock->flags & SOCK_FLAG_IRC_CLIENT)
	client++;
      */
    }
  sock_printf(sock, "IRC  connections: %d clients\r\n", client);
#endif

  for (n = 0; n < int_coservers; n++)
    {
      coserver = &int_coserver[n];
      if (coserver->used)
	{
	  sock_printf (sock, "%d. internal %s coserver\r\n", n + 1,
		       int_coserver_type[coserver->type].name);
	}
    }
  sock_printf (sock, "\r\n");

  return flag;
}

/*
 * Shutdown a specified network connection.
 */
int
ctrl_kill_id (socket_t sock, int flag, char *arg)
{
  int id;
  socket_t xsock;

  id = atoi (arg);
  if ((xsock = find_sock_by_id (id)) == NULL)
    {
      sock_printf (sock, "no such connection: %d\r\n", id);
      return flag;
    }

  sock_schedule_for_shutdown (xsock);
  sock_printf (sock, "scheduled socket id %d for shutdown\r\n", id);
  return flag;
}

/*
 * Shutdown all network connections except listening and CTRL
 * connections.
 */
int
ctrl_killall (socket_t sock, int flag, char *arg)
{
  socket_t xsock;
  int n = 0;

  for (xsock = socket_root; xsock; xsock = xsock->next)
    {
      if(xsock != sock &&
	 !(xsock->flags & SOCK_FLAG_LISTENING))
	if(!(xsock->flags & SOCK_FLAG_PIPE))
	  {
	    sock_schedule_for_shutdown(xsock);
	    n++;
	  }
    }
  sock_printf(sock, "killed %d network connections\r\n", n);

  return flag;
}

/*
 * Restart coservers.
 */	  
int
ctrl_restart(socket_t sock, int type, char *arg)
{
  int_coserver_t *coserver;
  int n;

  for (n = 0; n < int_coservers; n++)
    {
      coserver = &int_coserver[n];
      if (coserver->used && coserver->type == type)
	{
	  destroy_internal_coservers (type);
	  create_internal_coserver (type);
	  sock_printf (sock, "internal %s coserver restarted\r\n",
		       int_coserver_type[type].name);
	  break;
	}
    }
  /* start a new internal coserver */
  if (n == int_coservers)
    {
      create_internal_coserver (type);
      sock_printf (sock, "internal %s coserver invoked\r\n",
		   int_coserver_type[type].name);
    }
  return 0;
}

/*
 * This structure defines the calling conventions for the various
 * control protocol commands.
 */
struct
{
  char *command;                      /* the complete command string */
  int (*func)(socket_t, int, char *); /* callback routine */
  int flag;                           /* second argument */
}
ctrl[] =
{
  { CTRL_CMD_HELP,
    ctrl_help,
    0 },
  { CTRL_CMD_QUIT, 
    ctrl_quit,
    -1 },
  { CTRL_CMD_STAT_CON,
    ctrl_stat_con,
    0 },
  { CTRL_CMD_STAT_ID,
    ctrl_stat_id,
    0 },
  { CTRL_CMD_STAT_ALL,
    ctrl_stat_all,
    0 },
#if ENABLE_HTTP_PROTO
  { CTRL_CMD_STAT_CACHE,
    ctrl_stat_cache, 
    0 },
  { CTRL_CMD_KILL_CACHE,
    ctrl_kill_cache, 
    0 },
#endif
  { CTRL_CMD_STAT,
    ctrl_stat,
    0 },
  { CTRL_CMD_KILLALL,
    ctrl_killall,
    0 },
  { CTRL_CMD_KILL_ID,
    ctrl_kill_id,
    0 },
#if ENABLE_REVERSE_LOOKUP
  { CTRL_CMD_RESTART_REVERSE_DNS,
    ctrl_restart,
    COSERVER_REVERSE_DNS },
#endif
#if ENABLE_IDENT
  { CTRL_CMD_RESTART_IDENT,
    ctrl_restart,
    COSERVER_IDENT },
#endif
#if ENABLE_REVERSE_LOOKUP
  { CTRL_CMD_RESTART_DNS,
    ctrl_restart,
    COSERVER_DNS },
#endif
  { NULL,
    NULL,
    0 }
};

/*
 * The ctrl_handle_request routine gets called by the check_request
 * routine of the control protocol.
 */
int
ctrl_handle_request (socket_t sock, char *request, int len)
{
  static char last_request[CTRL_RECV_BUFSIZE];
  static int last_len;
  int n;
  int ret = 0;
  int l;

  while (request[len-1] == '\r' || request[len-1] == '\n')
    len--;
  
  if (!(sock->userflags & CTRL_FLAG_PASSED))
    {
      /*
       * check here the control protocol password
       */
      if (len <= 2) return -1;
      if (!memcmp (request, serveez_config.server_password, len - 1))
	{
	  sock->userflags |= CTRL_FLAG_PASSED;
	  sock_printf (sock, "Login ok.\r\n%s", CTRL_PROMPT);
	}
      else return -1;
    }
  else if (len > 0)
    {
      if (!memcmp (request, "/\r\n", 3))
	{
	  memcpy (request, last_request, len = last_len);
	}
      n = 0;
      while (ctrl[n].command != NULL)
	{
	  l = strlen (ctrl[n].command);
	  if (!memcmp (request, ctrl[n].command, l))
	    {
	      memcpy (last_request, request, last_len = len);
	      ret = ctrl[n].func (sock, ctrl[n].flag, &request[l+1]);
	      sock_printf (sock, "%s", CTRL_PROMPT);
	      return ret;
	    }
	  n++;
	}
      l = 0;
      while (l < len && request[l] >= ' ') l++;
      request[l] = 0;
      sock_printf (sock, "no such command: %s\r\n", request);
      sock_printf (sock, "%s", CTRL_PROMPT);
    }
  else
    {
      sock_printf (sock, "%s", CTRL_PROMPT);
    }
  return ret;
}

/*
 * Depending on the systems this routine gets the cpu load. 
 * Returns -1 if an error occured.
 * Linux   -- /proc/stat
 * HP-Unix -- pstat_getdynamic()
 * Solaris -- kstat_read()
 */
int
get_cpu_state(void)
{
  int n;

#if HAVE_LIBKSTAT
  static kstat_ctl_t *kc;
  static kstat_t *ksp = NULL;
  static cpu_stat_t cs;
#elif HAVE_PROC_STAT
  FILE *f;
  static char stat[STAT_BUFFER_SIZE];
#elif HAVE_PSTAT
  struct pst_dynamic stats;
#elif HAVE_SYSGET
  struct sysinfo_cpu info;
  sgt_cookie_t cookie;
#endif

#if HAVE_TIMES
  struct tms proc_tms;
#endif

  n = (cpu_state.index + 1) & 1;

#if HAVE_TIMES
  cpu_state.ptotal[n] = times (&proc_tms);
  cpu_state.proc[n][0] = proc_tms.tms_utime;
  cpu_state.proc[n][1] = proc_tms.tms_stime;
  cpu_state.proc[n][2] = proc_tms.tms_cutime;
  cpu_state.proc[n][3] = proc_tms.tms_cstime;
#else /* not HAVE_TIMES */
  cpu_state.ptotal[n] = cpu_state.ptotal[cpu_state.index] + 
    (CLOCKS_PER_SEC * CTRL_LOAD_UPDATE);
  cpu_state.proc[n][0] = clock ();
#endif /* not HAVE_TIMES */

#if HAVE_LIBKSTAT /* Solaris */

  if(ksp == NULL)
    {
      kc = kstat_open(); 

      for(ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next) 
	if(strncmp(ksp->ks_name, "cpu_stat", 8) == 0) 
	  break;
    }
  else
    {
      if(kstat_read(kc, ksp, &cs) == -1) 
	{
	  snprintf(cpu_state.info, STAT_BUFFER_SIZE, 
		   "kstat_read() failed");
	  return -1;
	}

      cpu_state.cpu[n][0] = cs.cpu_sysinfo.cpu[CPU_USER];
      cpu_state.cpu[n][1] = cs.cpu_sysinfo.cpu[CPU_KERNEL];
      cpu_state.cpu[n][2] = cs.cpu_sysinfo.cpu[CPU_WAIT];
      cpu_state.cpu[n][3] = cs.cpu_sysinfo.cpu[CPU_IDLE];
    }

#elif HAVE_PROC_STAT /* Linux */

  /* open the statistics file */
  if((f = fopen(cpu_state.cpufile, "r")) == NULL)
    {
      snprintf(cpu_state.info, STAT_BUFFER_SIZE, 
	       "%s not available", cpu_state.cpufile);
      return -1;
    }

  /* find the apropiate cpu statistics line */
  while(fgets(stat, STAT_BUFFER_SIZE, f))
    {
      if(4 == sscanf(stat, cpu_state.cpuline, 
		     &cpu_state.cpu[n][0], 
		     &cpu_state.cpu[n][1], 
		     &cpu_state.cpu[n][2], 
		     &cpu_state.cpu[n][3]))
	{
	  fclose(f);
	  return 0;
	}
    }

  /* cpu line not found */
  snprintf(cpu_state.info, STAT_BUFFER_SIZE, 
	   "cpu line not found in %s", cpu_state.cpufile);
  fclose(f);

#elif HAVE_PSTAT /* HP Unix */

  pstat_getdynamic(&stats, sizeof(struct pst_dynamic), 1, 0);

  cpu_state.cpu[n][0] = stats.psd_cpu_time[0];
  cpu_state.cpu[n][1] = stats.psd_cpu_time[1];
  cpu_state.cpu[n][2] = stats.psd_cpu_time[2];
  cpu_state.cpu[n][3] = stats.psd_cpu_time[3];

#elif HAVE_SYSGET /* Irix */

  SGT_COOKIE_INIT(&cookie);
  sysget(SGT_SINFO_CPU, (char *)&info, sizeof(info), SGT_READ, &cookie);

  cpu_state.cpu[n][0] = info.cpu[CPU_USER];
  cpu_state.cpu[n][1] = info.cpu[CPU_KERNEL];
  cpu_state.cpu[n][2] = info.cpu[CPU_WAIT];
  cpu_state.cpu[n][3] = info.cpu[CPU_IDLE];
  
#endif
  return 0;
}

/*
 * Within the CTRL_IDLE function the server gets the CPU
 * load. This procedure differs on different platforms.
 */

#define PROC_DIFF(x) (c->proc[n][x] - c->proc[old][x])
#define CPU_DIFF(x) (c->cpu[n][x] - c->cpu[old][x])

int
ctrl_idle (socket_t sock)
{
  int n, old;
  unsigned long all;
  cpu_state_t *c = &cpu_state;

  old = c->index;
  n = (c->index + 1) & 1;

  /* get status of the cpu and process */
  if (get_cpu_state () != -1)
    {
      /* calculate process specific info */
      all = c->ptotal[n] - c->ptotal[old]; 
      if (all != 0)
	{
	  snprintf (c->pinfo, STAT_BUFFER_SIZE, PROC_FORMAT,
		    PROC_DIFF(0) * 100 / all,
		    PROC_DIFF(0) * 1000 / all % 10,
		    PROC_DIFF(1) * 100 / all,
		    PROC_DIFF(1) * 1000 / all % 10,
		    PROC_DIFF(2) * 100 / all,
		    PROC_DIFF(2) * 1000 / all % 10,
		    PROC_DIFF(3) * 100 / all,
		    PROC_DIFF(3) * 1000 / all % 10);
	}

      /* calculate cpu specific info */
      c->total[n] = c->cpu[n][0] + c->cpu[n][1] + c->cpu[n][2] + c->cpu[n][3];
      all = c->total[n] - c->total[old];
      if (all != 0)
	{
	  snprintf (c->info, STAT_BUFFER_SIZE, c->cpuinfoline,
		    CPU_DIFF (0) * 100 / all,
		    CPU_DIFF (0) * 1000 / all % 10,
		    CPU_DIFF (1) * 100 / all,
		    CPU_DIFF (1) * 1000 / all % 10,
		    CPU_DIFF (2) * 100 / all,
		    CPU_DIFF (2) * 1000 / all % 10,
		    CPU_DIFF (3) * 100 / all,
		    CPU_DIFF (3) * 1000 / all % 10);
	}
    }
  
  c->index = n;
  sock->idle_counter = CTRL_LOAD_UPDATE;
  return 0;
}

int have_ctrl = 1;

#else /* ENABLE_CONTROL_PROTO */

int have_ctrl = 0; /* shut up compiler */

#endif /* ENABLE_CONTROL_PROTO */
