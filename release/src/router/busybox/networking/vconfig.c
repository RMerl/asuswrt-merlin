/* vi: set sw=4 ts=4: */
/*
 * vconfig implementation for busybox
 *
 * Copyright (C) 2001  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 N/A */

#include "libbb.h"
#include <net/if.h>

/* Stuff from linux/if_vlan.h, kernel version 2.4.23 */
enum vlan_ioctl_cmds {
	ADD_VLAN_CMD,
	DEL_VLAN_CMD,
	SET_VLAN_INGRESS_PRIORITY_CMD,
	SET_VLAN_EGRESS_PRIORITY_CMD,
	GET_VLAN_INGRESS_PRIORITY_CMD,
	GET_VLAN_EGRESS_PRIORITY_CMD,
	SET_VLAN_NAME_TYPE_CMD,
	SET_VLAN_FLAG_CMD
};
enum vlan_name_types {
	VLAN_NAME_TYPE_PLUS_VID, /* Name will look like:  vlan0005 */
	VLAN_NAME_TYPE_RAW_PLUS_VID, /* name will look like:  eth1.0005 */
	VLAN_NAME_TYPE_PLUS_VID_NO_PAD, /* Name will look like:  vlan5 */
	VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD, /* Name will look like:  eth0.5 */
	VLAN_NAME_TYPE_HIGHEST
};

struct vlan_ioctl_args {
	int cmd; /* Should be one of the vlan_ioctl_cmds enum above. */
	char device1[24];

	union {
		char device2[24];
		int VID;
		unsigned int skb_priority;
		unsigned int name_type;
		unsigned int bind_type;
		unsigned int flag; /* Matches vlan_dev_info flags */
	} u;

	short vlan_qos;
};

#define VLAN_GROUP_ARRAY_LEN 4096
#define SIOCSIFVLAN	0x8983		/* Set 802.1Q VLAN options */

/* On entry, table points to the length of the current string
 * plus NUL terminator plus data length for the subsequent entry.
 * The return value is the last data entry for the matching string. */
static const char *xfind_str(const char *table, const char *str)
{
	while (strcasecmp(str, table+1) != 0) {
		table += table[0];
		if (!*table) {
			bb_show_usage();
		}
	}
	return table - 1;
}

static const char cmds[] ALIGN1 = {
	4, ADD_VLAN_CMD, 7,
	'a', 'd', 'd', 0,
	3, DEL_VLAN_CMD, 7,
	'r', 'e', 'm', 0,
	3, SET_VLAN_NAME_TYPE_CMD, 17,
	's', 'e', 't', '_',
	'n', 'a', 'm', 'e', '_',
	't', 'y', 'p', 'e', 0,
	5, SET_VLAN_FLAG_CMD, 12,
	's', 'e', 't', '_',
	'f', 'l', 'a', 'g', 0,
	5, SET_VLAN_EGRESS_PRIORITY_CMD, 18,
	's', 'e', 't', '_',
	'e', 'g', 'r', 'e', 's', 's', '_',
	'm', 'a', 'p', 0,
	5, SET_VLAN_INGRESS_PRIORITY_CMD, 16,
	's', 'e', 't', '_',
	'i', 'n', 'g', 'r', 'e', 's', 's', '_',
	'm', 'a', 'p', 0,
};

static const char name_types[] ALIGN1 = {
	VLAN_NAME_TYPE_PLUS_VID, 16,
	'V', 'L', 'A', 'N',
	'_', 'P', 'L', 'U', 'S', '_', 'V', 'I', 'D',
	0,
	VLAN_NAME_TYPE_PLUS_VID_NO_PAD, 22,
	'V', 'L', 'A', 'N',
	'_', 'P', 'L', 'U', 'S', '_', 'V', 'I', 'D',
	'_', 'N', 'O', '_', 'P', 'A', 'D', 0,
	VLAN_NAME_TYPE_RAW_PLUS_VID, 15,
	'D', 'E', 'V',
	'_', 'P', 'L', 'U', 'S', '_', 'V', 'I', 'D',
	0,
	VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD, 20,
	'D', 'E', 'V',
	'_', 'P', 'L', 'U', 'S', '_', 'V', 'I', 'D',
	'_', 'N', 'O', '_', 'P', 'A', 'D', 0,
};

static const char conf_file_name[] ALIGN1 = "/proc/net/vlan/config";

int vconfig_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int vconfig_main(int argc, char **argv)
{
	struct vlan_ioctl_args ifr;
	const char *p;
	int fd;

	if (argc < 3) {
		bb_show_usage();
	}

	/* Don't bother closing the filedes.  It will be closed on cleanup. */
	/* Will die if 802.1q is not present */
	xopen(conf_file_name, O_RDONLY);

	memset(&ifr, 0, sizeof(ifr));

	++argv;
	p = xfind_str(cmds+2, *argv);
	ifr.cmd = *p;
	if (argc != p[-1]) {
		bb_show_usage();
	}

	if (ifr.cmd == SET_VLAN_NAME_TYPE_CMD) { /* set_name_type */
		ifr.u.name_type = *xfind_str(name_types+1, argv[1]);
	} else {
		strncpy_IFNAMSIZ(ifr.device1, argv[1]);
		p = argv[2];

		/* I suppose one could try to combine some of the function calls below,
		 * since ifr.u.flag, ifr.u.VID, and ifr.u.skb_priority are all same-sized
		 * (unsigned) int members of a unions.  But because of the range checking,
		 * doing so wouldn't save that much space and would also make maintainence
		 * more of a pain. */
		if (ifr.cmd == SET_VLAN_FLAG_CMD) { /* set_flag */
			ifr.u.flag = xatoul_range(p, 0, 1);
			/* DM: in order to set reorder header, qos must be set */
			ifr.vlan_qos = xatoul_range(argv[3], 0, 7);
		} else if (ifr.cmd == ADD_VLAN_CMD) { /* add */
			ifr.u.VID = xatoul_range(p, 0, VLAN_GROUP_ARRAY_LEN-1);
		} else if (ifr.cmd != DEL_VLAN_CMD) { /* set_{egress|ingress}_map */
			ifr.u.skb_priority = xatou(p);
			ifr.vlan_qos = xatoul_range(argv[3], 0, 7);
		}
	}

	fd = xsocket(AF_INET, SOCK_STREAM, 0);
	ioctl_or_perror_and_die(fd, SIOCSIFVLAN, &ifr,
						"ioctl error for %s", *argv);

	return 0;
}
