#include "r8192U.h"
#include "r8192U_hw.h"
#include "r819xU_phy.h"
#include "r819xU_phyreg.h"
#include "r8190_rtl8256.h"
#include "r8192U_dm.h"
#include "r819xU_firmware_img.h"

#ifdef ENABLE_DOT11D
#include "dot11d.h"
#endif
static u32 RF_CHANNEL_TABLE_ZEBRA[] = {
	0,
	0x085c, //2412 1
	0x08dc, //2417 2
	0x095c, //2422 3
	0x09dc, //2427 4
	0x0a5c, //2432 5
	0x0adc, //2437 6
	0x0b5c, //2442 7
	0x0bdc, //2447 8
	0x0c5c, //2452 9
	0x0cdc, //2457 10
	0x0d5c, //2462 11
	0x0ddc, //2467 12
	0x0e5c, //2472 13
	0x0f72, //2484
};


#define rtl819XPHY_REG_1T2RArray Rtl8192UsbPHY_REG_1T2RArray
#define rtl819XMACPHY_Array_PG Rtl8192UsbMACPHY_Array_PG
#define rtl819XMACPHY_Array Rtl8192UsbMACPHY_Array
#define rtl819XRadioA_Array  Rtl8192UsbRadioA_Array
#define rtl819XRadioB_Array Rtl8192UsbRadioB_Array
#define rtl819XRadioC_Array Rtl8192UsbRadioC_Array
#define rtl819XRadioD_Array Rtl8192UsbRadioD_Array
#define rtl819XAGCTAB_Array Rtl8192UsbAGCTAB_Array

/******************************************************************************
 *function:  This function read BB parameters from Header file we gen,
 *	     and do register read/write
 *   input:  u32	dwBitMask  //taget bit pos in the addr to be modified
 *  output:  none
 *  return:  u32	return the shift bit bit position of the mask
 * ****************************************************************************/
u32 rtl8192_CalculateBitShift(u32 dwBitMask)
{
	u32 i;
	for (i=0; i<=31; i++)
	{
		if (((dwBitMask>>i)&0x1) == 1)
			break;
	}
	return i;
}
/******************************************************************************
 *function:  This function check different RF type to execute legal judgement. If RF Path is illegal, we will return false.
 *   input:  none
 *  output:  none
 *  return:  0(illegal, false), 1(legal,true)
 * ***************************************************************************/
u8 rtl8192_phy_CheckIsLegalRFPath(struct net_device* dev, u32 eRFPath)
{
	u8 ret = 1;
	struct r8192_priv *priv = ieee80211_priv(dev);
	if (priv->rf_type == RF_2T4R)
		ret = 0;
	else if (priv->rf_type == RF_1T2R)
	{
		if (eRFPath == RF90_PATH_A || eRFPath == RF90_PATH_B)
			ret = 1;
		else if (eRFPath == RF90_PATH_C || eRFPath == RF90_PATH_D)
			ret = 0;
	}
	return ret;
}
/******************************************************************************
 *function:  This function set specific bits to BB register
 *   input:  net_device dev
 *           u32	dwRegAddr  //target addr to be modified
 *           u32	dwBitMask  //taget bit pos in the addr to be modified
 *           u32	dwData     //value to be write
 *  output:  none
 *  return:  none
 *  notice:
 * ****************************************************************************/
void rtl8192_setBBreg(struct net_device* dev, u32 dwRegAddr, u32 dwBitMask, u32 dwData)
{

	u32 OriginalValue, BitShift, NewValue;

	if(dwBitMask!= bMaskDWord)
	{//if not "double word" write
		OriginalValue = read_nic_dword(dev, dwRegAddr);
		BitShift = rtl8192_CalculateBitShift(dwBitMask);
		NewValue = (((OriginalValue) & (~dwBitMask)) | (dwData << BitShift));
		write_nic_dword(dev, dwRegAddr, NewValue);
	}else
		write_nic_dword(dev, dwRegAddr, dwData);
	return;
}
/******************************************************************************
 *function:  This function reads specific bits from BB register
 *   input:  net_device dev
 *           u32	dwRegAddr  //target addr to be readback
 *           u32	dwBitMask  //taget bit pos in the addr to be readback
 *  output:  none
 *  return:  u32	Data	//the readback register value
 *  notice:
 * ****************************************************************************/
u32 rtl8192_QueryBBReg(struct net_device* dev, u32 dwRegAddr, u32 dwBitMask)
{
	u32 Ret = 0, OriginalValue, BitShift;

	OriginalValue = read_nic_dword(dev, dwRegAddr);
	BitShift = rtl8192_CalculateBitShift(dwBitMask);
	Ret =(OriginalValue & dwBitMask) >> BitShift;

	return (Ret);
}
static  u32 phy_FwRFSerialRead( struct net_device* dev, RF90_RADIO_PATH_E       eRFPath, u32 Offset  );

static void phy_FwRFSerialWrite( struct net_device* dev, RF90_RADIO_PATH_E       eRFPath, u32  Offset, u32  Data);

/******************************************************************************
 *function:  This function read register from RF chip
 *   input:  net_device dev
 *   	     RF90_RADIO_PATH_E eRFPath //radio path of A/B/C/D
 *           u32	Offset     //target address to be read
 *  output:  none
 *  return:  u32 	readback value
 *  notice:  There are three types of serial operations:(1) Software serial write.(2)Hardware LSSI-Low Speed Serial Interface.(3)Hardware HSSI-High speed serial write. Driver here need to implement (1) and (2)---need more spec for this information.
 * ****************************************************************************/
u32 rtl8192_phy_RFSerialRead(struct net_device* dev, RF90_RADIO_PATH_E eRFPath, u32 Offset)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u32 ret = 0;
	u32 NewOffset = 0;
	BB_REGISTER_DEFINITION_T* pPhyReg = &priv->PHYRegDef[eRFPath];
	rtl8192_setBBreg(dev, pPhyReg->rfLSSIReadBack, bLSSIReadBackData, 0);
	//make sure RF register offset is correct
	Offset &= 0x3f;

	//switch page for 8256 RF IC
	if (priv->rf_chip == RF_8256)
	{
		if (Offset >= 31)
		{
			priv->RfReg0Value[eRFPath] |= 0x140;
			//Switch to Reg_Mode2 for Reg 31-45
			rtl8192_setBBreg(dev, pPhyReg->rf3wireOffset, bMaskDWord, (priv->RfReg0Value[eRFPath]<<16) );
			//modify offset
			NewOffset = Offset -30;
		}
		else if (Offset >= 16)
		{
			priv->RfReg0Value[eRFPath] |= 0x100;
			priv->RfReg0Value[eRFPath] &= (~0x40);
			//Switch to Reg_Mode 1 for Reg16-30
			rtl8192_setBBreg(dev, pPhyReg->rf3wireOffset, bMaskDWord, (priv->RfReg0Value[eRFPath]<<16) );

			NewOffset = Offset - 15;
		}
		else
			NewOffset = Offset;
	}
	else
	{
		RT_TRACE((COMP_PHY|COMP_ERR), "check RF type here, need to be 8256\n");
		NewOffset = Offset;
	}
	//put desired read addr to LSSI control Register
	rtl8192_setBBreg(dev, pPhyReg->rfHSSIPara2, bLSSIReadAddress, NewOffset);
	//Issue a posedge trigger
	//
	rtl8192_setBBreg(dev, pPhyReg->rfHSSIPara2,  bLSSIReadEdge, 0x0);
	rtl8192_setBBreg(dev, pPhyReg->rfHSSIPara2,  bLSSIReadEdge, 0x1);


	// TODO: we should not delay such a  long time. Ask help from SD3
	msleep(1);

	ret = rtl8192_QueryBBReg(dev, pPhyReg->rfLSSIReadBack, bLSSIReadBackData);


	// Switch back to Reg_Mode0;
	if(priv->rf_chip == RF_8256)
	{
		priv->RfReg0Value[eRFPath] &= 0xebf;

		rtl8192_setBBreg(
			dev,
			pPhyReg->rf3wireOffset,
			bMaskDWord,
			(priv->RfReg0Value[eRFPath] << 16));
	}

	return ret;

}

/******************************************************************************
 *function:  This function write data to RF register
 *   input:  net_device dev
 *   	     RF90_RADIO_PATH_E eRFPath //radio path of A/B/C/D
 *           u32	Offset     //target address to be written
 *           u32	Data	//The new register data to be written
 *  output:  none
 *  return:  none
 *  notice:  For RF8256 only.
  ===========================================================
 *Reg Mode	RegCTL[1]	RegCTL[0]		Note
 *		(Reg00[12])	(Reg00[10])
 *===========================================================
 *Reg_Mode0	0		x			Reg 0 ~15(0x0 ~ 0xf)
 *------------------------------------------------------------------
 *Reg_Mode1	1		0			Reg 16 ~30(0x1 ~ 0xf)
 *------------------------------------------------------------------
 * Reg_Mode2	1		1			Reg 31 ~ 45(0x1 ~ 0xf)
 *------------------------------------------------------------------
 * ****************************************************************************/
void rtl8192_phy_RFSerialWrite(struct net_device* dev, RF90_RADIO_PATH_E eRFPath, u32 Offset, u32 Data)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u32 DataAndAddr = 0, NewOffset = 0;
	BB_REGISTER_DEFINITION_T	*pPhyReg = &priv->PHYRegDef[eRFPath];

	Offset &= 0x3f;
	//spin_lock_irqsave(&priv->rf_lock, flags);
//	down(&priv->rf_sem);
	if (priv->rf_chip == RF_8256)
	{

		if (Offset >= 31)
		{
			priv->RfReg0Value[eRFPath] |= 0x140;
			rtl8192_setBBreg(dev, pPhyReg->rf3wireOffset, bMaskDWord, (priv->RfReg0Value[eRFPath] << 16));
			NewOffset = Offset - 30;
		}
		else if (Offset >= 16)
		{
			priv->RfReg0Value[eRFPath] |= 0x100;
			priv->RfReg0Value[eRFPath] &= (~0x40);
			rtl8192_setBBreg(dev, pPhyReg->rf3wireOffset, bMaskDWord, (priv->RfReg0Value[eRFPath]<<16));
			NewOffset = Offset - 15;
		}
		else
			NewOffset = Offset;
	}
	else
	{
		RT_TRACE((COMP_PHY|COMP_ERR), "check RF type here, need to be 8256\n");
		NewOffset = Offset;
	}

	// Put write addr in [5:0]  and write data in [31:16]
	DataAndAddr = (Data<<16) | (NewOffset&0x3f);

	// Write Operation
	rtl8192_setBBreg(dev, pPhyReg->rf3wireOffset, bMaskDWord, DataAndAddr);


	if(Offset==0x0)
		priv->RfReg0Value[eRFPath] = Data;

	// Switch back to Reg_Mode0;
	if(priv->rf_chip == RF_8256)
	{
		if(Offset != 0)
		{
			priv->RfReg0Value[eRFPath] &= 0xebf;
			rtl8192_setBBreg(
				dev,
				pPhyReg->rf3wireOffset,
				bMaskDWord,
				(priv->RfReg0Value[eRFPath] << 16));
		}
	}
	//spin_unlock_irqrestore(&priv->rf_lock, flags);
//	up(&priv->rf_sem);
	return;
}

/******************************************************************************
 *function:  This function set specific bits to RF register
 *   input:  net_device dev
 *   	     RF90_RADIO_PATH_E eRFPath //radio path of A/B/C/D
 *           u32	RegAddr  //target addr to be modified
 *           u32	BitMask  //taget bit pos in the addr to be modified
 *           u32	Data     //value to be write
 *  output:  none
 *  return:  none
 *  notice:
 * ****************************************************************************/
void rtl8192_phy_SetRFReg(struct net_device* dev, RF90_RADIO_PATH_E eRFPath, u32 RegAddr, u32 BitMask, u32 Data)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u32 Original_Value, BitShift, New_Value;
//	u8	time = 0;

	if (!rtl8192_phy_CheckIsLegalRFPath(dev, eRFPath))
		return;

	if (priv->Rf_Mode == RF_OP_By_FW)
	{
		if (BitMask != bMask12Bits) // RF data is 12 bits only
		{
			Original_Value = phy_FwRFSerialRead(dev, eRFPath, RegAddr);
			BitShift =  rtl8192_CalculateBitShift(BitMask);
			New_Value = ((Original_Value) & (~BitMask)) | (Data<< BitShift);

			phy_FwRFSerialWrite(dev, eRFPath, RegAddr, New_Value);
		}else
			phy_FwRFSerialWrite(dev, eRFPath, RegAddr, Data);

		udelay(200);

	}
	else
	{
		if (BitMask != bMask12Bits) // RF data is 12 bits only
		{
			Original_Value = rtl8192_phy_RFSerialRead(dev, eRFPath, RegAddr);
			BitShift =  rtl8192_CalculateBitShift(BitMask);
			New_Value = (((Original_Value) & (~BitMask)) | (Data<< BitShift));

			rtl8192_phy_RFSerialWrite(dev, eRFPath, RegAddr, New_Value);
		}else
			rtl8192_phy_RFSerialWrite(dev, eRFPath, RegAddr, Data);
	}
	return;
}

/******************************************************************************
 *function:  This function reads specific bits from RF register
 *   input:  net_device dev
 *           u32	RegAddr  //target addr to be readback
 *           u32	BitMask  //taget bit pos in the addr to be readback
 *  output:  none
 *  return:  u32	Data	//the readback register value
 *  notice:
 * ****************************************************************************/
u32 rtl8192_phy_QueryRFReg(struct net_device* dev, RF90_RADIO_PATH_E eRFPath, u32 RegAddr, u32 BitMask)
{
	u32 Original_Value, Readback_Value, BitShift;
	struct r8192_priv *priv = ieee80211_priv(dev);


	if (!rtl8192_phy_CheckIsLegalRFPath(dev, eRFPath))
		return 0;
	if (priv->Rf_Mode == RF_OP_By_FW)
	{
		Original_Value = phy_FwRFSerialRead(dev, eRFPath, RegAddr);
		BitShift =  rtl8192_CalculateBitShift(BitMask);
		Readback_Value = (Original_Value & BitMask) >> BitShift;
		udelay(200);
		return (Readback_Value);
	}
	else
	{
		Original_Value = rtl8192_phy_RFSerialRead(dev, eRFPath, RegAddr);
		BitShift =  rtl8192_CalculateBitShift(BitMask);
		Readback_Value = (Original_Value & BitMask) >> BitShift;
		return (Readback_Value);
	}
}
/******************************************************************************
 *function:  We support firmware to execute RF-R/W.
 *   input:  dev
 *  output:  none
 *  return:  none
 *  notice:
 * ***************************************************************************/
static	u32
phy_FwRFSerialRead(
	struct net_device* dev,
	RF90_RADIO_PATH_E	eRFPath,
	u32				Offset	)
{
	u32		retValue = 0;
	u32		Data = 0;
	u8		time = 0;
	//DbgPrint("FW RF CTRL\n\r");
	/* 2007/11/02 MH Firmware RF Write control. By Francis' suggestion, we can
	   not execute the scheme in the initial step. Otherwise, RF-R/W will waste
	   much time. This is only for site survey. */
	// 1. Read operation need not insert data. bit 0-11
	//Data &= bMask12Bits;
	// 2. Write RF register address. Bit 12-19
	Data |= ((Offset&0xFF)<<12);
	// 3. Write RF path.  bit 20-21
	Data |= ((eRFPath&0x3)<<20);
	// 4. Set RF read indicator. bit 22=0
	//Data |= 0x00000;
	// 5. Trigger Fw to operate the command. bit 31
	Data |= 0x80000000;
	// 6. We can not execute read operation if bit 31 is 1.
	while (read_nic_dword(dev, QPNR)&0x80000000)
	{
		// If FW can not finish RF-R/W for more than ?? times. We must reset FW.
		if (time++ < 100)
		{
			//DbgPrint("FW not finish RF-R Time=%d\n\r", time);
			udelay(10);
		}
		else
			break;
	}
	// 7. Execute read operation.
	write_nic_dword(dev, QPNR, Data);
	// 8. Check if firmawre send back RF content.
	while (read_nic_dword(dev, QPNR)&0x80000000)
	{
		// If FW can not finish RF-R/W for more than ?? times. We must reset FW.
		if (time++ < 100)
		{
			//DbgPrint("FW not finish RF-W Time=%d\n\r", time);
			udelay(10);
		}
		else
			return	(0);
	}
	retValue = read_nic_dword(dev, RF_DATA);

	return	(retValue);

}	/* phy_FwRFSerialRead */

/******************************************************************************
 *function:  We support firmware to execute RF-R/W.
 *   input:  dev
 *  output:  none
 *  return:  none
 *  notice:
 * ***************************************************************************/
static void
phy_FwRFSerialWrite(
		struct net_device* dev,
		RF90_RADIO_PATH_E	eRFPath,
		u32				Offset,
		u32				Data	)
{
	u8	time = 0;

	//DbgPrint("N FW RF CTRL RF-%d OF%02x DATA=%03x\n\r", eRFPath, Offset, Data);
	/* 2007/11/02 MH Firmware RF Write control. By Francis' suggestion, we can
	   not execute the scheme in the initial step. Otherwise, RF-R/W will waste
	   much time. This is only for site survey. */

	// 1. Set driver write bit and 12 bit data. bit 0-11
	//Data &= bMask12Bits;	// Done by uper layer.
	// 2. Write RF register address. bit 12-19
	Data |= ((Offset&0xFF)<<12);
	// 3. Write RF path.  bit 20-21
	Data |= ((eRFPath&0x3)<<20);
	// 4. Set RF write indicator. bit 22=1
	Data |= 0x400000;
	// 5. Trigger Fw to operate the command. bit 31=1
	Data |= 0x80000000;

	// 6. Write operation. We can not write if bit 31 is 1.
	while (read_nic_dword(dev, QPNR)&0x80000000)
	{
		// If FW can not finish RF-R/W for more than ?? times. We must reset FW.
		if (time++ < 100)
		{
			//DbgPrint("FW not finish RF-W Time=%d\n\r", time);
			udelay(10);
		}
		else
			break;
	}
	// 7. No matter check bit. We always force the write. Because FW will
	//    not accept the command.
	write_nic_dword(dev, QPNR, Data);
	/* 2007/11/02 MH Acoording to test, we must delay 20us to wait firmware
	   to finish RF write operation. */
	/* 2008/01/17 MH We support delay in firmware side now. */
	//delay_us(20);

}	/* phy_FwRFSerialWrite */


/******************************************************************************
 *function:  This function read BB parameters from Header file we gen,
 *	     and do register read/write
 *   input:  dev
 *  output:  none
 *  return:  none
 *  notice:  BB parameters may change all the time, so please make
 *           sure it has been synced with the newest.
 * ***************************************************************************/
void rtl8192_phy_configmac(struct net_device* dev)
{
	u32 dwArrayLen = 0, i;
	u32* pdwArray = NULL;
	struct r8192_priv *priv = ieee80211_priv(dev);

	if(priv->btxpowerdata_readfromEEPORM)
	{
		RT_TRACE(COMP_PHY, "Rtl819XMACPHY_Array_PG\n");
		dwArrayLen = MACPHY_Array_PGLength;
		pdwArray = rtl819XMACPHY_Array_PG;

	}
	else
	{
		RT_TRACE(COMP_PHY, "Rtl819XMACPHY_Array\n");
		dwArrayLen = MACPHY_ArrayLength;
		pdwArray = rtl819XMACPHY_Array;
	}
	for(i = 0; i<dwArrayLen; i=i+3){
		if(pdwArray[i] == 0x318)
		{
			pdwArray[i+2] = 0x00000800;
			//DbgPrint("ptrArray[i], ptrArray[i+1], ptrArray[i+2] = %x, %x, %x\n",
			//	ptrArray[i], ptrArray[i+1], ptrArray[i+2]);
		}

		RT_TRACE(COMP_DBG, "The Rtl8190MACPHY_Array[0] is %x Rtl8190MACPHY_Array[1] is %x Rtl8190MACPHY_Array[2] is %x\n",
				pdwArray[i], pdwArray[i+1], pdwArray[i+2]);
		rtl8192_setBBreg(dev, pdwArray[i], pdwArray[i+1], pdwArray[i+2]);
	}
	return;

}

/******************************************************************************
 *function:  This function do dirty work
 *   input:  dev
 *  output:  none
 *  return:  none
 *  notice:  BB parameters may change all the time, so please make
 *           sure it has been synced with the newest.
 * ***************************************************************************/

void rtl8192_phyConfigBB(struct net_device* dev, u8 ConfigType)
{
	u32 i;

#ifdef TO_DO_LIST
	u32 *rtl8192PhyRegArrayTable = NULL, *rtl8192AgcTabArrayTable = NULL;
	if(Adapter->bInHctTest)
	{
		PHY_REGArrayLen = PHY_REGArrayLengthDTM;
		AGCTAB_ArrayLen = AGCTAB_ArrayLengthDTM;
		Rtl8190PHY_REGArray_Table = Rtl819XPHY_REGArrayDTM;
		Rtl8190AGCTAB_Array_Table = Rtl819XAGCTAB_ArrayDTM;
	}
#endif
	if (ConfigType == BaseBand_Config_PHY_REG)
	{
		for (i=0; i<PHY_REG_1T2RArrayLength; i+=2)
		{
			rtl8192_setBBreg(dev, rtl819XPHY_REG_1T2RArray[i], bMaskDWord, rtl819XPHY_REG_1T2RArray[i+1]);
			RT_TRACE(COMP_DBG, "i: %x, The Rtl819xUsbPHY_REGArray[0] is %x Rtl819xUsbPHY_REGArray[1] is %x \n",i, rtl819XPHY_REG_1T2RArray[i], rtl819XPHY_REG_1T2RArray[i+1]);
		}
	}
	else if (ConfigType == BaseBand_Config_AGC_TAB)
	{
		for (i=0; i<AGCTAB_ArrayLength; i+=2)
		{
			rtl8192_setBBreg(dev, rtl819XAGCTAB_Array[i], bMaskDWord, rtl819XAGCTAB_Array[i+1]);
			RT_TRACE(COMP_DBG, "i:%x, The rtl819XAGCTAB_Array[0] is %x rtl819XAGCTAB_Array[1] is %x \n",i, rtl819XAGCTAB_Array[i], rtl819XAGCTAB_Array[i+1]);
		}
	}
	return;


}
/******************************************************************************
 *function:  This function initialize Register definition offset for Radio Path
 *	     A/B/C/D
 *   input:  net_device dev
 *  output:  none
 *  return:  none
 *  notice:  Initialization value here is constant and it should never be changed
 * ***************************************************************************/
void rtl8192_InitBBRFRegDef(struct net_device* dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
// RF Interface Sowrtware Control
	priv->PHYRegDef[RF90_PATH_A].rfintfs = rFPGA0_XAB_RFInterfaceSW; // 16 LSBs if read 32-bit from 0x870
	priv->PHYRegDef[RF90_PATH_B].rfintfs = rFPGA0_XAB_RFInterfaceSW; // 16 MSBs if read 32-bit from 0x870 (16-bit for 0x872)
	priv->PHYRegDef[RF90_PATH_C].rfintfs = rFPGA0_XCD_RFInterfaceSW;// 16 LSBs if read 32-bit from 0x874
	priv->PHYRegDef[RF90_PATH_D].rfintfs = rFPGA0_XCD_RFInterfaceSW;// 16 MSBs if read 32-bit from 0x874 (16-bit for 0x876)

	// RF Interface Readback Value
	priv->PHYRegDef[RF90_PATH_A].rfintfi = rFPGA0_XAB_RFInterfaceRB; // 16 LSBs if read 32-bit from 0x8E0
	priv->PHYRegDef[RF90_PATH_B].rfintfi = rFPGA0_XAB_RFInterfaceRB;// 16 MSBs if read 32-bit from 0x8E0 (16-bit for 0x8E2)
	priv->PHYRegDef[RF90_PATH_C].rfintfi = rFPGA0_XCD_RFInterfaceRB;// 16 LSBs if read 32-bit from 0x8E4
	priv->PHYRegDef[RF90_PATH_D].rfintfi = rFPGA0_XCD_RFInterfaceRB;// 16 MSBs if read 32-bit from 0x8E4 (16-bit for 0x8E6)

	// RF Interface Output (and Enable)
	priv->PHYRegDef[RF90_PATH_A].rfintfo = rFPGA0_XA_RFInterfaceOE; // 16 LSBs if read 32-bit from 0x860
	priv->PHYRegDef[RF90_PATH_B].rfintfo = rFPGA0_XB_RFInterfaceOE; // 16 LSBs if read 32-bit from 0x864
	priv->PHYRegDef[RF90_PATH_C].rfintfo = rFPGA0_XC_RFInterfaceOE;// 16 LSBs if read 32-bit from 0x868
	priv->PHYRegDef[RF90_PATH_D].rfintfo = rFPGA0_XD_RFInterfaceOE;// 16 LSBs if read 32-bit from 0x86C

	// RF Interface (Output and)  Enable
	priv->PHYRegDef[RF90_PATH_A].rfintfe = rFPGA0_XA_RFInterfaceOE; // 16 MSBs if read 32-bit from 0x860 (16-bit for 0x862)
	priv->PHYRegDef[RF90_PATH_B].rfintfe = rFPGA0_XB_RFInterfaceOE; // 16 MSBs if read 32-bit from 0x864 (16-bit for 0x866)
	priv->PHYRegDef[RF90_PATH_C].rfintfe = rFPGA0_XC_RFInterfaceOE;// 16 MSBs if read 32-bit from 0x86A (16-bit for 0x86A)
	priv->PHYRegDef[RF90_PATH_D].rfintfe = rFPGA0_XD_RFInterfaceOE;// 16 MSBs if read 32-bit from 0x86C (16-bit for 0x86E)

	//Addr of LSSI. Wirte RF register by driver
	priv->PHYRegDef[RF90_PATH_A].rf3wireOffset = rFPGA0_XA_LSSIParameter; //LSSI Parameter
	priv->PHYRegDef[RF90_PATH_B].rf3wireOffset = rFPGA0_XB_LSSIParameter;
	priv->PHYRegDef[RF90_PATH_C].rf3wireOffset = rFPGA0_XC_LSSIParameter;
	priv->PHYRegDef[RF90_PATH_D].rf3wireOffset = rFPGA0_XD_LSSIParameter;

	// RF parameter
	priv->PHYRegDef[RF90_PATH_A].rfLSSI_Select = rFPGA0_XAB_RFParameter;  //BB Band Select
	priv->PHYRegDef[RF90_PATH_B].rfLSSI_Select = rFPGA0_XAB_RFParameter;
	priv->PHYRegDef[RF90_PATH_C].rfLSSI_Select = rFPGA0_XCD_RFParameter;
	priv->PHYRegDef[RF90_PATH_D].rfLSSI_Select = rFPGA0_XCD_RFParameter;

	// Tx AGC Gain Stage (same for all path. Should we remove this?)
	priv->PHYRegDef[RF90_PATH_A].rfTxGainStage = rFPGA0_TxGainStage; //Tx gain stage
	priv->PHYRegDef[RF90_PATH_B].rfTxGainStage = rFPGA0_TxGainStage; //Tx gain stage
	priv->PHYRegDef[RF90_PATH_C].rfTxGainStage = rFPGA0_TxGainStage; //Tx gain stage
	priv->PHYRegDef[RF90_PATH_D].rfTxGainStage = rFPGA0_TxGainStage; //Tx gain stage

	// Tranceiver A~D HSSI Parameter-1
	priv->PHYRegDef[RF90_PATH_A].rfHSSIPara1 = rFPGA0_XA_HSSIParameter1;  //wire control parameter1
	priv->PHYRegDef[RF90_PATH_B].rfHSSIPara1 = rFPGA0_XB_HSSIParameter1;  //wire control parameter1
	priv->PHYRegDef[RF90_PATH_C].rfHSSIPara1 = rFPGA0_XC_HSSIParameter1;  //wire control parameter1
	priv->PHYRegDef[RF90_PATH_D].rfHSSIPara1 = rFPGA0_XD_HSSIParameter1;  //wire control parameter1

	// Tranceiver A~D HSSI Parameter-2
	priv->PHYRegDef[RF90_PATH_A].rfHSSIPara2 = rFPGA0_XA_HSSIParameter2;  //wire control parameter2
	priv->PHYRegDef[RF90_PATH_B].rfHSSIPara2 = rFPGA0_XB_HSSIParameter2;  //wire control parameter2
	priv->PHYRegDef[RF90_PATH_C].rfHSSIPara2 = rFPGA0_XC_HSSIParameter2;  //wire control parameter2
	priv->PHYRegDef[RF90_PATH_D].rfHSSIPara2 = rFPGA0_XD_HSSIParameter2;  //wire control parameter1

	// RF switch Control
	priv->PHYRegDef[RF90_PATH_A].rfSwitchControl = rFPGA0_XAB_SwitchControl; //TR/Ant switch control
	priv->PHYRegDef[RF90_PATH_B].rfSwitchControl = rFPGA0_XAB_SwitchControl;
	priv->PHYRegDef[RF90_PATH_C].rfSwitchControl = rFPGA0_XCD_SwitchControl;
	priv->PHYRegDef[RF90_PATH_D].rfSwitchControl = rFPGA0_XCD_SwitchControl;

	// AGC control 1
	priv->PHYRegDef[RF90_PATH_A].rfAGCControl1 = rOFDM0_XAAGCCore1;
	priv->PHYRegDef[RF90_PATH_B].rfAGCControl1 = rOFDM0_XBAGCCore1;
	priv->PHYRegDef[RF90_PATH_C].rfAGCControl1 = rOFDM0_XCAGCCore1;
	priv->PHYRegDef[RF90_PATH_D].rfAGCControl1 = rOFDM0_XDAGCCore1;

	// AGC control 2
	priv->PHYRegDef[RF90_PATH_A].rfAGCControl2 = rOFDM0_XAAGCCore2;
	priv->PHYRegDef[RF90_PATH_B].rfAGCControl2 = rOFDM0_XBAGCCore2;
	priv->PHYRegDef[RF90_PATH_C].rfAGCControl2 = rOFDM0_XCAGCCore2;
	priv->PHYRegDef[RF90_PATH_D].rfAGCControl2 = rOFDM0_XDAGCCore2;

	// RX AFE control 1
	priv->PHYRegDef[RF90_PATH_A].rfRxIQImbalance = rOFDM0_XARxIQImbalance;
	priv->PHYRegDef[RF90_PATH_B].rfRxIQImbalance = rOFDM0_XBRxIQImbalance;
	priv->PHYRegDef[RF90_PATH_C].rfRxIQImbalance = rOFDM0_XCRxIQImbalance;
	priv->PHYRegDef[RF90_PATH_D].rfRxIQImbalance = rOFDM0_XDRxIQImbalance;

	// RX AFE control 1
	priv->PHYRegDef[RF90_PATH_A].rfRxAFE = rOFDM0_XARxAFE;
	priv->PHYRegDef[RF90_PATH_B].rfRxAFE = rOFDM0_XBRxAFE;
	priv->PHYRegDef[RF90_PATH_C].rfRxAFE = rOFDM0_XCRxAFE;
	priv->PHYRegDef[RF90_PATH_D].rfRxAFE = rOFDM0_XDRxAFE;

	// Tx AFE control 1
	priv->PHYRegDef[RF90_PATH_A].rfTxIQImbalance = rOFDM0_XATxIQImbalance;
	priv->PHYRegDef[RF90_PATH_B].rfTxIQImbalance = rOFDM0_XBTxIQImbalance;
	priv->PHYRegDef[RF90_PATH_C].rfTxIQImbalance = rOFDM0_XCTxIQImbalance;
	priv->PHYRegDef[RF90_PATH_D].rfTxIQImbalance = rOFDM0_XDTxIQImbalance;

	// Tx AFE control 2
	priv->PHYRegDef[RF90_PATH_A].rfTxAFE = rOFDM0_XATxAFE;
	priv->PHYRegDef[RF90_PATH_B].rfTxAFE = rOFDM0_XBTxAFE;
	priv->PHYRegDef[RF90_PATH_C].rfTxAFE = rOFDM0_XCTxAFE;
	priv->PHYRegDef[RF90_PATH_D].rfTxAFE = rOFDM0_XDTxAFE;

	// Tranceiver LSSI Readback
	priv->PHYRegDef[RF90_PATH_A].rfLSSIReadBack = rFPGA0_XA_LSSIReadBack;
	priv->PHYRegDef[RF90_PATH_B].rfLSSIReadBack = rFPGA0_XB_LSSIReadBack;
	priv->PHYRegDef[RF90_PATH_C].rfLSSIReadBack = rFPGA0_XC_LSSIReadBack;
	priv->PHYRegDef[RF90_PATH_D].rfLSSIReadBack = rFPGA0_XD_LSSIReadBack;

}
/******************************************************************************
 *function:  This function is to write register and then readback to make sure whether BB and RF is OK
 *   input:  net_device dev
 *   	     HW90_BLOCK_E CheckBlock
 *   	     RF90_RADIO_PATH_E eRFPath  //only used when checkblock is HW90_BLOCK_RF
 *  output:  none
 *  return:  return whether BB and RF is ok(0:OK; 1:Fail)
 *  notice:  This function may be removed in the ASIC
 * ***************************************************************************/
u8 rtl8192_phy_checkBBAndRF(struct net_device* dev, HW90_BLOCK_E CheckBlock, RF90_RADIO_PATH_E eRFPath)
{
//	struct r8192_priv *priv = ieee80211_priv(dev);
//	BB_REGISTER_DEFINITION_T *pPhyReg = &priv->PHYRegDef[eRFPath];
	u8 ret = 0;
	u32 i, CheckTimes = 4, dwRegRead = 0;
	u32 WriteAddr[4];
	u32 WriteData[] = {0xfffff027, 0xaa55a02f, 0x00000027, 0x55aa502f};
	// Initialize register address offset to be checked
	WriteAddr[HW90_BLOCK_MAC] = 0x100;
	WriteAddr[HW90_BLOCK_PHY0] = 0x900;
	WriteAddr[HW90_BLOCK_PHY1] = 0x800;
	WriteAddr[HW90_BLOCK_RF] = 0x3;
	RT_TRACE(COMP_PHY, "=======>%s(), CheckBlock:%d\n", __FUNCTION__, CheckBlock);
	for(i=0 ; i < CheckTimes ; i++)
	{

		//
		// Write Data to register and readback
		//
		switch(CheckBlock)
		{
		case HW90_BLOCK_MAC:
			RT_TRACE(COMP_ERR, "PHY_CheckBBRFOK(): Never Write 0x100 here!");
			break;

		case HW90_BLOCK_PHY0:
		case HW90_BLOCK_PHY1:
			write_nic_dword(dev, WriteAddr[CheckBlock], WriteData[i]);
			dwRegRead = read_nic_dword(dev, WriteAddr[CheckBlock]);
			break;

		case HW90_BLOCK_RF:
			WriteData[i] &= 0xfff;
			rtl8192_phy_SetRFReg(dev, eRFPath, WriteAddr[HW90_BLOCK_RF], bMask12Bits, WriteData[i]);
			// TODO: we should not delay for such a long time. Ask SD3
			msleep(1);
			dwRegRead = rtl8192_phy_QueryRFReg(dev, eRFPath, WriteAddr[HW90_BLOCK_RF], bMask12Bits);
			msleep(1);
			break;

		default:
			ret = 1;
			break;
		}


		//
		// Check whether readback data is correct
		//
		if(dwRegRead != WriteData[i])
		{
			RT_TRACE((COMP_PHY|COMP_ERR), "====>error=====dwRegRead: %x, WriteData: %x \n", dwRegRead, WriteData[i]);
			ret = 1;
			break;
		}
	}

	return ret;
}


/******************************************************************************
 *function:  This function initialize BB&RF
 *   input:  net_device dev
 *  output:  none
 *  return:  none
 *  notice:  Initialization value may change all the time, so please make
 *           sure it has been synced with the newest.
 * ***************************************************************************/
void rtl8192_BB_Config_ParaFile(struct net_device* dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8 bRegValue = 0, eCheckItem = 0, rtStatus = 0;
	u32 dwRegValue = 0;
	/**************************************
	//<1>Initialize BaseBand
	**************************************/

	/*--set BB Global Reset--*/
	bRegValue = read_nic_byte(dev, BB_GLOBAL_RESET);
	write_nic_byte(dev, BB_GLOBAL_RESET,(bRegValue|BB_GLOBAL_RESET_BIT));
	mdelay(50);
	/*---set BB reset Active---*/
	dwRegValue = read_nic_dword(dev, CPU_GEN);
	write_nic_dword(dev, CPU_GEN, (dwRegValue&(~CPU_GEN_BB_RST)));

	/*----Ckeck FPGAPHY0 and PHY1 board is OK----*/
	// TODO: this function should be removed on ASIC , Emily 2007.2.2
	for(eCheckItem=(HW90_BLOCK_E)HW90_BLOCK_PHY0; eCheckItem<=HW90_BLOCK_PHY1; eCheckItem++)
	{
		rtStatus  = rtl8192_phy_checkBBAndRF(dev, (HW90_BLOCK_E)eCheckItem, (RF90_RADIO_PATH_E)0); //don't care RF path
		if(rtStatus != 0)
		{
			RT_TRACE((COMP_ERR | COMP_PHY), "PHY_RF8256_Config():Check PHY%d Fail!!\n", eCheckItem-1);
			return ;
		}
	}
	/*---- Set CCK and OFDM Block "OFF"----*/
	rtl8192_setBBreg(dev, rFPGA0_RFMOD, bCCKEn|bOFDMEn, 0x0);
	/*----BB Register Initilazation----*/
	//==m==>Set PHY REG From Header<==m==
	rtl8192_phyConfigBB(dev, BaseBand_Config_PHY_REG);

	/*----Set BB reset de-Active----*/
	dwRegValue = read_nic_dword(dev, CPU_GEN);
	write_nic_dword(dev, CPU_GEN, (dwRegValue|CPU_GEN_BB_RST));

	/*----BB AGC table Initialization----*/
	//==m==>Set PHY REG From Header<==m==
	rtl8192_phyConfigBB(dev, BaseBand_Config_AGC_TAB);

	/*----Enable XSTAL ----*/
	write_nic_byte_E(dev, 0x5e, 0x00);
	if (priv->card_8192_version == (u8)VERSION_819xU_A)
	{
		//Antenna gain offset from B/C/D to A
		dwRegValue = (priv->AntennaTxPwDiff[1]<<4 | priv->AntennaTxPwDiff[0]);
		rtl8192_setBBreg(dev, rFPGA0_TxGainStage, (bXBTxAGC|bXCTxAGC), dwRegValue);

		//XSTALLCap
		dwRegValue = priv->CrystalCap & 0xf;
		rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, bXtalCap, dwRegValue);
	}

	// Check if the CCK HighPower is turned ON.
	// This is used to calculate PWDB.
	priv->bCckHighPower = (u8)(rtl8192_QueryBBReg(dev, rFPGA0_XA_HSSIParameter2, 0x200));
	return;
}
/******************************************************************************
 *function:  This function initialize BB&RF
 *   input:  net_device dev
 *  output:  none
 *  return:  none
 *  notice:  Initialization value may change all the time, so please make
 *           sure it has been synced with the newest.
 * ***************************************************************************/
void rtl8192_BBConfig(struct net_device* dev)
{
	rtl8192_InitBBRFRegDef(dev);
	//config BB&RF. As hardCode based initialization has not been well
	rtl8192_BB_Config_ParaFile(dev);
	return;
}

/******************************************************************************
 *function:  This function obtains the initialization value of Tx power Level offset
 *   input:  net_device dev
 *  output:  none
 *  return:  none
 * ***************************************************************************/
void rtl8192_phy_getTxPower(struct net_device* dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	priv->MCSTxPowerLevelOriginalOffset[0] =
		read_nic_dword(dev, rTxAGC_Rate18_06);
	priv->MCSTxPowerLevelOriginalOffset[1] =
		read_nic_dword(dev, rTxAGC_Rate54_24);
	priv->MCSTxPowerLevelOriginalOffset[2] =
		read_nic_dword(dev, rTxAGC_Mcs03_Mcs00);
	priv->MCSTxPowerLevelOriginalOffset[3] =
		read_nic_dword(dev, rTxAGC_Mcs07_Mcs04);
	priv->MCSTxPowerLevelOriginalOffset[4] =
		read_nic_dword(dev, rTxAGC_Mcs11_Mcs08);
	priv->MCSTxPowerLevelOriginalOffset[5] =
		read_nic_dword(dev, rTxAGC_Mcs15_Mcs12);

	// read rx initial gain
	priv->DefaultInitialGain[0] = read_nic_byte(dev, rOFDM0_XAAGCCore1);
	priv->DefaultInitialGain[1] = read_nic_byte(dev, rOFDM0_XBAGCCore1);
	priv->DefaultInitialGain[2] = read_nic_byte(dev, rOFDM0_XCAGCCore1);
	priv->DefaultInitialGain[3] = read_nic_byte(dev, rOFDM0_XDAGCCore1);
	RT_TRACE(COMP_INIT, "Default initial gain (c50=0x%x, c58=0x%x, c60=0x%x, c68=0x%x) \n",
		priv->DefaultInitialGain[0], priv->DefaultInitialGain[1],
		priv->DefaultInitialGain[2], priv->DefaultInitialGain[3]);

	// read framesync
	priv->framesync = read_nic_byte(dev, rOFDM0_RxDetector3);
	priv->framesyncC34 = read_nic_byte(dev, rOFDM0_RxDetector2);
	RT_TRACE(COMP_INIT, "Default framesync (0x%x) = 0x%x \n",
		rOFDM0_RxDetector3, priv->framesync);

	// read SIFS (save the value read fome MACPHY_REG.txt)
	priv->SifsTime = read_nic_word(dev, SIFS);

	return;
}

/******************************************************************************
 *function:  This function obtains the initialization value of Tx power Level offset
 *   input:  net_device dev
 *  output:  none
 *  return:  none
 * ***************************************************************************/
void rtl8192_phy_setTxPower(struct net_device* dev, u8 channel)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8	powerlevel = priv->TxPowerLevelCCK[channel-1];
	u8	powerlevelOFDM24G = priv->TxPowerLevelOFDM24G[channel-1];

	switch(priv->rf_chip)
	{
	case RF_8256:
		PHY_SetRF8256CCKTxPower(dev, powerlevel); //need further implement
		PHY_SetRF8256OFDMTxPower(dev, powerlevelOFDM24G);
		break;
	default:
//	case RF_8225:
//	case RF_8258:
		RT_TRACE((COMP_PHY|COMP_ERR), "error RF chipID(8225 or 8258) in function %s()\n", __FUNCTION__);
		break;
	}
	return;
}

/******************************************************************************
 *function:  This function check Rf chip to do RF config
 *   input:  net_device dev
 *  output:  none
 *  return:  only 8256 is supported
 * ***************************************************************************/
void rtl8192_phy_RFConfig(struct net_device* dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	switch(priv->rf_chip)
	{
		case RF_8256:
			PHY_RF8256_Config(dev);
			break;
	//	case RF_8225:
	//	case RF_8258:
		default:
			RT_TRACE(COMP_ERR, "error chip id\n");
			break;
	}
	return;
}

/******************************************************************************
 *function:  This function update Initial gain
 *   input:  net_device dev
 *  output:  none
 *  return:  As Windows has not implemented this, wait for complement
 * ***************************************************************************/
void rtl8192_phy_updateInitGain(struct net_device* dev)
{
	return;
}

/******************************************************************************
 *function:  This function read RF parameters from general head file, and do RF 3-wire
 *   input:  net_device dev
 *  output:  none
 *  return:  return code show if RF configuration is successful(0:pass, 1:fail)
 *    Note:  Delay may be required for RF configuration
 * ***************************************************************************/
u8 rtl8192_phy_ConfigRFWithHeaderFile(struct net_device* dev, RF90_RADIO_PATH_E	eRFPath)
{

	int i;
	//u32* pRFArray;
	u8 ret = 0;

	switch(eRFPath){
		case RF90_PATH_A:
			for(i = 0;i<RadioA_ArrayLength; i=i+2){

				if(rtl819XRadioA_Array[i] == 0xfe){
						mdelay(100);
						continue;
				}
				rtl8192_phy_SetRFReg(dev, eRFPath, rtl819XRadioA_Array[i], bMask12Bits, rtl819XRadioA_Array[i+1]);
				mdelay(1);

			}
			break;
		case RF90_PATH_B:
			for(i = 0;i<RadioB_ArrayLength; i=i+2){

				if(rtl819XRadioB_Array[i] == 0xfe){
						mdelay(100);
						continue;
				}
				rtl8192_phy_SetRFReg(dev, eRFPath, rtl819XRadioB_Array[i], bMask12Bits, rtl819XRadioB_Array[i+1]);
				mdelay(1);

			}
			break;
		case RF90_PATH_C:
			for(i = 0;i<RadioC_ArrayLength; i=i+2){

				if(rtl819XRadioC_Array[i] == 0xfe){
						mdelay(100);
						continue;
				}
				rtl8192_phy_SetRFReg(dev, eRFPath, rtl819XRadioC_Array[i], bMask12Bits, rtl819XRadioC_Array[i+1]);
				mdelay(1);

			}
			break;
		case RF90_PATH_D:
			for(i = 0;i<RadioD_ArrayLength; i=i+2){

				if(rtl819XRadioD_Array[i] == 0xfe){
						mdelay(100);
						continue;
				}
				rtl8192_phy_SetRFReg(dev, eRFPath, rtl819XRadioD_Array[i], bMask12Bits, rtl819XRadioD_Array[i+1]);
				mdelay(1);

			}
			break;
		default:
			break;
	}

	return ret;;

}
/******************************************************************************
 *function:  This function set Tx Power of the channel
 *   input:  struct net_device *dev
 *   	     u8 		channel
 *  output:  none
 *  return:  none
 *    Note:
 * ***************************************************************************/
void rtl8192_SetTxPowerLevel(struct net_device *dev, u8 channel)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8	powerlevel = priv->TxPowerLevelCCK[channel-1];
	u8	powerlevelOFDM24G = priv->TxPowerLevelOFDM24G[channel-1];

	switch(priv->rf_chip)
	{
	case RF_8225:
#ifdef TO_DO_LIST
		PHY_SetRF8225CckTxPower(Adapter, powerlevel);
		PHY_SetRF8225OfdmTxPower(Adapter, powerlevelOFDM24G);
#endif
		break;

	case RF_8256:
		PHY_SetRF8256CCKTxPower(dev, powerlevel);
		PHY_SetRF8256OFDMTxPower(dev, powerlevelOFDM24G);
		break;

	case RF_8258:
		break;
	default:
		RT_TRACE(COMP_ERR, "unknown rf chip ID in rtl8192_SetTxPowerLevel()\n");
		break;
	}
	return;
}

/******************************************************************************
 *function:  This function set RF state on or off
 *   input:  struct net_device *dev
 *   	     RT_RF_POWER_STATE eRFPowerState  //Power State to set
 *  output:  none
 *  return:  none
 *    Note:
 * ***************************************************************************/
bool rtl8192_SetRFPowerState(struct net_device *dev, RT_RF_POWER_STATE eRFPowerState)
{
	bool				bResult = true;
//	u8					eRFPath;
	struct r8192_priv *priv = ieee80211_priv(dev);

	if(eRFPowerState == priv->ieee80211->eRFPowerState)
		return false;

	if(priv->SetRFPowerStateInProgress == true)
		return false;

	priv->SetRFPowerStateInProgress = true;

	switch(priv->rf_chip)
	{
		case RF_8256:
		switch( eRFPowerState )
		{
			case eRfOn:
	//RF-A, RF-B
					//enable RF-Chip A/B
					rtl8192_setBBreg(dev, rFPGA0_XA_RFInterfaceOE, BIT4, 0x1);	// 0x860[4]
					//analog to digital on
					rtl8192_setBBreg(dev, rFPGA0_AnalogParameter4, 0x300, 0x3);// 0x88c[9:8]
					//digital to analog on
					rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, 0x18, 0x3); // 0x880[4:3]
					//rx antenna on
					rtl8192_setBBreg(dev, rOFDM0_TRxPathEnable, 0x3, 0x3);// 0xc04[1:0]
					//rx antenna on
					rtl8192_setBBreg(dev, rOFDM1_TRxPathEnable, 0x3, 0x3);// 0xd04[1:0]
					//analog to digital part2 on
					rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, 0x60, 0x3); // 0x880[6:5]

				break;

			case eRfSleep:

				break;

			case eRfOff:
					//RF-A, RF-B
					//disable RF-Chip A/B
					rtl8192_setBBreg(dev, rFPGA0_XA_RFInterfaceOE, BIT4, 0x0);	// 0x860[4]
					//analog to digital off, for power save
					rtl8192_setBBreg(dev, rFPGA0_AnalogParameter4, 0xf00, 0x0);// 0x88c[11:8]
					//digital to analog off, for power save
					rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, 0x18, 0x0); // 0x880[4:3]
					//rx antenna off
					rtl8192_setBBreg(dev, rOFDM0_TRxPathEnable, 0xf, 0x0);// 0xc04[3:0]
					//rx antenna off
					rtl8192_setBBreg(dev, rOFDM1_TRxPathEnable, 0xf, 0x0);// 0xd04[3:0]
					//analog to digital part2 off, for power save
					rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, 0x60, 0x0); // 0x880[6:5]

				break;

			default:
				bResult = false;
				RT_TRACE(COMP_ERR, "SetRFPowerState819xUsb(): unknow state to set: 0x%X!!!\n", eRFPowerState);
				break;
		}
			break;
		default:
			RT_TRACE(COMP_ERR, "Not support rf_chip(%x)\n", priv->rf_chip);
			break;
	}
#ifdef TO_DO_LIST
	if(bResult)
	{
		// Update current RF state variable.
		pHalData->eRFPowerState = eRFPowerState;
		switch(pHalData->RFChipID )
		{
			case RF_8256:
		switch(pHalData->eRFPowerState)
				{
				case eRfOff:
					//
					//If Rf off reason is from IPS, Led should blink with no link, by Maddest 071015
					//
					if(pMgntInfo->RfOffReason==RF_CHANGE_BY_IPS )
					{
						Adapter->HalFunc.LedControlHandler(Adapter,LED_CTL_NO_LINK);
					}
					else
					{
						// Turn off LED if RF is not ON.
						Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_POWER_OFF);
					}
					break;

				case eRfOn:
					// Turn on RF we are still linked, which might happen when
					// we quickly turn off and on HW RF. 2006.05.12, by rcnjko.
					if( pMgntInfo->bMediaConnect == TRUE )
					{
						Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_LINK);
					}
					else
					{
						// Turn off LED if RF is not ON.
						Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_NO_LINK);
					}
					break;

				default:
					// do nothing.
					break;
				}// Switch RF state
				break;

				default:
					RT_TRACE(COMP_RF, DBG_LOUD, ("SetRFPowerState8190(): Unknown RF type\n"));
					break;
			}

	}
#endif
	priv->SetRFPowerStateInProgress = false;

	return bResult;
}

/****************************************************************************************
 *function:  This function set command table variable(struct SwChnlCmd).
 *   input:  SwChnlCmd*		CmdTable 	//table to be set.
 *   	     u32		CmdTableIdx 	//variable index in table to be set
 *   	     u32		CmdTableSz	//table size.
 *   	     SwChnlCmdID	CmdID		//command ID to set.
 *	     u32		Para1
 *	     u32		Para2
 *	     u32		msDelay
 *  output:
 *  return:  true if finished, false otherwise
 *    Note:
 * ************************************************************************************/
u8 rtl8192_phy_SetSwChnlCmdArray(
	SwChnlCmd*		CmdTable,
	u32			CmdTableIdx,
	u32			CmdTableSz,
	SwChnlCmdID		CmdID,
	u32			Para1,
	u32			Para2,
	u32			msDelay
	)
{
	SwChnlCmd* pCmd;

	if(CmdTable == NULL)
	{
		RT_TRACE(COMP_ERR, "phy_SetSwChnlCmdArray(): CmdTable cannot be NULL.\n");
		return false;
	}
	if(CmdTableIdx >= CmdTableSz)
	{
		RT_TRACE(COMP_ERR, "phy_SetSwChnlCmdArray(): Access invalid index, please check size of the table, CmdTableIdx:%d, CmdTableSz:%d\n",
				CmdTableIdx, CmdTableSz);
		return false;
	}

	pCmd = CmdTable + CmdTableIdx;
	pCmd->CmdID = CmdID;
	pCmd->Para1 = Para1;
	pCmd->Para2 = Para2;
	pCmd->msDelay = msDelay;

	return true;
}
/******************************************************************************
 *function:  This function set channel step by step
 *   input:  struct net_device *dev
 *   	     u8 		channel
 *   	     u8* 		stage //3 stages
 *   	     u8* 		step  //
 *   	     u32* 		delay //whether need to delay
 *  output:  store new stage, step and delay for next step(combine with function above)
 *  return:  true if finished, false otherwise
 *    Note:  Wait for simpler function to replace it //wb
 * ***************************************************************************/
u8 rtl8192_phy_SwChnlStepByStep(struct net_device *dev, u8 channel, u8* stage, u8* step, u32* delay)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
//	PCHANNEL_ACCESS_SETTING	pChnlAccessSetting;
	SwChnlCmd				PreCommonCmd[MAX_PRECMD_CNT];
	u32					PreCommonCmdCnt;
	SwChnlCmd				PostCommonCmd[MAX_POSTCMD_CNT];
	u32					PostCommonCmdCnt;
	SwChnlCmd				RfDependCmd[MAX_RFDEPENDCMD_CNT];
	u32					RfDependCmdCnt;
	SwChnlCmd				*CurrentCmd = NULL;
	//RF90_RADIO_PATH_E		eRFPath;
	u8		eRFPath;
//	u32		RfRetVal;
//	u8		RetryCnt;

	RT_TRACE(COMP_CH, "====>%s()====stage:%d, step:%d, channel:%d\n", __FUNCTION__, *stage, *step, channel);
//	RT_ASSERT(IsLegalChannel(Adapter, channel), ("illegal channel: %d\n", channel));
#ifdef ENABLE_DOT11D
	if (!IsLegalChannel(priv->ieee80211, channel))
	{
		RT_TRACE(COMP_ERR, "=============>set to illegal channel:%d\n", channel);
		return true; //return true to tell upper caller function this channel setting is finished! Or it will in while loop.
	}
#endif


	//for(eRFPath = RF90_PATH_A; eRFPath <pHalData->NumTotalRFPath; eRFPath++)
//	for(eRFPath = 0; eRFPath <RF90_PATH_MAX; eRFPath++)
//	{
//		if (!rtl8192_phy_CheckIsLegalRFPath(dev, eRFPath))
//			continue;
		// <1> Fill up pre common command.
		PreCommonCmdCnt = 0;
		rtl8192_phy_SetSwChnlCmdArray(PreCommonCmd, PreCommonCmdCnt++, MAX_PRECMD_CNT,
					CmdID_SetTxPowerLevel, 0, 0, 0);
		rtl8192_phy_SetSwChnlCmdArray(PreCommonCmd, PreCommonCmdCnt++, MAX_PRECMD_CNT,
					CmdID_End, 0, 0, 0);

		// <2> Fill up post common command.
		PostCommonCmdCnt = 0;

		rtl8192_phy_SetSwChnlCmdArray(PostCommonCmd, PostCommonCmdCnt++, MAX_POSTCMD_CNT,
					CmdID_End, 0, 0, 0);

		// <3> Fill up RF dependent command.
		RfDependCmdCnt = 0;
		switch( priv->rf_chip )
		{
		case RF_8225:
			if (!(channel >= 1 && channel <= 14))
			{
				RT_TRACE(COMP_ERR, "illegal channel for Zebra 8225: %d\n", channel);
				return true;
			}
			rtl8192_phy_SetSwChnlCmdArray(RfDependCmd, RfDependCmdCnt++, MAX_RFDEPENDCMD_CNT,
				CmdID_RF_WriteReg, rZebra1_Channel, RF_CHANNEL_TABLE_ZEBRA[channel], 10);
			rtl8192_phy_SetSwChnlCmdArray(RfDependCmd, RfDependCmdCnt++, MAX_RFDEPENDCMD_CNT,
				CmdID_End, 0, 0, 0);
			break;

		case RF_8256:
			// TEST!! This is not the table for 8256!!
			if (!(channel >= 1 && channel <= 14))
			{
				RT_TRACE(COMP_ERR, "illegal channel for Zebra 8256: %d\n", channel);
				return true;
			}
			rtl8192_phy_SetSwChnlCmdArray(RfDependCmd, RfDependCmdCnt++, MAX_RFDEPENDCMD_CNT,
				CmdID_RF_WriteReg, rZebra1_Channel, channel, 10);
			rtl8192_phy_SetSwChnlCmdArray(RfDependCmd, RfDependCmdCnt++, MAX_RFDEPENDCMD_CNT,
			CmdID_End, 0, 0, 0);
			break;

		case RF_8258:
			break;

		default:
			RT_TRACE(COMP_ERR, "Unknown RFChipID: %d\n", priv->rf_chip);
			return true;
			break;
		}


		do{
			switch(*stage)
			{
			case 0:
				CurrentCmd=&PreCommonCmd[*step];
				break;
			case 1:
				CurrentCmd=&RfDependCmd[*step];
				break;
			case 2:
				CurrentCmd=&PostCommonCmd[*step];
				break;
			}

			if(CurrentCmd->CmdID==CmdID_End)
			{
				if((*stage)==2)
				{
					(*delay)=CurrentCmd->msDelay;
					return true;
				}
				else
				{
					(*stage)++;
					(*step)=0;
					continue;
				}
			}

			switch(CurrentCmd->CmdID)
			{
			case CmdID_SetTxPowerLevel:
				if(priv->card_8192_version == (u8)VERSION_819xU_A) //xiong: consider it later!
					rtl8192_SetTxPowerLevel(dev,channel);
				break;
			case CmdID_WritePortUlong:
				write_nic_dword(dev, CurrentCmd->Para1, CurrentCmd->Para2);
				break;
			case CmdID_WritePortUshort:
				write_nic_word(dev, CurrentCmd->Para1, (u16)CurrentCmd->Para2);
				break;
			case CmdID_WritePortUchar:
				write_nic_byte(dev, CurrentCmd->Para1, (u8)CurrentCmd->Para2);
				break;
			case CmdID_RF_WriteReg:
				for(eRFPath = 0; eRFPath < RF90_PATH_MAX; eRFPath++)
				{
				rtl8192_phy_SetRFReg(dev, (RF90_RADIO_PATH_E)eRFPath, CurrentCmd->Para1, bZebra1_ChannelNum, CurrentCmd->Para2);
				}
				break;
			default:
				break;
			}

			break;
		}while(true);
//	}/*for(Number of RF paths)*/

	(*delay)=CurrentCmd->msDelay;
	(*step)++;
	return false;
}

/******************************************************************************
 *function:  This function does acturally set channel work
 *   input:  struct net_device *dev
 *   	     u8 		channel
 *  output:  none
 *  return:  noin
 *    Note:  We should not call this function directly
 * ***************************************************************************/
void rtl8192_phy_FinishSwChnlNow(struct net_device *dev, u8 channel)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u32	delay = 0;

	while(!rtl8192_phy_SwChnlStepByStep(dev,channel,&priv->SwChnlStage,&priv->SwChnlStep,&delay))
	{
	//	if(delay>0)
	//		msleep(delay);//or mdelay? need further consideration
		if(!priv->up)
			break;
	}
}
/******************************************************************************
 *function:  Callback routine of the work item for switch channel.
 *   input:
 *
 *  output:  none
 *  return:  noin
 * ***************************************************************************/
void rtl8192_SwChnl_WorkItem(struct net_device *dev)
{

	struct r8192_priv *priv = ieee80211_priv(dev);

	RT_TRACE(COMP_CH, "==> SwChnlCallback819xUsbWorkItem(), chan:%d\n", priv->chan);


	rtl8192_phy_FinishSwChnlNow(dev , priv->chan);

	RT_TRACE(COMP_CH, "<== SwChnlCallback819xUsbWorkItem()\n");
}

/******************************************************************************
 *function:  This function scheduled actural workitem to set channel
 *   input:  net_device dev
 *   	     u8		channel //channel to set
 *  output:  none
 *  return:  return code show if workitem is scheduled(1:pass, 0:fail)
 *    Note:  Delay may be required for RF configuration
 * ***************************************************************************/
u8 rtl8192_phy_SwChnl(struct net_device* dev, u8 channel)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	RT_TRACE(COMP_CH, "=====>%s(), SwChnlInProgress:%d\n", __FUNCTION__, priv->SwChnlInProgress);
	if(!priv->up)
		return false;
	if(priv->SwChnlInProgress)
		return false;

//	if(pHalData->SetBWModeInProgress)
//		return;
if (0) //to test current channel from RF reg 0x7.
{
	u8		eRFPath;
	for(eRFPath = 0; eRFPath < 2; eRFPath++){
	printk("====>set channel:%x\n",rtl8192_phy_QueryRFReg(dev, (RF90_RADIO_PATH_E)eRFPath, 0x7, bZebra1_ChannelNum));
	udelay(10);
	}
}
	//--------------------------------------------
	switch(priv->ieee80211->mode)
	{
	case WIRELESS_MODE_A:
	case WIRELESS_MODE_N_5G:
		if (channel<=14){
			RT_TRACE(COMP_ERR, "WIRELESS_MODE_A but channel<=14");
			return false;
		}
		break;
	case WIRELESS_MODE_B:
		if (channel>14){
			RT_TRACE(COMP_ERR, "WIRELESS_MODE_B but channel>14");
			return false;
		}
		break;
	case WIRELESS_MODE_G:
	case WIRELESS_MODE_N_24G:
		if (channel>14){
			RT_TRACE(COMP_ERR, "WIRELESS_MODE_G but channel>14");
			return false;
		}
		break;
	}
	//--------------------------------------------

	priv->SwChnlInProgress = true;
	if(channel == 0)
		channel = 1;

	priv->chan=channel;

	priv->SwChnlStage=0;
	priv->SwChnlStep=0;
//	schedule_work(&(priv->SwChnlWorkItem));
//	rtl8192_SwChnl_WorkItem(dev);
	if(priv->up) {
//		queue_work(priv->priv_wq,&(priv->SwChnlWorkItem));
	rtl8192_SwChnl_WorkItem(dev);
	}

	priv->SwChnlInProgress = false;
	return true;
}


//
/******************************************************************************
 *function:  Callback routine of the work item for set bandwidth mode.
 *   input:  struct net_device *dev
 *   	     HT_CHANNEL_WIDTH	Bandwidth  //20M or 40M
 *   	     HT_EXTCHNL_OFFSET Offset 	   //Upper, Lower, or Don't care
 *  output:  none
 *  return:  none
 *    Note:  I doubt whether SetBWModeInProgress flag is necessary as we can
 *    	     test whether current work in the queue or not.//do I?
 * ***************************************************************************/
void rtl8192_SetBWModeWorkItem(struct net_device *dev)
{

	struct r8192_priv *priv = ieee80211_priv(dev);
	u8 regBwOpMode;

	RT_TRACE(COMP_SWBW, "==>rtl8192_SetBWModeWorkItem()  Switch to %s bandwidth\n", \
					priv->CurrentChannelBW == HT_CHANNEL_WIDTH_20?"20MHz":"40MHz")


	if(priv->rf_chip == RF_PSEUDO_11N)
	{
		priv->SetBWModeInProgress= false;
		return;
	}

	//<1>Set MAC register
	regBwOpMode = read_nic_byte(dev, BW_OPMODE);

	switch(priv->CurrentChannelBW)
	{
		case HT_CHANNEL_WIDTH_20:
			regBwOpMode |= BW_OPMODE_20MHZ;
		       // 2007/02/07 Mark by Emily becasue we have not verify whether this register works
			write_nic_byte(dev, BW_OPMODE, regBwOpMode);
			break;

		case HT_CHANNEL_WIDTH_20_40:
			regBwOpMode &= ~BW_OPMODE_20MHZ;
			// 2007/02/07 Mark by Emily becasue we have not verify whether this register works
			write_nic_byte(dev, BW_OPMODE, regBwOpMode);
			break;

		default:
			RT_TRACE(COMP_ERR, "SetChannelBandwidth819xUsb(): unknown Bandwidth: %#X\n",priv->CurrentChannelBW);
			break;
	}

	//<2>Set PHY related register
	switch(priv->CurrentChannelBW)
	{
		case HT_CHANNEL_WIDTH_20:
			// Add by Vivi 20071119
			rtl8192_setBBreg(dev, rFPGA0_RFMOD, bRFMOD, 0x0);
			rtl8192_setBBreg(dev, rFPGA1_RFMOD, bRFMOD, 0x0);
			rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, 0x00100000, 1);

			// Correct the tx power for CCK rate in 20M. Suggest by YN, 20071207
			priv->cck_present_attentuation =
				priv->cck_present_attentuation_20Mdefault + priv->cck_present_attentuation_difference;

			if(priv->cck_present_attentuation > 22)
				priv->cck_present_attentuation= 22;
			if(priv->cck_present_attentuation< 0)
				priv->cck_present_attentuation = 0;
			RT_TRACE(COMP_INIT, "20M, pHalData->CCKPresentAttentuation = %d\n", priv->cck_present_attentuation);

			if(priv->chan == 14 && !priv->bcck_in_ch14)
			{
				priv->bcck_in_ch14 = TRUE;
				dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);
			}
			else if(priv->chan != 14 && priv->bcck_in_ch14)
			{
				priv->bcck_in_ch14 = FALSE;
				dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);
			}
			else
				dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);

			break;
		case HT_CHANNEL_WIDTH_20_40:
			// Add by Vivi 20071119
			rtl8192_setBBreg(dev, rFPGA0_RFMOD, bRFMOD, 0x1);
			rtl8192_setBBreg(dev, rFPGA1_RFMOD, bRFMOD, 0x1);
			rtl8192_setBBreg(dev, rCCK0_System, bCCKSideBand, (priv->nCur40MhzPrimeSC>>1));
			rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, 0x00100000, 0);
			rtl8192_setBBreg(dev, rOFDM1_LSTF, 0xC00, priv->nCur40MhzPrimeSC);
			priv->cck_present_attentuation =
				priv->cck_present_attentuation_40Mdefault + priv->cck_present_attentuation_difference;

			if(priv->cck_present_attentuation > 22)
				priv->cck_present_attentuation = 22;
			if(priv->cck_present_attentuation < 0)
				priv->cck_present_attentuation = 0;

			RT_TRACE(COMP_INIT, "40M, pHalData->CCKPresentAttentuation = %d\n", priv->cck_present_attentuation);
			if(priv->chan == 14 && !priv->bcck_in_ch14)
			{
				priv->bcck_in_ch14 = true;
				dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);
			}
			else if(priv->chan!= 14 && priv->bcck_in_ch14)
			{
				priv->bcck_in_ch14 = false;
				dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);
			}
			else
				dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);

			break;
		default:
			RT_TRACE(COMP_ERR, "SetChannelBandwidth819xUsb(): unknown Bandwidth: %#X\n" ,priv->CurrentChannelBW);
			break;

	}
	//Skip over setting of J-mode in BB register here. Default value is "None J mode". Emily 20070315

	//<3>Set RF related register
	switch( priv->rf_chip )
	{
		case RF_8225:
#ifdef TO_DO_LIST
			PHY_SetRF8225Bandwidth(Adapter, pHalData->CurrentChannelBW);
#endif
			break;

		case RF_8256:
			PHY_SetRF8256Bandwidth(dev, priv->CurrentChannelBW);
			break;

		case RF_8258:
			// PHY_SetRF8258Bandwidth();
			break;

		case RF_PSEUDO_11N:
			// Do Nothing
			break;

		default:
			RT_TRACE(COMP_ERR, "Unknown RFChipID: %d\n", priv->rf_chip);
			break;
	}
	priv->SetBWModeInProgress= false;

	RT_TRACE(COMP_SWBW, "<==SetBWMode819xUsb(), %d", atomic_read(&(priv->ieee80211->atm_swbw)) );
}

/******************************************************************************
 *function:  This function schedules bandwith switch work.
 *   input:  struct net_device *dev
 *   	     HT_CHANNEL_WIDTH	Bandwidth  //20M or 40M
 *   	     HT_EXTCHNL_OFFSET Offset 	   //Upper, Lower, or Don't care
 *  output:  none
 *  return:  none
 *    Note:  I doubt whether SetBWModeInProgress flag is necessary as we can
 *    	     test whether current work in the queue or not.//do I?
 * ***************************************************************************/
void rtl8192_SetBWMode(struct net_device *dev, HT_CHANNEL_WIDTH	Bandwidth, HT_EXTCHNL_OFFSET Offset)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	if(priv->SetBWModeInProgress)
		return;
	priv->SetBWModeInProgress= true;

	priv->CurrentChannelBW = Bandwidth;

	if(Offset==HT_EXTCHNL_OFFSET_LOWER)
		priv->nCur40MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_UPPER;
	else if(Offset==HT_EXTCHNL_OFFSET_UPPER)
		priv->nCur40MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_LOWER;
	else
		priv->nCur40MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_DONT_CARE;

	//queue_work(priv->priv_wq, &(priv->SetBWModeWorkItem));
	//	schedule_work(&(priv->SetBWModeWorkItem));
	rtl8192_SetBWModeWorkItem(dev);

}

void InitialGain819xUsb(struct net_device *dev,	u8 Operation)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	priv->InitialGainOperateType = Operation;

	if(priv->up)
	{
		queue_delayed_work(priv->priv_wq,&priv->initialgain_operate_wq,0);
	}
}

extern void InitialGainOperateWorkItemCallBack(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work,struct delayed_work,work);
       struct r8192_priv *priv = container_of(dwork,struct r8192_priv,initialgain_operate_wq);
       struct net_device *dev = priv->ieee80211->dev;
#define SCAN_RX_INITIAL_GAIN	0x17
#define POWER_DETECTION_TH	0x08
	u32	BitMask;
	u8	initial_gain;
	u8	Operation;

	Operation = priv->InitialGainOperateType;

	switch(Operation)
	{
		case IG_Backup:
			RT_TRACE(COMP_SCAN, "IG_Backup, backup the initial gain.\n");
			initial_gain = SCAN_RX_INITIAL_GAIN;//priv->DefaultInitialGain[0];//
			BitMask = bMaskByte0;
			if(dm_digtable.dig_algorithm == DIG_ALGO_BY_FALSE_ALARM)
				rtl8192_setBBreg(dev, UFWP, bMaskByte1, 0x8);	// FW DIG OFF
			priv->initgain_backup.xaagccore1 = (u8)rtl8192_QueryBBReg(dev, rOFDM0_XAAGCCore1, BitMask);
			priv->initgain_backup.xbagccore1 = (u8)rtl8192_QueryBBReg(dev, rOFDM0_XBAGCCore1, BitMask);
			priv->initgain_backup.xcagccore1 = (u8)rtl8192_QueryBBReg(dev, rOFDM0_XCAGCCore1, BitMask);
			priv->initgain_backup.xdagccore1 = (u8)rtl8192_QueryBBReg(dev, rOFDM0_XDAGCCore1, BitMask);
			BitMask  = bMaskByte2;
			priv->initgain_backup.cca		= (u8)rtl8192_QueryBBReg(dev, rCCK0_CCA, BitMask);

			RT_TRACE(COMP_SCAN, "Scan InitialGainBackup 0xc50 is %x\n",priv->initgain_backup.xaagccore1);
			RT_TRACE(COMP_SCAN, "Scan InitialGainBackup 0xc58 is %x\n",priv->initgain_backup.xbagccore1);
			RT_TRACE(COMP_SCAN, "Scan InitialGainBackup 0xc60 is %x\n",priv->initgain_backup.xcagccore1);
			RT_TRACE(COMP_SCAN, "Scan InitialGainBackup 0xc68 is %x\n",priv->initgain_backup.xdagccore1);
			RT_TRACE(COMP_SCAN, "Scan InitialGainBackup 0xa0a is %x\n",priv->initgain_backup.cca);

			RT_TRACE(COMP_SCAN, "Write scan initial gain = 0x%x \n", initial_gain);
			write_nic_byte(dev, rOFDM0_XAAGCCore1, initial_gain);
			write_nic_byte(dev, rOFDM0_XBAGCCore1, initial_gain);
			write_nic_byte(dev, rOFDM0_XCAGCCore1, initial_gain);
			write_nic_byte(dev, rOFDM0_XDAGCCore1, initial_gain);
			RT_TRACE(COMP_SCAN, "Write scan 0xa0a = 0x%x \n", POWER_DETECTION_TH);
			write_nic_byte(dev, 0xa0a, POWER_DETECTION_TH);
			break;
		case IG_Restore:
			RT_TRACE(COMP_SCAN, "IG_Restore, restore the initial gain.\n");
			BitMask = 0x7f; //Bit0~ Bit6
			if(dm_digtable.dig_algorithm == DIG_ALGO_BY_FALSE_ALARM)
				rtl8192_setBBreg(dev, UFWP, bMaskByte1, 0x8);	// FW DIG OFF

			rtl8192_setBBreg(dev, rOFDM0_XAAGCCore1, BitMask, (u32)priv->initgain_backup.xaagccore1);
			rtl8192_setBBreg(dev, rOFDM0_XBAGCCore1, BitMask, (u32)priv->initgain_backup.xbagccore1);
			rtl8192_setBBreg(dev, rOFDM0_XCAGCCore1, BitMask, (u32)priv->initgain_backup.xcagccore1);
			rtl8192_setBBreg(dev, rOFDM0_XDAGCCore1, BitMask, (u32)priv->initgain_backup.xdagccore1);
			BitMask  = bMaskByte2;
			rtl8192_setBBreg(dev, rCCK0_CCA, BitMask, (u32)priv->initgain_backup.cca);

			RT_TRACE(COMP_SCAN, "Scan BBInitialGainRestore 0xc50 is %x\n",priv->initgain_backup.xaagccore1);
			RT_TRACE(COMP_SCAN, "Scan BBInitialGainRestore 0xc58 is %x\n",priv->initgain_backup.xbagccore1);
			RT_TRACE(COMP_SCAN, "Scan BBInitialGainRestore 0xc60 is %x\n",priv->initgain_backup.xcagccore1);
			RT_TRACE(COMP_SCAN, "Scan BBInitialGainRestore 0xc68 is %x\n",priv->initgain_backup.xdagccore1);
			RT_TRACE(COMP_SCAN, "Scan BBInitialGainRestore 0xa0a is %x\n",priv->initgain_backup.cca);

#ifdef RTL8190P
			SetTxPowerLevel8190(Adapter,priv->CurrentChannel);
#endif
#ifdef RTL8192E
			SetTxPowerLevel8190(Adapter,priv->CurrentChannel);
#endif
//#ifdef RTL8192U
			rtl8192_phy_setTxPower(dev,priv->ieee80211->current_network.channel);
//#endif

			if(dm_digtable.dig_algorithm == DIG_ALGO_BY_FALSE_ALARM)
				rtl8192_setBBreg(dev, UFWP, bMaskByte1, 0x1);	// FW DIG ON
			break;
		default:
			RT_TRACE(COMP_SCAN, "Unknown IG Operation. \n");
			break;
	}
}
