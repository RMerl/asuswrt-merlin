#ifndef _RTL8367C_ASICDRV_INBWCTRL_H_
#define _RTL8367C_ASICDRV_INBWCTRL_H_

#include <rtl8367c_asicdrv.h>

extern ret_t rtl8367c_setAsicPortIngressBandwidth(rtk_uint32 port, rtk_uint32 bandwidth, rtk_uint32 preifg, rtk_uint32 enableFC);
extern ret_t rtl8367c_getAsicPortIngressBandwidth(rtk_uint32 port, rtk_uint32* pBandwidth, rtk_uint32* pPreifg, rtk_uint32* pEnableFC );
extern ret_t rtl8367c_setAsicPortIngressBandwidthBypass(rtk_uint32 enabled);
extern ret_t rtl8367c_getAsicPortIngressBandwidthBypass(rtk_uint32* pEnabled);


#endif /*_RTL8367C_ASICDRV_INBWCTRL_H_*/

