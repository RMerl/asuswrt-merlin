/*****************************************************************************\
*  _  _       _          _              ___                                   *
* | || | ___ | |_  _ __ | | _  _  __ _ |_  )                                  *
* | __ |/ _ \|  _|| '_ \| || || |/ _` | / /                                   *
* |_||_|\___/ \__|| .__/|_| \_,_|\__, |/___|                                  *
*                 |_|            |___/                                        *
\*****************************************************************************/

#ifndef CHILDLIST_H
#define CHILDLIST_H 1

struct hotplug2_child_t *add_child(struct hotplug2_child_t *, pid_t, event_seqnum_t);
struct hotplug2_child_t *remove_child_by_pid(struct hotplug2_child_t *, pid_t, event_seqnum_t *, int *);

struct hotplug2_child_t {
	pid_t pid;
	event_seqnum_t seqnum;
	void *prev;
	void *next;
};

#endif /* ifndef CHILDLIST_H */
