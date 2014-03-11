<?xml version="1.0" ?>
<devicemap>  <% ajax_wanstate(); %>
  <wan>monoClient=<% nvram_get("mfp_ip_monopoly"); %></wan>
  <wan>wlc_state=<% nvram_get("wlc_state"); %></wan>
  <wan>wlc_sbstate=<% nvram_get("wlc_sbstate"); %></wan>
  <wan>psta:<% wlc_psta_state(); %></wan>
  <wan>wifi_hw_switch=<% nvram_get("wl0_radio"); %></wan>
  <wan>ddnsRet=<% nvram_get("ddns_return_code_chk"); %></wan>
  <wan>ddnsUpdate=<% nvram_get("ddns_updated"); %></wan>
  <wan>wan_line_state=<% nvram_get("dsltmp_adslsyncsts"); %></wan>
  <wan>wlan0_radio_flag=<% nvram_get("wl0_radio"); %></wan>
  <wan>wlan1_radio_flag=<% nvram_get("wl1_radio"); %></wan>
  <wan>data_rate_info_2g=<% wl_rate_2g(); %></wan>  
  <wan>data_rate_info_5g=<% wl_rate_5g(); %></wan>  
  <vpn>vpnc_proto=<% nvram_get("vpnc_proto"); %></vpn>  
  <vpn>vpnc_state_t=<% nvram_get("vpnc_state_t"); %></vpn>  
  <vpn>vpnc_sbstate_t=<% nvram_get("vpnc_sbstate_t"); %></vpn>
  <vpn>vpn_client1_state=<% nvram_get("vpn_client1_state"); %></vpn>  
  <vpn>vpn_client2_state=<% nvram_get("vpn_client2_state"); %></vpn>
  <vpn>vpnd_state=<% nvram_get("VPNServer_enable"); %></vpn>
  <% secondary_ajax_wanstate(); %>
	<usb>'<% show_usb_path(); %>'</usb>
</devicemap>
