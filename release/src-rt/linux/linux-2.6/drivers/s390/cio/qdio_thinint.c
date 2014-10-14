/*
 * linux/drivers/s390/cio/thinint_qdio.c
 *
 * Copyright 2000,2009 IBM Corp.
 * Author(s): Utz Bacher <utz.bacher@de.ibm.com>
 *	      Cornelia Huck <cornelia.huck@de.ibm.com>
 *	      Jan Glauber <jang@linux.vnet.ibm.com>
 */
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/kernel_stat.h>
#include <asm/atomic.h>
#include <asm/debug.h>
#include <asm/qdio.h>
#include <asm/airq.h>
#include <asm/isc.h>

#include "cio.h"
#include "ioasm.h"
#include "qdio.h"
#include "qdio_debug.h"

/*
 * Restriction: only 63 iqdio subchannels would have its own indicator,
 * after that, subsequent subchannels share one indicator
 */
#define TIQDIO_NR_NONSHARED_IND		63
#define TIQDIO_NR_INDICATORS		(TIQDIO_NR_NONSHARED_IND + 1)

/* list of thin interrupt input queues */
static LIST_HEAD(tiq_list);
DEFINE_MUTEX(tiq_list_lock);

/* adapter local summary indicator */
static u8 *tiqdio_alsi;

struct indicator_t *q_indicators;

static u64 last_ai_time;

/* returns addr for the device state change indicator */
static u32 *get_indicator(void)
{
	int i;

	for (i = 0; i < TIQDIO_NR_NONSHARED_IND; i++)
		if (!atomic_read(&q_indicators[i].count)) {
			atomic_set(&q_indicators[i].count, 1);
			return &q_indicators[i].ind;
		}

	/* use the shared indicator */
	atomic_inc(&q_indicators[TIQDIO_SHARED_IND].count);
	return &q_indicators[TIQDIO_SHARED_IND].ind;
}

static void put_indicator(u32 *addr)
{
	int i;

	if (!addr)
		return;
	i = ((unsigned long)addr - (unsigned long)q_indicators) /
		sizeof(struct indicator_t);
	atomic_dec(&q_indicators[i].count);
}

void tiqdio_add_input_queues(struct qdio_irq *irq_ptr)
{
	struct qdio_q *q;
	int i;

	mutex_lock(&tiq_list_lock);
	for_each_input_queue(irq_ptr, q, i)
		list_add_rcu(&q->entry, &tiq_list);
	mutex_unlock(&tiq_list_lock);
	xchg(irq_ptr->dsci, 1 << 7);
}

void tiqdio_remove_input_queues(struct qdio_irq *irq_ptr)
{
	struct qdio_q *q;
	int i;

	for (i = 0; i < irq_ptr->nr_input_qs; i++) {
		q = irq_ptr->input_qs[i];
		/* if establish triggered an error */
		if (!q || !q->entry.prev || !q->entry.next)
			continue;

		mutex_lock(&tiq_list_lock);
		list_del_rcu(&q->entry);
		mutex_unlock(&tiq_list_lock);
		synchronize_rcu();
	}
}

static inline u32 shared_ind_set(void)
{
	return q_indicators[TIQDIO_SHARED_IND].ind;
}

/**
 * tiqdio_thinint_handler - thin interrupt handler for qdio
 * @alsi: pointer to adapter local summary indicator
 * @data: NULL
 */
static void tiqdio_thinint_handler(void *alsi, void *data)
{
	u32 si_used = shared_ind_set();
	struct qdio_q *q;

	last_ai_time = S390_lowcore.int_clock;
	kstat_cpu(smp_processor_id()).irqs[IOINT_QAI]++;

	/* protect tiq_list entries, only changed in activate or shutdown */
	rcu_read_lock();

	/* check for work on all inbound thinint queues */
	list_for_each_entry_rcu(q, &tiq_list, entry) {

		/* only process queues from changed sets */
		if (unlikely(shared_ind(q->irq_ptr->dsci))) {
			if (!si_used)
				continue;
		} else if (!*q->irq_ptr->dsci)
			continue;

		if (q->u.in.queue_start_poll) {
			/* skip if polling is enabled or already in work */
			if (test_and_set_bit(QDIO_QUEUE_IRQS_DISABLED,
					     &q->u.in.queue_irq_state)) {
				qperf_inc(q, int_discarded);
				continue;
			}

			/* avoid dsci clear here, done after processing */
			q->u.in.queue_start_poll(q->irq_ptr->cdev, q->nr,
						 q->irq_ptr->int_parm);
		} else {
			/* only clear it if the indicator is non-shared */
			if (!shared_ind(q->irq_ptr->dsci))
				xchg(q->irq_ptr->dsci, 0);
			/*
			 * Call inbound processing but not directly
			 * since that could starve other thinint queues.
			 */
			tasklet_schedule(&q->tasklet);
		}
		qperf_inc(q, adapter_int);
	}
	rcu_read_unlock();

	/*
	 * If the shared indicator was used clear it now after all queues
	 * were processed.
	 */
	if (si_used && shared_ind_set())
		xchg(&q_indicators[TIQDIO_SHARED_IND].ind, 0);
}

static int set_subchannel_ind(struct qdio_irq *irq_ptr, int reset)
{
	struct scssc_area *scssc_area;
	int rc;

	scssc_area = (struct scssc_area *)irq_ptr->chsc_page;
	memset(scssc_area, 0, PAGE_SIZE);

	if (reset) {
		scssc_area->summary_indicator_addr = 0;
		scssc_area->subchannel_indicator_addr = 0;
	} else {
		scssc_area->summary_indicator_addr = virt_to_phys(tiqdio_alsi);
		scssc_area->subchannel_indicator_addr =
			virt_to_phys(irq_ptr->dsci);
	}

	scssc_area->request = (struct chsc_header) {
		.length = 0x0fe0,
		.code	= 0x0021,
	};
	scssc_area->operation_code = 0;
	scssc_area->ks = PAGE_DEFAULT_KEY >> 4;
	scssc_area->kc = PAGE_DEFAULT_KEY >> 4;
	scssc_area->isc = QDIO_AIRQ_ISC;
	scssc_area->schid = irq_ptr->schid;

	/* enable the time delay disablement facility */
	if (css_general_characteristics.aif_tdd)
		scssc_area->word_with_d_bit = 0x10000000;

	rc = chsc(scssc_area);
	if (rc)
		return -EIO;

	rc = chsc_error_from_response(scssc_area->response.code);
	if (rc) {
		DBF_ERROR("%4x SSI r:%4x", irq_ptr->schid.sch_no,
			  scssc_area->response.code);
		DBF_ERROR_HEX(&scssc_area->response, sizeof(void *));
		return rc;
	}

	DBF_EVENT("setscind");
	DBF_HEX(&scssc_area->summary_indicator_addr, sizeof(unsigned long));
	DBF_HEX(&scssc_area->subchannel_indicator_addr,	sizeof(unsigned long));
	return 0;
}

/* allocate non-shared indicators and shared indicator */
int __init tiqdio_allocate_memory(void)
{
	q_indicators = kzalloc(sizeof(struct indicator_t) * TIQDIO_NR_INDICATORS,
			     GFP_KERNEL);
	if (!q_indicators)
		return -ENOMEM;
	return 0;
}

void tiqdio_free_memory(void)
{
	kfree(q_indicators);
}

int __init tiqdio_register_thinints(void)
{
	isc_register(QDIO_AIRQ_ISC);
	tiqdio_alsi = s390_register_adapter_interrupt(&tiqdio_thinint_handler,
						      NULL, QDIO_AIRQ_ISC);
	if (IS_ERR(tiqdio_alsi)) {
		DBF_EVENT("RTI:%lx", PTR_ERR(tiqdio_alsi));
		tiqdio_alsi = NULL;
		isc_unregister(QDIO_AIRQ_ISC);
		return -ENOMEM;
	}
	return 0;
}

int qdio_establish_thinint(struct qdio_irq *irq_ptr)
{
	if (!is_thinint_irq(irq_ptr))
		return 0;
	return set_subchannel_ind(irq_ptr, 0);
}

void qdio_setup_thinint(struct qdio_irq *irq_ptr)
{
	if (!is_thinint_irq(irq_ptr))
		return;
	irq_ptr->dsci = get_indicator();
	DBF_HEX(&irq_ptr->dsci, sizeof(void *));
}

void qdio_shutdown_thinint(struct qdio_irq *irq_ptr)
{
	if (!is_thinint_irq(irq_ptr))
		return;

	/* reset adapter interrupt indicators */
	set_subchannel_ind(irq_ptr, 1);
	put_indicator(irq_ptr->dsci);
}

void __exit tiqdio_unregister_thinints(void)
{
	WARN_ON(!list_empty(&tiq_list));

	if (tiqdio_alsi) {
		s390_unregister_adapter_interrupt(tiqdio_alsi, QDIO_AIRQ_ISC);
		isc_unregister(QDIO_AIRQ_ISC);
	}
}
