/*
 * irc-proto.h - IRC protocol header definitions
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
 * $Id: irc-proto.h,v 1.7 2000/07/17 16:15:04 ela Exp $
 *
 */

#ifndef __IRC_PROTO_H__
#define __IRC_PROTO_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <time.h>

#include "hash.h"
#include "server.h"
#include "irc-core/irc-core.h"

/* Some restrictions. */
#define MAX_CHANNEL_LEN       200 /* maximum channel name length */
#define INVALID_CHANNEL_CHARS "\007, "

#define IRC_PING_INTERVAL (3*60)  /* three (3) minutes intervals */

#define MAX_MODE_LEN   32   /* length of mode string (user or channel) */
#define MAX_NICK_LEN   16   /* nick name length */
#define MAX_NAME_LEN   256  /* name length */
#define MAX_MSG_LEN    512  /* length of an IRC message */

#define MAX_CHANNELS   32   /* maximum amount of channels per client */
#define MAX_CLIENTS    128  /* maximum amount of clients per channels */
#define MAX_SERVERS    4    /* servers this servers can connect to */
#define MAX_HOSTS      256  /* maximum allowed IRC servers */
#define MAX_OPERS      4    /* maximum allowed IRC operators */
#define MAX_MOTD_LINES 256  /* Message of the Day lines */
#define MOTD_LINE_LEN  80   /* lenght of one MOTD line */

#if ENABLE_TIMESTAMP  
# define TS_CURRENT 1       /* current (highest) TS version */
# define TS_MIN     1       /* the lowest TS version  */
# define TS_PASS    "TS"    /* passed as second arg in PASS */
#endif

/*
 * Useful typedefs.
 */
typedef unsigned char byte;
typedef struct irc_client irc_client_t;
typedef struct irc_client_history irc_client_history_t;
typedef struct irc_ban irc_ban_t;
typedef struct irc_channel irc_channel_t;
typedef struct irc_server irc_server_t;

/*
 * Client structure.
 */
struct irc_client
{
  char nick[MAX_NICK_LEN];     /* nick name */
  char real[MAX_NAME_LEN];     /* real name */
  char user[MAX_NAME_LEN];     /* user name (ident) */
  char host[MAX_NAME_LEN];     /* host name (nslookup) */
  char server[MAX_NAME_LEN];   /* the server the client is connected to */
  /* array of channels this client joined */
  irc_channel_t *channel[MAX_CHANNELS]; 
  int channels;                /* amount of channels the client joined */
  socket_t sock;               /* this clients socket structure */
  int flag;                    /* this client's user flags */
  byte key;                    /* the key */
  char pass[MAX_NAME_LEN];     /* the given password */
  char away[MAX_MSG_LEN];      /* the away message if UMODE_AWAY is set */
  int hopcount;                /* the client's hopcount (server distance) */
  time_t since;                /* signon time */
  int ping;                    /* ping <-> pong counter */
};

/*
 * Client history structure.
 */
struct irc_client_history
{
  char nick[MAX_NICK_LEN];     /* nick name */
  char real[MAX_NAME_LEN];     /* real name */
  char user[MAX_NAME_LEN];     /* user name (ident) */
  char host[MAX_NAME_LEN];     /* host name (nslookup) */
  void *next;                  /* next client in the history list */
};

/*
 * Ban Mask structure.
 */
struct irc_ban
{
  char nick[MAX_NAME_LEN]; /* nick name */
  char user[MAX_NAME_LEN]; /* user name */
  char host[MAX_NAME_LEN]; /* host name */
  char by[MAX_NAME_LEN];   /* created by  */
  time_t since;            /* banned since */
};

/*
 * Channel structure.
 */
struct irc_channel
{
  char name[MAX_NAME_LEN];        /* channel name (max. 200 characters) */
  char topic[MAX_NAME_LEN];       /* current topic */
  char topic_by[MAX_NAME_LEN];    /* topic set by */
  time_t topic_since;             /* topic set at */
  /* array of clients in this channel */
  irc_client_t *client[MAX_CLIENTS]; 
  int cflag[MAX_CLIENTS];         /* these clients's channel flags */
  int clients;                    /* clients in this channel */
  int flag;                       /* channel flags */
  irc_ban_t *ban[MAX_CLIENTS];    /* channel bans */
  int bans;                       /* amount of active channel bans */
  int users;                      /* user limit */
  char key[MAX_NAME_LEN];         /* the key */
  char by[MAX_NAME_LEN];          /* this channel is created by */
  time_t since;                   /* channel exists since */
  char *invite[MAX_CLIENTS];      /* invited clients's nick names */
  int invites;                    /* number of invited clients */
};

/*
 * Server structure.
 */
struct irc_server
{
  char *realhost;                 /* real host */
  unsigned addr;                  /* the actual network address */
  unsigned short port;            /* tcp port */
  char *host;                     /* server name (virtual host) */
  char *pass;                     /* password */
  int id;                         /* socket id */
  void *cfg;                      /* irc server configuration hash */
  void *next;                     /* next server in the list */
};

/*
 * IRC server configuration hash.
 */
typedef struct
{
  portcfg_t *netport;             /* port configuration */
  int operators;                  /* amount of logged in server operators */
  int users;                      /* amount of logged in users */
  int unknowns;                   /* unknown connections */
  int invisibles;                 /* invisible users */

  char *host;                     /* local server virtual host */
  char *realhost;                 /* local server host */
  int port;
  int users_disabled;             /* is USERS command disabled ? */

#if ENABLE_TIMESTAMP  
  time_t tsdelta;                 /* delta value to UTC */
#endif

  char *MOTD[MAX_MOTD_LINES];     /* the message of the day */
  int MOTDs;                      /* amount of lines in this message */
  time_t MOTD_lastModified;       /* last modified date */
  char *MOTD_file;                /* the file name */

  /* 
   * M [Mandatory] -- this IRC server's configuration
   *
   * :virtual host name
   * :optional bind address (real host name)
   * :text name
   * :port
   */
  char *MLine;

  /* 
   * A [Mandatory] -- administrative info, printed by the /ADMIN command
   *
   * :administrative info (department, university)
   * :the server's geographical location
   * :email address for a person responsible for the irc server
   */
  char *ALine;

  /* 
   * Y [Suggested] -- connection class
   *
   * :class number (higher numbers refer to a higher priority)
   * :ping frequency (in seconds)
   * :connect frequency in seconds for servers, 0 for client class
   * :maximum number of links in this class 
   * :send queue size
   */
  char **YLine;

  /*
   * I [Mandatory] -- authorize client, wildcards permitted, a valid client
   *                  is matched "user@ip" OR "user@host"
   *
   * :user@ip, you can specify "NOMATCH" here to force matching user@host
   * :password (optional)
   * :user@host
   * :password (optional)
   * :connection class number (YLine)
   */
  char **ILine;

  /*
   * O [Optional] -- authorize operator, wildcards allowed
   * :user@host, user@ forces checking ident
   * :password
   * :nick
   */
  char **OLine;

  /*
   * o [Optional] -- authorize local operator, see above at the O lines for
   *                 description
   */
  char **oLine;

  /*
   * Note: + C and N lines can also use the user@ combination in order
   *         to check specific users (ident) starting servers
   *       + C and N lines are usually given in pairs
   */

  /*
   * C [Networked] -- server to connect to
   * :host name
   * :password
   * :server name (virtual)
   * :port (if not given we will not connect)
   * :connection class number (YLine)
   */
  char **CLine;

  /*
   * N [Networked] -- server which may connect
   * :host name
   * :password
   * :server name (virtual host name)
   * :password
   * :how many components of your own server's name to strip off the
   *  front and be replaced with a *.
   * :connection class number (YLine)
   */
  char **NLine;

  /*
   * K [Optional] -- kill user, wildcards allowed
   * :host
   * :time of day
   * :user
   */
  char **KLine;

  char *pass;                     /* server password */
  char *info;                     /* server info */

  char *email;                    /* email */
  char *admininfo;                /* administrative info */
  char *location1;                /* city, state, country */
  char *location2;                /* university, department */
  char *oper_host[MAX_OPERS];     /* accept operator hosts from */
  char *oper_pass[MAX_OPERS];     /* accept operator passwords (use crypt) */

  hash_t *channels; /* channel list root */
  hash_t *clients;  /* client list root */
  irc_server_t *servers;  /* server list root */
  irc_client_history_t *history; /* client history list root */
}
irc_config_t;

/* Export the irc server definition to `server.c'. */
extern server_definition_t irc_server_definition;

/* these functions can be used by all of the IRC event subsections */
int irc_client_in_channel (socket_t, irc_client_t *, irc_channel_t *);
int irc_check_args (socket_t, irc_client_t *, irc_config_t *, 
		    irc_request_t *, int);
int irc_client_absent (irc_client_t *, irc_client_t *);
#ifndef __STDC__
int irc_printf ();
#else
int irc_printf (socket_t, const char *, ...);
#endif

/* serveez callbacks */
int irc_handle_request (socket_t sock, char *request, int len);
int irc_disconnect (socket_t sock);
int irc_idle (socket_t sock);

/* channel operations */
static int irc_delete_channel (irc_config_t *cfg, irc_channel_t *);
static irc_channel_t *irc_add_channel (irc_config_t *cfg, char *channel);
irc_channel_t *irc_find_channel (irc_config_t *cfg, char *channel);
irc_channel_t **irc_regex_channel (irc_config_t *cfg, char *regex);
int irc_join_channel (irc_config_t *cfg, irc_client_t *client, char *chan);
int irc_leave_channel (irc_config_t *, irc_client_t *, irc_channel_t *);
int irc_leave_all_channels (irc_config_t *, irc_client_t *, char *reason);

/* client operations */
void irc_delete_client_history (irc_config_t *cfg);
void irc_add_client_history (irc_config_t *cfg, irc_client_t *cl);
int irc_delete_client (irc_config_t *cfg, irc_client_t *cl);
irc_client_t *irc_add_client (irc_config_t *cfg, irc_client_t *client);
irc_client_t *irc_create_client (irc_config_t *cfg);
irc_client_t *irc_find_nick (irc_config_t *cfg, char *nick);
irc_client_t *irc_find_userhost (irc_config_t *cfg, char *user, char *host);
irc_client_t **irc_regex_nick (irc_config_t *cfg, char *regex);
irc_client_history_t *irc_find_nick_history (irc_config_t *, 
					     irc_client_history_t *, char *);

/* irc server functions */
int irc_init (server_t *server);
int irc_global_init (void);
int irc_finalize (server_t *server);
int irc_global_finalize (void);

#define IRC_CLOSING_LINK    "Closing Link: %s (%s)"
#define IRC_CONNECTION_LOST "Connection reset by peer"

/* Channel Modes. */
#define MODE_OPERATOR 0x0001 /* give/take channel operator privileges */
#define MODE_PRIVATE  0x0002 /* private channel flag */
#define MODE_SECRET   0x0004 /* secret channel flag */
#define MODE_INVITE   0x0008 /* invite-only channel flag */
#define MODE_TOPIC    0x0010 /* topic settable by channel operator only flag */
#define MODE_MESSAGE  0x0020 /* no messages to channel from outside clients  */
#define MODE_MODERATE 0x0040 /* moderated channel */
#define MODE_ULIMIT   0x0080 /* set the user limit to channel */
#define MODE_BAN      0x0100 /* set a ban mask to keep users out */
#define MODE_VOICE    0x0200 /* ability to speak on a moderated channel */
#define MODE_KEY      0x0400 /* set a channel key (password) */
#define CHANNEL_MODES "opsitnmlbvk"

/* User Modes. */
#define UMODE_INVISIBLE 0x0001 /* marks a users as invisible */
#define UMODE_SERVER    0x0002 /* marks a user for receipt of server notices */
#define UMODE_WALLOP    0x0004 /* user receives wallops */
#define UMODE_OPERATOR  0x0008 /* operator flag */
#define USER_MODES      "iswo"

/* Additional flags. */
#define UMODE_AWAY      0x0010 /* away flag */
#define UMODE_PASS      0x0020 /* password + crypt flag */
#define UMODE_NICK      0x0040 /* nick flag */
#define UMODE_USER      0x0080 /* user flag */
#define UMODE_IDENT     0x0100 /* identification flag */
#define UMODE_DNS       0x0200 /* nslookup flag */

#define UMODE_REGISTERED (UMODE_NICK | UMODE_USER | UMODE_PASS)

/* Error Replies. */
#define ERR_NOSUCHNICK            401
#define ERR_NOSUCHNICK_TEXT       "%s :No such nick/channel."

#define ERR_NOSUCHSERVER          402
#define ERR_NOSUCHSERVER_TEXT     "%s :No such server"

#define ERR_NOSUCHCHANNEL         403
#define ERR_NOSUCHCHANNEL_TEXT    "%s :No such channel."

#define ERR_CANNOTSENDTOCHAN      404
#define ERR_CANNOTSENDTOCHAN_TEXT "%s :Cannot send to channel."

#define ERR_TOOMANYCHANNELS       405

#define ERR_WASNOSUCHNICK         406
#define ERR_WASNOSUCHNICK_TEXT    "%s :There was no such nickname"

#define ERR_TOOMANYTARGETS        407

#define ERR_NOORIGIN              409
#define ERR_NOORIGIN_TEXT         ":No origin specified"

#define ERR_NORECIPIENT           411

#define ERR_NOTEXTTOSEND          412
#define ERR_NOTEXTTOSEND_TEXT     ":No text to send"

#define ERR_NOTOPLEVEL            413
#define ERR_WILDTOPLEVEL          414

#define ERR_UNKNOWNCOMMAND        421
#define ERR_UNKNOWNCOMMAND_TEXT   "%s :Unknown command"

#define ERR_NOMOTD                422
#define ERR_NOMOTD_TEXT           ":MOTD File is missing"

#define ERR_NOADMININFO           423
#define ERR_FILEERROR             424

#define ERR_NONICKNAMEGIVEN       431
#define ERR_NONICKNAMEGIVEN_TEXT  ":No nickname given"

#define ERR_ERRONEUSNICKNAME      432
#define ERR_ERRONEUSNICKNAME_TEXT "%s :Erroneus nickname"

#define ERR_NICKNAMEINUSE         433
#define ERR_NICKNAMEINUSE_TEXT    "%s :Nickname is already in use"

#define ERR_NICKCOLLISION         436
#define ERR_USERNOTINCHANNEL      441

#define ERR_NOTONCHANNEL          442
#define ERR_NOTONCHANNEL_TEXT     "%s :You're not on that channel."

#define ERR_USERONCHANNEL         443
#define ERR_USERONCHANNEL_TEXT    "%s %s :is already on channel."

#define ERR_NOLOGIN               444
#define ERR_SUMMONDISABLED        445
#define ERR_USERSDISABLED         446
#define ERR_USERSDISABLED_TEXT    ":USERS has been disabled"
#define ERR_NOTREGISTERED         451

#define ERR_NEEDMOREPARAMS        461
#define ERR_NEEDMOREPARAMS_TEXT   "%s :Not enough parameters."

#define ERR_ALREADYREGISTRED      462
#define ERR_ALREADYREGISTRED_TEXT ":You may not reregister"

#define ERR_NOPERMFORHOST         463
#define ERR_PASSWDMISMATCH        464
#define ERR_YOUREBANNEDCREEP      465

#define ERR_KEYSET                467
#define ERR_KEYSET_TEXT           "%s :Channel key already set."

#define ERR_CHANNELISFULL         471
#define ERR_CHANNELISFULL_TEXT    "%s :Cannot join channel (+l)"

#define ERR_UNKNOWNMODE           472
#define ERR_UNKNOWNMODE_TEXT      "%c :is unknown mode char to me."

#define ERR_INVITEONLYCHAN        473
#define ERR_INVITEONLYCHAN_TEXT   "%s :Cannot join channel (+i)"

#define ERR_BANNEDFROMCHAN        474
#define ERR_BANNEDFROMCHAN_TEXT   "%s :Cannot join channel (+b)"

#define ERR_BADCHANNELKEY         475
#define ERR_BADCHANNELKEY_TEXT    "%s :Cannot join channel (+k)"

#define ERR_NOPRIVILEGES          481
#define ERR_NOPRIVILEGES_TEXT     ":Permission Denied- " \
                                  "You're not an IRC operator"

#define ERR_CHANOPRIVSNEEDED      482
#define ERR_CHANOPRIVSNEEDED_TEXT "%s :You're not channel operator."

#define ERR_CANTKILLSERVER        483
#define ERR_NOOPERHOST            491
#define ERR_UMODEUNKNOWNFLAG      501

#define ERR_USERSDONTMATCH        502
#define ERR_USERSDONTMATCH_TEXT   ":Cant change mode for other users"

/*  Command responses. */
#define RPL_WELCOME               001
#define RPL_WELCOME_TEXT          "Welcome to the Internet Relay Chat, %s !"

#define RPL_YOURHOST              002
#define RPL_YOURHOST_TEXT         "Your host is %s, running version %s-%s"

#define RPL_CREATED               003
#define RPL_CREATED_TEXT          "This server was created %s"

#define RPL_MYINFO                004
#define RPL_MYINFO_TEXT           "%s %s-%s %s %s"

#define RPL_NONE                  300
#define RPL_USERHOST              302
#define RPL_ISON                  303

#define RPL_AWAY                  301
#define RPL_AWAY_TEXT             "%s :%s"

#define RPL_UNAWAY                305
#define RPL_UNAWAY_TEXT           ":You are no longer marked as being away"

#define RPL_NOWAWAY               306
#define RPL_NOWAWAY_TEXT          ":You have been marked as being away"

#define RPL_WHOISUSER             311
#define RPL_WHOISUSER_TEXT        "%s %s %s * :%s"

#define RPL_WHOISSERVER           312
#define RPL_WHOISSERVER_TEXT      "%s %s :%s"

#define RPL_WHOISOPERATOR         313
#define RPL_WHOISOPERATOR_TEXT     "%s :is an IRC operator"

#define RPL_WHOISIDLE             317
#define RPL_WHOISIDLE_TEXT        "%s %d %d :seconds idle, signon time"

#define RPL_ENDOFWHOIS            318
#define RPL_ENDOFWHOIS_TEXT       "%s :End of /WHOIS list"

#define RPL_WHOISCHANNELS         319
#define RPL_WHOISCHANNELS_TEXT    "%s :%s"

#define RPL_WHOWASUSER            314
#define RPL_WHOWASUSER_TEXT       "%s %s %s * :%s"

#define RPL_ENDOFWHOWAS           369
#define RPL_ENDOFWHOWAS_TEXT      "%s :End of WHOWAS"

#define RPL_LISTSTART             321
#define RPL_LISTSTART_TEXT        "Channel :Users  Name"

#define RPL_LIST                  322
#define RPL_LIST_TEXT             "%s %d :%s"

#define RPL_LISTEND               323
#define RPL_LISTEND_TEXT          ":End of /LIST"

#define RPL_CHANNELMODEIS         324

#define RPL_CHANCREATED           329

#define RPL_NOTOPIC               331
#define RPL_NOTOPIC_TEXT          "%s :No topic is set"

#define RPL_TOPIC                 332
#define RPL_TOPICSET              333

#define RPL_INVITING              341
#define RPL_SUMMONING             342

#define RPL_VERSION               351
#define RPL_VERSION_TEXT          "%s %s :place a version comment here"

#define RPL_WHOREPLY              352
#define RPL_ENDOFWHO              315
#define RPL_NAMREPLY              353

#define RPL_ENDOFNAMES            366
#define RPL_ENDOFNAMES_TEXT       "%s :End of /NAMES list"

#define RPL_LINKS                 364
#define RPL_ENDOFLINKS            365
#define RPL_BANLIST               367

#define RPL_ENDOFBANLIST          368
#define RPL_ENDOFBANLIST_TEXT     "%s :End of channel ban list"

#define RPL_INFO                  371
#define RPL_ENDOFINFO             374

#define RPL_MOTDSTART             375
#define RPL_MOTDSTART_TEXT        ":- Message of the day - %s -"

#define RPL_MOTD                  372
#define RPL_MOTD_TEXT             ":- %s"

#define RPL_ENDOFMOTD             376
#define RPL_ENDOFMOTD_TEXT        ":End of /MOTD command"

#define RPL_YOUREOPER             381
#define RPL_REHASHING             382

#define RPL_TIME                  391
#define RPL_TIME_TEXT             "%s :%s"

#define RPL_USERSSTART            392
#define RPL_USERS                 393
#define RPL_ENDOFUSERS            394
#define RPL_NOUSERS               395

#define RPL_TRACELINK             200
#define RPL_TRACECONNECTING       201
#define RPL_TRACEHANDSHAKE        202
#define RPL_TRACEUNKNOWN          203
#define RPL_TRACEOPERATOR         204
#define RPL_TRACEUSER             205
#define RPL_TRACESERVER           206
#define RPL_TRACENEWTYPE          208
#define RPL_TRACELOG              261

#define RPL_STATSLINKINFO         211
#define RPL_STATSCOMMANDS         212
#define RPL_STATSCLINE            213
#define RPL_STATSNLINE            214
#define RPL_STATSILINE            215
#define RPL_STATSKLINE            216
#define RPL_STATSYLINE            218

#define RPL_ENDOFSTATS            219
#define RPL_ENDOFSTATS_TEXT       "%c :End of /STATS report"

#define RPL_STATSLLINE            241

#define RPL_STATSUPTIME           242
#define RPL_STATSUPTIME_TEXT      ":Server Up %d days %d:%02d:%02d"

#define RPL_STATSOLINE            243
#define RPL_STATSHLINE            244

#define RPL_UMODEIS               221

#define RPL_LUSERCLIENT           251
#define RPL_LUSERCLIENT_TEXT      ":There are %d users and %d invisible " \
                                  "on %d servers"

#define RPL_LUSEROP               252
#define RPL_LUSEROP_TEXT          "%d :operators online"

#define RPL_LUSERUNKNOWN          253
#define RPL_LUSERUNKNOWN_TEXT     "%d :unknown connections"

#define RPL_LUSERCHANNELS         254
#define RPL_LUSERCHANNELS_TEXT    "%d :channels formed"

#define RPL_LUSERME               255
#define RPL_LUSERME_TEXT          ":I have %d clients and %d servers"

#define RPL_ADMINME               256
#define RPL_ADMINME_TEXT          "%s :%s"

#define RPL_ADMINLOC1             257
#define RPL_ADMINLOC1_TEXT        ":%s"

#define RPL_ADMINLOC2             258
#define RPL_ADMINLOC2_TEXT        ":%s"

#define RPL_ADMINEMAIL            259
#define RPL_ADMINEMAIL_TEXT       ":%s"

#endif /* __IRC_PROTO_H__ */
