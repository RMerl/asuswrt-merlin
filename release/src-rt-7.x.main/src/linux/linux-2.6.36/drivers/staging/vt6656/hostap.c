/*
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * File: hostap.c
 *
 * Purpose: handle hostap deamon ioctl input/out functions
 *
 * Author: Lyndon Chen
 *
 * Date: Oct. 20, 2003
 *
 * Functions:
 *
 * Revision History:
 *
 */

#include "hostap.h"
#include "iocmd.h"
#include "mac.h"
#include "card.h"
#include "baseband.h"
#include "wpactl.h"
#include "key.h"
#include "datarate.h"

#define VIAWGET_HOSTAPD_MAX_BUF_SIZE 1024
#define HOSTAP_CRYPT_FLAG_SET_TX_KEY BIT0
#define HOSTAP_CRYPT_FLAG_PERMANENT BIT1
#define HOSTAP_CRYPT_ERR_UNKNOWN_ALG 2
#define HOSTAP_CRYPT_ERR_UNKNOWN_ADDR 3
#define HOSTAP_CRYPT_ERR_CRYPT_INIT_FAILED 4
#define HOSTAP_CRYPT_ERR_KEY_SET_FAILED 5
#define HOSTAP_CRYPT_ERR_TX_KEY_SET_FAILED 6
#define HOSTAP_CRYPT_ERR_CARD_CONF_FAILED 7


/*---------------------  Static Definitions -------------------------*/

/*---------------------  Static Classes  ----------------------------*/

/*---------------------  Static Variables  --------------------------*/
//static int          msglevel                =MSG_LEVEL_DEBUG;
static int          msglevel                =MSG_LEVEL_INFO;

/*---------------------  Static Functions  --------------------------*/




/*---------------------  Export Variables  --------------------------*/


/*
 * Description:
 *      register net_device (AP) for hostap deamon
 *
 * Parameters:
 *  In:
 *      pDevice             -
 *      rtnl_locked         -
 *  Out:
 *
 * Return Value:
 *
 */

static int hostap_enable_hostapd(PSDevice pDevice, int rtnl_locked)
{
    PSDevice apdev_priv;
	struct net_device *dev = pDevice->dev;
	int ret;
	const struct net_device_ops apdev_netdev_ops = {
		.ndo_start_xmit         = pDevice->tx_80211,
	};

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "%s: Enabling hostapd mode\n", dev->name);

	pDevice->apdev = kzalloc(sizeof(struct net_device), GFP_KERNEL);
	if (pDevice->apdev == NULL)
		return -ENOMEM;

    apdev_priv = netdev_priv(pDevice->apdev);
    *apdev_priv = *pDevice;
	memcpy(pDevice->apdev->dev_addr, dev->dev_addr, ETH_ALEN);

	pDevice->apdev->netdev_ops = &apdev_netdev_ops;

	pDevice->apdev->type = ARPHRD_IEEE80211;

	pDevice->apdev->base_addr = dev->base_addr;
	pDevice->apdev->irq = dev->irq;
	pDevice->apdev->mem_start = dev->mem_start;
	pDevice->apdev->mem_end = dev->mem_end;
	sprintf(pDevice->apdev->name, "%sap", dev->name);
	if (rtnl_locked)
		ret = register_netdevice(pDevice->apdev);
	else
		ret = register_netdev(pDevice->apdev);
	if (ret) {
		DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "%s: register_netdevice(AP) failed!\n",
		       dev->name);
		return -1;
	}

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "%s: Registered netdevice %s for AP management\n",
	       dev->name, pDevice->apdev->name);

    KeyvInitTable(pDevice,&pDevice->sKey);

	return 0;
}

/*
 * Description:
 *      unregister net_device(AP)
 *
 * Parameters:
 *  In:
 *      pDevice             -
 *      rtnl_locked         -
 *  Out:
 *
 * Return Value:
 *
 */

static int hostap_disable_hostapd(PSDevice pDevice, int rtnl_locked)
{

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "%s: disabling hostapd mode\n", pDevice->dev->name);

    if (pDevice->apdev && pDevice->apdev->name && pDevice->apdev->name[0]) {
		if (rtnl_locked)
			unregister_netdevice(pDevice->apdev);
		else
			unregister_netdev(pDevice->apdev);
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "%s: Netdevice %s unregistered\n",
		       pDevice->dev->name, pDevice->apdev->name);
	}
	kfree(pDevice->apdev);
	pDevice->apdev = NULL;
    pDevice->bEnable8021x = FALSE;
    pDevice->bEnableHostWEP = FALSE;
    pDevice->bEncryptionEnable = FALSE;

	return 0;
}


/*
 * Description:
 *      Set enable/disable hostapd mode
 *
 * Parameters:
 *  In:
 *      pDevice             -
 *      rtnl_locked         -
 *  Out:
 *
 * Return Value:
 *
 */

int vt6656_hostap_set_hostapd(PSDevice pDevice, int val, int rtnl_locked)
{
	if (val < 0 || val > 1)
		return -EINVAL;

	if (pDevice->bEnableHostapd == val)
		return 0;

	pDevice->bEnableHostapd = val;

	if (val)
		return hostap_enable_hostapd(pDevice, rtnl_locked);
	else
		return hostap_disable_hostapd(pDevice, rtnl_locked);
}


/*
 * Description:
 *      remove station function supported for hostap deamon
 *
 * Parameters:
 *  In:
 *      pDevice   -
 *      param     -
 *  Out:
 *
 * Return Value:
 *
 */
static int hostap_remove_sta(PSDevice pDevice,
				     struct viawget_hostapd_param *param)
{
	unsigned int uNodeIndex;


    if (BSSbIsSTAInNodeDB(pDevice, param->sta_addr, &uNodeIndex)) {
        BSSvRemoveOneNode(pDevice, uNodeIndex);
    }
    else {
        return -ENOENT;
    }
	return 0;
}

/*
 * Description:
 *      add a station from hostap deamon
 *
 * Parameters:
 *  In:
 *      pDevice   -
 *      param     -
 *  Out:
 *
 * Return Value:
 *
 */
static int hostap_add_sta(PSDevice pDevice,
				  struct viawget_hostapd_param *param)
{
    PSMgmtObject    pMgmt = &(pDevice->sMgmtObj);
	unsigned int uNodeIndex;


    if (!BSSbIsSTAInNodeDB(pDevice, param->sta_addr, &uNodeIndex)) {
        BSSvCreateOneNode((PSDevice)pDevice, &uNodeIndex);
    }
    memcpy(pMgmt->sNodeDBTable[uNodeIndex].abyMACAddr, param->sta_addr, WLAN_ADDR_LEN);
    pMgmt->sNodeDBTable[uNodeIndex].eNodeState = NODE_ASSOC;
    pMgmt->sNodeDBTable[uNodeIndex].wCapInfo = param->u.add_sta.capability;
// TODO listenInterval
//    pMgmt->sNodeDBTable[uNodeIndex].wListenInterval = 1;
    pMgmt->sNodeDBTable[uNodeIndex].bPSEnable = FALSE;
    pMgmt->sNodeDBTable[uNodeIndex].bySuppRate = param->u.add_sta.tx_supp_rates;

    // set max tx rate
    pMgmt->sNodeDBTable[uNodeIndex].wTxDataRate =
           pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate;
    // set max basic rate
    pMgmt->sNodeDBTable[uNodeIndex].wMaxBasicRate = RATE_2M;
    // Todo: check sta preamble, if ap can't support, set status code
    pMgmt->sNodeDBTable[uNodeIndex].bShortPreamble =
            WLAN_GET_CAP_INFO_SHORTPREAMBLE(pMgmt->sNodeDBTable[uNodeIndex].wCapInfo);

    pMgmt->sNodeDBTable[uNodeIndex].wAID = (WORD)param->u.add_sta.aid;

    pMgmt->sNodeDBTable[uNodeIndex].ulLastRxJiffer = jiffies;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Add STA AID= %d \n", pMgmt->sNodeDBTable[uNodeIndex].wAID);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "MAC=%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X \n",
               param->sta_addr[0],
               param->sta_addr[1],
               param->sta_addr[2],
               param->sta_addr[3],
               param->sta_addr[4],
               param->sta_addr[5]
              ) ;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Max Support rate = %d \n",
               pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate);

	return 0;
}

/*
 * Description:
 *      get station info
 *
 * Parameters:
 *  In:
 *      pDevice   -
 *      param     -
 *  Out:
 *
 * Return Value:
 *
 */

static int hostap_get_info_sta(PSDevice pDevice,
				       struct viawget_hostapd_param *param)
{
    PSMgmtObject    pMgmt = &(pDevice->sMgmtObj);
	unsigned int uNodeIndex;

    if (BSSbIsSTAInNodeDB(pDevice, param->sta_addr, &uNodeIndex)) {
	    param->u.get_info_sta.inactive_sec =
	        (jiffies - pMgmt->sNodeDBTable[uNodeIndex].ulLastRxJiffer) / HZ;

	    //param->u.get_info_sta.txexc = pMgmt->sNodeDBTable[uNodeIndex].uTxAttempts;
	}
	else {
	    return -ENOENT;
	}

	return 0;
}

/*
 * Description:
 *      reset txexec
 *
 * Parameters:
 *  In:
 *      pDevice   -
 *      param     -
 *  Out:
 *      TURE, FALSE
 *
 * Return Value:
 *
 */
/*
static int hostap_reset_txexc_sta(PSDevice pDevice,
					  struct viawget_hostapd_param *param)
{
    PSMgmtObject    pMgmt = &(pDevice->sMgmtObj);
	unsigned int uNodeIndex;

    if (BSSbIsSTAInNodeDB(pDevice, param->sta_addr, &uNodeIndex)) {
        pMgmt->sNodeDBTable[uNodeIndex].uTxAttempts = 0;
	}
	else {
	    return -ENOENT;
	}

	return 0;
}
*/

/*
 * Description:
 *      set station flag
 *
 * Parameters:
 *  In:
 *      pDevice   -
 *      param     -
 *  Out:
 *
 * Return Value:
 *
 */
static int hostap_set_flags_sta(PSDevice pDevice,
					struct viawget_hostapd_param *param)
{
    PSMgmtObject    pMgmt = &(pDevice->sMgmtObj);
	unsigned int uNodeIndex;

    if (BSSbIsSTAInNodeDB(pDevice, param->sta_addr, &uNodeIndex)) {
		pMgmt->sNodeDBTable[uNodeIndex].dwFlags |= param->u.set_flags_sta.flags_or;
		pMgmt->sNodeDBTable[uNodeIndex].dwFlags &= param->u.set_flags_sta.flags_and;
		DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO " dwFlags = %x\n",
			(unsigned int) pMgmt->sNodeDBTable[uNodeIndex].dwFlags);
	}
	else {
	    return -ENOENT;
	}

	return 0;
}



/*
 * Description:
 *      set generic element (wpa ie)
 *
 * Parameters:
 *  In:
 *      pDevice   -
 *      param     -
 *  Out:
 *
 * Return Value:
 *
 */
static int hostap_set_generic_element(PSDevice pDevice,
					struct viawget_hostapd_param *param)
{
    PSMgmtObject    pMgmt = &(pDevice->sMgmtObj);



    memcpy( pMgmt->abyWPAIE,
            param->u.generic_elem.data,
            param->u.generic_elem.len
           );

    pMgmt->wWPAIELen = 	param->u.generic_elem.len;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"pMgmt->wWPAIELen = %d\n",  pMgmt->wWPAIELen);

    // disable wpa
    if (pMgmt->wWPAIELen == 0) {
        pMgmt->eAuthenMode = WMAC_AUTH_OPEN;
		DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO " No WPAIE, Disable WPA \n");
    } else  {
        // enable wpa
        if ((pMgmt->abyWPAIE[0] == WLAN_EID_RSN_WPA) ||
             (pMgmt->abyWPAIE[0] == WLAN_EID_RSN)) {
              pMgmt->eAuthenMode = WMAC_AUTH_WPANONE;
               DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Set WPAIE enable WPA\n");
        } else
            return -EINVAL;
    }

	return 0;
}

/*
 * Description:
 *      flush station nodes table.
 *
 * Parameters:
 *  In:
 *      pDevice   -
 *  Out:
 *
 * Return Value:
 *
 */

static void hostap_flush_sta(PSDevice pDevice)
{
    // reserved node index =0 for multicast node.
    BSSvClearNodeDBTable(pDevice, 1);
    pDevice->uAssocCount = 0;

    return;
}

/*
 * Description:
 *      set each stations encryption key
 *
 * Parameters:
 *  In:
 *      pDevice   -
 *      param     -
 *  Out:
 *
 * Return Value:
 *
 */
static int hostap_set_encryption(PSDevice pDevice,
				       struct viawget_hostapd_param *param,
				       int param_len)
{
    PSMgmtObject    pMgmt = &(pDevice->sMgmtObj);
    DWORD   dwKeyIndex = 0;
    BYTE    abyKey[MAX_KEY_LEN];
    BYTE    abySeq[MAX_KEY_LEN];
    NDIS_802_11_KEY_RSC   KeyRSC;
    BYTE    byKeyDecMode = KEY_CTL_WEP;
	int     ret = 0;
	int     iNodeIndex = -1;
	int     ii;
	BOOL    bKeyTableFull = FALSE;
	WORD    wKeyCtl = 0;


	param->u.crypt.err = 0;
/*
	if (param_len !=
	    (int) ((char *) param->u.crypt.key - (char *) param) +
	    param->u.crypt.key_len)
		return -EINVAL;
*/

	if (param->u.crypt.alg > WPA_ALG_CCMP)
		return -EINVAL;


	if ((param->u.crypt.idx > 3) || (param->u.crypt.key_len > MAX_KEY_LEN)) {
		param->u.crypt.err = HOSTAP_CRYPT_ERR_KEY_SET_FAILED;
		DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO " HOSTAP_CRYPT_ERR_KEY_SET_FAILED\n");
		return -EINVAL;
	}

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff) {
		if (param->u.crypt.idx >= MAX_GROUP_KEY)
			return -EINVAL;
        iNodeIndex = 0;

	} else {
	    if (BSSbIsSTAInNodeDB(pDevice, param->sta_addr, &iNodeIndex) == FALSE) {
	        param->u.crypt.err = HOSTAP_CRYPT_ERR_UNKNOWN_ADDR;
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO " HOSTAP_CRYPT_ERR_UNKNOWN_ADDR\n");
	        return -EINVAL;
	    }
	}
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO " hostap_set_encryption: sta_index %d \n", iNodeIndex);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO " hostap_set_encryption: alg %d \n", param->u.crypt.alg);

	if (param->u.crypt.alg == WPA_ALG_NONE) {

        if (pMgmt->sNodeDBTable[iNodeIndex].bOnFly == TRUE) {
            if (KeybRemoveKey( pDevice,
                               &(pDevice->sKey),
                               param->sta_addr,
                               pMgmt->sNodeDBTable[iNodeIndex].dwKeyIndex
                              ) == FALSE) {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "KeybRemoveKey fail \n");
            }
            pMgmt->sNodeDBTable[iNodeIndex].bOnFly = FALSE;
        }
        pMgmt->sNodeDBTable[iNodeIndex].byKeyIndex = 0;
        pMgmt->sNodeDBTable[iNodeIndex].dwKeyIndex = 0;
        pMgmt->sNodeDBTable[iNodeIndex].uWepKeyLength = 0;
        pMgmt->sNodeDBTable[iNodeIndex].KeyRSC = 0;
        pMgmt->sNodeDBTable[iNodeIndex].dwTSC47_16 = 0;
        pMgmt->sNodeDBTable[iNodeIndex].wTSC15_0 = 0;
        pMgmt->sNodeDBTable[iNodeIndex].byCipherSuite = 0;
        memset(&pMgmt->sNodeDBTable[iNodeIndex].abyWepKey[0],
                0,
                MAX_KEY_LEN
               );

        return ret;
	}

    memcpy(abyKey, param->u.crypt.key, param->u.crypt.key_len);
    // copy to node key tbl
    pMgmt->sNodeDBTable[iNodeIndex].byKeyIndex = param->u.crypt.idx;
    pMgmt->sNodeDBTable[iNodeIndex].uWepKeyLength = param->u.crypt.key_len;
    memcpy(&pMgmt->sNodeDBTable[iNodeIndex].abyWepKey[0],
            param->u.crypt.key,
            param->u.crypt.key_len
           );

    dwKeyIndex = (DWORD)(param->u.crypt.idx);
    if (param->u.crypt.flags & HOSTAP_CRYPT_FLAG_SET_TX_KEY) {
        pDevice->byKeyIndex = (BYTE)dwKeyIndex;
        pDevice->bTransmitKey = TRUE;
        dwKeyIndex |= (1 << 31);
    }

	if (param->u.crypt.alg == WPA_ALG_WEP) {

        if ((pDevice->bEnable8021x == FALSE) || (iNodeIndex == 0)) {
            KeybSetDefaultKey(  pDevice,
                                &(pDevice->sKey),
                                dwKeyIndex & ~(BIT30 | USE_KEYRSC),
                                param->u.crypt.key_len,
                                NULL,
                                abyKey,
                                KEY_CTL_WEP
                             );

        } else {
            // 8021x enable, individual key
            dwKeyIndex |= (1 << 30); // set pairwise key
            if (KeybSetKey(pDevice,
                           &(pDevice->sKey),
                           &param->sta_addr[0],
                           dwKeyIndex & ~(USE_KEYRSC),
                           param->u.crypt.key_len,
                           (PQWORD) &(KeyRSC),
                           (PBYTE)abyKey,
                            KEY_CTL_WEP
                           ) == TRUE) {


                pMgmt->sNodeDBTable[iNodeIndex].bOnFly = TRUE;

            } else {
                // Key Table Full
                pMgmt->sNodeDBTable[iNodeIndex].bOnFly = FALSE;
                bKeyTableFull = TRUE;
            }
        }
        pDevice->eEncryptionStatus = Ndis802_11Encryption1Enabled;
        pDevice->bEncryptionEnable = TRUE;
        pMgmt->byCSSPK = KEY_CTL_WEP;
        pMgmt->byCSSGK = KEY_CTL_WEP;
        pMgmt->sNodeDBTable[iNodeIndex].byCipherSuite = KEY_CTL_WEP;
        pMgmt->sNodeDBTable[iNodeIndex].dwKeyIndex = dwKeyIndex;
        return ret;
	}

	if (param->u.crypt.seq) {
	    memcpy(&abySeq, param->u.crypt.seq, 8);
		for (ii = 0 ; ii < 8 ; ii++) {
	         KeyRSC |= (abySeq[ii] << (ii * 8));
		}
		dwKeyIndex |= 1 << 29;
		pMgmt->sNodeDBTable[iNodeIndex].KeyRSC = KeyRSC;
	}

	if (param->u.crypt.alg == WPA_ALG_TKIP) {
	    if (param->u.crypt.key_len != MAX_KEY_LEN)
	        return -EINVAL;
	    pDevice->eEncryptionStatus = Ndis802_11Encryption2Enabled;
        byKeyDecMode = KEY_CTL_TKIP;
        pMgmt->byCSSPK = KEY_CTL_TKIP;
        pMgmt->byCSSGK = KEY_CTL_TKIP;
	}

	if (param->u.crypt.alg == WPA_ALG_CCMP) {
	    if ((param->u.crypt.key_len != AES_KEY_LEN) ||
	        (pDevice->byLocalID <= REV_ID_VT3253_A1))
	        return -EINVAL;
        pDevice->eEncryptionStatus = Ndis802_11Encryption3Enabled;
        byKeyDecMode = KEY_CTL_CCMP;
        pMgmt->byCSSPK = KEY_CTL_CCMP;
        pMgmt->byCSSGK = KEY_CTL_CCMP;
    }


    if (iNodeIndex == 0) {
       KeybSetDefaultKey(  pDevice,
                           &(pDevice->sKey),
                           dwKeyIndex,
                           param->u.crypt.key_len,
                           (PQWORD) &(KeyRSC),
                           abyKey,
                           byKeyDecMode
                          );
       pMgmt->sNodeDBTable[iNodeIndex].bOnFly = TRUE;

    } else {
        dwKeyIndex |= (1 << 30); // set pairwise key
        if (KeybSetKey(pDevice,
                       &(pDevice->sKey),
                       &param->sta_addr[0],
                       dwKeyIndex,
                       param->u.crypt.key_len,
                       (PQWORD) &(KeyRSC),
                       (PBYTE)abyKey,
                        byKeyDecMode
                       ) == TRUE) {

            pMgmt->sNodeDBTable[iNodeIndex].bOnFly = TRUE;

        } else {
            // Key Table Full
            pMgmt->sNodeDBTable[iNodeIndex].bOnFly = FALSE;
            bKeyTableFull = TRUE;
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO " Key Table Full\n");
        }

    }

    if (bKeyTableFull == TRUE) {
        wKeyCtl &= 0x7F00;              // clear all key control filed
        wKeyCtl |= (byKeyDecMode << 4);
        wKeyCtl |= (byKeyDecMode);
        wKeyCtl |= 0x0044;              // use group key for all address
        wKeyCtl |= 0x4000;              // disable KeyTable[MAX_KEY_TABLE-1] on-fly to genernate rx int
// Todo.. xxxxxx
        //MACvSetDefaultKeyCtl(pDevice->PortOffset, wKeyCtl, MAX_KEY_TABLE-1, pDevice->byLocalID);
    }

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO " Set key sta_index= %d \n", iNodeIndex);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO " tx_index=%d len=%d \n", param->u.crypt.idx,
               param->u.crypt.key_len );
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO " key=%x-%x-%x-%x-%x-xxxxx \n",
               pMgmt->sNodeDBTable[iNodeIndex].abyWepKey[0],
               pMgmt->sNodeDBTable[iNodeIndex].abyWepKey[1],
               pMgmt->sNodeDBTable[iNodeIndex].abyWepKey[2],
               pMgmt->sNodeDBTable[iNodeIndex].abyWepKey[3],
               pMgmt->sNodeDBTable[iNodeIndex].abyWepKey[4]
              );

	// set wep key
    pDevice->bEncryptionEnable = TRUE;
    pMgmt->sNodeDBTable[iNodeIndex].byCipherSuite = byKeyDecMode;
    pMgmt->sNodeDBTable[iNodeIndex].dwKeyIndex = dwKeyIndex;
    pMgmt->sNodeDBTable[iNodeIndex].dwTSC47_16 = 0;
    pMgmt->sNodeDBTable[iNodeIndex].wTSC15_0 = 0;

	return ret;
}



/*
 * Description:
 *      get each stations encryption key
 *
 * Parameters:
 *  In:
 *      pDevice   -
 *      param     -
 *  Out:
 *
 * Return Value:
 *
 */
static int hostap_get_encryption(PSDevice pDevice,
				       struct viawget_hostapd_param *param,
				       int param_len)
{
    PSMgmtObject    pMgmt = &(pDevice->sMgmtObj);
	int     ret = 0;
	int     ii;
	int     iNodeIndex =0;


	param->u.crypt.err = 0;

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff) {
        iNodeIndex = 0;
	} else {
	    if (BSSbIsSTAInNodeDB(pDevice, param->sta_addr, &iNodeIndex) == FALSE) {
	        param->u.crypt.err = HOSTAP_CRYPT_ERR_UNKNOWN_ADDR;
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "hostap_get_encryption: HOSTAP_CRYPT_ERR_UNKNOWN_ADDR\n");
	        return -EINVAL;
	    }
	}
	DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "hostap_get_encryption: %d\n", iNodeIndex);
    memset(param->u.crypt.seq, 0, 8);
    for (ii = 0 ; ii < 8 ; ii++) {
        param->u.crypt.seq[ii] = (BYTE)pMgmt->sNodeDBTable[iNodeIndex].KeyRSC >> (ii * 8);
    }

	return ret;
}


/*
 * Description:
 *      vt6656_hostap_ioctl main function supported for hostap deamon.
 *
 * Parameters:
 *  In:
 *      pDevice   -
 *      iw_point  -
 *  Out:
 *
 * Return Value:
 *
 */

int vt6656_hostap_ioctl(PSDevice pDevice, struct iw_point *p)
{
	struct viawget_hostapd_param *param;
	int ret = 0;
	int ap_ioctl = 0;

	if (p->length < sizeof(struct viawget_hostapd_param) ||
	    p->length > VIAWGET_HOSTAPD_MAX_BUF_SIZE || !p->pointer)
		return -EINVAL;

	param = kmalloc((int)p->length, (int)GFP_KERNEL);
	if (param == NULL)
		return -ENOMEM;

	if (copy_from_user(param, p->pointer, p->length)) {
		ret = -EFAULT;
		goto out;
	}

	switch (param->cmd) {
	case VIAWGET_HOSTAPD_SET_ENCRYPTION:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "VIAWGET_HOSTAPD_SET_ENCRYPTION \n");
        spin_lock_irq(&pDevice->lock);
		ret = hostap_set_encryption(pDevice, param, p->length);
        spin_unlock_irq(&pDevice->lock);
		break;
	case VIAWGET_HOSTAPD_GET_ENCRYPTION:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "VIAWGET_HOSTAPD_GET_ENCRYPTION \n");
        spin_lock_irq(&pDevice->lock);
		ret = hostap_get_encryption(pDevice, param, p->length);
        spin_unlock_irq(&pDevice->lock);
		break;
	case VIAWGET_HOSTAPD_SET_ASSOC_AP_ADDR:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "VIAWGET_HOSTAPD_SET_ASSOC_AP_ADDR \n");
		return -EOPNOTSUPP;
		break;
	case VIAWGET_HOSTAPD_FLUSH:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "VIAWGET_HOSTAPD_FLUSH \n");
        spin_lock_irq(&pDevice->lock);
    	hostap_flush_sta(pDevice);
        spin_unlock_irq(&pDevice->lock);
		break;
	case VIAWGET_HOSTAPD_ADD_STA:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "VIAWGET_HOSTAPD_ADD_STA \n");
         spin_lock_irq(&pDevice->lock);
		 ret = hostap_add_sta(pDevice, param);
         spin_unlock_irq(&pDevice->lock);
		break;
	case VIAWGET_HOSTAPD_REMOVE_STA:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "VIAWGET_HOSTAPD_REMOVE_STA \n");
         spin_lock_irq(&pDevice->lock);
		 ret = hostap_remove_sta(pDevice, param);
         spin_unlock_irq(&pDevice->lock);
		break;
	case VIAWGET_HOSTAPD_GET_INFO_STA:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "VIAWGET_HOSTAPD_GET_INFO_STA \n");
		 ret = hostap_get_info_sta(pDevice, param);
		 ap_ioctl = 1;
		break;
/*
	case VIAWGET_HOSTAPD_RESET_TXEXC_STA:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "VIAWGET_HOSTAPD_RESET_TXEXC_STA \n");
		 ret = hostap_reset_txexc_sta(pDevice, param);
		break;
*/
	case VIAWGET_HOSTAPD_SET_FLAGS_STA:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "VIAWGET_HOSTAPD_SET_FLAGS_STA \n");
		 ret = hostap_set_flags_sta(pDevice, param);
		break;

	case VIAWGET_HOSTAPD_MLME:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "VIAWGET_HOSTAPD_MLME \n");
	    return -EOPNOTSUPP;

	case VIAWGET_HOSTAPD_SET_GENERIC_ELEMENT:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "VIAWGET_HOSTAPD_SET_GENERIC_ELEMENT \n");
		ret = hostap_set_generic_element(pDevice, param);
		break;

	case VIAWGET_HOSTAPD_SCAN_REQ:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "VIAWGET_HOSTAPD_SCAN_REQ \n");
	    return -EOPNOTSUPP;

	case VIAWGET_HOSTAPD_STA_CLEAR_STATS:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "VIAWGET_HOSTAPD_STA_CLEAR_STATS \n");
	    return -EOPNOTSUPP;

	default:
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "vt6656_hostap_ioctl: unknown cmd=%d\n",
		       (int)param->cmd);
		return -EOPNOTSUPP;
		break;
	}


	if ((ret == 0) && ap_ioctl) {
		if (copy_to_user(p->pointer, param, p->length)) {
			ret = -EFAULT;
			goto out;
		}
	}

 out:
	if (param != NULL)
		kfree(param);

	return ret;
}
