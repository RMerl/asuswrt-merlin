/*****************************************************************************\
*  _  _       _          _              ___                                   *
* | || | ___ | |_  _ __ | | _  _  __ _ |_  )                                  *
* | __ |/ _ \|  _|| '_ \| || || |/ _` | / /                                   *
* |_||_|\___/ \__|| .__/|_| \_,_|\__, |/___|                                  *
*                 |_|            |___/                                        *
\*****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "mem_utils.h"
#include "hotplug2.h"
#include "childlist.h"

 struct hotplug2_child_t *add_child(struct hotplug2_child_t *child, pid_t pid, event_seqnum_t seqnum) {
	void *tmp;
	
	if (child == NULL) {
		child = xmalloc(sizeof(struct hotplug2_child_t));
		tmp = NULL;
	} else {
		for (; child->next; child = child->next);
		
		child->next = xmalloc(sizeof(struct hotplug2_child_t));
		tmp = child;
		child = child->next;
	}
	
	child->seqnum = seqnum;
	child->pid = pid;
	child->prev = tmp;
	child->next = NULL;
	
	return child;
}

struct hotplug2_child_t *remove_child_by_pid(struct hotplug2_child_t *child, pid_t pid, event_seqnum_t *largest_seqnum, int *child_c) {
	struct hotplug2_child_t *tmp_child;
	
	if (child == NULL) {
		ERROR("remove_child_by_pid", "Invalid child list passed (NULL).");
		return NULL;
	}
	
	tmp_child = child;
	
	for (; child->prev && child->pid != pid; child = child->prev);
	
	if (child->pid != pid) {
		return tmp_child;
	}
	
	if (child->prev != NULL)
		((struct hotplug2_child_t *)(child->prev))->next = child->next;
	
	if (child->next != NULL)
		((struct hotplug2_child_t *)(child->next))->prev = child->prev;
	
	if (largest_seqnum != NULL)
		if (child->seqnum > *largest_seqnum)
			*largest_seqnum = child->seqnum;
	
	if (child_c != NULL)
		*child_c -= 1;
	
	if (child == tmp_child) {
		if (child->next != NULL)
			tmp_child = child->next;
		else if (child->prev != NULL)
			tmp_child = child->prev;
		else
			tmp_child = NULL;
	}
	
	/* We always want the rightmost */
	if (tmp_child != NULL)
		for (; tmp_child->next; tmp_child = tmp_child->next);

	free(child);
	return tmp_child;
}

