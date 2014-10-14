/*
 * Include file for the interface to a PnP BIOS
 *
 * Original BIOS code (C) 1998 Christian Schmidt (chr.schmidt@tu-bs.de)
 * PnP handler parts (c) 1998 Tom Lees <tom@lpsg.demon.co.uk>
 * Minor reorganizations by David Hinds <dahinds@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LINUX_PNPBIOS_H
#define _LINUX_PNPBIOS_H

#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/pnp.h>

/*
 * Return codes
 */
#define PNP_SUCCESS                     0x00
#define PNP_NOT_SET_STATICALLY          0x7f
#define PNP_UNKNOWN_FUNCTION            0x81
#define PNP_FUNCTION_NOT_SUPPORTED      0x82
#define PNP_INVALID_HANDLE              0x83
#define PNP_BAD_PARAMETER               0x84
#define PNP_SET_FAILED                  0x85
#define PNP_EVENTS_NOT_PENDING          0x86
#define PNP_SYSTEM_NOT_DOCKED           0x87
#define PNP_NO_ISA_PNP_CARDS            0x88
#define PNP_UNABLE_TO_DETERMINE_DOCK_CAPABILITIES 0x89
#define PNP_CONFIG_CHANGE_FAILED_NO_BATTERY 0x8a
#define PNP_CONFIG_CHANGE_FAILED_RESOURCE_CONFLICT 0x8b
#define PNP_BUFFER_TOO_SMALL            0x8c
#define PNP_USE_ESCD_SUPPORT            0x8d
#define PNP_MESSAGE_NOT_SUPPORTED       0x8e
#define PNP_HARDWARE_ERROR              0x8f

#define ESCD_SUCCESS                    0x00
#define ESCD_IO_ERROR_READING           0x55
#define ESCD_INVALID                    0x56
#define ESCD_BUFFER_TOO_SMALL           0x59
#define ESCD_NVRAM_TOO_SMALL            0x5a
#define ESCD_FUNCTION_NOT_SUPPORTED     0x81

/*
 * Events that can be received by "get event"
 */
#define PNPEV_ABOUT_TO_CHANGE_CONFIG	0x0001
#define PNPEV_DOCK_CHANGED		0x0002
#define PNPEV_SYSTEM_DEVICE_CHANGED	0x0003
#define PNPEV_CONFIG_CHANGED_FAILED	0x0004
#define PNPEV_UNKNOWN_SYSTEM_EVENT	0xffff
/* 0x8000 through 0xfffe are OEM defined */

/*
 * Messages that should be sent through "send message"
 */
#define PNPMSG_OK			0x00
#define PNPMSG_ABORT			0x01
#define PNPMSG_UNDOCK_DEFAULT_ACTION	0x40
#define PNPMSG_POWER_OFF		0x41
#define PNPMSG_PNP_OS_ACTIVE		0x42
#define PNPMSG_PNP_OS_INACTIVE		0x43

/*
 * Plug and Play BIOS flags
 */
#define PNPBIOS_NO_DISABLE		0x0001
#define PNPBIOS_NO_CONFIG		0x0002
#define PNPBIOS_OUTPUT			0x0004
#define PNPBIOS_INPUT			0x0008
#define PNPBIOS_BOOTABLE		0x0010
#define PNPBIOS_DOCK			0x0020
#define PNPBIOS_REMOVABLE		0x0040
#define pnpbios_is_static(x) (((x)->flags & 0x0100) == 0x0000)
#define pnpbios_is_dynamic(x) ((x)->flags & 0x0080)

/*
 * Function Parameters
 */
#define PNPMODE_STATIC 1
#define PNPMODE_DYNAMIC 0

/* 0x8000 through 0xffff are OEM defined */

#pragma pack(1)
struct pnp_dev_node_info {
	__u16	no_nodes;
	__u16	max_node_size;
};
struct pnp_docking_station_info {
	__u32	location_id;
	__u32	serial;
	__u16	capabilities;
};
struct pnp_isa_config_struc {
	__u8	revision;
	__u8	no_csns;
	__u16	isa_rd_data_port;
	__u16	reserved;
};
struct escd_info_struc {
	__u16	min_escd_write_size;
	__u16	escd_size;
	__u32	nv_storage_base;
};
struct pnp_bios_node {
	__u16	size;
	__u8	handle;
	__u32	eisa_id;
	__u8	type_code[3];
	__u16	flags;
	__u8	data[0];
};
#pragma pack()

#ifdef CONFIG_PNPBIOS

/* non-exported */
extern struct pnp_dev_node_info node_info;

extern int pnp_bios_dev_node_info (struct pnp_dev_node_info *data);
extern int pnp_bios_get_dev_node (u8 *nodenum, char config, struct pnp_bios_node *data);
extern int pnp_bios_set_dev_node (u8 nodenum, char config, struct pnp_bios_node *data);
extern int pnp_bios_get_stat_res (char *info);
extern int pnp_bios_isapnp_config (struct pnp_isa_config_struc *data);
extern int pnp_bios_escd_info (struct escd_info_struc *data);
extern int pnp_bios_read_escd (char *data, u32 nvram_base);
extern int pnp_bios_dock_station_info(struct pnp_docking_station_info *data);
#define needed 0
#if needed
extern int pnp_bios_get_event (u16 *message);
extern int pnp_bios_send_message (u16 message);
extern int pnp_bios_set_stat_res (char *info);
extern int pnp_bios_apm_id_table (char *table, u16 *size);
extern int pnp_bios_write_escd (char *data, u32 nvram_base);
#endif

#endif /* CONFIG_PNPBIOS */

#endif /* __KERNEL__ */

#endif /* _LINUX_PNPBIOS_H */
