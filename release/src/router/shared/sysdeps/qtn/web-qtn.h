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

extern int rpc_qcsapi_init(int verbose);
extern int rpc_qtn_ready();
extern int qcsapi_init(void);
extern int rpc_qcsapi_restore_default_config(int flag);
extern int rpc_qcsapi_bootcfg_commit(void);
extern void rpc_set_radio(int unit, int subunit, int on);
extern void rpc_show_config(void);
extern void rpc_parse_nvram(const char *name, const char *value);
extern int rpc_qcsapi_set_SSID(const char *ifname, const char *ssid);
extern int rpc_qcsapi_set_SSID_broadcast(const char *ifname, const char *option);
extern int rpc_qcsapi_set_vht(const char *mode);
extern int rpc_qcsapi_set_bw(const char *bw);
extern int rpc_qcsapi_set_channel(const char *chspec_buf);
extern int rpc_qcsapi_set_beacon_type(const char *ifname, const char *auth_mode);
extern int rpc_qcsapi_set_WPA_encryption_modes(const char *ifname, const char *crypto);
extern int rpc_qcsapi_set_key_passphrase(const char *ifname, const char *wpa_psk);
extern int rpc_qcsapi_set_dtim(const char *dtim);
extern int rpc_qcsapi_set_beacon_interval(const char *beacon_interval);
extern int rpc_qcsapi_set_mac_address_filtering(const char *ifname, const char *mac_address_filtering);
extern int rpc_qcsapi_authorize_mac_address(const char *ifname, const char *macaddr);
extern int rpc_qcsapi_deny_mac_address(const char *ifname, const char *macaddr);
extern int rpc_qcsapi_wds_set_psk(const char *ifname, const char *macaddr, const char *wpa_psk);
extern int rpc_qcsapi_set_wlmaclist(const char *ifname);
extern int rpc_update_ap_isolate(const char *ifname, const int isolate);
extern void rpc_update_wlmaclist(void);
extern void rpc_update_wdslist();
extern void rpc_update_wds_psk(const char *wds_psk);
extern void rpc_update_mbss(const char *name, const char *value);
extern int rpc_qcsapi_get_SSID(const char *ifname, qcsapi_SSID *ssid);
extern int rpc_qcsapi_get_SSID_broadcast(const char *ifname, int *p_current_option);
extern int rpc_qcsapi_get_vht(qcsapi_unsigned_int *vht);
extern int rpc_qcsapi_get_bw(qcsapi_unsigned_int *p_bw);
extern int rpc_qcsapi_get_channel(qcsapi_unsigned_int *p_channel);
extern int rpc_qcsapi_get_channel_list(string_1024* list_of_channels);
extern int rpc_qcsapi_get_beacon_type(const char *ifname, char *p_current_beacon);
extern int rpc_qcsapi_get_WPA_encryption_modes(const char *ifname, char *p_current_encryption_mode);
extern int rpc_qcsapi_get_key_passphrase(const char *ifname, char *p_current_key_passphrase);
extern int rpc_qcsapi_get_dtim(qcsapi_unsigned_int *p_dtim);
extern int rpc_qcsapi_get_beacon_interval(qcsapi_unsigned_int *p_beacon_interval);
extern int rpc_qcsapi_get_mac_address_filtering(const char* ifname, qcsapi_mac_address_filtering *p_mac_address_filtering);
extern int rpc_qcsapi_wps_get_state(const char *ifname, char *wps_state, const qcsapi_unsigned_int max_len);
extern int rpc_qcsapi_wifi_disable_wps(const char *ifname, int disable_wps);
extern int rpc_qcsapi_wps_get_ap_pin(const char *ifname, char *wps_pin, int force_regenerate);
extern int rpc_qcsapi_wps_cancel(const char *ifname);
extern int rpc_qcsapi_wps_registrar_report_button_press(const char *ifname);
extern int rpc_qcsapi_wps_registrar_report_pin(const char *ifname, const char *wps_pin);
extern int rpc_qcsapi_interface_get_mac_addr(const char *ifname, qcsapi_mac_addr *current_mac_addr);
extern char *getWscStatusStr_qtn();
extern char *get_WPSConfiguredStr_qtn();
extern char *getWPSAuthMode_qtn();
extern char *getWPSEncrypType_qtn();

#endif /* _web_qtn_h_ */

