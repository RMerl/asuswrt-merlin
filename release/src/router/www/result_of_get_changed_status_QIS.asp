<% wanlink(); %>
<% wanstate(); %>
var autodet_plc_state = "<% nvram_get("autodet_plc_state"); %>";
var wans_dualwan = '<% nvram_get("wans_dualwan"); %>'.split(" ");
if(wans_dualwan != ""){
	var ewan_index = wans_dualwan.indexOf("wan");
	var autodet_state = (ewan_index == 0)? '<% nvram_get("autodet_state"); %>': '<% nvram_get("autodet1_state"); %>';
	var autodet_auxstate = (ewan_index == 0)? '<% nvram_get("autodet_auxstate"); %>': '<% nvram_get("autodet1_auxstate"); %>';
}
else{
	var autodet_state = '<% nvram_get("autodet_state"); %>';
	var autodet_auxstate = '<% nvram_get("autodet_auxstate"); %>';	
}
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
usb_modem_auto_lines = '<% nvram_get("usb_modem_auto_lines"); %>';
usb_modem_auto_running = '<% nvram_get("usb_modem_auto_running"); %>';
usb_modem_auto_spn = '<% nvram_get("usb_modem_auto_spn"); %>';
var link_internet = '<% nvram_get("link_internet"); %>';
var wan0_realip_state = '<% nvram_get("wan0_realip_state"); %>';
var wan1_realip_state = '<% nvram_get("wan1_realip_state"); %>';