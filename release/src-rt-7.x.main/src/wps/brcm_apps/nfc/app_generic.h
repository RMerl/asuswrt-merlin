/*****************************************************************************
 **
 **  Name:           app_generic.h
 **
 **  Description:    NSA generic application
 **
 **  Copyright (c) 2012, Broadcom Corp., All Rights Reserved.
 **  Broadcom Bluetooth Core. Proprietary and confidential.
 **
 *****************************************************************************/

#ifndef APP_GENERIC_H
#define APP_GENERIC_H

#include "data_types.h"

/*******************************************************************************
 **
 ** Function         app_nsa_init
 **
 ** Description      Init NSA application
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_gen_init(void);

/*******************************************************************************
 **
 ** Function         app_nsa_end
 **
 ** Description      This function is used to close application
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_nsa_end(void);

/*******************************************************************************
 **
 ** Function         app_nsa_dm_enable
 **
 ** Description      Enable NSA
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_enable(void);

/*******************************************************************************
 **
 ** Function         app_nsa_dm_disable
 **
 ** Description      Disable NSA
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_disable(void);

/*******************************************************************************
 **
 ** Function         app_nsa_dm_startRFdiscov
 **
 ** Description      NSA starts RF discovery
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_startRFdiscov(void);

/*******************************************************************************
 **
 ** Function         app_nsa_dm_stopRFdiscov
 **
 ** Description      NSA starts RF discovery
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_stopRFdiscov(void);

/*******************************************************************************
 **
 ** Function         app_nsa_dm_startSbeam
 **
 ** Description      NSA starts SBEAM to accept incoming Sbeam procedure from a peer
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_startSbeam(void);

/*******************************************************************************
 **
 ** Function         app_nsa_dm_stopSbeam
 **
 ** Description      NSA starts SBEAM to accept incoming Sbeam procedure from a peer
 **
 ** Returns          0 if successful, error code otherwise
 **
 *******************************************************************************/
int app_nsa_dm_stopSbeam(void);

/*NSA p2p command*/
int app_nsa_p2p_start_server(void);
int app_nsa_p2p_stop_server(void);
int app_nsa_p2p_start_client(void);
int app_nsa_p2p_stop_client(void);
int app_nsa_p2p_config_client_test(void);
int app_nsa_p2p_config_server_test(void);

/*NSA SNEP command*/
int app_nsa_snep_start_server(void);
int app_nsa_snep_stop_server(void);
int app_nsa_snep_start_client(void);
int app_nsa_snep_config_test(void);
int app_nsa_snep_stop_client(void);
void app_nsa_snep_client_callback(tNSA_SNEP_EVT event, tNSA_SNEP_EVT_DATA *p_data);
void app_nsa_snep_server_callback(tNSA_SNEP_EVT event, tNSA_SNEP_EVT_DATA *p_data);

/*NSA RW command*/
int app_nsa_rw_write_text(void);
int app_nsa_rw_write_sp(void);
int app_nsa_rw_write_bt(void);
int app_nsa_rw_write_wps(void);
/* Kenlo + */
int app_nsa_dm_enable_wps(void);
int app_nsa_rw_format_wps(void);
int app_nsa_rw_read_wps(void);
int app_nsa_rw_write_wps_with_payload(UINT8 *payload, UINT32 payload_size);
/* Kenlo - */
int app_nsa_rw_read(void);
int app_nsa_rw_format(void);
int app_nsa_rw_set_readOnly(void);
int app_nsa_rw_stop(void);

/*NSA CHO command*/
int app_nsa_cho_start(void);
int app_nsa_cho_stop(void);
int app_nsa_cho_send(void);
int app_nsa_cho_cancel(void);
/* Kenlo + */
int app_nsa_cho_start_wps_with_payload(BOOLEAN cho_as_server, UINT8 *payload, UINT32 payload_size);
int app_nsa_cho_send_wps(void);
/* Kenlo - */

/*NSA RF discovery only command*/
int app_nsa_dm_stopRF(void);
int app_nsa_dm_startRF(void);

/*Raw Frame to tag using exclusive RF control*/
int app_nsa_tags_start_t4t(void);
int app_nsa_tags_stop_t4t(void);

#endif /* APP_GENERIC_H */
