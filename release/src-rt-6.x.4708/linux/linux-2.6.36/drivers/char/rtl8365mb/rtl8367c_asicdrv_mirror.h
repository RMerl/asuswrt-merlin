#ifndef _RTL8367C_ASICDRV_MIRROR_H_
#define _RTL8367C_ASICDRV_MIRROR_H_

#include <rtl8367c_asicdrv.h>

extern ret_t rtl8367c_setAsicPortMirror(rtk_uint32 source, rtk_uint32 monitor);
extern ret_t rtl8367c_getAsicPortMirror(rtk_uint32 *pSource, rtk_uint32 *pMonitor);
extern ret_t rtl8367c_setAsicPortMirrorRxFunction(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicPortMirrorRxFunction(rtk_uint32* pEnabled);
extern ret_t rtl8367c_setAsicPortMirrorTxFunction(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicPortMirrorTxFunction(rtk_uint32* pEnabled);
extern ret_t rtl8367c_setAsicPortMirrorIsolation(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicPortMirrorIsolation(rtk_uint32* pEnabled);
extern ret_t rtl8367c_setAsicPortMirrorPriority(rtk_uint32 priority);
extern ret_t rtl8367c_getAsicPortMirrorPriority(rtk_uint32* pPriority);
extern ret_t rtl8367c_setAsicPortMirrorMask(rtk_uint32 SourcePortmask);
extern ret_t rtl8367c_getAsicPortMirrorMask(rtk_uint32 *pSourcePortmask);
extern ret_t rtl8367c_setAsicPortMirrorVlanRxLeaky(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicPortMirrorVlanRxLeaky(rtk_uint32* pEnabled);
extern ret_t rtl8367c_setAsicPortMirrorVlanTxLeaky(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicPortMirrorVlanTxLeaky(rtk_uint32* pEnabled);
extern ret_t rtl8367c_setAsicPortMirrorIsolationRxLeaky(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicPortMirrorIsolationRxLeaky(rtk_uint32* pEnabled);
extern ret_t rtl8367c_setAsicPortMirrorIsolationTxLeaky(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicPortMirrorIsolationTxLeaky(rtk_uint32* pEnabled);
extern ret_t rtl8367c_setAsicPortMirrorRealKeep(rtk_uint32 mode);
extern ret_t rtl8367c_getAsicPortMirrorRealKeep(rtk_uint32* pMode);
extern ret_t rtl8367c_setAsicPortMirrorOverride(rtk_uint32 rxMirror, rtk_uint32 txMirror, rtk_uint32 aclMirror);
extern ret_t rtl8367c_getAsicPortMirrorOverride(rtk_uint32 *pRxMirror, rtk_uint32 *pTxMirror, rtk_uint32 *pAclMirror);

#endif /*#ifndef _RTL8367C_ASICDRV_MIRROR_H_*/

