/*
 * src/interfaces.c - network interface function implementation
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
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
 * $Id: interface.c,v 1.5 2000/07/04 19:33:55 raimi Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_IFLIST

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
# include <unistd.h>

# include <netinet/in.h>
# include <arpa/inet.h>
#endif

/* Solaris, IRIX */
#if HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
#endif

#include "alloc.h"
#include "util.h"
#include "interface.h"

/*
 * Print a list of all local interfaces if we are able to do so.
 */
#define NET_LIST_STRING \
  "--- list of local interfaces you " \
  "can start ip services on ---\n"

#ifdef _WIN32

/*
 * The local interface list is requested by some "unrevealed"
 * WinSocket-Routine called "WsControl".
 * Works with: Win95, Win98
 * Or we try using the IP Helper API. Works with WinNT4x / Win2k.
 */

WsControlProc WsControl = NULL;
GetIfTableProc GetIfTable = NULL;
GetIpAddrTableProc GetIpAddrTable = NULL;

#define NO_METHOD    0
#define WSCTL_METHOD 1
#define IPAPI_METHOD 2

void
list_local_interfaces (void)
{
  int result = 0;
  HMODULE WSockHandle;
  WSADATA WSAData;
  TCP_REQUEST_QUERY_INFORMATION_EX tcpRequestQueryInfoEx;
  DWORD tcpRequestBufSize;
  DWORD entityIdsBufSize;
  TDIEntityID *entityIds;
  DWORD entityCount;
  DWORD i, n, k;
  DWORD ifCount;
  ULONG entityType;
  DWORD entityTypeSize;
  DWORD ifEntrySize;
  IFEntry *ifEntry;
  DWORD ipAddrEntryBufSize;
  IPAddrEntry *ipAddrEntry;
  ULONG ifTableSize, ipTableSize;
  PMIB_IFTABLE ifTable;
  PMIB_IPADDRTABLE ipTable;
  unsigned char *addr;

  DWORD ifEntries = 0;
  ifList_t *ifList = NULL;
  DWORD Method = NO_METHOD;

  /*
   * Try getting WsControl () from "wsock32.dll" via LoadLibrary
   * and GetProcAddress. Or try the IP Helper API.
   */
  if ((WSockHandle = LoadLibrary ("iphlpapi.dll")) != NULL)
    {
      Method = IPAPI_METHOD;
    }
  else
    {
      if ((WSockHandle = LoadLibrary ("wsock32.dll")) != NULL)
	{
	  WsControl = (WsControlProc) 
	    GetProcAddress (WSockHandle, "WsControl");
	  if (!WsControl)
	    {
	      fprintf (stderr, "GetProcAddress (WsControl): %s\n", SYS_ERROR);
	      FreeLibrary (WSockHandle);
	      return;
	    }
	  Method = WSCTL_METHOD;
	}
      else
	{
	  fprintf (stderr, "LoadLibrary (WSock32.dll): %s\n", SYS_ERROR);
	  return;
	}
    }

  if (Method == WSCTL_METHOD)
    {
      result = WSAStartup (MAKEWORD (1, 1), &WSAData);
      if (result) 
	{
	  fprintf (stderr, "WSAStartup: %s\n", NET_ERROR);
	  FreeLibrary (WSockHandle);
	  return;
	}

      memset (&tcpRequestQueryInfoEx, 0, sizeof (tcpRequestQueryInfoEx));
      tcpRequestQueryInfoEx.ID.toi_entity.tei_entity = GENERIC_ENTITY;
      tcpRequestQueryInfoEx.ID.toi_entity.tei_instance = 0;
      tcpRequestQueryInfoEx.ID.toi_class = INFO_CLASS_GENERIC;
      tcpRequestQueryInfoEx.ID.toi_type = INFO_TYPE_PROVIDER;
      tcpRequestQueryInfoEx.ID.toi_id = ENTITY_LIST_ID;
      tcpRequestBufSize = sizeof (tcpRequestQueryInfoEx);

      /*
       * this probably allocates too much space; not sure if MAX_TDI_ENTITIES
       * represents the max number of entities that can be returned or, if it
       * is the highest entity value that can be defined.
       */
      entityIdsBufSize = MAX_TDI_ENTITIES * sizeof (TDIEntityID);
      entityIds = (TDIEntityID *) calloc (entityIdsBufSize, 1);
      
      result = WsControl (IPPROTO_TCP,
			  WSCTL_TCP_QUERY_INFORMATION,
			  &tcpRequestQueryInfoEx,
			  &tcpRequestBufSize, entityIds, &entityIdsBufSize);
      
      if (result) 
	{
	  fprintf (stderr, "WsControl: %s\n", NET_ERROR);
	  WSACleanup ();
	  FreeLibrary (WSockHandle);
	  return;
	}

      /*...after the call we compute: */
      entityCount = entityIdsBufSize / sizeof (TDIEntityID);
      ifCount = 0;

      /* find out the interface info for the generic interfaces */
      for (i = 0; i < entityCount; i++) 
	{
	  if (entityIds[i].tei_entity == IF_ENTITY) 
	    {
	      ++ifCount;

	      /* see if the interface supports snmp mib-2 info */
	      memset (&tcpRequestQueryInfoEx, 0,
		      sizeof (tcpRequestQueryInfoEx));
	      tcpRequestQueryInfoEx.ID.toi_entity = entityIds[i];
	      tcpRequestQueryInfoEx.ID.toi_class = INFO_CLASS_GENERIC;
	      tcpRequestQueryInfoEx.ID.toi_type = INFO_TYPE_PROVIDER;
	      tcpRequestQueryInfoEx.ID.toi_id = ENTITY_TYPE_ID;

	      entityTypeSize = sizeof (entityType);
	      
	      result = WsControl (IPPROTO_TCP,
				  WSCTL_TCP_QUERY_INFORMATION,
				  &tcpRequestQueryInfoEx,
				  &tcpRequestBufSize,
				  &entityType, &entityTypeSize);
	      
	      if (result) 
		{
		  fprintf (stderr, "WsControl: %s\n", NET_ERROR);
		  WSACleanup ();
		  FreeLibrary (WSockHandle);
		  return;
		}

	      if (entityType == IF_MIB) 
		{ 
		  /* Supports MIB-2 interface. Get snmp mib-2 info. */
		  tcpRequestQueryInfoEx.ID.toi_class = INFO_CLASS_PROTOCOL;
		  tcpRequestQueryInfoEx.ID.toi_id = IF_MIB_STATS_ID;

		  /*
		   * note: win95 winipcfg use 130 for MAX_IFDESCR_LEN while
		   * ddk\src\network\wshsmple\SMPLETCP.H defines it as 256
		   * we are trying to dup the winipcfg parameters for now
		   */
		  ifEntrySize = sizeof (IFEntry) + 128 + 1;
		  ifEntry = (IFEntry *) calloc (ifEntrySize, 1);
		  
		  result = WsControl (IPPROTO_TCP,
				      WSCTL_TCP_QUERY_INFORMATION,
				      &tcpRequestQueryInfoEx,
				      &tcpRequestBufSize,
				      ifEntry, &ifEntrySize);

		  if (result) 
		    {
		      fprintf (stderr, "WsControl: %s\n", NET_ERROR);
		      WSACleanup ();
		      FreeLibrary (WSockHandle);
		      return;
		    }

		  /* store interface index and description */
		  *(ifEntry->if_descr + ifEntry->if_descrlen) = 0;
		  ifEntries++;
		  if (ifList == NULL)
		    ifList = (ifList_t *) 
		      malloc (ifEntries * sizeof (ifList_t));
		  else
		    ifList = (ifList_t *) 
		      realloc (ifList, ifEntries * sizeof (ifList_t));
		  ifList[ifEntries-1].index = ifEntry->if_index;
		  ifList[ifEntries-1].description = 
		    (char *) malloc (ifEntry->if_descrlen + 1);
		  memcpy (ifList[ifEntries-1].description, 
			  ifEntry->if_descr, ifEntry->if_descrlen + 1);
		}
	    }
	}
  
      /* find the ip interfaces */
      for (i = 0; i < entityCount; i++) 
	{
	  if (entityIds[i].tei_entity == CL_NL_ENTITY) 
	    {
	      /* get ip interface info */
	      memset (&tcpRequestQueryInfoEx, 0,
		      sizeof (tcpRequestQueryInfoEx));
	      tcpRequestQueryInfoEx.ID.toi_entity = entityIds[i];
	      tcpRequestQueryInfoEx.ID.toi_class = INFO_CLASS_GENERIC;
	      tcpRequestQueryInfoEx.ID.toi_type = INFO_TYPE_PROVIDER;
	      tcpRequestQueryInfoEx.ID.toi_id = ENTITY_TYPE_ID;

	      entityTypeSize = sizeof (entityType);

	      result = WsControl (IPPROTO_TCP,
				  WSCTL_TCP_QUERY_INFORMATION,
				  &tcpRequestQueryInfoEx,
				  &tcpRequestBufSize,
				  &entityType, &entityTypeSize);

	      if (result) 
		{
		  fprintf (stderr, "WsControl: %s\n", NET_ERROR);
		  WSACleanup ();
		  FreeLibrary (WSockHandle);
		  return;
		}

	      if (entityType == CL_NL_IP) 
		{
		  /* Entity implements IP. Get ip address list. */
		  tcpRequestQueryInfoEx.ID.toi_class = INFO_CLASS_PROTOCOL;
		  tcpRequestQueryInfoEx.ID.toi_id = IP_MIB_ADDRTABLE_ENTRY_ID;

		  ipAddrEntryBufSize = sizeof (IPAddrEntry) * ifCount;
		  ipAddrEntry = (IPAddrEntry*) calloc (ipAddrEntryBufSize, 1);

		  result = WsControl (IPPROTO_TCP,
				      WSCTL_TCP_QUERY_INFORMATION,
				      &tcpRequestQueryInfoEx,
				      &tcpRequestBufSize,
				      ipAddrEntry, &ipAddrEntryBufSize);

		  if (result) 
		    {
		      fprintf (stderr, "WsControl: %s\n", NET_ERROR);
		      WSACleanup ();
		      FreeLibrary (WSockHandle);
		      return;
		    }
		
		  printf (NET_LIST_STRING);

		  /* print ip address list and interface description */
		  for (n = 0; n < ifCount; n++) 
		    {
		      addr = (unsigned char *) &ipAddrEntry[n].iae_addr;

		      for (k = 0; k < ifEntries; k++)
			if (ifList[k].index == ipAddrEntry[n].iae_index)
			  break;

		      if (k != ifEntries)
			{
			  /* interface with description */
			  fprintf (stdout, "%40s: %u.%u.%u.%u\n",
				   ifList[k].description,
				   addr[0], addr[1], addr[2], addr[3]);
			}
		      else
			{
			  /* interface with interface # only */
			  fprintf (stdout,
				   "%31s%09lu: %u.%u.%u.%u\n",
				   "interface # ",
				   ipAddrEntry[n].iae_index,
				   addr[0], addr[1], addr[2], addr[3]);
			}
		    }
		}
	    }
	}

      WSACleanup ();
      FreeLibrary (WSockHandle);
    }

  /* this is for WinNT... */
  else if (Method == IPAPI_METHOD)
    {
      /* Use of the IPHelper-API here. */
      GetIfTable = (GetIfTableProc) 
	GetProcAddress (WSockHandle, "GetIfTable");
      if (!GetIfTable)
	{
	  fprintf (stderr, "GetProcAddress (GetIfTable): %s\n", SYS_ERROR);
	  FreeLibrary (WSockHandle);
	  return;
	}

      GetIpAddrTable = (GetIpAddrTableProc) 
	GetProcAddress (WSockHandle, "GetIpAddrTable");
      if (!GetIpAddrTable)
	{
	  fprintf (stderr, "GetProcAddress (GetIpAddrTable): %s\n", SYS_ERROR);
	  FreeLibrary (WSockHandle);
	  return;
	}

      ifTableSize = sizeof (MIB_IFTABLE);
      ifTable = (PMIB_IFTABLE) malloc (ifTableSize);
      GetIfTable (ifTable, &ifTableSize, FALSE);
      ifTable = (PMIB_IFTABLE) realloc (ifTable, ifTableSize);
      if (GetIfTable (ifTable, &ifTableSize, FALSE) != NO_ERROR)
	{
	  fprintf (stderr, "GetIfTable: %s\n", SYS_ERROR);
	  FreeLibrary (WSockHandle);
	  return;
	}
  
      ipTableSize = sizeof (MIB_IPADDRTABLE);
      ipTable = (PMIB_IPADDRTABLE) malloc (ipTableSize);
      GetIpAddrTable (ipTable, &ipTableSize, FALSE);
      ipTable = (PMIB_IPADDRTABLE) realloc (ipTable, ipTableSize);
      if (GetIpAddrTable (ipTable, &ipTableSize, FALSE) != NO_ERROR)
	{
	  fprintf (stderr, "GetIpAddrTable: %s\n", SYS_ERROR);
	  FreeLibrary (WSockHandle);
	  return;
	}
      
      printf (NET_LIST_STRING);

      for (n = 0; n < ipTable->dwNumEntries; n++)
	{
	  for (i = 0; i < ifTable->dwNumEntries; i++)
	    {
	      if (ifTable->table[i].dwIndex == ipTable->table[n].dwIndex)
		{
		  ifTable->table[i].bDescr[ifTable->table[i].dwDescrLen] = 0;
		  
		  fprintf (stdout, "%40s: %u.%u.%u.%u\n", 
			  ifTable->table[i].bDescr,
			  ipTable->table[n].dwAddr & 0xFF,
			  (ipTable->table[n].dwAddr >> 8) & 0xFF,
			  (ipTable->table[n].dwAddr >> 16) & 0xFF,
			  (ipTable->table[n].dwAddr >> 24) & 0xFF);
		  break;
		}
	    }
	  if (i == ipTable->dwNumEntries)
	    {
	      fprintf (stdout, "%31s%09u: %u.%u.%u.%u\n", 
		      "interface # ",
		      ipTable->table[n].dwIndex,
		      ipTable->table[n].dwAddr & 0xFF,
		      (ipTable->table[n].dwAddr >> 8) & 0xFF,
		      (ipTable->table[n].dwAddr >> 16) & 0xFF,
		      (ipTable->table[n].dwAddr >> 24) & 0xFF);
	    }
	}

      FreeLibrary (WSockHandle);
    }
  else
    {
      fprintf (stderr, 
	       "Neither IPHlpApi.dll nor WSock32.WsControl found...\n");
    }
}

#else /* not _WIN32 */

void
list_local_interfaces (void)
{
  int numreqs = 16;
  struct ifconf ifc;
  struct ifreq *ifr;
  struct ifreq ifr2;
  int n;
  int fd;

  /* Get a socket out of the Internet Address Family. */
  if ((fd = socket (AF_INET, SOCK_STREAM,0)) < 0) 
    {
      perror ("socket");
      return;
    }

  /* Collect information. */
  ifc.ifc_buf = NULL;
  for (;;) 
    {
      ifc.ifc_len = sizeof (struct ifreq) * numreqs;
      ifc.ifc_buf = xrealloc (ifc.ifc_buf, ifc.ifc_len);


      /**
       * On newer AIXes we cannot use SIOCGICONF anymore, although it is
       * present. The data structure returned is bogus. Using OSIOCGIFCONF.
       */
#if defined (OSIOCGIFCONF)
      if (ioctl (fd, OSIOCGIFCONF, &ifc) < 0)
	{
	  perror ("OSIOCGIFCONF");
	  close (fd);
	  free (ifc.ifc_buf);
	  return;	  
	}
#else /* OSIOCGIFCONF */
      if (ioctl (fd, SIOCGIFCONF, &ifc) < 0) 
	{
	  perror ("SIOCGIFCONF");
	  close (fd);
	  free (ifc.ifc_buf);
	  return;
	}
#endif /* OSIOCGIFCONF */

      if ((unsigned) ifc.ifc_len == sizeof (struct ifreq) * numreqs) 
	{
	  /* Assume it overflowed and try again. */
	  numreqs += 10;
	  continue;
	}
      break;
    }

  printf (NET_LIST_STRING);

  ifr = ifc.ifc_req;
  for (n = 0; n < ifc.ifc_len; n += sizeof (struct ifreq), ifr++)
    {

      /*
       * On AIX (and perhaps others) you get interfaces that are not AF_INET
       * from the first ioctl, so filter here again
       */
      if (ifr->ifr_addr.sa_family != AF_INET)
	continue;

      strcpy (ifr2.ifr_name, ifr->ifr_name);
      ifr2.ifr_addr.sa_family = AF_INET;
      if (ioctl (fd, SIOCGIFADDR, &ifr2) == 0)
	{
	  /* The following cast looks bogus. ifr2.ifr_addr is a
	   * (struct sockaddr), but we know that we deal with a 
	   * (struct sockaddr_in) here. since you cannot cast structures
	   * in C, I cast addresses just to get a (struct sockaddr_in) in the
	   * end... phew
	   */
	  printf ("%8s: %s\n", ifr->ifr_name,
		  inet_ntoa ( (*((struct sockaddr_in*)
				 &ifr2.ifr_addr)).sin_addr) );

	}
      else 
	{
	  perror ("SIOCGIFADDR");
	  break;
	}
    }
  
  printf ("\n");
  close (fd);
  xfree (ifc.ifc_buf);
}

#endif /* not __MINGW32__ */

#else /* not ENABLE_IFLIST */

void
list_local_interfaces (void)
{
  printf ("\n"
	  "Sorry, the list of local interfaces is not available. If you\n"
	  "know how to get such a list on your OS, please contact\n"
	  "Raimund Jacob <raimi@lkcc.org>. Thanks.\n\n");
}

#endif /* not ENABLE_IFLIST */
