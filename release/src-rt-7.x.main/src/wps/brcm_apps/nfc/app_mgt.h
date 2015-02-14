/*****************************************************************************
**
**  Name:           app_mgt.h
**
**  Description:    Bluetooth Management function
**
**  Copyright (c) 2011, Broadcom Corp., All Rights Reserved.
**  Broadcom Bluetooth Core. Proprietary and confidential.
**
*****************************************************************************/

#ifndef APP_MGT_H
#define APP_MGT_H

/*
 * Definitions
 */
#ifndef APP_DEFAULT_UIPC_PATH
#define APP_DEFAULT_UIPC_PATH "./"
#endif

/* Management Custom Callback */
typedef BOOLEAN (tAPP_MGT_CUSTOM_CBACK)(tNSA_MGT_EVT event, tNSA_MGT_MSG *p_data);


typedef struct
{
    tAPP_MGT_CUSTOM_CBACK* mgt_custom_cback;
} tAPP_MGT_CB;

extern tAPP_MGT_CB app_mgt_cb;


/*******************************************************************************
 **
 ** Function        app_mgt_init
 **
 ** Description     Init management
 **
 ** Parameters      None
 **
 ** Returns         status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_mgt_init(void);

/*******************************************************************************
 **
 ** Function        app_mgt_open
 **
 ** Description     Open communication to NSA Server
 **
 ** Parameters      uipc_path: path to UIPC channels
 **                 p_mgt_callback: Application's custom callback
 **                 bsa mgt already opened so GKI and UIPC already init
 **
 ** Returns         status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_mgt_open(const char *p_uipc_path, tAPP_MGT_CUSTOM_CBACK *p_mgt_callback,
		BOOLEAN bsa_mgt_already_opened);

/*******************************************************************************
 **
 ** Function        app_mgt_close
 **
 ** Description     This function is used to closes the NSA connection
 **
 ** Parameters      None
 **
 ** Returns         status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_mgt_close(void);

/*******************************************************************************
 **
 ** Function         app_mgt_kill_server
 **
 ** Description      This function is used to kill the server
 **
 ** Parameters       None
 **
 ** Returns          status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_mgt_kill_server(void);
#endif /* APP_MGT_H_ */
