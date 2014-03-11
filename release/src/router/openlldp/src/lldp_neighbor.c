/*******************************************************************
 *
 * OpenLLDP Neighbor 
 *
 * See LICENSE file for more info.
 * 
 * File: lldp_neighbor.c
 * 
 * Authors: Jason Peterson (condurre@users.sourceforge.net)
 *
 *******************************************************************/

#ifndef WIN32
#include <stdint.h>
#include <sys/utsname.h>
#include <unistd.h>
#else  // WIN32
#include "stdintwin.h"
#endif // WIN32
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <string.h>

#include "lldp_port.h"
#include "lldp_debug.h"
#include "lldp_neighbor.h"
#include "tlv/tlv_common.h"
#include "tlv/tlv.h"

int get_sys_desc() {
#ifndef WIN32
    int retval;

    struct utsname sysinfo;

    bzero(&lldp_systemdesc[0], 512);

    retval = uname(&sysinfo);

    if (retval < 0) { 

        debug_printf(DEBUG_NORMAL, "Call to uname failed!\n");
        exit(0);    

    }

    debug_printf(DEBUG_NORMAL, "sysinfo.machine: %s\n", sysinfo.machine);  
    debug_printf(DEBUG_NORMAL, "sysinfo.sysname: %s\n", sysinfo.sysname);
    debug_printf(DEBUG_NORMAL, "sysinfo.release: %s\n", sysinfo.release);

    strcpy(lldp_systemdesc, sysinfo.machine);
    strcat(lldp_systemdesc, "/");
    strcat(lldp_systemdesc, sysinfo.sysname);  
    strcat(lldp_systemdesc, " ");
    strcat(lldp_systemdesc, sysinfo.release);

    debug_printf(DEBUG_NORMAL, "lldp_systemdesc: %s\n", lldp_systemdesc);

    return(0);
#endif // WIN32
	return -1;
}

int get_sys_fqdn() {
#ifndef WIN32
    int retval;

    bzero(&lldp_systemname[0], 512);

    retval = gethostname(lldp_systemname, 255);

    if (retval < 0) 
    {
        debug_printf(DEBUG_NORMAL, "gethostname() failed! retval = %d.\n", retval);
    }

    strcat(lldp_systemname, ".");

    retval = getdomainname(&lldp_systemname[strlen(lldp_systemname)], 255 - strlen(lldp_systemname));

    if (retval < 0)
    {
        debug_printf(DEBUG_NORMAL, "getdomainname() failed! retval = %d.\n", retval);
    }

    debug_printf(DEBUG_NORMAL, "lldp_systemname: %s\n", lldp_systemname);

    return(0);
#endif // WIN32
}

char *lldp_neighbor_information(struct lldp_port *lldp_ports) {
  struct lldp_port *lldp_port      = lldp_ports;
  struct lldp_msap *msap_cache     = NULL;
  struct lldp_tlv_list *tlv_list   = NULL;
  int neighbor_count               = 0;
  char *result = malloc(2048);
  char *buffer = malloc(2048);
  char *info_buffer = malloc(2048);
  char *tmp_buffer = malloc(2048);
  char *tlv_name = NULL;
  char *tlv_subtype = NULL;

  memset(result, 0x0, 2048);
  memset(buffer, 0x0, 2048);
  memset(info_buffer, 0x0, 2048);
  memset(tmp_buffer, 0x0, 2048);

  sprintf(result, "\nOpenLLDP Neighbor Info: \n\n");

  while(lldp_port != NULL) {
    neighbor_count = 0;
    memset(buffer, 0x0, 2048);
    memset(info_buffer, 0x0, 2048);
    memset(tmp_buffer, 0x0, 2048);
    sprintf(buffer, "Interface '%s' has ", lldp_port->if_name);

    strncat(result, buffer, 2048);

    msap_cache = lldp_port->msap_cache;

    while(msap_cache != NULL) {
      
      neighbor_count++;

      tlv_list = msap_cache->tlv_list;

      sprintf(tmp_buffer, "Neighbor %d:\n", neighbor_count);
      
      strncat(info_buffer, tmp_buffer, 2048);
      
      memset(tmp_buffer, 0x0, 2048);

      
      while(tlv_list != NULL) {
	  memset(tmp_buffer, 0x0, 2048);

	if(tlv_list->tlv != NULL) {

	  tlv_name = tlv_typetoname(tlv_list->tlv->type);

	  if(tlv_name != NULL) {
	    sprintf(tmp_buffer, "\t%s: ", tlv_name);

	    strncat(info_buffer, tmp_buffer, 2048);

	    //free(tlv_name);    
	    //tlv_name = NULL;

	    memset(tmp_buffer, 0x0, 2048);

	    tlv_subtype = decode_tlv_subtype(tlv_list->tlv);

	    if(tlv_subtype != NULL) {	    
	      sprintf(tmp_buffer, "\t%s\n", tlv_subtype);

	      strncat(info_buffer, tmp_buffer, 2048);

	      memset(tmp_buffer, 0x0, 2048);

	      free(tlv_subtype);
	      tlv_subtype = NULL;
	    }
	  } else {
	    sprintf(tmp_buffer, "\t\tUnknown TLV Type (%d)\n", tlv_list->tlv->type);
	    strncat(info_buffer, tmp_buffer, 2048);
	  }
       
	} else {
	  debug_printf(DEBUG_NORMAL, "Yikes... NULL TLV in MSAP cache!\n");
	}

	tlv_list = tlv_list->next;
      }

      strncat(info_buffer, "\n", 2048);
      
      msap_cache = msap_cache->next;
    }

    memset(buffer, 0x0, 2048);
    
    sprintf(buffer, "%d LLDP Neighbors: \n\n", neighbor_count);
    strncat(result, buffer, 2048);
    strncat(result, info_buffer, 2048);
 
    lldp_port = lldp_port->next;
  }


  free(tmp_buffer);
  free(info_buffer);
  free(buffer);

  return(result);
}


