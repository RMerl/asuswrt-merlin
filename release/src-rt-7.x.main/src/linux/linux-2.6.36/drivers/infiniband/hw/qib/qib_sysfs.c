/*
 * Copyright (c) 2006, 2007, 2008, 2009 QLogic Corporation. All rights reserved.
 * Copyright (c) 2006 PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <linux/ctype.h>

#include "qib.h"

/**
 * qib_parse_ushort - parse an unsigned short value in an arbitrary base
 * @str: the string containing the number
 * @valp: where to put the result
 *
 * Returns the number of bytes consumed, or negative value on error.
 */
static int qib_parse_ushort(const char *str, unsigned short *valp)
{
	unsigned long val;
	char *end;
	int ret;

	if (!isdigit(str[0])) {
		ret = -EINVAL;
		goto bail;
	}

	val = simple_strtoul(str, &end, 0);

	if (val > 0xffff) {
		ret = -EINVAL;
		goto bail;
	}

	*valp = val;

	ret = end + 1 - str;
	if (ret == 0)
		ret = -EINVAL;

bail:
	return ret;
}

/* start of per-port functions */
/*
 * Get/Set heartbeat enable. OR of 1=enabled, 2=auto
 */
static ssize_t show_hrtbt_enb(struct qib_pportdata *ppd, char *buf)
{
	struct qib_devdata *dd = ppd->dd;
	int ret;

	ret = dd->f_get_ib_cfg(ppd, QIB_IB_CFG_HRTBT);
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", ret);
	return ret;
}

static ssize_t store_hrtbt_enb(struct qib_pportdata *ppd, const char *buf,
			       size_t count)
{
	struct qib_devdata *dd = ppd->dd;
	int ret;
	u16 val;

	ret = qib_parse_ushort(buf, &val);

	/*
	 * Set the "intentional" heartbeat enable per either of
	 * "Enable" and "Auto", as these are normally set together.
	 * This bit is consulted when leaving loopback mode,
	 * because entering loopback mode overrides it and automatically
	 * disables heartbeat.
	 */
	if (ret >= 0)
		ret = dd->f_set_ib_cfg(ppd, QIB_IB_CFG_HRTBT, val);
	if (ret < 0)
		qib_dev_err(dd, "attempt to set invalid Heartbeat enable\n");
	return ret < 0 ? ret : count;
}

static ssize_t store_loopback(struct qib_pportdata *ppd, const char *buf,
			      size_t count)
{
	struct qib_devdata *dd = ppd->dd;
	int ret = count, r;

	r = dd->f_set_ib_loopback(ppd, buf);
	if (r < 0)
		ret = r;

	return ret;
}

static ssize_t store_led_override(struct qib_pportdata *ppd, const char *buf,
				  size_t count)
{
	struct qib_devdata *dd = ppd->dd;
	int ret;
	u16 val;

	ret = qib_parse_ushort(buf, &val);
	if (ret > 0)
		qib_set_led_override(ppd, val);
	else
		qib_dev_err(dd, "attempt to set invalid LED override\n");
	return ret < 0 ? ret : count;
}

static ssize_t show_status(struct qib_pportdata *ppd, char *buf)
{
	ssize_t ret;

	if (!ppd->statusp)
		ret = -EINVAL;
	else
		ret = scnprintf(buf, PAGE_SIZE, "0x%llx\n",
				(unsigned long long) *(ppd->statusp));
	return ret;
}

/*
 * For userland compatibility, these offsets must remain fixed.
 * They are strings for QIB_STATUS_*
 */
static const char *qib_status_str[] = {
	"Initted",
	"",
	"",
	"",
	"",
	"Present",
	"IB_link_up",
	"IB_configured",
	"",
	"Fatal_Hardware_Error",
	NULL,
};

static ssize_t show_status_str(struct qib_pportdata *ppd, char *buf)
{
	int i, any;
	u64 s;
	ssize_t ret;

	if (!ppd->statusp) {
		ret = -EINVAL;
		goto bail;
	}

	s = *(ppd->statusp);
	*buf = '\0';
	for (any = i = 0; s && qib_status_str[i]; i++) {
		if (s & 1) {
			/* if overflow */
			if (any && strlcat(buf, " ", PAGE_SIZE) >= PAGE_SIZE)
				break;
			if (strlcat(buf, qib_status_str[i], PAGE_SIZE) >=
					PAGE_SIZE)
				break;
			any = 1;
		}
		s >>= 1;
	}
	if (any)
		strlcat(buf, "\n", PAGE_SIZE);

	ret = strlen(buf);

bail:
	return ret;
}

/* end of per-port functions */

/*
 * Start of per-port file structures and support code
 * Because we are fitting into other infrastructure, we have to supply the
 * full set of kobject/sysfs_ops structures and routines.
 */
#define QIB_PORT_ATTR(name, mode, show, store) \
	static struct qib_port_attr qib_port_attr_##name = \
		__ATTR(name, mode, show, store)

struct qib_port_attr {
	struct attribute attr;
	ssize_t (*show)(struct qib_pportdata *, char *);
	ssize_t (*store)(struct qib_pportdata *, const char *, size_t);
};

QIB_PORT_ATTR(loopback, S_IWUSR, NULL, store_loopback);
QIB_PORT_ATTR(led_override, S_IWUSR, NULL, store_led_override);
QIB_PORT_ATTR(hrtbt_enable, S_IWUSR | S_IRUGO, show_hrtbt_enb,
	      store_hrtbt_enb);
QIB_PORT_ATTR(status, S_IRUGO, show_status, NULL);
QIB_PORT_ATTR(status_str, S_IRUGO, show_status_str, NULL);

static struct attribute *port_default_attributes[] = {
	&qib_port_attr_loopback.attr,
	&qib_port_attr_led_override.attr,
	&qib_port_attr_hrtbt_enable.attr,
	&qib_port_attr_status.attr,
	&qib_port_attr_status_str.attr,
	NULL
};

static ssize_t qib_portattr_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	struct qib_port_attr *pattr =
		container_of(attr, struct qib_port_attr, attr);
	struct qib_pportdata *ppd =
		container_of(kobj, struct qib_pportdata, pport_kobj);

	return pattr->show(ppd, buf);
}

static ssize_t qib_portattr_store(struct kobject *kobj,
	struct attribute *attr, const char *buf, size_t len)
{
	struct qib_port_attr *pattr =
		container_of(attr, struct qib_port_attr, attr);
	struct qib_pportdata *ppd =
		container_of(kobj, struct qib_pportdata, pport_kobj);

	return pattr->store(ppd, buf, len);
}

static void qib_port_release(struct kobject *kobj)
{
	/* nothing to do since memory is freed by qib_free_devdata() */
}

static const struct sysfs_ops qib_port_ops = {
	.show = qib_portattr_show,
	.store = qib_portattr_store,
};

static struct kobj_type qib_port_ktype = {
	.release = qib_port_release,
	.sysfs_ops = &qib_port_ops,
	.default_attrs = port_default_attributes
};

/* Start sl2vl */

#define QIB_SL2VL_ATTR(N) \
	static struct qib_sl2vl_attr qib_sl2vl_attr_##N = { \
		.attr = { .name = __stringify(N), .mode = 0444 }, \
		.sl = N \
	}

struct qib_sl2vl_attr {
	struct attribute attr;
	int sl;
};

QIB_SL2VL_ATTR(0);
QIB_SL2VL_ATTR(1);
QIB_SL2VL_ATTR(2);
QIB_SL2VL_ATTR(3);
QIB_SL2VL_ATTR(4);
QIB_SL2VL_ATTR(5);
QIB_SL2VL_ATTR(6);
QIB_SL2VL_ATTR(7);
QIB_SL2VL_ATTR(8);
QIB_SL2VL_ATTR(9);
QIB_SL2VL_ATTR(10);
QIB_SL2VL_ATTR(11);
QIB_SL2VL_ATTR(12);
QIB_SL2VL_ATTR(13);
QIB_SL2VL_ATTR(14);
QIB_SL2VL_ATTR(15);

static struct attribute *sl2vl_default_attributes[] = {
	&qib_sl2vl_attr_0.attr,
	&qib_sl2vl_attr_1.attr,
	&qib_sl2vl_attr_2.attr,
	&qib_sl2vl_attr_3.attr,
	&qib_sl2vl_attr_4.attr,
	&qib_sl2vl_attr_5.attr,
	&qib_sl2vl_attr_6.attr,
	&qib_sl2vl_attr_7.attr,
	&qib_sl2vl_attr_8.attr,
	&qib_sl2vl_attr_9.attr,
	&qib_sl2vl_attr_10.attr,
	&qib_sl2vl_attr_11.attr,
	&qib_sl2vl_attr_12.attr,
	&qib_sl2vl_attr_13.attr,
	&qib_sl2vl_attr_14.attr,
	&qib_sl2vl_attr_15.attr,
	NULL
};

static ssize_t sl2vl_attr_show(struct kobject *kobj, struct attribute *attr,
			       char *buf)
{
	struct qib_sl2vl_attr *sattr =
		container_of(attr, struct qib_sl2vl_attr, attr);
	struct qib_pportdata *ppd =
		container_of(kobj, struct qib_pportdata, sl2vl_kobj);
	struct qib_ibport *qibp = &ppd->ibport_data;

	return sprintf(buf, "%u\n", qibp->sl_to_vl[sattr->sl]);
}

static const struct sysfs_ops qib_sl2vl_ops = {
	.show = sl2vl_attr_show,
};

static struct kobj_type qib_sl2vl_ktype = {
	.release = qib_port_release,
	.sysfs_ops = &qib_sl2vl_ops,
	.default_attrs = sl2vl_default_attributes
};

/* End sl2vl */

/* Start diag_counters */

#define QIB_DIAGC_ATTR(N) \
	static struct qib_diagc_attr qib_diagc_attr_##N = { \
		.attr = { .name = __stringify(N), .mode = 0664 }, \
		.counter = offsetof(struct qib_ibport, n_##N) \
	}

struct qib_diagc_attr {
	struct attribute attr;
	size_t counter;
};

QIB_DIAGC_ATTR(rc_resends);
QIB_DIAGC_ATTR(rc_acks);
QIB_DIAGC_ATTR(rc_qacks);
QIB_DIAGC_ATTR(rc_delayed_comp);
QIB_DIAGC_ATTR(seq_naks);
QIB_DIAGC_ATTR(rdma_seq);
QIB_DIAGC_ATTR(rnr_naks);
QIB_DIAGC_ATTR(other_naks);
QIB_DIAGC_ATTR(rc_timeouts);
QIB_DIAGC_ATTR(loop_pkts);
QIB_DIAGC_ATTR(pkt_drops);
QIB_DIAGC_ATTR(dmawait);
QIB_DIAGC_ATTR(unaligned);
QIB_DIAGC_ATTR(rc_dupreq);
QIB_DIAGC_ATTR(rc_seqnak);

static struct attribute *diagc_default_attributes[] = {
	&qib_diagc_attr_rc_resends.attr,
	&qib_diagc_attr_rc_acks.attr,
	&qib_diagc_attr_rc_qacks.attr,
	&qib_diagc_attr_rc_delayed_comp.attr,
	&qib_diagc_attr_seq_naks.attr,
	&qib_diagc_attr_rdma_seq.attr,
	&qib_diagc_attr_rnr_naks.attr,
	&qib_diagc_attr_other_naks.attr,
	&qib_diagc_attr_rc_timeouts.attr,
	&qib_diagc_attr_loop_pkts.attr,
	&qib_diagc_attr_pkt_drops.attr,
	&qib_diagc_attr_dmawait.attr,
	&qib_diagc_attr_unaligned.attr,
	&qib_diagc_attr_rc_dupreq.attr,
	&qib_diagc_attr_rc_seqnak.attr,
	NULL
};

static ssize_t diagc_attr_show(struct kobject *kobj, struct attribute *attr,
			       char *buf)
{
	struct qib_diagc_attr *dattr =
		container_of(attr, struct qib_diagc_attr, attr);
	struct qib_pportdata *ppd =
		container_of(kobj, struct qib_pportdata, diagc_kobj);
	struct qib_ibport *qibp = &ppd->ibport_data;

	return sprintf(buf, "%u\n", *(u32 *)((char *)qibp + dattr->counter));
}

static ssize_t diagc_attr_store(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t size)
{
	struct qib_diagc_attr *dattr =
		container_of(attr, struct qib_diagc_attr, attr);
	struct qib_pportdata *ppd =
		container_of(kobj, struct qib_pportdata, diagc_kobj);
	struct qib_ibport *qibp = &ppd->ibport_data;
	char *endp;
	long val = simple_strtol(buf, &endp, 0);

	if (val < 0 || endp == buf)
		return -EINVAL;

	*(u32 *)((char *) qibp + dattr->counter) = val;
	return size;
}

static const struct sysfs_ops qib_diagc_ops = {
	.show = diagc_attr_show,
	.store = diagc_attr_store,
};

static struct kobj_type qib_diagc_ktype = {
	.release = qib_port_release,
	.sysfs_ops = &qib_diagc_ops,
	.default_attrs = diagc_default_attributes
};

/* End diag_counters */

/* end of per-port file structures and support code */

/*
 * Start of per-unit (or driver, in some cases, but replicated
 * per unit) functions (these get a device *)
 */
static ssize_t show_rev(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct qib_ibdev *dev =
		container_of(device, struct qib_ibdev, ibdev.dev);

	return sprintf(buf, "%x\n", dd_from_dev(dev)->minrev);
}

static ssize_t show_hca(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct qib_ibdev *dev =
		container_of(device, struct qib_ibdev, ibdev.dev);
	struct qib_devdata *dd = dd_from_dev(dev);
	int ret;

	if (!dd->boardname)
		ret = -EINVAL;
	else
		ret = scnprintf(buf, PAGE_SIZE, "%s\n", dd->boardname);
	return ret;
}

static ssize_t show_version(struct device *device,
			    struct device_attribute *attr, char *buf)
{
	/* The string printed here is already newline-terminated. */
	return scnprintf(buf, PAGE_SIZE, "%s", (char *)ib_qib_version);
}

static ssize_t show_boardversion(struct device *device,
				 struct device_attribute *attr, char *buf)
{
	struct qib_ibdev *dev =
		container_of(device, struct qib_ibdev, ibdev.dev);
	struct qib_devdata *dd = dd_from_dev(dev);

	/* The string printed here is already newline-terminated. */
	return scnprintf(buf, PAGE_SIZE, "%s", dd->boardversion);
}


static ssize_t show_localbus_info(struct device *device,
				  struct device_attribute *attr, char *buf)
{
	struct qib_ibdev *dev =
		container_of(device, struct qib_ibdev, ibdev.dev);
	struct qib_devdata *dd = dd_from_dev(dev);

	/* The string printed here is already newline-terminated. */
	return scnprintf(buf, PAGE_SIZE, "%s", dd->lbus_info);
}


static ssize_t show_nctxts(struct device *device,
			   struct device_attribute *attr, char *buf)
{
	struct qib_ibdev *dev =
		container_of(device, struct qib_ibdev, ibdev.dev);
	struct qib_devdata *dd = dd_from_dev(dev);

	/* Return the number of user ports (contexts) available. */
	return scnprintf(buf, PAGE_SIZE, "%u\n", dd->cfgctxts -
		dd->first_user_ctxt);
}

static ssize_t show_serial(struct device *device,
			   struct device_attribute *attr, char *buf)
{
	struct qib_ibdev *dev =
		container_of(device, struct qib_ibdev, ibdev.dev);
	struct qib_devdata *dd = dd_from_dev(dev);

	buf[sizeof dd->serial] = '\0';
	memcpy(buf, dd->serial, sizeof dd->serial);
	strcat(buf, "\n");
	return strlen(buf);
}

static ssize_t store_chip_reset(struct device *device,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct qib_ibdev *dev =
		container_of(device, struct qib_ibdev, ibdev.dev);
	struct qib_devdata *dd = dd_from_dev(dev);
	int ret;

	if (count < 5 || memcmp(buf, "reset", 5) || !dd->diag_client) {
		ret = -EINVAL;
		goto bail;
	}

	ret = qib_reset_device(dd->unit);
bail:
	return ret < 0 ? ret : count;
}

static ssize_t show_logged_errs(struct device *device,
				struct device_attribute *attr, char *buf)
{
	struct qib_ibdev *dev =
		container_of(device, struct qib_ibdev, ibdev.dev);
	struct qib_devdata *dd = dd_from_dev(dev);
	int idx, count;

	/* force consistency with actual EEPROM */
	if (qib_update_eeprom_log(dd) != 0)
		return -ENXIO;

	count = 0;
	for (idx = 0; idx < QIB_EEP_LOG_CNT; ++idx) {
		count += scnprintf(buf + count, PAGE_SIZE - count, "%d%c",
				   dd->eep_st_errs[idx],
				   idx == (QIB_EEP_LOG_CNT - 1) ? '\n' : ' ');
	}

	return count;
}

/*
 * Dump tempsense regs. in decimal, to ease shell-scripts.
 */
static ssize_t show_tempsense(struct device *device,
			      struct device_attribute *attr, char *buf)
{
	struct qib_ibdev *dev =
		container_of(device, struct qib_ibdev, ibdev.dev);
	struct qib_devdata *dd = dd_from_dev(dev);
	int ret;
	int idx;
	u8 regvals[8];

	ret = -ENXIO;
	for (idx = 0; idx < 8; ++idx) {
		if (idx == 6)
			continue;
		ret = dd->f_tempsense_rd(dd, idx);
		if (ret < 0)
			break;
		regvals[idx] = ret;
	}
	if (idx == 8)
		ret = scnprintf(buf, PAGE_SIZE, "%d %d %02X %02X %d %d\n",
				*(signed char *)(regvals),
				*(signed char *)(regvals + 1),
				regvals[2], regvals[3],
				*(signed char *)(regvals + 5),
				*(signed char *)(regvals + 7));
	return ret;
}

/*
 * end of per-unit (or driver, in some cases, but replicated
 * per unit) functions
 */

/* start of per-unit file structures and support code */
static DEVICE_ATTR(hw_rev, S_IRUGO, show_rev, NULL);
static DEVICE_ATTR(hca_type, S_IRUGO, show_hca, NULL);
static DEVICE_ATTR(board_id, S_IRUGO, show_hca, NULL);
static DEVICE_ATTR(version, S_IRUGO, show_version, NULL);
static DEVICE_ATTR(nctxts, S_IRUGO, show_nctxts, NULL);
static DEVICE_ATTR(serial, S_IRUGO, show_serial, NULL);
static DEVICE_ATTR(boardversion, S_IRUGO, show_boardversion, NULL);
static DEVICE_ATTR(logged_errors, S_IRUGO, show_logged_errs, NULL);
static DEVICE_ATTR(tempsense, S_IRUGO, show_tempsense, NULL);
static DEVICE_ATTR(localbus_info, S_IRUGO, show_localbus_info, NULL);
static DEVICE_ATTR(chip_reset, S_IWUSR, NULL, store_chip_reset);

static struct device_attribute *qib_attributes[] = {
	&dev_attr_hw_rev,
	&dev_attr_hca_type,
	&dev_attr_board_id,
	&dev_attr_version,
	&dev_attr_nctxts,
	&dev_attr_serial,
	&dev_attr_boardversion,
	&dev_attr_logged_errors,
	&dev_attr_tempsense,
	&dev_attr_localbus_info,
	&dev_attr_chip_reset,
};

int qib_create_port_files(struct ib_device *ibdev, u8 port_num,
			  struct kobject *kobj)
{
	struct qib_pportdata *ppd;
	struct qib_devdata *dd = dd_from_ibdev(ibdev);
	int ret;

	if (!port_num || port_num > dd->num_pports) {
		qib_dev_err(dd, "Skipping infiniband class with "
			    "invalid port %u\n", port_num);
		ret = -ENODEV;
		goto bail;
	}
	ppd = &dd->pport[port_num - 1];

	ret = kobject_init_and_add(&ppd->pport_kobj, &qib_port_ktype, kobj,
				   "linkcontrol");
	if (ret) {
		qib_dev_err(dd, "Skipping linkcontrol sysfs info, "
			    "(err %d) port %u\n", ret, port_num);
		goto bail;
	}
	kobject_uevent(&ppd->pport_kobj, KOBJ_ADD);

	ret = kobject_init_and_add(&ppd->sl2vl_kobj, &qib_sl2vl_ktype, kobj,
				   "sl2vl");
	if (ret) {
		qib_dev_err(dd, "Skipping sl2vl sysfs info, "
			    "(err %d) port %u\n", ret, port_num);
		goto bail_sl;
	}
	kobject_uevent(&ppd->sl2vl_kobj, KOBJ_ADD);

	ret = kobject_init_and_add(&ppd->diagc_kobj, &qib_diagc_ktype, kobj,
				   "diag_counters");
	if (ret) {
		qib_dev_err(dd, "Skipping diag_counters sysfs info, "
			    "(err %d) port %u\n", ret, port_num);
		goto bail_diagc;
	}
	kobject_uevent(&ppd->diagc_kobj, KOBJ_ADD);

	return 0;

bail_diagc:
	kobject_put(&ppd->sl2vl_kobj);
bail_sl:
	kobject_put(&ppd->pport_kobj);
bail:
	return ret;
}

/*
 * Register and create our files in /sys/class/infiniband.
 */
int qib_verbs_register_sysfs(struct qib_devdata *dd)
{
	struct ib_device *dev = &dd->verbs_dev.ibdev;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(qib_attributes); ++i) {
		ret = device_create_file(&dev->dev, qib_attributes[i]);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * Unregister and remove our files in /sys/class/infiniband.
 */
void qib_verbs_unregister_sysfs(struct qib_devdata *dd)
{
	struct qib_pportdata *ppd;
	int i;

	for (i = 0; i < dd->num_pports; i++) {
		ppd = &dd->pport[i];
		kobject_put(&ppd->pport_kobj);
		kobject_put(&ppd->sl2vl_kobj);
	}
}
