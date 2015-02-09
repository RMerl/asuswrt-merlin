/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
/*
 * linux/kernel/irq/chip.c
 *
 * Copyright (C) 1992, 1998-2006 Linus Torvalds, Ingo Molnar
 * Copyright (C) 2005-2006, Thomas Gleixner, Russell King
 *
 * This file contains the core interrupt handling code, for irq-chip
 * based architectures.
 *
 * Detailed information is available in Documentation/DocBook/genericirq
 */

#include <linux/irq.h>
#include <linux/msi.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>

#include "internals.h"

#include <typedefs.h>
#include <bcmdefs.h>

static void dynamic_irq_init_x(unsigned int irq, bool keep_chip_data)
{
	struct irq_desc *desc;
	unsigned long flags;

	desc = irq_to_desc(irq);
	if (!desc) {
		WARN(1, KERN_ERR "Trying to initialize invalid IRQ%d\n", irq);
		return;
	}

	/* Ensure we don't have left over values from a previous use of this irq */
	raw_spin_lock_irqsave(&desc->lock, flags);
	desc->status = IRQ_DISABLED;
	desc->chip = &no_irq_chip;
	desc->handle_irq = handle_bad_irq;
	desc->depth = 1;
	desc->msi_desc = NULL;
	desc->handler_data = NULL;
	if (!keep_chip_data)
		desc->chip_data = NULL;
	desc->action = NULL;
	desc->irq_count = 0;
	desc->irqs_unhandled = 0;
#ifdef CONFIG_SMP
	cpumask_setall(desc->affinity);
#ifdef CONFIG_GENERIC_PENDING_IRQ
	cpumask_clear(desc->pending_mask);
#endif
#endif
	raw_spin_unlock_irqrestore(&desc->lock, flags);
}

/**
 *	dynamic_irq_init - initialize a dynamically allocated irq
 *	@irq:	irq number to initialize
 */
void dynamic_irq_init(unsigned int irq)
{
	dynamic_irq_init_x(irq, false);
}

/**
 *	dynamic_irq_init_keep_chip_data - initialize a dynamically allocated irq
 *	@irq:	irq number to initialize
 *
 *	does not set irq_to_desc(irq)->chip_data to NULL
 */
void dynamic_irq_init_keep_chip_data(unsigned int irq)
{
	dynamic_irq_init_x(irq, true);
}

static void dynamic_irq_cleanup_x(unsigned int irq, bool keep_chip_data)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		WARN(1, KERN_ERR "Trying to cleanup invalid IRQ%d\n", irq);
		return;
	}

	raw_spin_lock_irqsave(&desc->lock, flags);
	if (desc->action) {
		raw_spin_unlock_irqrestore(&desc->lock, flags);
		WARN(1, KERN_ERR "Destroying IRQ%d without calling free_irq\n",
			irq);
		return;
	}
	desc->msi_desc = NULL;
	desc->handler_data = NULL;
	if (!keep_chip_data)
		desc->chip_data = NULL;
	desc->handle_irq = handle_bad_irq;
	desc->chip = &no_irq_chip;
	desc->name = NULL;
	clear_kstat_irqs(desc);
	raw_spin_unlock_irqrestore(&desc->lock, flags);
}

/**
 *	dynamic_irq_cleanup - cleanup a dynamically allocated irq
 *	@irq:	irq number to initialize
 */
void dynamic_irq_cleanup(unsigned int irq)
{
	dynamic_irq_cleanup_x(irq, false);
}

/**
 *	dynamic_irq_cleanup_keep_chip_data - cleanup a dynamically allocated irq
 *	@irq:	irq number to initialize
 *
 *	does not set irq_to_desc(irq)->chip_data to NULL
 */
void dynamic_irq_cleanup_keep_chip_data(unsigned int irq)
{
	dynamic_irq_cleanup_x(irq, true);
}


/**
 *	set_irq_chip - set the irq chip for an irq
 *	@irq:	irq number
 *	@chip:	pointer to irq chip description structure
 */
int set_irq_chip(unsigned int irq, struct irq_chip *chip)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		WARN(1, KERN_ERR "Trying to install chip for IRQ%d\n", irq);
		return -EINVAL;
	}

	if (!chip)
		chip = &no_irq_chip;

	raw_spin_lock_irqsave(&desc->lock, flags);
	irq_chip_set_defaults(chip);
	desc->chip = chip;
	raw_spin_unlock_irqrestore(&desc->lock, flags);

	return 0;
}
EXPORT_SYMBOL(set_irq_chip);

/**
 *	set_irq_type - set the irq trigger type for an irq
 *	@irq:	irq number
 *	@type:	IRQ_TYPE_{LEVEL,EDGE}_* value - see include/linux/irq.h
 */
int set_irq_type(unsigned int irq, unsigned int type)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;
	int ret = -ENXIO;

	if (!desc) {
		printk(KERN_ERR "Trying to set irq type for IRQ%d\n", irq);
		return -ENODEV;
	}

	type &= IRQ_TYPE_SENSE_MASK;
	if (type == IRQ_TYPE_NONE)
		return 0;

	raw_spin_lock_irqsave(&desc->lock, flags);
	ret = __irq_set_trigger(desc, irq, type);
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	return ret;
}
EXPORT_SYMBOL(set_irq_type);

/**
 *	set_irq_data - set irq type data for an irq
 *	@irq:	Interrupt number
 *	@data:	Pointer to interrupt specific data
 *
 *	Set the hardware irq controller data for an irq
 */
int set_irq_data(unsigned int irq, void *data)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		printk(KERN_ERR
		       "Trying to install controller data for IRQ%d\n", irq);
		return -EINVAL;
	}

	raw_spin_lock_irqsave(&desc->lock, flags);
	desc->handler_data = data;
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	return 0;
}
EXPORT_SYMBOL(set_irq_data);

/**
 *	set_irq_msi - set MSI descriptor data for an irq
 *	@irq:	Interrupt number
 *	@entry:	Pointer to MSI descriptor data
 *
 *	Set the MSI descriptor entry for an irq
 */
int set_irq_msi(unsigned int irq, struct msi_desc *entry)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		printk(KERN_ERR
		       "Trying to install msi data for IRQ%d\n", irq);
		return -EINVAL;
	}

	raw_spin_lock_irqsave(&desc->lock, flags);
	desc->msi_desc = entry;
	if (entry)
		entry->irq = irq;
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	return 0;
}

/**
 *	set_irq_chip_data - set irq chip data for an irq
 *	@irq:	Interrupt number
 *	@data:	Pointer to chip specific data
 *
 *	Set the hardware irq chip data for an irq
 */
int set_irq_chip_data(unsigned int irq, void *data)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		printk(KERN_ERR
		       "Trying to install chip data for IRQ%d\n", irq);
		return -EINVAL;
	}

	if (!desc->chip) {
		printk(KERN_ERR "BUG: bad set_irq_chip_data(IRQ#%d)\n", irq);
		return -EINVAL;
	}

	raw_spin_lock_irqsave(&desc->lock, flags);
	desc->chip_data = data;
	raw_spin_unlock_irqrestore(&desc->lock, flags);

	return 0;
}
EXPORT_SYMBOL(set_irq_chip_data);

/**
 *	set_irq_nested_thread - Set/Reset the IRQ_NESTED_THREAD flag of an irq
 *
 *	@irq:	Interrupt number
 *	@nest:	0 to clear / 1 to set the IRQ_NESTED_THREAD flag
 *
 *	The IRQ_NESTED_THREAD flag indicates that on
 *	request_threaded_irq() no separate interrupt thread should be
 *	created for the irq as the handler are called nested in the
 *	context of a demultiplexing interrupt handler thread.
 */
void set_irq_nested_thread(unsigned int irq, int nest)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc)
		return;

	raw_spin_lock_irqsave(&desc->lock, flags);
	if (nest)
		desc->status |= IRQ_NESTED_THREAD;
	else
		desc->status &= ~IRQ_NESTED_THREAD;
	raw_spin_unlock_irqrestore(&desc->lock, flags);
}
EXPORT_SYMBOL_GPL(set_irq_nested_thread);

/*
 * default enable function
 */
static void default_enable(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	desc->chip->unmask(irq);
	desc->status &= ~IRQ_MASKED;
}

/*
 * default disable function
 */
static void default_disable(unsigned int irq)
{
}

/*
 * default startup function
 */
static unsigned int default_startup(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	desc->chip->enable(irq);
	return 0;
}

/*
 * default shutdown function
 */
static void default_shutdown(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	desc->chip->mask(irq);
	desc->status |= IRQ_MASKED;
}

/*
 * Fixup enable/disable function pointers
 */
void irq_chip_set_defaults(struct irq_chip *chip)
{
	if (!chip->enable)
		chip->enable = default_enable;
	if (!chip->disable)
		chip->disable = default_disable;
	if (!chip->startup)
		chip->startup = default_startup;
	/*
	 * We use chip->disable, when the user provided its own. When
	 * we have default_disable set for chip->disable, then we need
	 * to use default_shutdown, otherwise the irq line is not
	 * disabled on free_irq():
	 */
	if (!chip->shutdown)
		chip->shutdown = chip->disable != default_disable ?
			chip->disable : default_shutdown;
	if (!chip->name)
		chip->name = chip->typename;
	if (!chip->end)
		chip->end = dummy_irq_chip.end;
}

static inline void mask_ack_irq(struct irq_desc *desc, int irq)
{
	if (desc->chip->mask_ack)
		desc->chip->mask_ack(irq);
	else {
		desc->chip->mask(irq);
		if (desc->chip->ack)
			desc->chip->ack(irq);
	}
	desc->status |= IRQ_MASKED;
}

static inline void mask_irq(struct irq_desc *desc, int irq)
{
	if (desc->chip->mask) {
		desc->chip->mask(irq);
		desc->status |= IRQ_MASKED;
	}
}

static inline void unmask_irq(struct irq_desc *desc, int irq)
{
	if (desc->chip->unmask) {
		desc->chip->unmask(irq);
		desc->status &= ~IRQ_MASKED;
	}
}

/*
 *	handle_nested_irq - Handle a nested irq from a irq thread
 *	@irq:	the interrupt number
 *
 *	Handle interrupts which are nested into a threaded interrupt
 *	handler. The handler function is called inside the calling
 *	threads context.
 */
void handle_nested_irq(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
	struct irqaction *action;
	irqreturn_t action_ret;

	might_sleep();

	raw_spin_lock_irq(&desc->lock);

	kstat_incr_irqs_this_cpu(irq, desc);

	action = desc->action;
	if (unlikely(!action || (desc->status & IRQ_DISABLED)))
		goto out_unlock;

	desc->status |= IRQ_INPROGRESS;
	raw_spin_unlock_irq(&desc->lock);

	action_ret = action->thread_fn(action->irq, action->dev_id);
	if (!noirqdebug)
		note_interrupt(irq, desc, action_ret);

	raw_spin_lock_irq(&desc->lock);
	desc->status &= ~IRQ_INPROGRESS;

out_unlock:
	raw_spin_unlock_irq(&desc->lock);
}
EXPORT_SYMBOL_GPL(handle_nested_irq);

/**
 *	handle_simple_irq - Simple and software-decoded IRQs.
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Simple interrupts are either sent from a demultiplexing interrupt
 *	handler or come from hardware, where no interrupt hardware control
 *	is necessary.
 *
 *	Note: The caller is expected to handle the ack, clear, mask and
 *	unmask issues if necessary.
 */
void
handle_simple_irq(unsigned int irq, struct irq_desc *desc)
{
	struct irqaction *action;
	irqreturn_t action_ret;

	raw_spin_lock(&desc->lock);

	if (unlikely(desc->status & IRQ_INPROGRESS))
		goto out_unlock;
	desc->status &= ~(IRQ_REPLAY | IRQ_WAITING);
	kstat_incr_irqs_this_cpu(irq, desc);

	action = desc->action;
	if (unlikely(!action || (desc->status & IRQ_DISABLED)))
		goto out_unlock;

	desc->status |= IRQ_INPROGRESS;
	raw_spin_unlock(&desc->lock);

	action_ret = handle_IRQ_event(irq, action);
	if (!noirqdebug)
		note_interrupt(irq, desc, action_ret);

	raw_spin_lock(&desc->lock);
	desc->status &= ~IRQ_INPROGRESS;
out_unlock:
	raw_spin_unlock(&desc->lock);
}

/**
 *	handle_level_irq - Level type irq handler
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Level type interrupts are active as long as the hardware line has
 *	the active level. This may require to mask the interrupt and unmask
 *	it after the associated handler has acknowledged the device, so the
 *	interrupt line is back to inactive.
 */
void BCMFASTPATH
handle_level_irq(unsigned int irq, struct irq_desc *desc)
{
	struct irqaction *action;
	irqreturn_t action_ret;

	raw_spin_lock(&desc->lock);
	mask_ack_irq(desc, irq);

	if (unlikely(desc->status & IRQ_INPROGRESS))
		goto out_unlock;
	desc->status &= ~(IRQ_REPLAY | IRQ_WAITING);
	kstat_incr_irqs_this_cpu(irq, desc);

	/*
	 * If its disabled or no action available
	 * keep it masked and get out of here
	 */
	action = desc->action;
	if (unlikely(!action || (desc->status & IRQ_DISABLED)))
		goto out_unlock;

	desc->status |= IRQ_INPROGRESS;
	raw_spin_unlock(&desc->lock);

	action_ret = handle_IRQ_event(irq, action);
	if (!noirqdebug)
		note_interrupt(irq, desc, action_ret);

	raw_spin_lock(&desc->lock);
	desc->status &= ~IRQ_INPROGRESS;

	if (!(desc->status & (IRQ_DISABLED | IRQ_ONESHOT)))
		unmask_irq(desc, irq);
out_unlock:
	raw_spin_unlock(&desc->lock);
}
EXPORT_SYMBOL_GPL(handle_level_irq);

/**
 *	handle_fasteoi_irq - irq handler for transparent controllers
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Only a single callback will be issued to the chip: an ->eoi()
 *	call when the interrupt has been serviced. This enables support
 *	for modern forms of interrupt handlers, which handle the flow
 *	details in hardware, transparently.
 */
void
handle_fasteoi_irq(unsigned int irq, struct irq_desc *desc)
{
	struct irqaction *action;
	irqreturn_t action_ret;

	raw_spin_lock(&desc->lock);

	if (unlikely(desc->status & IRQ_INPROGRESS))
		goto out;

	desc->status &= ~(IRQ_REPLAY | IRQ_WAITING);
	kstat_incr_irqs_this_cpu(irq, desc);

	/*
	 * If its disabled or no action available
	 * then mask it and get out of here:
	 */
	action = desc->action;
	if (unlikely(!action || (desc->status & IRQ_DISABLED))) {
		desc->status |= IRQ_PENDING;
		mask_irq(desc, irq);
		goto out;
	}

	desc->status |= IRQ_INPROGRESS;
	desc->status &= ~IRQ_PENDING;
	raw_spin_unlock(&desc->lock);

	action_ret = handle_IRQ_event(irq, action);
	if (!noirqdebug)
		note_interrupt(irq, desc, action_ret);

	raw_spin_lock(&desc->lock);
	desc->status &= ~IRQ_INPROGRESS;
out:
	desc->chip->eoi(irq);

	raw_spin_unlock(&desc->lock);
}

/**
 *	handle_edge_irq - edge type IRQ handler
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Interrupt occures on the falling and/or rising edge of a hardware
 *	signal. The occurence is latched into the irq controller hardware
 *	and must be acked in order to be reenabled. After the ack another
 *	interrupt can happen on the same source even before the first one
 *	is handled by the associated event handler. If this happens it
 *	might be necessary to disable (mask) the interrupt depending on the
 *	controller hardware. This requires to reenable the interrupt inside
 *	of the loop which handles the interrupts which have arrived while
 *	the handler was running. If all pending interrupts are handled, the
 *	loop is left.
 */
void
handle_edge_irq(unsigned int irq, struct irq_desc *desc)
{
	raw_spin_lock(&desc->lock);

	desc->status &= ~(IRQ_REPLAY | IRQ_WAITING);

	/*
	 * If we're currently running this IRQ, or its disabled,
	 * we shouldn't process the IRQ. Mark it pending, handle
	 * the necessary masking and go out
	 */
	if (unlikely((desc->status & (IRQ_INPROGRESS | IRQ_DISABLED)) ||
		    !desc->action)) {
		desc->status |= (IRQ_PENDING | IRQ_MASKED);
		mask_ack_irq(desc, irq);
		goto out_unlock;
	}
	kstat_incr_irqs_this_cpu(irq, desc);

	/* Start handling the irq */
	if (desc->chip->ack)
		desc->chip->ack(irq);

	/* Mark the IRQ currently in progress.*/
	desc->status |= IRQ_INPROGRESS;

	do {
		struct irqaction *action = desc->action;
		irqreturn_t action_ret;

		if (unlikely(!action)) {
			mask_irq(desc, irq);
			goto out_unlock;
		}

		/*
		 * When another irq arrived while we were handling
		 * one, we could have masked the irq.
		 * Renable it, if it was not disabled in meantime.
		 */
		if (unlikely((desc->status &
			       (IRQ_PENDING | IRQ_MASKED | IRQ_DISABLED)) ==
			      (IRQ_PENDING | IRQ_MASKED))) {
			unmask_irq(desc, irq);
		}

		desc->status &= ~IRQ_PENDING;
		raw_spin_unlock(&desc->lock);
		action_ret = handle_IRQ_event(irq, action);
		if (!noirqdebug)
			note_interrupt(irq, desc, action_ret);
		raw_spin_lock(&desc->lock);

	} while ((desc->status & (IRQ_PENDING | IRQ_DISABLED)) == IRQ_PENDING);

	desc->status &= ~IRQ_INPROGRESS;
out_unlock:
	raw_spin_unlock(&desc->lock);
}

/**
 *	handle_percpu_irq - Per CPU local irq handler
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Per CPU interrupts on SMP machines without locking requirements
 */
void
handle_percpu_irq(unsigned int irq, struct irq_desc *desc)
{
	irqreturn_t action_ret;

	kstat_incr_irqs_this_cpu(irq, desc);

	if (desc->chip->ack)
		desc->chip->ack(irq);

	action_ret = handle_IRQ_event(irq, desc->action);
	if (!noirqdebug)
		note_interrupt(irq, desc, action_ret);

	if (desc->chip->eoi)
		desc->chip->eoi(irq);
}

void
__set_irq_handler(unsigned int irq, irq_flow_handler_t handle, int is_chained,
		  const char *name)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		printk(KERN_ERR
		       "Trying to install type control for IRQ%d\n", irq);
		return;
	}

	if (!handle)
		handle = handle_bad_irq;
	else if (desc->chip == &no_irq_chip) {
		printk(KERN_WARNING "Trying to install %sinterrupt handler "
		       "for IRQ%d\n", is_chained ? "chained " : "", irq);
		/*
		 * Some ARM implementations install a handler for really dumb
		 * interrupt hardware without setting an irq_chip. This worked
		 * with the ARM no_irq_chip but the check in setup_irq would
		 * prevent us to setup the interrupt at all. Switch it to
		 * dummy_irq_chip for easy transition.
		 */
		desc->chip = &dummy_irq_chip;
	}

	chip_bus_lock(irq, desc);
	raw_spin_lock_irqsave(&desc->lock, flags);

	/* Uninstall? */
	if (handle == handle_bad_irq) {
		if (desc->chip != &no_irq_chip)
			mask_ack_irq(desc, irq);
		desc->status |= IRQ_DISABLED;
		desc->depth = 1;
	}
	desc->handle_irq = handle;
	desc->name = name;

	if (handle != handle_bad_irq && is_chained) {
		desc->status &= ~IRQ_DISABLED;
		desc->status |= IRQ_NOREQUEST | IRQ_NOPROBE;
		desc->depth = 0;
		desc->chip->startup(irq);
	}
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	chip_bus_sync_unlock(irq, desc);
}
EXPORT_SYMBOL_GPL(__set_irq_handler);

void
set_irq_chip_and_handler(unsigned int irq, struct irq_chip *chip,
			 irq_flow_handler_t handle)
{
	set_irq_chip(irq, chip);
	__set_irq_handler(irq, handle, 0, NULL);
}

void
set_irq_chip_and_handler_name(unsigned int irq, struct irq_chip *chip,
			      irq_flow_handler_t handle, const char *name)
{
	set_irq_chip(irq, chip);
	__set_irq_handler(irq, handle, 0, name);
}

void set_irq_noprobe(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		printk(KERN_ERR "Trying to mark IRQ%d non-probeable\n", irq);
		return;
	}

	raw_spin_lock_irqsave(&desc->lock, flags);
	desc->status |= IRQ_NOPROBE;
	raw_spin_unlock_irqrestore(&desc->lock, flags);
}

void set_irq_probe(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		printk(KERN_ERR "Trying to mark IRQ%d probeable\n", irq);
		return;
	}

	raw_spin_lock_irqsave(&desc->lock, flags);
	desc->status &= ~IRQ_NOPROBE;
	raw_spin_unlock_irqrestore(&desc->lock, flags);
}
