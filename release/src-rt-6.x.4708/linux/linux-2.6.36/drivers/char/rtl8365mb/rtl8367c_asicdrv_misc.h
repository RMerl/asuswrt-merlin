#ifndef _RTL8367C_ASICDRV_MISC_H_
#define _RTL8367C_ASICDRV_MISC_H_

#include <rtl8367c_asicdrv.h>

extern ret_t rtl8367c_setAsicMacAddress(ether_addr_t mac);
extern ret_t rtl8367c_getAsicMacAddress(ether_addr_t *pMac);
extern ret_t rtl8367c_getAsicDebugInfo(rtk_uint32 port, rtk_uint32 *pDebugifo);
extern ret_t rtl8367c_setAsicPortJamMode(rtk_uint32 mode);
extern ret_t rtl8367c_getAsicPortJamMode(rtk_uint32* pMode);
extern ret_t rtl8367c_setAsicMaxLengthCfg(rtk_uint32 cfgId, rtk_uint32 maxLength);
extern ret_t rtl8367c_getAsicMaxLengthCfg(rtk_uint32 cfgId, rtk_uint32 *pMaxLength);
extern ret_t rtl8367c_setAsicMaxLength(rtk_uint32 port, rtk_uint32 type, rtk_uint32 cfgId);
extern ret_t rtl8367c_getAsicMaxLength(rtk_uint32 port, rtk_uint32 type, rtk_uint32 *pCfgId);

#endif /*_RTL8367C_ASICDRV_MISC_H_*/

