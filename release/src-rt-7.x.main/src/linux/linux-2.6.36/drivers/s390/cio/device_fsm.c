/*
 * drivers/s390/cio/device_fsm.c
 * finite state machine for device handling
 *
 *    Copyright IBM Corp. 2002,2008
 *    Author(s): Cornelia Huck (cornelia.huck@de.ibm.com)
 *		 Martin Schwidefsky (schwidefsky@de.ibm.com)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/string.h>

#include <asm/ccwdev.h>
#include <asm/cio.h>
#include <asm/chpid.h>

#include "cio.h"
#include "cio_debug.h"
#include "css.h"
#include "device.h"
#include "chsc.h"
#include "ioasm.h"
#include "chp.h"

static int timeout_log_enabled;

static int __init ccw_timeout_log_setup(char *unused)
{
	timeout_log_enabled = 1;
	return 1;
}

__setup("ccw_timeout_log", ccw_timeout_log_setup);

static void ccw_timeout_log(struct ccw_device *cdev)
{
	struct schib schib;
	struct subchannel *sch;
	struct io_subchannel_private *private;
	union orb *orb;
	int cc;

	sch = to_subchannel(cdev->dev.parent);
	private = to_io_private(sch);
	orb = &private->orb;
	cc = stsch_err(sch->schid, &schib);

	printk(KERN_WARNING "cio: ccw device timeout occurred at %llx, "
	       "device information:\n", get_clock());
	printk(KERN_WARNING "cio: orb:\n");
	print_hex_dump(KERN_WARNING, "cio:  ", DUMP_PREFIX_NONE, 16, 1,
		       orb, sizeof(*orb), 0);
	printk(KERN_WARNING "cio: ccw device bus id: %s\n",
	       dev_name(&cdev->dev));
	printk(KERN_WARNING "cio: subchannel bus id: %s\n",
	       dev_name(&sch->dev));
	printk(KERN_WARNING "cio: subchannel lpm: %02x, opm: %02x, "
	       "vpm: %02x\n", sch->lpm, sch->opm, sch->vpm);

	if (orb->tm.b) {
		printk(KERN_WARNING "cio: orb indicates transport mode\n");
		printk(KERN_WARNING "cio: last tcw:\n");
		print_hex_dump(KERN_WARNING, "cio:  ", DUMP_PREFIX_NONE, 16, 1,
			       (void *)(addr_t)orb->tm.tcw,
			       sizeof(struct tcw), 0);
	} else {
		printk(KERN_WARNING "cio: orb indicates command mode\n");
		if ((void *)(addr_t)orb->cmd.cpa == &private->sense_ccw ||
		    (void *)(addr_t)orb->cmd.cpa == cdev->private->iccws)
			printk(KERN_WARNING "cio: last channel program "
			       "(intern):\n");
		else
			printk(KERN_WARNING "cio: last channel program:\n");

		print_hex_dump(KERN_WARNING, "cio:  ", DUMP_PREFIX_NONE, 16, 1,
			       (void *)(addr_t)orb->cmd.cpa,
			       sizeof(struct ccw1), 0);
	}
	printk(KERN_WARNING "cio: ccw device state: %d\n",
	       cdev->private->state);
	printk(KERN_WARNING "cio: store subchannel returned: cc=%d\n", cc);
	printk(KERN_WARNING "cio: schib:\n");
	print_hex_dump(KERN_WARNING, "cio:  ", DUMP_PREFIX_NONE, 16, 1,
		       &schib, sizeof(schib), 0);
	printk(KERN_WARNING "cio: ccw device flags:\n");
	print_hex_dump(KERN_WARNING, "cio:  ", DUMP_PREFIX_NONE, 16, 1,
		       &cdev->private->flags, sizeof(cdev->private->flags), 0);
}

/*
 * Timeout function. It just triggers a DEV_EVENT_TIMEOUT.
 */
static void
ccw_device_timeout(unsigned long data)
{
	struct ccw_device *cdev;

	cdev = (struct ccw_device *) data;
	spin_lock_irq(cdev->ccwlock);
	if (timeout_log_enabled)
		ccw_timeout_log(cdev);
	dev_fsm_event(cdev, DEV_EVENT_TIMEOUT);
	spin_unlock_irq(cdev->ccwlock);
}

/*
 * Set timeout
 */
void
ccw_device_set_timeout(struct ccw_device *cdev, int expires)
{
	if (expires == 0) {
		del_timer(&cdev->private->timer);
		return;
	}
	if (timer_pending(&cdev->private->timer)) {
		if (mod_timer(&cdev->private->timer, jiffies + expires))
			return;
	}
	cdev->private->timer.function = ccw_device_timeout;
	cdev->private->timer.data = (unsigned long) cdev;
	cdev->private->timer.expires = jiffies + expires;
	add_timer(&cdev->private->timer);
}

/*
 * Cancel running i/o. This is called repeatedly since halt/clear are
 * asynchronous operations. We do one try with cio_cancel, two tries
 * with cio_halt, 255 tries with cio_clear. If everythings fails panic.
 * Returns 0 if device now idle, -ENODEV for device not operational and
 * -EBUSY if an interrupt is expected (either from halt/clear or from a
 * status pending).
 */
int
ccw_device_cancel_halt_clear(struct ccw_device *cdev)
{
	struct subchannel *sch;
	int ret;

	sch = to_subchannel(cdev->dev.parent);
	if (cio_update_schib(sch))
		return -ENODEV; 
	if (!sch->schib.pmcw.ena)
		/* Not operational -> done. */
		return 0;
	/* Stage 1: cancel io. */
	if (!(scsw_actl(&sch->schib.scsw) & SCSW_ACTL_HALT_PEND) &&
	    !(scsw_actl(&sch->schib.scsw) & SCSW_ACTL_CLEAR_PEND)) {
		if (!scsw_is_tm(&sch->schib.scsw)) {
			ret = cio_cancel(sch);
			if (ret != -EINVAL)
				return ret;
		}
		/* cancel io unsuccessful or not applicable (transport mode).
		 * Continue with asynchronous instructions. */
		cdev->private->iretry = 3;	/* 3 halt retries. */
	}
	if (!(scsw_actl(&sch->schib.scsw) & SCSW_ACTL_CLEAR_PEND)) {
		/* Stage 2: halt io. */
		if (cdev->private->iretry) {
			cdev->private->iretry--;
			ret = cio_halt(sch);
			if (ret != -EBUSY)
				return (ret == 0) ? -EBUSY : ret;
		}
		/* halt io unsuccessful. */
		cdev->private->iretry = 255;	/* 255 clear retries. */
	}
	/* Stage 3: clear io. */
	if (cdev->private->iretry) {
		cdev->private->iretry--;
		ret = cio_clear (sch);
		return (ret == 0) ? -EBUSY : ret;
	}
	panic("Can't stop i/o on subchannel.\n");
}

void ccw_device_update_sense_data(struct ccw_device *cdev)
{
	memset(&cdev->id, 0, sizeof(cdev->id));
	cdev->id.cu_type   = cdev->private->senseid.cu_type;
	cdev->id.cu_model  = cdev->private->senseid.cu_model;
	cdev->id.dev_type  = cdev->private->senseid.dev_type;
	cdev->id.dev_model = cdev->private->senseid.dev_model;
}

int ccw_device_test_sense_data(struct ccw_device *cdev)
{
	return cdev->id.cu_type == cdev->private->senseid.cu_type &&
		cdev->id.cu_model == cdev->private->senseid.cu_model &&
		cdev->id.dev_type == cdev->private->senseid.dev_type &&
		cdev->id.dev_model == cdev->private->senseid.dev_model;
}

/*
 * The machine won't give us any notification by machine check if a chpid has
 * been varied online on the SE so we have to find out by magic (i. e. driving
 * the channel subsystem to device selection and updating our path masks).
 */
static void
__recover_lost_chpids(struct subchannel *sch, int old_lpm)
{
	int mask, i;
	struct chp_id chpid;

	chp_id_init(&chpid);
	for (i = 0; i<8; i++) {
		mask = 0x80 >> i;
		if (!(sch->lpm & mask))
			continue;
		if (old_lpm & mask)
			continue;
		chpid.id = sch->schib.pmcw.chpid[i];
		if (!chp_is_registered(chpid))
			css_schedule_eval_all();
	}
}

/*
 * Stop device recognition.
 */
static void
ccw_device_recog_done(struct ccw_device *cdev, int state)
{
	struct subchannel *sch;
	int old_lpm;

	sch = to_subchannel(cdev->dev.parent);

	if (cio_disable_subchannel(sch))
		state = DEV_STATE_NOT_OPER;
	/*
	 * Now that we tried recognition, we have performed device selection
	 * through ssch() and the path information is up to date.
	 */
	old_lpm = sch->lpm;

	/* Check since device may again have become not operational. */
	if (cio_update_schib(sch))
		state = DEV_STATE_NOT_OPER;
	else
		sch->lpm = sch->schib.pmcw.pam & sch->opm;

	if (cdev->private->state == DEV_STATE_DISCONNECTED_SENSE_ID)
		/* Force reprobe on all chpids. */
		old_lpm = 0;
	if (sch->lpm != old_lpm)
		__recover_lost_chpids(sch, old_lpm);
	if (cdev->private->state == DEV_STATE_DISCONNECTED_SENSE_ID &&
	    (state == DEV_STATE_NOT_OPER || state == DEV_STATE_BOXED)) {
		cdev->private->flags.recog_done = 1;
		cdev->private->state = DEV_STATE_DISCONNECTED;
		wake_up(&cdev->private->wait_q);
		return;
	}
	if (cdev->private->flags.resuming) {
		cdev->private->state = state;
		cdev->private->flags.recog_done = 1;
		wake_up(&cdev->private->wait_q);
		return;
	}
	switch (state) {
	case DEV_STATE_NOT_OPER:
		break;
	case DEV_STATE_OFFLINE:
		if (!cdev->online) {
			ccw_device_update_sense_data(cdev);
			break;
		}
		cdev->private->state = DEV_STATE_OFFLINE;
		cdev->private->flags.recog_done = 1;
		if (ccw_device_test_sense_data(cdev)) {
			cdev->private->flags.donotify = 1;
			ccw_device_online(cdev);
			wake_up(&cdev->private->wait_q);
		} else {
			ccw_device_update_sense_data(cdev);
			ccw_device_sched_todo(cdev, CDEV_TODO_REBIND);
		}
		return;
	case DEV_STATE_BOXED:
		if (cdev->id.cu_type != 0) { /* device was recognized before */
			cdev->private->flags.recog_done = 1;
			cdev->private->state = DEV_STATE_BOXED;
			wake_up(&cdev->private->wait_q);
			return;
		}
		break;
	}
	cdev->private->state = state;
	io_subchannel_recog_done(cdev);
	wake_up(&cdev->private->wait_q);
}

/*
 * Function called from device_id.c after sense id has completed.
 */
void
ccw_device_sense_id_done(struct ccw_device *cdev, int err)
{
	switch (err) {
	case 0:
		ccw_device_recog_done(cdev, DEV_STATE_OFFLINE);
		break;
	case -ETIME:		/* Sense id stopped by timeout. */
		ccw_device_recog_done(cdev, DEV_STATE_BOXED);
		break;
	default:
		ccw_device_recog_done(cdev, DEV_STATE_NOT_OPER);
		break;
	}
}

/**
  * ccw_device_notify() - inform the device's driver about an event
  * @cdev: device for which an event occured
  * @event: event that occurred
  *
  * Returns:
  *   -%EINVAL if the device is offline or has no driver.
  *   -%EOPNOTSUPP if the device's driver has no notifier registered.
  *   %NOTIFY_OK if the driver wants to keep the device.
  *   %NOTIFY_BAD if the driver doesn't want to keep the device.
  */
int ccw_device_notify(struct ccw_device *cdev, int event)
{
	int ret = -EINVAL;

	if (!cdev->drv)
		goto out;
	if (!cdev->online)
		goto out;
	CIO_MSG_EVENT(2, "notify called for 0.%x.%04x, event=%d\n",
		      cdev->private->dev_id.ssid, cdev->private->dev_id.devno,
		      event);
	if (!cdev->drv->notify) {
		ret = -EOPNOTSUPP;
		goto out;
	}
	if (cdev->drv->notify(cdev, event))
		ret = NOTIFY_OK;
	else
		ret = NOTIFY_BAD;
out:
	return ret;
}

static void ccw_device_oper_notify(struct ccw_device *cdev)
{
	if (ccw_device_notify(cdev, CIO_OPER) == NOTIFY_OK) {
		/* Reenable channel measurements, if needed. */
		ccw_device_sched_todo(cdev, CDEV_TODO_ENABLE_CMF);
		return;
	}
	/* Driver doesn't want device back. */
	ccw_device_set_notoper(cdev);
	ccw_device_sched_todo(cdev, CDEV_TODO_REBIND);
}

/*
 * Finished with online/offline processing.
 */
static void
ccw_device_done(struct ccw_device *cdev, int state)
{
	struct subchannel *sch;

	sch = to_subchannel(cdev->dev.parent);

	ccw_device_set_timeout(cdev, 0);

	if (state != DEV_STATE_ONLINE)
		cio_disable_subchannel(sch);

	/* Reset device status. */
	memset(&cdev->private->irb, 0, sizeof(struct irb));

	cdev->private->state = state;

	switch (state) {
	case DEV_STATE_BOXED:
		CIO_MSG_EVENT(0, "Boxed device %04x on subchannel %04x\n",
			      cdev->private->dev_id.devno, sch->schid.sch_no);
		if (cdev->online &&
		    ccw_device_notify(cdev, CIO_BOXED) != NOTIFY_OK)
			ccw_device_sched_todo(cdev, CDEV_TODO_UNREG);
		cdev->private->flags.donotify = 0;
		break;
	case DEV_STATE_NOT_OPER:
		CIO_MSG_EVENT(0, "Device %04x gone on subchannel %04x\n",
			      cdev->private->dev_id.devno, sch->schid.sch_no);
		if (ccw_device_notify(cdev, CIO_GONE) != NOTIFY_OK)
			ccw_device_sched_todo(cdev, CDEV_TODO_UNREG);
		else
			ccw_device_set_disconnected(cdev);
		cdev->private->flags.donotify = 0;
		break;
	case DEV_STATE_DISCONNECTED:
		CIO_MSG_EVENT(0, "Disconnected device %04x on subchannel "
			      "%04x\n", cdev->private->dev_id.devno,
			      sch->schid.sch_no);
		if (ccw_device_notify(cdev, CIO_NO_PATH) != NOTIFY_OK)
			ccw_device_sched_todo(cdev, CDEV_TODO_UNREG);
		else
			ccw_device_set_disconnected(cdev);
		cdev->private->flags.donotify = 0;
		break;
	default:
		break;
	}

	if (cdev->private->flags.donotify) {
		cdev->private->flags.donotify = 0;
		ccw_device_oper_notify(cdev);
	}
	wake_up(&cdev->private->wait_q);
}

/*
 * Start device recognition.
 */
void ccw_device_recognition(struct ccw_device *cdev)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);

	/*
	 * We used to start here with a sense pgid to find out whether a device
	 * is locked by someone else. Unfortunately, the sense pgid command
	 * code has other meanings on devices predating the path grouping
	 * algorithm, so we start with sense id and box the device after an
	 * timeout (or if sense pgid during path verification detects the device
	 * is locked, as may happen on newer devices).
	 */
	cdev->private->flags.recog_done = 0;
	cdev->private->state = DEV_STATE_SENSE_ID;
	if (cio_enable_subchannel(sch, (u32) (addr_t) sch)) {
		ccw_device_recog_done(cdev, DEV_STATE_NOT_OPER);
		return;
	}
	ccw_device_sense_id_start(cdev);
}

/*
 * Handle events for states that use the ccw request infrastructure.
 */
static void ccw_device_request_event(struct ccw_device *cdev, enum dev_event e)
{
	switch (e) {
	case DEV_EVENT_NOTOPER:
		ccw_request_notoper(cdev);
		break;
	case DEV_EVENT_INTERRUPT:
		ccw_request_handler(cdev);
		break;
	case DEV_EVENT_TIMEOUT:
		ccw_request_timeout(cdev);
		break;
	default:
		break;
	}
}

void
ccw_device_verify_done(struct ccw_device *cdev, int err)
{
	struct subchannel *sch;

	sch = to_subchannel(cdev->dev.parent);
	/* Update schib - pom may have changed. */
	if (cio_update_schib(sch)) {
		err = -ENODEV;
		goto callback;
	}
	/* Update lpm with verified path mask. */
	sch->lpm = sch->vpm;
	/* Repeat path verification? */
	if (cdev->private->flags.doverify) {
		ccw_device_verify_start(cdev);
		return;
	}
callback:
	switch (err) {
	case 0:
		ccw_device_done(cdev, DEV_STATE_ONLINE);
		/* Deliver fake irb to device driver, if needed. */
		if (cdev->private->flags.fake_irb) {
			memset(&cdev->private->irb, 0, sizeof(struct irb));
			cdev->private->irb.scsw.cmd.cc = 1;
			cdev->private->irb.scsw.cmd.fctl = SCSW_FCTL_START_FUNC;
			cdev->private->irb.scsw.cmd.actl = SCSW_ACTL_START_PEND;
			cdev->private->irb.scsw.cmd.stctl =
				SCSW_STCTL_STATUS_PEND;
			cdev->private->flags.fake_irb = 0;
			if (cdev->handler)
				cdev->handler(cdev, cdev->private->intparm,
					      &cdev->private->irb);
			memset(&cdev->private->irb, 0, sizeof(struct irb));
		}
		break;
	case -ETIME:
	case -EUSERS:
		/* Reset oper notify indication after verify error. */
		cdev->private->flags.donotify = 0;
		ccw_device_done(cdev, DEV_STATE_BOXED);
		break;
	case -EACCES:
		/* Reset oper notify indication after verify error. */
		cdev->private->flags.donotify = 0;
		ccw_device_done(cdev, DEV_STATE_DISCONNECTED);
		break;
	default:
		/* Reset oper notify indication after verify error. */
		cdev->private->flags.donotify = 0;
		ccw_device_done(cdev, DEV_STATE_NOT_OPER);
		break;
	}
}

/*
 * Get device online.
 */
int
ccw_device_online(struct ccw_device *cdev)
{
	struct subchannel *sch;
	int ret;

	if ((cdev->private->state != DEV_STATE_OFFLINE) &&
	    (cdev->private->state != DEV_STATE_BOXED))
		return -EINVAL;
	sch = to_subchannel(cdev->dev.parent);
	ret = cio_enable_subchannel(sch, (u32)(addr_t)sch);
	if (ret != 0) {
		/* Couldn't enable the subchannel for i/o. Sick device. */
		if (ret == -ENODEV)
			dev_fsm_event(cdev, DEV_EVENT_NOTOPER);
		return ret;
	}
	/* Start initial path verification. */
	cdev->private->state = DEV_STATE_VERIFY;
	ccw_device_verify_start(cdev);
	return 0;
}

void
ccw_device_disband_done(struct ccw_device *cdev, int err)
{
	switch (err) {
	case 0:
		ccw_device_done(cdev, DEV_STATE_OFFLINE);
		break;
	case -ETIME:
		ccw_device_done(cdev, DEV_STATE_BOXED);
		break;
	default:
		cdev->private->flags.donotify = 0;
		ccw_device_done(cdev, DEV_STATE_NOT_OPER);
		break;
	}
}

/*
 * Shutdown device.
 */
int
ccw_device_offline(struct ccw_device *cdev)
{
	struct subchannel *sch;

	/* Allow ccw_device_offline while disconnected. */
	if (cdev->private->state == DEV_STATE_DISCONNECTED ||
	    cdev->private->state == DEV_STATE_NOT_OPER) {
		cdev->private->flags.donotify = 0;
		ccw_device_done(cdev, DEV_STATE_NOT_OPER);
		return 0;
	}
	if (cdev->private->state == DEV_STATE_BOXED) {
		ccw_device_done(cdev, DEV_STATE_BOXED);
		return 0;
	}
	if (ccw_device_is_orphan(cdev)) {
		ccw_device_done(cdev, DEV_STATE_OFFLINE);
		return 0;
	}
	sch = to_subchannel(cdev->dev.parent);
	if (cio_update_schib(sch))
		return -ENODEV;
	if (scsw_actl(&sch->schib.scsw) != 0)
		return -EBUSY;
	if (cdev->private->state != DEV_STATE_ONLINE)
		return -EINVAL;
	/* Are we doing path grouping? */
	if (!cdev->private->flags.pgroup) {
		/* No, set state offline immediately. */
		ccw_device_done(cdev, DEV_STATE_OFFLINE);
		return 0;
	}
	/* Start Set Path Group commands. */
	cdev->private->state = DEV_STATE_DISBAND_PGID;
	ccw_device_disband_start(cdev);
	return 0;
}

/*
 * Handle not operational event in non-special state.
 */
static void ccw_device_generic_notoper(struct ccw_device *cdev,
				       enum dev_event dev_event)
{
	if (ccw_device_notify(cdev, CIO_GONE) != NOTIFY_OK)
		ccw_device_sched_todo(cdev, CDEV_TODO_UNREG);
	else
		ccw_device_set_disconnected(cdev);
}

/*
 * Handle path verification event in offline state.
 */
static void ccw_device_offline_verify(struct ccw_device *cdev,
				      enum dev_event dev_event)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);

	css_schedule_eval(sch->schid);
}

/*
 * Handle path verification event.
 */
static void
ccw_device_online_verify(struct ccw_device *cdev, enum dev_event dev_event)
{
	struct subchannel *sch;

	if (cdev->private->state == DEV_STATE_W4SENSE) {
		cdev->private->flags.doverify = 1;
		return;
	}
	sch = to_subchannel(cdev->dev.parent);
	/*
	 * Since we might not just be coming from an interrupt from the
	 * subchannel we have to update the schib.
	 */
	if (cio_update_schib(sch)) {
		ccw_device_verify_done(cdev, -ENODEV);
		return;
	}

	if (scsw_actl(&sch->schib.scsw) != 0 ||
	    (scsw_stctl(&sch->schib.scsw) & SCSW_STCTL_STATUS_PEND) ||
	    (scsw_stctl(&cdev->private->irb.scsw) & SCSW_STCTL_STATUS_PEND)) {
		/*
		 * No final status yet or final status not yet delivered
		 * to the device driver. Can't do path verfication now,
		 * delay until final status was delivered.
		 */
		cdev->private->flags.doverify = 1;
		return;
	}
	/* Device is idle, we can do the path verification. */
	cdev->private->state = DEV_STATE_VERIFY;
	ccw_device_verify_start(cdev);
}

/*
 * Handle path verification event in boxed state.
 */
static void ccw_device_boxed_verify(struct ccw_device *cdev,
				    enum dev_event dev_event)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);

	if (cdev->online) {
		if (cio_enable_subchannel(sch, (u32) (addr_t) sch))
			ccw_device_done(cdev, DEV_STATE_NOT_OPER);
		else
			ccw_device_online_verify(cdev, dev_event);
	} else
		css_schedule_eval(sch->schid);
}

/*
 * Got an interrupt for a normal io (state online).
 */
static void
ccw_device_irq(struct ccw_device *cdev, enum dev_event dev_event)
{
	struct irb *irb;
	int is_cmd;

	irb = (struct irb *)&S390_lowcore.irb;
	is_cmd = !scsw_is_tm(&irb->scsw);
	/* Check for unsolicited interrupt. */
	if (!scsw_is_solicited(&irb->scsw)) {
		if (is_cmd && (irb->scsw.cmd.dstat & DEV_STAT_UNIT_CHECK) &&
		    !irb->esw.esw0.erw.cons) {
			/* Unit check but no sense data. Need basic sense. */
			if (ccw_device_do_sense(cdev, irb) != 0)
				goto call_handler_unsol;
			memcpy(&cdev->private->irb, irb, sizeof(struct irb));
			cdev->private->state = DEV_STATE_W4SENSE;
			cdev->private->intparm = 0;
			return;
		}
call_handler_unsol:
		if (cdev->handler)
			cdev->handler (cdev, 0, irb);
		if (cdev->private->flags.doverify)
			ccw_device_online_verify(cdev, 0);
		return;
	}
	/* Accumulate status and find out if a basic sense is needed. */
	ccw_device_accumulate_irb(cdev, irb);
	if (is_cmd && cdev->private->flags.dosense) {
		if (ccw_device_do_sense(cdev, irb) == 0) {
			cdev->private->state = DEV_STATE_W4SENSE;
		}
		return;
	}
	/* Call the handler. */
	if (ccw_device_call_handler(cdev) && cdev->private->flags.doverify)
		/* Start delayed path verification. */
		ccw_device_online_verify(cdev, 0);
}

/*
 * Got an timeout in online state.
 */
static void
ccw_device_online_timeout(struct ccw_device *cdev, enum dev_event dev_event)
{
	int ret;

	ccw_device_set_timeout(cdev, 0);
	ret = ccw_device_cancel_halt_clear(cdev);
	if (ret == -EBUSY) {
		ccw_device_set_timeout(cdev, 3*HZ);
		cdev->private->state = DEV_STATE_TIMEOUT_KILL;
		return;
	}
	if (ret == -ENODEV)
		dev_fsm_event(cdev, DEV_EVENT_NOTOPER);
	else if (cdev->handler)
		cdev->handler(cdev, cdev->private->intparm,
			      ERR_PTR(-ETIMEDOUT));
}

/*
 * Got an interrupt for a basic sense.
 */
static void
ccw_device_w4sense(struct ccw_device *cdev, enum dev_event dev_event)
{
	struct irb *irb;

	irb = (struct irb *)&S390_lowcore.irb;
	/* Check for unsolicited interrupt. */
	if (scsw_stctl(&irb->scsw) ==
	    (SCSW_STCTL_STATUS_PEND | SCSW_STCTL_ALERT_STATUS)) {
		if (scsw_cc(&irb->scsw) == 1)
			/* Basic sense hasn't started. Try again. */
			ccw_device_do_sense(cdev, irb);
		else {
			CIO_MSG_EVENT(0, "0.%x.%04x: unsolicited "
				      "interrupt during w4sense...\n",
				      cdev->private->dev_id.ssid,
				      cdev->private->dev_id.devno);
			if (cdev->handler)
				cdev->handler (cdev, 0, irb);
		}
		return;
	}
	/*
	 * Check if a halt or clear has been issued in the meanwhile. If yes,
	 * only deliver the halt/clear interrupt to the device driver as if it
	 * had killed the original request.
	 */
	if (scsw_fctl(&irb->scsw) &
	    (SCSW_FCTL_CLEAR_FUNC | SCSW_FCTL_HALT_FUNC)) {
		cdev->private->flags.dosense = 0;
		memset(&cdev->private->irb, 0, sizeof(struct irb));
		ccw_device_accumulate_irb(cdev, irb);
		goto call_handler;
	}
	/* Add basic sense info to irb. */
	ccw_device_accumulate_basic_sense(cdev, irb);
	if (cdev->private->flags.dosense) {
		/* Another basic sense is needed. */
		ccw_device_do_sense(cdev, irb);
		return;
	}
call_handler:
	cdev->private->state = DEV_STATE_ONLINE;
	/* In case sensing interfered with setting the device online */
	wake_up(&cdev->private->wait_q);
	/* Call the handler. */
	if (ccw_device_call_handler(cdev) && cdev->private->flags.doverify)
		/* Start delayed path verification. */
		ccw_device_online_verify(cdev, 0);
}

static void
ccw_device_killing_irq(struct ccw_device *cdev, enum dev_event dev_event)
{
	struct subchannel *sch;

	sch = to_subchannel(cdev->dev.parent);
	ccw_device_set_timeout(cdev, 0);
	/* Start delayed path verification. */
	ccw_device_online_verify(cdev, 0);
	/* OK, i/o is dead now. Call interrupt handler. */
	if (cdev->handler)
		cdev->handler(cdev, cdev->private->intparm,
			      ERR_PTR(-EIO));
}

static void
ccw_device_killing_timeout(struct ccw_device *cdev, enum dev_event dev_event)
{
	int ret;

	ret = ccw_device_cancel_halt_clear(cdev);
	if (ret == -EBUSY) {
		ccw_device_set_timeout(cdev, 3*HZ);
		return;
	}
	/* Start delayed path verification. */
	ccw_device_online_verify(cdev, 0);
	if (cdev->handler)
		cdev->handler(cdev, cdev->private->intparm,
			      ERR_PTR(-EIO));
}

void ccw_device_kill_io(struct ccw_device *cdev)
{
	int ret;

	ret = ccw_device_cancel_halt_clear(cdev);
	if (ret == -EBUSY) {
		ccw_device_set_timeout(cdev, 3*HZ);
		cdev->private->state = DEV_STATE_TIMEOUT_KILL;
		return;
	}
	/* Start delayed path verification. */
	ccw_device_online_verify(cdev, 0);
	if (cdev->handler)
		cdev->handler(cdev, cdev->private->intparm,
			      ERR_PTR(-EIO));
}

static void
ccw_device_delay_verify(struct ccw_device *cdev, enum dev_event dev_event)
{
	/* Start verification after current task finished. */
	cdev->private->flags.doverify = 1;
}

static void
ccw_device_start_id(struct ccw_device *cdev, enum dev_event dev_event)
{
	struct subchannel *sch;

	sch = to_subchannel(cdev->dev.parent);
	if (cio_enable_subchannel(sch, (u32)(addr_t)sch) != 0)
		/* Couldn't enable the subchannel for i/o. Sick device. */
		return;
	cdev->private->state = DEV_STATE_DISCONNECTED_SENSE_ID;
	ccw_device_sense_id_start(cdev);
}

void ccw_device_trigger_reprobe(struct ccw_device *cdev)
{
	struct subchannel *sch;

	if (cdev->private->state != DEV_STATE_DISCONNECTED)
		return;

	sch = to_subchannel(cdev->dev.parent);
	/* Update some values. */
	if (cio_update_schib(sch))
		return;
	/*
	 * The pim, pam, pom values may not be accurate, but they are the best
	 * we have before performing device selection :/
	 */
	sch->lpm = sch->schib.pmcw.pam & sch->opm;
	/*
	 * Use the initial configuration since we can't be shure that the old
	 * paths are valid.
	 */
	io_subchannel_init_config(sch);
	if (cio_commit_config(sch))
		return;

	/* We should also udate ssd info, but this has to wait. */
	/* Check if this is another device which appeared on the same sch. */
	if (sch->schib.pmcw.dev != cdev->private->dev_id.devno)
		css_schedule_eval(sch->schid);
	else
		ccw_device_start_id(cdev, 0);
}

static void ccw_device_disabled_irq(struct ccw_device *cdev,
				    enum dev_event dev_event)
{
	struct subchannel *sch;

	sch = to_subchannel(cdev->dev.parent);
	/*
	 * An interrupt in a disabled state means a previous disable was not
	 * successful - should not happen, but we try to disable again.
	 */
	cio_disable_subchannel(sch);
}

static void
ccw_device_change_cmfstate(struct ccw_device *cdev, enum dev_event dev_event)
{
	retry_set_schib(cdev);
	cdev->private->state = DEV_STATE_ONLINE;
	dev_fsm_event(cdev, dev_event);
}

static void ccw_device_update_cmfblock(struct ccw_device *cdev,
				       enum dev_event dev_event)
{
	cmf_retry_copy_block(cdev);
	cdev->private->state = DEV_STATE_ONLINE;
	dev_fsm_event(cdev, dev_event);
}

static void
ccw_device_quiesce_done(struct ccw_device *cdev, enum dev_event dev_event)
{
	ccw_device_set_timeout(cdev, 0);
	cdev->private->state = DEV_STATE_NOT_OPER;
	wake_up(&cdev->private->wait_q);
}

static void
ccw_device_quiesce_timeout(struct ccw_device *cdev, enum dev_event dev_event)
{
	int ret;

	ret = ccw_device_cancel_halt_clear(cdev);
	if (ret == -EBUSY) {
		ccw_device_set_timeout(cdev, HZ/10);
	} else {
		cdev->private->state = DEV_STATE_NOT_OPER;
		wake_up(&cdev->private->wait_q);
	}
}

/*
 * No operation action. This is used e.g. to ignore a timeout event in
 * state offline.
 */
static void
ccw_device_nop(struct ccw_device *cdev, enum dev_event dev_event)
{
}

/*
 * device statemachine
 */
fsm_func_t *dev_jumptable[NR_DEV_STATES][NR_DEV_EVENTS] = {
	[DEV_STATE_NOT_OPER] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_nop,
		[DEV_EVENT_INTERRUPT]	= ccw_device_disabled_irq,
		[DEV_EVENT_TIMEOUT]	= ccw_device_nop,
		[DEV_EVENT_VERIFY]	= ccw_device_nop,
	},
	[DEV_STATE_SENSE_PGID] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_request_event,
		[DEV_EVENT_INTERRUPT]	= ccw_device_request_event,
		[DEV_EVENT_TIMEOUT]	= ccw_device_request_event,
		[DEV_EVENT_VERIFY]	= ccw_device_nop,
	},
	[DEV_STATE_SENSE_ID] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_request_event,
		[DEV_EVENT_INTERRUPT]	= ccw_device_request_event,
		[DEV_EVENT_TIMEOUT]	= ccw_device_request_event,
		[DEV_EVENT_VERIFY]	= ccw_device_nop,
	},
	[DEV_STATE_OFFLINE] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_generic_notoper,
		[DEV_EVENT_INTERRUPT]	= ccw_device_disabled_irq,
		[DEV_EVENT_TIMEOUT]	= ccw_device_nop,
		[DEV_EVENT_VERIFY]	= ccw_device_offline_verify,
	},
	[DEV_STATE_VERIFY] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_request_event,
		[DEV_EVENT_INTERRUPT]	= ccw_device_request_event,
		[DEV_EVENT_TIMEOUT]	= ccw_device_request_event,
		[DEV_EVENT_VERIFY]	= ccw_device_delay_verify,
	},
	[DEV_STATE_ONLINE] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_generic_notoper,
		[DEV_EVENT_INTERRUPT]	= ccw_device_irq,
		[DEV_EVENT_TIMEOUT]	= ccw_device_online_timeout,
		[DEV_EVENT_VERIFY]	= ccw_device_online_verify,
	},
	[DEV_STATE_W4SENSE] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_generic_notoper,
		[DEV_EVENT_INTERRUPT]	= ccw_device_w4sense,
		[DEV_EVENT_TIMEOUT]	= ccw_device_nop,
		[DEV_EVENT_VERIFY]	= ccw_device_online_verify,
	},
	[DEV_STATE_DISBAND_PGID] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_request_event,
		[DEV_EVENT_INTERRUPT]	= ccw_device_request_event,
		[DEV_EVENT_TIMEOUT]	= ccw_device_request_event,
		[DEV_EVENT_VERIFY]	= ccw_device_nop,
	},
	[DEV_STATE_BOXED] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_generic_notoper,
		[DEV_EVENT_INTERRUPT]	= ccw_device_nop,
		[DEV_EVENT_TIMEOUT]	= ccw_device_nop,
		[DEV_EVENT_VERIFY]	= ccw_device_boxed_verify,
	},
	/* states to wait for i/o completion before doing something */
	[DEV_STATE_TIMEOUT_KILL] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_generic_notoper,
		[DEV_EVENT_INTERRUPT]	= ccw_device_killing_irq,
		[DEV_EVENT_TIMEOUT]	= ccw_device_killing_timeout,
		[DEV_EVENT_VERIFY]	= ccw_device_nop,
	},
	[DEV_STATE_QUIESCE] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_quiesce_done,
		[DEV_EVENT_INTERRUPT]	= ccw_device_quiesce_done,
		[DEV_EVENT_TIMEOUT]	= ccw_device_quiesce_timeout,
		[DEV_EVENT_VERIFY]	= ccw_device_nop,
	},
	/* special states for devices gone not operational */
	[DEV_STATE_DISCONNECTED] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_nop,
		[DEV_EVENT_INTERRUPT]	= ccw_device_start_id,
		[DEV_EVENT_TIMEOUT]	= ccw_device_nop,
		[DEV_EVENT_VERIFY]	= ccw_device_start_id,
	},
	[DEV_STATE_DISCONNECTED_SENSE_ID] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_request_event,
		[DEV_EVENT_INTERRUPT]	= ccw_device_request_event,
		[DEV_EVENT_TIMEOUT]	= ccw_device_request_event,
		[DEV_EVENT_VERIFY]	= ccw_device_nop,
	},
	[DEV_STATE_CMFCHANGE] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_change_cmfstate,
		[DEV_EVENT_INTERRUPT]	= ccw_device_change_cmfstate,
		[DEV_EVENT_TIMEOUT]	= ccw_device_change_cmfstate,
		[DEV_EVENT_VERIFY]	= ccw_device_change_cmfstate,
	},
	[DEV_STATE_CMFUPDATE] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_update_cmfblock,
		[DEV_EVENT_INTERRUPT]	= ccw_device_update_cmfblock,
		[DEV_EVENT_TIMEOUT]	= ccw_device_update_cmfblock,
		[DEV_EVENT_VERIFY]	= ccw_device_update_cmfblock,
	},
	[DEV_STATE_STEAL_LOCK] = {
		[DEV_EVENT_NOTOPER]	= ccw_device_request_event,
		[DEV_EVENT_INTERRUPT]	= ccw_device_request_event,
		[DEV_EVENT_TIMEOUT]	= ccw_device_request_event,
		[DEV_EVENT_VERIFY]	= ccw_device_nop,
	},
};

EXPORT_SYMBOL_GPL(ccw_device_set_timeout);
