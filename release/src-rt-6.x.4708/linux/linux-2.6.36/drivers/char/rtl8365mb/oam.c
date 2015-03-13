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
 * $Revision: 38851 $
 * $Date: 2013-04-24 13:35:46 +0800 (週三, 24 四月 2013) $
 *
 * Purpose : RTK switch high-level API for RTL8367/RTL8367C
 * Feature : Here is a list of all functions and variables in OAM(802.3ah)  module.
 *
 */

#include <rtk_switch.h>
#include <rtk_error.h>
#include <oam.h>
#ifndef __KERNEL__
#include <string.h>
#else
#include <linux/string.h>
#endif

#include <rtl8367c_asicdrv.h>
#include <rtl8367c_asicdrv_oam.h>


/* Module Name : OAM */

/* Function Name:
 *      rtk_oam_init
 * Description:
 *      Initialize oam module.
 * Input:
 *      None
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 * Note:
 *      Must initialize oam module before calling any oam APIs.
 */
rtk_api_ret_t rtk_oam_init(void)
{
    return RT_ERR_OK;
} /* end of rtk_oam_init */


/* Function Name:
 *      rtk_oam_state_set
 * Description:
 *      This API set OAM state.
 * Input:
 *      enabled     -OAMstate
 * Output:
 *      None.
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - Failed
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT           - Error parameter
 * Note:
 *      This API set OAM state.
 */
rtk_api_ret_t rtk_oam_state_set(rtk_enable_t enabled)
{
    rtk_api_ret_t retVal;

    /* Check initialization state */
    RTK_CHK_INIT_STATE();

    if (enabled >= RTK_ENABLE_END)
        return RT_ERR_INPUT;

    if ((retVal = rtl8367c_setAsicOamEnable(enabled))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}

/* Function Name:
 *      rtk_oam_state_get
 * Description:
 *      This API get OAM state.
 * Input:
 *      None.
 * Output:
 *      pEnabled        - H/W IGMP state
 * Return:
 *      RT_ERR_OK              - OK
 *      RT_ERR_FAILED          - Failed
 *      RT_ERR_SMI             - SMI access error
 *      RT_ERR_INPUT           - Error parameter
 * Note:
 *      This API set current OAM state.
 */
rtk_api_ret_t rtk_oam_state_get(rtk_enable_t *pEnabled)
{
    rtk_api_ret_t retVal;

    /* Check initialization state */
    RTK_CHK_INIT_STATE();

    if ((retVal = rtl8367c_getAsicOamEnable(pEnabled))!=RT_ERR_OK)
        return retVal;

    return RT_ERR_OK;
}



/* Function Name:
 *      rtk_oam_parserAction_set
 * Description:
 *      Set OAM parser action
 * Input:
 *      port    - port id
 *      action  - parser action 
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
rtk_api_ret_t  rtk_oam_parserAction_set(rtk_port_t port, rtk_oam_parser_act_t action)
{
    rtk_api_ret_t retVal;

    /* Check initialization state */
    RTK_CHK_INIT_STATE();

    /* Check Port Valid */
    RTK_CHK_PORT_VALID(port);	

    if (action >= OAM_PARSER_ACTION_END)
        return RT_ERR_INPUT;

    if ((retVal = rtl8367c_setAsicOamParser(rtk_switch_port_L2P_get(port), action))!=RT_ERR_OK)
        return retVal;
	
    return RT_ERR_OK;
} 

/* Function Name:
 *      rtk_oam_parserAction_set
 * Description:
 *      Get OAM parser action
 * Input:
 *      port    - port id
 * Output:
 *      pAction  - parser action 
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
rtk_api_ret_t  rtk_oam_parserAction_get(rtk_port_t port, rtk_oam_parser_act_t *pAction)
{
    rtk_api_ret_t retVal;

    /* Check initialization state */
    RTK_CHK_INIT_STATE();

    /* Check Port Valid */
    RTK_CHK_PORT_VALID(port);	

    if ((retVal = rtl8367c_getAsicOamParser(rtk_switch_port_L2P_get(port), pAction))!=RT_ERR_OK)
        return retVal;
	
    return RT_ERR_OK;
} 


/* Function Name:
 *      rtk_oam_multiplexerAction_set
 * Description:
 *      Set OAM multiplexer action
 * Input:
 *      port    - port id
 *      action  - parser action 
 * Output:
 *      None
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
rtk_api_ret_t  rtk_oam_multiplexerAction_set(rtk_port_t port, rtk_oam_multiplexer_act_t action)
{
    rtk_api_ret_t retVal;

    /* Check initialization state */
    RTK_CHK_INIT_STATE();

    /* Check Port Valid */
    RTK_CHK_PORT_VALID(port);	

    if (action >= OAM_MULTIPLEXER_ACTION_END)
        return RT_ERR_INPUT;

    if ((retVal = rtl8367c_setAsicOamMultiplexer(rtk_switch_port_L2P_get(port), action))!=RT_ERR_OK)
        return retVal;
	
    return RT_ERR_OK;
} 

/* Function Name:
 *      rtk_oam_parserAction_set
 * Description:
 *      Get OAM multiplexer action
 * Input:
 *      port    - port id
 * Output:
 *      pAction  - parser action 
 * Return:
 *      RT_ERR_OK
 *      RT_ERR_FAILED
 *      RT_ERR_PORT_ID      - invalid port id
 * Note:
 *      None
 */
rtk_api_ret_t  rtk_oam_multiplexerAction_get(rtk_port_t port, rtk_oam_multiplexer_act_t *pAction)
{
    rtk_api_ret_t retVal;

    /* Check initialization state */
    RTK_CHK_INIT_STATE();

    /* Check Port Valid */
    RTK_CHK_PORT_VALID(port);	

    if ((retVal = rtl8367c_getAsicOamMultiplexer(rtk_switch_port_L2P_get(port), pAction))!=RT_ERR_OK)
        return retVal;
	
    return RT_ERR_OK;
}


