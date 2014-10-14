/*
 *  drivers/s390/cio/chsc.c
 *   S/390 common I/O routines -- channel subsystem call
 *
 *    Copyright IBM Corp. 1999,2010
 *    Author(s): Ingo Adlung (adlung@de.ibm.com)
 *		 Cornelia Huck (cornelia.huck@de.ibm.com)
 *		 Arnd Bergmann (arndb@de.ibm.com)
 */

#define KMSG_COMPONENT "cio"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>

#include <asm/cio.h>
#include <asm/chpid.h>
#include <asm/chsc.h>
#include <asm/crw.h>

#include "css.h"
#include "cio.h"
#include "cio_debug.h"
#include "ioasm.h"
#include "chp.h"
#include "chsc.h"

static void *sei_page;
static void *chsc_page;
static DEFINE_SPINLOCK(chsc_page_lock);

/**
 * chsc_error_from_response() - convert a chsc response to an error
 * @response: chsc response code
 *
 * Returns an appropriate Linux error code for @response.
 */
int chsc_error_from_response(int response)
{
	switch (response) {
	case 0x0001:
		return 0;
	case 0x0002:
	case 0x0003:
	case 0x0006:
	case 0x0007:
	case 0x0008:
	case 0x000a:
	case 0x0104:
		return -EINVAL;
	case 0x0004:
		return -EOPNOTSUPP;
	default:
		return -EIO;
	}
}
EXPORT_SYMBOL_GPL(chsc_error_from_response);

struct chsc_ssd_area {
	struct chsc_header request;
	u16 :10;
	u16 ssid:2;
	u16 :4;
	u16 f_sch;	  /* first subchannel */
	u16 :16;
	u16 l_sch;	  /* last subchannel */
	u32 :32;
	struct chsc_header response;
	u32 :32;
	u8 sch_valid : 1;
	u8 dev_valid : 1;
	u8 st	     : 3; /* subchannel type */
	u8 zeroes    : 3;
	u8  unit_addr;	  /* unit address */
	u16 devno;	  /* device number */
	u8 path_mask;
	u8 fla_valid_mask;
	u16 sch;	  /* subchannel */
	u8 chpid[8];	  /* chpids 0-7 */
	u16 fla[8];	  /* full link addresses 0-7 */
} __attribute__ ((packed));

int chsc_get_ssd_info(struct subchannel_id schid, struct chsc_ssd_info *ssd)
{
	struct chsc_ssd_area *ssd_area;
	int ccode;
	int ret;
	int i;
	int mask;

	spin_lock_irq(&chsc_page_lock);
	memset(chsc_page, 0, PAGE_SIZE);
	ssd_area = chsc_page;
	ssd_area->request.length = 0x0010;
	ssd_area->request.code = 0x0004;
	ssd_area->ssid = schid.ssid;
	ssd_area->f_sch = schid.sch_no;
	ssd_area->l_sch = schid.sch_no;

	ccode = chsc(ssd_area);
	/* Check response. */
	if (ccode > 0) {
		ret = (ccode == 3) ? -ENODEV : -EBUSY;
		goto out;
	}
	ret = chsc_error_from_response(ssd_area->response.code);
	if (ret != 0) {
		CIO_MSG_EVENT(2, "chsc: ssd failed for 0.%x.%04x (rc=%04x)\n",
			      schid.ssid, schid.sch_no,
			      ssd_area->response.code);
		goto out;
	}
	if (!ssd_area->sch_valid) {
		ret = -ENODEV;
		goto out;
	}
	/* Copy data */
	ret = 0;
	memset(ssd, 0, sizeof(struct chsc_ssd_info));
	if ((ssd_area->st != SUBCHANNEL_TYPE_IO) &&
	    (ssd_area->st != SUBCHANNEL_TYPE_MSG))
		goto out;
	ssd->path_mask = ssd_area->path_mask;
	ssd->fla_valid_mask = ssd_area->fla_valid_mask;
	for (i = 0; i < 8; i++) {
		mask = 0x80 >> i;
		if (ssd_area->path_mask & mask) {
			chp_id_init(&ssd->chpid[i]);
			ssd->chpid[i].id = ssd_area->chpid[i];
		}
		if (ssd_area->fla_valid_mask & mask)
			ssd->fla[i] = ssd_area->fla[i];
	}
out:
	spin_unlock_irq(&chsc_page_lock);
	return ret;
}

static int s390_subchannel_remove_chpid(struct subchannel *sch, void *data)
{
	spin_lock_irq(sch->lock);
	if (sch->driver && sch->driver->chp_event)
		if (sch->driver->chp_event(sch, data, CHP_OFFLINE) != 0)
			goto out_unreg;
	spin_unlock_irq(sch->lock);
	return 0;

out_unreg:
	sch->lpm = 0;
	spin_unlock_irq(sch->lock);
	css_schedule_eval(sch->schid);
	return 0;
}

void chsc_chp_offline(struct chp_id chpid)
{
	char dbf_txt[15];
	struct chp_link link;

	sprintf(dbf_txt, "chpr%x.%02x", chpid.cssid, chpid.id);
	CIO_TRACE_EVENT(2, dbf_txt);

	if (chp_get_status(chpid) <= 0)
		return;
	memset(&link, 0, sizeof(struct chp_link));
	link.chpid = chpid;
	/* Wait until previous actions have settled. */
	css_wait_for_slow_path();
	for_each_subchannel_staged(s390_subchannel_remove_chpid, NULL, &link);
}

static int s390_process_res_acc_new_sch(struct subchannel_id schid, void *data)
{
	struct schib schib;
	/*
	 * We don't know the device yet, but since a path
	 * may be available now to the device we'll have
	 * to do recognition again.
	 * Since we don't have any idea about which chpid
	 * that beast may be on we'll have to do a stsch
	 * on all devices, grr...
	 */
	if (stsch_err(schid, &schib))
		/* We're through */
		return -ENXIO;

	/* Put it on the slow path. */
	css_schedule_eval(schid);
	return 0;
}

static int __s390_process_res_acc(struct subchannel *sch, void *data)
{
	spin_lock_irq(sch->lock);
	if (sch->driver && sch->driver->chp_event)
		sch->driver->chp_event(sch, data, CHP_ONLINE);
	spin_unlock_irq(sch->lock);

	return 0;
}

static void s390_process_res_acc(struct chp_link *link)
{
	char dbf_txt[15];

	sprintf(dbf_txt, "accpr%x.%02x", link->chpid.cssid,
		link->chpid.id);
	CIO_TRACE_EVENT( 2, dbf_txt);
	if (link->fla != 0) {
		sprintf(dbf_txt, "fla%x", link->fla);
		CIO_TRACE_EVENT( 2, dbf_txt);
	}
	/* Wait until previous actions have settled. */
	css_wait_for_slow_path();
	/*
	 * I/O resources may have become accessible.
	 * Scan through all subchannels that may be concerned and
	 * do a validation on those.
	 * The more information we have (info), the less scanning
	 * will we have to do.
	 */
	for_each_subchannel_staged(__s390_process_res_acc,
				   s390_process_res_acc_new_sch, link);
}

static int
__get_chpid_from_lir(void *data)
{
	struct lir {
		u8  iq;
		u8  ic;
		u16 sci;
		/* incident-node descriptor */
		u32 indesc[28];
		/* attached-node descriptor */
		u32 andesc[28];
		/* incident-specific information */
		u32 isinfo[28];
	} __attribute__ ((packed)) *lir;

	lir = data;
	if (!(lir->iq&0x80))
		/* NULL link incident record */
		return -EINVAL;
	if (!(lir->indesc[0]&0xc0000000))
		/* node descriptor not valid */
		return -EINVAL;
	if (!(lir->indesc[0]&0x10000000))
		/* don't handle device-type nodes - FIXME */
		return -EINVAL;
	/* Byte 3 contains the chpid. Could also be CTCA, but we don't care */

	return (u16) (lir->indesc[0]&0x000000ff);
}

struct chsc_sei_area {
	struct chsc_header request;
	u32 reserved1;
	u32 reserved2;
	u32 reserved3;
	struct chsc_header response;
	u32 reserved4;
	u8  flags;
	u8  vf;		/* validity flags */
	u8  rs;		/* reporting source */
	u8  cc;		/* content code */
	u16 fla;	/* full link address */
	u16 rsid;	/* reporting source id */
	u32 reserved5;
	u32 reserved6;
	u8 ccdf[4096 - 16 - 24];	/* content-code dependent field */
	/* ccdf has to be big enough for a link-incident record */
} __attribute__ ((packed));

static void chsc_process_sei_link_incident(struct chsc_sei_area *sei_area)
{
	struct chp_id chpid;
	int id;

	CIO_CRW_EVENT(4, "chsc: link incident (rs=%02x, rs_id=%04x)\n",
		      sei_area->rs, sei_area->rsid);
	if (sei_area->rs != 4)
		return;
	id = __get_chpid_from_lir(sei_area->ccdf);
	if (id < 0)
		CIO_CRW_EVENT(4, "chsc: link incident - invalid LIR\n");
	else {
		chp_id_init(&chpid);
		chpid.id = id;
		chsc_chp_offline(chpid);
	}
}

static void chsc_process_sei_res_acc(struct chsc_sei_area *sei_area)
{
	struct chp_link link;
	struct chp_id chpid;
	int status;

	CIO_CRW_EVENT(4, "chsc: resource accessibility event (rs=%02x, "
		      "rs_id=%04x)\n", sei_area->rs, sei_area->rsid);
	if (sei_area->rs != 4)
		return;
	chp_id_init(&chpid);
	chpid.id = sei_area->rsid;
	/* allocate a new channel path structure, if needed */
	status = chp_get_status(chpid);
	if (status < 0)
		chp_new(chpid);
	else if (!status)
		return;
	memset(&link, 0, sizeof(struct chp_link));
	link.chpid = chpid;
	if ((sei_area->vf & 0xc0) != 0) {
		link.fla = sei_area->fla;
		if ((sei_area->vf & 0xc0) == 0xc0)
			/* full link address */
			link.fla_mask = 0xffff;
		else
			/* link address */
			link.fla_mask = 0xff00;
	}
	s390_process_res_acc(&link);
}

struct chp_config_data {
	u8 map[32];
	u8 op;
	u8 pc;
};

static void chsc_process_sei_chp_config(struct chsc_sei_area *sei_area)
{
	struct chp_config_data *data;
	struct chp_id chpid;
	int num;
	char *events[3] = {"configure", "deconfigure", "cancel deconfigure"};

	CIO_CRW_EVENT(4, "chsc: channel-path-configuration notification\n");
	if (sei_area->rs != 0)
		return;
	data = (struct chp_config_data *) &(sei_area->ccdf);
	chp_id_init(&chpid);
	for (num = 0; num <= __MAX_CHPID; num++) {
		if (!chp_test_bit(data->map, num))
			continue;
		chpid.id = num;
		pr_notice("Processing %s for channel path %x.%02x\n",
			  events[data->op], chpid.cssid, chpid.id);
		switch (data->op) {
		case 0:
			chp_cfg_schedule(chpid, 1);
			break;
		case 1:
			chp_cfg_schedule(chpid, 0);
			break;
		case 2:
			chp_cfg_cancel_deconfigure(chpid);
			break;
		}
	}
}

static void chsc_process_sei(struct chsc_sei_area *sei_area)
{
	/* Check if we might have lost some information. */
	if (sei_area->flags & 0x40) {
		CIO_CRW_EVENT(2, "chsc: event overflow\n");
		css_schedule_eval_all();
	}
	/* which kind of information was stored? */
	switch (sei_area->cc) {
	case 1: /* link incident*/
		chsc_process_sei_link_incident(sei_area);
		break;
	case 2: /* i/o resource accessibiliy */
		chsc_process_sei_res_acc(sei_area);
		break;
	case 8: /* channel-path-configuration notification */
		chsc_process_sei_chp_config(sei_area);
		break;
	default: /* other stuff */
		CIO_CRW_EVENT(4, "chsc: unhandled sei content code %d\n",
			      sei_area->cc);
		break;
	}
}

static void chsc_process_crw(struct crw *crw0, struct crw *crw1, int overflow)
{
	struct chsc_sei_area *sei_area;

	if (overflow) {
		css_schedule_eval_all();
		return;
	}
	CIO_CRW_EVENT(2, "CRW reports slct=%d, oflw=%d, "
		      "chn=%d, rsc=%X, anc=%d, erc=%X, rsid=%X\n",
		      crw0->slct, crw0->oflw, crw0->chn, crw0->rsc, crw0->anc,
		      crw0->erc, crw0->rsid);
	if (!sei_page)
		return;
	/* Access to sei_page is serialized through machine check handler
	 * thread, so no need for locking. */
	sei_area = sei_page;

	CIO_TRACE_EVENT(2, "prcss");
	do {
		memset(sei_area, 0, sizeof(*sei_area));
		sei_area->request.length = 0x0010;
		sei_area->request.code = 0x000e;
		if (chsc(sei_area))
			break;

		if (sei_area->response.code == 0x0001) {
			CIO_CRW_EVENT(4, "chsc: sei successful\n");
			chsc_process_sei(sei_area);
		} else {
			CIO_CRW_EVENT(2, "chsc: sei failed (rc=%04x)\n",
				      sei_area->response.code);
			break;
		}
	} while (sei_area->flags & 0x80);
}

void chsc_chp_online(struct chp_id chpid)
{
	char dbf_txt[15];
	struct chp_link link;

	sprintf(dbf_txt, "cadd%x.%02x", chpid.cssid, chpid.id);
	CIO_TRACE_EVENT(2, dbf_txt);

	if (chp_get_status(chpid) != 0) {
		memset(&link, 0, sizeof(struct chp_link));
		link.chpid = chpid;
		/* Wait until previous actions have settled. */
		css_wait_for_slow_path();
		for_each_subchannel_staged(__s390_process_res_acc, NULL,
					   &link);
	}
}

static void __s390_subchannel_vary_chpid(struct subchannel *sch,
					 struct chp_id chpid, int on)
{
	unsigned long flags;
	struct chp_link link;

	memset(&link, 0, sizeof(struct chp_link));
	link.chpid = chpid;
	spin_lock_irqsave(sch->lock, flags);
	if (sch->driver && sch->driver->chp_event)
		sch->driver->chp_event(sch, &link,
				       on ? CHP_VARY_ON : CHP_VARY_OFF);
	spin_unlock_irqrestore(sch->lock, flags);
}

static int s390_subchannel_vary_chpid_off(struct subchannel *sch, void *data)
{
	struct chp_id *chpid = data;

	__s390_subchannel_vary_chpid(sch, *chpid, 0);
	return 0;
}

static int s390_subchannel_vary_chpid_on(struct subchannel *sch, void *data)
{
	struct chp_id *chpid = data;

	__s390_subchannel_vary_chpid(sch, *chpid, 1);
	return 0;
}

static int
__s390_vary_chpid_on(struct subchannel_id schid, void *data)
{
	struct schib schib;

	if (stsch_err(schid, &schib))
		/* We're through */
		return -ENXIO;
	/* Put it on the slow path. */
	css_schedule_eval(schid);
	return 0;
}

/**
 * chsc_chp_vary - propagate channel-path vary operation to subchannels
 * @chpid: channl-path ID
 * @on: non-zero for vary online, zero for vary offline
 */
int chsc_chp_vary(struct chp_id chpid, int on)
{
	struct channel_path *chp = chpid_to_chp(chpid);
	struct chp_link link;

	memset(&link, 0, sizeof(struct chp_link));
	link.chpid = chpid;
	/* Wait until previous actions have settled. */
	css_wait_for_slow_path();
	/*
	 * Redo PathVerification on the devices the chpid connects to
	 */
	if (on) {
		/* Try to update the channel path descritor. */
		chsc_determine_base_channel_path_desc(chpid, &chp->desc);
		for_each_subchannel_staged(s390_subchannel_vary_chpid_on,
					   __s390_vary_chpid_on, &link);
	} else
		for_each_subchannel_staged(s390_subchannel_vary_chpid_off,
					   NULL, &link);

	return 0;
}

static void
chsc_remove_cmg_attr(struct channel_subsystem *css)
{
	int i;

	for (i = 0; i <= __MAX_CHPID; i++) {
		if (!css->chps[i])
			continue;
		chp_remove_cmg_attr(css->chps[i]);
	}
}

static int
chsc_add_cmg_attr(struct channel_subsystem *css)
{
	int i, ret;

	ret = 0;
	for (i = 0; i <= __MAX_CHPID; i++) {
		if (!css->chps[i])
			continue;
		ret = chp_add_cmg_attr(css->chps[i]);
		if (ret)
			goto cleanup;
	}
	return ret;
cleanup:
	for (--i; i >= 0; i--) {
		if (!css->chps[i])
			continue;
		chp_remove_cmg_attr(css->chps[i]);
	}
	return ret;
}

int __chsc_do_secm(struct channel_subsystem *css, int enable)
{
	struct {
		struct chsc_header request;
		u32 operation_code : 2;
		u32 : 30;
		u32 key : 4;
		u32 : 28;
		u32 zeroes1;
		u32 cub_addr1;
		u32 zeroes2;
		u32 cub_addr2;
		u32 reserved[13];
		struct chsc_header response;
		u32 status : 8;
		u32 : 4;
		u32 fmt : 4;
		u32 : 16;
	} __attribute__ ((packed)) *secm_area;
	int ret, ccode;

	spin_lock_irq(&chsc_page_lock);
	memset(chsc_page, 0, PAGE_SIZE);
	secm_area = chsc_page;
	secm_area->request.length = 0x0050;
	secm_area->request.code = 0x0016;

	secm_area->key = PAGE_DEFAULT_KEY >> 4;
	secm_area->cub_addr1 = (u64)(unsigned long)css->cub_addr1;
	secm_area->cub_addr2 = (u64)(unsigned long)css->cub_addr2;

	secm_area->operation_code = enable ? 0 : 1;

	ccode = chsc(secm_area);
	if (ccode > 0) {
		ret = (ccode == 3) ? -ENODEV : -EBUSY;
		goto out;
	}

	switch (secm_area->response.code) {
	case 0x0102:
	case 0x0103:
		ret = -EINVAL;
		break;
	default:
		ret = chsc_error_from_response(secm_area->response.code);
	}
	if (ret != 0)
		CIO_CRW_EVENT(2, "chsc: secm failed (rc=%04x)\n",
			      secm_area->response.code);
out:
	spin_unlock_irq(&chsc_page_lock);
	return ret;
}

int
chsc_secm(struct channel_subsystem *css, int enable)
{
	int ret;

	if (enable && !css->cm_enabled) {
		css->cub_addr1 = (void *)get_zeroed_page(GFP_KERNEL | GFP_DMA);
		css->cub_addr2 = (void *)get_zeroed_page(GFP_KERNEL | GFP_DMA);
		if (!css->cub_addr1 || !css->cub_addr2) {
			free_page((unsigned long)css->cub_addr1);
			free_page((unsigned long)css->cub_addr2);
			return -ENOMEM;
		}
	}
	ret = __chsc_do_secm(css, enable);
	if (!ret) {
		css->cm_enabled = enable;
		if (css->cm_enabled) {
			ret = chsc_add_cmg_attr(css);
			if (ret) {
				__chsc_do_secm(css, 0);
				css->cm_enabled = 0;
			}
		} else
			chsc_remove_cmg_attr(css);
	}
	if (!css->cm_enabled) {
		free_page((unsigned long)css->cub_addr1);
		free_page((unsigned long)css->cub_addr2);
	}
	return ret;
}

int chsc_determine_channel_path_desc(struct chp_id chpid, int fmt, int rfmt,
				     int c, int m, void *page)
{
	struct chsc_scpd *scpd_area;
	int ccode, ret;

	if ((rfmt == 1) && !css_general_characteristics.fcs)
		return -EINVAL;
	if ((rfmt == 2) && !css_general_characteristics.cib)
		return -EINVAL;

	memset(page, 0, PAGE_SIZE);
	scpd_area = page;
	scpd_area->request.length = 0x0010;
	scpd_area->request.code = 0x0002;
	scpd_area->cssid = chpid.cssid;
	scpd_area->first_chpid = chpid.id;
	scpd_area->last_chpid = chpid.id;
	scpd_area->m = m;
	scpd_area->c = c;
	scpd_area->fmt = fmt;
	scpd_area->rfmt = rfmt;

	ccode = chsc(scpd_area);
	if (ccode > 0)
		return (ccode == 3) ? -ENODEV : -EBUSY;

	ret = chsc_error_from_response(scpd_area->response.code);
	if (ret)
		CIO_CRW_EVENT(2, "chsc: scpd failed (rc=%04x)\n",
			      scpd_area->response.code);
	return ret;
}
EXPORT_SYMBOL_GPL(chsc_determine_channel_path_desc);

int chsc_determine_base_channel_path_desc(struct chp_id chpid,
					  struct channel_path_desc *desc)
{
	struct chsc_response_struct *chsc_resp;
	struct chsc_scpd *scpd_area;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&chsc_page_lock, flags);
	scpd_area = chsc_page;
	ret = chsc_determine_channel_path_desc(chpid, 0, 0, 0, 0, scpd_area);
	if (ret)
		goto out;
	chsc_resp = (void *)&scpd_area->response;
	memcpy(desc, &chsc_resp->data, sizeof(*desc));
out:
	spin_unlock_irqrestore(&chsc_page_lock, flags);
	return ret;
}

int chsc_determine_fmt1_channel_path_desc(struct chp_id chpid,
					  struct channel_path_desc_fmt1 *desc)
{
	struct chsc_response_struct *chsc_resp;
	struct chsc_scpd *scpd_area;
	int ret;

	spin_lock_irq(&chsc_page_lock);
	scpd_area = chsc_page;
	ret = chsc_determine_channel_path_desc(chpid, 0, 0, 1, 0, scpd_area);
	if (ret)
		goto out;
	chsc_resp = (void *)&scpd_area->response;
	memcpy(desc, &chsc_resp->data, sizeof(*desc));
out:
	spin_unlock_irq(&chsc_page_lock);
	return ret;
}

static void
chsc_initialize_cmg_chars(struct channel_path *chp, u8 cmcv,
			  struct cmg_chars *chars)
{
	struct cmg_chars *cmg_chars;
	int i, mask;

	cmg_chars = chp->cmg_chars;
	for (i = 0; i < NR_MEASUREMENT_CHARS; i++) {
		mask = 0x80 >> (i + 3);
		if (cmcv & mask)
			cmg_chars->values[i] = chars->values[i];
		else
			cmg_chars->values[i] = 0;
	}
}

int chsc_get_channel_measurement_chars(struct channel_path *chp)
{
	struct cmg_chars *cmg_chars;
	int ccode, ret;

	struct {
		struct chsc_header request;
		u32 : 24;
		u32 first_chpid : 8;
		u32 : 24;
		u32 last_chpid : 8;
		u32 zeroes1;
		struct chsc_header response;
		u32 zeroes2;
		u32 not_valid : 1;
		u32 shared : 1;
		u32 : 22;
		u32 chpid : 8;
		u32 cmcv : 5;
		u32 : 11;
		u32 cmgq : 8;
		u32 cmg : 8;
		u32 zeroes3;
		u32 data[NR_MEASUREMENT_CHARS];
	} __attribute__ ((packed)) *scmc_area;

	chp->cmg_chars = NULL;
	cmg_chars = kmalloc(sizeof(*cmg_chars), GFP_KERNEL);
	if (!cmg_chars)
		return -ENOMEM;

	spin_lock_irq(&chsc_page_lock);
	memset(chsc_page, 0, PAGE_SIZE);
	scmc_area = chsc_page;
	scmc_area->request.length = 0x0010;
	scmc_area->request.code = 0x0022;
	scmc_area->first_chpid = chp->chpid.id;
	scmc_area->last_chpid = chp->chpid.id;

	ccode = chsc(scmc_area);
	if (ccode > 0) {
		ret = (ccode == 3) ? -ENODEV : -EBUSY;
		goto out;
	}

	ret = chsc_error_from_response(scmc_area->response.code);
	if (ret) {
		CIO_CRW_EVENT(2, "chsc: scmc failed (rc=%04x)\n",
			      scmc_area->response.code);
		goto out;
	}
	if (scmc_area->not_valid) {
		chp->cmg = -1;
		chp->shared = -1;
		goto out;
	}
	chp->cmg = scmc_area->cmg;
	chp->shared = scmc_area->shared;
	if (chp->cmg != 2 && chp->cmg != 3) {
		/* No cmg-dependent data. */
		goto out;
	}
	chp->cmg_chars = cmg_chars;
	chsc_initialize_cmg_chars(chp, scmc_area->cmcv,
				  (struct cmg_chars *) &scmc_area->data);
out:
	spin_unlock_irq(&chsc_page_lock);
	if (!chp->cmg_chars)
		kfree(cmg_chars);

	return ret;
}

int __init chsc_init(void)
{
	int ret;

	sei_page = (void *)get_zeroed_page(GFP_KERNEL | GFP_DMA);
	chsc_page = (void *)get_zeroed_page(GFP_KERNEL | GFP_DMA);
	if (!sei_page || !chsc_page) {
		ret = -ENOMEM;
		goto out_err;
	}
	ret = crw_register_handler(CRW_RSC_CSS, chsc_process_crw);
	if (ret)
		goto out_err;
	return ret;
out_err:
	free_page((unsigned long)chsc_page);
	free_page((unsigned long)sei_page);
	return ret;
}

void __init chsc_init_cleanup(void)
{
	crw_unregister_handler(CRW_RSC_CSS);
	free_page((unsigned long)chsc_page);
	free_page((unsigned long)sei_page);
}

int chsc_enable_facility(int operation_code)
{
	unsigned long flags;
	int ret;
	struct {
		struct chsc_header request;
		u8 reserved1:4;
		u8 format:4;
		u8 reserved2;
		u16 operation_code;
		u32 reserved3;
		u32 reserved4;
		u32 operation_data_area[252];
		struct chsc_header response;
		u32 reserved5:4;
		u32 format2:4;
		u32 reserved6:24;
	} __attribute__ ((packed)) *sda_area;

	spin_lock_irqsave(&chsc_page_lock, flags);
	memset(chsc_page, 0, PAGE_SIZE);
	sda_area = chsc_page;
	sda_area->request.length = 0x0400;
	sda_area->request.code = 0x0031;
	sda_area->operation_code = operation_code;

	ret = chsc(sda_area);
	if (ret > 0) {
		ret = (ret == 3) ? -ENODEV : -EBUSY;
		goto out;
	}

	switch (sda_area->response.code) {
	case 0x0101:
		ret = -EOPNOTSUPP;
		break;
	default:
		ret = chsc_error_from_response(sda_area->response.code);
	}
	if (ret != 0)
		CIO_CRW_EVENT(2, "chsc: sda (oc=%x) failed (rc=%04x)\n",
			      operation_code, sda_area->response.code);
out:
	spin_unlock_irqrestore(&chsc_page_lock, flags);
	return ret;
}

struct css_general_char css_general_characteristics;
struct css_chsc_char css_chsc_characteristics;

int __init
chsc_determine_css_characteristics(void)
{
	int result;
	struct {
		struct chsc_header request;
		u32 reserved1;
		u32 reserved2;
		u32 reserved3;
		struct chsc_header response;
		u32 reserved4;
		u32 general_char[510];
		u32 chsc_char[508];
	} __attribute__ ((packed)) *scsc_area;

	spin_lock_irq(&chsc_page_lock);
	memset(chsc_page, 0, PAGE_SIZE);
	scsc_area = chsc_page;
	scsc_area->request.length = 0x0010;
	scsc_area->request.code = 0x0010;

	result = chsc(scsc_area);
	if (result) {
		result = (result == 3) ? -ENODEV : -EBUSY;
		goto exit;
	}

	result = chsc_error_from_response(scsc_area->response.code);
	if (result == 0) {
		memcpy(&css_general_characteristics, scsc_area->general_char,
		       sizeof(css_general_characteristics));
		memcpy(&css_chsc_characteristics, scsc_area->chsc_char,
		       sizeof(css_chsc_characteristics));
	} else
		CIO_CRW_EVENT(2, "chsc: scsc failed (rc=%04x)\n",
			      scsc_area->response.code);
exit:
	spin_unlock_irq(&chsc_page_lock);
	return result;
}

EXPORT_SYMBOL_GPL(css_general_characteristics);
EXPORT_SYMBOL_GPL(css_chsc_characteristics);

int chsc_sstpc(void *page, unsigned int op, u16 ctrl)
{
	struct {
		struct chsc_header request;
		unsigned int rsvd0;
		unsigned int op : 8;
		unsigned int rsvd1 : 8;
		unsigned int ctrl : 16;
		unsigned int rsvd2[5];
		struct chsc_header response;
		unsigned int rsvd3[7];
	} __attribute__ ((packed)) *rr;
	int rc;

	memset(page, 0, PAGE_SIZE);
	rr = page;
	rr->request.length = 0x0020;
	rr->request.code = 0x0033;
	rr->op = op;
	rr->ctrl = ctrl;
	rc = chsc(rr);
	if (rc)
		return -EIO;
	rc = (rr->response.code == 0x0001) ? 0 : -EIO;
	return rc;
}

int chsc_sstpi(void *page, void *result, size_t size)
{
	struct {
		struct chsc_header request;
		unsigned int rsvd0[3];
		struct chsc_header response;
		char data[size];
	} __attribute__ ((packed)) *rr;
	int rc;

	memset(page, 0, PAGE_SIZE);
	rr = page;
	rr->request.length = 0x0010;
	rr->request.code = 0x0038;
	rc = chsc(rr);
	if (rc)
		return -EIO;
	memcpy(result, &rr->data, size);
	return (rr->response.code == 0x0001) ? 0 : -EIO;
}

int chsc_siosl(struct subchannel_id schid)
{
	struct {
		struct chsc_header request;
		u32 word1;
		struct subchannel_id sid;
		u32 word3;
		struct chsc_header response;
		u32 word[11];
	} __attribute__ ((packed)) *siosl_area;
	unsigned long flags;
	int ccode;
	int rc;

	spin_lock_irqsave(&chsc_page_lock, flags);
	memset(chsc_page, 0, PAGE_SIZE);
	siosl_area = chsc_page;
	siosl_area->request.length = 0x0010;
	siosl_area->request.code = 0x0046;
	siosl_area->word1 = 0x80000000;
	siosl_area->sid = schid;

	ccode = chsc(siosl_area);
	if (ccode > 0) {
		if (ccode == 3)
			rc = -ENODEV;
		else
			rc = -EBUSY;
		CIO_MSG_EVENT(2, "chsc: chsc failed for 0.%x.%04x (ccode=%d)\n",
			      schid.ssid, schid.sch_no, ccode);
		goto out;
	}
	rc = chsc_error_from_response(siosl_area->response.code);
	if (rc)
		CIO_MSG_EVENT(2, "chsc: siosl failed for 0.%x.%04x (rc=%04x)\n",
			      schid.ssid, schid.sch_no,
			      siosl_area->response.code);
	else
		CIO_MSG_EVENT(4, "chsc: siosl succeeded for 0.%x.%04x\n",
			      schid.ssid, schid.sch_no);
out:
	spin_unlock_irqrestore(&chsc_page_lock, flags);
	return rc;
}
EXPORT_SYMBOL_GPL(chsc_siosl);
