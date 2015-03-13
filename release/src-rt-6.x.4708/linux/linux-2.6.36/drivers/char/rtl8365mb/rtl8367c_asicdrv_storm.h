#ifndef _RTL8367C_ASICDRV_STORM_H_
#define _RTL8367C_ASICDRV_STORM_H_

#include <rtl8367c_asicdrv.h>

extern ret_t rtl8367c_setAsicStormFilterBroadcastEnable(rtk_uint32 port, rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicStormFilterBroadcastEnable(rtk_uint32 port, rtk_uint32 *pEnabled);
extern ret_t rtl8367c_setAsicStormFilterBroadcastMeter(rtk_uint32 port, rtk_uint32 meter);
extern ret_t rtl8367c_getAsicStormFilterBroadcastMeter(rtk_uint32 port, rtk_uint32 *pMeter);
extern ret_t rtl8367c_setAsicStormFilterMulticastEnable(rtk_uint32 port, rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicStormFilterMulticastEnable(rtk_uint32 port, rtk_uint32 *pEnabled);
extern ret_t rtl8367c_setAsicStormFilterMulticastMeter(rtk_uint32 port, rtk_uint32 meter);
extern ret_t rtl8367c_getAsicStormFilterMulticastMeter(rtk_uint32 port, rtk_uint32 *pMeter);
extern ret_t rtl8367c_setAsicStormFilterUnknownMulticastEnable(rtk_uint32 port, rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicStormFilterUnknownMulticastEnable(rtk_uint32 port, rtk_uint32 *pEnabled);
extern ret_t rtl8367c_setAsicStormFilterUnknownMulticastMeter(rtk_uint32 port, rtk_uint32 meter);
extern ret_t rtl8367c_getAsicStormFilterUnknownMulticastMeter(rtk_uint32 port, rtk_uint32 *pMeter);
extern ret_t rtl8367c_setAsicStormFilterUnknownUnicastEnable(rtk_uint32 port, rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicStormFilterUnknownUnicastEnable(rtk_uint32 port, rtk_uint32 *pEnabled);
extern ret_t rtl8367c_setAsicStormFilterUnknownUnicastMeter(rtk_uint32 port, rtk_uint32 meter);
extern ret_t rtl8367c_getAsicStormFilterUnknownUnicastMeter(rtk_uint32 port, rtk_uint32 *pMeter);
extern ret_t rtl8367c_setAsicStormFilterExtBroadcastMeter(rtk_uint32 meter);
extern ret_t rtl8367c_getAsicStormFilterExtBroadcastMeter(rtk_uint32 *pMeter);
extern ret_t rtl8367c_setAsicStormFilterExtMulticastMeter(rtk_uint32 meter);
extern ret_t rtl8367c_getAsicStormFilterExtMulticastMeter(rtk_uint32 *pMeter);
extern ret_t rtl8367c_setAsicStormFilterExtUnknownMulticastMeter(rtk_uint32 meter);
extern ret_t rtl8367c_getAsicStormFilterExtUnknownMulticastMeter(rtk_uint32 *pMeter);
extern ret_t rtl8367c_setAsicStormFilterExtUnknownUnicastMeter(rtk_uint32 meter);
extern ret_t rtl8367c_getAsicStormFilterExtUnknownUnicastMeter(rtk_uint32 *pMeter);
extern ret_t rtl8367c_setAsicStormFilterExtBroadcastEnable(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicStormFilterExtBroadcastEnable(rtk_uint32 *pEnabled);
extern ret_t rtl8367c_setAsicStormFilterExtMulticastEnable(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicStormFilterExtMulticastEnable(rtk_uint32 *pEnabled);
extern ret_t rtl8367c_setAsicStormFilterExtUnknownMulticastEnable(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicStormFilterExtUnknownMulticastEnable(rtk_uint32 *pEnabled);
extern ret_t rtl8367c_setAsicStormFilterExtUnknownUnicastEnable(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicStormFilterExtUnknownUnicastEnable(rtk_uint32 *pEnabled);
extern ret_t rtl8367c_setAsicStormFilterExtEnablePortMask(rtk_uint32 portmask);
extern ret_t rtl8367c_getAsicStormFilterExtEnablePortMask(rtk_uint32 *pPortmask);


#endif /*_RTL8367C_ASICDRV_STORM_H_*/


