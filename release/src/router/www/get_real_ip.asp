<% get_realip(); %>
var wan0_realip_state = "<% nvram_get("wan0_realip_state"); %>";
var wan1_realip_state = "<% nvram_get("wan1_realip_state"); %>";
var wan0_realip_ip = "<% nvram_get("wan0_realip_ip"); %>";
var wan1_realip_ip = "<% nvram_get("wan1_realip_ip"); %>";
var active_wan_unit = "<% get_wan_unit(); %>";
var wan0_ipaddr = "<% nvram_get("wan0_ipaddr"); %>";
var wan1_ipaddr = "<% nvram_get("wan1_ipaddr"); %>";
var realip_state = "";
var realip_ip = "";
if(active_wan_unit == "0"){
	realip_state = wan0_realip_state;
	if(realip_state == "2"){
		realip_ip = wan0_realip_ip;
		external_ip = (realip_ip == wan0_ipaddr)? 1:0;
	}
	else{
		external_ip = -1;
	}
}
else if(active_wan_unit == "1"){
	realip_state = wan1_realip_state;
	if(realip_state == "2"){
		realip_ip = wan1_realip_ip;
		external_ip = (realip_ip == wan1_ipaddr)? 1:0;
	}
	else{
		external_ip = -1;
	}
}