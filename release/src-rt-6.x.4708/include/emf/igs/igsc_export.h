/*
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: igsc_export.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _IGSC_EXPORT_H_
#define _IGSC_EXPORT_H_

#ifdef IGSDBG

#define	IGS_LOG_DEBUG                   (1 << 0)
#define	IGS_LOG_ERROR                   (1 << 1)
#define	IGS_LOG_WARN                    (1 << 2)
#define	IGS_LOG_INFO                    (1 << 3)
#define	IGS_LOG_IGSDB                   (1 << 4)

#define	IGS_LOG_LVL                     (IGS_LOG_ERROR | \
					 IGS_LOG_WARN  | \
					 IGS_LOG_DEBUG | \
					 IGS_LOG_INFO  | \
					 IGS_LOG_IGSDB)

#if (IGS_LOG_LVL & IGS_LOG_DEBUG)
#define IGS_DEBUG(fmt, args...)         printf("IGS_DEBUG: " fmt, ##args)
#else /* IGS_LOG_LVL & IGS_LOG_DEBUG */
#define IGS_DEBUG(fmt, args...)
#endif /* IGS_LOG_LVL & IGS_LOG_DEBUG */

#if (IGS_LOG_LVL & IGS_LOG_ERROR)
#define IGS_ERROR(fmt, args...)         printf("IGS_ERROR: " fmt, ##args)
#else /* IGS_LOG_LVL & IGS_LOG_ERROR */
#define IGS_ERROR(fmt, args...)
#endif /* IGS_LOG_LVL & IGS_LOG_ERROR */

#if (IGS_LOG_LVL & IGS_LOG_WARN)
#define IGS_WARN(fmt, args...)          printf("IGS_WARN.: " fmt, ##args)
#else /* IGS_LOG_LVL & IGS_LOG_WARN */
#define IGS_WARN(fmt, args...)
#endif /* IGS_LOG_LVL & IGS_LOG_WARN */

#if (IGS_LOG_LVL & IGS_LOG_INFO)
#define IGS_INFO(fmt, args...)          printf("IGS_INFO.: " fmt, ##args)
#else /* IGS_LOG_LVL & IGS_LOG_INFO */
#define IGS_INFO(fmt, args...)
#endif /* IGS_LOG_LVL & IGS_LOG_INFO */

#if (IGS_LOG_LVL & IGS_LOG_IGSDB)
#define IGS_IGSDB(fmt, args...)         printf("IGS_IGSDB: " fmt, ##args)
#else /* IGS_LOG_LVL & IGS_LOG_IGSDB */
#define IGS_IGSDB(fmt, args...)
#endif /* IGS_LOG_LVL & IGS_LOG_IGSDB */

#else /* IGSDBG */

#define IGS_DEBUG(fmt, args...)
#define IGS_ERROR(fmt, args...)         printf(fmt, ##args)
#define IGS_WARN(fmt, args...)
#define IGS_INFO(fmt, args...)
#define IGS_IGSDB(fmt, args...)

#endif /* IGSDBG */

typedef int32 (*igs_broadcast_fn_ptr)(void *wrapper, uint8 *ip, uint32 length, uint32 mgrp_ip);

/*
 * Wrapper specific functions 
 */
typedef struct igsc_wrapper
{

	/* Function called to broadcast IGMP query*/
	int32 (*igs_broadcast)(void *wrapper, uint8 *ip, uint32 length, uint32 mgrp_ip);

}igsc_wrapper_t;

struct igsc_info;

/*
 * Description: This function is called from the OS specific module init
 *              routine to initialize the IGSL. This function primarily
 *              initializes the IGMP Snooping Layer global (IGSL) data and
 *              IGSDB. It also associates the snooper instance with EMF
 *              instance.
 *
 * Input:       inst_id    - IGS instance identifier.
 *              igs_info   - IGSL OS Specific global data handle
 *              osh        - OS abstraction layer handle
 *              emf_handle - Handle to use when interfacing with EMF.
 *              wrapper    - Wrapper spcific info.
 *
 * Return:      igsc_info - IGSL Common code global data handle
 */
extern void * igsc_init(int8 *inst_id, void *igs_info, osl_t *osh, igsc_wrapper_t *wrapper);

/*
 * Description: This function is called from OS specific module exit routine
 *              This routine primarily stops the group interval timers,
 *              deletes and frees up the IGSDB entries.
 *
 * Input:       igs_info - IGSL OS Specific global data handle
 */
extern void igsc_exit(struct igsc_info *igsc_info);

/*
 * Description: This function returns various packet counters and global
 *              stats of IGSL. It is called from the IGSL OS specific
 *              command handler.
 *
 * Input:       igsc_info - IGSL Common code global instance handle
 *              size      - Size of the input buffer.
 *
 * Output:      stats - Pointer to structure of type igs_stats_t
 *
 * Return:      SUCCESS or FAILURE.
 */
extern int32 igsc_stats_get(struct igsc_info *igsc_info, igs_stats_t *stats, uint32 size);

/*
 * Description: This function clears the group interval timers and
 *              deletes the group, host and interface entries of the
 *              IGSDB.
 *
 * Input:       igs_info - IGSL OS Specific global data handle
 */
extern void igsc_sdb_clear(struct igsc_info *igsc_info);

/*
 * Description: This function is called to obtain the IGSDB entry list.
 */
extern int32 igsc_sdb_list(struct igsc_info *igsc_info, igs_cfg_sdb_list_t *list, uint32 size);

/*
 * Description: This function is called from the OS Specific layer when
 *              user issues a configuration command.
 *
 * Input:        igs_info - IGSC global instance handle
 *
 * Input/Output: cfg  - Pointer to configuration request data. It
 *                      contains the command id, operation type,
 *                      corresponding arguments and output status.
 */
extern void igsc_cfg_request_process(struct igsc_info *igsc_info, igs_cfg_request_t *cfg);

/*
 * Description: This function is called to delete igsc sdb entries
 *		specific to an interface
 *
 * Input:        igs_info - IGSC global instance handle
 *		 ifp      - Interface pointer
 */
extern int32 igsc_sdb_interface_del(struct igsc_info *igsc_info, void *ifp);

/*
 * Description: This function is called to delete igsc sdb rtport entries
 *		specific to an interface
 *
 * Input:        igs_info - IGSC global instance handle
 *		 ifp      - Interface pointer
 */
extern int32 igsc_interface_rtport_del(struct igsc_info *igsc_info, void *ifp);

#endif /* _IGSC_EXPORT_H_ */
