/*
 *  Broadcom Port Trunking/Aggregation setup functions
 *
 * $Copyright Open Broadcom Corporation$
 *
 * $Id$
 */
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmrobo.h>
#include <proto/ethernet.h>
#include <proto/vlan.h>
#include <proto/bcmlacp.h>
#include <bcmendian.h>
#include <et_dbg.h>
#include <etc_agg.h>

typedef struct agg_info {
	agg_pub_t pub;			/* public info for common usage */
	osl_t *osh;			/* os handle */
	void *et;			/* pointer to os-specific private state */
	char *vars;			/* nvram variables handle */
	void *robo;			/* robo private data */
	int8 bhdr_sz;			/* size of the broadcom header */
	uint unit;			/* core unit */
} agg_info_t;

/*
 * Description: This function is used to parsing LACP packets and return
 *                     the address of LACP header.
 *
 * Input:          data - pointer of a packet
 *
 * Return:        lacp header address for LACP packets or NULL for other
 *                     kind of packets
 */
void * BCMFASTPATH
agg_get_lacp_header(void *data)
{
	struct ethervlan_header *evh;
	lacpdu_t *lacph = NULL;

	evh = (struct ethervlan_header *)data;

	if (evh->vlan_type == HTON16(ETHER_TYPE_8021Q) &&
		evh->ether_type == HTON16(ETHER_TYPE_SLOW))
		lacph = (lacpdu_t *)(data + ETHERVLAN_HDR_LEN);
	else if (evh->vlan_type == HTON16(ETHER_TYPE_SLOW))
		lacph = (lacpdu_t *)(data + ETHER_HDR_LEN);
	else
		return NULL;

	if (lacph->subtype != SLOW_SUBTYPE_LACP)
		return NULL;

	return (void *)lacph;
}

/*
 * Description: This function is used to get the destination port id from LACP header.
 *                     the address of LACP header.
 *
 * Input:          lacph - pointer of a LACP header
 *
 * Return:        the value of the actor port field in LACP header
 */
uint16 BCMFASTPATH
agg_get_lacp_dest_pid(void *lacph)
{
	ASSERT(lacph);

	return NTOH16(((lacpdu_t *)lacph)->actor_port);
}

/*
 * Description: This function is used to get the source port id of an incoming LACP packet
 *                     and remove the broadcom header if it is inserted after source mac of
 *                     the packet.
 *
 * Input:          agg - agg info
 *                     p - packet pointer
 *                     dataoff - the data offset in the packet buffer
 *                     bhdr_roff - brcm header's position relative to dest mac
 */
void BCMFASTPATH
agg_process_rx(void *agg, void *p, int dataoff, int8 bhdr_roff)
{
	agg_info_t *aggi = (agg_info_t *)agg;
	osl_t *osh;
	bcm53125_hdr_t bhdr;
	int src_pid = AGG_INVALID_PID;
	int bhdr_sz;
	struct ether_header *eh, *neh;
	void *lacph;
	void *data;

	ASSERT(aggi);
	ASSERT(p);

	osh = aggi->osh;

	/* get bhdr from rx packet */
	if (bhdr_roff > 0)
		bhdr.word = ntoh32_ua(PKTDATA(osh, p) + dataoff + bhdr_roff);
	else
		bhdr.word = ntoh32_ua(PKTDATA(osh, p) + dataoff);

	/* get bhdr size and src pid */
	bhdr_sz = aggi->bhdr_sz;
	switch (bhdr.eg_oc0.op_code) {
	case 0x0:
		src_pid = bhdr.eg_oc0.src_pid;
		break;
	case 0x1:
		bhdr_sz = 8;
		ET_ERROR(("%s op 1 bhdr_size 8\n",
			__FUNCTION__));
		break;
	case 0x2:
		src_pid = bhdr.eg_oc10.src_pid;
		break;
	default:
		ET_ERROR(("%s invalid rx brcm hdr op_code %d\n",
			__FUNCTION__, bhdr.eg_oc0.op_code));
		break;
	}

	/* remove bhdr if it is not in front of data */
	if (bhdr_roff > 0) {
		eh = (struct ether_header *)(PKTDATA(osh, p) + dataoff);
		neh = (struct ether_header *)(PKTDATA(osh, p) + dataoff + bhdr_sz);
		ether_rcopy(eh->ether_shost, neh->ether_shost);
		ether_rcopy(eh->ether_dhost, neh->ether_dhost);
		data = (void *)neh;
	}
	else {
		data = (void *)(PKTDATA(osh, p) + dataoff);
	}

	if (src_pid == AGG_INVALID_PID)
		return;

	lacph = agg_get_lacp_header(data);
	if (!lacph)
		return;

	/* store src pid in lacp partner reserved field */
	LACP_SET_SRC_PID(lacph, src_pid);
}

/*
 * Description: This function is used to add broadcom header for TX packets if
 *                     the header was not yet been added and dedicate the
 *                     destination port id for LACP packets
 *
 * Input:          agg - agg info
 *                     p - packet pointer
 *                     bhdrp - broadcom header pointer of p
 *                     bhdr_roff - brcm header's position relative to dest mac
 *                     dest_pid - dedicate the destination port to send the packet
 *
 * Return:        new packet pointer after processing the original TX packet
 */
void * BCMFASTPATH
agg_process_tx(void *agg, void *p, void *bhdrp, int8 bhdr_roff, uint16 dest_pid)
{
	agg_info_t *aggi = (agg_info_t *)agg;
	osl_t *osh;
	bcm53125_hdr_t bhdr;

	ASSERT(aggi);
	ASSERT(p);
	ASSERT(!PKTSHARED(p));

	osh = aggi->osh;

	/* update exist bhdr */
	if (bhdrp) {
		/* fixup dest pid of bhdr */
		if (dest_pid != AGG_INVALID_PID) {
			bhdr.word = ntoh32_ua(bhdrp);
			bhdr.ing_oc01.op_code = 0x1;
			bhdr.ing_oc01.dst_map = (1 << dest_pid);
		}
		return p;
	}

	/* create the content of bhdr */
	bhdr.word = 0;
	if (dest_pid != AGG_INVALID_PID) {
		bhdr.ing_oc01.op_code = 0x1;
		bhdr.ing_oc01.dst_map = (1 << dest_pid);
	}

	/* realloc a buf with larger headroom if there's no enough room to insert bhdr */
	if (PKTHEADROOM(osh, p) < aggi->bhdr_sz) {
		void *tmp_p = p;

		p = PKTEXPHEADROOM(osh, p, aggi->bhdr_sz);
		/* To free original one and check the new one */
		PKTFREE(osh, tmp_p, TRUE);
		if (p == NULL) {
			ET_ERROR(("%s: Out of memory while adjusting headroom\n",
				__FUNCTION__));
			return NULL;
		}
	}

	/* reserve for inserting bhdr */
	PKTPUSH(osh, p, aggi->bhdr_sz);

	if (bhdr_roff > 0) {
		ASSERT(bhdr_roff == (ETHER_ADDR_LEN * 2));
		/* move dst and src mac addresses if bhdr is not in front of eth hdr */
		ether_copy((PKTDATA(osh, p) + aggi->bhdr_sz), PKTDATA(osh, p));
		ether_copy((PKTDATA(osh, p) + aggi->bhdr_sz + ETHER_ADDR_LEN),
			(PKTDATA(osh, p) + ETHER_ADDR_LEN));
		/* fill bhdr content */
		hton32_ua_store(bhdr.word, PKTDATA(osh, p) + bhdr_roff);
	}
	else {
		/* fill bhdr content */
		hton32_ua_store(bhdr.word, PKTDATA(osh, p));
	}

	if (PKTLEN(osh, p) < (ETHER_MIN_LEN + aggi->bhdr_sz))
		PKTSETLEN(osh, p, (ETHER_MIN_LEN + aggi->bhdr_sz));

	return p;
}

/*
 * Description: This function is used to get link status of robo switch
 *
 * Input:          agg - agg info
 *                     linksts - linksts pointer for the address to store the link status
 *
 * Return:        BCME_OK/BCME_ERROR
 */
int32
agg_get_linksts(void *agg, uint32 *linksts)
{
	agg_info_t *aggi = (agg_info_t *)agg;

	if (!aggi || !linksts) {
		ET_ERROR(("%s: input NULL pointer, aggi %p linksts %p\n",
			__FUNCTION__, aggi, linksts));
		return BCME_ERROR;
	}

	if (!aggi->robo) {
		ET_ERROR(("%s: robo is NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	*linksts = robo_get_linksts(aggi->robo);

	return BCME_OK;
}

/*
 * Description: This function is used to get the dedicate port status of robo switch
 *
 * Input:          agg - agg info
 *                     portid - dedicate port id
 *                     link - pointer for the address to store the link status of the dedicate port
 *                     speed - pointer for the address to store the speed status of the dedicate
 *                                   port
 *                     duplex - pointer for the address to store the duplex status of the dedicate
 *                                   port
 *
 * Return:        BCME_OK/BCME_ERROR
 */
int32
agg_get_portsts(void *agg, int32 portid, uint32 *link, uint32 *speed,
	uint32 *duplex)
{
	agg_info_t *aggi = (agg_info_t *)agg;
	int ret = BCME_ERROR;

	if (!aggi || !link || !speed || !duplex) {
		ET_ERROR(("%s: input NULL pointer, aggi %p link %p speed %p duplex %p\n",
			__FUNCTION__, aggi, link, speed, duplex));
		return ret;
	}

	if (!aggi->robo) {
		ET_ERROR(("%s: robo is NULL\n", __FUNCTION__));
		return ret;
	}

	ret = robo_get_portsts(aggi->robo, portid, link, speed, duplex);

	return ret;
}

/*
 * Description: This function is used to update the port trunking group
 *
 * Input:          agg - agg info
 *                     group - group id
 *                     portmap - the bitmap of those ports in this group
 *
 * Return:        BCME_OK/BCME_ERROR
 */
int32
agg_group_update(void *agg, int group, uint32 portmap)
{
	agg_info_t *aggi = (agg_info_t *)agg;
	int ret = BCME_ERROR;

	if (!aggi) {
		ET_ERROR(("%s: aggi is NULL\n", __FUNCTION__));
		return ret;
	}

	if (!aggi->robo) {
		ET_ERROR(("%s: robo is NULL\n", __FUNCTION__));
		return ret;
	}

	ET_TRACE(("%s: group %d portmap 0x%x\n",
		__FUNCTION__, group, portmap));

	/* group mapping from lacp to robo:
	 * lacp grp 1 -> robo trunking grp 0
	 * lacp grp 2 -> robo trunking grp 1
	 */
	ret = robo_update_agg_group(aggi->robo, (group - 1), portmap);

	return ret;
}

/*
 * Description: This function is used to register/unregister broadcom header
 *
 * Input:          agg - agg info
 *                     on - turn on broadcom header if possible
 *
 * Return:        BCME_OK/BCME_ERROR
 */
int32
agg_bhdr_switch(void *agg, uint32 on)
{
	agg_info_t *aggi = (agg_info_t *)agg;
	bhdr_ports_t bhdr_port;
	int ret = BCME_ERROR;

	if (!aggi) {
		ET_ERROR(("%s: aggi is NULL\n", __FUNCTION__));
		return ret;
	}

	if (!aggi->robo) {
		ET_ERROR(("%s: robo is NULL\n", __FUNCTION__));
		return ret;
	}

	if (aggi->unit == 0)
		bhdr_port = BHDR_PORT5;
	else if (aggi->unit == 1)
		bhdr_port = BHDR_PORT7;
	else if (aggi->unit == 2)
		bhdr_port = BHDR_PORT8;
	else {
		ET_ERROR(("%s: unit %d is not as expected\n", __FUNCTION__, aggi->unit));
		return ret;
	}

	if (on) {
		ret = robo_bhdr_register(aggi->robo, bhdr_port, BHDR_AGG);
		if (!ret)
			aggi->pub.flags |= AGG_FLAG_BCM_HDR;
	}
	else {
		ret = robo_bhdr_unregister(aggi->robo, bhdr_port, BHDR_AGG);
		if (!ret)
			aggi->pub.flags &= ~AGG_FLAG_BCM_HDR;
	}

	return ret;
}

void *
agg_attach(void *osh, void *et, char *vars, uint coreunit, void *robo)
{
	agg_info_t *aggi;

	if (!osh || !et || !robo) {
		ET_ERROR(("%s: input args wrong, osh %p et %p robo %p\n",
			__FUNCTION__, osh, et, robo));
		return NULL;
	}

	/* Allocate private info structure */
	if ((aggi = MALLOC((osl_t *)osh, sizeof(agg_info_t))) == NULL) {
		ET_ERROR(("%s: out of memory, malloced %d bytes\n", __FUNCTION__,
		          MALLOCED(osh)));
		return NULL;
	}
	bzero((char *)aggi, sizeof(agg_info_t));

	aggi->osh = (osl_t *)osh;
	aggi->et = et;
	aggi->vars = vars;
	aggi->robo = robo;
	aggi->bhdr_sz = sizeof(bcm53125_hdr_t);
	ASSERT(aggi->bhdr_sz == 4);
	aggi->unit = coreunit;

	robo_permit_fwd_rsv_mcast(aggi->robo);

	return (void *)aggi;
}

void
agg_detach(void *agg)
{
	if (!agg)
		return;

	agg_bhdr_switch(agg, 0);

	MFREE(((agg_info_t *)agg)->osh, agg, sizeof(agg_info_t));
}
