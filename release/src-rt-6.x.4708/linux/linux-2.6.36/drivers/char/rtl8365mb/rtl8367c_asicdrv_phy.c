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
 * $Revision: 48276 $
 * $Date: 2014-06-05 15:21:01 +0800 (週四, 05 六月 2014) $
 *
 * Purpose : RTL8367C switch high-level API for RTL8367C
 * Feature : PHY related functions
 *
 */
#include <rtl8367c_asicdrv_phy.h>

#if defined(MDC_MDIO_OPERATION)
/* Function Name:
 *      rtl8367c_setAsicPHYReg
 * Description:
 *      Set PHY registers
 * Input:
 *      phyNo 	- Physical port number (0~4)
 *      phyAddr - PHY address (0~31)
 *      phyData - Writing data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 				- Success
 *      RT_ERR_SMI  			- SMI access error
 *      RT_ERR_PHY_REG_ID  		- invalid PHY address
 *      RT_ERR_PHY_ID  			- invalid PHY no
 *      RT_ERR_BUSYWAIT_TIMEOUT - PHY access busy
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPHYReg( rtk_uint32 phyNo, rtk_uint32 phyAddr, rtk_uint32 value)
{
    ret_t retVal;
    rtk_uint32 regAddr;

#ifdef RTK_X86_CLE

#else
    if(phyNo > RTL8367C_PHY_INTERNALNOMAX)
        return RT_ERR_PORT_ID;
#endif

    if(phyAddr > RTL8367C_PHY_REGNOMAX)
        return RT_ERR_PHY_REG_ID;

    /* Default OCP Address */
    if((retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_GPHY_OCP_MSB_0, RTL8367C_CFG_CPU_OCPADR_MSB_MASK, 0x29)) != RT_ERR_OK)
        return retVal;

    regAddr = 0x2000 + (phyNo << 5) + phyAddr;

    return rtl8367c_setAsicReg(regAddr, value);
}

/* Function Name:
 *      rtl8367c_getAsicPHYReg
 * Description:
 *      Get PHY registers
 * Input:
 *      phyNo 	- Physical port number (0~4)
 *      phyAddr - PHY address (0~31)
 *      pRegData - Writing data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 				- Success
 *      RT_ERR_SMI  			- SMI access error
 *      RT_ERR_PHY_REG_ID  		- invalid PHY address
 *      RT_ERR_PHY_ID  			- invalid PHY no
 *      RT_ERR_BUSYWAIT_TIMEOUT - PHY access busy
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPHYReg( rtk_uint32 phyNo, rtk_uint32 phyAddr, rtk_uint32 *value)
{
    ret_t retVal;
    rtk_uint32 regAddr;

#ifdef RTK_X86_CLE

#else
    if(phyNo > RTL8367C_PHY_INTERNALNOMAX)
        return RT_ERR_PORT_ID;
#endif

    if(phyAddr > RTL8367C_PHY_REGNOMAX)
        return RT_ERR_PHY_REG_ID;

    /* Default OCP Address */
    if((retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_GPHY_OCP_MSB_0, RTL8367C_CFG_CPU_OCPADR_MSB_MASK, 0x29)) != RT_ERR_OK)
        return retVal;

    regAddr = 0x2000 + (phyNo << 5) + phyAddr;

    return rtl8367c_getAsicReg(regAddr, value);
}

/* Function Name:
 *      rtl8367c_setAsicPHYOCPReg
 * Description:
 *      Set PHY OCP registers
 * Input:
 *      phyNo 	- Physical port number (0~7)
 *      ocpAddr - OCP address
 *      ocpData - Writing data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 				- Success
 *      RT_ERR_SMI  			- SMI access error
 *      RT_ERR_PHY_REG_ID  		- invalid PHY address
 *      RT_ERR_PHY_ID  			- invalid PHY no
 *      RT_ERR_BUSYWAIT_TIMEOUT - PHY access busy
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPHYOCPReg(rtk_uint32 phyNo, rtk_uint32 ocpAddr, rtk_uint32 ocpData )
{
    ret_t retVal;
	rtk_uint32 regAddr;
    rtk_uint32 ocpAddrPrefix, ocpAddr9_6, ocpAddr5_1;

    /* OCP prefix */
    ocpAddrPrefix = ((ocpAddr & 0xFC00) >> 10);
    if((retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_GPHY_OCP_MSB_0, RTL8367C_CFG_CPU_OCPADR_MSB_MASK, ocpAddrPrefix)) != RT_ERR_OK)
        return retVal;

    /*prepare access address*/
    ocpAddr9_6 = ((ocpAddr >> 6) & 0x000F);
    ocpAddr5_1 = ((ocpAddr >> 1) & 0x001F);
    regAddr = RTL8367C_PHY_BASE | (ocpAddr9_6 << 8) | (phyNo << RTL8367C_PHY_OFFSET) | ocpAddr5_1;
    if((retVal = rtl8367c_setAsicReg(regAddr, ocpData)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtl8367c_getAsicPHYOCPReg
 * Description:
 *      Get PHY OCP registers
 * Input:
 *      phyNo 	- Physical port number (0~7)
 *      ocpAddr - PHY address
 *      pRegData - read data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 				- Success
 *      RT_ERR_SMI  			- SMI access error
 *      RT_ERR_PHY_REG_ID  		- invalid PHY address
 *      RT_ERR_PHY_ID  			- invalid PHY no
 *      RT_ERR_BUSYWAIT_TIMEOUT - PHY access busy
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPHYOCPReg(rtk_uint32 phyNo, rtk_uint32 ocpAddr, rtk_uint32 *pRegData )
{
    ret_t retVal;
	rtk_uint32 regAddr;
    rtk_uint32 ocpAddrPrefix, ocpAddr9_6, ocpAddr5_1;

    /* OCP prefix */
    ocpAddrPrefix = ((ocpAddr & 0xFC00) >> 10);
    if((retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_GPHY_OCP_MSB_0, RTL8367C_CFG_CPU_OCPADR_MSB_MASK, ocpAddrPrefix)) != RT_ERR_OK)
        return retVal;

    /*prepare access address*/
    ocpAddr9_6 = ((ocpAddr >> 6) & 0x000F);
    ocpAddr5_1 = ((ocpAddr >> 1) & 0x001F);
    regAddr = RTL8367C_PHY_BASE | (ocpAddr9_6 << 8) | (phyNo << RTL8367C_PHY_OFFSET) | ocpAddr5_1;
    if((retVal = rtl8367c_getAsicReg(regAddr, pRegData)) != RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

#else

/* Function Name:
 *      rtl8367c_setAsicPHYReg
 * Description:
 *      Set PHY registers
 * Input:
 *      phyNo 	- Physical port number (0~7)
 *      phyAddr - PHY address (0~31)
 *      phyData - Writing data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 				- Success
 *      RT_ERR_SMI  			- SMI access error
 *      RT_ERR_PHY_REG_ID  		- invalid PHY address
 *      RT_ERR_PHY_ID  			- invalid PHY no
 *      RT_ERR_BUSYWAIT_TIMEOUT - PHY access busy
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPHYReg(rtk_uint32 phyNo, rtk_uint32 phyAddr, rtk_uint32 phyData )
{
	ret_t retVal;
	rtk_uint32 regData;
    rtk_uint32 busyFlag, checkCounter;

#ifdef RTK_X86_CLE

#else
	if(phyNo > RTL8367C_PHY_INTERNALNOMAX)
		return RT_ERR_PHY_ID;
#endif

	if(phyAddr > RTL8367C_PHY_REGNOMAX)
		return RT_ERR_PHY_REG_ID;

    /*Check internal phy access busy or not*/
    /*retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_INDRECT_ACCESS_STATUS, RTL8367C_INDRECT_ACCESS_STATUS_OFFSET,&busyFlag);*/
    retVal = rtl8367c_getAsicReg(RTL8367C_REG_INDRECT_ACCESS_STATUS,&busyFlag);
	if(retVal != RT_ERR_OK)
		return retVal;

    if(busyFlag)
        return RT_ERR_BUSYWAIT_TIMEOUT;

    /* Default OCP Address */
    if((retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_GPHY_OCP_MSB_0, RTL8367C_CFG_CPU_OCPADR_MSB_MASK, 0x29)) != RT_ERR_OK)
        return retVal;

    /*prepare access data*/
    retVal = rtl8367c_setAsicReg(RTL8367C_REG_INDRECT_ACCESS_WRITE_DATA, phyData);
	if(retVal != RT_ERR_OK)
		return retVal;

    /*prepare access address*/
    regData = RTL8367C_PHY_BASE | (phyNo << RTL8367C_PHY_OFFSET) | phyAddr;

    retVal = rtl8367c_setAsicReg(RTL8367C_REG_INDRECT_ACCESS_ADDRESS, regData);
	if(retVal != RT_ERR_OK)
		return retVal;

    /*Set WRITE Command*/
    retVal = rtl8367c_setAsicReg(RTL8367C_REG_INDRECT_ACCESS_CTRL, RTL8367C_CMD_MASK | RTL8367C_RW_MASK);

    checkCounter = 100;
	while(checkCounter)
	{
    	retVal = rtl8367c_getAsicReg(RTL8367C_REG_INDRECT_ACCESS_STATUS,&busyFlag);
		if((retVal != RT_ERR_OK) || busyFlag)
		{
			checkCounter --;
			if(0 == checkCounter)
                return RT_ERR_BUSYWAIT_TIMEOUT;
		}
		else
		{
			checkCounter = 0;
		}
	}

    return retVal;
}
/* Function Name:
 *      rtl8367c_getAsicPHYReg
 * Description:
 *      Get PHY registers
 * Input:
 *      phyNo 	- Physical port number (0~7)
 *      phyAddr - PHY address (0~31)
 *      pRegData - Writing data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 				- Success
 *      RT_ERR_SMI  			- SMI access error
 *      RT_ERR_PHY_REG_ID  		- invalid PHY address
 *      RT_ERR_PHY_ID  			- invalid PHY no
 *      RT_ERR_BUSYWAIT_TIMEOUT - PHY access busy
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPHYReg(rtk_uint32 phyNo, rtk_uint32 phyAddr, rtk_uint32 *pRegData )
{
	ret_t retVal;
	rtk_uint32 regData;
    rtk_uint32 busyFlag,checkCounter;

#ifdef RTK_X86_CLE

#else
	if(phyNo > RTL8367C_PHY_INTERNALNOMAX)
		return RT_ERR_PHY_ID;
#endif
	if(phyAddr > RTL8367C_PHY_REGNOMAX)
		return RT_ERR_PHY_REG_ID;

    /*Check internal phy access busy or not*/
    /*retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_INDRECT_ACCESS_STATUS, RTL8367C_INDRECT_ACCESS_STATUS_OFFSET,&busyFlag);*/
    retVal = rtl8367c_getAsicReg(RTL8367C_REG_INDRECT_ACCESS_STATUS,&busyFlag);
	if(retVal != RT_ERR_OK)
		return retVal;

    if(busyFlag)
        return RT_ERR_BUSYWAIT_TIMEOUT;

    /* Default OCP Address */
    if((retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_GPHY_OCP_MSB_0, RTL8367C_CFG_CPU_OCPADR_MSB_MASK, 0x29)) != RT_ERR_OK)
        return retVal;

    /*prepare access address*/
    regData = RTL8367C_PHY_BASE | (phyNo << RTL8367C_PHY_OFFSET) | phyAddr;

    retVal = rtl8367c_setAsicReg(RTL8367C_REG_INDRECT_ACCESS_ADDRESS, regData);
	if(retVal != RT_ERR_OK)
		return retVal;

    /*Set READ Command*/
    retVal = rtl8367c_setAsicReg(RTL8367C_REG_INDRECT_ACCESS_CTRL, RTL8367C_CMD_MASK );
	if(retVal != RT_ERR_OK)
		return retVal;

	checkCounter = 100;
	while(checkCounter)
	{
    	retVal = rtl8367c_getAsicReg(RTL8367C_REG_INDRECT_ACCESS_STATUS,&busyFlag);
		if((retVal != RT_ERR_OK) || busyFlag)
		{
			checkCounter --;
			if(0 == checkCounter)
                return RT_ERR_FAILED;
		}
		else
		{
			checkCounter = 0;
		}
	}

    /*get PHY register*/
    retVal = rtl8367c_getAsicReg(RTL8367C_REG_INDRECT_ACCESS_READ_DATA, &regData);
	if(retVal != RT_ERR_OK)
		return retVal;

    *pRegData = regData;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtl8367c_setAsicPHYOCPReg
 * Description:
 *      Set PHY OCP registers
 * Input:
 *      phyNo 	- Physical port number (0~7)
 *      ocpAddr - OCP address
 *      ocpData - Writing data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 				- Success
 *      RT_ERR_SMI  			- SMI access error
 *      RT_ERR_PHY_REG_ID  		- invalid PHY address
 *      RT_ERR_PHY_ID  			- invalid PHY no
 *      RT_ERR_BUSYWAIT_TIMEOUT - PHY access busy
 * Note:
 *      None
 */
ret_t rtl8367c_setAsicPHYOCPReg(rtk_uint32 phyNo, rtk_uint32 ocpAddr, rtk_uint32 ocpData )
{
	ret_t retVal;
	rtk_uint32 regData;
    rtk_uint32 busyFlag, checkCounter;
    rtk_uint32 ocpAddrPrefix, ocpAddr9_6, ocpAddr5_1;

    /*Check internal phy access busy or not*/
    /*retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_INDRECT_ACCESS_STATUS, RTL8367C_INDRECT_ACCESS_STATUS_OFFSET,&busyFlag);*/
    retVal = rtl8367c_getAsicReg(RTL8367C_REG_INDRECT_ACCESS_STATUS,&busyFlag);
	if(retVal != RT_ERR_OK)
		return retVal;

    if(busyFlag)
        return RT_ERR_BUSYWAIT_TIMEOUT;

    /* OCP prefix */
    ocpAddrPrefix = ((ocpAddr & 0xFC00) >> 10);
    if((retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_GPHY_OCP_MSB_0, RTL8367C_CFG_CPU_OCPADR_MSB_MASK, ocpAddrPrefix)) != RT_ERR_OK)
        return retVal;

    /*prepare access data*/
    retVal = rtl8367c_setAsicReg(RTL8367C_REG_INDRECT_ACCESS_WRITE_DATA, ocpData);
	if(retVal != RT_ERR_OK)
		return retVal;

    /*prepare access address*/
    ocpAddr9_6 = ((ocpAddr >> 6) & 0x000F);
    ocpAddr5_1 = ((ocpAddr >> 1) & 0x001F);
    regData = RTL8367C_PHY_BASE | (ocpAddr9_6 << 8) | (phyNo << RTL8367C_PHY_OFFSET) | ocpAddr5_1;
    retVal = rtl8367c_setAsicReg(RTL8367C_REG_INDRECT_ACCESS_ADDRESS, regData);
	if(retVal != RT_ERR_OK)
		return retVal;

    /*Set WRITE Command*/
    retVal = rtl8367c_setAsicReg(RTL8367C_REG_INDRECT_ACCESS_CTRL, RTL8367C_CMD_MASK | RTL8367C_RW_MASK);

    checkCounter = 100;
	while(checkCounter)
	{
    	retVal = rtl8367c_getAsicReg(RTL8367C_REG_INDRECT_ACCESS_STATUS,&busyFlag);
		if((retVal != RT_ERR_OK) || busyFlag)
		{
			checkCounter --;
			if(0 == checkCounter)
                return RT_ERR_BUSYWAIT_TIMEOUT;
		}
		else
		{
			checkCounter = 0;
		}
	}

    return retVal;
}
/* Function Name:
 *      rtl8367c_getAsicPHYOCPReg
 * Description:
 *      Get PHY OCP registers
 * Input:
 *      phyNo 	- Physical port number (0~7)
 *      ocpAddr - PHY address
 *      pRegData - read data
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK 				- Success
 *      RT_ERR_SMI  			- SMI access error
 *      RT_ERR_PHY_REG_ID  		- invalid PHY address
 *      RT_ERR_PHY_ID  			- invalid PHY no
 *      RT_ERR_BUSYWAIT_TIMEOUT - PHY access busy
 * Note:
 *      None
 */
ret_t rtl8367c_getAsicPHYOCPReg(rtk_uint32 phyNo, rtk_uint32 ocpAddr, rtk_uint32 *pRegData )
{
	ret_t retVal;
	rtk_uint32 regData;
    rtk_uint32 busyFlag,checkCounter;
    rtk_uint32 ocpAddrPrefix, ocpAddr9_6, ocpAddr5_1;

    /*Check internal phy access busy or not*/
    /*retVal = rtl8367c_getAsicRegBit(RTL8367C_REG_INDRECT_ACCESS_STATUS, RTL8367C_INDRECT_ACCESS_STATUS_OFFSET,&busyFlag);*/
    retVal = rtl8367c_getAsicReg(RTL8367C_REG_INDRECT_ACCESS_STATUS,&busyFlag);
	if(retVal != RT_ERR_OK)
		return retVal;

    if(busyFlag)
        return RT_ERR_BUSYWAIT_TIMEOUT;

    /* OCP prefix */
    ocpAddrPrefix = ((ocpAddr & 0xFC00) >> 10);
    if((retVal = rtl8367c_setAsicRegBits(RTL8367C_REG_GPHY_OCP_MSB_0, RTL8367C_CFG_CPU_OCPADR_MSB_MASK, ocpAddrPrefix)) != RT_ERR_OK)
        return retVal;

    /*prepare access address*/
    ocpAddr9_6 = ((ocpAddr >> 6) & 0x000F);
    ocpAddr5_1 = ((ocpAddr >> 1) & 0x001F);
    regData = RTL8367C_PHY_BASE | (ocpAddr9_6 << 8) | (phyNo << RTL8367C_PHY_OFFSET) | ocpAddr5_1;
    retVal = rtl8367c_setAsicReg(RTL8367C_REG_INDRECT_ACCESS_ADDRESS, regData);
	if(retVal != RT_ERR_OK)
		return retVal;

    /*Set READ Command*/
    retVal = rtl8367c_setAsicReg(RTL8367C_REG_INDRECT_ACCESS_CTRL, RTL8367C_CMD_MASK );
	if(retVal != RT_ERR_OK)
		return retVal;

	checkCounter = 100;
	while(checkCounter)
	{
    	retVal = rtl8367c_getAsicReg(RTL8367C_REG_INDRECT_ACCESS_STATUS,&busyFlag);
		if((retVal != RT_ERR_OK) || busyFlag)
		{
			checkCounter --;
			if(0 == checkCounter)
                return RT_ERR_FAILED;
		}
		else
		{
			checkCounter = 0;
		}
	}

    /*get PHY register*/
    retVal = rtl8367c_getAsicReg(RTL8367C_REG_INDRECT_ACCESS_READ_DATA, &regData);
	if(retVal != RT_ERR_OK)
		return retVal;

    *pRegData = regData;

    return RT_ERR_OK;
}
#endif
