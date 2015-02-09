/*
 * msg_sm.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Implements upper edge functions for Bridge message module.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <linux/types.h>

/*  ----------------------------------- DSP/BIOS Bridge */
#include <dspbridge/dbdefs.h>

/*  ----------------------------------- Trace & Debug */
#include <dspbridge/dbc.h>

/*  ----------------------------------- OS Adaptation Layer */
#include <dspbridge/list.h>
#include <dspbridge/sync.h>

/*  ----------------------------------- Platform Manager */
#include <dspbridge/dev.h>

/*  ----------------------------------- Others */
#include <dspbridge/io_sm.h>

/*  ----------------------------------- This */
#include <_msg_sm.h>
#include <dspbridge/dspmsg.h>

/*  ----------------------------------- Function Prototypes */
static int add_new_msg(struct lst_list *msg_list);
static void delete_msg_mgr(struct msg_mgr *hmsg_mgr);
static void delete_msg_queue(struct msg_queue *msg_queue_obj, u32 num_to_dsp);
static void free_msg_list(struct lst_list *msg_list);

/*
 *  ======== bridge_msg_create ========
 *      Create an object to manage message queues. Only one of these objects
 *      can exist per device object.
 */
int bridge_msg_create(struct msg_mgr **msg_man,
			     struct dev_object *hdev_obj,
			     msg_onexit msg_callback)
{
	struct msg_mgr *msg_mgr_obj;
	struct io_mgr *hio_mgr;
	int status = 0;

	if (!msg_man || !msg_callback || !hdev_obj) {
		status = -EFAULT;
		goto func_end;
	}
	dev_get_io_mgr(hdev_obj, &hio_mgr);
	if (!hio_mgr) {
		status = -EFAULT;
		goto func_end;
	}
	*msg_man = NULL;
	/* Allocate msg_ctrl manager object */
	msg_mgr_obj = kzalloc(sizeof(struct msg_mgr), GFP_KERNEL);

	if (msg_mgr_obj) {
		msg_mgr_obj->on_exit = msg_callback;
		msg_mgr_obj->hio_mgr = hio_mgr;
		/* List of MSG_QUEUEs */
		msg_mgr_obj->queue_list = kzalloc(sizeof(struct lst_list),
							GFP_KERNEL);
		/*  Queues of message frames for messages to the DSP. Message
		 * frames will only be added to the free queue when a
		 * msg_queue object is created. */
		msg_mgr_obj->msg_free_list = kzalloc(sizeof(struct lst_list),
							GFP_KERNEL);
		msg_mgr_obj->msg_used_list = kzalloc(sizeof(struct lst_list),
							GFP_KERNEL);
		if (msg_mgr_obj->queue_list == NULL ||
		    msg_mgr_obj->msg_free_list == NULL ||
		    msg_mgr_obj->msg_used_list == NULL) {
			status = -ENOMEM;
		} else {
			INIT_LIST_HEAD(&msg_mgr_obj->queue_list->head);
			INIT_LIST_HEAD(&msg_mgr_obj->msg_free_list->head);
			INIT_LIST_HEAD(&msg_mgr_obj->msg_used_list->head);
			spin_lock_init(&msg_mgr_obj->msg_mgr_lock);
		}

		/*  Create an event to be used by bridge_msg_put() in waiting
		 *  for an available free frame from the message manager. */
		msg_mgr_obj->sync_event =
				kzalloc(sizeof(struct sync_object), GFP_KERNEL);
		if (!msg_mgr_obj->sync_event)
			status = -ENOMEM;
		else
			sync_init_event(msg_mgr_obj->sync_event);

		if (!status)
			*msg_man = msg_mgr_obj;
		else
			delete_msg_mgr(msg_mgr_obj);

	} else {
		status = -ENOMEM;
	}
func_end:
	return status;
}

/*
 *  ======== bridge_msg_create_queue ========
 *      Create a msg_queue for sending/receiving messages to/from a node
 *      on the DSP.
 */
int bridge_msg_create_queue(struct msg_mgr *hmsg_mgr,
				struct msg_queue **msgq,
				u32 msgq_id, u32 max_msgs, void *arg)
{
	u32 i;
	u32 num_allocated = 0;
	struct msg_queue *msg_q;
	int status = 0;

	if (!hmsg_mgr || msgq == NULL || !hmsg_mgr->msg_free_list) {
		status = -EFAULT;
		goto func_end;
	}

	*msgq = NULL;
	/* Allocate msg_queue object */
	msg_q = kzalloc(sizeof(struct msg_queue), GFP_KERNEL);
	if (!msg_q) {
		status = -ENOMEM;
		goto func_end;
	}
	lst_init_elem((struct list_head *)msg_q);
	msg_q->max_msgs = max_msgs;
	msg_q->hmsg_mgr = hmsg_mgr;
	msg_q->arg = arg;	/* Node handle */
	msg_q->msgq_id = msgq_id;	/* Node env (not valid yet) */
	/* Queues of Message frames for messages from the DSP */
	msg_q->msg_free_list = kzalloc(sizeof(struct lst_list), GFP_KERNEL);
	msg_q->msg_used_list = kzalloc(sizeof(struct lst_list), GFP_KERNEL);
	if (msg_q->msg_free_list == NULL || msg_q->msg_used_list == NULL)
		status = -ENOMEM;
	else {
		INIT_LIST_HEAD(&msg_q->msg_free_list->head);
		INIT_LIST_HEAD(&msg_q->msg_used_list->head);
	}

	/*  Create event that will be signalled when a message from
	 *  the DSP is available. */
	if (!status) {
		msg_q->sync_event = kzalloc(sizeof(struct sync_object),
							GFP_KERNEL);
		if (msg_q->sync_event)
			sync_init_event(msg_q->sync_event);
		else
			status = -ENOMEM;
	}

	/* Create a notification list for message ready notification. */
	if (!status) {
		msg_q->ntfy_obj = kmalloc(sizeof(struct ntfy_object),
							GFP_KERNEL);
		if (msg_q->ntfy_obj)
			ntfy_init(msg_q->ntfy_obj);
		else
			status = -ENOMEM;
	}

	/*  Create events that will be used to synchronize cleanup
	 *  when the object is deleted. sync_done will be set to
	 *  unblock threads in MSG_Put() or MSG_Get(). sync_done_ack
	 *  will be set by the unblocked thread to signal that it
	 *  is unblocked and will no longer reference the object. */
	if (!status) {
		msg_q->sync_done = kzalloc(sizeof(struct sync_object),
							GFP_KERNEL);
		if (msg_q->sync_done)
			sync_init_event(msg_q->sync_done);
		else
			status = -ENOMEM;
	}

	if (!status) {
		msg_q->sync_done_ack = kzalloc(sizeof(struct sync_object),
							GFP_KERNEL);
		if (msg_q->sync_done_ack)
			sync_init_event(msg_q->sync_done_ack);
		else
			status = -ENOMEM;
	}

	if (!status) {
		/* Enter critical section */
		spin_lock_bh(&hmsg_mgr->msg_mgr_lock);
		/* Initialize message frames and put in appropriate queues */
		for (i = 0; i < max_msgs && !status; i++) {
			status = add_new_msg(hmsg_mgr->msg_free_list);
			if (!status) {
				num_allocated++;
				status = add_new_msg(msg_q->msg_free_list);
			}
		}
		if (status) {
			/*  Stay inside CS to prevent others from taking any
			 *  of the newly allocated message frames. */
			delete_msg_queue(msg_q, num_allocated);
		} else {
			lst_put_tail(hmsg_mgr->queue_list,
				     (struct list_head *)msg_q);
			*msgq = msg_q;
			/* Signal that free frames are now available */
			if (!LST_IS_EMPTY(hmsg_mgr->msg_free_list))
				sync_set_event(hmsg_mgr->sync_event);

		}
		/* Exit critical section */
		spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
	} else {
		delete_msg_queue(msg_q, 0);
	}
func_end:
	return status;
}

/*
 *  ======== bridge_msg_delete ========
 *      Delete a msg_ctrl manager allocated in bridge_msg_create().
 */
void bridge_msg_delete(struct msg_mgr *hmsg_mgr)
{
	if (hmsg_mgr)
		delete_msg_mgr(hmsg_mgr);
}

/*
 *  ======== bridge_msg_delete_queue ========
 *      Delete a msg_ctrl queue allocated in bridge_msg_create_queue.
 */
void bridge_msg_delete_queue(struct msg_queue *msg_queue_obj)
{
	struct msg_mgr *hmsg_mgr;
	u32 io_msg_pend;

	if (!msg_queue_obj || !msg_queue_obj->hmsg_mgr)
		goto func_end;

	hmsg_mgr = msg_queue_obj->hmsg_mgr;
	msg_queue_obj->done = true;
	/*  Unblock all threads blocked in MSG_Get() or MSG_Put(). */
	io_msg_pend = msg_queue_obj->io_msg_pend;
	while (io_msg_pend) {
		/* Unblock thread */
		sync_set_event(msg_queue_obj->sync_done);
		/* Wait for acknowledgement */
		sync_wait_on_event(msg_queue_obj->sync_done_ack, SYNC_INFINITE);
		io_msg_pend = msg_queue_obj->io_msg_pend;
	}
	/* Remove message queue from hmsg_mgr->queue_list */
	spin_lock_bh(&hmsg_mgr->msg_mgr_lock);
	lst_remove_elem(hmsg_mgr->queue_list,
			(struct list_head *)msg_queue_obj);
	/* Free the message queue object */
	delete_msg_queue(msg_queue_obj, msg_queue_obj->max_msgs);
	if (!hmsg_mgr->msg_free_list)
		goto func_cont;
	if (LST_IS_EMPTY(hmsg_mgr->msg_free_list))
		sync_reset_event(hmsg_mgr->sync_event);
func_cont:
	spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
func_end:
	return;
}

/*
 *  ======== bridge_msg_get ========
 *      Get a message from a msg_ctrl queue.
 */
int bridge_msg_get(struct msg_queue *msg_queue_obj,
			  struct dsp_msg *pmsg, u32 utimeout)
{
	struct msg_frame *msg_frame_obj;
	struct msg_mgr *hmsg_mgr;
	bool got_msg = false;
	struct sync_object *syncs[2];
	u32 index;
	int status = 0;

	if (!msg_queue_obj || pmsg == NULL) {
		status = -ENOMEM;
		goto func_end;
	}

	hmsg_mgr = msg_queue_obj->hmsg_mgr;
	if (!msg_queue_obj->msg_used_list) {
		status = -EFAULT;
		goto func_end;
	}

	/* Enter critical section */
	spin_lock_bh(&hmsg_mgr->msg_mgr_lock);
	/* If a message is already there, get it */
	if (!LST_IS_EMPTY(msg_queue_obj->msg_used_list)) {
		msg_frame_obj = (struct msg_frame *)
		    lst_get_head(msg_queue_obj->msg_used_list);
		if (msg_frame_obj != NULL) {
			*pmsg = msg_frame_obj->msg_data.msg;
			lst_put_tail(msg_queue_obj->msg_free_list,
				     (struct list_head *)msg_frame_obj);
			if (LST_IS_EMPTY(msg_queue_obj->msg_used_list))
				sync_reset_event(msg_queue_obj->sync_event);

			got_msg = true;
		}
	} else {
		if (msg_queue_obj->done)
			status = -EPERM;
		else
			msg_queue_obj->io_msg_pend++;

	}
	/* Exit critical section */
	spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
	if (!status && !got_msg) {
		/*  Wait til message is available, timeout, or done. We don't
		 *  have to schedule the DPC, since the DSP will send messages
		 *  when they are available. */
		syncs[0] = msg_queue_obj->sync_event;
		syncs[1] = msg_queue_obj->sync_done;
		status = sync_wait_on_multiple_events(syncs, 2, utimeout,
						      &index);
		/* Enter critical section */
		spin_lock_bh(&hmsg_mgr->msg_mgr_lock);
		if (msg_queue_obj->done) {
			msg_queue_obj->io_msg_pend--;
			/* Exit critical section */
			spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
			/*  Signal that we're not going to access msg_queue_obj
			 *  anymore, so it can be deleted. */
			(void)sync_set_event(msg_queue_obj->sync_done_ack);
			status = -EPERM;
		} else {
			if (!status) {
				DBC_ASSERT(!LST_IS_EMPTY
					   (msg_queue_obj->msg_used_list));
				/* Get msg from used list */
				msg_frame_obj = (struct msg_frame *)
				    lst_get_head(msg_queue_obj->msg_used_list);
				/* Copy message into pmsg and put frame on the
				 * free list */
				if (msg_frame_obj != NULL) {
					*pmsg = msg_frame_obj->msg_data.msg;
					lst_put_tail
					    (msg_queue_obj->msg_free_list,
					     (struct list_head *)
					     msg_frame_obj);
				}
			}
			msg_queue_obj->io_msg_pend--;
			/* Reset the event if there are still queued messages */
			if (!LST_IS_EMPTY(msg_queue_obj->msg_used_list))
				sync_set_event(msg_queue_obj->sync_event);

			/* Exit critical section */
			spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
		}
	}
func_end:
	return status;
}

/*
 *  ======== bridge_msg_put ========
 *      Put a message onto a msg_ctrl queue.
 */
int bridge_msg_put(struct msg_queue *msg_queue_obj,
			  const struct dsp_msg *pmsg, u32 utimeout)
{
	struct msg_frame *msg_frame_obj;
	struct msg_mgr *hmsg_mgr;
	bool put_msg = false;
	struct sync_object *syncs[2];
	u32 index;
	int status = 0;

	if (!msg_queue_obj || !pmsg || !msg_queue_obj->hmsg_mgr) {
		status = -ENOMEM;
		goto func_end;
	}
	hmsg_mgr = msg_queue_obj->hmsg_mgr;
	if (!hmsg_mgr->msg_free_list) {
		status = -EFAULT;
		goto func_end;
	}

	spin_lock_bh(&hmsg_mgr->msg_mgr_lock);

	/* If a message frame is available, use it */
	if (!LST_IS_EMPTY(hmsg_mgr->msg_free_list)) {
		msg_frame_obj =
		    (struct msg_frame *)lst_get_head(hmsg_mgr->msg_free_list);
		if (msg_frame_obj != NULL) {
			msg_frame_obj->msg_data.msg = *pmsg;
			msg_frame_obj->msg_data.msgq_id =
			    msg_queue_obj->msgq_id;
			lst_put_tail(hmsg_mgr->msg_used_list,
				     (struct list_head *)msg_frame_obj);
			hmsg_mgr->msgs_pending++;
			put_msg = true;
		}
		if (LST_IS_EMPTY(hmsg_mgr->msg_free_list))
			sync_reset_event(hmsg_mgr->sync_event);

		/* Release critical section before scheduling DPC */
		spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
		/* Schedule a DPC, to do the actual data transfer: */
		iosm_schedule(hmsg_mgr->hio_mgr);
	} else {
		if (msg_queue_obj->done)
			status = -EPERM;
		else
			msg_queue_obj->io_msg_pend++;

		spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
	}
	if (!status && !put_msg) {
		/* Wait til a free message frame is available, timeout,
		 * or done */
		syncs[0] = hmsg_mgr->sync_event;
		syncs[1] = msg_queue_obj->sync_done;
		status = sync_wait_on_multiple_events(syncs, 2, utimeout,
						      &index);
		if (status)
			goto func_end;
		/* Enter critical section */
		spin_lock_bh(&hmsg_mgr->msg_mgr_lock);
		if (msg_queue_obj->done) {
			msg_queue_obj->io_msg_pend--;
			/* Exit critical section */
			spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
			/*  Signal that we're not going to access msg_queue_obj
			 *  anymore, so it can be deleted. */
			(void)sync_set_event(msg_queue_obj->sync_done_ack);
			status = -EPERM;
		} else {
			if (LST_IS_EMPTY(hmsg_mgr->msg_free_list)) {
				status = -EFAULT;
				goto func_cont;
			}
			/* Get msg from free list */
			msg_frame_obj = (struct msg_frame *)
			    lst_get_head(hmsg_mgr->msg_free_list);
			/*
			 * Copy message into pmsg and put frame on the
			 * used list.
			 */
			if (msg_frame_obj) {
				msg_frame_obj->msg_data.msg = *pmsg;
				msg_frame_obj->msg_data.msgq_id =
				    msg_queue_obj->msgq_id;
				lst_put_tail(hmsg_mgr->msg_used_list,
					     (struct list_head *)msg_frame_obj);
				hmsg_mgr->msgs_pending++;
				/*
				 * Schedule a DPC, to do the actual
				 * data transfer.
				 */
				iosm_schedule(hmsg_mgr->hio_mgr);
			}

			msg_queue_obj->io_msg_pend--;
			/* Reset event if there are still frames available */
			if (!LST_IS_EMPTY(hmsg_mgr->msg_free_list))
				sync_set_event(hmsg_mgr->sync_event);
func_cont:
			/* Exit critical section */
			spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
		}
	}
func_end:
	return status;
}

/*
 *  ======== bridge_msg_register_notify ========
 */
int bridge_msg_register_notify(struct msg_queue *msg_queue_obj,
				   u32 event_mask, u32 notify_type,
				   struct dsp_notification *hnotification)
{
	int status = 0;

	if (!msg_queue_obj || !hnotification) {
		status = -ENOMEM;
		goto func_end;
	}

	if (!(event_mask == DSP_NODEMESSAGEREADY || event_mask == 0)) {
		status = -EPERM;
		goto func_end;
	}

	if (notify_type != DSP_SIGNALEVENT) {
		status = -EBADR;
		goto func_end;
	}

	if (event_mask)
		status = ntfy_register(msg_queue_obj->ntfy_obj, hnotification,
						event_mask, notify_type);
	else
		status = ntfy_unregister(msg_queue_obj->ntfy_obj,
							hnotification);

	if (status == -EINVAL) {
		/*  Not registered. Ok, since we couldn't have known. Node
		 *  notifications are split between node state change handled
		 *  by NODE, and message ready handled by msg_ctrl. */
		status = 0;
	}
func_end:
	return status;
}

/*
 *  ======== bridge_msg_set_queue_id ========
 */
void bridge_msg_set_queue_id(struct msg_queue *msg_queue_obj, u32 msgq_id)
{
	/*
	 *  A message queue must be created when a node is allocated,
	 *  so that node_register_notify() can be called before the node
	 *  is created. Since we don't know the node environment until the
	 *  node is created, we need this function to set msg_queue_obj->msgq_id
	 *  to the node environment, after the node is created.
	 */
	if (msg_queue_obj)
		msg_queue_obj->msgq_id = msgq_id;
}

/*
 *  ======== add_new_msg ========
 *      Must be called in message manager critical section.
 */
static int add_new_msg(struct lst_list *msg_list)
{
	struct msg_frame *pmsg;
	int status = 0;

	pmsg = kzalloc(sizeof(struct msg_frame), GFP_ATOMIC);
	if (pmsg != NULL) {
		lst_init_elem((struct list_head *)pmsg);
		lst_put_tail(msg_list, (struct list_head *)pmsg);
	} else {
		status = -ENOMEM;
	}

	return status;
}

/*
 *  ======== delete_msg_mgr ========
 */
static void delete_msg_mgr(struct msg_mgr *hmsg_mgr)
{
	if (!hmsg_mgr)
		goto func_end;

	if (hmsg_mgr->queue_list) {
		if (LST_IS_EMPTY(hmsg_mgr->queue_list)) {
			kfree(hmsg_mgr->queue_list);
			hmsg_mgr->queue_list = NULL;
		}
	}

	if (hmsg_mgr->msg_free_list) {
		free_msg_list(hmsg_mgr->msg_free_list);
		hmsg_mgr->msg_free_list = NULL;
	}

	if (hmsg_mgr->msg_used_list) {
		free_msg_list(hmsg_mgr->msg_used_list);
		hmsg_mgr->msg_used_list = NULL;
	}

	kfree(hmsg_mgr->sync_event);

	kfree(hmsg_mgr);
func_end:
	return;
}

/*
 *  ======== delete_msg_queue ========
 */
static void delete_msg_queue(struct msg_queue *msg_queue_obj, u32 num_to_dsp)
{
	struct msg_mgr *hmsg_mgr;
	struct msg_frame *pmsg;
	u32 i;

	if (!msg_queue_obj ||
	    !msg_queue_obj->hmsg_mgr || !msg_queue_obj->hmsg_mgr->msg_free_list)
		goto func_end;

	hmsg_mgr = msg_queue_obj->hmsg_mgr;

	/* Pull off num_to_dsp message frames from Msg manager and free */
	for (i = 0; i < num_to_dsp; i++) {

		if (!LST_IS_EMPTY(hmsg_mgr->msg_free_list)) {
			pmsg = (struct msg_frame *)
			    lst_get_head(hmsg_mgr->msg_free_list);
			kfree(pmsg);
		} else {
			/* Cannot free all of the message frames */
			break;
		}
	}

	if (msg_queue_obj->msg_free_list) {
		free_msg_list(msg_queue_obj->msg_free_list);
		msg_queue_obj->msg_free_list = NULL;
	}

	if (msg_queue_obj->msg_used_list) {
		free_msg_list(msg_queue_obj->msg_used_list);
		msg_queue_obj->msg_used_list = NULL;
	}

	if (msg_queue_obj->ntfy_obj) {
		ntfy_delete(msg_queue_obj->ntfy_obj);
		kfree(msg_queue_obj->ntfy_obj);
	}

	kfree(msg_queue_obj->sync_event);
	kfree(msg_queue_obj->sync_done);
	kfree(msg_queue_obj->sync_done_ack);

	kfree(msg_queue_obj);
func_end:
	return;

}

/*
 *  ======== free_msg_list ========
 */
static void free_msg_list(struct lst_list *msg_list)
{
	struct msg_frame *pmsg;

	if (!msg_list)
		goto func_end;

	while ((pmsg = (struct msg_frame *)lst_get_head(msg_list)) != NULL)
		kfree(pmsg);

	DBC_ASSERT(LST_IS_EMPTY(msg_list));

	kfree(msg_list);
func_end:
	return;
}
