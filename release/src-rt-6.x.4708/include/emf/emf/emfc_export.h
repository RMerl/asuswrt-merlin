/*
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: emfc_export.h 246051 2011-03-12 03:30:44Z $
 */

#ifndef _EMFC_EXPORT_H_
#define _EMFC_EXPORT_H_

#include <emf/emf/emf_cfg.h>

#define SUCCESS                     0
#define FAILURE                     -1

#define EMF_DROP                    0
#define EMF_FORWARD                 1
#define EMF_NOP                     1
#define EMF_TAKEN                   2
#define EMF_FLOOD                   10
#define EMF_SENDUP                  11
#define EMF_CONVERT_QUERY           12

#ifdef EMFDBG

#define        EMF_LOG_DEBUG        (1 << 0)
#define        EMF_LOG_ERROR        (1 << 1)
#define        EMF_LOG_WARN         (1 << 2)
#define        EMF_LOG_INFO         (1 << 3)
#define        EMF_LOG_MFDB         (1 << 4)

#define        EMF_LOG_LVL          (EMF_LOG_ERROR | \
	                             EMF_LOG_WARN  | \
	                             EMF_LOG_DEBUG | \
	                             EMF_LOG_INFO  | \
	                             EMF_LOG_MFDB)

#if (EMF_LOG_LVL & EMF_LOG_DEBUG)
#define EMF_DEBUG(fmt, args...)      printf("EMF_DEBUG: " fmt, ##args)
#else /* EMF_LOG_LVL & EMF_LOG_DEBUG */
#define EMF_DEBUG(fmt, args...)
#endif /* EMF_LOG_LVL & EMF_LOG_DEBUG */

#if (EMF_LOG_LVL & EMF_LOG_ERROR)
#define EMF_ERROR(fmt, args...)      printf("EMF_ERROR: " fmt, ##args)
#else /* EMF_LOG_LVL & EMF_LOG_ERROR */
#define EMF_ERROR(fmt, args...)
#endif /* EMF_LOG_LVL & EMF_LOG_ERROR */

#if (EMF_LOG_LVL & EMF_LOG_WARN)
#define EMF_WARN(fmt, args...)      printf("EMF_WARN.: " fmt, ##args)
#else /* EMF_LOG_LVL & EMF_LOG_WARN */
#define EMF_WARN(fmt, args...)
#endif /* EMF_LOG_LVL & EMF_LOG_WARN */

#if (EMF_LOG_LVL & EMF_LOG_INFO)
#define EMF_INFO(fmt, args...)      printf("EMF_INFO.: " fmt, ##args)
#else /* EMF_LOG_LVL & EMF_LOG_INFO */
#define EMF_INFO(fmt, args...)
#endif /* EMF_LOG_LVL & EMF_LOG_INFO */

#if (EMF_LOG_LVL & EMF_LOG_MFDB)
#define EMF_MFDB(fmt, args...)      printf("EMF_MFDB.: " fmt, ##args)
#else /* EMF_LOG_LVL & EMF_LOG_MFDB */
#define EMF_MFDB(fmt, args...)
#endif /* EMF_LOG_LVL & EMF_LOG_MFDB */

#else /* EMFDBG */

#define EMF_DEBUG(fmt, args...)
#define EMF_ERROR(fmt, args...)     printf(fmt, ##args)
#define EMF_WARN(fmt, args...)
#define EMF_INFO(fmt, args...)
#define EMF_MFDB(fmt, args...)

#endif /* EMFDBG */

/* Function pointer declarations */
typedef int32   (*forward_fn_ptr)(void *, void *, uint32, void *, bool);
typedef void (*sendup_fn_ptr)(void *, void *);
typedef int32   (*hooks_register_fn_ptr)(void *);
typedef int32   (*hooks_unregister_fn_ptr)(void *);

/*
 * Snooper
 */
typedef struct emfc_snooper
{
	/* Input function called when an IGMP packet is received by
	 * EMFL.
	 */
	int32   (*input_fn)(struct emfc_snooper *snooper, void *ifp,
	                    uint8 *iph, uint8 *igmph, bool rt_port);

	/* Function called when EMF is enabled */
	int32   (*emf_enable_fn)(struct emfc_snooper *snooper);

	/* Function called when EMF is disabled */
	int32   (*emf_disable_fn)(struct emfc_snooper *snooper);
} emfc_snooper_t;

/*
 * Wrapper specific functions
 */
typedef struct emfc_wrapper
{
	/* Function called to forward frames on a specific interface */
	int32   (*forward_fn)(void *wrapper, void *p, uint32 mgrp_ip,
	                    void *txif, bool rt_port);

	/* Function called to send packet up the stack */
	void (*sendup_fn)(void *wrapper, void *p);

	/* Function called to register hooks into the bridge */
	int32   (*hooks_register_fn)(void *wrapper);

	/* Function called to unregister hooks into the bridge */
	int32   (*hooks_unregister_fn)(void *wrapper);
} emfc_wrapper_t;

struct emfc_info;

/*
 * Description: This function is called from the OS specific module init
 *              routine to initialize the EMFL. This function primarily
 *              initializes the EMFL global data and MFDB.
 *
 * Input:       inst_id - Instance identier used to associate EMF and
 *                        IGMP snooper instances.
 *              emfi    - EMFL OS Specific global data handle
 *              osh     - OS abstraction layer handle
 *              wrapper - Wrapper specific info
 *
 * Return:      emfc    - EMFL Common code global instance handle
 */
extern struct emfc_info * emfc_init(int8 *inst_id, void *emfi, osl_t *osh, emfc_wrapper_t *wrapper);

/*
 * Description: This function is called from OS specific module exit
 *              routine. This routine primarily clears the MFDB and
 *              frees the EMF instance data.
 *
 * Input:       emfc - EMFL Common code global instance handle
 */
extern void emfc_exit(struct emfc_info *emfc);

/*
 * Description: This function is called by the OS specific EMFL code
 *              for every frame seen at the bridge pre and ip post hook
 *              points. It calls the registered snooper input function
 *              if the received frame type is an IGMP frame. For multicast
 *              data frames it handles the frame forwarding. The frame
 *              forwarding is done based on the information stored in
 *              the MFDB. MFDB is updated by IGMP snooper module.
 *
 * Input:       emfc    - EMFL Common code Global instance handle
 *              sdu     - Pointer to packet buffer.
 *              ifp     - Interface on which the packet arrived.
 *              iph     - Pointer to start of IP header. When IGMP
 *                        frame is input, it is assumed that the IP
 *                        header and IGMP header are contiguos.
 *              rt_port - TRUE when the packet is received from IP
 *                        Stack, FALSE otherwise.
 *
 * Return:      EMF_NOP   - No processing needed by EMF, just return
 *                          the packet back.
 *              EMF_TAKEN - EMF has taken the ownership of the packet.
 *              EMF_DROP  - Indicates the packet should be dropped.
 */
extern uint32 emfc_input(struct emfc_info *emfc, void *sdu, void *ifp,
                         uint8 *iph, bool rt_port);

/*
 * Description: This function is called to add MFDB entry. Each MFDB entry
 *              contains the multicast group address and list of interfaces
 *              that has participating group members. The caller of the
 *              function needs to make sure to delete the entry as many
 *              times as add is done.
 *
 * Input:       emfc    - EMFL Common code global instance handle
 *              mgrp_ip - Multicast group IP address of the entry.
 *              ifp     - Pointer to the interface on which the member
 *                        is present.
 *
 * Return:      SUCCESS or FAILURE
 */
extern int32 emfc_mfdb_membership_add(struct emfc_info *emfc, uint32 mgrp_ip, void *ifp);

/*
 * Description: This function is called by the IGMP snooper layer to delete
 *              the MFDB entry. It deletes the group entry also if the
 *              interface entry is last in the group.
 *
 * Input:       Same as above function.
 *
 * Return:      SUCCESS or FAILURE
 */
extern int32 emfc_mfdb_membership_del(struct emfc_info *emfc, uint32 mgrp_ip, void *ifp);

/*
 * Description: This function is called by the IGMP snooper layer to
 *              register snooper instance with EMFL.
 *
 * Input:       inst_id - Instance identier used to associate EMF
 *                        and IGMP snooper instances.
 *              emfc    - EMFL Common code global instance handle
 *              snooper - Contains snooper specific parameters and
 *                        event callback functions. These functions
 *                        are called by EMFL on events like IGMP
 *                        packet receive, EMF enable and disable.
 *                        The snooper parameter needs to global.
 *
 * Return:      SUCCESS or FAILURE
 */
extern int32 emfc_igmp_snooper_register(int8 *inst_id, struct emfc_info **emfc,
                                        emfc_snooper_t *snooper);

/*
 * Description: This function is called by the IGMP snooper layer to
 *              unregister snooper instance.
 *
 * Input:       handle  - Handle returned during registration.
 *              snooper - Same as above.
 */
extern void emfc_igmp_snooper_unregister(struct emfc_info *emfc);

/*
 * Description: This function is called from the OS Specific layer when
 *              user issues a configuration command.
 *
 * Input:        emfc - EMFL Common code global instance handle
 *
 * Input/Output: cfg  - Pointer to configuration request data. It
 *                      contains the command id, operation type,
 *                      corresponding arguments and output status.
 */
extern void emfc_cfg_request_process(struct emfc_info *emfc, emf_cfg_request_t *cfg);

/*
 * Description: This function is called from OS specific module init
 *              routine. It allocates global resources required by the
 *              common code.
 *
 * Return:      SUCCESS or FAILURE
 */
extern int32 emfc_module_init(void);

/*
 * Description: This function is called from OS specific module cleanup
 *              routine. It frees all the global resources.
 */
extern void emfc_module_exit(void);

/*
 * Description: This function is called by the IGMP Snooper to add a Router
 *              Port. Router Port is the interface on which the IGMP Snooper
 *              determines that a multicast router is present. We set a bit
 *              in the flag field of the interface list entry to mark it as
 *              router port.
 *
 * Input:       emfc - EMFC Global Instance handle
 *              ifp  - Interface pointer
 *
 * Return:      SUCCESS or FAILURE
 */
extern int32 emfc_rtport_add(struct emfc_info *emfc, void *ifp);

/*
 * Description: This function is called by the IGMP Snooper to delete a
 *              Router Port. We clear the corresponding bit in the flags
 *              field to mark the port as non-router port.
 *
 * Input:       emfc - EMFC Global Instance handle
 *              ifp  - Interface pointer
 *
 * Return:      SUCCESS or FAILURE
 */
extern int32 emfc_rtport_del(struct emfc_info *emfc, void *ifp);

/*
 * Description: This function is called by OS Specific layer when user issues
 *              configuration command to add an interface. It adds an entry to
 *              the global interface list maintained by EMFC.
 *
 * Input:       emfc - EMFC Global Instance handle
 *              ifp  - Interface pointer
 */
extern int32 emfc_iflist_add(struct emfc_info *emfc, void *ifp);

/*
 * Description: This function is called by OS Specific layer when user issues
 *              configuration command to delete an interface. It removes an
 *              entry from the global interface list maintained by EMFC.
 *
 * Input:       emfc - EMFC Global Instance handle
 *              ifp  - Interface pointer
 */
extern int32 emfc_iflist_del(struct emfc_info *emfc, void *ifp);

#endif /* _EMFC_EXPORT_H_ */
