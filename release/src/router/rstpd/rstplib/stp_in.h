/************************************************************************ 
 * RSTP library - Rapid Spanning Tree (802.1t, 802.1w) 
 * Copyright (C) 2001-2003 Optical Access 
 * Author: Alex Rozin 
 * 
 * This file is part of RSTP library. 
 * 
 * RSTP library is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU Lesser General Public License as published by the 
 * Free Software Foundation; version 2.1 
 * 
 * RSTP library is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser 
 * General Public License for more details. 
 * 
 * You should have received a copy of the GNU Lesser General Public License 
 * along with RSTP library; see the file COPYING.  If not, write to the Free 
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
 * 02111-1307, USA. 
 **********************************************************************/

 /* This file contains prototypes for API from an operation
    system to the RSTP */
    
#ifndef _STP_API_H__
#define _STP_API_H__

/************************
 * Common base constants
 ************************/

#ifndef INOUT
#  define IN      /* consider as comments near 'input' parameters */
#  define OUT     /* consider as comments near 'output' parameters */
#  define INOUT   /* consider as comments near 'input/output' parameters */
#endif

#ifndef Zero
#  define Zero        0
#  define One         1
#endif

#ifndef Bool
#  define Bool        int
#  define False       0
#  define True        1
#endif

/********************************************
 * constants: default values and linitations
 *********************************************/
 
/* bridge configuration */

#define DEF_BR_PRIO 32768
#define MIN_BR_PRIO 0
#define MAX_BR_PRIO 61440
#define MASK_BR_PRIO 0xf000

#define DEF_BR_HELLOT   2
#define MIN_BR_HELLOT   1
#define MAX_BR_HELLOT   10

#define DEF_BR_MAXAGE   20
#define MIN_BR_MAXAGE   6
#define MAX_BR_MAXAGE   40

#define DEF_BR_FWDELAY  15
#define MIN_BR_FWDELAY  4
#define MAX_BR_FWDELAY  30

#define DEF_FORCE_VERS  2 /* NORMAL_RSTP */

/* port configuration */

#define DEF_PORT_PRIO   128
#define MIN_PORT_PRIO   0
#define MAX_PORT_PRIO   240 /* in steps of 16 */
#define MASK_PORT_PRIO  0xf0

#define DEF_ADMIN_NON_STP   False
#define DEF_ADMIN_EDGE      True
#define DEF_LINK_DELAY      3 /* see edge.c */
#define DEF_P2P         P2P_AUTO

#define MAX_PORT_PCOST 200000000

/* Section 1: Create/Delete/Start/Stop the RSTP instance */

void /* init the engine */
STP_IN_init (int max_port_index);

#ifdef __BITMAP_H
int
STP_IN_stpm_create (int vlan_id, char* name, BITMAP_T* port_bmp);
#endif

int
STP_IN_stpm_delete (int vlan_id);

int
STP_IN_stop_all (void);

int
STP_IN_delete_all (void);

/* Port create/delete */

int STP_IN_port_create(int vlan_id, int port_index);

int STP_IN_port_delete(int vlan_id, int port_index);

/* Section 2. "Get" management */

Bool
STP_IN_get_is_stpm_enabled (int vlan_id);

int
STP_IN_stpm_get_vlan_id_by_name (char* name, int* vlan_id);

int
STP_IN_stpm_get_name_by_vlan_id (int vlan_id, char* name, size_t buffsize);

const char*
STP_IN_get_error_explanation (int rstp_err_no);

#ifdef _UID_STP_H__
int
STP_IN_stpm_get_cfg (int vlan_id, UID_STP_CFG_T* uid_cfg);

int
STP_IN_stpm_get_state (int vlan_id, UID_STP_STATE_T* entry);

int
STP_IN_port_get_cfg (int vlan_id, int port_index, UID_STP_PORT_CFG_T* uid_cfg);

int
STP_IN_port_get_state (int vlan_id, UID_STP_PORT_STATE_T* entry);
#endif

/* Section 3. "Set" management */

int
STP_IN_stpm_set_cfg (int vlan_id,
                     BITMAP_T* port_bmp,
                     UID_STP_CFG_T* uid_cfg);

#ifdef ORIG
int
STP_IN_set_port_cfg (int vlan_id,
                     UID_STP_PORT_CFG_T* uid_cfg);
#else
int
STP_IN_set_port_cfg (int vlan_id, int port_index,
                     UID_STP_PORT_CFG_T* uid_cfg);
#endif

#ifdef STP_DBG
int STP_IN_dbg_set_port_trace (char* mach_name, int enadis,
                               int vlan_id, BITMAP_T* ports,
                               int is_print_err);
#endif

/* Section 4. RSTP functionality events */

int 
STP_IN_one_second (void);

int /* for Link UP/DOWN */
STP_IN_enable_port (int port_index, Bool enable);

int /* call it, when port speed has been changed, speed in Kb/s  */
STP_IN_changed_port_speed (int port_index, long speed);

int /* call it, when current port duplex mode has been changed  */
STP_IN_changed_port_duplex (int port_index);

#ifdef _STP_BPDU_H__
int
STP_IN_check_bpdu_header (BPDU_T* bpdu, size_t len);

int
STP_IN_rx_bpdu (int vlan_id, int port_index, BPDU_T* bpdu, size_t len);
#endif

/*--- For multiple STP instances - non multithread use ---*/

struct stp_instance;
/* Create struct to hold STP instance state and initialize it.
   A copy of all global state in the library. */
struct stp_instance *STP_IN_instance_create(void);
/* Set context from this STP instance */
void STP_IN_instance_begin(struct stp_instance *p);
/* Save context back to this STP instance */
void STP_IN_instance_end(struct stp_instance *p);
/* Delete this STP instance */
void STP_IN_instance_delete(struct stp_instance *p);

#ifdef _STP_MACHINE_H__
/* Inner usage definitions & functions */

extern int max_port;

#ifdef __LINUX__
#  define RSTP_INIT_CRITICAL_PATH_PROTECTIO
#  define RSTP_CRITICAL_PATH_START
#  define RSTP_CRITICAL_PATH_END
#else
#  define RSTP_INIT_CRITICAL_PATH_PROTECTIO STP_OUT_psos_init_semaphore ()
#  define RSTP_CRITICAL_PATH_START          STP_OUT_psos_close_semaphore ()
#  define RSTP_CRITICAL_PATH_END            STP_OUT_psos_open_semaphore ()
   extern void STP_OUT_psos_init_semaphore (void);
   extern void STP_OUT_psos_close_semaphore (void);
   extern void STP_OUT_psos_open_semaphore (void);
#endif

STPM_T* stpapi_stpm_find (int vlan_id);

int stp_in_stpm_enable (int vlan_id, char* name,
                    BITMAP_T* port_bmp,
                    UID_STP_MODE_T admin_state);
void* stp_in_stpm_create (int vlan_id, char* name, BITMAP_T* port_bmp,
                          int* err_code);

#endif /* _STP_MACHINE_H__ */


#endif /* _STP_API_H__ */
