#ifndef _RTL8367C_ASICDRV_PORTISOLATION_H_
#define _RTL8367C_ASICDRV_PORTISOLATION_H_

#include <rtl8367c_asicdrv.h>

extern ret_t rtl8367c_setAsicPortIsolationPermittedPortmask(rtk_uint32 port, rtk_uint32 permitPortmask);
extern ret_t rtl8367c_getAsicPortIsolationPermittedPortmask(rtk_uint32 port, rtk_uint32 *pPermitPortmask);
extern ret_t rtl8367c_setAsicPortIsolationEfid(rtk_uint32 port, rtk_uint32 efid);
extern ret_t rtl8367c_getAsicPortIsolationEfid(rtk_uint32 port, rtk_uint32 *pEfid);

#endif /*_RTL8367C_ASICDRV_PORTISOLATION_H_*/
