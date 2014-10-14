/*
 * zfcp device driver
 *
 * Setup and helper functions to access QDIO.
 *
 * Copyright IBM Corporation 2002, 2010
 */

#define KMSG_COMPONENT "zfcp"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/slab.h>
#include "zfcp_ext.h"
#include "zfcp_qdio.h"

#define QBUFF_PER_PAGE		(PAGE_SIZE / sizeof(struct qdio_buffer))

static int zfcp_qdio_buffers_enqueue(struct qdio_buffer **sbal)
{
	int pos;

	for (pos = 0; pos < QDIO_MAX_BUFFERS_PER_Q; pos += QBUFF_PER_PAGE) {
		sbal[pos] = (struct qdio_buffer *) get_zeroed_page(GFP_KERNEL);
		if (!sbal[pos])
			return -ENOMEM;
	}
	for (pos = 0; pos < QDIO_MAX_BUFFERS_PER_Q; pos++)
		if (pos % QBUFF_PER_PAGE)
			sbal[pos] = sbal[pos - 1] + 1;
	return 0;
}

static void zfcp_qdio_handler_error(struct zfcp_qdio *qdio, char *id,
				    unsigned int qdio_err)
{
	struct zfcp_adapter *adapter = qdio->adapter;

	dev_warn(&adapter->ccw_device->dev, "A QDIO problem occurred\n");

	if (qdio_err & QDIO_ERROR_SLSB_STATE)
		zfcp_qdio_siosl(adapter);
	zfcp_erp_adapter_reopen(adapter,
				ZFCP_STATUS_ADAPTER_LINK_UNPLUGGED |
				ZFCP_STATUS_COMMON_ERP_FAILED, id);
}

static void zfcp_qdio_zero_sbals(struct qdio_buffer *sbal[], int first, int cnt)
{
	int i, sbal_idx;

	for (i = first; i < first + cnt; i++) {
		sbal_idx = i % QDIO_MAX_BUFFERS_PER_Q;
		memset(sbal[sbal_idx], 0, sizeof(struct qdio_buffer));
	}
}

/* this needs to be called prior to updating the queue fill level */
static inline void zfcp_qdio_account(struct zfcp_qdio *qdio)
{
	unsigned long long now, span;
	int used;

	now = get_clock_monotonic();
	span = (now - qdio->req_q_time) >> 12;
	used = QDIO_MAX_BUFFERS_PER_Q - atomic_read(&qdio->req_q_free);
	qdio->req_q_util += used * span;
	qdio->req_q_time = now;
}

static void zfcp_qdio_int_req(struct ccw_device *cdev, unsigned int qdio_err,
			      int queue_no, int idx, int count,
			      unsigned long parm)
{
	struct zfcp_qdio *qdio = (struct zfcp_qdio *) parm;

	if (unlikely(qdio_err)) {
		zfcp_qdio_handler_error(qdio, "qdireq1", qdio_err);
		return;
	}

	/* cleanup all SBALs being program-owned now */
	zfcp_qdio_zero_sbals(qdio->req_q, idx, count);

	spin_lock_irq(&qdio->stat_lock);
	zfcp_qdio_account(qdio);
	spin_unlock_irq(&qdio->stat_lock);
	atomic_add(count, &qdio->req_q_free);
	wake_up(&qdio->req_q_wq);
}

static void zfcp_qdio_int_resp(struct ccw_device *cdev, unsigned int qdio_err,
			       int queue_no, int idx, int count,
			       unsigned long parm)
{
	struct zfcp_qdio *qdio = (struct zfcp_qdio *) parm;
	int sbal_idx, sbal_no;

	if (unlikely(qdio_err)) {
		zfcp_qdio_handler_error(qdio, "qdires1", qdio_err);
		return;
	}

	/*
	 * go through all SBALs from input queue currently
	 * returned by QDIO layer
	 */
	for (sbal_no = 0; sbal_no < count; sbal_no++) {
		sbal_idx = (idx + sbal_no) % QDIO_MAX_BUFFERS_PER_Q;
		/* go through all SBALEs of SBAL */
		zfcp_fsf_reqid_check(qdio, sbal_idx);
	}

	/*
	 * put SBALs back to response queue
	 */
	if (do_QDIO(cdev, QDIO_FLAG_SYNC_INPUT, 0, idx, count))
		zfcp_erp_adapter_reopen(qdio->adapter, 0, "qdires2");
}

static struct qdio_buffer_element *
zfcp_qdio_sbal_chain(struct zfcp_qdio *qdio, struct zfcp_qdio_req *q_req)
{
	struct qdio_buffer_element *sbale;

	/* set last entry flag in current SBALE of current SBAL */
	sbale = zfcp_qdio_sbale_curr(qdio, q_req);
	sbale->flags |= SBAL_FLAGS_LAST_ENTRY;

	/* don't exceed last allowed SBAL */
	if (q_req->sbal_last == q_req->sbal_limit)
		return NULL;

	/* set chaining flag in first SBALE of current SBAL */
	sbale = zfcp_qdio_sbale_req(qdio, q_req);
	sbale->flags |= SBAL_FLAGS0_MORE_SBALS;

	/* calculate index of next SBAL */
	q_req->sbal_last++;
	q_req->sbal_last %= QDIO_MAX_BUFFERS_PER_Q;

	/* keep this requests number of SBALs up-to-date */
	q_req->sbal_number++;
	BUG_ON(q_req->sbal_number > ZFCP_QDIO_MAX_SBALS_PER_REQ);

	/* start at first SBALE of new SBAL */
	q_req->sbale_curr = 0;

	/* set storage-block type for new SBAL */
	sbale = zfcp_qdio_sbale_curr(qdio, q_req);
	sbale->flags |= q_req->sbtype;

	return sbale;
}

static struct qdio_buffer_element *
zfcp_qdio_sbale_next(struct zfcp_qdio *qdio, struct zfcp_qdio_req *q_req)
{
	if (q_req->sbale_curr == ZFCP_QDIO_LAST_SBALE_PER_SBAL)
		return zfcp_qdio_sbal_chain(qdio, q_req);
	q_req->sbale_curr++;
	return zfcp_qdio_sbale_curr(qdio, q_req);
}

/**
 * zfcp_qdio_sbals_from_sg - fill SBALs from scatter-gather list
 * @qdio: pointer to struct zfcp_qdio
 * @q_req: pointer to struct zfcp_qdio_req
 * @sg: scatter-gather list
 * @max_sbals: upper bound for number of SBALs to be used
 * Returns: number of bytes, or error (negativ)
 */
int zfcp_qdio_sbals_from_sg(struct zfcp_qdio *qdio, struct zfcp_qdio_req *q_req,
			    struct scatterlist *sg)
{
	struct qdio_buffer_element *sbale;
	int bytes = 0;

	/* set storage-block type for this request */
	sbale = zfcp_qdio_sbale_req(qdio, q_req);
	sbale->flags |= q_req->sbtype;

	for (; sg; sg = sg_next(sg)) {
		sbale = zfcp_qdio_sbale_next(qdio, q_req);
		if (!sbale) {
			atomic_inc(&qdio->req_q_full);
			zfcp_qdio_zero_sbals(qdio->req_q, q_req->sbal_first,
					     q_req->sbal_number);
			return -EINVAL;
		}

		sbale->addr = sg_virt(sg);
		sbale->length = sg->length;

		bytes += sg->length;
	}

	return bytes;
}

static int zfcp_qdio_sbal_check(struct zfcp_qdio *qdio)
{
	spin_lock_irq(&qdio->req_q_lock);
	if (atomic_read(&qdio->req_q_free) ||
	    !(atomic_read(&qdio->adapter->status) & ZFCP_STATUS_ADAPTER_QDIOUP))
		return 1;
	spin_unlock_irq(&qdio->req_q_lock);
	return 0;
}

/**
 * zfcp_qdio_sbal_get - get free sbal in request queue, wait if necessary
 * @qdio: pointer to struct zfcp_qdio
 *
 * The req_q_lock must be held by the caller of this function, and
 * this function may only be called from process context; it will
 * sleep when waiting for a free sbal.
 *
 * Returns: 0 on success, -EIO if there is no free sbal after waiting.
 */
int zfcp_qdio_sbal_get(struct zfcp_qdio *qdio)
{
	long ret;

	spin_unlock_irq(&qdio->req_q_lock);
	ret = wait_event_interruptible_timeout(qdio->req_q_wq,
			       zfcp_qdio_sbal_check(qdio), 5 * HZ);

	if (!(atomic_read(&qdio->adapter->status) & ZFCP_STATUS_ADAPTER_QDIOUP))
		return -EIO;

	if (ret > 0)
		return 0;

	if (!ret) {
		atomic_inc(&qdio->req_q_full);
		/* assume hanging outbound queue, try queue recovery */
		zfcp_erp_adapter_reopen(qdio->adapter, 0, "qdsbg_1");
	}

	spin_lock_irq(&qdio->req_q_lock);
	return -EIO;
}

/**
 * zfcp_qdio_send - set PCI flag in first SBALE and send req to QDIO
 * @qdio: pointer to struct zfcp_qdio
 * @q_req: pointer to struct zfcp_qdio_req
 * Returns: 0 on success, error otherwise
 */
int zfcp_qdio_send(struct zfcp_qdio *qdio, struct zfcp_qdio_req *q_req)
{
	int retval;
	u8 sbal_number = q_req->sbal_number;

	spin_lock(&qdio->stat_lock);
	zfcp_qdio_account(qdio);
	spin_unlock(&qdio->stat_lock);

	retval = do_QDIO(qdio->adapter->ccw_device, QDIO_FLAG_SYNC_OUTPUT, 0,
			 q_req->sbal_first, sbal_number);

	if (unlikely(retval)) {
		zfcp_qdio_zero_sbals(qdio->req_q, q_req->sbal_first,
				     sbal_number);
		return retval;
	}

	/* account for transferred buffers */
	atomic_sub(sbal_number, &qdio->req_q_free);
	qdio->req_q_idx += sbal_number;
	qdio->req_q_idx %= QDIO_MAX_BUFFERS_PER_Q;

	return 0;
}


static void zfcp_qdio_setup_init_data(struct qdio_initialize *id,
				      struct zfcp_qdio *qdio)
{
	memset(id, 0, sizeof(*id));
	id->cdev = qdio->adapter->ccw_device;
	id->q_format = QDIO_ZFCP_QFMT;
	memcpy(id->adapter_name, dev_name(&id->cdev->dev), 8);
	ASCEBC(id->adapter_name, 8);
	id->qib_rflags = QIB_RFLAGS_ENABLE_DATA_DIV;
	id->no_input_qs = 1;
	id->no_output_qs = 1;
	id->input_handler = zfcp_qdio_int_resp;
	id->output_handler = zfcp_qdio_int_req;
	id->int_parm = (unsigned long) qdio;
	id->input_sbal_addr_array = (void **) (qdio->res_q);
	id->output_sbal_addr_array = (void **) (qdio->req_q);
	id->scan_threshold =
		QDIO_MAX_BUFFERS_PER_Q - ZFCP_QDIO_MAX_SBALS_PER_REQ * 2;
}

/**
 * zfcp_qdio_allocate - allocate queue memory and initialize QDIO data
 * @adapter: pointer to struct zfcp_adapter
 * Returns: -ENOMEM on memory allocation error or return value from
 *          qdio_allocate
 */
static int zfcp_qdio_allocate(struct zfcp_qdio *qdio)
{
	struct qdio_initialize init_data;

	if (zfcp_qdio_buffers_enqueue(qdio->req_q) ||
	    zfcp_qdio_buffers_enqueue(qdio->res_q))
		return -ENOMEM;

	zfcp_qdio_setup_init_data(&init_data, qdio);
	init_waitqueue_head(&qdio->req_q_wq);

	return qdio_allocate(&init_data);
}

/**
 * zfcp_close_qdio - close qdio queues for an adapter
 * @qdio: pointer to structure zfcp_qdio
 */
void zfcp_qdio_close(struct zfcp_qdio *qdio)
{
	struct zfcp_adapter *adapter = qdio->adapter;
	int idx, count;

	if (!(atomic_read(&adapter->status) & ZFCP_STATUS_ADAPTER_QDIOUP))
		return;

	/* clear QDIOUP flag, thus do_QDIO is not called during qdio_shutdown */
	spin_lock_irq(&qdio->req_q_lock);
	atomic_clear_mask(ZFCP_STATUS_ADAPTER_QDIOUP, &adapter->status);
	spin_unlock_irq(&qdio->req_q_lock);

	wake_up(&qdio->req_q_wq);

	qdio_shutdown(adapter->ccw_device, QDIO_FLAG_CLEANUP_USING_CLEAR);

	/* cleanup used outbound sbals */
	count = atomic_read(&qdio->req_q_free);
	if (count < QDIO_MAX_BUFFERS_PER_Q) {
		idx = (qdio->req_q_idx + count) % QDIO_MAX_BUFFERS_PER_Q;
		count = QDIO_MAX_BUFFERS_PER_Q - count;
		zfcp_qdio_zero_sbals(qdio->req_q, idx, count);
	}
	qdio->req_q_idx = 0;
	atomic_set(&qdio->req_q_free, 0);
}

/**
 * zfcp_qdio_open - prepare and initialize response queue
 * @qdio: pointer to struct zfcp_qdio
 * Returns: 0 on success, otherwise -EIO
 */
int zfcp_qdio_open(struct zfcp_qdio *qdio)
{
	struct qdio_buffer_element *sbale;
	struct qdio_initialize init_data;
	struct zfcp_adapter *adapter = qdio->adapter;
	struct ccw_device *cdev = adapter->ccw_device;
	struct qdio_ssqd_desc ssqd;
	int cc;

	if (atomic_read(&adapter->status) & ZFCP_STATUS_ADAPTER_QDIOUP)
		return -EIO;

	atomic_clear_mask(ZFCP_STATUS_ADAPTER_SIOSL_ISSUED,
			  &qdio->adapter->status);

	zfcp_qdio_setup_init_data(&init_data, qdio);

	if (qdio_establish(&init_data))
		goto failed_establish;

	if (qdio_get_ssqd_desc(init_data.cdev, &ssqd))
		goto failed_qdio;

	if (ssqd.qdioac2 & CHSC_AC2_DATA_DIV_ENABLED)
		atomic_set_mask(ZFCP_STATUS_ADAPTER_DATA_DIV_ENABLED,
				&qdio->adapter->status);

	if (qdio_activate(cdev))
		goto failed_qdio;

	for (cc = 0; cc < QDIO_MAX_BUFFERS_PER_Q; cc++) {
		sbale = &(qdio->res_q[cc]->element[0]);
		sbale->length = 0;
		sbale->flags = SBAL_FLAGS_LAST_ENTRY;
		sbale->addr = NULL;
	}

	if (do_QDIO(cdev, QDIO_FLAG_SYNC_INPUT, 0, 0, QDIO_MAX_BUFFERS_PER_Q))
		goto failed_qdio;

	/* set index of first available SBALS / number of available SBALS */
	qdio->req_q_idx = 0;
	atomic_set(&qdio->req_q_free, QDIO_MAX_BUFFERS_PER_Q);
	atomic_set_mask(ZFCP_STATUS_ADAPTER_QDIOUP, &qdio->adapter->status);

	return 0;

failed_qdio:
	qdio_shutdown(cdev, QDIO_FLAG_CLEANUP_USING_CLEAR);
failed_establish:
	dev_err(&cdev->dev,
		"Setting up the QDIO connection to the FCP adapter failed\n");
	return -EIO;
}

void zfcp_qdio_destroy(struct zfcp_qdio *qdio)
{
	int p;

	if (!qdio)
		return;

	if (qdio->adapter->ccw_device)
		qdio_free(qdio->adapter->ccw_device);

	for (p = 0; p < QDIO_MAX_BUFFERS_PER_Q; p += QBUFF_PER_PAGE) {
		free_page((unsigned long) qdio->req_q[p]);
		free_page((unsigned long) qdio->res_q[p]);
	}

	kfree(qdio);
}

int zfcp_qdio_setup(struct zfcp_adapter *adapter)
{
	struct zfcp_qdio *qdio;

	qdio = kzalloc(sizeof(struct zfcp_qdio), GFP_KERNEL);
	if (!qdio)
		return -ENOMEM;

	qdio->adapter = adapter;

	if (zfcp_qdio_allocate(qdio)) {
		zfcp_qdio_destroy(qdio);
		return -ENOMEM;
	}

	spin_lock_init(&qdio->req_q_lock);
	spin_lock_init(&qdio->stat_lock);

	adapter->qdio = qdio;
	return 0;
}

/**
 * zfcp_qdio_siosl - Trigger logging in FCP channel
 * @adapter: The zfcp_adapter where to trigger logging
 *
 * Call the cio siosl function to trigger hardware logging.  This
 * wrapper function sets a flag to ensure hardware logging is only
 * triggered once before going through qdio shutdown.
 *
 * The triggers are always run from qdio tasklet context, so no
 * additional synchronization is necessary.
 */
void zfcp_qdio_siosl(struct zfcp_adapter *adapter)
{
	int rc;

	if (atomic_read(&adapter->status) & ZFCP_STATUS_ADAPTER_SIOSL_ISSUED)
		return;

	rc = ccw_device_siosl(adapter->ccw_device);
	if (!rc)
		atomic_set_mask(ZFCP_STATUS_ADAPTER_SIOSL_ISSUED,
				&adapter->status);
}
