/*
 * Copyright (C) 2013 Realtek Semiconductor Corp.
 * All Rights Reserved.
 *
 * This program is the proprietary software of Realtek Semiconductor
 * Corporation and/or its licensors, and only be used, duplicated,
 * modified or distributed under the authorized license from Realtek.
 *
 * ANY USE OF THE SOFTWARE OTHER THAN AS AUTHORIZED UNDER
 * THIS LICENSE OR COPYRIGHT LAW IS PROHIBITED.
 *
 * $Revision: 38651 $
 * $Date: 2013-04-17 14:32:56 +0800 (週三, 17 四月 2013) $
 *
 * Purpose : RTL8367C switch high-level API for RTL8367C
 * Feature : RMA related functions
 *
 */
#include <rtl8367c_asicdrv_rma.h>
/* Function Name:
 *      rtl8367c_setAsicRma
 * Description:
 *      Set reserved multicast address for CPU trapping
 * Input:
 *      index     - reserved multicast LSB byte, 0x00~0x2F is available value
 *      pRmacfg     - type of RMA for trapping frame type setting
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 *      RT_ERR_RMA_ADDR - Invalid RMA address index
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicRma(rtk_uint32 index, rtl8367c_rma_t* pRmacfg)
{
    rtk_uint32 regData;
    ret_t retVal;

    if(index > RTL8367C_RMAMAX)
        return RT_ERR_RMA_ADDR;

    regData = *(rtk_uint16*)pRmacfg;

    if( (index >= 0x4 && index <= 0x7) || (index >= 0x9 && index <= 0x0C) || (0x0F == index))
        index = 0x04;
    else if((index >= 0x13 && index <= 0x17) || (0x19 == index) || (index >= 0x1B && index <= 0x1f))
        index = 0x13;
    else if(index >= 0x22 && index <= 0x2F)
        index = 0x22;

    retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_RMA_CTRL00, RTL8367C_TRAP_PRIORITY_MASK, pRmacfg->trap_priority);
    if(retVal != RT_ERR_OK)
        return retVal;

    return rtl8367c_setAsicReg(RTL8367C_REG_RMA_CTRL00+index, regData);
}
/* Function Name:
 *      rtl8367c_getAsicRma
 * Description:
 *      Get reserved multicast address for CPU trapping
 * Input:
 *      index     - reserved multicast LSB byte, 0x00~0x2F is available value
 *      rmacfg     - type of RMA for trapping frame type setting
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 *      RT_ERR_RMA_ADDR - Invalid RMA address index
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicRma(rtk_uint32 index, rtl8367c_rma_t* pRmacfg)
{
    ret_t retVal;
    rtk_uint32 regData;

    if(index > RTL8367C_RMAMAX)
        return RT_ERR_RMA_ADDR;

    if( (index >= 0x4 && index <= 0x7) || (index >= 0x9 && index <= 0x0C) || (0x0F == index))
        index = 0x04;
    else if((index >= 0x13 && index <= 0x17) || (0x19 == index) || (index >= 0x1B && index <= 0x1f))
        index = 0x13;
    else if(index >= 0x22 && index <= 0x2F)
        index = 0x22;

    retVal = rtl8367c_getAsicReg(RTL8367C_REG_RMA_CTRL00+index, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    *pRmacfg = *(rtl8367c_rma_t*)(&regData);

    retVal = rtl8367c_getAsicRegBits(RTL8367C_REG_RMA_CTRL00, RTL8367C_TRAP_PRIORITY_MASK, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    pRmacfg->trap_priority = regData;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtl8367c_setAsicRmaCdp
 * Description:
 *      Set CDP(Cisco Discovery Protocol) for CPU trapping
 * Input:
 *      pRmacfg     - type of RMA for trapping frame type setting
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 *      RT_ERR_RMA_ADDR - Invalid RMA address index
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicRmaCdp(rtl8367c_rma_t* pRmacfg)
{
    rtk_uint32 regData;
    ret_t retVal;

    if(pRmacfg->operation >= RMAOP_END)
        return RT_ERR_RMA_ACTION;

    if(pRmacfg->trap_priority > RTL8367C_PRIMAX)
        return RT_ERR_QOS_INT_PRIORITY;    

    regData = *(rtk_uint16*)pRmacfg;

    retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_RMA_CTRL00, RTL8367C_TRAP_PRIORITY_MASK, pRmacfg->trap_priority);
    if(retVal != RT_ERR_OK)
        return retVal;

    return rtl8367c_setAsicReg(RTL8367C_REG_RMA_CTRL_CDP, regData);
}
/* Function Name:
 *      rtl8367c_getAsicRmaCdp
 * Description:
 *      Get CDP(Cisco Discovery Protocol) for CPU trapping
 * Input:
 *      None
 * Output:
 *      pRmacfg     - type of RMA for trapping frame type setting
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 *      RT_ERR_RMA_ADDR - Invalid RMA address index
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicRmaCdp(rtl8367c_rma_t* pRmacfg)
{
    ret_t retVal;
    rtk_uint32 regData;

    retVal = rtl8367c_getAsicReg(RTL8367C_REG_RMA_CTRL_CDP, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    *pRmacfg = *(rtl8367c_rma_t*)(&regData);

    retVal = rtl8367c_getAsicRegBits(RTL8367C_REG_RMA_CTRL00, RTL8367C_TRAP_PRIORITY_MASK, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    pRmacfg->trap_priority = regData;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtl8367c_setAsicRmaCsstp
 * Description:
 *      Set CSSTP(Cisco Shared Spanning Tree Protocol) for CPU trapping
 * Input:
 *      pRmacfg     - type of RMA for trapping frame type setting
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 *      RT_ERR_RMA_ADDR - Invalid RMA address index
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicRmaCsstp(rtl8367c_rma_t* pRmacfg)
{
    rtk_uint32 regData;
    ret_t retVal;

    if(pRmacfg->operation >= RMAOP_END)
        return RT_ERR_RMA_ACTION;

    if(pRmacfg->trap_priority > RTL8367C_PRIMAX)
        return RT_ERR_QOS_INT_PRIORITY;    

    regData = *(rtk_uint16*)pRmacfg;

    retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_RMA_CTRL00, RTL8367C_TRAP_PRIORITY_MASK, pRmacfg->trap_priority);
    if(retVal != RT_ERR_OK)
        return retVal;

    return rtl8367c_setAsicReg(RTL8367C_REG_RMA_CTRL_CSSTP, regData);
}
/* Function Name:
 *      rtl8367c_getAsicRmaCsstp
 * Description:
 *      Get CSSTP(Cisco Shared Spanning Tree Protocol) for CPU trapping
 * Input:
 *      None
 * Output:
 *      pRmacfg     - type of RMA for trapping frame type setting
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 *      RT_ERR_RMA_ADDR - Invalid RMA address index
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicRmaCsstp(rtl8367c_rma_t* pRmacfg)
{
    ret_t retVal;
    rtk_uint32 regData;

    retVal = rtl8367c_getAsicReg(RTL8367C_REG_RMA_CTRL_CSSTP, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    *pRmacfg = *(rtl8367c_rma_t*)(&regData);

    retVal = rtl8367c_getAsicRegBits(RTL8367C_REG_RMA_CTRL00, RTL8367C_TRAP_PRIORITY_MASK, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    pRmacfg->trap_priority = regData;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtl8367c_setAsicRmaLldp
 * Description:
 *      Set LLDP for CPU trapping
 * Input:
 *      pRmacfg     - type of RMA for trapping frame type setting
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 *      RT_ERR_RMA_ADDR - Invalid RMA address index
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicRmaLldp(rtk_uint32 enabled, rtl8367c_rma_t* pRmacfg)
{
    rtk_uint32 regData;
    ret_t retVal;

    if(enabled > 1)
        return RT_ERR_ENABLE;    

    if(pRmacfg->operation >= RMAOP_END)
        return RT_ERR_RMA_ACTION;

    if(pRmacfg->trap_priority > RTL8367C_PRIMAX)
        return RT_ERR_QOS_INT_PRIORITY;    

    retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_RMA_LLDP_EN, RTL8367C_RMA_LLDP_EN_OFFSET,enabled);
    if(retVal != RT_ERR_OK)
        return retVal;

    regData = *(rtk_uint16*)pRmacfg;

    retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_RMA_CTRL00, RTL8367C_TRAP_PRIORITY_MASK, pRmacfg->trap_priority);
    if(retVal != RT_ERR_OK)
        return retVal;

    return rtl8367c_setAsicReg(RTL8367C_REG_RMA_CTRL_LLDP, regData);
}
/* Function Name:
 *      rtl8367c_getAsicRmaLldp
 * Description:
 *      Get LLDP for CPU trapping
 * Input:
 *      None
 * Output:
 *      pRmacfg     - type of RMA for trapping frame type setting
 * Return:
 *      RT_ERR_OK         - Success
 *      RT_ERR_SMI      - SMI access error
 *      RT_ERR_RMA_ADDR - Invalid RMA address index
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicRmaLldp(rtk_uint32 *pEnabled, rtl8367c_rma_t* pRmacfg)
{
    ret_t retVal;
    rtk_uint32 regData;

    retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_RMA_LLDP_EN, RTL8367C_RMA_LLDP_EN_OFFSET,pEnabled);
    if(retVal != RT_ERR_OK)
        return retVal;

    retVal = rtl8367c_getAsicReg(RTL8367C_REG_RMA_CTRL_LLDP, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    *pRmacfg = *(rtl8367c_rma_t*)(&regData);

    retVal = rtl8367c_getAsicRegBits(RTL8367C_REG_RMA_CTRL00, RTL8367C_TRAP_PRIORITY_MASK, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    pRmacfg->trap_priority = regData;

    return RT_ERR_OK;
}

