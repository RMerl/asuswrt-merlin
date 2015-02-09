/*
 * Copyright IBM Corp. 2002, 2009
 *
 * Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com)
 *	      Cornelia Huck (cornelia.huck@de.ibm.com)
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/completion.h>

#include <asm/ccwdev.h>
#include <asm/idals.h>
#include <asm/chpid.h>
#include <asm/fcx.h>

#include "cio.h"
#include "cio_debug.h"
#include "css.h"
#include "chsc.h"
#include "device.h"
#include "chp.h"

/**
 * ccw_device_set_options_mask() - set some options and unset the rest
 * @cdev: device for which the options are to be set
 * @flags: options to be set
 *
 * All flags specified in @flags are set, all flags not specified in @flags
 * are cleared.
 * Returns:
 *   %0 on success, -%EINVAL on an invalid flag combination.
 */
int ccw_device_set_options_mask(struct ccw_device *cdev, unsigned long flags)
{
       /*
	* The flag usage is mutal exclusive ...
	*/
	if ((flags & CCWDEV_EARLY_NOTIFICATION) &&
	    (flags & CCWDEV_REPORT_ALL))
		return -EINVAL;
	cdev->private->options.fast = (flags & CCWDEV_EARLY_NOTIFICATION) != 0;
	cdev->private->options.repall = (flags & CCWDEV_REPORT_ALL) != 0;
	cdev->private->options.pgroup = (flags & CCWDEV_DO_PATHGROUP) != 0;
	cdev->private->options.force = (flags & CCWDEV_ALLOW_FORCE) != 0;
	cdev->private->options.mpath = (flags & CCWDEV_DO_MULTIPATH) != 0;
	return 0;
}

/**
 * ccw_device_set_options() - set some options
 * @cdev: device for which the options are to be set
 * @flags: options to be set
 *
 * All flags specified in @flags are set, the remainder is left untouched.
 * Returns:
 *   %0 on success, -%EINVAL if an invalid flag combination would ensue.
 */
int ccw_device_set_options(struct ccw_device *cdev, unsigned long flags)
{
       /*
	* The flag usage is mutal exclusive ...
	*/
	if (((flags & CCWDEV_EARLY_NOTIFICATION) &&
	    (flags & CCWDEV_REPORT_ALL)) ||
	    ((flags & CCWDEV_EARLY_NOTIFICATION) &&
	     cdev->private->options.repall) ||
	    ((flags & CCWDEV_REPORT_ALL) &&
	     cdev->private->options.fast))
		return -EINVAL;
	cdev->private->options.fast |= (flags & CCWDEV_EARLY_NOTIFICATION) != 0;
	cdev->private->options.repall |= (flags & CCWDEV_REPORT_ALL) != 0;
	cdev->private->options.pgroup |= (flags & CCWDEV_DO_PATHGROUP) != 0;
	cdev->private->options.force |= (flags & CCWDEV_ALLOW_FORCE) != 0;
	cdev->private->options.mpath |= (flags & CCWDEV_DO_MULTIPATH) != 0;
	return 0;
}

/**
 * ccw_device_clear_options() - clear some options
 * @cdev: device for which the options are to be cleared
 * @flags: options to be cleared
 *
 * All flags specified in @flags are cleared, the remainder is left untouched.
 */
void ccw_device_clear_options(struct ccw_device *cdev, unsigned long flags)
{
	cdev->private->options.fast &= (flags & CCWDEV_EARLY_NOTIFICATION) == 0;
	cdev->private->options.repall &= (flags & CCWDEV_REPORT_ALL) == 0;
	cdev->private->options.pgroup &= (flags & CCWDEV_DO_PATHGROUP) == 0;
	cdev->private->options.force &= (flags & CCWDEV_ALLOW_FORCE) == 0;
	cdev->private->options.mpath &= (flags & CCWDEV_DO_MULTIPATH) == 0;
}

/**
 * ccw_device_is_pathgroup - determine if paths to this device are grouped
 * @cdev: ccw device
 *
 * Return non-zero if there is a path group, zero otherwise.
 */
int ccw_device_is_pathgroup(struct ccw_device *cdev)
{
	return cdev->private->flags.pgroup;
}
EXPORT_SYMBOL(ccw_device_is_pathgroup);

/**
 * ccw_device_is_multipath - determine if device is operating in multipath mode
 * @cdev: ccw device
 *
 * Return non-zero if device is operating in multipath mode, zero otherwise.
 */
int ccw_device_is_multipath(struct ccw_device *cdev)
{
	return cdev->private->flags.mpath;
}
EXPORT_SYMBOL(ccw_device_is_multipath);

/**
 * ccw_device_clear() - terminate I/O request processing
 * @cdev: target ccw device
 * @intparm: interruption parameter; value is only used if no I/O is
 *	     outstanding, otherwise the intparm associated with the I/O request
 *	     is returned
 *
 * ccw_device_clear() calls csch on @cdev's subchannel.
 * Returns:
 *  %0 on success,
 *  -%ENODEV on device not operational,
 *  -%EINVAL on invalid device state.
 * Context:
 *  Interrupts disabled, ccw device lock held
 */
int ccw_device_clear(struct ccw_device *cdev, unsigned long intparm)
{
	struct subchannel *sch;
	int ret;

	if (!cdev || !cdev->dev.parent)
		return -ENODEV;
	sch = to_subchannel(cdev->dev.parent);
	if (!sch->schib.pmcw.ena)
		return -EINVAL;
	if (cdev->private->state == DEV_STATE_NOT_OPER)
		return -ENODEV;
	if (cdev->private->state != DEV_STATE_ONLINE &&
	    cdev->private->state != DEV_STATE_W4SENSE)
		return -EINVAL;

	ret = cio_clear(sch);
	if (ret == 0)
		cdev->private->intparm = intparm;
	return ret;
}

/**
 * ccw_device_start_key() - start a s390 channel program with key
 * @cdev: target ccw device
 * @cpa: logical start address of channel program
 * @intparm: user specific interruption parameter; will be presented back to
 *	     @cdev's interrupt handler. Allows a device driver to associate
 *	     the interrupt with a particular I/O request.
 * @lpm: defines the channel path to be used for a specific I/O request. A
 *	 value of 0 will make cio use the opm.
 * @key: storage key to be used for the I/O
 * @flags: additional flags; defines the action to be performed for I/O
 *	   processing.
 *
 * Start a S/390 channel program. When the interrupt arrives, the
 * IRQ handler is called, either immediately, delayed (dev-end missing,
 * or sense required) or never (no IRQ handler registered).
 * Returns:
 *  %0, if the operation was successful;
 *  -%EBUSY, if the device is busy, or status pending;
 *  -%EACCES, if no path specified in @lpm is operational;
 *  -%ENODEV, if the device is not operational.
 * Context:
 *  Interrupts disabled, ccw device lock held
 */
int ccw_device_start_key(struct ccw_device *cdev, struct ccw1 *cpa,
			 unsigned long intparm, __u8 lpm, __u8 key,
			 unsigned long flags)
{
	struct subchannel *sch;
	int ret;

	if (!cdev || !cdev->dev.parent)
		return -ENODEV;
	sch = to_subchannel(cdev->dev.parent);
	if (!sch->schib.pmcw.ena)
		return -EINVAL;
	if (cdev->private->state == DEV_STATE_NOT_OPER)
		return -ENODEV;
	if (cdev->private->state == DEV_STATE_VERIFY) {
		/* Remember to fake irb when finished. */
		if (!cdev->private->flags.fake_irb) {
			cdev->private->flags.fake_irb = 1;
			cdev->private->intparm = intparm;
			return 0;
		} else
			/* There's already a fake I/O around. */
			return -EBUSY;
	}
	if (cdev->private->state != DEV_STATE_ONLINE ||
	    ((sch->schib.scsw.cmd.stctl & SCSW_STCTL_PRIM_STATUS) &&
	     !(sch->schib.scsw.cmd.stctl & SCSW_STCTL_SEC_STATUS)) ||
	    cdev->private->flags.doverify)
		return -EBUSY;
	ret = cio_set_options (sch, flags);
	if (ret)
		return ret;
	/* Adjust requested path mask to excluded varied off paths. */
	if (lpm) {
		lpm &= sch->opm;
		if (lpm == 0)
			return -EACCES;
	}
	ret = cio_start_key (sch, cpa, lpm, key);
	switch (ret) {
	case 0:
		cdev->private->intparm = intparm;
		break;
	case -EACCES:
	case -ENODEV:
		dev_fsm_event(cdev, DEV_EVENT_VERIFY);
		break;
	}
	return ret;
}

/**
 * ccw_device_start_timeout_key() - start a s390 channel program with timeout and key
 * @cdev: target ccw device
 * @cpa: logical start address of channel program
 * @intparm: user specific interruption parameter; will be presented back to
 *	     @cdev's interrupt handler. Allows a device driver to associate
 *	     the interrupt with a particular I/O request.
 * @lpm: defines the channel path to be used for a specific I/O request. A
 *	 value of 0 will make cio use the opm.
 * @key: storage key to be used for the I/O
 * @flags: additional flags; defines the action to be performed for I/O
 *	   processing.
 * @expires: timeout value in jiffies
 *
 * Start a S/390 channel program. When the interrupt arrives, the
 * IRQ handler is called, either immediately, delayed (dev-end missing,
 * or sense required) or never (no IRQ handler registered).
 * This function notifies the device driver if the channel program has not
 * completed during the time specified by @expires. If a timeout occurs, the
 * channel program is terminated via xsch, hsch or csch, and the device's
 * interrupt handler will be called with an irb containing ERR_PTR(-%ETIMEDOUT).
 * Returns:
 *  %0, if the operation was successful;
 *  -%EBUSY, if the device is busy, or status pending;
 *  -%EACCES, if no path specified in @lpm is operational;
 *  -%ENODEV, if the device is not operational.
 * Context:
 *  Interrupts disabled, ccw device lock held
 */
int ccw_device_start_timeout_key(struct ccw_device *cdev, struct ccw1 *cpa,
				 unsigned long intparm, __u8 lpm, __u8 key,
				 unsigned long flags, int expires)
{
	int ret;

	if (!cdev)
		return -ENODEV;
	ccw_device_set_timeout(cdev, expires);
	ret = ccw_device_start_key(cdev, cpa, intparm, lpm, key, flags);
	if (ret != 0)
		ccw_device_set_timeout(cdev, 0);
	return ret;
}

/**
 * ccw_device_start() - start a s390 channel program
 * @cdev: target ccw device
 * @cpa: logical start address of channel program
 * @intparm: user specific interruption parameter; will be presented back to
 *	     @cdev's interrupt handler. Allows a device driver to associate
 *	     the interrupt with a particular I/O request.
 * @lpm: defines the channel path to be used for a specific I/O request. A
 *	 value of 0 will make cio use the opm.
 * @flags: additional flags; defines the action to be performed for I/O
 *	   processing.
 *
 * Start a S/390 channel program. When the interrupt arrives, the
 * IRQ handler is called, either immediately, delayed (dev-end missing,
 * or sense required) or never (no IRQ handler registered).
 * Returns:
 *  %0, if the operation was successful;
 *  -%EBUSY, if the device is busy, or status pending;
 *  -%EACCES, if no path specified in @lpm is operational;
 *  -%ENODEV, if the device is not operational.
 * Context:
 *  Interrupts disabled, ccw device lock held
 */
int ccw_device_start(struct ccw_device *cdev, struct ccw1 *cpa,
		     unsigned long intparm, __u8 lpm, unsigned long flags)
{
	return ccw_device_start_key(cdev, cpa, intparm, lpm,
				    PAGE_DEFAULT_KEY, flags);
}

/**
 * ccw_device_start_timeout() - start a s390 channel program with timeout
 * @cdev: target ccw device
 * @cpa: logical start address of channel program
 * @intparm: user specific interruption parameter; will be presented back to
 *	     @cdev's interrupt handler. Allows a device driver to associate
 *	     the interrupt with a particular I/O request.
 * @lpm: defines the channel path to be used for a specific I/O request. A
 *	 value of 0 will make cio use the opm.
 * @flags: additional flags; defines the action to be performed for I/O
 *	   processing.
 * @expires: timeout value in jiffies
 *
 * Start a S/390 channel program. When the interrupt arrives, the
 * IRQ handler is called, either immediately, delayed (dev-end missing,
 * or sense required) or never (no IRQ handler registered).
 * This function notifies the device driver if the channel program has not
 * completed during the time specified by @expires. If a timeout occurs, the
 * channel program is terminated via xsch, hsch or csch, and the device's
 * interrupt handler will be called with an irb containing ERR_PTR(-%ETIMEDOUT).
 * Returns:
 *  %0, if the operation was successful;
 *  -%EBUSY, if the device is busy, or status pending;
 *  -%EACCES, if no path specified in @lpm is operational;
 *  -%ENODEV, if the device is not operational.
 * Context:
 *  Interrupts disabled, ccw device lock held
 */
int ccw_device_start_timeout(struct ccw_device *cdev, struct ccw1 *cpa,
			     unsigned long intparm, __u8 lpm,
			     unsigned long flags, int expires)
{
	return ccw_device_start_timeout_key(cdev, cpa, intparm, lpm,
					    PAGE_DEFAULT_KEY, flags,
					    expires);
}


/**
 * ccw_device_halt() - halt I/O request processing
 * @cdev: target ccw device
 * @intparm: interruption parameter; value is only used if no I/O is
 *	     outstanding, otherwise the intparm associated with the I/O request
 *	     is returned
 *
 * ccw_device_halt() calls hsch on @cdev's subchannel.
 * Returns:
 *  %0 on success,
 *  -%ENODEV on device not operational,
 *  -%EINVAL on invalid device state,
 *  -%EBUSY on device busy or interrupt pending.
 * Context:
 *  Interrupts disabled, ccw device lock held
 */
int ccw_device_halt(struct ccw_device *cdev, unsigned long intparm)
{
	struct subchannel *sch;
	int ret;

	if (!cdev || !cdev->dev.parent)
		return -ENODEV;
	sch = to_subchannel(cdev->dev.parent);
	if (!sch->schib.pmcw.ena)
		return -EINVAL;
	if (cdev->private->state == DEV_STATE_NOT_OPER)
		return -ENODEV;
	if (cdev->private->state != DEV_STATE_ONLINE &&
	    cdev->private->state != DEV_STATE_W4SENSE)
		return -EINVAL;

	ret = cio_halt(sch);
	if (ret == 0)
		cdev->private->intparm = intparm;
	return ret;
}

/**
 * ccw_device_resume() - resume channel program execution
 * @cdev: target ccw device
 *
 * ccw_device_resume() calls rsch on @cdev's subchannel.
 * Returns:
 *  %0 on success,
 *  -%ENODEV on device not operational,
 *  -%EINVAL on invalid device state,
 *  -%EBUSY on device busy or interrupt pending.
 * Context:
 *  Interrupts disabled, ccw device lock held
 */
int ccw_device_resume(struct ccw_device *cdev)
{
	struct subchannel *sch;

	if (!cdev || !cdev->dev.parent)
		return -ENODEV;
	sch = to_subchannel(cdev->dev.parent);
	if (!sch->schib.pmcw.ena)
		return -EINVAL;
	if (cdev->private->state == DEV_STATE_NOT_OPER)
		return -ENODEV;
	if (cdev->private->state != DEV_STATE_ONLINE ||
	    !(sch->schib.scsw.cmd.actl & SCSW_ACTL_SUSPENDED))
		return -EINVAL;
	return cio_resume(sch);
}

/*
 * Pass interrupt to device driver.
 */
int
ccw_device_call_handler(struct ccw_device *cdev)
{
	struct subchannel *sch;
	unsigned int stctl;
	int ending_status;

	sch = to_subchannel(cdev->dev.parent);

	/*
	 * we allow for the device action handler if .
	 *  - we received ending status
	 *  - the action handler requested to see all interrupts
	 *  - we received an intermediate status
	 *  - fast notification was requested (primary status)
	 *  - unsolicited interrupts
	 */
	stctl = scsw_stctl(&cdev->private->irb.scsw);
	ending_status = (stctl & SCSW_STCTL_SEC_STATUS) ||
		(stctl == (SCSW_STCTL_ALERT_STATUS | SCSW_STCTL_STATUS_PEND)) ||
		(stctl == SCSW_STCTL_STATUS_PEND);
	if (!ending_status &&
	    !cdev->private->options.repall &&
	    !(stctl & SCSW_STCTL_INTER_STATUS) &&
	    !(cdev->private->options.fast &&
	      (stctl & SCSW_STCTL_PRIM_STATUS)))
		return 0;

	/* Clear pending timers for device driver initiated I/O. */
	if (ending_status)
		ccw_device_set_timeout(cdev, 0);
	/*
	 * Now we are ready to call the device driver interrupt handler.
	 */
	if (cdev->handler)
		cdev->handler(cdev, cdev->private->intparm,
			      &cdev->private->irb);

	/*
	 * Clear the old and now useless interrupt response block.
	 */
	memset(&cdev->private->irb, 0, sizeof(struct irb));

	return 1;
}

/**
 * ccw_device_get_ciw() - Search for CIW command in extended sense data.
 * @cdev: ccw device to inspect
 * @ct: command type to look for
 *
 * During SenseID, command information words (CIWs) describing special
 * commands available to the device may have been stored in the extended
 * sense data. This function searches for CIWs of a specified command
 * type in the extended sense data.
 * Returns:
 *  %NULL if no extended sense data has been stored or if no CIW of the
 *  specified command type could be found,
 *  else a pointer to the CIW of the specified command type.
 */
struct ciw *ccw_device_get_ciw(struct ccw_device *cdev, __u32 ct)
{
	int ciw_cnt;

	if (cdev->private->flags.esid == 0)
		return NULL;
	for (ciw_cnt = 0; ciw_cnt < MAX_CIWS; ciw_cnt++)
		if (cdev->private->senseid.ciw[ciw_cnt].ct == ct)
			return cdev->private->senseid.ciw + ciw_cnt;
	return NULL;
}

/**
 * ccw_device_get_path_mask() - get currently available paths
 * @cdev: ccw device to be queried
 * Returns:
 *  %0 if no subchannel for the device is available,
 *  else the mask of currently available paths for the ccw device's subchannel.
 */
__u8 ccw_device_get_path_mask(struct ccw_device *cdev)
{
	struct subchannel *sch;

	if (!cdev->dev.parent)
		return 0;

	sch = to_subchannel(cdev->dev.parent);
	return sch->lpm;
}

struct stlck_data {
	struct completion done;
	int rc;
};

void ccw_device_stlck_done(struct ccw_device *cdev, void *data, int rc)
{
	struct stlck_data *sdata = data;

	sdata->rc = rc;
	complete(&sdata->done);
}

/*
 * Perform unconditional reserve + release.
 */
int ccw_device_stlck(struct ccw_device *cdev)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);
	struct stlck_data data;
	u8 *buffer;
	int rc;

	/* Check if steal lock operation is valid for this device. */
	if (cdev->drv) {
		if (!cdev->private->options.force)
			return -EINVAL;
	}
	buffer = kzalloc(64, GFP_DMA | GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;
	init_completion(&data.done);
	data.rc = -EIO;
	spin_lock_irq(sch->lock);
	rc = cio_enable_subchannel(sch, (u32) (addr_t) sch);
	if (rc)
		goto out_unlock;
	/* Perform operation. */
	cdev->private->state = DEV_STATE_STEAL_LOCK,
	ccw_device_stlck_start(cdev, &data, &buffer[0], &buffer[32]);
	spin_unlock_irq(sch->lock);
	/* Wait for operation to finish. */
	if (wait_for_completion_interruptible(&data.done)) {
		/* Got a signal. */
		spin_lock_irq(sch->lock);
		ccw_request_cancel(cdev);
		spin_unlock_irq(sch->lock);
		wait_for_completion(&data.done);
	}
	rc = data.rc;
	/* Check results. */
	spin_lock_irq(sch->lock);
	cio_disable_subchannel(sch);
	cdev->private->state = DEV_STATE_BOXED;
out_unlock:
	spin_unlock_irq(sch->lock);
	kfree(buffer);

	return rc;
}

void *ccw_device_get_chp_desc(struct ccw_device *cdev, int chp_no)
{
	struct subchannel *sch;
	struct chp_id chpid;

	sch = to_subchannel(cdev->dev.parent);
	chp_id_init(&chpid);
	chpid.id = sch->schib.pmcw.chpid[chp_no];
	return chp_get_chp_desc(chpid);
}

/**
 * ccw_device_get_id - obtain a ccw device id
 * @cdev: device to obtain the id for
 * @dev_id: where to fill in the values
 */
void ccw_device_get_id(struct ccw_device *cdev, struct ccw_dev_id *dev_id)
{
	*dev_id = cdev->private->dev_id;
}
EXPORT_SYMBOL(ccw_device_get_id);

/**
 * ccw_device_tm_start_key - perform start function
 * @cdev: ccw device on which to perform the start function
 * @tcw: transport-command word to be started
 * @intparm: user defined parameter to be passed to the interrupt handler
 * @lpm: mask of paths to use
 * @key: storage key to use for storage access
 *
 * Start the tcw on the given ccw device. Return zero on success, non-zero
 * otherwise.
 */
int ccw_device_tm_start_key(struct ccw_device *cdev, struct tcw *tcw,
			    unsigned long intparm, u8 lpm, u8 key)
{
	struct subchannel *sch;
	int rc;

	sch = to_subchannel(cdev->dev.parent);
	if (!sch->schib.pmcw.ena)
		return -EINVAL;
	if (cdev->private->state != DEV_STATE_ONLINE)
		return -EIO;
	/* Adjust requested path mask to excluded varied off paths. */
	if (lpm) {
		lpm &= sch->opm;
		if (lpm == 0)
			return -EACCES;
	}
	rc = cio_tm_start_key(sch, tcw, lpm, key);
	if (rc == 0)
		cdev->private->intparm = intparm;
	return rc;
}
EXPORT_SYMBOL(ccw_device_tm_start_key);

/**
 * ccw_device_tm_start_timeout_key - perform start function
 * @cdev: ccw device on which to perform the start function
 * @tcw: transport-command word to be started
 * @intparm: user defined parameter to be passed to the interrupt handler
 * @lpm: mask of paths to use
 * @key: storage key to use for storage access
 * @expires: time span in jiffies after which to abort request
 *
 * Start the tcw on the given ccw device. Return zero on success, non-zero
 * otherwise.
 */
int ccw_device_tm_start_timeout_key(struct ccw_device *cdev, struct tcw *tcw,
				    unsigned long intparm, u8 lpm, u8 key,
				    int expires)
{
	int ret;

	ccw_device_set_timeout(cdev, expires);
	ret = ccw_device_tm_start_key(cdev, tcw, intparm, lpm, key);
	if (ret != 0)
		ccw_device_set_timeout(cdev, 0);
	return ret;
}
EXPORT_SYMBOL(ccw_device_tm_start_timeout_key);

/**
 * ccw_device_tm_start - perform start function
 * @cdev: ccw device on which to perform the start function
 * @tcw: transport-command word to be started
 * @intparm: user defined parameter to be passed to the interrupt handler
 * @lpm: mask of paths to use
 *
 * Start the tcw on the given ccw device. Return zero on success, non-zero
 * otherwise.
 */
int ccw_device_tm_start(struct ccw_device *cdev, struct tcw *tcw,
			unsigned long intparm, u8 lpm)
{
	return ccw_device_tm_start_key(cdev, tcw, intparm, lpm,
				       PAGE_DEFAULT_KEY);
}
EXPORT_SYMBOL(ccw_device_tm_start);

/**
 * ccw_device_tm_start_timeout - perform start function
 * @cdev: ccw device on which to perform the start function
 * @tcw: transport-command word to be started
 * @intparm: user defined parameter to be passed to the interrupt handler
 * @lpm: mask of paths to use
 * @expires: time span in jiffies after which to abort request
 *
 * Start the tcw on the given ccw device. Return zero on success, non-zero
 * otherwise.
 */
int ccw_device_tm_start_timeout(struct ccw_device *cdev, struct tcw *tcw,
			       unsigned long intparm, u8 lpm, int expires)
{
	return ccw_device_tm_start_timeout_key(cdev, tcw, intparm, lpm,
					       PAGE_DEFAULT_KEY, expires);
}
EXPORT_SYMBOL(ccw_device_tm_start_timeout);

/**
 * ccw_device_tm_intrg - perform interrogate function
 * @cdev: ccw device on which to perform the interrogate function
 *
 * Perform an interrogate function on the given ccw device. Return zero on
 * success, non-zero otherwise.
 */
int ccw_device_tm_intrg(struct ccw_device *cdev)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);

	if (!sch->schib.pmcw.ena)
		return -EINVAL;
	if (cdev->private->state != DEV_STATE_ONLINE)
		return -EIO;
	if (!scsw_is_tm(&sch->schib.scsw) ||
	    !(scsw_actl(&sch->schib.scsw) & SCSW_ACTL_START_PEND))
		return -EINVAL;
	return cio_tm_intrg(sch);
}
EXPORT_SYMBOL(ccw_device_tm_intrg);


int
_ccw_device_get_subchannel_number(struct ccw_device *cdev)
{
	return cdev->private->schid.sch_no;
}


MODULE_LICENSE("GPL");
EXPORT_SYMBOL(ccw_device_set_options_mask);
EXPORT_SYMBOL(ccw_device_set_options);
EXPORT_SYMBOL(ccw_device_clear_options);
EXPORT_SYMBOL(ccw_device_clear);
EXPORT_SYMBOL(ccw_device_halt);
EXPORT_SYMBOL(ccw_device_resume);
EXPORT_SYMBOL(ccw_device_start_timeout);
EXPORT_SYMBOL(ccw_device_start);
EXPORT_SYMBOL(ccw_device_start_timeout_key);
EXPORT_SYMBOL(ccw_device_start_key);
EXPORT_SYMBOL(ccw_device_get_ciw);
EXPORT_SYMBOL(ccw_device_get_path_mask);
EXPORT_SYMBOL(_ccw_device_get_subchannel_number);
EXPORT_SYMBOL_GPL(ccw_device_get_chp_desc);
