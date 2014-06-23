/*
  Command parsing taken from brctl utility.
  Display code from stp_cli.c in rstplib.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <getopt.h>

#include <net/if.h>

/* For scanning through sysfs directories */
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#include "ctl_socket_client.h"
#include "ctl_functions.h"

#ifndef False
# define False 0
# define True 1
#endif

#define STP_IN_get_error_explanation CTL_error_explanation

static void print_bridge_id(UID_BRIDGE_ID_T * bridge_id, unsigned char cr)
{
	printf("%04lX-%02x%02x%02x%02x%02x%02x",
	       (unsigned long)bridge_id->prio,
	       (unsigned char)bridge_id->addr[0],
	       (unsigned char)bridge_id->addr[1],
	       (unsigned char)bridge_id->addr[2],
	       (unsigned char)bridge_id->addr[3],
	       (unsigned char)bridge_id->addr[4],
	       (unsigned char)bridge_id->addr[5]);
	if (cr)
		printf("\n");
}

static char *stp_state2str(RSTP_PORT_STATE stp_port_state, int detail)
{
	if (detail) {
		switch (stp_port_state) {
		case UID_PORT_DISABLED:
			return "Disabled";
		case UID_PORT_DISCARDING:
			return "Discarding";
		case UID_PORT_LEARNING:
			return "Learning";
		case UID_PORT_FORWARDING:
			return "Forwarding";
		case UID_PORT_NON_STP:
			return "NoStp";
		default:
			return "Unknown";
		}
	}

	switch (stp_port_state) {
	case UID_PORT_DISABLED:
		return "Dis";
	case UID_PORT_DISCARDING:
		return "Blk";
	case UID_PORT_LEARNING:
		return "Lrn";
	case UID_PORT_FORWARDING:
		return "Fwd";
	case UID_PORT_NON_STP:
		return "Non";
	default:
		return "Unk";
	}
}

static void CLI_out_port_id(int port, unsigned char cr)
{
	static char ifname[IFNAMSIZ];
	if (if_indextoname(port, ifname))
		printf("%s", ifname);
	else
		printf("Ifindex %02d", port);
	if (cr)
		printf("\n");
}

int get_index_die(const char *ifname, const char *doc, int die)
{
	int r = if_nametoindex(ifname);
	if (r == 0) {
		fprintf(stderr,
			"Can't find index for %s %s. Not a valid interface.\n",
			doc, ifname);
		if (die)
			exit(1);
		return -1;
	}
	return r;
}

int get_index(const char *ifname, const char *doc)
{
	return get_index_die(ifname, doc, 1);
}

static int cmd_rstp(int argc, char *const *argv)
{
	int stp, r;
	int br_index = get_index(argv[1], "bridge");

	if (!strcmp(argv[2], "on") || !strcmp(argv[2], "yes")
	    || !strcmp(argv[2], "1"))
		stp = 1;
	else if (!strcmp(argv[2], "off") || !strcmp(argv[2], "no")
		 || !strcmp(argv[2], "0"))
		stp = 0;
	else {
		fprintf(stderr, "expect on/off for argument\n");
		return 1;
	}
	r = CTL_enable_bridge_rstp(br_index, stp);
	if (r) {
		fprintf(stderr, "Failed to enable/disable RSTP: %s\n",
			CTL_error_explanation(r));
		return -1;
	}
	return 0;
}

static int do_showbridge(const char *br_name)
{
	UID_STP_STATE_T uid_state;
	UID_STP_CFG_T uid_cfg;

	int br_index = get_index_die(br_name, "bridge", 0);
	if (br_index < 0)
		return -1;

	int r = CTL_get_bridge_state(br_index, &uid_cfg, &uid_state);
	if (r) {
		fprintf(stderr, "Failed to get bridge state: %s\n",
			CTL_error_explanation(r));
		return -1;
	}
#if 0
	printf("Interface:       %-7s (tag:%d)    State: ",
	       uid_state.vlan_name, (int)uid_state.vlan_id);
#else
	printf("Bridge:          %-7s               State:",
	       uid_state.vlan_name);
#endif
	switch (uid_state.stp_enabled) {
	case STP_ENABLED:
		printf("enabled\n");
		break;
	case STP_DISABLED:
		printf("disabled\n");
		break;
	default:
		printf("unknown\n");
		return 0;
	}

	printf("BridgeId:        ");
	print_bridge_id(&uid_state.bridge_id, 0);
	printf("     Bridge Proirity: %lu (0x%lX)\n",
	       (unsigned long)uid_state.bridge_id.prio,
	       (unsigned long)uid_state.bridge_id.prio);
	if (uid_cfg.force_version < 2)
		printf("Force Version:   stp\n");

	printf("Designated Root: ");
	print_bridge_id(&uid_state.designated_root, 1);
	if (uid_state.root_port) {
		printf("Root Port:       %04lx",
		       (unsigned long)uid_state.root_port);
		//        CLI_out_port_id (uid_state.root_port & 0xfff, False);
		//    printf("not implemented"); // XXX
		printf(", Root Cost:     %-lu\n",
		       (unsigned long)uid_state.root_path_cost);
	} else {
		printf("Root Port:       none\n");
	}

	if (uid_state.Topo_Change)
		printf("Topology Change Count: %lu\n",
		       uid_state.Topo_Change_Count);
	else
		printf("Time Since Topology Change: %lu\n",
		       uid_state.timeSince_Topo_Change);

	printf("Max Age:         %2d   Bridge Max Age:       %-2d\n",
	       (int)uid_state.max_age, (int)uid_cfg.max_age);
	printf("Hello Time:      %2d   Bridge Hello Time:    %-2d\n",
	       (int)uid_state.hello_time, (int)uid_cfg.hello_time);
	printf("Forward Delay:   %2d   Bridge Forward Delay: %-2d\n",
	       (int)uid_state.forward_delay, (int)uid_cfg.forward_delay);
	printf("Hold Time:       %2d\n", (int)uid_cfg.hold_time);

	return 0;
}

#define SYSFS_PATH_MAX 256
#define SYSFS_CLASS_NET "/sys/class/net"

static int isbridge(const struct dirent *entry)
{
	char path[SYSFS_PATH_MAX];
	struct stat st;

	snprintf(path, SYSFS_PATH_MAX, SYSFS_CLASS_NET "/%s/bridge",
		 entry->d_name);
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int cmd_showbridge(int argc, char *const *argv)
{
	int i, count = 0;
	int r = 0;
	struct dirent **namelist;

	if (argc > 1) {
		count = argc - 1;
	} else {
		count =
		    scandir(SYSFS_CLASS_NET, &namelist, isbridge, alphasort);
		if (count < 0) {
			fprintf(stderr, "Error getting list of all bridges\n");
			return -1;
		}
	}

	for (i = 0; i < count; i++) {
		const char *name;
		if (argc > 1)
			name = argv[i + 1];
		else
			name = namelist[i]->d_name;

		int err = do_showbridge(name);
		if (err)
			r = err;
	}

	if (argc <= 1) {
		for (i = 0; i < count; i++)
			free(namelist[i]);
		free(namelist);
	}

	return r;
}

int detail = 0;

static int do_showport(int br_index, const char *port_name,
		       UID_STP_STATE_T * uid_state)
{
	UID_STP_PORT_STATE_T uid_port;
	UID_STP_PORT_CFG_T uid_cfg;
	int r = 0;
	int port_index = get_index_die(port_name, "port", 0);
	if (port_index < 0)
		return -1;

	memset(&uid_cfg, 0, sizeof(UID_STP_PORT_CFG_T));
	r = CTL_get_port_state(br_index, port_index, &uid_cfg, &uid_port);
	if (r) {
		fprintf(stderr, "Failed to get port state for port %d: %s\n",
			port_index, CTL_error_explanation(r));
		return -1;
	}

	if (detail) {
		printf("Stp Port ");
		CLI_out_port_id(port_index, False);
#if 0
		printf(": PortId: %04lx in vlan '%s' with tag %d:\n",
		       (unsigned long)uid_port.port_id, uid_state->vlan_name,
		       (int)uid_state->vlan_id);
#else
		printf(": PortId: %04lx in Bridge '%s':\n",
		       (unsigned long)uid_port.port_id, uid_state->vlan_name);
#endif
		printf("Priority:          %-d\n",
		       (int)(uid_port.port_id >> 8));
		printf("State:             %-16s",
		       stp_state2str(uid_port.state, 1));
		printf("       Uptime: %-9lu\n", uid_port.uptime);
		printf("PortPathCost:      admin: ");
		if (ADMIN_PORT_PATH_COST_AUTO == uid_cfg.admin_port_path_cost)
			printf("%-9s", "Auto");
		else
			printf("%-9lu", uid_cfg.admin_port_path_cost);
		printf("       oper: %-9lu\n", uid_port.oper_port_path_cost);

		printf("Point2Point:       admin: ");
		switch (uid_cfg.admin_point2point) {
		case P2P_FORCE_TRUE:
			printf("%-9s", "ForceYes");
			break;
		case P2P_FORCE_FALSE:
			printf("%-9s", "ForceNo");
			break;
		case P2P_AUTO:
			printf("%-9s", "Auto");
			break;
		}
		printf("       oper: %-9s\n",
		       uid_port.oper_point2point ? "Yes" : "No");
		printf("Edge:              admin: %-9s       oper: %-9s\n",
		       uid_cfg.admin_edge ? "Y" : "N",
		       uid_port.oper_edge ? "Y" : "N");
		printf("Partner:                                  oper: %-9s\n",
		       uid_port.oper_stp_neigb ? "Slow" : "Rapid");

		if (' ' != uid_port.role) {
			if ('-' != uid_port.role) {
				printf("PathCost:          %-lu\n",
				       (unsigned long)(uid_port.path_cost));
				printf("Designated Root:   ");
				print_bridge_id(&uid_port.designated_root, 1);
				printf("Designated Cost:   %-ld\n",
				       (unsigned long)uid_port.designated_cost);
				printf("Designated Bridge: ");
				print_bridge_id(&uid_port.designated_bridge, 1);
				printf("Designated Port:   %-4lx\n\r",
				       (unsigned long)uid_port.designated_port);
			}
			printf("Role:              ");
			switch (uid_port.role) {
			case 'A':
				printf("Alternate\n");
				break;
			case 'B':
				printf("Backup\n");
				break;
			case 'R':
				printf("Root\n");
				break;
			case 'D':
				printf("Designated\n");
				break;
			case '-':
				printf("NonStp\n");
				break;
			default:
				printf("Unknown(%c)\n", uid_port.role);
				break;
			}

			if ('R' == uid_port.role || 'D' == uid_port.role) {
				/* printf("Tc:                %c  ", uid_port.tc ? 'Y' : 'n'); */
				printf("TcAck:             %c  ",
				       uid_port.top_change_ack ? 'Y' : 'N');
				printf("TcWhile:       %3d\n",
				       (int)uid_port.tcWhile);
			}
		}

		if (UID_PORT_DISABLED == uid_port.state || '-' == uid_port.role) {
#if 0
			printf("helloWhen:       %3d  ",
			       (int)uid_port.helloWhen);
			printf("lnkWhile:      %3d\n", (int)uid_port.lnkWhile);
			printf("fdWhile:         %3d\n", (int)uid_port.fdWhile);
#endif
		} else if ('-' != uid_port.role) {
			printf("fdWhile:         %3d  ", (int)uid_port.fdWhile);
			printf("rcvdInfoWhile: %3d\n",
			       (int)uid_port.rcvdInfoWhile);
			printf("rbWhile:         %3d  ", (int)uid_port.rbWhile);
			printf("rrWhile:       %3d\n", (int)uid_port.rrWhile);
#if 0
			printf("mdelayWhile:     %3d  ",
			       (int)uid_port.mdelayWhile);
			printf("lnkWhile:      %3d\n", (int)uid_port.lnkWhile);
			printf("helloWhen:       %3d  ",
			       (int)uid_port.helloWhen);
			printf("txCount:       %3d\n", (int)uid_port.txCount);
#endif
		}

		printf("RSTP BPDU rx:      %lu\n",
		       (unsigned long)uid_port.rx_rstp_bpdu_cnt);
		printf("CONFIG BPDU rx:    %lu\n",
		       (unsigned long)uid_port.rx_cfg_bpdu_cnt);
		printf("TCN BPDU rx:       %lu\n",
		       (unsigned long)uid_port.rx_tcn_bpdu_cnt);
	} else {
		printf("%c%c%c  ",
		       (uid_port.oper_point2point) ? ' ' : '*',
		       (uid_port.oper_edge) ? 'E' : ' ',
		       (uid_port.oper_stp_neigb) ? 's' : ' ');
		CLI_out_port_id(port_index, False);
		printf(" %04lx %3s ", (unsigned long)uid_port.port_id,
		       stp_state2str(uid_port.state, 0));
		printf(" ");
		print_bridge_id(&uid_port.designated_root, 0);
		printf(" ");
		print_bridge_id(&uid_port.designated_bridge, 0);
		printf(" %4lx %c", (unsigned long)uid_port.designated_port,
		       uid_port.role);
		printf("\n");
	}
	return 0;
}

static int not_dot_dotdot(const struct dirent *entry)
{
	const char *n = entry->d_name;

	return !(n[0] == '.' && (n[1] == 0 || (n[1] == '.' && n[2] == 0)));
}

static int cmd_showport(int argc, char *const *argv)
{
	UID_STP_STATE_T uid_state;
	UID_STP_CFG_T uid_br_cfg;
	int r = 0;

	int br_index = get_index(argv[1], "bridge");

	r = CTL_get_bridge_state(br_index, &uid_br_cfg, &uid_state);
	if (r) {
		fprintf(stderr, "Failed to get bridge state: %s\n",
			CTL_error_explanation(r));
		return -1;
	}

	int i, count = 0;
	struct dirent **namelist;

	if (argc > 2) {
		count = argc - 2;
	} else {
		char buf[SYSFS_PATH_MAX];
		snprintf(buf, sizeof(buf), SYSFS_CLASS_NET "/%s/brif", argv[1]);
		count = scandir(buf, &namelist, not_dot_dotdot, alphasort);
		if (count < 0) {
			fprintf(stderr,
				"Error getting list of all ports of bridge %s\n",
				argv[1]);
			return -1;
		}
	}

	for (i = 0; i < count; i++) {
		const char *name;
		if (argc > 2)
			name = argv[i + 2];
		else
			name = namelist[i]->d_name;

		int err = do_showport(br_index, name, &uid_state);
		if (err)
			r = err;
	}

	if (argc <= 2) {
		for (i = 0; i < count; i++)
			free(namelist[i]);
		free(namelist);
	}

	return r;
}

static int cmd_showportdetail(int argc, char *const *argv)
{
	detail = 1;
	return cmd_showport(argc, argv);
}

unsigned int getuint(const char *s)
{
	char *end;
	long l;
	l = strtoul(s, &end, 0);
	if (*s == 0 || *end != 0 || l > INT_MAX) {
		fprintf(stderr, "Invalid unsigned int arg %s\n", s);
		exit(1);
	}
	return l;
}

int getenum(const char *s, const char *opt[])
{
	int i;
	for (i = 0; opt[i] != NULL; i++)
		if (strcmp(s, opt[i]) == 0)
			return i;

	fprintf(stderr, "Invalid argument %s: expecting one of ", s);
	for (i = 0; opt[i] != NULL; i++)
		fprintf(stderr, "%s%s", opt[i], (opt[i + 1] ? ", " : "\n"));

	exit(1);
}

int getyesno(const char *s, const char *yes, const char *no)
{
	/* Reverse yes and no so error message looks more normal */
	const char *opt[] = { yes, no, NULL };
	return 1 - getenum(s, opt);
}

static int set_bridge_cfg_value(int br_index, unsigned long value,
				unsigned long val_mask)
{
	UID_STP_CFG_T uid_cfg;
	char *val_name;
	int rc;

	uid_cfg.field_mask = val_mask;
	switch (val_mask) {
	case BR_CFG_STATE:
		uid_cfg.stp_enabled = value;
		val_name = "state";
		break;
	case BR_CFG_PRIO:
		uid_cfg.bridge_priority = value;
		val_name = "priority";
		break;
	case BR_CFG_AGE:
		uid_cfg.max_age = value;
		val_name = "max_age";
		break;
	case BR_CFG_HELLO:
		uid_cfg.hello_time = value;
		val_name = "hello_time";
		break;
	case BR_CFG_DELAY:
		uid_cfg.forward_delay = value;
		val_name = "forward_delay";
		break;
	case BR_CFG_FORCE_VER:
		uid_cfg.force_version = value;
		val_name = "force_version";
		break;
	case BR_CFG_AGE_MODE:
	case BR_CFG_AGE_TIME:
	default:
		printf("Invalid value mask 0X%lx\n", val_mask);
		return -1;
		break;
	}

	rc = CTL_set_bridge_config(br_index, &uid_cfg);

	if (0 != rc) {
		printf("Can't change rstp bridge %s:%s\n", val_name,
		       STP_IN_get_error_explanation(rc));
		return -1;
	}
	return 0;
}

static int cmd_setbridgestate(int argc, char *const *argv)
{

	int br_index = get_index(argv[1], "bridge");
	return set_bridge_cfg_value(br_index,
				    getyesno(argv[2], "on", "off"),
				    BR_CFG_STATE);
}

static int cmd_setbridgeprio(int argc, char *const *argv)
{

	int br_index = get_index(argv[1], "bridge");
	return set_bridge_cfg_value(br_index, getuint(argv[2]), BR_CFG_PRIO);
}

static int cmd_setbridgemaxage(int argc, char *const *argv)
{

	int br_index = get_index(argv[1], "bridge");
	return set_bridge_cfg_value(br_index, getuint(argv[2]), BR_CFG_AGE);
}

static int cmd_setbridgehello(int argc, char *const *argv)
{

	int br_index = get_index(argv[1], "bridge");
	return set_bridge_cfg_value(br_index, getuint(argv[2]), BR_CFG_HELLO);
}

static int cmd_setbridgefdelay(int argc, char *const *argv)
{

	int br_index = get_index(argv[1], "bridge");
	return set_bridge_cfg_value(br_index, getuint(argv[2]), BR_CFG_DELAY);
}

static int cmd_setbridgeforcevers(int argc, char *const *argv)
{

	int br_index = get_index(argv[1], "bridge");
	return set_bridge_cfg_value(br_index,
				    2 * getyesno(argv[2], "normal", "slow"),
				    BR_CFG_FORCE_VER);
}

static int
set_port_cfg_value(int br_index, int port_index,
		   unsigned long value, unsigned long val_mask)
{
	UID_STP_PORT_CFG_T uid_cfg;
	int rc;
	char *val_name;

	BitmapClear(&uid_cfg.port_bmp);
	uid_cfg.field_mask = val_mask;
	switch (val_mask) {
	case PT_CFG_MCHECK:
		val_name = "mcheck";
		break;
	case PT_CFG_COST:
		uid_cfg.admin_port_path_cost = value;
		val_name = "path cost";
		break;
	case PT_CFG_PRIO:
		uid_cfg.port_priority = value;
		val_name = "priority";
		break;
	case PT_CFG_P2P:
		uid_cfg.admin_point2point = (ADMIN_P2P_T) value;
		val_name = "p2p flag";
		break;
	case PT_CFG_EDGE:
		uid_cfg.admin_edge = value;
		val_name = "adminEdge";
		break;
	case PT_CFG_NON_STP:
		uid_cfg.admin_non_stp = value;
		val_name = "adminNonStp";
		break;
#ifdef STP_DBG
	case PT_CFG_DBG_SKIP_TX:
		uid_cfg.skip_tx = value;
		val_name = "skip tx";
		break;
	case PT_CFG_DBG_SKIP_RX:
		uid_cfg.skip_rx = value;
		val_name = "skip rx";
		break;
#endif
	case PT_CFG_STATE:
	default:
		printf("Invalid value mask 0X%lx\n", val_mask);
		return -1;
	}

	rc = CTL_set_port_config(br_index, port_index, &uid_cfg);

	if (0 != rc) {
		printf("can't change rstp port[s] %s: %s\n",
		       val_name, STP_IN_get_error_explanation(rc));
		return -1;
	}
	return 0;
}

static int cmd_setportprio(int argc, char *const *argv)
{

	int br_index = get_index(argv[1], "bridge");
	int port_index = get_index(argv[2], "port");
	return set_port_cfg_value(br_index, port_index,
				  getuint(argv[3]), PT_CFG_PRIO);
}

static int cmd_setportpathcost(int argc, char *const *argv)
{

	int br_index = get_index(argv[1], "bridge");
	int port_index = get_index(argv[2], "port");
	return set_port_cfg_value(br_index, port_index,
				  getuint(argv[3]), PT_CFG_COST);
}

static int cmd_setportedge(int argc, char *const *argv)
{

	int br_index = get_index(argv[1], "bridge");
	int port_index = get_index(argv[2], "port");
	return set_port_cfg_value(br_index, port_index,
				  getyesno(argv[3], "yes", "no"), PT_CFG_EDGE);
}

static int cmd_setportnonstp(int argc, char *const *argv)
{

	int br_index = get_index(argv[1], "bridge");
	int port_index = get_index(argv[2], "port");
	return set_port_cfg_value(br_index, port_index,
				  getyesno(argv[3], "yes", "no"),
				  PT_CFG_NON_STP);
}

static int cmd_setportp2p(int argc, char *const *argv)
{

	int br_index = get_index(argv[1], "bridge");
	int port_index = get_index(argv[2], "port");
	const char *opts[] = { "yes", "no", "auto", NULL };
	int vals[] = { P2P_FORCE_TRUE, P2P_FORCE_FALSE, P2P_AUTO };

	return set_port_cfg_value(br_index, port_index,
				  vals[getenum(argv[3], opts)], PT_CFG_P2P);
}

static int cmd_portmcheck(int argc, char *const *argv)
{

	int br_index = get_index(argv[1], "bridge");
	int port_index = get_index(argv[2], "port");
	return set_port_cfg_value(br_index, port_index, 0, PT_CFG_MCHECK);
}

static int cmd_debuglevel(int argc, char *const *argv)
{
	return CTL_set_debug_level(getuint(argv[1]));
}

struct command {
	int nargs;
	int optargs;
	const char *name;
	int (*func) (int argc, char *const *argv);
	const char *help;
};

static const struct command commands[] = {
	{0, 32, "showbridge", cmd_showbridge,
	 "[<bridge> ... ]\t\tshow bridge state"},
	{1, 32, "showport", cmd_showport,
	 "<bridge> [<port> ... ]\tshow port state"},
	{1, 32, "showportdetail", cmd_showportdetail,
	 "<bridge> [<port> ... ]\tshow port state (detail)"},
	{2, 0, "rstp", cmd_rstp,
	 "<bridge> {on|off}\tenable/disable rstpd control"},
	{2, 0, "setbridgestate", cmd_setbridgestate,
	 "<bridge> {on|off}\tstart/stop rstp (when enabled)"},
	{2, 0, "setbridgeprio", cmd_setbridgeprio,
	 "<bridge> <priority>\tset bridge priority (0-61440)"},
	{2, 0, "sethello", cmd_setbridgehello,
	 "<bridge> <hellotime>\tset bridge hello time (1-10)"},
	{2, 0, "setmaxage", cmd_setbridgemaxage,
	 "<bridge> <maxage>\tset bridge max age (6-40)"},
	{2, 0, "setfdelay", cmd_setbridgefdelay,
	 "<bridge> <fwd_delay>\tset bridge forward delay (4-30)"},
	{2, 0, "setforcevers", cmd_setbridgeforcevers,
	 "<bridge> {normal|slow}\tnormal RSTP or force to STP"},
	{3, 0, "setportprio", cmd_setportprio,
	 "<bridge> <port> <priority>\tset port priority (0-240)"},
	{3, 0, "setportpathcost", cmd_setportpathcost,
	 "<bridge> <port> <cost>\tset port path cost"},
	{3, 0, "setportedge", cmd_setportedge,
	 "<bridge> <port> {yes|no}\tconfigure if it is an edge port"},
	{3, 0, "setportnonstp", cmd_setportnonstp,
	 "<bridge> <port> {yes|no}\tdisable STP for the port"},
	{3, 0, "setportp2p", cmd_setportp2p,
	 "<bridge> <port> {yes|no|auto}\tset whether p2p connection"},
	{2, 0, "portmcheck", cmd_portmcheck,
	 "<bridge> <port>\ttry to get back from STP to RSTP mode"},
	{1, 0, "debuglevel", cmd_debuglevel, "<level>\t\tLevel of verbosity"},
};

const struct command *command_lookup(const char *cmd)
{
	int i;

	for (i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
		if (!strcmp(cmd, commands[i].name))
			return &commands[i];
	}

	return NULL;
}

void command_helpall(void)
{
	int i;

	for (i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
		printf("\t%-10s\t%s\n", commands[i].name, commands[i].help);
	}
}

static void help()
{
	printf("Usage: rstpctl [commands]\n");
	printf("commands:\n");
	command_helpall();
}

#define PACKAGE_VERSION2(v, b) "rstp, " #v "-" #b
#define PACKAGE_VERSION(v, b) PACKAGE_VERSION2(v, b)

int main(int argc, char *const *argv)
{
	const struct command *cmd;
	int f;
	static const struct option options[] = {
		{.name = "help",.val = 'h'},
		{.name = "version",.val = 'V'},
		{0}
	};

	while ((f = getopt_long(argc, argv, "Vh", options, NULL)) != EOF)
		switch (f) {
		case 'h':
			help();
			return 0;
		case 'V':
			printf("%s\n", PACKAGE_VERSION(VERSION, BUILD));
			return 0;
		default:
			fprintf(stderr, "Unknown option '%c'\n", f);
			goto help;
		}

	if (argc == optind)
		goto help;

	if (ctl_client_init()) {
		fprintf(stderr, "can't setup control connection\n");
		return 1;
	}

	argc -= optind;
	argv += optind;
	if ((cmd = command_lookup(argv[0])) == NULL) {
		fprintf(stderr, "never heard of command [%s]\n", argv[0]);
		goto help;
	}

	if (argc < cmd->nargs + 1 || argc > cmd->nargs + cmd->optargs + 1) {
		printf("Incorrect number of arguments for command\n");
		printf("Usage: rstpctl %s %s\n", cmd->name, cmd->help);
		return 1;
	}

	return cmd->func(argc, argv);

      help:
	help();
	return 1;
}
