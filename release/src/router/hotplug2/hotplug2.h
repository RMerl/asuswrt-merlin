/*****************************************************************************\
*  _  _       _          _              ___                                   *
* | || | ___ | |_  _ __ | | _  _  __ _ |_  )                                  *
* | __ |/ _ \|  _|| '_ \| || || |/ _` | / /                                   *
* |_||_|\___/ \__|| .__/|_| \_,_|\__, |/___|                                  *
*                 |_|            |___/                                        *
\*****************************************************************************/


#ifndef HOTPLUG2_H
#define HOTPLUG2_H 1

/*
 * Various compatibility definitions.
 */
#ifndef SO_RCVBUFFORCE
#if defined(__alpha__) || defined(__hppa__) || defined(__sparc__) || defined(__sparc_v9__)
#define SO_RCVBUFFORCE 			0x100b
#else
#define SO_RCVBUFFORCE			33
#endif
#endif

#ifndef SO_SNDBUFFORCE
#if defined(__alpha__) || defined(__hppa__) || defined(__sparc__) || defined(__sparc_v9__)
#define SO_SNDBUFFORCE 			0x100a
#else
#define SO_SNDBUFFORCE			32
#endif
#endif

#ifndef NETLINK_KOBJECT_UEVENT
#define NETLINK_KOBJECT_UEVENT		15
#endif

#ifndef O_NOATIME
#define O_NOATIME			01000000
#endif

#define ERROR(action, fmt, arg...)	fprintf(stderr, "[%s]: " fmt"\n", action, ##arg);
#define INFO(action, fmt, arg...)	fprintf(stdout, "[%s]: " fmt"\n", action, ##arg);
#ifdef DEBUG
#define DBG(action, fmt, arg...)	fprintf(stdout, "[%s]: " fmt"\n", action, ##arg);
#else
#define DBG(action, fmt, arg...)
#endif

#define HOTPLUG2_MSG_BACKLOG	64
#define UEVENT_BUFFER_SIZE		2048
#define HOTPLUG2_THROTTLE_INTERVAL	50
#define HOTPLUG2_RULE_PATH		"/etc/hotplug2.rules"

#define ACTION_ADD			0
#define ACTION_REMOVE			1
#define ACTION_UNKNOWN			-1

#define UDEVTRIGGER_COMMAND		"/sbin/udevtrigger", "/sbin/udevtrigger"

#define FORK_CHILD			0
#define FORK_ERROR			-1
#define FORK_FINISHED			-2

#define event_seqnum_t			unsigned long long

#define sysfs_seqnum_path		"/sys/kernel/uevent_seqnum"

struct env_var_t {
	char *key;
	char *value;
};

struct hotplug2_event_t {
	int action;
	event_seqnum_t seqnum;
	struct env_var_t *env_vars;
	int env_vars_c;
	char *plain;
	int plain_s;
	struct hotplug2_event_t *next;
};

struct options_t {
	char *name;
	int *value;
};

char *get_hotplug2_value_by_key(struct hotplug2_event_t *, char *);

#endif /* ifndef HOTPLUG2_H */

