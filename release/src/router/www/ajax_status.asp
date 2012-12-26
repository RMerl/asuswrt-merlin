<?xml version="1.0" ?>
<devicemap>
  <% ajax_wanstate(); %>
  <wan>usb=<% nvram_get("usb_path1"); %></wan>
  <wan>usb=<% nvram_get("usb_path2"); %></wan>
  <wan>monoClient=<% nvram_get("mfp_ip_monopoly"); %></wan>
  <wan>cooler=<% nvram_get("cooler"); %></wan>
  <wan>wlc_state=<% nvram_get("wlc_state"); %></wan>
  <wan>wlc_sbstate=<% nvram_get("wlc_sbstate"); %></wan>
  <wan>wifi_hw_switch=<% nvram_get("wl0_radio"); %></wan>
  <wan>psta:<% wlc_psta_state(); %></wan>
  <wan>umount=<% nvram_get("usb_path1_removed"); %></wan>
  <wan>umount=<% nvram_get("usb_path2_removed"); %></wan>
</devicemap>
