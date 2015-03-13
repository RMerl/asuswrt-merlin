#ifndef _RTL8367C_ASICDRV_VLAN_H_
#define _RTL8367C_ASICDRV_VLAN_H_

/****************************************************************/
/* Header File inclusion                                        */
/****************************************************************/
#include <rtl8367c_asicdrv.h>

/****************************************************************/
/* Constant Definition                                          */
/****************************************************************/
#define RTL8367C_PROTOVLAN_GIDX_MAX 3
#define RTL8367C_PROTOVLAN_GROUPNO  4

#define	RTL8367C_VLAN_BUSY_CHECK_NO		(10)


/****************************************************************/
/* Type Definition                                              */
/****************************************************************/
typedef struct  VLANCONFIGSMI
{
#ifdef _LITTLE_ENDIAN
	rtk_uint16	mbr:11;
	rtk_uint16  reserved:5;

	rtk_uint16	fid_msti:4;
	rtk_uint16  reserved2:12;

	rtk_uint16	vbpen:1;
	rtk_uint16	vbpri:3;
	rtk_uint16	envlanpol:1;
	rtk_uint16	meteridx:6;
	rtk_uint16	reserved3:5;

	rtk_uint16	evid:13;
	rtk_uint16  reserved4:3;
#else
	rtk_uint16  reserved:5;
	rtk_uint16	mbr:11;

	rtk_uint16  reserved2:12;
	rtk_uint16	fid_msti:4;

	rtk_uint16	reserved3:5;
	rtk_uint16	meteridx:6;
	rtk_uint16	envlanpol:1;
	rtk_uint16	vbpri:3;
	rtk_uint16	vbpen:1;

	rtk_uint16  reserved4:3;
	rtk_uint16	evid:13;
#endif

}rtl8367c_vlanconfigsmi;

typedef struct  VLANCONFIGUSER
{
    rtk_uint16 	evid;
	rtk_uint16 	mbr;
    rtk_uint16  fid_msti;
    rtk_uint16  envlanpol;
    rtk_uint16  meteridx;
    rtk_uint16  vbpen;
    rtk_uint16  vbpri;
}rtl8367c_vlanconfiguser;

typedef struct  VLANTABLE
{
#ifdef _LITTLE_ENDIAN
	rtk_uint16 	mbr:8;
 	rtk_uint16 	untag:8;

 	rtk_uint16 	fid_msti:4;
 	rtk_uint16 	vbpen:1;
	rtk_uint16	vbpri:3;
	rtk_uint16	envlanpol:1;
	rtk_uint16	meteridx:5;
	rtk_uint16	ivl_svl:1;
	rtk_uint16	mbr_ext_0_1:2;

	rtk_uint16	mbr_ext_2:1;
	rtk_uint16	untagset_ext:3;
	rtk_uint16	mtr_idx_ext:1;
	rtk_uint16	reserved:11;
#else
 	rtk_uint16 	untag:8;
	rtk_uint16 	mbr:8;

	rtk_uint16	mbr_ext_0_1:2;
	rtk_uint16	ivl_svl:1;
	rtk_uint16	meteridx:5;
	rtk_uint16	envlanpol:1;
	rtk_uint16	vbpri:3;
 	rtk_uint16 	vbpen:1;
 	rtk_uint16 	fid_msti:4;

	rtk_uint16	reserved:11;
	rtk_uint16	mtr_idx_ext:1;
	rtk_uint16	untagset_ext:3;
	rtk_uint16	mbr_ext_2:1;

#endif
}rtl8367c_vlan4kentrysmi;

typedef struct  USER_VLANTABLE{

	rtk_uint16 	vid;
	rtk_uint16 	mbr;
 	rtk_uint16 	untag;
    rtk_uint16  fid_msti;
    rtk_uint16  envlanpol;
    rtk_uint16  meteridx;
    rtk_uint16  vbpen;
    rtk_uint16  vbpri;
	rtk_uint16 	ivl_svl;

}rtl8367c_user_vlan4kentry;

typedef enum
{
    FRAME_TYPE_BOTH = 0,
    FRAME_TYPE_TAGGED_ONLY,
    FRAME_TYPE_UNTAGGED_ONLY,
    FRAME_TYPE_MAX_BOUND
} rtl8367c_accframetype;

typedef enum
{
    EG_TAG_MODE_ORI = 0,
    EG_TAG_MODE_KEEP,
    EG_TAG_MODE_PRI_TAG,
    EG_TAG_MODE_REAL_KEEP,
    EG_TAG_MODE_END
} rtl8367c_egtagmode;

typedef enum
{
    PPVLAN_FRAME_TYPE_ETHERNET = 0,
    PPVLAN_FRAME_TYPE_LLC,
    PPVLAN_FRAME_TYPE_RFC1042,
    PPVLAN_FRAME_TYPE_END
} rtl8367c_provlan_frametype;

enum RTL8367C_STPST
{
	STPST_DISABLED = 0,
	STPST_BLOCKING,
	STPST_LEARNING,
	STPST_FORWARDING
};

enum RTL8367C_RESVIDACT
{
	RES_VID_ACT_UNTAG = 0,
	RES_VID_ACT_TAG,
	RES_VID_ACT_END
};

typedef struct
{
    rtl8367c_provlan_frametype  frameType;
    rtk_uint32                      etherType;
} rtl8367c_protocolgdatacfg;

typedef struct
{
    rtk_uint32 valid;
    rtk_uint32 vlan_idx;
    rtk_uint32 priority;
} rtl8367c_protocolvlancfg;


void _rtl8367c_VlanMCStUser2Smi(rtl8367c_vlanconfiguser *pVlanCg, rtl8367c_vlanconfigsmi *pSmiVlanCfg);
void _rtl8367c_VlanMCStSmi2User(rtl8367c_vlanconfigsmi *pSmiVlanCfg, rtl8367c_vlanconfiguser *pVlanCg);
void _rtl8367c_Vlan4kStUser2Smi(rtl8367c_user_vlan4kentry *pUserVlan4kEntry, rtl8367c_vlan4kentrysmi *pSmiVlan4kEntry);
void _rtl8367c_Vlan4kStSmi2User(rtl8367c_vlan4kentrysmi *pSmiVlan4kEntry, rtl8367c_user_vlan4kentry *pUserVlan4kEntry);

extern ret_t rtl8367c_setAsicVlanMemberConfig(rtk_uint32 index, rtl8367c_vlanconfiguser *pVlanCg);
extern ret_t rtl8367c_getAsicVlanMemberConfig(rtk_uint32 index, rtl8367c_vlanconfiguser *pVlanCg);
extern ret_t rtl8367c_setAsicVlan4kEntry(rtl8367c_user_vlan4kentry *pVlan4kEntry );
extern ret_t rtl8367c_getAsicVlan4kEntry(rtl8367c_user_vlan4kentry *pVlan4kEntry );
extern ret_t rtl8367c_setAsicVlanAccpetFrameType(rtk_uint32 port, rtl8367c_accframetype frameType);
extern ret_t rtl8367c_getAsicVlanAccpetFrameType(rtk_uint32 port, rtl8367c_accframetype *pFrameType);
extern ret_t rtl8367c_setAsicVlanIngressFilter(rtk_uint32 port, rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicVlanIngressFilter(rtk_uint32 port, rtk_uint32 *pEnable);
extern ret_t rtl8367c_setAsicVlanEgressTagMode(rtk_uint32 port, rtl8367c_egtagmode tagMode);
extern ret_t rtl8367c_getAsicVlanEgressTagMode(rtk_uint32 port, rtl8367c_egtagmode *pTagMode);
extern ret_t rtl8367c_setAsicVlanPortBasedVID(rtk_uint32 port, rtk_uint32 index, rtk_uint32 pri);
extern ret_t rtl8367c_getAsicVlanPortBasedVID(rtk_uint32 port, rtk_uint32 *pIndex, rtk_uint32 *pPri);
extern ret_t rtl8367c_setAsicVlanProtocolBasedGroupData(rtk_uint32 index, rtl8367c_protocolgdatacfg *pPbCfg);
extern ret_t rtl8367c_getAsicVlanProtocolBasedGroupData(rtk_uint32 index, rtl8367c_protocolgdatacfg *pPbCfg);
extern ret_t rtl8367c_setAsicVlanPortAndProtocolBased(rtk_uint32 port, rtk_uint32 index, rtl8367c_protocolvlancfg *pPpbCfg);
extern ret_t rtl8367c_getAsicVlanPortAndProtocolBased(rtk_uint32 port, rtk_uint32 index, rtl8367c_protocolvlancfg *pPpbCfg);
extern ret_t rtl8367c_setAsicVlanFilter(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicVlanFilter(rtk_uint32* pEnabled);

extern ret_t rtl8367c_setAsicPortBasedFid(rtk_uint32 port, rtk_uint32 fid);
extern ret_t rtl8367c_getAsicPortBasedFid(rtk_uint32 port, rtk_uint32* pFid);
extern ret_t rtl8367c_setAsicPortBasedFidEn(rtk_uint32 port, rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicPortBasedFidEn(rtk_uint32 port, rtk_uint32* pEnabled);
extern ret_t rtl8367c_setAsicSpanningTreeStatus(rtk_uint32 port, rtk_uint32 msti, rtk_uint32 state);
extern ret_t rtl8367c_getAsicSpanningTreeStatus(rtk_uint32 port, rtk_uint32 msti, rtk_uint32* pState);
extern ret_t rtl8367c_setAsicVlanUntagDscpPriorityEn(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicVlanUntagDscpPriorityEn(rtk_uint32* enabled);
extern ret_t rtl8367c_setAsicVlanTransparent(rtk_uint32 port, rtk_uint32 portmask);
extern ret_t rtl8367c_getAsicVlanTransparent(rtk_uint32 port, rtk_uint32 *pPortmask);
extern ret_t rtl8367c_setAsicVlanEgressKeep(rtk_uint32 port, rtk_uint32 portmask);
extern ret_t rtl8367c_getAsicVlanEgressKeep(rtk_uint32 port, rtk_uint32* pPortmask);
extern ret_t rtl8367c_setReservedVidAction(rtk_uint32 vid0Action, rtk_uint32 vid4095Action);
extern ret_t rtl8367c_getReservedVidAction(rtk_uint32 *pVid0Action, rtk_uint32 *pVid4095Action);
extern ret_t rtl8367c_setRealKeepRemarkEn(rtk_uint32 enabled);
extern ret_t rtl8367c_getRealKeepRemarkEn(rtk_uint32 *pEnabled);
extern ret_t rtl8367c_resetVlan(void);

#endif /*#ifndef _RTL8367C_ASICDRV_VLAN_H_*/

