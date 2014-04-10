#ifndef _web_qtn_h_
#define _web_qtn_h_

#include "qcsapi_output.h"
#include "qcsapi_rpc_common/client/find_host_addr.h"

#include "qcsapi.h"
#include "qcsapi_rpc/client/qcsapi_rpc_client.h"
#include "qcsapi_rpc/generated/qcsapi_rpc.h"
#include "qcsapi_driver.h"
#include "call_qcsapi.h"

#define WIFINAME "wifi0"

extern int rpc_qcsapi_init();
extern int qcsapi_init(void);
extern int rpc_qcsapi_restore_default_config(int flag);
extern int rpc_qcsapi_bootcfg_commit(void);
extern void rpc_show_config(void);
extern void rpc_parse_nvram(const char *name, const char *value);
extern int rpc_qcsapi_set_SSID(const char *ssid);
extern int rpc_qcsapi_set_SSID_broadcast(const char *option);
extern int rpc_qcsapi_set_vht(const char *mode);
extern int rpc_qcsapi_set_bw(const char *bw);
extern int rpc_qcsapi_set_channel(const char *chspec_buf);
extern int rpc_qcsapi_set_beacon_type(const char *auth_mode);
extern int rpc_qcsapi_set_WPA_encryption_modes(const char *crypto);
extern int rpc_qcsapi_set_key_passphrase(const char *wpa_psk);
extern int rpc_qcsapi_set_dtim(const char *dtim);
extern int rpc_qcsapi_set_beacon_interval(const char *beacon_interval);
extern int rpc_qcsapi_set_mac_address_filtering(const char *ifname, const char *mac_address_filtering);
extern void rpc_update_wlmaclist(const char *ifname);
extern void rpc_update_wdslist();
extern void rpc_update_wds_psk();
extern int rpc_qcsapi_get_SSID(qcsapi_SSID *ssid);
extern int rpc_qcsapi_get_SSID_broadcast(int *p_current_option);
extern int rpc_qcsapi_get_vht(qcsapi_unsigned_int *vht);
extern int rpc_qcsapi_get_bw(qcsapi_unsigned_int *p_bw);
extern int rpc_qcsapi_get_channel(qcsapi_unsigned_int *p_channel);
extern int rpc_qcsapi_get_channel_list(string_1024* list_of_channels);
extern int rpc_qcsapi_get_beacon_type(char *p_current_beacon);
extern int rpc_qcsapi_get_WPA_encryption_modes(char *p_current_encryption_mode);
extern int rpc_qcsapi_get_key_passphrase(char *p_current_key_passphrase);
extern int rpc_qcsapi_get_dtim(qcsapi_unsigned_int *p_dtim);
extern int rpc_qcsapi_get_beacon_interval(qcsapi_unsigned_int *p_beacon_interval);
extern int rpc_qcsapi_get_mac_address_filtering(const char* ifname, qcsapi_mac_address_filtering *p_mac_address_filtering);
extern int rpc_qcsapi_interface_get_mac_addr(const char *ifname, qcsapi_mac_addr *current_mac_addr);

#endif /* _web_qtn_h_ */

