/*
 * Copyright (c) 2014 Quantenna Communications, Inc.
 * All rights reserved.
 */

#ifndef _QTN_VLAN_H_
#define _QTN_VLAN_H_

#include "../common/ruby_mem.h"
#include <qtn/qtn_debug.h>
#include <qtn/qtn_uc_comm.h>
#include <qtn/qtn_net_packet.h>
#if defined(__KERNEL__) || defined(MUC_BUILD) || defined(AUC_BUILD)
#include <qtn/topaz_tqe_cpuif.h>
#endif
#if defined(__KERNEL__)
#include <qtn/dmautil.h>
#endif

#define QVLAN_MODE_ACCESS		0
#define QVLAN_MODE_TRUNK		1
#define QVLAN_MODE_HYBRID		2
#define QVLAN_MODE_DYNAMIC		3
#define QVLAN_MODE_MAX			QVLAN_MODE_DYNAMIC
#define QVLAN_MODE_DISABLED		(QVLAN_MODE_MAX + 1)
#define QVLAN_SHIFT_MODE		16
#define QVLAN_MASK_MODE			0xffff0000
#define QVLAN_MASK_VID			0x00000fff

#define QVLAN_MODE(x)			(uint16_t)((x) >> QVLAN_SHIFT_MODE)
#define QVLAN_VID(x)			(uint16_t)((x) & QVLAN_MASK_VID)

#define QVLAN_MODE_STR_ACCESS	"Access mode"
#define QVLAN_MODE_STR_TRUNK	"Trunk mode"
#define QVLAN_MODE_STR_HYBRID	"Hybrid mode"
#define QVLAN_MODE_STR_DYNAMIC	"Dynamic mode"

/* default port vlan id */
#define QVLAN_DEF_PVID			1

#define QVLAN_VID_MAX			4096
#define QVLAN_VID_MAX_S			12
#define QVLAN_VID_ALL			0xffff

#ifndef NBBY
#define NBBY		8
#endif

#ifndef NBDW
#define NBDW		32
#endif

#ifdef CONFIG_TOPAZ_DBDC_HOST
#define VLAN_INTERFACE_MAX	(QTN_MAX_VAPS + 2 + MAX_QFP_NETDEV)
#define QFP_VDEV_IDX(dev_id)	(QTN_MAX_VAPS + 2 + (dev_id))
#else
#define VLAN_INTERFACE_MAX	(QTN_MAX_VAPS + 2)
#endif
#define WMAC_VDEV_IDX_MAX	QTN_MAX_VAPS
#define EMAC_VDEV_IDX(port)	(QTN_MAX_VAPS + (port))
#define PCIE_VDEV_IDX		(QTN_MAX_VAPS + 0)

#ifndef howmany
#define howmany(x, y)			(((x) + ((y) - 1)) / (y))
#endif

#define bitsz_var(var)			(sizeof(var) * 8)
#define bitsz_ptr(ptr)			bitsz_var((ptr)[0])

#define set_bit_a(a, i)			((a)[(i) / bitsz_ptr(a)] |= 1 << ((i) % bitsz_ptr(a)))
#define clr_bit_a(a, i)			((a)[(i) / bitsz_ptr(a)] &= ~(1 << ((i) % bitsz_ptr(a))))
#define is_set_a(a, i)			((a)[(i) / bitsz_ptr(a)] & (1 << ((i) % bitsz_ptr(a))))
#define is_clr_a(a, i)			(is_set_a(a, i) == 0)

struct qtn_vlan_stats {
	uint32_t lhost;
	uint32_t auc;
	uint32_t muc;
};

struct qtn_vlan_user_interface {
	unsigned long bus_addr;
	uint8_t mode;
};

struct qtn_vlan_dev {
	uint8_t		idx;
	uint8_t		port;
	uint16_t	pvid;
#define QVLAN_DEV_F_DYNAMIC	BIT(0)
	uint32_t	flags;
	unsigned long	bus_addr;
	int		ifindex;
	union {
		uint32_t	member_bitmap[howmany(QVLAN_VID_MAX, NBDW)];
		uint16_t	node_vlan[QTN_NCIDX_MAX];
	}u;
	uint32_t	tag_bitmap[howmany(QVLAN_VID_MAX, NBDW)];
	struct qtn_vlan_stats ig_pass;
	struct qtn_vlan_stats ig_drop;
	struct qtn_vlan_stats eg_pass;
	struct qtn_vlan_stats eg_drop;
	struct qtn_vlan_stats magic_invalid;
	void		*user_data;
};
#define QVLAN_IS_DYNAMIC(vdev)		((vdev)->flags & QVLAN_DEV_F_DYNAMIC)

struct qtn_vlan_pkt {
#define QVLAN_PKT_MAGIC			0x1234
	uint16_t	magic;
#define QVLAN_TAGGED			0x8000
#define QVLAN_SKIP_CHECK		0x4000
	uint16_t	vlan_info;
} __packed;

#define QVLAN_PKTCTRL_LEN	sizeof(struct qtn_vlan_pkt)

struct qtn_vlan_info {
#define QVLAN_TAGRX_UNTOUCH		0
#define QVLAN_TAGRX_STRIP		1
#define QVLAN_TAGRX_TAG			2
#define QVLAN_TAGRX_BITMASK		0x3
#define QVLAN_TAGRX_BITWIDTH		2
#define QVLAN_TAGRX_BITSHIFT		1
#define QVLAN_TAGRX_NUM_PER_DW		(32 / QVLAN_TAGRX_BITWIDTH)
#define QVLAN_TAGRX_NUM_PER_DW_S	4
	uint32_t vlan_tagrx_bitmap[howmany(QVLAN_VID_MAX * QVLAN_TAGRX_BITWIDTH, NBDW)];
};

RUBY_INLINE int qvlan_tagrx_index(int vid)
{
	return (vid >> QVLAN_TAGRX_NUM_PER_DW_S);
}

RUBY_INLINE int qvlan_tagrx_shift(int vid)
{
	int shift;

	shift = vid & (QVLAN_TAGRX_NUM_PER_DW - 1);
	return (shift << QVLAN_TAGRX_BITSHIFT);
}

/*
 * Must be in sync with qcsapi_vlan_config in qcsapi.h
 *  -- Whenever 'struct qtn_vlan_config' changes, qcsapi.h changes as well
 */
struct qtn_vlan_config {
	uint32_t	vlan_cfg;
	union {
		struct vlan_dev_config {
			uint32_t	member_bitmap[howmany(QVLAN_VID_MAX, NBDW)];
			uint32_t	tag_bitmap[howmany(QVLAN_VID_MAX, NBDW)];
		} dev_config;
		uint32_t	tagrx_config[howmany(QVLAN_VID_MAX * QVLAN_TAGRX_BITWIDTH, NBDW)];
	} u;
};

/*
* VLAN forward/drop table
*|	traffic direction	|  frame	|  Access(MBSS/Dynamic mode)	  | Trunk(Passthrough mode)
*|--------------------------------------------------------------------------------------------------------------
*|	wifi tx			|  no vlan	|  drop				  | forward
*|--------------------------------------------------------------------------------------------------------------
*|				|  vlan tagged	| compare tag with PVID:	  | compare tag against VID list
*|				|		| 1.equal:untag and forward	  | 1.Found:forward
*|				|		| 2.not equal:drop		  | 2.Not found:drop
*|--------------------------------------------------------------------------------------------------------------
*|	wifi rx			|  no vlan	| Add PVID tag and forward	  | forward
*|--------------------------------------------------------------------------------------------------------------
*|				|  vlan tagged	| Compare tag with PVID:	  | compare tag against VID list
*|				|		| 1.equal:forward		  | 1. Found:forward
*|				|		| 2.not equal:drop		  | 2. Not found:drop
*|--------------------------------------------------------------------------------------------------------------
*/

#define QVLAN_BYTES_PER_VID		((QTN_MAX_BSS_VAPS + NBBY - 1) / NBBY)
#define QVLAN_BYTES_PER_VID_SHIFT	0

RUBY_INLINE int
qtn_vlan_is_valid(int vid)
{
	/* VLAN ID 0 is reserved */
	return (vid > 0 && vid < QVLAN_VID_MAX);
}

RUBY_INLINE int
qtn_vlan_is_member(volatile struct qtn_vlan_dev *vdev, uint16_t vid)
{
	return !!is_set_a(vdev->u.member_bitmap, vid);
}

RUBY_INLINE int
qtn_vlan_is_tagged_member(volatile struct qtn_vlan_dev *vdev, uint16_t vid)
{
	return !!is_set_a(vdev->tag_bitmap, vid);
}

RUBY_INLINE int
qtn_vlan_is_pvid(volatile struct qtn_vlan_dev *vdev, uint16_t vid)
{
	return vdev->pvid == vid;
}

RUBY_INLINE int
qtn_vlan_is_mode(volatile struct qtn_vlan_dev *vdev, uint16_t mode)
{
	return ((struct qtn_vlan_user_interface *)vdev->user_data)->mode == mode;
}

#if defined(__KERNEL__) || defined(MUC_BUILD) || defined(AUC_BUILD)
RUBY_INLINE int
qtn_vlan_port_indexable(uint8_t port)
{
	return ((port == TOPAZ_TQE_EMAC_0_PORT)
		|| (port == TOPAZ_TQE_EMAC_1_PORT)
		|| (port == TOPAZ_TQE_PCIE_PORT)
		|| (port == TOPAZ_TQE_DSP_PORT));
}
#endif

RUBY_INLINE int
qtn_vlan_get_tagrx(uint32_t *tagrx_bitmap, uint16_t vlanid)
{
	return (tagrx_bitmap[vlanid >> QVLAN_TAGRX_NUM_PER_DW_S] >>
				((vlanid & (QVLAN_TAGRX_NUM_PER_DW - 1)) << QVLAN_TAGRX_BITSHIFT)) &
		QVLAN_TAGRX_BITMASK;
}

RUBY_INLINE void
qtn_vlan_gen_group_addr(uint8_t *mac, uint16_t vid, uint8_t vapid)
{
	uint16_t encode;

	mac[0] = 0xff;
	mac[1] = 0xff;
	mac[2] = 0xff;
	mac[3] = 0xff;

	encode = ((uint16_t)vapid << QVLAN_VID_MAX_S) | vid;
	mac[4] = encode >> 8;
	mac[5] = (uint8_t)(encode & 0xff);
}

RUBY_INLINE int
qtn_vlan_is_group_addr(const uint8_t *mac)
{
	return (mac[0] == 0xff && mac[1] == 0xff
		&& mac[2] == 0xff && mac[3] == 0xff
		&& mac[4] != 0xff);
}

#if defined(__KERNEL__) || defined(MUC_BUILD) || defined(AUC_BUILD)
RUBY_INLINE struct qtn_vlan_pkt*
qtn_vlan_get_info(const void *data)
{
	struct qtn_vlan_pkt *pkt;
#if defined(AUC_BUILD)
#pragma Off(Behaved)
#endif
	pkt = (struct qtn_vlan_pkt *)((const uint8_t *)data - QVLAN_PKTCTRL_LEN);
#if defined(AUC_BUILD)
#pragma On(Behaved)
#endif
	return pkt;
}

RUBY_INLINE void
qtn_vlan_inc_stats(struct qtn_vlan_stats *stats) {
#if defined(__KERNEL__)
	stats->lhost++;
#elif defined(AUC_BUILD)
	stats->auc++;
#elif defined(MUC_BUILD)
	stats->muc++;
#endif
}

RUBY_INLINE int
qtn_vlan_magic_check(struct qtn_vlan_dev *outdev, struct qtn_vlan_pkt *pkt)
{
	if (unlikely(pkt->magic != QVLAN_PKT_MAGIC)) {
		qtn_vlan_inc_stats(&outdev->magic_invalid);
		return 0;
	}

	return 1;
}

RUBY_INLINE int
qtn_vlan_vlanid_check(struct qtn_vlan_dev *vdev, uint16_t ncidx, uint16_t vlanid)
{
	if (QVLAN_IS_DYNAMIC(vdev))
		return (vdev->u.node_vlan[ncidx] == vlanid);
	else
		return qtn_vlan_is_member(vdev, vlanid);
}

RUBY_INLINE int
qtn_vlan_egress(struct qtn_vlan_dev *outdev, uint16_t ncidx, void *data)
{
	struct qtn_vlan_pkt *pkt = qtn_vlan_get_info(data);

	if (!qtn_vlan_magic_check(outdev, pkt)
			|| (pkt->vlan_info & QVLAN_SKIP_CHECK)
			|| qtn_vlan_vlanid_check(outdev, ncidx, pkt->vlan_info & QVLAN_MASK_VID)) {
		qtn_vlan_inc_stats(&outdev->eg_pass);
		return 1;
	}

	qtn_vlan_inc_stats(&outdev->eg_drop);
	return 0;
}
#endif

#if defined(__KERNEL__) || defined(MUC_BUILD)
RUBY_INLINE int
qtn_vlan_ingress(struct qtn_vlan_dev *indev, uint16_t ncidx,
		void *data, uint16_t known_vlanid, uint8_t cache_op)
{
	struct ether_header *eh = (struct ether_header *)data;
	struct qtn_vlan_pkt *pkt = qtn_vlan_get_info(data);
	uint16_t vlanid;
	const uint16_t magic = QVLAN_PKT_MAGIC;

	if (eh->ether_type == htons(ETHERTYPE_8021Q)) {
		vlanid = ntohs(*(uint16_t *)(eh + 1)) & QVLAN_MASK_VID;

		if (!qtn_vlan_vlanid_check(indev, ncidx, vlanid)) {
			qtn_vlan_inc_stats(&indev->ig_drop);
			return 0;
		}

		vlanid |= QVLAN_TAGGED;
	} else if (known_vlanid) {
		vlanid = known_vlanid;

		if (!qtn_vlan_vlanid_check(indev, ncidx, vlanid)) {
			qtn_vlan_inc_stats(&indev->ig_drop);
			return 0;
		}
	} else {
		vlanid = indev->pvid;
	}

	pkt->magic = magic;
	pkt->vlan_info = vlanid;

	if (cache_op) {
#if defined(__KERNEL__)
		flush_and_inv_dcache_sizerange_safe(pkt, QVLAN_PKTCTRL_LEN);
#elif defined(MUC_BUILD)
		flush_and_inv_dcache_range_safe(pkt, QVLAN_PKTCTRL_LEN);
#endif
	}

	qtn_vlan_inc_stats(&indev->ig_pass);
	return 1;
}
#endif

#if defined(__KERNEL__)
extern uint8_t vlan_enabled;
extern struct qtn_vlan_dev *vdev_tbl_lhost[VLAN_INTERFACE_MAX];
extern struct qtn_vlan_dev *vdev_tbl_bus[VLAN_INTERFACE_MAX];
extern struct qtn_vlan_dev *vport_tbl_lhost[TOPAZ_TQE_NUM_PORTS];
extern struct qtn_vlan_dev *vport_tbl_bus[TOPAZ_TQE_NUM_PORTS];
extern struct qtn_vlan_info qtn_vlan_info;

extern struct qtn_vlan_dev *switch_alloc_vlan_dev(uint8_t port, uint8_t idx, int ifindex);
extern void switch_free_vlan_dev(struct qtn_vlan_dev *dev);
extern void switch_free_vlan_dev_by_idx(uint8_t idx);
extern struct qtn_vlan_dev *switch_vlan_dev_get_by_port(uint8_t port);
extern struct qtn_vlan_dev *switch_vlan_dev_get_by_idx(uint8_t idx);

extern int switch_vlan_add_member(struct qtn_vlan_dev *vdev, uint16_t vid, uint8_t tag);
extern int switch_vlan_del_member(struct qtn_vlan_dev *vdev, uint16_t vid);
extern int switch_vlan_tag_member(struct qtn_vlan_dev *vdev, uint16_t vid);
extern int switch_vlan_untag_member(struct qtn_vlan_dev *vdev, uint16_t vid);
extern int switch_vlan_set_pvid(struct qtn_vlan_dev *vdev, uint16_t vid);

/* dynamic VLAN support */
extern void switch_vlan_dyn_enable(struct qtn_vlan_dev *vdev);
extern void switch_vlan_dyn_disable(struct qtn_vlan_dev *vdev);
extern int switch_vlan_set_node(struct qtn_vlan_dev *vdev, uint16_t ncidx, uint16_t vlan);
extern int switch_vlan_clr_node(struct qtn_vlan_dev *vdev, uint16_t ncidx);

extern struct sk_buff *switch_vlan_to_proto_stack(struct sk_buff *, int copy);
extern struct sk_buff *switch_vlan_from_proto_stack(struct sk_buff *, struct qtn_vlan_dev *, uint16_t ncidx, int copy);
extern void switch_vlan_reset(void);
extern void switch_vlan_dev_reset(struct qtn_vlan_dev *vdev, uint8_t mode);
extern void switch_vlan_emac_to_lhost(uint32_t enable);
#endif

#endif
