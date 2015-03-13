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
 * $Revision: 51993 $
 * $Date: 2014-10-08 19:44:38 +0800 (週三, 08 十月 2014) $
 *
 * Purpose : RTL8367C switch high-level API for RTL8367C
 * Feature : Port security related functions
 *
 */

#include <rtl8367c_asicdrv_port.h>

#ifndef __KERNEL__
#include <string.h>
#else
#include <linux/string.h>
#endif

/* Function Name:
 *      rtl8367c_setAsicPortUnknownDaBehavior
 * Description:
 *      Set UNDA behavior
 * Input:
 *      port        - port ID
 *      behavior 	- 0: flooding to unknwon DA portmask; 1: drop; 2:trap; 3: flooding
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_NOT_ALLOWED  - Invalid behavior
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortUnknownDaBehavior(rtk_uint32 port, rtk_uint32 behavior)
{
    if(port >= RTL8367C_PORTNO)
        return RT_ERR_PORT_ID;

    if(behavior >= L2_UNDA_BEHAVE_END)
		return RT_ERR_NOT_ALLOWED;

	if(port < 8)
		return rtl8367c_setAsicRegBits(RTL8367C_REG_UNKNOWN_UNICAST_DA_PORT_BEHAVE, RTL8367C_Port0_ACTION_MASK << (port * 2), behavior);
	else
		return rtl8367c_setAsicRegBits(RTL8367C_REG_UNKNOWN_UNICAST_DA_PORT_BEHAVE_EXT, RTL8367C_PORT8_ACTION_MASK << ((port-8) * 2), behavior);
}
/* Function Name:
 *      rtl8367c_getAsicPortUnknownDaBehavior
 * Description:
 *      Get UNDA behavior
 * Input:
 *      port        - port ID
 * Output:
 *      pBehavior 	- 0: flooding to unknwon DA portmask; 1: drop; 2:trap; 3: flooding
 * Return:
 *      RT_ERR_OK 	- Success
 *      RT_ERR_SMI  - SMI access error
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortUnknownDaBehavior(rtk_uint32 port, rtk_uint32 *pBehavior)
{
    if(port >= RTL8367C_PORTNO)
        return RT_ERR_PORT_ID;

	if(port < 8)
		return rtl8367c_getAsicRegBits(RTL8367C_REG_UNKNOWN_UNICAST_DA_PORT_BEHAVE, RTL8367C_Port0_ACTION_MASK << (port * 2), pBehavior);
	else
		return rtl8367c_getAsicRegBits(RTL8367C_REG_UNKNOWN_UNICAST_DA_PORT_BEHAVE_EXT, RTL8367C_PORT8_ACTION_MASK << ((port-8) * 2), pBehavior);
}
/* Function Name:
 *      rtl8367c_setAsicPortUnknownSaBehavior
 * Description:
 *      Set UNSA behavior
 * Input:
 *      behavior 	- 0: flooding; 1: drop; 2:trap
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_NOT_ALLOWED  - Invalid behavior
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortUnknownSaBehavior(rtk_uint32 behavior)
{
    if(behavior >= L2_BEHAVE_SA_END)
		return RT_ERR_NOT_ALLOWED;

	return rtl8367c_setAsicRegBits(RTL8367C_PORT_SECURIT_CTRL_REG, RTL8367C_UNKNOWN_SA_BEHAVE_MASK, behavior);
}
/* Function Name:
 *      rtl8367c_getAsicPortUnknownSaBehavior
 * Description:
 *      Get UNSA behavior
 * Input:
 *      pBehavior 	- 0: flooding; 1: drop; 2:trap
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 	- Success
 *      RT_ERR_SMI  - SMI access error
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortUnknownSaBehavior(rtk_uint32 *pBehavior)
{
	return rtl8367c_getAsicRegBits(RTL8367C_PORT_SECURIT_CTRL_REG, RTL8367C_UNKNOWN_SA_BEHAVE_MASK, pBehavior);
}
/* Function Name:
 *      rtl8367c_setAsicPortUnmatchedSaBehavior
 * Description:
 *      Set Unmatched SA behavior
 * Input:
 *      behavior 	- 0: flooding; 1: drop; 2:trap
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_NOT_ALLOWED  - Invalid behavior
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortUnmatchedSaBehavior(rtk_uint32 behavior)
{
    if(behavior >= L2_BEHAVE_SA_END)
		return RT_ERR_NOT_ALLOWED;

	return rtl8367c_setAsicRegBits(RTL8367C_PORT_SECURIT_CTRL_REG, RTL8367C_UNMATCHED_SA_BEHAVE_MASK, behavior);
}
/* Function Name:
 *      rtl8367c_getAsicPortUnmatchedSaBehavior
 * Description:
 *      Get Unmatched SA behavior
 * Input:
 *      pBehavior 	- 0: flooding; 1: drop; 2:trap
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 	- Success
 *      RT_ERR_SMI  - SMI access error
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortUnmatchedSaBehavior(rtk_uint32 *pBehavior)
{
	return rtl8367c_getAsicRegBits(RTL8367C_PORT_SECURIT_CTRL_REG, RTL8367C_UNMATCHED_SA_BEHAVE_MASK, pBehavior);
}

/* Function Name:
 *      rtl8367c_setAsicPortUnmatchedSaMoving
 * Description:
 *      Set Unmatched SA moving state
 * Input:
 *      port        - Port ID
 *      enabled  	- 0: can't move to new port; 1: can move to new port
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_PORT_ID      - Error Port ID
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortUnmatchedSaMoving(rtk_uint32 port, rtk_uint32 enabled)
{
    if(port >= RTL8367C_PORTNO)
        return RT_ERR_PORT_ID;

    return rtl8367c_setAsicRegBit(RTL8367C_REG_L2_SA_MOVING_FORBID, port, (enabled == 1) ? 0 : 1);
}

/* Function Name:
 *      rtl8367c_getAsicPortUnmatchedSaMoving
 * Description:
 *      Get Unmatched SA moving state
 * Input:
 *      port        - Port ID
 * Output:
 *      pEnabled  	- 0: can't move to new port; 1: can move to new port
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_PORT_ID      - Error Port ID
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortUnmatchedSaMoving(rtk_uint32 port, rtk_uint32 *pEnabled)
{
    rtk_uint32 data;
    ret_t retVal;

    if(port >= RTL8367C_PORTNO)
        return RT_ERR_PORT_ID;

    if((retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_L2_SA_MOVING_FORBID, port, &data)) != RT_ERR_OK)
        return retVal;

    *pEnabled = (data == 1) ? 0 : 1;
    return RT_ERR_OK;
}

/* Function Name:
 *      rtl8367c_setAsicPortUnknownDaFloodingPortmask
 * Description:
 *      Set UNDA flooding portmask
 * Input:
 *      portmask 	- portmask(0~0xFF)
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_PORT_MASK 	- Invalid portmask
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortUnknownDaFloodingPortmask(rtk_uint32 portmask)
{
	if(portmask > RTL8367C_PORTMASK)
		return RT_ERR_PORT_MASK;

	return rtl8367c_setAsicReg(RTL8367C_UNUCAST_FLOADING_PMSK_REG, portmask);
}
/* Function Name:
 *      rtl8367c_getAsicPortUnknownDaFloodingPortmask
 * Description:
 *      Get UNDA flooding portmask
 * Input:
 *      pPortmask 	- portmask(0~0xFF)
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 	- Success
 *      RT_ERR_SMI  - SMI access error
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortUnknownDaFloodingPortmask(rtk_uint32 *pPortmask)
{
	return rtl8367c_getAsicReg(RTL8367C_UNUCAST_FLOADING_PMSK_REG, pPortmask);
}
/* Function Name:
 *      rtl8367c_setAsicPortUnknownMulticastFloodingPortmask
 * Description:
 *      Set UNMC flooding portmask
 * Input:
 *      portmask 	- portmask(0~0xFF)
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_PORT_MASK 	- Invalid portmask
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortUnknownMulticastFloodingPortmask(rtk_uint32 portmask)
{
	if(portmask > RTL8367C_PORTMASK)
		return RT_ERR_PORT_MASK;

	return rtl8367c_setAsicReg(RTL8367C_UNMCAST_FLOADING_PMSK_REG, portmask);
}
/* Function Name:
 *      rtl8367c_getAsicPortUnknownMulticastFloodingPortmask
 * Description:
 *      Get UNMC flooding portmask
 * Input:
 *      pPortmask 	- portmask(0~0xFF)
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 	- Success
 *      RT_ERR_SMI  - SMI access error
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortUnknownMulticastFloodingPortmask(rtk_uint32 *pPortmask)
{
	return rtl8367c_getAsicReg(RTL8367C_UNMCAST_FLOADING_PMSK_REG, pPortmask);
}
/* Function Name:
 *      rtl8367c_setAsicPortBcastFloodingPortmask
 * Description:
 *      Set Bcast flooding portmask
 * Input:
 *      portmask 	- portmask(0~0xFF)
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_PORT_MASK 	- Invalid portmask
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortBcastFloodingPortmask(rtk_uint32 portmask)
{
	if(portmask > RTL8367C_PORTMASK)
		return RT_ERR_PORT_MASK;

	return rtl8367c_setAsicReg(RTL8367C_BCAST_FLOADING_PMSK_REG, portmask);
}
/* Function Name:
 *      rtl8367c_getAsicPortBcastFloodingPortmask
 * Description:
 *      Get Bcast flooding portmask
 * Input:
 *      pPortmask 	- portmask(0~0xFF)
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 	- Success
 *      RT_ERR_SMI  - SMI access error
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortBcastFloodingPortmask(rtk_uint32 *pPortmask)
{
	return rtl8367c_getAsicReg(RTL8367C_BCAST_FLOADING_PMSK_REG, pPortmask);
}
/* Function Name:
 *      rtl8367c_setAsicPortBlockSpa
 * Description:
 *      Set disabling blocking frame if source port and destination port are the same
 * Input:
 *      port 	- Physical port number (0~7)
 *      permit 	- 0: block; 1: permit
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 *      RT_ERR_PORT_ID  - Invalid port number
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortBlockSpa(rtk_uint32 port, rtk_uint32 permit)
{
	if(port >= RTL8367C_PORTNO)
		return RT_ERR_PORT_ID;

	return rtl8367c_setAsicRegBit(RTL8367C_SOURCE_PORT_BLOCK_REG, port, permit);
}
/* Function Name:
 *      rtl8367c_getAsicPortBlockSpa
 * Description:
 *      Get disabling blocking frame if source port and destination port are the same
 * Input:
 *      port 	- Physical port number (0~7)
 *      pPermit 	- 0: block; 1: permit
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 *      RT_ERR_PORT_ID  - Invalid port number
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortBlockSpa(rtk_uint32 port, rtk_uint32* pPermit)
{
	if(port >= RTL8367C_PORTNO)
		return RT_ERR_PORT_ID;

	return rtl8367c_getAsicRegBit(RTL8367C_SOURCE_PORT_BLOCK_REG, port, pPermit);
}
/* Function Name:
 *      rtl8367c_setAsicPortDos
 * Description:
 *      Set DOS function
 * Input:
 *      type 	- DOS type
 *      drop 	- 0: permit; 1: drop
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_OUT_OF_RANGE - Invalid payload index
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortDos(rtk_uint32 type, rtk_uint32 drop)
{
	if(type >= DOS_END)
		return RT_ERR_OUT_OF_RANGE;

	return rtl8367c_setAsicRegBit(RTL8367C_REG_DOS_CFG, RTL8367C_DROP_DAEQSA_OFFSET + type, drop);
}
/* Function Name:
 *      rtl8367c_getAsicPortDos
 * Description:
 *      Get DOS function
 * Input:
 *      type 	- DOS type
 *      pDrop 	- 0: permit; 1: drop
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_OUT_OF_RANGE - Invalid payload index
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortDos(rtk_uint32 type, rtk_uint32* pDrop)
{
	if(type >= DOS_END)
		return RT_ERR_OUT_OF_RANGE;

	return rtl8367c_getAsicRegBit(RTL8367C_REG_DOS_CFG, RTL8367C_DROP_DAEQSA_OFFSET + type,pDrop);
}
/* Function Name:
 *      rtl8367c_setAsicPortForceLink
 * Description:
 *      Set port force linking configuration
 * Input:
 *      port 		- Physical port number (0~7)
 *      pPortAbility - port ability configuration
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 *      RT_ERR_PORT_ID  - Invalid port number
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortForceLink(rtk_uint32 port, rtl8367c_port_ability_t *pPortAbility)
{
    rtk_uint32 regData;
    rtk_uint16 *accessPtr;
	rtl8367c_port_ability_t ability;

    /* Invalid input parameter */
    if(port >= RTL8367C_PORTNO)
        return RT_ERR_PORT_ID;

    memset(&ability, 0x00, sizeof(rtl8367c_port_ability_t));
    memcpy(&ability, pPortAbility, sizeof(rtl8367c_port_ability_t));

    accessPtr =  (rtk_uint16*)&ability;

    regData = *accessPtr;
    return rtl8367c_setAsicReg(RTL8367C_REG_MAC0_FORCE_SELECT+port, regData);
}
/* Function Name:
 *      rtl8367c_getAsicPortForceLink
 * Description:
 *      Get port force linking configuration
 * Input:
 *      port 		- Physical port number (0~7)
 *      pPortAbility - port ability configuration
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 *      RT_ERR_PORT_ID  - Invalid port number
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortForceLink(rtk_uint32 port, rtl8367c_port_ability_t *pPortAbility)
{
    ret_t retVal;
    rtk_uint32 regData;
    rtk_uint16 *accessPtr;
    rtl8367c_port_ability_t ability;

    /* Invalid input parameter */
    if(port >= RTL8367C_PORTNO)
        return RT_ERR_PORT_ID;

    memset(&ability, 0x00, sizeof(rtl8367c_port_ability_t));


    accessPtr = (rtk_uint16*)&ability;

    retVal = rtl8367c_getAsicReg(RTL8367C_REG_MAC0_FORCE_SELECT + port, &regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    *accessPtr = regData;

    memcpy(pPortAbility, &ability, sizeof(rtl8367c_port_ability_t));

    return RT_ERR_OK;
}
/* Function Name:
 *      rtl8367c_getAsicPortStatus
 * Description:
 *      Get port link status
 * Input:
 *      port 		- Physical port number (0~7)
 *      pPortAbility - port ability configuration
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 *      RT_ERR_PORT_ID  - Invalid port number
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortStatus(rtk_uint32 port, rtl8367c_port_status_t *pPortStatus)
{
    ret_t retVal;
    rtk_uint32 regData;
    rtk_uint16 *accessPtr;
    rtl8367c_port_status_t status;

    /* Invalid input parameter */
    if(port >= RTL8367C_PORTNO)
        return RT_ERR_PORT_ID;

    memset(&status, 0x00, sizeof(rtl8367c_port_status_t));


    accessPtr =  (rtk_uint16*)&status;

    retVal = rtl8367c_getAsicReg(RTL8367C_REG_PORT0_STATUS+port,&regData);
    if(retVal != RT_ERR_OK)
        return retVal;

    *accessPtr = regData;

    memcpy(pPortStatus, &status, sizeof(rtl8367c_port_status_t));

    return RT_ERR_OK;
}
/* Function Name:
 *      rtl8367c_setAsicPortForceLinkExt
 * Description:
 *      Set external interface force linking configuration
 * Input:
 *      id 			- external interface id (0~2)
 *      portAbility - port ability configuration
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_OUT_OF_RANGE - input parameter out of range
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortForceLinkExt(rtk_uint32 id, rtl8367c_port_ability_t *pPortAbility)
{
    rtk_uint32 retVal;
    rtk_uint32 reg_data;

    /* Invalid input parameter */
    if(id >= RTL8367C_EXTNO)
        return RT_ERR_OUT_OF_RANGE;

    reg_data = (rtk_uint32)(*(rtk_uint16 *)pPortAbility);

    if(1 == id)
    {
        if((retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_SGMII_FDUP_OFFSET, pPortAbility->duplex)) != RT_ERR_OK)
            return retVal;

        if((retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_SGMII_SPD_MASK, pPortAbility->speed)) != RT_ERR_OK)
            return retVal;

        if((retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_SGMII_LINK_OFFSET, pPortAbility->link)) != RT_ERR_OK)
            return retVal;

        if((retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_SGMII_TXFC_OFFSET, pPortAbility->txpause)) != RT_ERR_OK)
            return retVal;

        if((retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_SGMII_RXFC_OFFSET, pPortAbility->rxpause)) != RT_ERR_OK)
            return retVal;
    }

	if(0 == id || 1 == id)
	    return rtl8367c_setAsicReg(RTL8367C_REG_DIGITAL_INTERFACE0_FORCE + id, reg_data);
	else
	    return rtl8367c_setAsicReg(RTL8367C_REG_DIGITAL_INTERFACE2_FORCE, reg_data);
}
/* Function Name:
 *      rtl8367c_getAsicPortForceLinkExt
 * Description:
 *      Get external interface force linking configuration
 * Input:
 *      id 			- external interface id (0~1)
 *      pPortAbility - port ability configuration
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_OUT_OF_RANGE - input parameter out of range
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortForceLinkExt(rtk_uint32 id, rtl8367c_port_ability_t *pPortAbility)
{
    rtk_uint32  reg_data;
    rtk_uint32  sgmiiSel;
    rtk_uint32  hsgmiiSel;
    rtk_uint16  ability_data;
    ret_t       retVal;

    /* Invalid input parameter */
    if(id >= RTL8367C_EXTNO)
        return RT_ERR_OUT_OF_RANGE;

    if(1 == id)
    {
        if((retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_MAC8_SEL_SGMII_OFFSET, &sgmiiSel)) != RT_ERR_OK)
            return retVal;

        if((retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_MAC8_SEL_HSGMII_OFFSET, &hsgmiiSel)) != RT_ERR_OK)
            return retVal;

        if( (sgmiiSel == 1) || (hsgmiiSel == 1) )
        {
            memset(pPortAbility, 0x00, sizeof(rtl8367c_port_ability_t));
            pPortAbility->forcemode = 1;

            if((retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_SGMII_FDUP_OFFSET, &reg_data)) != RT_ERR_OK)
                return retVal;

            pPortAbility->duplex = reg_data;

            if((retVal = rtl8367c_getAsicRegBits(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_SGMII_SPD_MASK, &reg_data)) != RT_ERR_OK)
                return retVal;

            pPortAbility->speed = reg_data;

            if((retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_SGMII_LINK_OFFSET, &reg_data)) != RT_ERR_OK)
                return retVal;

            pPortAbility->link = reg_data;

            if((retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_SGMII_TXFC_OFFSET, &reg_data)) != RT_ERR_OK)
                return retVal;

            pPortAbility->txpause = reg_data;

            if((retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_SGMII_RXFC_OFFSET, &reg_data)) != RT_ERR_OK)
                return retVal;

            pPortAbility->rxpause = reg_data;

            return RT_ERR_OK;
        }
    }

	if(0 == id || 1 == id)
    	retVal = rtl8367c_getAsicReg(RTL8367C_REG_DIGITAL_INTERFACE0_FORCE+id, &reg_data);
	else
	    retVal = rtl8367c_getAsicReg(RTL8367C_REG_DIGITAL_INTERFACE2_FORCE, &reg_data);

    if(retVal != RT_ERR_OK)
        return retVal;

    ability_data = (rtk_uint16)reg_data;
    memcpy(pPortAbility, &ability_data, sizeof(rtl8367c_port_ability_t));

    return RT_ERR_OK;
}
/* Function Name:
 *      rtl8367c_setAsicPortExtMode
 * Description:
 *      Set external interface mode configuration
 * Input:
 *      id 		- external interface id (0~2)
 *      mode 	- external interface mode
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_OUT_OF_RANGE - input parameter out of range
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortExtMode(rtk_uint32 id, rtk_uint32 mode)
{
    ret_t   retVal;

    if(id >= RTL8367C_EXTNO)
        return RT_ERR_OUT_OF_RANGE;

    if(mode >= EXT_END)
        return RT_ERR_OUT_OF_RANGE;

    if( (mode == EXT_SGMII) || (mode == EXT_HSGMII) )
    {
        if(id != 1)
            return RT_ERR_PORT_ID;
    }

    if(mode == EXT_GMII)
    {
        if( (retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_EXT0_RGMXF, RTL8367C_EXT0_RGTX_INV_OFFSET, 1)) != RT_ERR_OK)
            return retVal;

        if( (retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_EXT1_RGMXF, RTL8367C_EXT1_RGTX_INV_OFFSET, 1)) != RT_ERR_OK)
            return retVal;

        if( (retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_EXT_TXC_DLY, RTL8367C_EXT1_GMII_TX_DELAY_MASK, 5)) != RT_ERR_OK)
            return retVal;

        if( (retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_EXT_TXC_DLY, RTL8367C_EXT0_GMII_TX_DELAY_MASK, 6)) != RT_ERR_OK)
            return retVal;
    }

    /* Serdes reset */
    if( (mode == EXT_TMII_MAC) || (mode == EXT_TMII_PHY) )
    {
        if( (retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_BYPASS_LINE_RATE, id, 1)) != RT_ERR_OK)
            return retVal;
    }
    else
    {
        if( (retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_BYPASS_LINE_RATE, id, 0)) != RT_ERR_OK)
            return retVal;
    }

    if( (mode == EXT_SGMII) || (mode == EXT_HSGMII) )
    {
        if( (retVal = rtl8367c_setAsicReg(RTL8367C_REG_SDS_INDACS_DATA, 0x0F80)) != RT_ERR_OK)
            return retVal;

        if( (retVal = rtl8367c_setAsicReg(RTL8367C_REG_SDS_INDACS_ADR, 0x0001)) != RT_ERR_OK)
            return retVal;

        if( (retVal = rtl8367c_setAsicReg(RTL8367C_REG_SDS_INDACS_CMD, 0x00C0)) != RT_ERR_OK)
            return retVal;
    }

    if(mode == EXT_SGMII)
    {
        if( (retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_MAC8_SEL_SGMII_OFFSET, 1)) != RT_ERR_OK)
            return retVal;

        if( (retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_MAC8_SEL_HSGMII_OFFSET, 0)) != RT_ERR_OK)
            return retVal;
    }
    else if(mode == EXT_HSGMII)
    {
        if( (retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_MAC8_SEL_SGMII_OFFSET, 0)) != RT_ERR_OK)
            return retVal;

        if( (retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_MAC8_SEL_HSGMII_OFFSET, 1)) != RT_ERR_OK)
            return retVal;
    }
    else
    {
        if( (retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_MAC8_SEL_SGMII_OFFSET, 0)) != RT_ERR_OK)
            return retVal;

        if( (retVal = rtl8367c_setAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_MAC8_SEL_HSGMII_OFFSET, 0)) != RT_ERR_OK)
            return retVal;

        if(0 == id || 1 == id)
        {
            if((retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_DIGITAL_INTERFACE_SELECT, RTL8367C_SELECT_GMII_0_MASK << (id * RTL8367C_SELECT_GMII_1_OFFSET), mode)) != RT_ERR_OK)
                return retVal;
        }
        else
        {
            if((retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_DIGITAL_INTERFACE_SELECT_1, RTL8367C_SELECT_GMII_2_MASK, mode)) != RT_ERR_OK)
                return retVal;
        }
    }

    /* Serdes not reset */
    if( (mode == EXT_SGMII) || (mode == EXT_HSGMII) )
    {
        if( (retVal = rtl8367c_setAsicReg(RTL8367C_REG_SDS_INDACS_DATA, 0x7106)) != RT_ERR_OK)
            return retVal;

        if( (retVal = rtl8367c_setAsicReg(RTL8367C_REG_SDS_INDACS_ADR, 0x0003)) != RT_ERR_OK)
            return retVal;

        if( (retVal = rtl8367c_setAsicReg(RTL8367C_REG_SDS_INDACS_CMD, 0x00C0)) != RT_ERR_OK)
            return retVal;
    }

    return RT_ERR_OK;
}
/* Function Name:
 *      rtl8367c_getAsicPortExtMode
 * Description:
 *      Get external interface mode configuration
 * Input:
 *      id 		- external interface id (0~1)
 *      pMode 	- external interface mode
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 *      RT_ERR_OUT_OF_RANGE - input parameter out of range
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortExtMode(rtk_uint32 id, rtk_uint32 *pMode)
{
    ret_t   retVal;
    rtk_uint32 regData;

    if(id >= RTL8367C_EXTNO)
        return RT_ERR_OUT_OF_RANGE;

    if (1 == id)
    {
        if( (retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_MAC8_SEL_SGMII_OFFSET, &regData)) != RT_ERR_OK)
            return retVal;

        if(1 == regData)
        {
            *pMode = EXT_SGMII;
            return RT_ERR_OK;
        }

        if( (retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_SDS_MISC, RTL8367C_CFG_MAC8_SEL_HSGMII_OFFSET, &regData)) != RT_ERR_OK)
            return retVal;

        if(1 == regData)
        {
            *pMode = EXT_HSGMII;
            return RT_ERR_OK;
        }
    }

	if(0 == id || 1 == id)
  		return rtl8367c_getAsicRegBits(RTL8367C_REG_DIGITAL_INTERFACE_SELECT, RTL8367C_SELECT_GMII_0_MASK << (id * RTL8367C_SELECT_GMII_1_OFFSET), pMode);
	else
   		return rtl8367c_getAsicRegBits(RTL8367C_REG_DIGITAL_INTERFACE_SELECT_1, RTL8367C_SELECT_GMII_2_MASK, pMode);
}

/* Function Name:
 *      rtl8370_setAsicPortEnableAll
 * Description:
 *      Set ALL ports enable.
 * Input:
 *      enable - enable all ports.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortEnableAll(rtk_uint32 enable)
{
    if(enable >= 2)
        return RT_ERR_INPUT;

    return rtl8367c_setAsicRegBit(RTL8367C_REG_PHY_AD, RTL8367C_PDNPHY_OFFSET, !enable);
}

/* Function Name:
 *      rtl8367c_getAsicPortEnableAll
 * Description:
 *      Set ALL ports enable.
 * Input:
 *      enable - enable all ports.
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 			- Success
 *      RT_ERR_SMI  		- SMI access error
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortEnableAll(rtk_uint32 *pEnable)
{
    ret_t retVal;
    rtk_uint32 regData;

    retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_PHY_AD, RTL8367C_PDNPHY_OFFSET, &regData);
    if(retVal !=  RT_ERR_OK)
        return retVal;

    if (regData==0)
        *pEnable = 1;
    else
        *pEnable = 0;

    return RT_ERR_OK;
}
/* Function Name:
 *      rtl8367c_setAsicPortSmallIpg
 * Description:
 *      Set small ipg egress mode
 * Input:
 *      port 	- Physical port number (0~7)
 *      enable 	- 0: normal, 1: small
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 *      RT_ERR_PORT_ID  - Invalid port number
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortSmallIpg(rtk_uint32 port, rtk_uint32 enable)
{
	if(port >= RTL8367C_PORTNO)
		return RT_ERR_PORT_ID;

	return rtl8367c_setAsicRegBit(RTL8367C_PORT_SMALL_IPG_REG(port), RTL8367C_PORT0_MISC_CFG_SMALL_TAG_IPG_OFFSET, enable);
}

/* Function Name:
 *      rtl8367c_getAsicPortSmallIpg
 * Description:
 *      Get small ipg egress mode
 * Input:
 *      port 	- Physical port number (0~7)
 *      pEnable 	- 0: normal, 1: small
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 *      RT_ERR_PORT_ID  - Invalid port number
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortSmallIpg(rtk_uint32 port, rtk_uint32* pEnable)
{
	if(port >= RTL8367C_PORTNO)
		return RT_ERR_PORT_ID;

	return rtl8367c_getAsicRegBit(RTL8367C_PORT_SMALL_IPG_REG(port), RTL8367C_PORT0_MISC_CFG_SMALL_TAG_IPG_OFFSET, pEnable);
}

/* Function Name:
 *      rtl8367c_setAsicPortLoopback
 * Description:
 *      Set MAC loopback
 * Input:
 *      port 	- Physical port number (0~7)
 *      enable 	- 0: Disable, 1: enable
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 *      RT_ERR_PORT_ID  - Invalid port number
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPortLoopback(rtk_uint32 port, rtk_uint32 enable)
{
    if(port >= RTL8367C_PORTNO)
		return RT_ERR_PORT_ID;

    return rtl8367c_setAsicRegBit(RTL8367C_PORT_MISC_CFG_REG(port), RTL8367C_PORT0_MISC_CFG_MAC_LOOPBACK_OFFSET, enable);
}

/* Function Name:
 *      rtl8367c_getAsicPortLoopback
 * Description:
 *      Set MAC loopback
 * Input:
 *      port 	- Physical port number (0~7)
 * Output:
 *      pEnable - 0: Disable, 1: enable
 * Return:
 *      RT_ERR_OK 		- Success
 *      RT_ERR_SMI  	- SMI access error
 *      RT_ERR_PORT_ID  - Invalid port number
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPortLoopback(rtk_uint32 port, rtk_uint32 *pEnable)
{
    if(port >= RTL8367C_PORTNO)
		return RT_ERR_PORT_ID;

    return rtl8367c_getAsicRegBit(RTL8367C_PORT_MISC_CFG_REG(port), RTL8367C_PORT0_MISC_CFG_MAC_LOOPBACK_OFFSET, pEnable);
}

/* Function Name:
 *      rtl8367c_setAsicPortRTCT
 * Description:
 *      Set RTCT
 * Input:
 *      portmask 	- Port mask of RTCT enabled (0-4)
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK 		    - Success
 *      RT_ERR_SMI  	    - SMI access error
 *      RT_ERR_PORT_MASK    - Invalid port mask
 * Note:
 *      RTCT test takes 4.8 seconds at most.
 */
ret_t rtl8367c_setAsicPortRTCT(rtk_uint32 portmask)
{
    ret_t       retVal;
    rtk_uint32  regData;
    rtk_uint32  port;

    if((retVal = rtl8367c_setAsicReg(0x13C2, 0x0249)) != RT_ERR_OK)
        return retVal;

    if((retVal = rtl8367c_getAsicReg(0x1300, &regData)) != RT_ERR_OK)
        return retVal;

    if( (regData == 0x0276) || (regData == 0x0597) )
        return RT_ERR_CHIP_NOT_SUPPORTED;

    for(port = 0; port <= 10 ; port++)
    {
        if(portmask & (0x0001 << port))
        {
             if((retVal = rtl8367c_getAsicPHYOCPReg(port, 0xa422, &regData)) != RT_ERR_OK)
                 return retVal;

             regData &= 0x7FFF;
             if((retVal = rtl8367c_setAsicPHYOCPReg(port, 0xa422, regData)) != RT_ERR_OK)
                 return retVal;

             regData |= 0x00F0;
             if((retVal = rtl8367c_setAsicPHYOCPReg(port, 0xa422, regData)) != RT_ERR_OK)
                 return retVal;

             regData |= 0x0001;
             if((retVal = rtl8367c_setAsicPHYOCPReg(port, 0xa422, regData)) != RT_ERR_OK)
                 return retVal;
        }
    }

    return RT_ERR_OK;
}

/* Function Name:
 *      rtl8367c_getAsicPortRTCTResult
 * Description:
 *      Get RTCT result
 * Input:
 *      port 	- Port ID of RTCT result
 * Output:
 *      pResult - The result of port ID
 * Return:
 *      RT_ERR_OK 		            - Success
 *      RT_ERR_SMI  	            - SMI access error
 *      RT_ERR_PORT_MASK            - Invalid port mask
 *      RT_ERR_PHY_RTCT_NOT_FINISH  - RTCT test doesn't finish.
 * Note:
 *      RTCT test takes 4.8 seconds at most.
 *      If this API returns RT_ERR_PHY_RTCT_NOT_FINISH,
 *      users should wait a whole then read it again.
 */
ret_t rtl8367c_getAsicPortRTCTResult(rtk_uint32 port, rtl8367c_port_rtct_result_t *pResult)
{
    ret_t       retVal;
    rtk_uint32  regData, finish = 1;

    if((retVal = rtl8367c_setAsicReg(0x13C2, 0x0249)) != RT_ERR_OK)
        return retVal;

    if((retVal = rtl8367c_getAsicReg(0x1300, &regData)) != RT_ERR_OK)
        return retVal;

    if( (regData == 0x6367) )
    {
        if((retVal = rtl8367c_getAsicPHYOCPReg(port, 0xa422, &regData)) != RT_ERR_OK)
            return retVal;

        if((regData & 0x8000) == 0x8000)
        {
            /* Channel A */
            if((retVal = rtl8367c_setAsicPHYOCPReg(port, 0xa436, 0x802a)) != RT_ERR_OK)
                return retVal;

            if((retVal = rtl8367c_getAsicPHYOCPReg(port, 0xa438, &regData)) != RT_ERR_OK)
                return retVal;

            pResult->channelAOpen       = (regData == 0x0048) ? 1 : 0;
            pResult->channelAShort      = (regData == 0x0050) ? 1 : 0;
            pResult->channelAMismatch   = ((regData == 0x0042) || (regData == 0x0044)) ? 1 : 0;
            pResult->channelALinedriver = (regData == 0x0041) ? 1 : 0;

            /* Channel B */
            if((retVal = rtl8367c_setAsicPHYOCPReg(port, 0xa436, 0x802e)) != RT_ERR_OK)
                return retVal;

            if((retVal = rtl8367c_getAsicPHYOCPReg(port, 0xa438, &regData)) != RT_ERR_OK)
                return retVal;

            pResult->channelBOpen       = (regData == 0x0048) ? 1 : 0;
            pResult->channelBShort      = (regData == 0x0050) ? 1 : 0;
            pResult->channelBMismatch   = ((regData == 0x0042) || (regData == 0x0044)) ? 1 : 0;
            pResult->channelBLinedriver = (regData == 0x0041) ? 1 : 0;

            /* Channel C */
            if((retVal = rtl8367c_setAsicPHYOCPReg(port, 0xa436, 0x8032)) != RT_ERR_OK)
                return retVal;

            if((retVal = rtl8367c_getAsicPHYOCPReg(port, 0xa438, &regData)) != RT_ERR_OK)
                return retVal;

            pResult->channelCOpen       = (regData == 0x0048) ? 1 : 0;
            pResult->channelCShort      = (regData == 0x0050) ? 1 : 0;
            pResult->channelCMismatch   = ((regData == 0x0042) || (regData == 0x0044)) ? 1 : 0;
            pResult->channelCLinedriver = (regData == 0x0041) ? 1 : 0;

            /* Channel D */
            if((retVal = rtl8367c_setAsicPHYOCPReg(port, 0xa436, 0x8036)) != RT_ERR_OK)
                return retVal;

            if((retVal = rtl8367c_getAsicPHYOCPReg(port, 0xa438, &regData)) != RT_ERR_OK)
                return retVal;

            pResult->channelDOpen       = (regData == 0x0048) ? 1 : 0;
            pResult->channelDShort      = (regData == 0x0050) ? 1 : 0;
            pResult->channelDMismatch   = ((regData == 0x0042) || (regData == 0x0044)) ? 1 : 0;
            pResult->channelDLinedriver = (regData == 0x0041) ? 1 : 0;

            /* Channel A Length */
            if((retVal = rtl8367c_setAsicPHYOCPReg(port, 0xa436, 0x802c)) != RT_ERR_OK)
                return retVal;

            if((retVal = rtl8367c_getAsicPHYOCPReg(port, 0xa438, &regData)) != RT_ERR_OK)
                return retVal;

            pResult->channelALen = (regData / 2);

            /* Channel B Length */
            if((retVal = rtl8367c_setAsicPHYOCPReg(port, 0xa436, 0x8030)) != RT_ERR_OK)
                return retVal;

            if((retVal = rtl8367c_getAsicPHYOCPReg(port, 0xa438, &regData)) != RT_ERR_OK)
                return retVal;

            pResult->channelBLen = (regData / 2);

            /* Channel C Length */
            if((retVal = rtl8367c_setAsicPHYOCPReg(port, 0xa436, 0x8034)) != RT_ERR_OK)
                return retVal;

            if((retVal = rtl8367c_getAsicPHYOCPReg(port, 0xa438, &regData)) != RT_ERR_OK)
                return retVal;

            pResult->channelCLen = (regData / 2);

            /* Channel D Length */
            if((retVal = rtl8367c_setAsicPHYOCPReg(port, 0xa436, 0x8038)) != RT_ERR_OK)
                return retVal;

            if((retVal = rtl8367c_getAsicPHYOCPReg(port, 0xa438, &regData)) != RT_ERR_OK)
                return retVal;

            pResult->channelDLen = (regData / 2);
        }
        else
            finish = 0;
    }
    else
        return RT_ERR_CHIP_NOT_SUPPORTED;

    if(finish == 0)
        return RT_ERR_PHY_RTCT_NOT_FINISH;
    else
        return RT_ERR_OK;
}
