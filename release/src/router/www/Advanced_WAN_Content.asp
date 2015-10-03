<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#menu5_3_1#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<style>
.apply_gen_wan{
 	text-align:center;
 	background-color:#4D595D;
 	width:99%;
 	margin-top:10px;
	border-radius: 0 0 3px 3px;
	-moz-border-radius-bottomright: 3px;
	-moz-border-radius-bottomleft: 3px;
	behavior: url(/PIE.htc);
	border-radius: 0 0 3px 3px;
}
.FormTable{
 	margin-top:10px;	
}
</style>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script>

var wans_dualwan = '<% nvram_get("wans_dualwan"); %>';
var nowWAN = '<% get_parameter("flag"); %>';

if(dualWAN_support && ( wans_dualwan.search("wan") >= 0 || wans_dualwan.search("lan") >= 0)){
	var wan_type_name = wans_dualwan.split(" ")[<% nvram_get("wan_unit"); %>].toUpperCase();
	switch(wan_type_name){
		case "DSL":
			location.href = "Advanced_DSL_Content.asp";
			break;
		case "USB":
			if(based_modelid == "4G-AC55U")
				location.href = "Advanced_MobileBroadband_Content.asp";
			else
				location.href = "Advanced_Modem_Content.asp";
			break;
		default:
			break;	
	}
}
<% login_state_hook(); %>
<% wan_get_parameter(); %>

var wan_proto_orig = '<% nvram_get("wan_proto"); %>';
var original_wan_type = wan_proto_orig;
var original_wan_dhcpenable = parseInt('<% nvram_get("wan_dhcpenable_x"); %>');
var original_dnsenable = parseInt('<% nvram_get("wan_dnsenable_x"); %>');
var wan_unit_flag = '<% nvram_get("wan_unit"); %>';

if(yadns_support){
	var yadns_enable = '<% nvram_get("yadns_enable_x"); %>';
	var yadns_mode = '<% nvram_get("yadns_mode"); %>';
}

var pppoe_username = decodeURIComponent('<% nvram_char_to_ascii("", "wan_pppoe_username"); %>');
var pppoe_password = decodeURIComponent('<% nvram_char_to_ascii("", "wan_pppoe_passwd"); %>');

function initial(){
	if(!dualWAN_support){
		if(wan_unit_flag == 1){	
			document.wanUnit_form.wan_unit.value = 0;
			document.wanUnit_form.target = "";
			document.wanUnit_form.submit();
		}
	}
	
	show_menu();			
	change_wan_type(document.form.wan_proto.value, 0);	
	fixed_change_wan_type(document.form.wan_proto.value);
	genWANSoption();
	//change_wan_unit(document.form.wan_unit);
	change_wan_type(document.form.wan_proto.value, 0);	

	var wan_type = document.form.wan_proto.value;
	if(wan_type == "pppoe" || wan_type == "pptp" || wan_type == "l2tp" ||
	   document.form.wan_auth_x.value != ""){
		document.form.wan_pppoe_username.value = pppoe_username;
		document.form.wan_pppoe_passwd.value = pppoe_password;
	}

	if(yadns_support){
		if(yadns_enable != 0 && yadns_mode != -1){
			document.getElementById("yadns_hint").style.display = "";
			document.getElementById("yadns_hint").innerHTML = "<span><#YandexDNS_settings_hint#></span>";
		}
	}
	display_upnp_options();

	addOnlineHelp(document.getElementById("faq"), ["UPnP"]);

	if(gobi_support){
		document.getElementById("page_title").innerHTML = "<#WAN_page_desc#>";
		document.getElementById("wan_inf_th").innerHTML = "<#WAN_Interface_Title#>";
	}

}

function display_upnp_options(){
	document.getElementById("upnp_range_int").style.display = (document.form.wan_upnp_enable[0].checked) ? "" : "none";
	document.getElementById("upnp_range_ext").style.display = (document.form.wan_upnp_enable[0].checked) ? "" : "none";
	if (igd2_support) {
		document.getElementById("upnp_pinhole").style.display = (document.form.wan_upnp_enable[0].checked) ? "" : "none";
	} else {
		document.getElementById("upnp_pinhole").style.display = "none";
	}
}

var dsltmp_transmode = "<% nvram_get("dsltmp_transmode"); %>";
function change_wan_unit(obj){
	if(!dualWAN_support) return;
	
	if(obj.options[obj.selectedIndex].text == "DSL"){	
		if(dsltmp_transmode == "atm")
			document.form.current_page.value = "Advanced_DSL_Content.asp";
		else //ptm
			document.form.current_page.value = "Advanced_VDSL_Content.asp";	
	}else if(document.form.dsltmp_transmode){
		document.form.dsltmp_transmode.style.display = "none";
	}
	
	if(obj.options[obj.selectedIndex].text == "USB") {
		document.form.current_page.value = "Advanced_Modem_Content.asp";
	}else if(obj.options[obj.selectedIndex].text == "WAN" 
			|| obj.options[obj.selectedIndex].text == "WAN2"
			|| obj.options[obj.selectedIndex].text == "Ethernet LAN"
			|| obj.options[obj.selectedIndex].text == "Ethernet WAN"){
		if((wans_dualwan == "wan lan" || wans_dualwan == "lan wan")
				 || (wans_dualwan == "wan2 wan" || wans_dualwan == "wan wan2")){
			if(obj.selectedIndex != wan_unit_flag){
				document.form.wan_unit.value = obj.selectedIndex;
			}	
			else{ 
				return false;
			}
		}
		else{
			return false;
		}			
	}
	else if(obj.options[obj.selectedIndex].text == "<#Mobile_title#>"){
		document.form.current_page.value = "Advanced_MobileBroadband_Content.asp";
	}

	FormActions("apply.cgi", "change_wan_unit", "", "");
	document.form.target = "";
	document.form.submit();
}

function genWANSoption(){
	for(i=0; i<wans_dualwan.split(" ").length; i++){
		var wans_dualwan_NAME = wans_dualwan.split(" ")[i].toUpperCase();
        //MODELDEP: DSL-N55U, DSL-N55U-B, DSL-AC68U, DSL-AC68R
        if(wans_dualwan_NAME == "LAN" && 
          (productid == "DSL-N55U" || productid == "DSL-N55U-B" || productid == "DSL-AC68U" || productid == "DSL-AC68R")) 
        	wans_dualwan_NAME = "Ethernet WAN";
		else if(wans_dualwan_NAME == "LAN")
        	wans_dualwan_NAME = "Ethernet LAN";		
		else if(wans_dualwan_NAME == "USB" && based_modelid == "4G-AC55U")
			wans_dualwan_NAME = "<#Mobile_title#>";                       
		document.form.wan_unit.options[i] = new Option(wans_dualwan_NAME, i);
	}	
	
	document.form.wan_unit.selectedIndex = '<% nvram_get("wan_unit"); %>';
	if(wans_dualwan.search(" ") < 0 || wans_dualwan.split(" ")[1] == 'none' || !dualWAN_support)
		document.getElementById("WANscap").style.display = "none";
}

function applyRule(){
	if(ctf.dhcpToPppoe() && ctf.getLevel() == 2){
		if(confirm("Level 2 CTF can not be supported under PPPoE、PPTP or L2TP. If you want to switch to Level 1 CTF, please click confirm.")){
			document.form.ctf_disable_force.value = 0;
			document.form.ctf_fa_mode.value = 0;	
			FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
		}
		else{				
			return false;
		}
	}

	if(validForm()){
		showLoading();
		inputCtrl(document.form.wan_dhcpenable_x[0], 1);
		inputCtrl(document.form.wan_dhcpenable_x[1], 1);
		if(!document.form.wan_dhcpenable_x[0].checked){
			inputCtrl(document.form.wan_ipaddr_x, 1);
			inputCtrl(document.form.wan_netmask_x, 1);
			inputCtrl(document.form.wan_gateway_x, 1);
		}
		
		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);
		if(!document.form.wan_dnsenable_x[0].checked){
			inputCtrl(document.form.wan_dns1_x, 1);
			inputCtrl(document.form.wan_dns2_x, 1);
		}

		document.form.submit();	
	}

}

var ctf = {
	disable_force: '<% nvram_get("ctf_disable"); %>',
	fa_mode: '<% nvram_get("ctf_fa_mode"); %>',

	dhcpToPppoe: function(){
		if((document.form.wan_proto.value == 'dhcp' || document.form.wan_proto.value == 'static') && 
		   (wan_proto_orig == "pppoe" || wan_proto_orig == "pptp" || wan_proto_orig == "l2tp"))
			return false;
		else if((document.form.wan_proto.value == "pppoe" || document.form.wan_proto.value == "pptp" || document.form.wan_proto.value == "l2tp") && 
		   (wan_proto_orig == 'dhcp' || wan_proto_orig == 'static'))
			return true;

		return false;
	},

	getLevel: function(){
		var curVal;

		if(ctf.disable_force == '0' && ctf.fa_mode == '0')
			curVal = 1;
		else if(ctf.disable_force == '0' && ctf.fa_mode == '2')
			curVal = 2;
		else
			curVal = 0;		

		return curVal;
	}
}

// test if WAN IP & Gateway & DNS IP is a valid IP
// DNS IP allows to input nothing
function valid_IP(obj_name, obj_flag){
		// A : 1.0.0.0~126.255.255.255
		// B : 127.0.0.0~127.255.255.255 (forbidden)
		// C : 128.0.0.0~255.255.255.254
		var A_class_start = inet_network("1.0.0.0");
		var A_class_end = inet_network("126.255.255.255");
		var B_class_start = inet_network("127.0.0.0");
		var B_class_end = inet_network("127.255.255.255");
		var C_class_start = inet_network("128.0.0.0");
		var C_class_end = inet_network("255.255.255.255");
		
		var ip_obj = obj_name;
		var ip_num = inet_network(ip_obj.value);

		if(obj_flag == "DNS" && ip_num == -1){ //DNS allows to input nothing
			return true;
		}
		
		if(obj_flag == "GW" && ip_num == -1){ //GW allows to input nothing
			return true;
		}
		
		if(ip_num > A_class_start && ip_num < A_class_end){
		   obj_name.value = ipFilterZero(ip_obj.value);
			return true;
		}
		else if(ip_num > B_class_start && ip_num < B_class_end){
			alert(ip_obj.value+" <#JS_validip#>");
			ip_obj.focus();
			ip_obj.select();
			return false;
		}
		else if(ip_num > C_class_start && ip_num < C_class_end){
			obj_name.value = ipFilterZero(ip_obj.value);
			return true;
		}
		else{
			alert(ip_obj.value+" <#JS_validip#>");
			ip_obj.focus();
			ip_obj.select();
			return false;
		}	
}

function validForm(){
	var wan_type = document.form.wan_proto.value;

	if(!document.form.wan_dhcpenable_x[0].checked){// Set IP address by userself
		if(!valid_IP(document.form.wan_ipaddr_x, "")) return false;  //WAN IP
		if(!valid_IP(document.form.wan_gateway_x, "GW"))return false;  //Gateway IP		

		if(document.form.wan_gateway_x.value == document.form.wan_ipaddr_x.value){
			document.form.wan_ipaddr_x.focus();
			alert("<#IPConnection_warning_WANIPEQUALGatewayIP#>");
			return false;
		}
		
		// test if netmask is valid.
		var default_netmask = "";
		var wrong_netmask = 0;
		var netmask_obj = document.form.wan_netmask_x;
		var netmask_num = inet_network(netmask_obj.value);
		
		if(netmask_num==0){
			var netmask_reverse_num = 0;		//Viz 2011.07 : Let netmask 0.0.0.0 pass
		}else{
		var netmask_reverse_num = ~netmask_num;
		}
		
		if(netmask_num < 0) wrong_netmask = 1;

		var test_num = netmask_reverse_num;
		while(test_num != 0){
			if((test_num+1)%2 == 0)
				test_num = (test_num+1)/2-1;
			else{
				wrong_netmask = 1;
				break;
			}
		}
		if(wrong_netmask == 1){
			alert(netmask_obj.value+" <#JS_validip#>");
			netmask_obj.value = default_netmask;
			netmask_obj.focus();
			netmask_obj.select();
			return false;
		}
	}
	
	if(document.form.wan_dnsenable_x[1].checked == true && document.form.wan_proto.value != "dhcp" && document.form.wan_dns1_x.value == "" && document.form.wan_dns2_x.value == ""){
		document.form.wan_dns1_x.focus();
		alert("<#IPConnection_x_DNSServer_blank#>");
		return false;
	}
	
	if(!document.form.wan_dnsenable_x[0].checked){
		if(!valid_IP(document.form.wan_dns1_x, "DNS")) return false;  //DNS1
		if(!valid_IP(document.form.wan_dns2_x, "DNS")) return false;  //DNS2
	}
	
	if(wan_type == "pppoe" || wan_type == "pptp" || wan_type == "l2tp" ||
	   document.form.wan_auth_x.value != ""){
	   	
		if(document.form.wan_pppoe_username.value.length <= 0){
			alert("<#WANJS9Text#>");
			document.form.wan_pppoe_username.focus();
			return false;
		}
		if(!validator.string(document.form.wan_pppoe_username)){
			document.form.wan_pppoe_username.focus();
			return false;
		}
		if(document.form.wan_pppoe_passwd.value.length <= 0){
			alert("<#WANJS9Text#>");
			document.form.wan_pppoe_passwd.focus();
			return false;
		}
		if(!validator.string(document.form.wan_pppoe_passwd)){
			document.form.wan_pppoe_passwd.focus();
			return false;
		}
	}
	
	if(wan_type == "pppoe" || wan_type == "pptp" || wan_type == "l2tp"){
		if(!validator.numberRange(document.form.wan_pppoe_idletime, 0, 4294967295))
			return false;
	}
	
	if(wan_type == "pppoe"){
		if(!validator.numberRange(document.form.wan_pppoe_mtu, 576, 1500)
				|| !validator.numberRange(document.form.wan_pppoe_mru, 576, 1500))
			return false;
		
		if(!validator.string(document.form.wan_pppoe_service)
				|| !validator.string(document.form.wan_pppoe_ac))
			return false;
	}

        if((document.form.wan_proto.value == "dhcp")
		|| (document.form.wan_proto.value == "static")){
			if(!validator.numberRange(document.form.wan_mtu, 576, 9000))
				return false;
	}
	
	if(document.form.wan_hostname.value.length > 0){
		var alert_str = validator.hostName(document.form.wan_hostname);
	
		if(alert_str != ""){
			showtext(document.getElementById("alert_msg1"), alert_str);
			document.getElementById("alert_msg1").style.display = "";
			document.form.wan_hostname.focus();
			document.form.wan_hostname.select();
			return false;
		}else{
			document.getElementById("alert_msg1").style.display = "none";
  	}

		document.form.wan_hostname.value = trim(document.form.wan_hostname.value);
	}	
	
	if(document.form.wan_hwaddr_x.value.length > 0)
			if(!check_macaddr(document.form.wan_hwaddr_x,check_hwaddr_flag(document.form.wan_hwaddr_x))){
					document.form.wan_hwaddr_x.select();
					document.form.wan_hwaddr_x.focus();
		 	return false;
			}		 	

	if (document.form.wan_upnp_enable[0].checked) {
		if((!validator.numberRange(document.form.upnp_min_port_int, 1, 65535))
			|| (!validator.numberRange(document.form.upnp_max_port_int, 1, 65535))
			|| (!validator.numberRange(document.form.upnp_min_port_ext, 1, 65535))
			|| (!validator.numberRange(document.form.upnp_max_port_ext, 1, 65535))) {
				return false;
		}
		if((parseInt(document.form.upnp_max_port_int.value) < parseInt(document.form.upnp_min_port_int.value))
			|| (parseInt(document.form.upnp_max_port_ext.value) < parseInt(document.form.upnp_min_port_ext.value))) {
				alert("Invalid UPNP ports!  First port must be lower than last port value.");
	                        return false;
		}
	}
	
	if(document.form.wan_heartbeat_x.value.length > 0)
		 if(!validator.string(document.form.wan_heartbeat_x))
		 	return false;
	return true;
}

function done_validating(action){
	refreshpage();
}

function change_wan_type(wan_type, flag){
	if(typeof(flag) != "undefined")
		change_wan_dhcp_enable(flag);
	else
		change_wan_dhcp_enable(1);
	
	if(wan_type == "pppoe"){
		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);
		
		inputCtrl(document.form.wan_auth_x, 0);
		inputCtrl(document.form.wan_pppoe_username, 1);
		inputCtrl(document.form.wan_pppoe_passwd, 1);
		inputCtrl(document.form.wan_pppoe_idletime, 1);
		inputCtrl(document.form.wan_pppoe_idletime_check, 1);
		inputCtrl(document.form.wan_pppoe_mtu, 1);
		inputCtrl(document.form.wan_pppoe_mru, 1);
		inputCtrl(document.form.wan_pppoe_service, 1);
		inputCtrl(document.form.wan_pppoe_ac, 1);
		inputCtrl(document.form.dhcpc_mode, 0);
		inputCtrl(document.form.wan_mtu, 0);
		
		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.wan_pppoe_options_x, 1);
		inputCtrl(document.form.wan_pptp_options_x, 0);
		// 2008.03 James. patch for Oleg's patch. }
		inputCtrl(document.form.wan_heartbeat_x, 0);
		document.getElementById("vpn_dhcp").style.display = "";
		if(based_modelid.toUpperCase().substr(0,3) != "DSL"){
			inputCtrl(document.form.wan_ppp_echo[0], 1);
			inputCtrl(document.form.wan_ppp_echo[1], 1);
		}
	}
	else if(wan_type == "pptp"){
		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);
		
		inputCtrl(document.form.wan_auth_x, 0);
		inputCtrl(document.form.wan_pppoe_username, 1);
		inputCtrl(document.form.wan_pppoe_passwd, 1);
		inputCtrl(document.form.wan_pppoe_idletime, 1);
		inputCtrl(document.form.wan_pppoe_idletime_check, 1);
		inputCtrl(document.form.wan_pppoe_mtu, 0);
		inputCtrl(document.form.wan_pppoe_mru, 0);
		inputCtrl(document.form.wan_pppoe_service, 0);
		inputCtrl(document.form.wan_pppoe_ac, 0);
		inputCtrl(document.form.dhcpc_mode, 0);
		inputCtrl(document.form.wan_mtu, 0);
		
		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.wan_pppoe_options_x, 1);
		inputCtrl(document.form.wan_pptp_options_x, 1);
		// 2008.03 James. patch for Oleg's patch. }
		inputCtrl(document.form.wan_heartbeat_x, 1);
		document.getElementById("vpn_dhcp").style.display = "none";
		inputCtrl(document.form.wan_ppp_echo[0], 0);
		inputCtrl(document.form.wan_ppp_echo[1], 0);
	}
	else if(wan_type == "l2tp"){
		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);
		
		inputCtrl(document.form.wan_auth_x, 0);
		inputCtrl(document.form.wan_pppoe_username, 1);
		inputCtrl(document.form.wan_pppoe_passwd, 1);
		inputCtrl(document.form.wan_pppoe_idletime, 0);
		inputCtrl(document.form.wan_pppoe_idletime_check, 0);
		inputCtrl(document.form.wan_pppoe_mtu, 0);
		inputCtrl(document.form.wan_pppoe_mru, 0);
		inputCtrl(document.form.wan_pppoe_service, 0);
		inputCtrl(document.form.wan_pppoe_ac, 0);
		inputCtrl(document.form.dhcpc_mode, 0);
		inputCtrl(document.form.wan_mtu, 0);
		
		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.wan_pppoe_options_x, 1);
		inputCtrl(document.form.wan_pptp_options_x, 0);
		// 2008.03 James. patch for Oleg's patch. }
		inputCtrl(document.form.wan_heartbeat_x, 1);
		document.getElementById("vpn_dhcp").style.display = "none";
		inputCtrl(document.form.wan_ppp_echo[0], 0);
		inputCtrl(document.form.wan_ppp_echo[1], 0);
	}
	else if(wan_type == "static"){
		inputCtrl(document.form.wan_dnsenable_x[0], 0);
		inputCtrl(document.form.wan_dnsenable_x[1], 0);
		
		inputCtrl(document.form.wan_auth_x, 1);
		inputCtrl(document.form.wan_pppoe_username, (document.form.wan_auth_x.value != ""));
		inputCtrl(document.form.wan_pppoe_passwd, (document.form.wan_auth_x.value != ""));
		inputCtrl(document.form.wan_pppoe_idletime, 0);
		inputCtrl(document.form.wan_pppoe_idletime_check, 0);
		inputCtrl(document.form.wan_pppoe_mtu, 0);
		inputCtrl(document.form.wan_pppoe_mru, 0);
		inputCtrl(document.form.wan_pppoe_service, 0);
		inputCtrl(document.form.wan_pppoe_ac, 0);
		inputCtrl(document.form.dhcpc_mode, 0);
		inputCtrl(document.form.wan_mtu, 1);
		
		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.wan_pppoe_options_x, 0);
		inputCtrl(document.form.wan_pptp_options_x, 0);
		// 2008.03 James. patch for Oleg's patch. }
		inputCtrl(document.form.wan_heartbeat_x, 0);
		document.getElementById("vpn_dhcp").style.display = "none";
		inputCtrl(document.form.wan_ppp_echo[0], 0);
		inputCtrl(document.form.wan_ppp_echo[1], 0);
	}
	else{	// Automatic IP or 802.11 MD or ""		
		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);
		
		inputCtrl(document.form.wan_auth_x, 1);	
		inputCtrl(document.form.wan_pppoe_username, (document.form.wan_auth_x.value != ""));
		inputCtrl(document.form.wan_pppoe_passwd, (document.form.wan_auth_x.value != ""));
		inputCtrl(document.form.wan_pppoe_idletime, 0);
		inputCtrl(document.form.wan_pppoe_idletime_check, 0);
		inputCtrl(document.form.wan_pppoe_mtu, 0);
		inputCtrl(document.form.wan_pppoe_mru, 0);
		inputCtrl(document.form.wan_pppoe_service, 0);
		inputCtrl(document.form.wan_pppoe_ac, 0);
		inputCtrl(document.form.dhcpc_mode, 1);
		inputCtrl(document.form.wan_mtu, 1);
		
		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.wan_pppoe_options_x, 0);
		inputCtrl(document.form.wan_pptp_options_x, 0);
		// 2008.03 James. patch for Oleg's patch. }
		inputCtrl(document.form.wan_heartbeat_x, 0);
		document.getElementById("vpn_dhcp").style.display = "none";
		inputCtrl(document.form.wan_ppp_echo[0], 0);
		inputCtrl(document.form.wan_ppp_echo[1], 0);
	}
}

function fixed_change_wan_type(wan_type){
	var flag = false;
	
	if(!document.form.wan_dhcpenable_x[0].checked){
		if(document.form.wan_ipaddr_x.value.length == 0)
			document.form.wan_ipaddr_x.focus();
		else if(document.form.wan_netmask_x.value.length == 0)
			document.form.wan_netmask_x.focus();
		else if(document.form.wan_gateway_x.value.length == 0)
			document.form.wan_gateway_x.focus();
		else
			flag = true;
	}else
		flag = true;
	
	if(wan_type == "pppoe"){
		if(wan_type == original_wan_type){
			document.form.wan_dnsenable_x[0].checked = original_dnsenable;
			document.form.wan_dnsenable_x[1].checked = !original_dnsenable;
			change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', original_dnsenable);
		}
		else{
			document.form.wan_dnsenable_x[0].checked = 1;
			document.form.wan_dnsenable_x[1].checked = 0;
			change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', 1);	
		}		
	}else if(wan_type == "pptp"	|| wan_type == "l2tp"){
		
		if(wan_type == original_wan_type){
			document.form.wan_dnsenable_x[0].checked = original_dnsenable;
			document.form.wan_dnsenable_x[1].checked = !original_dnsenable;
			change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', original_dnsenable);
		}
		else{
			document.form.wan_dnsenable_x[0].checked = 0;
			document.form.wan_dnsenable_x[1].checked = 1;
			change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', 0);
			
			inputCtrl(document.form.wan_dnsenable_x[0], 1);
			inputCtrl(document.form.wan_dnsenable_x[1], 1);
		}
	}
	else if(wan_type == "static"){
		document.form.wan_dnsenable_x[0].checked = 0;
		document.form.wan_dnsenable_x[1].checked = 1;
		document.form.wan_dnsenable_x[0].disabled = true;
		change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', 0);
	}
	else{	// wan_type == "dhcp"
		
		if(wan_type == original_wan_type){
			document.form.wan_dnsenable_x[0].checked = original_dnsenable;
			document.form.wan_dnsenable_x[1].checked = !original_dnsenable;
			change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', original_dnsenable);
		}
		else{
			document.form.wan_dnsenable_x[0].checked = 1;
			document.form.wan_dnsenable_x[1].checked = 0;
			change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', 1);
		}
	}
	
	if(wan_type != "static"){  // disable DNS server "Yes" when choosing static IP, Jieming add at 2012/12/18
		if(document.form.wan_dhcpenable_x[0].checked){
			inputCtrl(document.form.wan_dnsenable_x[0], 1);
			inputCtrl(document.form.wan_dnsenable_x[1], 1);
		}
		else{		//wan_dhcpenable_x NO
			document.form.wan_dnsenable_x[0].checked = 0;
			document.form.wan_dnsenable_x[1].checked = 1;
			
			inputCtrl(document.form.wan_dnsenable_x[0], 1);
			inputCtrl(document.form.wan_dnsenable_x[1], 1);
			document.form.wan_dnsenable_x[0].disabled = true;
		}
	}	
}

function change_wan_dhcp_enable(flag){
	var wan_type = document.form.wan_proto.value;
	
	// 2008.03 James. patch for Oleg's patch. {
	if(wan_type == "pppoe"){
		if(flag == 1){
			if(wan_type == original_wan_type){
				document.form.wan_dhcpenable_x[0].checked = original_wan_dhcpenable;
				document.form.wan_dhcpenable_x[1].checked = !original_wan_dhcpenable;
			}
			else{
				document.form.wan_dhcpenable_x[0].checked = 1;
				document.form.wan_dhcpenable_x[1].checked = 0;
			}
		}
		
		document.getElementById('IPsetting').style.display = "";
		inputCtrl(document.form.wan_dhcpenable_x[0], 1);
		inputCtrl(document.form.wan_dhcpenable_x[1], 1);
		
		var wan_dhcpenable = document.form.wan_dhcpenable_x[0].checked;
		
		inputCtrl(document.form.wan_ipaddr_x, !wan_dhcpenable);
		inputCtrl(document.form.wan_netmask_x, !wan_dhcpenable);
		inputCtrl(document.form.wan_gateway_x, !wan_dhcpenable);
	}
	// 2008.03 James. patch for Oleg's patch. }
	else if(wan_type == "pptp"
			|| wan_type == "l2tp"
			){
		if(flag == 1){
			if(wan_type == original_wan_type){
				document.form.wan_dhcpenable_x[0].checked = original_wan_dhcpenable;
				document.form.wan_dhcpenable_x[1].checked = !original_wan_dhcpenable;
			}
			else{
				document.form.wan_dhcpenable_x[0].checked = 0;
				document.form.wan_dhcpenable_x[1].checked = 1;
			}
		}
		
		document.getElementById('IPsetting').style.display = "";
		inputCtrl(document.form.wan_dhcpenable_x[0], 1);
		inputCtrl(document.form.wan_dhcpenable_x[1], 1);
		
		var wan_dhcpenable = document.form.wan_dhcpenable_x[0].checked;
		
		inputCtrl(document.form.wan_ipaddr_x, !wan_dhcpenable);
		inputCtrl(document.form.wan_netmask_x, !wan_dhcpenable);
		inputCtrl(document.form.wan_gateway_x, !wan_dhcpenable);
	}
	else if(wan_type == "static"){
		document.form.wan_dhcpenable_x[0].checked = 0;
		document.form.wan_dhcpenable_x[1].checked = 1;
		
		inputCtrl(document.form.wan_dhcpenable_x[0], 0);
		inputCtrl(document.form.wan_dhcpenable_x[1], 0);
		
		document.getElementById('IPsetting').style.display = "";
		inputCtrl(document.form.wan_ipaddr_x, 1);
		inputCtrl(document.form.wan_netmask_x, 1);
		inputCtrl(document.form.wan_gateway_x, 1);
	}
	else{	// wan_type == "dhcp"
		document.form.wan_dhcpenable_x[0].checked = 1;
		document.form.wan_dhcpenable_x[1].checked = 0;
		
		inputCtrl(document.form.wan_dhcpenable_x[0], 0);
		inputCtrl(document.form.wan_dhcpenable_x[1], 0);
		
		inputCtrl(document.form.wan_ipaddr_x, 0);
		inputCtrl(document.form.wan_netmask_x, 0);
		inputCtrl(document.form.wan_gateway_x, 0);
		document.getElementById('IPsetting').style.display = "none";
	}
	
	if(document.form.wan_dhcpenable_x[0].checked){
		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);
	}
	else{		//wan_dhcpenable_x NO
		document.form.wan_dnsenable_x[0].checked = 0;
		document.form.wan_dnsenable_x[1].checked = 1;
		change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', 0);
		
		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);
		document.form.wan_dnsenable_x[0].disabled = true;
	}
}

function showMAC(){
	var tempMAC = "";
	document.form.wan_hwaddr_x.value = login_mac_str();
	document.form.wan_hwaddr_x.focus();
}

function check_macaddr(obj,flag){ //control hint of input mac address
	if(flag == 1){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac").innerHTML="<#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";		
		return false;
	}else if(flag ==2){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac").innerHTML="<#IPConnection_x_illegal_mac#>";
		return false;
	}else{
		document.getElementById("check_mac") ? document.getElementById("check_mac").style.display="none" : true;
		return true;
	}
}

/* password item show or not */
function pass_checked(obj){
	switchType(obj, document.form.show_pass_1.checked, true);
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<script>
	if(sw_mode == 3){
		alert("<#page_not_support_mode_hint#>");
		location.href = "/index.asp";
	}
</script>
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="wanUnit_form" action="/apply.cgi" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_WAN_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WAN_Content.asp">
<input type="hidden" name="action_mode" value="change_wan_unit">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="wan_unit" value="">
</form>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="support_cdma" value="<% nvram_get("support_cdma"); %>">
<input type="hidden" name="current_page" value="Advanced_WAN_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WAN_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_wan_if">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="lan_ipaddr" value="<% nvram_get("lan_ipaddr"); %>">
<input type="hidden" name="lan_netmask" value="<% nvram_get("lan_netmask"); %>">
<input type="hidden" name="ctf_fa_mode" value="<% nvram_get("ctf_fa_mode"); %>">
<input type="hidden" name="ctf_disable_force" value="<% nvram_get("ctf_disable_force"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td width="17">&nbsp;</td>
	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>
	
	<td height="430" valign="top">
	  <div id="tabMenu" class="submenuBlock"></div>
	  
	  <!--===================================Beginning of Main Content===========================================-->
	<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top">
			<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">			
				<tbody>
				<tr>
	  			<td bgcolor="#4D595D" valign="top">
		  			<div>&nbsp;</div>
		  			<div class="formfonttitle"><#menu5_3#> - <#menu5_3_1#></div>
		  			<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  			<div id="page_title" class="formfontdesc" style="margin-bottom:0px;"><#Layer3Forwarding_x_ConnectionType_sectiondesc#></div>

						<table id="WANscap" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
							<thead>
							<tr>
								<td colspan="2"><#wan_index#></td>
							</tr>
							</thead>							
							<tr>
								<th id="wan_inf_th"><#wan_type#></th>
								<td align="left">
									<select class="input_option" name="wan_unit" onchange="change_wan_unit(this);"></select>
									<!--select id="dsltmp_transmode" name="dsltmp_transmode" class="input_option" style="margin-left:7px;" onChange="change_dsl_transmode(this);">
													<option value="atm" <% nvram_match("dsltmp_transmode", "atm", "selected"); %>>ADSL WAN (ATM)</option>
													<option value="ptm" <% nvram_match("dsltmp_transmode", "ptm", "selected"); %>>VDSL WAN (PTM)</option>
									</select-->
								</td>
							</tr>
						</table>
						
						<div id="basic_setting_desc" class="formfontdesc" style="margin-bottom:0px; margin-top: 15px;"><#WAN_Cfg_Ethernet_WAN#></div>
						<table id="t2BC" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
						  <thead>
						  <tr>
							<td colspan="2"><#t2BC#></td>
						  </tr>
						  </thead>		

							<tr>
								<th><#Layer3Forwarding_x_ConnectionType_itemname#></th>
								<td align="left">
									<select id="wan_proto_menu" class="input_option" name="wan_proto" onchange="change_wan_type(this.value);fixed_change_wan_type(this.value);">
										<option value="dhcp" <% nvram_match("wan_proto", "dhcp", "selected"); %>><#BOP_ctype_title1#></option>
										<option value="static" <% nvram_match("wan_proto", "static", "selected"); %>><#BOP_ctype_title5#></option>
										<option value="pppoe" <% nvram_match("wan_proto", "pppoe", "selected"); %>>PPPoE</option>
										<option value="pptp" <% nvram_match("wan_proto", "pptp", "selected"); %>>PPTP</option>
										<option value="l2tp" <% nvram_match("wan_proto", "l2tp", "selected"); %>>L2TP</option>										
									</select>
								</td>
							</tr>

							<tr>
								<th><#Enable_WAN#></th>                 
								<td>
									<input type="radio" name="wan_enable" class="input" value="1" <% nvram_match("wan_enable", "1", "checked"); %>><#checkbox_Yes#>
									<input type="radio" name="wan_enable" class="input" value="0" <% nvram_match("wan_enable", "0", "checked"); %>><#checkbox_No#>
								</td>
							</tr>				

							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,22);"><#Enable_NAT#></a></th>                 
								<td>
									<input type="radio" name="wan_nat_x" class="input" value="1" <% nvram_match("wan_nat_x", "1", "checked"); %>><#checkbox_Yes#>
									<input type="radio" name="wan_nat_x" class="input" value="0" <% nvram_match("wan_nat_x", "0", "checked"); %>><#checkbox_No#>
								</td>
							</tr>				

							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,23);"><#BasicConfig_EnableMediaServer_itemname#></a>&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp<a id="faq" href="" target="_blank" style="font-family:Lucida Console;text-decoration:underline;">UPnP&nbspFAQ</a></th>
								<td>
									<input type="radio" name="wan_upnp_enable" class="input" value="1" onclick="display_upnp_options();return change_common_radio(this, 'LANHostConfig', 'wan_upnp_enable', '1')" <% nvram_match("wan_upnp_enable", "1", "checked"); %>><#checkbox_Yes#>
									<input type="radio" name="wan_upnp_enable" class="input" value="0" onclick="display_upnp_options();return change_common_radio(this, 'LANHostConfig', 'wan_upnp_enable', '0')" <% nvram_match("wan_upnp_enable", "0", "checked"); %>><#checkbox_No#>
								</td>
							</tr>
							<tr id="upnp_pinhole">
								<th>Enable UPnP IPv6 pinhole support</th>
								<td>
									<input type="radio" name="upnp_pinhole_enable" class="input" value="1" onclick="display_upnp_options();" <% nvram_match("upnp_pinhole_enable", "1", "checked"); %>><#checkbox_Yes#>
									<input type="radio" name="upnp_pinhole_enable" class="input" value="0" onclick="display_upnp_options();" <% nvram_match("upnp_pinhole_enable", "0", "checked"); %>><#checkbox_No#>
								</td>
							</tr>
							<tr id="upnp_range_int">
								<th>UPNP: Allowed internal port range</th>
									<td>
										<input type="text" maxlength="5" name="upnp_min_port_int" class="input_6_table" value="<% nvram_get("upnp_min_port_int"); %>" onkeypress="return validator.isNumber(this,event);">
											to
										<input type="text" maxlength="5" name="upnp_max_port_int" class="input_6_table" value="<% nvram_get("upnp_max_port_int"); %>" onkeypress="return validator.isNumber(this,event);">
									</td>
							</tr>
							<tr id="upnp_range_ext">

								<th>UPNP: Allowed external port range</th>
									<td>
										<input type="text" maxlength="5" name="upnp_min_port_ext" class="input_6_table" value="<% nvram_get("upnp_min_port_ext"); %>" onkeypress="return validator.isNumber(this,event);">
											to
										<input type="text" maxlength="5" name="upnp_max_port_ext" class="input_6_table" value="<% nvram_get("upnp_max_port_ext"); %>" onkeypress="return validator.isNumber(this,event);">
									</td>
							</tr>										
						</table>

						<table id="IPsetting" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
							<thead>
							<tr>
								<td colspan="2"><#IPConnection_ExternalIPAddress_sectionname#></td>
							</tr>
							</thead>
							
							<tr>
								<th><#Layer3Forwarding_x_DHCPClient_itemname#></th>
								<td>
									<input type="radio" name="wan_dhcpenable_x" class="input" value="1" onclick="change_wan_dhcp_enable(0);" <% nvram_match("wan_dhcpenable_x", "1", "checked"); %>><#checkbox_Yes#>
									<input type="radio" name="wan_dhcpenable_x" class="input" value="0" onclick="change_wan_dhcp_enable(0);" <% nvram_match("wan_dhcpenable_x", "0", "checked"); %>><#checkbox_No#>
								</td>
							</tr>
            
							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,1);"><#IPConnection_ExternalIPAddress_itemname#></a></th>
								<td><input type="text" name="wan_ipaddr_x" maxlength="15" class="input_15_table" value="<% nvram_get("wan_ipaddr_x"); %>" onKeyPress="return validator.isIPAddr(this, event);" autocorrect="off" autocapitalize="off"></td>
							</tr>
							
							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,2);"><#IPConnection_x_ExternalSubnetMask_itemname#></a></th>
								<td><input type="text" name="wan_netmask_x" maxlength="15" class="input_15_table" value="<% nvram_get("wan_netmask_x"); %>" onKeyPress="return validator.isIPAddr(this, event);" autocorrect="off" autocapitalize="off"></td>
							</tr>
							
							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,3);"><#IPConnection_x_ExternalGateway_itemname#></a></th>
								<td><input type="text" name="wan_gateway_x" maxlength="15" class="input_15_table" value="<% nvram_get("wan_gateway_x"); %>" onKeyPress="return validator.isIPAddr(this, event);" autocorrect="off" autocapitalize="off"></td>
							</tr>
						</table>

				<table id="DNSsetting" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
          		<thead>
            	<tr>
            	<td colspan="2"><#IPConnection_x_DNSServerEnable_sectionname#></td>
            	</tr>
          		</thead>
         			<tr>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,12);"><#IPConnection_x_DNSServerEnable_itemname#></a></th>
								<td>
			  					<input type="radio" name="wan_dnsenable_x" class="input" value="1" onclick="return change_common_radio(this, 'IPConnection', 'wan_dnsenable_x', 1)" <% nvram_match("wan_dnsenable_x", "1", "checked"); %> /><#checkbox_Yes#>
			  					<input type="radio" name="wan_dnsenable_x" class="input" value="0" onclick="return change_common_radio(this, 'IPConnection', 'wan_dnsenable_x', 0)" <% nvram_match("wan_dnsenable_x", "0", "checked"); %> /><#checkbox_No#>
								<div id="yadns_hint" style="display:none;"></div>
								</td>
          		</tr>          		
          		
          		<tr>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,13);"><#IPConnection_x_DNSServer1_itemname#></a></th>
            		<td><input type="text" maxlength="15" class="input_15_table" name="wan_dns1_x" value="<% nvram_get("wan_dns1_x"); %>" onkeypress="return validator.isIPAddr(this, event)" autocorrect="off" autocapitalize="off"></td>
          		</tr>
          		<tr>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,14);"><#IPConnection_x_DNSServer2_itemname#></a></th>
            		<td><input type="text" maxlength="15" class="input_15_table" name="wan_dns2_x" value="<% nvram_get("wan_dns2_x"); %>" onkeypress="return validator.isIPAddr(this, event)" autocorrect="off" autocapitalize="off"></td>
          		</tr>
        		</table>

		  		<table id="PPPsetting" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
            	<thead>
            	<tr>
              	<td colspan="2"><#PPPConnection_UserName_sectionname#></td>
            	</tr>
            	</thead>
                <th>MTU</th>
                <td><input type="text" maxlength="5" name="wan_mtu" class="input_6_table" value="<% nvram_get("wan_mtu"); %>" onKeyPress="return validator.isNumber(this,event);"/></td>
                </tr>
            	<tr>
							<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,29);"><#PPPConnection_Authentication_itemname#></a></th>
							<td align="left">
							    <select class="input_option" name="wan_auth_x" onChange="change_wan_type(document.form.wan_proto.value);">
							    <option value="" <% nvram_match("wan_auth_x", "", "selected"); %>><#wl_securitylevel_0#></option>
							    <option value="8021x-md5" <% nvram_match("wan_auth_x", "8021x-md5", "selected"); %>>802.1x MD5</option>
							    <option value="telenet" <% nvram_match("wan_auth_x", "telenet", "selected"); %>>Кабinet</option>
							    </select></td>
							</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,4);"><#PPPConnection_UserName_itemname#></a></th>
              	<td><input type="text" maxlength="64" class="input_32_table" name="wan_pppoe_username" value="" autocomplete="off" onkeypress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off"></td>
            	</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,5);"><#PPPConnection_Password_itemname#></a></th>
              	<td>
									<input type="password" maxlength="64" class="input_32_table" style="margin-top:2px;" id="wan_pppoe_passwd" name="wan_pppoe_passwd" value="" autocomplete="off" autocorrect="off" autocapitalize="off">
									<div style="margin-top:1px;"><input type="checkbox" name="show_pass_1" onclick="pass_checked(document.form.wan_pppoe_passwd);"><#QIS_show_pass#></div>
								</td>
            	</tr>            	
							<tr style="display:none">
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,6);"><#PPPConnection_IdleDisconnectTime_itemname#></a></th>
              	<td>
                	<input type="text" maxlength="10" class="input_12_table" name="wan_pppoe_idletime" value="<% nvram_get("wan_pppoe_idletime"); %>" onKeyPress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off"/>
                	<input type="checkbox" style="margin-left:30;display:none;" name="wan_pppoe_idletime_check" value="" />
              	</td>
            	</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,7);"><#PPPConnection_x_PPPoEMTU_itemname#></a></th>
              	<td><input type="text" maxlength="5" name="wan_pppoe_mtu" class="input_6_table" value="<% nvram_get("wan_pppoe_mtu"); %>" onKeyPress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off"/></td>
            	</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,8);"><#PPPConnection_x_PPPoEMRU_itemname#></a></th>
              	<td><input type="text" maxlength="5" name="wan_pppoe_mru" class="input_6_table" value="<% nvram_get("wan_pppoe_mru"); %>" onKeyPress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off"/></td>
            	</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,9);"><#PPPConnection_x_ServiceName_itemname#></a></th>
              	<td><input type="text" maxlength="32" class="input_32_table" name="wan_pppoe_service" value="<% nvram_get("wan_pppoe_service"); %>" onkeypress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off"/></td>
            	</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,10);"><#PPPConnection_x_AccessConcentrator_itemname#></a></th>
              	<td><input type="text" maxlength="32" class="input_32_table" name="wan_pppoe_ac" value="<% nvram_get("wan_pppoe_ac"); %>" onkeypress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off"/></td>
            	</tr>
            	<!-- 2008.03 James. patch for Oleg's patch. { -->
		<tr>
		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,17);"><#PPPConnection_x_PPTPOptions_itemname#></a></th>
		<td>
		<select name="wan_pptp_options_x" class="input_option">
			<option value="" <% nvram_match("wan_pptp_options_x", "","selected"); %>><#Auto#></option>
			<option value="-mppc" <% nvram_match("wan_pptp_options_x", "-mppc","selected"); %>><#No_Encryp#></option>
			<option value="+mppe-40" <% nvram_match("wan_pptp_options_x", "+mppe-40","selected"); %>>MPPE 40</option>
			<!--option value="+mppe-56" <% nvram_match("wan_pptp_options_x", "+mppe-56","selected"); %>>MPPE 56</option-->
			<option value="+mppe-128" <% nvram_match("wan_pptp_options_x", "+mppe-128","selected"); %>>MPPE 128</option>
		</select>
		</td>
		</tr>
		<tr>
		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,18);"><#PPPConnection_x_AdditionalOptions_itemname#></a></th>
		<td><input type="text" name="wan_pppoe_options_x" value="<% nvram_get("wan_pppoe_options_x"); %>" class="input_32_table" maxlength="255" onKeyPress="return validator.isString(this, event)" onBlur="validator.string(this)" autocorrect="off" autocapitalize="off"></td>
		</tr>
		<!-- 2008.03 James. patch for Oleg's patch. } -->
		<tr>
			<th><a class="hintstyle" href="javascript:void(0);">Enable PPP Echo Detect</a></th><!--untranslated-->
			<td>
				<input type="radio" name="wan_ppp_echo" class="input" value="1" <% nvram_match("wan_ppp_echo", "1", "checked"); %> /><#checkbox_Yes#>
				<input type="radio" name="wan_ppp_echo" class="input" value="0" <% nvram_match("wan_ppp_echo", "0", "checked"); %> /><#checkbox_No#>
			</td>
		</tr>		
          </table>

      <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
	  	<thead>
		<tr>
            	<td colspan="2"><#PPPConnection_x_HostNameForISP_sectionname#></td>
            	</tr>
		</thead>
		<tr>
          	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,19);"><#BOP_isp_heart_item#></a></th>
          	<td>
          	<!-- 2008.03 James. patch for Oleg's patch. { -->
          	<input type="text" name="wan_heartbeat_x" class="input_32_table" maxlength="256" value="<% nvram_get("wan_heartbeat_x"); %>" onKeyPress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off"></td>
          	<!-- 2008.03 James. patch for Oleg's patch. } -->
        	</tr>
		<tr id="vpn_dhcp">
		<th><!--a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,);"-->Enable VPN + DHCP Connection<!--/a--></th>
		<td><input type="radio" name="wan_vpndhcp" class="input" value="1" onclick="return change_common_radio(this, 'IPConnection', 'wan_vpndhcp', 1)" <% nvram_match("wan_vpndhcp", "1", "checked"); %> /><#checkbox_Yes#>
		    <input type="radio" name="wan_vpndhcp" class="input" value="0" onclick="return change_common_radio(this, 'IPConnection', 'wan_vpndhcp', 0)" <% nvram_match("wan_vpndhcp", "0", "checked"); %> /><#checkbox_No#>
		</td>
        	</tr>
        	<tr>
          	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,15);"><#PPPConnection_x_HostNameForISP_itemname#></a></th>
          	<td>
          		<div><input type="text" name="wan_hostname" class="input_32_table" maxlength="32" value="<% nvram_get("wan_hostname"); %>" onkeypress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off"><br/><span id="alert_msg1" style="color:#FC0;"></span></div>
          	</td>
        	</tr>
        	<tr>
          	<th ><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,16);"><#PPPConnection_x_MacAddressForISP_itemname#></a></th>
				<td>
					<input type="text" name="wan_hwaddr_x" class="input_20_table" maxlength="17" value="<% nvram_get("wan_hwaddr_x"); %>" onKeyPress="return validator.isHWAddr(this,event)" autocorrect="off" autocapitalize="off">
					<input type="button" class="button_gen_long" onclick="showMAC();" value="<#BOP_isp_MACclone#>">
				</td>
        	</tr>

        <tr>
		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,30);"><#DHCP_query_freq#></a></th>
		<td>
		<select name="dhcpc_mode" class="input_option">
			<option value="0" <% nvram_match(" dhcpc_mode", "0","selected"); %>><#DHCPnormal#></option>
			<option value="1" <% nvram_match(" dhcpc_mode", "1","selected"); %>><#DHCPaggressive#></option>
		</select>
		</td>
		</tr>

		<tr>
		<th>Manual clientid (for some ISPs)</th>
			<td><input type="text" name="wan_dhcpc_options" class="input_32_table" maxlength="128" value="<% nvram_get("wan_dhcpc_options"); %>" onkeypress="return is_string(this, event)"></td>
		</tr>

		<tr style="display:none;">
			<th><a class="hintstyle" href="javascript:void(0);" onClick=""><#Extend_TTL_Value#></a></th>
				<td>
					<input type="radio" name="ttl_inc_enable" class="input" value="1" <% nvram_match("ttl_inc_enable", "1", "checked"); %>><#checkbox_Yes#>
					<input type="radio" name="ttl_inc_enable" class="input" value="0" <% nvram_match("ttl_inc_enable", "0", "checked"); %>><#checkbox_No#>
				</td>
		</tr>	
		</table>
		<div class="apply_gen" style="height:auto">
			<input class="button_gen" onclick="applyRule();" type="button" value="<#CTL_apply#>"/>
		</div>

                    </td>
                    </tr>

      	  </td>
      	  </tr>
</tbody>
</table>
</td>
</form>
				</tr>
			</table>

		</td>
		<!--===================================Ending of Main Content===========================================-->
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>

</body>
</html>
