<% wanlink(); %>
<% wanstate(); %>
var autodet_state = "<% nvram_get("autodet_state"); %>";
var autodet_auxstate = "<% nvram_get("autodet_auxstate"); %>";
parent.allUsbStatusArray = <% show_usb_path(); %>;
var link_wan_status = "<% nvram_get("link_wan"); %>";
var link_wan1_status = "<% nvram_get("link_wan1"); %>";
var qtn_ready_t = "<% nvram_get("qtn_ready"); %>";
sim_state = "<% nvram_get("usb_modem_act_sim"); %>";
g3err_pin = '<% nvram_get("g3err_pin"); %>';
usb_modem_act_auth = '<% nvram_get("usb_modem_act_auth"); %>';
pin_remaining_count = '<% nvram_get("usb_modem_act_auth_pin"); %>';
puk_remaining_count = '<% nvram_get("usb_modem_act_auth_puk"); %>';
<% dual_wanstate(); %>
usb_modem_auto_running = '<% nvram_get("usb_modem_auto_running"); %>';
usb_modem_auto_isp = '<% nvram_get("usb_modem_auto_isp"); %>';
