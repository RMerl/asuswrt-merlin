/*
 * linux/kernel/time/clockevents.c
 *
 * This file contains functions which manage clock event devices.
 *
 * Copyright(C) 2005-2006, Thomas Gleixner <tglx@linutronix.de>
 * Copyright(C) 2005-2007, Red Hat, Inc., Ingo Molnar
 * Copyright(C) 2006-2007, Timesys Corp., Thomas Gleixner
 *
 * This code is licenced under the GPL version 2. For details see
 * kernel-base/COPYING.
 */

#include <linux/clockchips.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/smp.h>
#include <linux/sysdev.h>
#include <linux/tick.h>

#include "tick-internal.h"

/* The registered clock event devices */
static LIST_HEAD(clockevent_devices);
static LIST_HEAD(clockevents_released);

/* Notification for clock events */
static RAW_NOTIFIER_HEAD(clockevents_chain);

/* Protection for the above */
static DEFINE_RAW_SPINLOCK(clockevents_lock);

/**
 * clockevents_delta2ns - Convert a latch value (device ticks) to nanoseconds
 * @latch:	value to convert
 * @evt:	pointer to clock event device descriptor
 *
 * Math helper, returns latch value converted to nanoseconds (bound checked)
 */
u64 clockevent_delta2ns(unsigned long latch, struct clock_event_device *evt)
{
	u64 clc = (u64) latch << evt->shift;

	if (unlikely(!evt->mult)) {
		evt->mult = 1;
		WARN_ON(1);
	}

	do_div(clc, evt->mult);
	if (clc < 1000)
		clc = 1000;
	if (clc > KTIME_MAX)
		clc = KTIME_MAX;

	return clc;
}
EXPORT_SYMBOL_GPL(clockevent_delta2ns);

/**
 * clockevents_set_mode - set the operating mode of a clock event device
 * @dev:	device to modify
 * @mode:	new mode
 *
 * Must be called with interrupts disabled !
 */
void clockevents_set_mode(struct clock_event_device *dev,
				 enum clock_event_mode mode)
{
	if (dev->mode != mode) {
		dev->set_mode(mode, dev);
		dev->mode = mode;

		/*
		 * A nsec2cyc multiplicator of 0 is invalid and we'd crash
		 * on it, so fix it up and emit a warning:
		 */
		if (mode == CLOCK_EVT_MODE_ONESHOT) {
			if (unlikely(!dev->mult)) {
				dev->mult = 1;
				WARN_ON(1);
			}
		}
	}
}

/**
 * clockevents_shutdown - shutdown the device and clear next_event
 * @dev:	device to shutdown
 */
void clockevents_shutdown(struct clock_event_device *dev)
{
	clockevents_set_mode(dev, CLOCK_EVT_MODE_SHUTDOWN);
	dev->next_event.tv64 = KTIME_MAX;
}

/**
 * clockevents_program_event - Reprogram the clock event device.
 * @expires:	absolute expiry time (monotonic clock)
 *
 * Returns 0 on success, -ETIME when the event is in the past.
 */
int clockevents_program_event(struct clock_event_device *dev, ktime_t expires,
			      ktime_t now)
{
	unsigned long long clc;
	int64_t delta;

	if (unlikely(expires.tv64 < 0)) {
		WARN_ON_ONCE(1);
		return -ETIME;
	}

	delta = ktime_to_ns(ktime_sub(expires, now));

	if (delta <= 0)
		return -ETIME;

	dev->next_event = expires;

	if (dev->mode == CLOCK_EVT_MODE_SHUTDOWN)
		return 0;

	if (delta > dev->max_delta_ns)
		delta = dev->max_delta_ns;
	if (delta < dev->min_delta_ns)
		delta = dev->min_delta_ns;

	clc = delta * dev->mult;
	clc >>= dev->shift;

	return dev->set_next_event((unsigned long) clc, dev);
}

/**
 * clockevents_register_notifier - register a clock events change listener
 */
int clockevents_register_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	raw_spin_lock_irqsave(&clockevents_lock, flags);
	ret = raw_notifier_chain_register(&clockevents_chain, nb);
	raw_spin_unlock_irqrestore(&clockevents_lock, flags);

	return ret;
}

/*
 * Notify about a clock event change. Called with clockevents_lock
 * held.
 */
static void clockevents_do_notify(unsigned long reason, void *dev)
{
	raw_notifier_call_chain(&clockevents_chain, reason, dev);
}

/*
 * Called after a notify add to make devices available which were
 * released from the notifier call.
 */
static void clockevents_notify_released(void)
{
	struct clock_event_device *dev;

	while (!list_empty(&clockevents_released)) {
		dev = list_entry(clockevents_released.next,
				 struct clock_event_device, list);
		list_del(&dev->list);
		list_add(&dev->list, &clockevent_devices);
		clockevents_do_notify(CLOCK_EVT_NOTIFY_ADD, dev);
	}
}

/**
 * clockevents_register_device - register a clock event device
 * @dev:	device to register
 */
void clockevents_register_device(struct clock_event_device *dev)
{
	unsigned long flags;

	BUG_ON(dev->mode != CLOCK_EVT_MODE_UNUSED);
	BUG_ON(!dev->cpumask);

	raw_spin_lock_irqsave(&clockevents_lock, flags);

	list_add(&dev->list, &clockevent_devices);
	clockevents_do_notify(CLOCK_EVT_NOTIFY_ADD, dev);
	clockevents_notify_released();

	raw_spin_unlock_irqrestore(&clockevents_lock, flags);
}
EXPORT_SYMBOL_GPL(clockevents_register_device);

/*
 * Noop handler when we shut down an event device
 */
void clockevents_handle_noop(struct clock_event_device *dev)
{
}

/**
 * clockevents_exchange_device - release and request clock devices
 * @old:	device to release (can be NULL)
 * @new:	device to request (can be NULL)
 *
 * Called from the notifier chain. clockevents_lock is held already
 */
void clockevents_exchange_device(struct clock_event_device *old,
				 struct clock_event_device *new)
{
	unsigned long flags;

	local_irq_save(flags);
	/*
	 * Caller releases a clock event device. We queue it into the
	 * released list and do a notify add later.
	 */
	if (old) {
		clockevents_set_mode(old, CLOCK_EVT_MODE_UNUSED);
		list_del(&old->list);
		list_add(&old->list, &clockevents_released);
	}

	if (new) {
		BUG_ON(new->mode != CLOCK_EVT_MODE_UNUSED);
		clockevents_shutdown(new);
	}
	local_irq_restore(flags);
}

#ifdef CONFIG_GENERIC_CLOCKEVENTS
/**
 * clockevents_notify - notification about relevant events
 */
void clockevents_notify(unsigned long reason, void *arg)
{
	struct clock_event_device *dev, *tmp;
	unsigned long flags;
	int cpu;

	raw_spin_lock_irqsave(&clockevents_lock, flags);
	clockevents_do_notify(reason, arg);

	switch (reason) {
	case CLOCK_EVT_NOTIFY_CPU_DEAD:
		/*
		 * Unregister the clock event devices which were
		 * released from the users in the notify chain.
		 */
		list_for_each_entry_safe(dev, tmp, &clockevents_released, list)
			list_del(&dev->list);
		/*
		 * Now check whether the CPU has left unused per cpu devices
		 */
		cpu = *((int *)arg);
		list_for_each_entry_safe(dev, tmp, &clockevent_devices, list) {
			if (cpumask_test_cpu(cpu, dev->cpumask) &&
			    cpumask_weight(dev->cpumask) == 1 &&
			    !tick_is_broadcast_device(dev)) {
				BUG_ON(dev->mode != CLOCK_EVT_MODE_UNUSED);
				list_del(&dev->list);
			}
		}
		break;
	default:
		break;
	}
	raw_spin_unlock_irqrestore(&clockevents_lock, flags);
}
EXPORT_SYMBOL_GPL(clockevents_notify);
#endif
