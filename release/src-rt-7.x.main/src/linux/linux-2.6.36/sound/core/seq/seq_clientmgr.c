/*
 *  ALSA sequencer Client Manager
 *  Copyright (c) 1998-2001 by Frank van de Pol <fvdpol@coil.demon.nl>
 *                             Jaroslav Kysela <perex@perex.cz>
 *                             Takashi Iwai <tiwai@suse.de>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/minors.h>
#include <linux/kmod.h>

#include <sound/seq_kernel.h>
#include "seq_clientmgr.h"
#include "seq_memory.h"
#include "seq_queue.h"
#include "seq_timer.h"
#include "seq_info.h"
#include "seq_system.h"
#include <sound/seq_device.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

/* Client Manager

 * this module handles the connections of userland and kernel clients
 * 
 */

/*
 * There are four ranges of client numbers (last two shared):
 * 0..15: global clients
 * 16..127: statically allocated client numbers for cards 0..27
 * 128..191: dynamically allocated client numbers for cards 28..31
 * 128..191: dynamically allocated client numbers for applications
 */

/* number of kernel non-card clients */
#define SNDRV_SEQ_GLOBAL_CLIENTS	16
/* clients per cards, for static clients */
#define SNDRV_SEQ_CLIENTS_PER_CARD	4
/* dynamically allocated client numbers (both kernel drivers and user space) */
#define SNDRV_SEQ_DYNAMIC_CLIENTS_BEGIN	128

#define SNDRV_SEQ_LFLG_INPUT	0x0001
#define SNDRV_SEQ_LFLG_OUTPUT	0x0002
#define SNDRV_SEQ_LFLG_OPEN	(SNDRV_SEQ_LFLG_INPUT|SNDRV_SEQ_LFLG_OUTPUT)

static DEFINE_SPINLOCK(clients_lock);
static DEFINE_MUTEX(register_mutex);

/*
 * client table
 */
static char clienttablock[SNDRV_SEQ_MAX_CLIENTS];
static struct snd_seq_client *clienttab[SNDRV_SEQ_MAX_CLIENTS];
static struct snd_seq_usage client_usage;

/*
 * prototypes
 */
static int bounce_error_event(struct snd_seq_client *client,
			      struct snd_seq_event *event,
			      int err, int atomic, int hop);
static int snd_seq_deliver_single_event(struct snd_seq_client *client,
					struct snd_seq_event *event,
					int filter, int atomic, int hop);

/*
 */
 
static inline mm_segment_t snd_enter_user(void)
{
	mm_segment_t fs = get_fs();
	set_fs(get_ds());
	return fs;
}

static inline void snd_leave_user(mm_segment_t fs)
{
	set_fs(fs);
}

/*
 */
static inline unsigned short snd_seq_file_flags(struct file *file)
{
        switch (file->f_mode & (FMODE_READ | FMODE_WRITE)) {
        case FMODE_WRITE:
                return SNDRV_SEQ_LFLG_OUTPUT;
        case FMODE_READ:
                return SNDRV_SEQ_LFLG_INPUT;
        default:
                return SNDRV_SEQ_LFLG_OPEN;
        }
}

static inline int snd_seq_write_pool_allocated(struct snd_seq_client *client)
{
	return snd_seq_total_cells(client->pool) > 0;
}

/* return pointer to client structure for specified id */
static struct snd_seq_client *clientptr(int clientid)
{
	if (clientid < 0 || clientid >= SNDRV_SEQ_MAX_CLIENTS) {
		snd_printd("Seq: oops. Trying to get pointer to client %d\n",
			   clientid);
		return NULL;
	}
	return clienttab[clientid];
}

struct snd_seq_client *snd_seq_client_use_ptr(int clientid)
{
	unsigned long flags;
	struct snd_seq_client *client;

	if (clientid < 0 || clientid >= SNDRV_SEQ_MAX_CLIENTS) {
		snd_printd("Seq: oops. Trying to get pointer to client %d\n",
			   clientid);
		return NULL;
	}
	spin_lock_irqsave(&clients_lock, flags);
	client = clientptr(clientid);
	if (client)
		goto __lock;
	if (clienttablock[clientid]) {
		spin_unlock_irqrestore(&clients_lock, flags);
		return NULL;
	}
	spin_unlock_irqrestore(&clients_lock, flags);
#ifdef CONFIG_MODULES
	if (!in_interrupt()) {
		static char client_requested[SNDRV_SEQ_GLOBAL_CLIENTS];
		static char card_requested[SNDRV_CARDS];
		if (clientid < SNDRV_SEQ_GLOBAL_CLIENTS) {
			int idx;
			
			if (!client_requested[clientid]) {
				client_requested[clientid] = 1;
				for (idx = 0; idx < 15; idx++) {
					if (seq_client_load[idx] < 0)
						break;
					if (seq_client_load[idx] == clientid) {
						request_module("snd-seq-client-%i",
							       clientid);
						break;
					}
				}
			}
		} else if (clientid < SNDRV_SEQ_DYNAMIC_CLIENTS_BEGIN) {
			int card = (clientid - SNDRV_SEQ_GLOBAL_CLIENTS) /
				SNDRV_SEQ_CLIENTS_PER_CARD;
			if (card < snd_ecards_limit) {
				if (! card_requested[card]) {
					card_requested[card] = 1;
					snd_request_card(card);
				}
				snd_seq_device_load_drivers();
			}
		}
		spin_lock_irqsave(&clients_lock, flags);
		client = clientptr(clientid);
		if (client)
			goto __lock;
		spin_unlock_irqrestore(&clients_lock, flags);
	}
#endif
	return NULL;

      __lock:
	snd_use_lock_use(&client->use_lock);
	spin_unlock_irqrestore(&clients_lock, flags);
	return client;
}

static void usage_alloc(struct snd_seq_usage *res, int num)
{
	res->cur += num;
	if (res->cur > res->peak)
		res->peak = res->cur;
}

static void usage_free(struct snd_seq_usage *res, int num)
{
	res->cur -= num;
}

/* initialise data structures */
int __init client_init_data(void)
{
	/* zap out the client table */
	memset(&clienttablock, 0, sizeof(clienttablock));
	memset(&clienttab, 0, sizeof(clienttab));
	return 0;
}


static struct snd_seq_client *seq_create_client1(int client_index, int poolsize)
{
	unsigned long flags;
	int c;
	struct snd_seq_client *client;

	/* init client data */
	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (client == NULL)
		return NULL;
	client->pool = snd_seq_pool_new(poolsize);
	if (client->pool == NULL) {
		kfree(client);
		return NULL;
	}
	client->type = NO_CLIENT;
	snd_use_lock_init(&client->use_lock);
	rwlock_init(&client->ports_lock);
	mutex_init(&client->ports_mutex);
	INIT_LIST_HEAD(&client->ports_list_head);

	/* find free slot in the client table */
	spin_lock_irqsave(&clients_lock, flags);
	if (client_index < 0) {
		for (c = SNDRV_SEQ_DYNAMIC_CLIENTS_BEGIN;
		     c < SNDRV_SEQ_MAX_CLIENTS;
		     c++) {
			if (clienttab[c] || clienttablock[c])
				continue;
			clienttab[client->number = c] = client;
			spin_unlock_irqrestore(&clients_lock, flags);
			return client;
		}
	} else {
		if (clienttab[client_index] == NULL && !clienttablock[client_index]) {
			clienttab[client->number = client_index] = client;
			spin_unlock_irqrestore(&clients_lock, flags);
			return client;
		}
	}
	spin_unlock_irqrestore(&clients_lock, flags);
	snd_seq_pool_delete(&client->pool);
	kfree(client);
	return NULL;	/* no free slot found or busy, return failure code */
}


static int seq_free_client1(struct snd_seq_client *client)
{
	unsigned long flags;

	if (!client)
		return 0;
	snd_seq_delete_all_ports(client);
	snd_seq_queue_client_leave(client->number);
	spin_lock_irqsave(&clients_lock, flags);
	clienttablock[client->number] = 1;
	clienttab[client->number] = NULL;
	spin_unlock_irqrestore(&clients_lock, flags);
	snd_use_lock_sync(&client->use_lock);
	snd_seq_queue_client_termination(client->number);
	if (client->pool)
		snd_seq_pool_delete(&client->pool);
	spin_lock_irqsave(&clients_lock, flags);
	clienttablock[client->number] = 0;
	spin_unlock_irqrestore(&clients_lock, flags);
	return 0;
}


static void seq_free_client(struct snd_seq_client * client)
{
	mutex_lock(&register_mutex);
	switch (client->type) {
	case NO_CLIENT:
		snd_printk(KERN_WARNING "Seq: Trying to free unused client %d\n",
			   client->number);
		break;
	case USER_CLIENT:
	case KERNEL_CLIENT:
		seq_free_client1(client);
		usage_free(&client_usage, 1);
		break;

	default:
		snd_printk(KERN_ERR "Seq: Trying to free client %d with undefined type = %d\n",
			   client->number, client->type);
	}
	mutex_unlock(&register_mutex);

	snd_seq_system_client_ev_client_exit(client->number);
}



/* -------------------------------------------------------- */

/* create a user client */
static int snd_seq_open(struct inode *inode, struct file *file)
{
	int c, mode;			/* client id */
	struct snd_seq_client *client;
	struct snd_seq_user_client *user;
	int err;

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;

	if (mutex_lock_interruptible(&register_mutex))
		return -ERESTARTSYS;
	client = seq_create_client1(-1, SNDRV_SEQ_DEFAULT_EVENTS);
	if (client == NULL) {
		mutex_unlock(&register_mutex);
		return -ENOMEM;	/* failure code */
	}

	mode = snd_seq_file_flags(file);
	if (mode & SNDRV_SEQ_LFLG_INPUT)
		client->accept_input = 1;
	if (mode & SNDRV_SEQ_LFLG_OUTPUT)
		client->accept_output = 1;

	user = &client->data.user;
	user->fifo = NULL;
	user->fifo_pool_size = 0;

	if (mode & SNDRV_SEQ_LFLG_INPUT) {
		user->fifo_pool_size = SNDRV_SEQ_DEFAULT_CLIENT_EVENTS;
		user->fifo = snd_seq_fifo_new(user->fifo_pool_size);
		if (user->fifo == NULL) {
			seq_free_client1(client);
			kfree(client);
			mutex_unlock(&register_mutex);
			return -ENOMEM;
		}
	}

	usage_alloc(&client_usage, 1);
	client->type = USER_CLIENT;
	mutex_unlock(&register_mutex);

	c = client->number;
	file->private_data = client;

	/* fill client data */
	user->file = file;
	sprintf(client->name, "Client-%d", c);

	/* make others aware this new client */
	snd_seq_system_client_ev_client_start(c);

	return 0;
}

/* delete a user client */
static int snd_seq_release(struct inode *inode, struct file *file)
{
	struct snd_seq_client *client = file->private_data;

	if (client) {
		seq_free_client(client);
		if (client->data.user.fifo)
			snd_seq_fifo_delete(&client->data.user.fifo);
		kfree(client);
	}

	return 0;
}


/* handle client read() */
/* possible error values:
 *	-ENXIO	invalid client or file open mode
 *	-ENOSPC	FIFO overflow (the flag is cleared after this error report)
 *	-EINVAL	no enough user-space buffer to write the whole event
 *	-EFAULT	seg. fault during copy to user space
 */
static ssize_t snd_seq_read(struct file *file, char __user *buf, size_t count,
			    loff_t *offset)
{
	struct snd_seq_client *client = file->private_data;
	struct snd_seq_fifo *fifo;
	int err;
	long result = 0;
	struct snd_seq_event_cell *cell;

	if (!(snd_seq_file_flags(file) & SNDRV_SEQ_LFLG_INPUT))
		return -ENXIO;

	if (!access_ok(VERIFY_WRITE, buf, count))
		return -EFAULT;

	/* check client structures are in place */
	if (snd_BUG_ON(!client))
		return -ENXIO;

	if (!client->accept_input || (fifo = client->data.user.fifo) == NULL)
		return -ENXIO;

	if (atomic_read(&fifo->overflow) > 0) {
		/* buffer overflow is detected */
		snd_seq_fifo_clear(fifo);
		/* return error code */
		return -ENOSPC;
	}

	cell = NULL;
	err = 0;
	snd_seq_fifo_lock(fifo);

	/* while data available in queue */
	while (count >= sizeof(struct snd_seq_event)) {
		int nonblock;

		nonblock = (file->f_flags & O_NONBLOCK) || result > 0;
		if ((err = snd_seq_fifo_cell_out(fifo, &cell, nonblock)) < 0) {
			break;
		}
		if (snd_seq_ev_is_variable(&cell->event)) {
			struct snd_seq_event tmpev;
			tmpev = cell->event;
			tmpev.data.ext.len &= ~SNDRV_SEQ_EXT_MASK;
			if (copy_to_user(buf, &tmpev, sizeof(struct snd_seq_event))) {
				err = -EFAULT;
				break;
			}
			count -= sizeof(struct snd_seq_event);
			buf += sizeof(struct snd_seq_event);
			err = snd_seq_expand_var_event(&cell->event, count,
						       (char __force *)buf, 0,
						       sizeof(struct snd_seq_event));
			if (err < 0)
				break;
			result += err;
			count -= err;
			buf += err;
		} else {
			if (copy_to_user(buf, &cell->event, sizeof(struct snd_seq_event))) {
				err = -EFAULT;
				break;
			}
			count -= sizeof(struct snd_seq_event);
			buf += sizeof(struct snd_seq_event);
		}
		snd_seq_cell_free(cell);
		cell = NULL; /* to be sure */
		result += sizeof(struct snd_seq_event);
	}

	if (err < 0) {
		if (cell)
			snd_seq_fifo_cell_putback(fifo, cell);
		if (err == -EAGAIN && result > 0)
			err = 0;
	}
	snd_seq_fifo_unlock(fifo);

	return (err < 0) ? err : result;
}


/*
 * check access permission to the port
 */
static int check_port_perm(struct snd_seq_client_port *port, unsigned int flags)
{
	if ((port->capability & flags) != flags)
		return 0;
	return flags;
}

/*
 * check if the destination client is available, and return the pointer
 * if filter is non-zero, client filter bitmap is tested.
 */
static struct snd_seq_client *get_event_dest_client(struct snd_seq_event *event,
						    int filter)
{
	struct snd_seq_client *dest;

	dest = snd_seq_client_use_ptr(event->dest.client);
	if (dest == NULL)
		return NULL;
	if (! dest->accept_input)
		goto __not_avail;
	if ((dest->filter & SNDRV_SEQ_FILTER_USE_EVENT) &&
	    ! test_bit(event->type, dest->event_filter))
		goto __not_avail;
	if (filter && !(dest->filter & filter))
		goto __not_avail;

	return dest; /* ok - accessible */
__not_avail:
	snd_seq_client_unlock(dest);
	return NULL;
}


/*
 * Return the error event.
 *
 * If the receiver client is a user client, the original event is
 * encapsulated in SNDRV_SEQ_EVENT_BOUNCE as variable length event.  If
 * the original event is also variable length, the external data is
 * copied after the event record. 
 * If the receiver client is a kernel client, the original event is
 * quoted in SNDRV_SEQ_EVENT_KERNEL_ERROR, since this requires no extra
 * kmalloc.
 */
static int bounce_error_event(struct snd_seq_client *client,
			      struct snd_seq_event *event,
			      int err, int atomic, int hop)
{
	struct snd_seq_event bounce_ev;
	int result;

	if (client == NULL ||
	    ! (client->filter & SNDRV_SEQ_FILTER_BOUNCE) ||
	    ! client->accept_input)
		return 0; /* ignored */

	/* set up quoted error */
	memset(&bounce_ev, 0, sizeof(bounce_ev));
	bounce_ev.type = SNDRV_SEQ_EVENT_KERNEL_ERROR;
	bounce_ev.flags = SNDRV_SEQ_EVENT_LENGTH_FIXED;
	bounce_ev.queue = SNDRV_SEQ_QUEUE_DIRECT;
	bounce_ev.source.client = SNDRV_SEQ_CLIENT_SYSTEM;
	bounce_ev.source.port = SNDRV_SEQ_PORT_SYSTEM_ANNOUNCE;
	bounce_ev.dest.client = client->number;
	bounce_ev.dest.port = event->source.port;
	bounce_ev.data.quote.origin = event->dest;
	bounce_ev.data.quote.event = event;
	bounce_ev.data.quote.value = -err; /* use positive value */
	result = snd_seq_deliver_single_event(NULL, &bounce_ev, 0, atomic, hop + 1);
	if (result < 0) {
		client->event_lost++;
		return result;
	}

	return result;
}


/*
 * rewrite the time-stamp of the event record with the curren time
 * of the given queue.
 * return non-zero if updated.
 */
static int update_timestamp_of_queue(struct snd_seq_event *event,
				     int queue, int real_time)
{
	struct snd_seq_queue *q;

	q = queueptr(queue);
	if (! q)
		return 0;
	event->queue = queue;
	event->flags &= ~SNDRV_SEQ_TIME_STAMP_MASK;
	if (real_time) {
		event->time.time = snd_seq_timer_get_cur_time(q->timer);
		event->flags |= SNDRV_SEQ_TIME_STAMP_REAL;
	} else {
		event->time.tick = snd_seq_timer_get_cur_tick(q->timer);
		event->flags |= SNDRV_SEQ_TIME_STAMP_TICK;
	}
	queuefree(q);
	return 1;
}


/*
 * deliver an event to the specified destination.
 * if filter is non-zero, client filter bitmap is tested.
 *
 *  RETURN VALUE: 0 : if succeeded
 *		 <0 : error
 */
static int snd_seq_deliver_single_event(struct snd_seq_client *client,
					struct snd_seq_event *event,
					int filter, int atomic, int hop)
{
	struct snd_seq_client *dest = NULL;
	struct snd_seq_client_port *dest_port = NULL;
	int result = -ENOENT;
	int direct;

	direct = snd_seq_ev_is_direct(event);

	dest = get_event_dest_client(event, filter);
	if (dest == NULL)
		goto __skip;
	dest_port = snd_seq_port_use_ptr(dest, event->dest.port);
	if (dest_port == NULL)
		goto __skip;

	/* check permission */
	if (! check_port_perm(dest_port, SNDRV_SEQ_PORT_CAP_WRITE)) {
		result = -EPERM;
		goto __skip;
	}
		
	if (dest_port->timestamping)
		update_timestamp_of_queue(event, dest_port->time_queue,
					  dest_port->time_real);

	switch (dest->type) {
	case USER_CLIENT:
		if (dest->data.user.fifo)
			result = snd_seq_fifo_event_in(dest->data.user.fifo, event);
		break;

	case KERNEL_CLIENT:
		if (dest_port->event_input == NULL)
			break;
		result = dest_port->event_input(event, direct,
						dest_port->private_data,
						atomic, hop);
		break;
	default:
		break;
	}

  __skip:
	if (dest_port)
		snd_seq_port_unlock(dest_port);
	if (dest)
		snd_seq_client_unlock(dest);

	if (result < 0 && !direct) {
		result = bounce_error_event(client, event, result, atomic, hop);
	}
	return result;
}


/*
 * send the event to all subscribers:
 */
static int deliver_to_subscribers(struct snd_seq_client *client,
				  struct snd_seq_event *event,
				  int atomic, int hop)
{
	struct snd_seq_subscribers *subs;
	int err = 0, num_ev = 0;
	struct snd_seq_event event_saved;
	struct snd_seq_client_port *src_port;
	struct snd_seq_port_subs_info *grp;

	src_port = snd_seq_port_use_ptr(client, event->source.port);
	if (src_port == NULL)
		return -EINVAL; /* invalid source port */
	/* save original event record */
	event_saved = *event;
	grp = &src_port->c_src;
	
	/* lock list */
	if (atomic)
		read_lock(&grp->list_lock);
	else
		down_read(&grp->list_mutex);
	list_for_each_entry(subs, &grp->list_head, src_list) {
		event->dest = subs->info.dest;
		if (subs->info.flags & SNDRV_SEQ_PORT_SUBS_TIMESTAMP)
			/* convert time according to flag with subscription */
			update_timestamp_of_queue(event, subs->info.queue,
						  subs->info.flags & SNDRV_SEQ_PORT_SUBS_TIME_REAL);
		err = snd_seq_deliver_single_event(client, event,
						   0, atomic, hop);
		if (err < 0)
			break;
		num_ev++;
		/* restore original event record */
		*event = event_saved;
	}
	if (atomic)
		read_unlock(&grp->list_lock);
	else
		up_read(&grp->list_mutex);
	*event = event_saved; /* restore */
	snd_seq_port_unlock(src_port);
	return (err < 0) ? err : num_ev;
}


#ifdef SUPPORT_BROADCAST 
/*
 * broadcast to all ports:
 */
static int port_broadcast_event(struct snd_seq_client *client,
				struct snd_seq_event *event,
				int atomic, int hop)
{
	int num_ev = 0, err = 0;
	struct snd_seq_client *dest_client;
	struct snd_seq_client_port *port;

	dest_client = get_event_dest_client(event, SNDRV_SEQ_FILTER_BROADCAST);
	if (dest_client == NULL)
		return 0; /* no matching destination */

	read_lock(&dest_client->ports_lock);
	list_for_each_entry(port, &dest_client->ports_list_head, list) {
		event->dest.port = port->addr.port;
		/* pass NULL as source client to avoid error bounce */
		err = snd_seq_deliver_single_event(NULL, event,
						   SNDRV_SEQ_FILTER_BROADCAST,
						   atomic, hop);
		if (err < 0)
			break;
		num_ev++;
	}
	read_unlock(&dest_client->ports_lock);
	snd_seq_client_unlock(dest_client);
	event->dest.port = SNDRV_SEQ_ADDRESS_BROADCAST; /* restore */
	return (err < 0) ? err : num_ev;
}

/*
 * send the event to all clients:
 * if destination port is also ADDRESS_BROADCAST, deliver to all ports.
 */
static int broadcast_event(struct snd_seq_client *client,
			   struct snd_seq_event *event, int atomic, int hop)
{
	int err = 0, num_ev = 0;
	int dest;
	struct snd_seq_addr addr;

	addr = event->dest; /* save */

	for (dest = 0; dest < SNDRV_SEQ_MAX_CLIENTS; dest++) {
		/* don't send to itself */
		if (dest == client->number)
			continue;
		event->dest.client = dest;
		event->dest.port = addr.port;
		if (addr.port == SNDRV_SEQ_ADDRESS_BROADCAST)
			err = port_broadcast_event(client, event, atomic, hop);
		else
			/* pass NULL as source client to avoid error bounce */
			err = snd_seq_deliver_single_event(NULL, event,
							   SNDRV_SEQ_FILTER_BROADCAST,
							   atomic, hop);
		if (err < 0)
			break;
		num_ev += err;
	}
	event->dest = addr; /* restore */
	return (err < 0) ? err : num_ev;
}


/* multicast - not supported yet */
static int multicast_event(struct snd_seq_client *client, struct snd_seq_event *event,
			   int atomic, int hop)
{
	snd_printd("seq: multicast not supported yet.\n");
	return 0; /* ignored */
}
#endif /* SUPPORT_BROADCAST */


/* deliver an event to the destination port(s).
 * if the event is to subscribers or broadcast, the event is dispatched
 * to multiple targets.
 *
 * RETURN VALUE: n > 0  : the number of delivered events.
 *               n == 0 : the event was not passed to any client.
 *               n < 0  : error - event was not processed.
 */
static int snd_seq_deliver_event(struct snd_seq_client *client, struct snd_seq_event *event,
				 int atomic, int hop)
{
	int result;

	hop++;
	if (hop >= SNDRV_SEQ_MAX_HOPS) {
		snd_printd("too long delivery path (%d:%d->%d:%d)\n",
			   event->source.client, event->source.port,
			   event->dest.client, event->dest.port);
		return -EMLINK;
	}

	if (event->queue == SNDRV_SEQ_ADDRESS_SUBSCRIBERS ||
	    event->dest.client == SNDRV_SEQ_ADDRESS_SUBSCRIBERS)
		result = deliver_to_subscribers(client, event, atomic, hop);
#ifdef SUPPORT_BROADCAST
	else if (event->queue == SNDRV_SEQ_ADDRESS_BROADCAST ||
		 event->dest.client == SNDRV_SEQ_ADDRESS_BROADCAST)
		result = broadcast_event(client, event, atomic, hop);
	else if (event->dest.client >= SNDRV_SEQ_MAX_CLIENTS)
		result = multicast_event(client, event, atomic, hop);
	else if (event->dest.port == SNDRV_SEQ_ADDRESS_BROADCAST)
		result = port_broadcast_event(client, event, atomic, hop);
#endif
	else
		result = snd_seq_deliver_single_event(client, event, 0, atomic, hop);

	return result;
}

/*
 * dispatch an event cell:
 * This function is called only from queue check routines in timer
 * interrupts or after enqueued.
 * The event cell shall be released or re-queued in this function.
 *
 * RETURN VALUE: n > 0  : the number of delivered events.
 *		 n == 0 : the event was not passed to any client.
 *		 n < 0  : error - event was not processed.
 */
int snd_seq_dispatch_event(struct snd_seq_event_cell *cell, int atomic, int hop)
{
	struct snd_seq_client *client;
	int result;

	if (snd_BUG_ON(!cell))
		return -EINVAL;

	client = snd_seq_client_use_ptr(cell->event.source.client);
	if (client == NULL) {
		snd_seq_cell_free(cell); /* release this cell */
		return -EINVAL;
	}

	if (cell->event.type == SNDRV_SEQ_EVENT_NOTE) {
		/* NOTE event:
		 * the event cell is re-used as a NOTE-OFF event and
		 * enqueued again.
		 */
		struct snd_seq_event tmpev, *ev;

		/* reserve this event to enqueue note-off later */
		tmpev = cell->event;
		tmpev.type = SNDRV_SEQ_EVENT_NOTEON;
		result = snd_seq_deliver_event(client, &tmpev, atomic, hop);

		/*
		 * This was originally a note event.  We now re-use the
		 * cell for the note-off event.
		 */

		ev = &cell->event;
		ev->type = SNDRV_SEQ_EVENT_NOTEOFF;
		ev->flags |= SNDRV_SEQ_PRIORITY_HIGH;

		/* add the duration time */
		switch (ev->flags & SNDRV_SEQ_TIME_STAMP_MASK) {
		case SNDRV_SEQ_TIME_STAMP_TICK:
			ev->time.tick += ev->data.note.duration;
			break;
		case SNDRV_SEQ_TIME_STAMP_REAL:
			/* unit for duration is ms */
			ev->time.time.tv_nsec += 1000000 * (ev->data.note.duration % 1000);
			ev->time.time.tv_sec += ev->data.note.duration / 1000 +
						ev->time.time.tv_nsec / 1000000000;
			ev->time.time.tv_nsec %= 1000000000;
			break;
		}
		ev->data.note.velocity = ev->data.note.off_velocity;

		/* Now queue this cell as the note off event */
		if (snd_seq_enqueue_event(cell, atomic, hop) < 0)
			snd_seq_cell_free(cell); /* release this cell */

	} else {
		/* Normal events:
		 * event cell is freed after processing the event
		 */

		result = snd_seq_deliver_event(client, &cell->event, atomic, hop);
		snd_seq_cell_free(cell);
	}

	snd_seq_client_unlock(client);
	return result;
}


/* Allocate a cell from client pool and enqueue it to queue:
 * if pool is empty and blocking is TRUE, sleep until a new cell is
 * available.
 */
static int snd_seq_client_enqueue_event(struct snd_seq_client *client,
					struct snd_seq_event *event,
					struct file *file, int blocking,
					int atomic, int hop)
{
	struct snd_seq_event_cell *cell;
	int err;

	/* special queue values - force direct passing */
	if (event->queue == SNDRV_SEQ_ADDRESS_SUBSCRIBERS) {
		event->dest.client = SNDRV_SEQ_ADDRESS_SUBSCRIBERS;
		event->queue = SNDRV_SEQ_QUEUE_DIRECT;
	} else
#ifdef SUPPORT_BROADCAST
		if (event->queue == SNDRV_SEQ_ADDRESS_BROADCAST) {
			event->dest.client = SNDRV_SEQ_ADDRESS_BROADCAST;
			event->queue = SNDRV_SEQ_QUEUE_DIRECT;
		}
#endif
	if (event->dest.client == SNDRV_SEQ_ADDRESS_SUBSCRIBERS) {
		/* check presence of source port */
		struct snd_seq_client_port *src_port = snd_seq_port_use_ptr(client, event->source.port);
		if (src_port == NULL)
			return -EINVAL;
		snd_seq_port_unlock(src_port);
	}

	/* direct event processing without enqueued */
	if (snd_seq_ev_is_direct(event)) {
		if (event->type == SNDRV_SEQ_EVENT_NOTE)
			return -EINVAL; /* this event must be enqueued! */
		return snd_seq_deliver_event(client, event, atomic, hop);
	}

	/* Not direct, normal queuing */
	if (snd_seq_queue_is_used(event->queue, client->number) <= 0)
		return -EINVAL;  /* invalid queue */
	if (! snd_seq_write_pool_allocated(client))
		return -ENXIO; /* queue is not allocated */

	/* allocate an event cell */
	err = snd_seq_event_dup(client->pool, event, &cell, !blocking || atomic, file);
	if (err < 0)
		return err;

	/* we got a cell. enqueue it. */
	if ((err = snd_seq_enqueue_event(cell, atomic, hop)) < 0) {
		snd_seq_cell_free(cell);
		return err;
	}

	return 0;
}


/*
 * check validity of event type and data length.
 * return non-zero if invalid.
 */
static int check_event_type_and_length(struct snd_seq_event *ev)
{
	switch (snd_seq_ev_length_type(ev)) {
	case SNDRV_SEQ_EVENT_LENGTH_FIXED:
		if (snd_seq_ev_is_variable_type(ev))
			return -EINVAL;
		break;
	case SNDRV_SEQ_EVENT_LENGTH_VARIABLE:
		if (! snd_seq_ev_is_variable_type(ev) ||
		    (ev->data.ext.len & ~SNDRV_SEQ_EXT_MASK) >= SNDRV_SEQ_MAX_EVENT_LEN)
			return -EINVAL;
		break;
	case SNDRV_SEQ_EVENT_LENGTH_VARUSR:
		if (! snd_seq_ev_is_direct(ev))
			return -EINVAL;
		break;
	}
	return 0;
}


/* handle write() */
/* possible error values:
 *	-ENXIO	invalid client or file open mode
 *	-ENOMEM	malloc failed
 *	-EFAULT	seg. fault during copy from user space
 *	-EINVAL	invalid event
 *	-EAGAIN	no space in output pool
 *	-EINTR	interrupts while sleep
 *	-EMLINK	too many hops
 *	others	depends on return value from driver callback
 */
static ssize_t snd_seq_write(struct file *file, const char __user *buf,
			     size_t count, loff_t *offset)
{
	struct snd_seq_client *client = file->private_data;
	int written = 0, len;
	int err = -EINVAL;
	struct snd_seq_event event;

	if (!(snd_seq_file_flags(file) & SNDRV_SEQ_LFLG_OUTPUT))
		return -ENXIO;

	/* check client structures are in place */
	if (snd_BUG_ON(!client))
		return -ENXIO;
		
	if (!client->accept_output || client->pool == NULL)
		return -ENXIO;

	/* allocate the pool now if the pool is not allocated yet */ 
	if (client->pool->size > 0 && !snd_seq_write_pool_allocated(client)) {
		if (snd_seq_pool_init(client->pool) < 0)
			return -ENOMEM;
	}

	/* only process whole events */
	while (count >= sizeof(struct snd_seq_event)) {
		/* Read in the event header from the user */
		len = sizeof(event);
		if (copy_from_user(&event, buf, len)) {
			err = -EFAULT;
			break;
		}
		event.source.client = client->number;	/* fill in client number */
		/* Check for extension data length */
		if (check_event_type_and_length(&event)) {
			err = -EINVAL;
			break;
		}

		/* check for special events */
		if (event.type == SNDRV_SEQ_EVENT_NONE)
			goto __skip_event;
		else if (snd_seq_ev_is_reserved(&event)) {
			err = -EINVAL;
			break;
		}

		if (snd_seq_ev_is_variable(&event)) {
			int extlen = event.data.ext.len & ~SNDRV_SEQ_EXT_MASK;
			if ((size_t)(extlen + len) > count) {
				/* back out, will get an error this time or next */
				err = -EINVAL;
				break;
			}
			/* set user space pointer */
			event.data.ext.len = extlen | SNDRV_SEQ_EXT_USRPTR;
			event.data.ext.ptr = (char __force *)buf
						+ sizeof(struct snd_seq_event);
			len += extlen; /* increment data length */
		} else {
#ifdef CONFIG_COMPAT
			if (client->convert32 && snd_seq_ev_is_varusr(&event)) {
				void *ptr = compat_ptr(event.data.raw32.d[1]);
				event.data.ext.ptr = ptr;
			}
#endif
		}

		/* ok, enqueue it */
		err = snd_seq_client_enqueue_event(client, &event, file,
						   !(file->f_flags & O_NONBLOCK),
						   0, 0);
		if (err < 0)
			break;

	__skip_event:
		/* Update pointers and counts */
		count -= len;
		buf += len;
		written += len;
	}

	return written ? written : err;
}


/*
 * handle polling
 */
static unsigned int snd_seq_poll(struct file *file, poll_table * wait)
{
	struct snd_seq_client *client = file->private_data;
	unsigned int mask = 0;

	/* check client structures are in place */
	if (snd_BUG_ON(!client))
		return -ENXIO;

	if ((snd_seq_file_flags(file) & SNDRV_SEQ_LFLG_INPUT) &&
	    client->data.user.fifo) {

		/* check if data is available in the outqueue */
		if (snd_seq_fifo_poll_wait(client->data.user.fifo, file, wait))
			mask |= POLLIN | POLLRDNORM;
	}

	if (snd_seq_file_flags(file) & SNDRV_SEQ_LFLG_OUTPUT) {

		/* check if data is available in the pool */
		if (!snd_seq_write_pool_allocated(client) ||
		    snd_seq_pool_poll_wait(client->pool, file, wait))
			mask |= POLLOUT | POLLWRNORM;
	}

	return mask;
}


/*-----------------------------------------------------*/


/* SYSTEM_INFO ioctl() */
static int snd_seq_ioctl_system_info(struct snd_seq_client *client, void __user *arg)
{
	struct snd_seq_system_info info;

	memset(&info, 0, sizeof(info));
	/* fill the info fields */
	info.queues = SNDRV_SEQ_MAX_QUEUES;
	info.clients = SNDRV_SEQ_MAX_CLIENTS;
	info.ports = 256;	/* fixed limit */
	info.channels = 256;	/* fixed limit */
	info.cur_clients = client_usage.cur;
	info.cur_queues = snd_seq_queue_get_cur_queues();

	if (copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;
	return 0;
}


/* RUNNING_MODE ioctl() */
static int snd_seq_ioctl_running_mode(struct snd_seq_client *client, void __user *arg)
{
	struct snd_seq_running_info info;
	struct snd_seq_client *cptr;
	int err = 0;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	/* requested client number */
	cptr = snd_seq_client_use_ptr(info.client);
	if (cptr == NULL)
		return -ENOENT;		/* don't change !!! */

#ifdef SNDRV_BIG_ENDIAN
	if (! info.big_endian) {
		err = -EINVAL;
		goto __err;
	}
#else
	if (info.big_endian) {
		err = -EINVAL;
		goto __err;
	}

#endif
	if (info.cpu_mode > sizeof(long)) {
		err = -EINVAL;
		goto __err;
	}
	cptr->convert32 = (info.cpu_mode < sizeof(long));
 __err:
	snd_seq_client_unlock(cptr);
	return err;
}

/* CLIENT_INFO ioctl() */
static void get_client_info(struct snd_seq_client *cptr,
			    struct snd_seq_client_info *info)
{
	info->client = cptr->number;

	/* fill the info fields */
	info->type = cptr->type;
	strcpy(info->name, cptr->name);
	info->filter = cptr->filter;
	info->event_lost = cptr->event_lost;
	memcpy(info->event_filter, cptr->event_filter, 32);
	info->num_ports = cptr->num_ports;
	memset(info->reserved, 0, sizeof(info->reserved));
}

static int snd_seq_ioctl_get_client_info(struct snd_seq_client *client,
					 void __user *arg)
{
	struct snd_seq_client *cptr;
	struct snd_seq_client_info client_info;

	if (copy_from_user(&client_info, arg, sizeof(client_info)))
		return -EFAULT;

	/* requested client number */
	cptr = snd_seq_client_use_ptr(client_info.client);
	if (cptr == NULL)
		return -ENOENT;		/* don't change !!! */

	get_client_info(cptr, &client_info);
	snd_seq_client_unlock(cptr);

	if (copy_to_user(arg, &client_info, sizeof(client_info)))
		return -EFAULT;
	return 0;
}


/* CLIENT_INFO ioctl() */
static int snd_seq_ioctl_set_client_info(struct snd_seq_client *client,
					 void __user *arg)
{
	struct snd_seq_client_info client_info;

	if (copy_from_user(&client_info, arg, sizeof(client_info)))
		return -EFAULT;

	/* it is not allowed to set the info fields for an another client */
	if (client->number != client_info.client)
		return -EPERM;
	/* also client type must be set now */
	if (client->type != client_info.type)
		return -EINVAL;

	/* fill the info fields */
	if (client_info.name[0])
		strlcpy(client->name, client_info.name, sizeof(client->name));

	client->filter = client_info.filter;
	client->event_lost = client_info.event_lost;
	memcpy(client->event_filter, client_info.event_filter, 32);

	return 0;
}


/* 
 * CREATE PORT ioctl() 
 */
static int snd_seq_ioctl_create_port(struct snd_seq_client *client,
				     void __user *arg)
{
	struct snd_seq_client_port *port;
	struct snd_seq_port_info info;
	struct snd_seq_port_callback *callback;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	/* it is not allowed to create the port for an another client */
	if (info.addr.client != client->number)
		return -EPERM;

	port = snd_seq_create_port(client, (info.flags & SNDRV_SEQ_PORT_FLG_GIVEN_PORT) ? info.addr.port : -1);
	if (port == NULL)
		return -ENOMEM;

	if (client->type == USER_CLIENT && info.kernel) {
		snd_seq_delete_port(client, port->addr.port);
		return -EINVAL;
	}
	if (client->type == KERNEL_CLIENT) {
		if ((callback = info.kernel) != NULL) {
			if (callback->owner)
				port->owner = callback->owner;
			port->private_data = callback->private_data;
			port->private_free = callback->private_free;
			port->callback_all = callback->callback_all;
			port->event_input = callback->event_input;
			port->c_src.open = callback->subscribe;
			port->c_src.close = callback->unsubscribe;
			port->c_dest.open = callback->use;
			port->c_dest.close = callback->unuse;
		}
	}

	info.addr = port->addr;

	snd_seq_set_port_info(port, &info);
	snd_seq_system_client_ev_port_start(port->addr.client, port->addr.port);

	if (copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;

	return 0;
}

/* 
 * DELETE PORT ioctl() 
 */
static int snd_seq_ioctl_delete_port(struct snd_seq_client *client,
				     void __user *arg)
{
	struct snd_seq_port_info info;
	int err;

	/* set passed parameters */
	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;
	
	/* it is not allowed to remove the port for an another client */
	if (info.addr.client != client->number)
		return -EPERM;

	err = snd_seq_delete_port(client, info.addr.port);
	if (err >= 0)
		snd_seq_system_client_ev_port_exit(client->number, info.addr.port);
	return err;
}


/* 
 * GET_PORT_INFO ioctl() (on any client) 
 */
static int snd_seq_ioctl_get_port_info(struct snd_seq_client *client,
				       void __user *arg)
{
	struct snd_seq_client *cptr;
	struct snd_seq_client_port *port;
	struct snd_seq_port_info info;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;
	cptr = snd_seq_client_use_ptr(info.addr.client);
	if (cptr == NULL)
		return -ENXIO;

	port = snd_seq_port_use_ptr(cptr, info.addr.port);
	if (port == NULL) {
		snd_seq_client_unlock(cptr);
		return -ENOENT;			/* don't change */
	}

	/* get port info */
	snd_seq_get_port_info(port, &info);
	snd_seq_port_unlock(port);
	snd_seq_client_unlock(cptr);

	if (copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;
	return 0;
}


/* 
 * SET_PORT_INFO ioctl() (only ports on this/own client) 
 */
static int snd_seq_ioctl_set_port_info(struct snd_seq_client *client,
				       void __user *arg)
{
	struct snd_seq_client_port *port;
	struct snd_seq_port_info info;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	if (info.addr.client != client->number) /* only set our own ports ! */
		return -EPERM;
	port = snd_seq_port_use_ptr(client, info.addr.port);
	if (port) {
		snd_seq_set_port_info(port, &info);
		snd_seq_port_unlock(port);
	}
	return 0;
}


/*
 * port subscription (connection)
 */
#define PERM_RD		(SNDRV_SEQ_PORT_CAP_READ|SNDRV_SEQ_PORT_CAP_SUBS_READ)
#define PERM_WR		(SNDRV_SEQ_PORT_CAP_WRITE|SNDRV_SEQ_PORT_CAP_SUBS_WRITE)

static int check_subscription_permission(struct snd_seq_client *client,
					 struct snd_seq_client_port *sport,
					 struct snd_seq_client_port *dport,
					 struct snd_seq_port_subscribe *subs)
{
	if (client->number != subs->sender.client &&
	    client->number != subs->dest.client) {
		/* connection by third client - check export permission */
		if (check_port_perm(sport, SNDRV_SEQ_PORT_CAP_NO_EXPORT))
			return -EPERM;
		if (check_port_perm(dport, SNDRV_SEQ_PORT_CAP_NO_EXPORT))
			return -EPERM;
	}

	/* check read permission */
	/* if sender or receiver is the subscribing client itself,
	 * no permission check is necessary
	 */
	if (client->number != subs->sender.client) {
		if (! check_port_perm(sport, PERM_RD))
			return -EPERM;
	}
	/* check write permission */
	if (client->number != subs->dest.client) {
		if (! check_port_perm(dport, PERM_WR))
			return -EPERM;
	}
	return 0;
}

/*
 * send an subscription notify event to user client:
 * client must be user client.
 */
int snd_seq_client_notify_subscription(int client, int port,
				       struct snd_seq_port_subscribe *info,
				       int evtype)
{
	struct snd_seq_event event;

	memset(&event, 0, sizeof(event));
	event.type = evtype;
	event.data.connect.dest = info->dest;
	event.data.connect.sender = info->sender;

	return snd_seq_system_notify(client, port, &event);  /* non-atomic */
}


/* 
 * add to port's subscription list IOCTL interface 
 */
static int snd_seq_ioctl_subscribe_port(struct snd_seq_client *client,
					void __user *arg)
{
	int result = -EINVAL;
	struct snd_seq_client *receiver = NULL, *sender = NULL;
	struct snd_seq_client_port *sport = NULL, *dport = NULL;
	struct snd_seq_port_subscribe subs;

	if (copy_from_user(&subs, arg, sizeof(subs)))
		return -EFAULT;

	if ((receiver = snd_seq_client_use_ptr(subs.dest.client)) == NULL)
		goto __end;
	if ((sender = snd_seq_client_use_ptr(subs.sender.client)) == NULL)
		goto __end;
	if ((sport = snd_seq_port_use_ptr(sender, subs.sender.port)) == NULL)
		goto __end;
	if ((dport = snd_seq_port_use_ptr(receiver, subs.dest.port)) == NULL)
		goto __end;

	result = check_subscription_permission(client, sport, dport, &subs);
	if (result < 0)
		goto __end;

	/* connect them */
	result = snd_seq_port_connect(client, sender, sport, receiver, dport, &subs);
	if (! result) /* broadcast announce */
		snd_seq_client_notify_subscription(SNDRV_SEQ_ADDRESS_SUBSCRIBERS, 0,
						   &subs, SNDRV_SEQ_EVENT_PORT_SUBSCRIBED);
      __end:
      	if (sport)
		snd_seq_port_unlock(sport);
	if (dport)
		snd_seq_port_unlock(dport);
	if (sender)
		snd_seq_client_unlock(sender);
	if (receiver)
		snd_seq_client_unlock(receiver);
	return result;
}


/* 
 * remove from port's subscription list 
 */
static int snd_seq_ioctl_unsubscribe_port(struct snd_seq_client *client,
					  void __user *arg)
{
	int result = -ENXIO;
	struct snd_seq_client *receiver = NULL, *sender = NULL;
	struct snd_seq_client_port *sport = NULL, *dport = NULL;
	struct snd_seq_port_subscribe subs;

	if (copy_from_user(&subs, arg, sizeof(subs)))
		return -EFAULT;

	if ((receiver = snd_seq_client_use_ptr(subs.dest.client)) == NULL)
		goto __end;
	if ((sender = snd_seq_client_use_ptr(subs.sender.client)) == NULL)
		goto __end;
	if ((sport = snd_seq_port_use_ptr(sender, subs.sender.port)) == NULL)
		goto __end;
	if ((dport = snd_seq_port_use_ptr(receiver, subs.dest.port)) == NULL)
		goto __end;

	result = check_subscription_permission(client, sport, dport, &subs);
	if (result < 0)
		goto __end;

	result = snd_seq_port_disconnect(client, sender, sport, receiver, dport, &subs);
	if (! result) /* broadcast announce */
		snd_seq_client_notify_subscription(SNDRV_SEQ_ADDRESS_SUBSCRIBERS, 0,
						   &subs, SNDRV_SEQ_EVENT_PORT_UNSUBSCRIBED);
      __end:
      	if (sport)
		snd_seq_port_unlock(sport);
	if (dport)
		snd_seq_port_unlock(dport);
	if (sender)
		snd_seq_client_unlock(sender);
	if (receiver)
		snd_seq_client_unlock(receiver);
	return result;
}


/* CREATE_QUEUE ioctl() */
static int snd_seq_ioctl_create_queue(struct snd_seq_client *client,
				      void __user *arg)
{
	struct snd_seq_queue_info info;
	int result;
	struct snd_seq_queue *q;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	result = snd_seq_queue_alloc(client->number, info.locked, info.flags);
	if (result < 0)
		return result;

	q = queueptr(result);
	if (q == NULL)
		return -EINVAL;

	info.queue = q->queue;
	info.locked = q->locked;
	info.owner = q->owner;

	/* set queue name */
	if (! info.name[0])
		snprintf(info.name, sizeof(info.name), "Queue-%d", q->queue);
	strlcpy(q->name, info.name, sizeof(q->name));
	queuefree(q);

	if (copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;

	return 0;
}

/* DELETE_QUEUE ioctl() */
static int snd_seq_ioctl_delete_queue(struct snd_seq_client *client,
				      void __user *arg)
{
	struct snd_seq_queue_info info;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	return snd_seq_queue_delete(client->number, info.queue);
}

/* GET_QUEUE_INFO ioctl() */
static int snd_seq_ioctl_get_queue_info(struct snd_seq_client *client,
					void __user *arg)
{
	struct snd_seq_queue_info info;
	struct snd_seq_queue *q;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	q = queueptr(info.queue);
	if (q == NULL)
		return -EINVAL;

	memset(&info, 0, sizeof(info));
	info.queue = q->queue;
	info.owner = q->owner;
	info.locked = q->locked;
	strlcpy(info.name, q->name, sizeof(info.name));
	queuefree(q);

	if (copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;

	return 0;
}

/* SET_QUEUE_INFO ioctl() */
static int snd_seq_ioctl_set_queue_info(struct snd_seq_client *client,
					void __user *arg)
{
	struct snd_seq_queue_info info;
	struct snd_seq_queue *q;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	if (info.owner != client->number)
		return -EINVAL;

	/* change owner/locked permission */
	if (snd_seq_queue_check_access(info.queue, client->number)) {
		if (snd_seq_queue_set_owner(info.queue, client->number, info.locked) < 0)
			return -EPERM;
		if (info.locked)
			snd_seq_queue_use(info.queue, client->number, 1);
	} else {
		return -EPERM;
	}	

	q = queueptr(info.queue);
	if (! q)
		return -EINVAL;
	if (q->owner != client->number) {
		queuefree(q);
		return -EPERM;
	}
	strlcpy(q->name, info.name, sizeof(q->name));
	queuefree(q);

	return 0;
}

/* GET_NAMED_QUEUE ioctl() */
static int snd_seq_ioctl_get_named_queue(struct snd_seq_client *client, void __user *arg)
{
	struct snd_seq_queue_info info;
	struct snd_seq_queue *q;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	q = snd_seq_queue_find_name(info.name);
	if (q == NULL)
		return -EINVAL;
	info.queue = q->queue;
	info.owner = q->owner;
	info.locked = q->locked;
	queuefree(q);

	if (copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;

	return 0;
}

/* GET_QUEUE_STATUS ioctl() */
static int snd_seq_ioctl_get_queue_status(struct snd_seq_client *client,
					  void __user *arg)
{
	struct snd_seq_queue_status status;
	struct snd_seq_queue *queue;
	struct snd_seq_timer *tmr;

	if (copy_from_user(&status, arg, sizeof(status)))
		return -EFAULT;

	queue = queueptr(status.queue);
	if (queue == NULL)
		return -EINVAL;
	memset(&status, 0, sizeof(status));
	status.queue = queue->queue;
	
	tmr = queue->timer;
	status.events = queue->tickq->cells + queue->timeq->cells;

	status.time = snd_seq_timer_get_cur_time(tmr);
	status.tick = snd_seq_timer_get_cur_tick(tmr);

	status.running = tmr->running;

	status.flags = queue->flags;
	queuefree(queue);

	if (copy_to_user(arg, &status, sizeof(status)))
		return -EFAULT;
	return 0;
}


/* GET_QUEUE_TEMPO ioctl() */
static int snd_seq_ioctl_get_queue_tempo(struct snd_seq_client *client,
					 void __user *arg)
{
	struct snd_seq_queue_tempo tempo;
	struct snd_seq_queue *queue;
	struct snd_seq_timer *tmr;

	if (copy_from_user(&tempo, arg, sizeof(tempo)))
		return -EFAULT;

	queue = queueptr(tempo.queue);
	if (queue == NULL)
		return -EINVAL;
	memset(&tempo, 0, sizeof(tempo));
	tempo.queue = queue->queue;
	
	tmr = queue->timer;

	tempo.tempo = tmr->tempo;
	tempo.ppq = tmr->ppq;
	tempo.skew_value = tmr->skew;
	tempo.skew_base = tmr->skew_base;
	queuefree(queue);

	if (copy_to_user(arg, &tempo, sizeof(tempo)))
		return -EFAULT;
	return 0;
}


/* SET_QUEUE_TEMPO ioctl() */
int snd_seq_set_queue_tempo(int client, struct snd_seq_queue_tempo *tempo)
{
	if (!snd_seq_queue_check_access(tempo->queue, client))
		return -EPERM;
	return snd_seq_queue_timer_set_tempo(tempo->queue, client, tempo);
}

EXPORT_SYMBOL(snd_seq_set_queue_tempo);

static int snd_seq_ioctl_set_queue_tempo(struct snd_seq_client *client,
					 void __user *arg)
{
	int result;
	struct snd_seq_queue_tempo tempo;

	if (copy_from_user(&tempo, arg, sizeof(tempo)))
		return -EFAULT;

	result = snd_seq_set_queue_tempo(client->number, &tempo);
	return result < 0 ? result : 0;
}


/* GET_QUEUE_TIMER ioctl() */
static int snd_seq_ioctl_get_queue_timer(struct snd_seq_client *client,
					 void __user *arg)
{
	struct snd_seq_queue_timer timer;
	struct snd_seq_queue *queue;
	struct snd_seq_timer *tmr;

	if (copy_from_user(&timer, arg, sizeof(timer)))
		return -EFAULT;

	queue = queueptr(timer.queue);
	if (queue == NULL)
		return -EINVAL;

	if (mutex_lock_interruptible(&queue->timer_mutex)) {
		queuefree(queue);
		return -ERESTARTSYS;
	}
	tmr = queue->timer;
	memset(&timer, 0, sizeof(timer));
	timer.queue = queue->queue;

	timer.type = tmr->type;
	if (tmr->type == SNDRV_SEQ_TIMER_ALSA) {
		timer.u.alsa.id = tmr->alsa_id;
		timer.u.alsa.resolution = tmr->preferred_resolution;
	}
	mutex_unlock(&queue->timer_mutex);
	queuefree(queue);
	
	if (copy_to_user(arg, &timer, sizeof(timer)))
		return -EFAULT;
	return 0;
}


/* SET_QUEUE_TIMER ioctl() */
static int snd_seq_ioctl_set_queue_timer(struct snd_seq_client *client,
					 void __user *arg)
{
	int result = 0;
	struct snd_seq_queue_timer timer;

	if (copy_from_user(&timer, arg, sizeof(timer)))
		return -EFAULT;

	if (timer.type != SNDRV_SEQ_TIMER_ALSA)
		return -EINVAL;

	if (snd_seq_queue_check_access(timer.queue, client->number)) {
		struct snd_seq_queue *q;
		struct snd_seq_timer *tmr;

		q = queueptr(timer.queue);
		if (q == NULL)
			return -ENXIO;
		if (mutex_lock_interruptible(&q->timer_mutex)) {
			queuefree(q);
			return -ERESTARTSYS;
		}
		tmr = q->timer;
		snd_seq_queue_timer_close(timer.queue);
		tmr->type = timer.type;
		if (tmr->type == SNDRV_SEQ_TIMER_ALSA) {
			tmr->alsa_id = timer.u.alsa.id;
			tmr->preferred_resolution = timer.u.alsa.resolution;
		}
		result = snd_seq_queue_timer_open(timer.queue);
		mutex_unlock(&q->timer_mutex);
		queuefree(q);
	} else {
		return -EPERM;
	}	

	return result;
}


/* GET_QUEUE_CLIENT ioctl() */
static int snd_seq_ioctl_get_queue_client(struct snd_seq_client *client,
					  void __user *arg)
{
	struct snd_seq_queue_client info;
	int used;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	used = snd_seq_queue_is_used(info.queue, client->number);
	if (used < 0)
		return -EINVAL;
	info.used = used;
	info.client = client->number;

	if (copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;
	return 0;
}


/* SET_QUEUE_CLIENT ioctl() */
static int snd_seq_ioctl_set_queue_client(struct snd_seq_client *client,
					  void __user *arg)
{
	int err;
	struct snd_seq_queue_client info;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	if (info.used >= 0) {
		err = snd_seq_queue_use(info.queue, client->number, info.used);
		if (err < 0)
			return err;
	}

	return snd_seq_ioctl_get_queue_client(client, arg);
}


/* GET_CLIENT_POOL ioctl() */
static int snd_seq_ioctl_get_client_pool(struct snd_seq_client *client,
					 void __user *arg)
{
	struct snd_seq_client_pool info;
	struct snd_seq_client *cptr;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	cptr = snd_seq_client_use_ptr(info.client);
	if (cptr == NULL)
		return -ENOENT;
	memset(&info, 0, sizeof(info));
	info.output_pool = cptr->pool->size;
	info.output_room = cptr->pool->room;
	info.output_free = info.output_pool;
	info.output_free = snd_seq_unused_cells(cptr->pool);
	if (cptr->type == USER_CLIENT) {
		info.input_pool = cptr->data.user.fifo_pool_size;
		info.input_free = info.input_pool;
		if (cptr->data.user.fifo)
			info.input_free = snd_seq_unused_cells(cptr->data.user.fifo->pool);
	} else {
		info.input_pool = 0;
		info.input_free = 0;
	}
	snd_seq_client_unlock(cptr);
	
	if (copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;
	return 0;
}

/* SET_CLIENT_POOL ioctl() */
static int snd_seq_ioctl_set_client_pool(struct snd_seq_client *client,
					 void __user *arg)
{
	struct snd_seq_client_pool info;
	int rc;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	if (client->number != info.client)
		return -EINVAL; /* can't change other clients */

	if (info.output_pool >= 1 && info.output_pool <= SNDRV_SEQ_MAX_EVENTS &&
	    (! snd_seq_write_pool_allocated(client) ||
	     info.output_pool != client->pool->size)) {
		if (snd_seq_write_pool_allocated(client)) {
			/* remove all existing cells */
			snd_seq_queue_client_leave_cells(client->number);
			snd_seq_pool_done(client->pool);
		}
		client->pool->size = info.output_pool;
		rc = snd_seq_pool_init(client->pool);
		if (rc < 0)
			return rc;
	}
	if (client->type == USER_CLIENT && client->data.user.fifo != NULL &&
	    info.input_pool >= 1 &&
	    info.input_pool <= SNDRV_SEQ_MAX_CLIENT_EVENTS &&
	    info.input_pool != client->data.user.fifo_pool_size) {
		/* change pool size */
		rc = snd_seq_fifo_resize(client->data.user.fifo, info.input_pool);
		if (rc < 0)
			return rc;
		client->data.user.fifo_pool_size = info.input_pool;
	}
	if (info.output_room >= 1 &&
	    info.output_room <= client->pool->size) {
		client->pool->room  = info.output_room;
	}

	return snd_seq_ioctl_get_client_pool(client, arg);
}


/* REMOVE_EVENTS ioctl() */
static int snd_seq_ioctl_remove_events(struct snd_seq_client *client,
				       void __user *arg)
{
	struct snd_seq_remove_events info;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	if (info.remove_mode & SNDRV_SEQ_REMOVE_INPUT) {
		/*
		 * No restrictions so for a user client we can clear
		 * the whole fifo
		 */
		if (client->type == USER_CLIENT)
			snd_seq_fifo_clear(client->data.user.fifo);
	}

	if (info.remove_mode & SNDRV_SEQ_REMOVE_OUTPUT)
		snd_seq_queue_remove_cells(client->number, &info);

	return 0;
}


/*
 * get subscription info
 */
static int snd_seq_ioctl_get_subscription(struct snd_seq_client *client,
					  void __user *arg)
{
	int result;
	struct snd_seq_client *sender = NULL;
	struct snd_seq_client_port *sport = NULL;
	struct snd_seq_port_subscribe subs;
	struct snd_seq_subscribers *p;

	if (copy_from_user(&subs, arg, sizeof(subs)))
		return -EFAULT;

	result = -EINVAL;
	if ((sender = snd_seq_client_use_ptr(subs.sender.client)) == NULL)
		goto __end;
	if ((sport = snd_seq_port_use_ptr(sender, subs.sender.port)) == NULL)
		goto __end;
	p = snd_seq_port_get_subscription(&sport->c_src, &subs.dest);
	if (p) {
		result = 0;
		subs = p->info;
	} else
		result = -ENOENT;

      __end:
      	if (sport)
		snd_seq_port_unlock(sport);
	if (sender)
		snd_seq_client_unlock(sender);
	if (result >= 0) {
		if (copy_to_user(arg, &subs, sizeof(subs)))
			return -EFAULT;
	}
	return result;
}


/*
 * get subscription info - check only its presence
 */
static int snd_seq_ioctl_query_subs(struct snd_seq_client *client,
				    void __user *arg)
{
	int result = -ENXIO;
	struct snd_seq_client *cptr = NULL;
	struct snd_seq_client_port *port = NULL;
	struct snd_seq_query_subs subs;
	struct snd_seq_port_subs_info *group;
	struct list_head *p;
	int i;

	if (copy_from_user(&subs, arg, sizeof(subs)))
		return -EFAULT;

	if ((cptr = snd_seq_client_use_ptr(subs.root.client)) == NULL)
		goto __end;
	if ((port = snd_seq_port_use_ptr(cptr, subs.root.port)) == NULL)
		goto __end;

	switch (subs.type) {
	case SNDRV_SEQ_QUERY_SUBS_READ:
		group = &port->c_src;
		break;
	case SNDRV_SEQ_QUERY_SUBS_WRITE:
		group = &port->c_dest;
		break;
	default:
		goto __end;
	}

	down_read(&group->list_mutex);
	/* search for the subscriber */
	subs.num_subs = group->count;
	i = 0;
	result = -ENOENT;
	list_for_each(p, &group->list_head) {
		if (i++ == subs.index) {
			/* found! */
			struct snd_seq_subscribers *s;
			if (subs.type == SNDRV_SEQ_QUERY_SUBS_READ) {
				s = list_entry(p, struct snd_seq_subscribers, src_list);
				subs.addr = s->info.dest;
			} else {
				s = list_entry(p, struct snd_seq_subscribers, dest_list);
				subs.addr = s->info.sender;
			}
			subs.flags = s->info.flags;
			subs.queue = s->info.queue;
			result = 0;
			break;
		}
	}
	up_read(&group->list_mutex);

      __end:
   	if (port)
		snd_seq_port_unlock(port);
	if (cptr)
		snd_seq_client_unlock(cptr);
	if (result >= 0) {
		if (copy_to_user(arg, &subs, sizeof(subs)))
			return -EFAULT;
	}
	return result;
}


/*
 * query next client
 */
static int snd_seq_ioctl_query_next_client(struct snd_seq_client *client,
					   void __user *arg)
{
	struct snd_seq_client *cptr = NULL;
	struct snd_seq_client_info info;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	/* search for next client */
	info.client++;
	if (info.client < 0)
		info.client = 0;
	for (; info.client < SNDRV_SEQ_MAX_CLIENTS; info.client++) {
		cptr = snd_seq_client_use_ptr(info.client);
		if (cptr)
			break; /* found */
	}
	if (cptr == NULL)
		return -ENOENT;

	get_client_info(cptr, &info);
	snd_seq_client_unlock(cptr);

	if (copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;
	return 0;
}

/* 
 * query next port
 */
static int snd_seq_ioctl_query_next_port(struct snd_seq_client *client,
					 void __user *arg)
{
	struct snd_seq_client *cptr;
	struct snd_seq_client_port *port = NULL;
	struct snd_seq_port_info info;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;
	cptr = snd_seq_client_use_ptr(info.addr.client);
	if (cptr == NULL)
		return -ENXIO;

	/* search for next port */
	info.addr.port++;
	port = snd_seq_port_query_nearest(cptr, &info);
	if (port == NULL) {
		snd_seq_client_unlock(cptr);
		return -ENOENT;
	}

	/* get port info */
	info.addr = port->addr;
	snd_seq_get_port_info(port, &info);
	snd_seq_port_unlock(port);
	snd_seq_client_unlock(cptr);

	if (copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;
	return 0;
}

/* -------------------------------------------------------- */

static struct seq_ioctl_table {
	unsigned int cmd;
	int (*func)(struct snd_seq_client *client, void __user * arg);
} ioctl_tables[] = {
	{ SNDRV_SEQ_IOCTL_SYSTEM_INFO, snd_seq_ioctl_system_info },
	{ SNDRV_SEQ_IOCTL_RUNNING_MODE, snd_seq_ioctl_running_mode },
	{ SNDRV_SEQ_IOCTL_GET_CLIENT_INFO, snd_seq_ioctl_get_client_info },
	{ SNDRV_SEQ_IOCTL_SET_CLIENT_INFO, snd_seq_ioctl_set_client_info },
	{ SNDRV_SEQ_IOCTL_CREATE_PORT, snd_seq_ioctl_create_port },
	{ SNDRV_SEQ_IOCTL_DELETE_PORT, snd_seq_ioctl_delete_port },
	{ SNDRV_SEQ_IOCTL_GET_PORT_INFO, snd_seq_ioctl_get_port_info },
	{ SNDRV_SEQ_IOCTL_SET_PORT_INFO, snd_seq_ioctl_set_port_info },
	{ SNDRV_SEQ_IOCTL_SUBSCRIBE_PORT, snd_seq_ioctl_subscribe_port },
	{ SNDRV_SEQ_IOCTL_UNSUBSCRIBE_PORT, snd_seq_ioctl_unsubscribe_port },
	{ SNDRV_SEQ_IOCTL_CREATE_QUEUE, snd_seq_ioctl_create_queue },
	{ SNDRV_SEQ_IOCTL_DELETE_QUEUE, snd_seq_ioctl_delete_queue },
	{ SNDRV_SEQ_IOCTL_GET_QUEUE_INFO, snd_seq_ioctl_get_queue_info },
	{ SNDRV_SEQ_IOCTL_SET_QUEUE_INFO, snd_seq_ioctl_set_queue_info },
	{ SNDRV_SEQ_IOCTL_GET_NAMED_QUEUE, snd_seq_ioctl_get_named_queue },
	{ SNDRV_SEQ_IOCTL_GET_QUEUE_STATUS, snd_seq_ioctl_get_queue_status },
	{ SNDRV_SEQ_IOCTL_GET_QUEUE_TEMPO, snd_seq_ioctl_get_queue_tempo },
	{ SNDRV_SEQ_IOCTL_SET_QUEUE_TEMPO, snd_seq_ioctl_set_queue_tempo },
	{ SNDRV_SEQ_IOCTL_GET_QUEUE_TIMER, snd_seq_ioctl_get_queue_timer },
	{ SNDRV_SEQ_IOCTL_SET_QUEUE_TIMER, snd_seq_ioctl_set_queue_timer },
	{ SNDRV_SEQ_IOCTL_GET_QUEUE_CLIENT, snd_seq_ioctl_get_queue_client },
	{ SNDRV_SEQ_IOCTL_SET_QUEUE_CLIENT, snd_seq_ioctl_set_queue_client },
	{ SNDRV_SEQ_IOCTL_GET_CLIENT_POOL, snd_seq_ioctl_get_client_pool },
	{ SNDRV_SEQ_IOCTL_SET_CLIENT_POOL, snd_seq_ioctl_set_client_pool },
	{ SNDRV_SEQ_IOCTL_GET_SUBSCRIPTION, snd_seq_ioctl_get_subscription },
	{ SNDRV_SEQ_IOCTL_QUERY_NEXT_CLIENT, snd_seq_ioctl_query_next_client },
	{ SNDRV_SEQ_IOCTL_QUERY_NEXT_PORT, snd_seq_ioctl_query_next_port },
	{ SNDRV_SEQ_IOCTL_REMOVE_EVENTS, snd_seq_ioctl_remove_events },
	{ SNDRV_SEQ_IOCTL_QUERY_SUBS, snd_seq_ioctl_query_subs },
	{ 0, NULL },
};

static int snd_seq_do_ioctl(struct snd_seq_client *client, unsigned int cmd,
			    void __user *arg)
{
	struct seq_ioctl_table *p;

	switch (cmd) {
	case SNDRV_SEQ_IOCTL_PVERSION:
		/* return sequencer version number */
		return put_user(SNDRV_SEQ_VERSION, (int __user *)arg) ? -EFAULT : 0;
	case SNDRV_SEQ_IOCTL_CLIENT_ID:
		/* return the id of this client */
		return put_user(client->number, (int __user *)arg) ? -EFAULT : 0;
	}

	if (! arg)
		return -EFAULT;
	for (p = ioctl_tables; p->cmd; p++) {
		if (p->cmd == cmd)
			return p->func(client, arg);
	}
	snd_printd("seq unknown ioctl() 0x%x (type='%c', number=0x%02x)\n",
		   cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
	return -ENOTTY;
}


static long snd_seq_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct snd_seq_client *client = file->private_data;

	if (snd_BUG_ON(!client))
		return -ENXIO;
		
	return snd_seq_do_ioctl(client, cmd, (void __user *) arg);
}

#ifdef CONFIG_COMPAT
#include "seq_compat.c"
#else
#define snd_seq_ioctl_compat	NULL
#endif

/* -------------------------------------------------------- */


/* exported to kernel modules */
int snd_seq_create_kernel_client(struct snd_card *card, int client_index,
				 const char *name_fmt, ...)
{
	struct snd_seq_client *client;
	va_list args;

	if (snd_BUG_ON(in_interrupt()))
		return -EBUSY;

	if (card && client_index >= SNDRV_SEQ_CLIENTS_PER_CARD)
		return -EINVAL;
	if (card == NULL && client_index >= SNDRV_SEQ_GLOBAL_CLIENTS)
		return -EINVAL;

	if (mutex_lock_interruptible(&register_mutex))
		return -ERESTARTSYS;

	if (card) {
		client_index += SNDRV_SEQ_GLOBAL_CLIENTS
			+ card->number * SNDRV_SEQ_CLIENTS_PER_CARD;
		if (client_index >= SNDRV_SEQ_DYNAMIC_CLIENTS_BEGIN)
			client_index = -1;
	}

	/* empty write queue as default */
	client = seq_create_client1(client_index, 0);
	if (client == NULL) {
		mutex_unlock(&register_mutex);
		return -EBUSY;	/* failure code */
	}
	usage_alloc(&client_usage, 1);

	client->accept_input = 1;
	client->accept_output = 1;
		
	va_start(args, name_fmt);
	vsnprintf(client->name, sizeof(client->name), name_fmt, args);
	va_end(args);

	client->type = KERNEL_CLIENT;
	mutex_unlock(&register_mutex);

	/* make others aware this new client */
	snd_seq_system_client_ev_client_start(client->number);
	
	/* return client number to caller */
	return client->number;
}

EXPORT_SYMBOL(snd_seq_create_kernel_client);

/* exported to kernel modules */
int snd_seq_delete_kernel_client(int client)
{
	struct snd_seq_client *ptr;

	if (snd_BUG_ON(in_interrupt()))
		return -EBUSY;

	ptr = clientptr(client);
	if (ptr == NULL)
		return -EINVAL;

	seq_free_client(ptr);
	kfree(ptr);
	return 0;
}

EXPORT_SYMBOL(snd_seq_delete_kernel_client);

/* skeleton to enqueue event, called from snd_seq_kernel_client_enqueue
 * and snd_seq_kernel_client_enqueue_blocking
 */
static int kernel_client_enqueue(int client, struct snd_seq_event *ev,
				 struct file *file, int blocking,
				 int atomic, int hop)
{
	struct snd_seq_client *cptr;
	int result;

	if (snd_BUG_ON(!ev))
		return -EINVAL;

	if (ev->type == SNDRV_SEQ_EVENT_NONE)
		return 0; /* ignore this */
	if (ev->type == SNDRV_SEQ_EVENT_KERNEL_ERROR)
		return -EINVAL; /* quoted events can't be enqueued */

	/* fill in client number */
	ev->source.client = client;

	if (check_event_type_and_length(ev))
		return -EINVAL;

	cptr = snd_seq_client_use_ptr(client);
	if (cptr == NULL)
		return -EINVAL;
	
	if (! cptr->accept_output)
		result = -EPERM;
	else /* send it */
		result = snd_seq_client_enqueue_event(cptr, ev, file, blocking, atomic, hop);

	snd_seq_client_unlock(cptr);
	return result;
}

/*
 * exported, called by kernel clients to enqueue events (w/o blocking)
 *
 * RETURN VALUE: zero if succeed, negative if error
 */
int snd_seq_kernel_client_enqueue(int client, struct snd_seq_event * ev,
				  int atomic, int hop)
{
	return kernel_client_enqueue(client, ev, NULL, 0, atomic, hop);
}

EXPORT_SYMBOL(snd_seq_kernel_client_enqueue);

/*
 * exported, called by kernel clients to enqueue events (with blocking)
 *
 * RETURN VALUE: zero if succeed, negative if error
 */
int snd_seq_kernel_client_enqueue_blocking(int client, struct snd_seq_event * ev,
					   struct file *file,
					   int atomic, int hop)
{
	return kernel_client_enqueue(client, ev, file, 1, atomic, hop);
}

EXPORT_SYMBOL(snd_seq_kernel_client_enqueue_blocking);

/* 
 * exported, called by kernel clients to dispatch events directly to other
 * clients, bypassing the queues.  Event time-stamp will be updated.
 *
 * RETURN VALUE: negative = delivery failed,
 *		 zero, or positive: the number of delivered events
 */
int snd_seq_kernel_client_dispatch(int client, struct snd_seq_event * ev,
				   int atomic, int hop)
{
	struct snd_seq_client *cptr;
	int result;

	if (snd_BUG_ON(!ev))
		return -EINVAL;

	/* fill in client number */
	ev->queue = SNDRV_SEQ_QUEUE_DIRECT;
	ev->source.client = client;

	if (check_event_type_and_length(ev))
		return -EINVAL;

	cptr = snd_seq_client_use_ptr(client);
	if (cptr == NULL)
		return -EINVAL;

	if (!cptr->accept_output)
		result = -EPERM;
	else
		result = snd_seq_deliver_event(cptr, ev, atomic, hop);

	snd_seq_client_unlock(cptr);
	return result;
}

EXPORT_SYMBOL(snd_seq_kernel_client_dispatch);

/*
 * exported, called by kernel clients to perform same functions as with
 * userland ioctl() 
 */
int snd_seq_kernel_client_ctl(int clientid, unsigned int cmd, void *arg)
{
	struct snd_seq_client *client;
	mm_segment_t fs;
	int result;

	client = clientptr(clientid);
	if (client == NULL)
		return -ENXIO;
	fs = snd_enter_user();
	result = snd_seq_do_ioctl(client, cmd, (void __user *)arg);
	snd_leave_user(fs);
	return result;
}

EXPORT_SYMBOL(snd_seq_kernel_client_ctl);

/* exported (for OSS emulator) */
int snd_seq_kernel_client_write_poll(int clientid, struct file *file, poll_table *wait)
{
	struct snd_seq_client *client;

	client = clientptr(clientid);
	if (client == NULL)
		return -ENXIO;

	if (! snd_seq_write_pool_allocated(client))
		return 1;
	if (snd_seq_pool_poll_wait(client->pool, file, wait))
		return 1;
	return 0;
}

EXPORT_SYMBOL(snd_seq_kernel_client_write_poll);

/*---------------------------------------------------------------------------*/

#ifdef CONFIG_PROC_FS
/*
 *  /proc interface
 */
static void snd_seq_info_dump_subscribers(struct snd_info_buffer *buffer,
					  struct snd_seq_port_subs_info *group,
					  int is_src, char *msg)
{
	struct list_head *p;
	struct snd_seq_subscribers *s;
	int count = 0;

	down_read(&group->list_mutex);
	if (list_empty(&group->list_head)) {
		up_read(&group->list_mutex);
		return;
	}
	snd_iprintf(buffer, msg);
	list_for_each(p, &group->list_head) {
		if (is_src)
			s = list_entry(p, struct snd_seq_subscribers, src_list);
		else
			s = list_entry(p, struct snd_seq_subscribers, dest_list);
		if (count++)
			snd_iprintf(buffer, ", ");
		snd_iprintf(buffer, "%d:%d",
			    is_src ? s->info.dest.client : s->info.sender.client,
			    is_src ? s->info.dest.port : s->info.sender.port);
		if (s->info.flags & SNDRV_SEQ_PORT_SUBS_TIMESTAMP)
			snd_iprintf(buffer, "[%c:%d]", ((s->info.flags & SNDRV_SEQ_PORT_SUBS_TIME_REAL) ? 'r' : 't'), s->info.queue);
		if (group->exclusive)
			snd_iprintf(buffer, "[ex]");
	}
	up_read(&group->list_mutex);
	snd_iprintf(buffer, "\n");
}

#define FLAG_PERM_RD(perm) ((perm) & SNDRV_SEQ_PORT_CAP_READ ? ((perm) & SNDRV_SEQ_PORT_CAP_SUBS_READ ? 'R' : 'r') : '-')
#define FLAG_PERM_WR(perm) ((perm) & SNDRV_SEQ_PORT_CAP_WRITE ? ((perm) & SNDRV_SEQ_PORT_CAP_SUBS_WRITE ? 'W' : 'w') : '-')
#define FLAG_PERM_EX(perm) ((perm) & SNDRV_SEQ_PORT_CAP_NO_EXPORT ? '-' : 'e')

#define FLAG_PERM_DUPLEX(perm) ((perm) & SNDRV_SEQ_PORT_CAP_DUPLEX ? 'X' : '-')

static void snd_seq_info_dump_ports(struct snd_info_buffer *buffer,
				    struct snd_seq_client *client)
{
	struct snd_seq_client_port *p;

	mutex_lock(&client->ports_mutex);
	list_for_each_entry(p, &client->ports_list_head, list) {
		snd_iprintf(buffer, "  Port %3d : \"%s\" (%c%c%c%c)\n",
			    p->addr.port, p->name,
			    FLAG_PERM_RD(p->capability),
			    FLAG_PERM_WR(p->capability),
			    FLAG_PERM_EX(p->capability),
			    FLAG_PERM_DUPLEX(p->capability));
		snd_seq_info_dump_subscribers(buffer, &p->c_src, 1, "    Connecting To: ");
		snd_seq_info_dump_subscribers(buffer, &p->c_dest, 0, "    Connected From: ");
	}
	mutex_unlock(&client->ports_mutex);
}


void snd_seq_info_pool(struct snd_info_buffer *buffer,
		       struct snd_seq_pool *pool, char *space);

/* exported to seq_info.c */
void snd_seq_info_clients_read(struct snd_info_entry *entry, 
			       struct snd_info_buffer *buffer)
{
	int c;
	struct snd_seq_client *client;

	snd_iprintf(buffer, "Client info\n");
	snd_iprintf(buffer, "  cur  clients : %d\n", client_usage.cur);
	snd_iprintf(buffer, "  peak clients : %d\n", client_usage.peak);
	snd_iprintf(buffer, "  max  clients : %d\n", SNDRV_SEQ_MAX_CLIENTS);
	snd_iprintf(buffer, "\n");

	/* list the client table */
	for (c = 0; c < SNDRV_SEQ_MAX_CLIENTS; c++) {
		client = snd_seq_client_use_ptr(c);
		if (client == NULL)
			continue;
		if (client->type == NO_CLIENT) {
			snd_seq_client_unlock(client);
			continue;
		}

		snd_iprintf(buffer, "Client %3d : \"%s\" [%s]\n",
			    c, client->name,
			    client->type == USER_CLIENT ? "User" : "Kernel");
		snd_seq_info_dump_ports(buffer, client);
		if (snd_seq_write_pool_allocated(client)) {
			snd_iprintf(buffer, "  Output pool :\n");
			snd_seq_info_pool(buffer, client->pool, "    ");
		}
		if (client->type == USER_CLIENT && client->data.user.fifo &&
		    client->data.user.fifo->pool) {
			snd_iprintf(buffer, "  Input pool :\n");
			snd_seq_info_pool(buffer, client->data.user.fifo->pool, "    ");
		}
		snd_seq_client_unlock(client);
	}
}
#endif /* CONFIG_PROC_FS */

/*---------------------------------------------------------------------------*/


/*
 *  REGISTRATION PART
 */

static const struct file_operations snd_seq_f_ops =
{
	.owner =	THIS_MODULE,
	.read =		snd_seq_read,
	.write =	snd_seq_write,
	.open =		snd_seq_open,
	.release =	snd_seq_release,
	.llseek =	no_llseek,
	.poll =		snd_seq_poll,
	.unlocked_ioctl =	snd_seq_ioctl,
	.compat_ioctl =	snd_seq_ioctl_compat,
};

/* 
 * register sequencer device 
 */
int __init snd_sequencer_device_init(void)
{
	int err;

	if (mutex_lock_interruptible(&register_mutex))
		return -ERESTARTSYS;

	if ((err = snd_register_device(SNDRV_DEVICE_TYPE_SEQUENCER, NULL, 0,
				       &snd_seq_f_ops, NULL, "seq")) < 0) {
		mutex_unlock(&register_mutex);
		return err;
	}
	
	mutex_unlock(&register_mutex);

	return 0;
}



/* 
 * unregister sequencer device 
 */
void __exit snd_sequencer_device_done(void)
{
	snd_unregister_device(SNDRV_DEVICE_TYPE_SEQUENCER, NULL, 0);
}
