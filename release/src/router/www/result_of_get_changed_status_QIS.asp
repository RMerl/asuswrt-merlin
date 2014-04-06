<% wanlink(); %>
<% wanstate(); %>
var autodet_state = "<% nvram_get("autodet_state"); %>";
parent.allUsbStatusArray = <% show_usb_path(); %>;
var link_wan_status = "<% nvram_get("link_wan"); %>";
var link_wan1_status = "<% nvram_get("link_wan1"); %>";
