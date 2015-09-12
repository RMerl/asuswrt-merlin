#ifndef _RTL8367C_ASICDRV_RMA_H_
#define _RTL8367C_ASICDRV_RMA_H_

#include <rtl8367c_asicdrv.h>

#define RTL8367C_RMAMAX                     0x2F

enum RTL8367C_RMAOP
{
    RMAOP_FORWARD = 0,
    RMAOP_TRAP_TO_CPU,
    RMAOP_DROP,
    RMAOP_FORWARD_EXCLUDE_CPU,
    RMAOP_END    
};


typedef struct  rtl8367c_rma_s{

#ifdef _LITTLE_ENDIAN
    rtk_uint16 portiso_leaky:1;
    rtk_uint16 vlan_leaky:1;
    rtk_uint16 keep_format:1;
    rtk_uint16 trap_priority:3;
    rtk_uint16 discard_storm_filter:1;
    rtk_uint16 operation:2;
    rtk_uint16 reserved:7;
#else
    rtk_uint16 reserved:7;
    rtk_uint16 operation:2;
    rtk_uint16 discard_storm_filter:1;
    rtk_uint16 trap_priority:3;
    rtk_uint16 keep_format:1;
    rtk_uint16 vlan_leaky:1;
    rtk_uint16 portiso_leaky:1;
#endif

}rtl8367c_rma_t;


extern ret_t rtl8367c_setAsicRma(rtk_uint32 index, rtl8367c_rma_t* pRmacfg);
extern ret_t rtl8367c_getAsicRma(rtk_uint32 index, rtl8367c_rma_t* pRmacfg);
extern ret_t rtl8367c_setAsicRmaCdp(rtl8367c_rma_t* pRmacfg);
extern ret_t rtl8367c_getAsicRmaCdp(rtl8367c_rma_t* pRmacfg);
extern ret_t rtl8367c_setAsicRmaCsstp(rtl8367c_rma_t* pRmacfg);
extern ret_t rtl8367c_getAsicRmaCsstp(rtl8367c_rma_t* pRmacfg);
extern ret_t rtl8367c_setAsicRmaLldp(rtk_uint32 enabled, rtl8367c_rma_t* pRmacfg);
extern ret_t rtl8367c_getAsicRmaLldp(rtk_uint32 *pEnabled, rtl8367c_rma_t* pRmacfg);

#endif /*#ifndef _RTL8367C_ASICDRV_RMA_H_*/

