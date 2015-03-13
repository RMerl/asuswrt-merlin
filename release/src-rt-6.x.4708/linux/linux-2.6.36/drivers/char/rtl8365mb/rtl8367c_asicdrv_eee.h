#ifndef _RTL8367C_ASICDRV_EEE_H_
#define _RTL8367C_ASICDRV_EEE_H_

#include <rtl8367c_asicdrv.h>

#define EEE_OCP_PHY_ADDR    (0xA5D0)

extern ret_t rtl8367c_setAsicEee100M(rtk_uint32 port, rtk_uint32 enable);
extern ret_t rtl8367c_getAsicEee100M(rtk_uint32 port, rtk_uint32 *enable);
extern ret_t rtl8367c_setAsicEeeGiga(rtk_uint32 port, rtk_uint32 enable);
extern ret_t rtl8367c_getAsicEeeGiga(rtk_uint32 port, rtk_uint32 *enable);


#endif /*_RTL8367C_ASICDRV_EEE_H_*/
