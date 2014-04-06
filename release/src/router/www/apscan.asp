child_macaddr = '<% nvram_get("lan_hwaddr"); %>';
_sw_mode = '<% nvram_get("sw_mode"); %>';
wlc_state = '<% nvram_get("wlc_state"); %>';
wlc_sbstate = '<% nvram_get("wlc_sbstate"); %>';
wlc_scan_state = '<% nvram_get("wlc_scan_state"); %>';
_wlc_ssid = '<% nvram_char_to_ascii("", "wlc_ssid"); %>';
aplist = [<% get_ap_info(); %>];