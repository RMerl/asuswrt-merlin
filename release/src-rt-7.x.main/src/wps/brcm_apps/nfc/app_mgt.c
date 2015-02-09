/*****************************************************************************
**
**  Name:           app_mgt.c
**
**  Description:    Bluetooth Management function
**
**  Copyright (c) 2011, Broadcom Corp., All Rights Reserved.
**  Broadcom Bluetooth Core. Proprietary and confidential.
**
*****************************************************************************/


#include <stdio.h>
#include <unistd.h>

#include "nsa_api.h"
#include "bsa_trace.h"
#include "app_nsa_utils.h"
#include "app_mgt.h"


/*
 * Local functions
 */
static void app_mgt_generic_callback(tNSA_MGT_EVT event, tNSA_MGT_MSG *p_data);

/*
 * Global variables
 */
tAPP_MGT_CB app_mgt_cb;

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
int app_mgt_init(void)
{
    memset(&app_mgt_cb, 0, sizeof(app_mgt_cb));
    return 0;
}

/*******************************************************************************
 **
 ** Function        app_mgt_open
 **
 ** Description     Open communication to NSA Server
 **
 ** Parameters      uipc_path: path to UIPC channels
 **                 p_mgt_callback: Application's custom callback
 **
 ** Returns         status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_mgt_open(const char *p_uipc_path, tAPP_MGT_CUSTOM_CBACK *p_mgt_callback,
		BOOLEAN bsa_mgt_already_opened)
{
    tNSA_MGT_OPEN nsa_open_param;
    tNSA_STATUS nsa_status;
    int i;

    NSA_MgtOpenInit(&nsa_open_param);

    /* If the application passed a NULL UIPC path, use the default one */
    if (p_uipc_path == NULL)
    {
        strncpy(nsa_open_param.uipc_path, APP_DEFAULT_UIPC_PATH, sizeof(nsa_open_param.uipc_path) - 1);
        nsa_open_param.uipc_path[sizeof(nsa_open_param.uipc_path) - 1] = '\0';
    }
    /* Else, use the given path */
    else
    {
        strncpy(nsa_open_param.uipc_path, p_uipc_path, sizeof(nsa_open_param.uipc_path) - 1);
        nsa_open_param.uipc_path[sizeof(nsa_open_param.uipc_path) - 1] = '\0';
    }

    /* Use the Generic Callback */
    nsa_open_param.callback = app_mgt_generic_callback;


    nsa_open_param.bsa_mgt_already_opened = bsa_mgt_already_opened;

    /* Save the Application's custom callback */
    app_mgt_cb.mgt_custom_cback = p_mgt_callback;

    /* Let's try to connect several time */
    for (i = 0 ; i < 5 ; i++)
    {
        /* Connect to NSA Server */
        nsa_status = NSA_MgtOpen(&nsa_open_param);
        if (nsa_status == NSA_SUCCESS)
        {
            break;
        }
        APP_ERROR1("Connection to server unsuccessful (%d), retrying...", i);
        sleep(1);
    }
    if (nsa_status != NSA_SUCCESS)
    {
        APP_ERROR0("Unable to connect to server");
        return -1;
    }

    return 0;
}


/*******************************************************************************
 **
 ** Function        app_mgt_close
 **
 ** Description     This function is used to closes the NSA connection
 **
 ** Returns         The status of the operation
 **
 ** Returns         status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_mgt_close(void)
{
    tNSA_MGT_CLOSE  nsa_close_param;
    tNSA_STATUS nsa_status;

    NSA_MgtCloseInit(&nsa_close_param);
    nsa_status = NSA_MgtClose(&nsa_close_param);
    if(nsa_status != NSA_SUCCESS)
    {
        APP_ERROR1("NSA_MgtClose failed status:%d", nsa_status);
        return -1;
    }
    return 0;
}

/*******************************************************************************
 **
 ** Function        app_mgt_generic_callback
 **
 ** Description     This is an example of Generic Management callback function.
 **                 The Management Callback function is called in case of server
 **                 disconnection (e.g. server crashes) or when the Bluetooth
 **                 status changes (enable/disable)
 **
 ** Parameters      event: the event received (Status or Disconnect event)
 **                 p_data:associated data
 **
 ** Returns         void
 **
 *******************************************************************************/
static void app_mgt_generic_callback(tNSA_MGT_EVT event, tNSA_MGT_MSG *p_data)
{
    BOOLEAN exit_generic_cback= FALSE;

    /*  If Application provided its own custom callback */
    if(app_mgt_cb.mgt_custom_cback != NULL)
    {
        /* Call it */
        exit_generic_cback = app_mgt_cb.mgt_custom_cback(event, p_data);
    }

    /* If custom callback indicates that does not need the generic callback to execute */
    if(exit_generic_cback != FALSE)
    {
        return;
    }

    switch(event)
    {
    case NSA_MGT_STATUS_EVT:
        APP_INFO0("app_mgt_generic_callback NSA_MGT_STATUS_EVT");
        if (p_data->status.enable == FALSE)
        {
            APP_INFO0("\tBluetooth Stopped");
        }
        else
        {
            /* The FALSE parameter indicates Starts */
            APP_INFO0("\tBluetooth restarted");
        }
        break;

    case NSA_MGT_DISCONNECT_EVT:
        /* Connection with the Server lost => Application will have to reconnect */
        APP_INFO1("app_mgt_generic_callback NSA_MGT_DISCONNECT_EVT reason:%d", p_data->disconnect.reason);
        break;
    }
}

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
int app_mgt_kill_server(void)
{
    tNSA_MGT_KILL_SERVER param;

    if(NSA_MgtKillServerInit(&param) == NSA_SUCCESS)
    {
        if(NSA_MgtKillServer(&param) == NSA_SUCCESS)
        {
            return 0;
        }
    }

    return -1;
}
