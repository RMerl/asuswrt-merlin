#ifndef _RTL8367C_ASICDRV_GREEN_H_
#define _RTL8367C_ASICDRV_GREEN_H_

#include <rtl8367c_asicdrv.h>
#include <rtl8367c_asicdrv_phy.h>

#define PHY_POWERSAVING_REG                         24

extern ret_t rtl8367c_setAsicGreenTrafficType(rtk_uint32 priority, rtk_uint32 traffictype);
extern ret_t rtl8367c_getAsicGreenTrafficType(rtk_uint32 priority, rtk_uint32* pTraffictype);
extern ret_t rtl8367c_getAsicGreenPortPage(rtk_uint32 port, rtk_uint32* pPage);
extern ret_t rtl8367c_getAsicGreenHighPriorityTraffic(rtk_uint32 port, rtk_uint32* pIndicator);
extern ret_t rtl8367c_setAsicGreenHighPriorityTraffic(rtk_uint32 port);
extern ret_t rtl8367c_setAsicGreenEthernet(rtk_uint32 port, rtk_uint32 green);
extern ret_t rtl8367c_getAsicGreenEthernet(rtk_uint32 port, rtk_uint32* green);
extern ret_t rtl8367c_setAsicPowerSaving(rtk_uint32 phy, rtk_uint32 enable);
extern ret_t rtl8367c_getAsicPowerSaving(rtk_uint32 phy, rtk_uint32* enable);
#endif /*#ifndef _RTL8367C_ASICDRV_GREEN_H_*/

