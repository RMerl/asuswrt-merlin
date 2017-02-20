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
<title><#Web_Title#> - IPTV</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script type="text/javascript" src="state.js"></script>
<script type="text/javascript" src="general.js"></script>
<script type="text/javascript" src="popup.js"></script>
<script type="text/javascript" src="help.js"></script>
<script type="text/javascript" src="validator.js"></script>
<script type="text/javaScript" src="/js/jquery.js"></script>
<script type="text/javascript" src="switcherplugin/jquery.iphone-switch.js"></script>

<style>
.contentM_connection{
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:500;
	background-color:#2B373B;
	margin-left: 30%;
	margin-top: 10px;
	width:650px;
	display:none;
	box-shadow: 3px 3px 10px #000;
}
</style>

<script>
var original_switch_stb_x = '<% nvram_get("switch_stb_x"); %>';
var original_switch_wantag = '<% nvram_get("switch_wantag"); %>';
var original_switch_wan0tagid = '<%nvram_get("switch_wan0tagid"); %>';
var original_switch_wan0prio  = '<%nvram_get("switch_wan0prio"); %>';
var original_switch_wan1tagid = '<%nvram_get("switch_wan1tagid"); %>';
var original_switch_wan1prio  = '<%nvram_get("switch_wan1prio"); %>';
var original_switch_wan2tagid = '<%nvram_get("switch_wan2tagid"); %>';
var original_switch_wan2prio  = '<%nvram_get("switch_wan2prio"); %>';

var wans_lanport = '<% nvram_get("wans_lanport"); %>';
var wans_dualwan_orig = '<% nvram_get("wans_dualwan"); %>';

function initial(){
	show_menu();
	if(dsl_support) {
		document.form.action_script.value = "reboot";
		document.form.action_wait.value = "<% get_default_reboot_time(); %>";
	}
	else{	//DSL not support
		ISP_Profile_Selection(original_switch_wantag);
		
		if(!manualstb_support)
			document.form.switch_wantag.remove(8);
	}
	
	if(vdsl_support) {
		if(document.form.dslx_rmvlan.value == "1")
			document.form.dslx_rmvlan_check.checked = true;
		else
			document.form.dslx_rmvlan_check.checked = false;
	}
	
	document.form.switch_stb_x.value = original_switch_stb_x;
	disable_udpxy();
	if(!Qcawifi_support)
		document.getElementById('enable_eff_multicast_forward').style.display="";
	
	if(dualWAN_support)
		document.getElementById("IPTV_desc_DualWAN").style.display = "";
	else	
		document.getElementById("IPTV_desc").style.display = "";

	if(based_modelid == "RT-AC87U"){ //MODELDEP: RT-AC87 : Quantenna port
		document.form.switch_stb_x.remove(5);	//LAN1 & LAN2
		document.form.switch_stb_x.remove(1);	//LAN1
	}

	if( based_modelid != "RT-AC5300" &&
		based_modelid != "RT-AC3200" &&
		based_modelid != "RT-AC3100" &&
		based_modelid != "RT-AC1200G+" &&
		based_modelid != "RT-AC88U" &&
		based_modelid != "RT-AC87U" && 
		based_modelid != "RT-AC68U" &&
		based_modelid != "RT-AC68A" &&
		based_modelid != "4G-AC68U" &&
		based_modelid != "RT-AC66U" &&
		based_modelid != "RT-AC56U" &&
		based_modelid != "RT-AC51U" &&
		based_modelid != "RT-N66U" &&
		based_modelid != "RT-N18U"
	){
		document.getElementById('meoOption').outerHTML = "";
		document.getElementById('vodafoneOption').outerHTML = "";
	}
	
	if( based_modelid != "RT-AC5300" &&
		based_modelid != "RT-AC3200" &&
		based_modelid != "RT-AC3100" &&
		based_modelid != "RT-AC1200G+" &&
		based_modelid != "RT-AC88U" &&
		based_modelid != "RT-AC87U" &&
		based_modelid != "RT-AC68U" &&
		based_modelid != "RT-AC68A" &&
		based_modelid != "4G-AC68U" &&
		based_modelid != "RT-AC66U" &&
		based_modelid != "RT-AC56U" &&
		based_modelid != "RT-AC56S" &&
		based_modelid != "RT-AC51U" &&
		based_modelid != "RT-N66U" &&
		based_modelid != "RT-N18U"
        ){
		document.getElementById('movistarOption').outerHTML = "";
	}

}

function load_ISP_profile(){
	//setting_value = [[wan0tagid, wan0prio], [wan1tagid, wan1prio], [wan2tagid, wan2prio], switch_stb_x.value];
	var setting_value = new Array();
	if(document.form.switch_wantag.value == "unifi_home"){
		setting_value = [["500", "0"], ["600", "0"], ["", "0"], "4"];
	}
	else if(document.form.switch_wantag.value == "unifi_biz"){
		setting_value = [["500", "0"], ["", "0"], ["", "0"], "0"];
	}
	else if(document.form.switch_wantag.value == "singtel_mio"){
		setting_value = [["10", "0"], ["20", "4"], ["30", "4"], "6"]; 
	}
	else if(document.form.switch_wantag.value == "singtel_others"){
		setting_value = [["10", "0"], ["20", "4"], ["", "0"], "4"];  
	}
	else if(document.form.switch_wantag.value == "m1_fiber"){
		setting_value = [["1103", "1"], ["", "0"], ["1107", "1"], "3"]; 
	}
	else if(document.form.switch_wantag.value == "maxis_fiber_sp"){
		setting_value = [["11", "0"], ["", "0"], ["14", "0"], "3"];
	}
	else if(document.form.switch_wantag.value == "maxis_fiber"){
		setting_value = [["621", "0"], ["", "0"], ["821,822", "0"], "3"]; 
	}
	else if(document.form.switch_wantag.value == "maxis_fiber_sp_iptv"){
		setting_value = [["11", "0"], ["15", "0"], ["14", "0"], "7"];
	}
	else if(document.form.switch_wantag.value == "maxis_fiber_iptv") {
		setting_value = [["621", "0"], ["823", "0"], ["821,822", "0"], "7"];
	}
	else if(document.form.switch_wantag.value == "movistar") {
		setting_value = [["6", "0"], ["2", "0"], ["3", "0"], "8"]; 
	}
	else if(document.form.switch_wantag.value == "meo") {
		setting_value = [["12", "0"], ["12", "0"], ["", "0"], "4"]; 
	}
        else if(document.form.switch_wantag.value == "vodafone") {
                setting_value = [["100", "1"], ["", "0"], ["105", "1"], "3"]; 
        }
        else if(document.form.switch_wantag.value == "hinet") {
                setting_value = [["", "0"], ["", "0"], ["", "0"], "4"]; 
        }
	
	if(setting_value.length == 4){
		document.form.switch_wan0tagid.value = setting_value[0][0];
		document.form.switch_wan0prio.value = setting_value[0][1];
		document.form.switch_wan1tagid.value = setting_value[1][0];
		document.form.switch_wan1prio.value = setting_value[1][1];
		document.form.switch_wan2tagid.value = setting_value[2][0];
		document.form.switch_wan2prio.value = setting_value[2][1];
		document.form.switch_stb_x.value = setting_value[3];
	}

	if(document.form.switch_wantag.value == "maxis_fiber_sp_iptv" || 
	   document.form.switch_wantag.value == "maxis_fiber_iptv" ||
	   document.form.switch_wantag.value == "meo" ||
	   document.form.switch_wantag.value == "vodafone"
	) {
		document.form.mr_enable_x.value = "1";
		document.form.emf_enable.value = "1";
		document.form.wan_vpndhcp.value = "0";
		document.form.quagga_enable.value = "0";
	}
	else if(document.form.switch_wantag.value == "movistar") {
		document.form.quagga_enable.value = "1";
		document.form.mr_enable_x.value = "1";
		document.form.wan_vpndhcp.value = "0";
		document.form.mr_altnet_x.value = "172.0.0.0/8";
	}
	else {
		document.form.quagga_enable.value = "0";
		document.form.mr_altnet_x.value = "";
	}
	if(document.form.switch_wantag.value == "meo")
		document.form.ttl_inc_enable.value = "1";
}

function ISP_Profile_Selection(isp){
/*	ISP_setting = [
			wan_stb_x.style.display,
			wan_iptv_x.style.display,
			wan_voip_x.style.display,
			wan_internet_x.style.display,
			wan_iptv_port4_x.style.display,
			wan_voip_port3_x.style.display,
			switch_stb_x.value,
			mr_enable_field.style.display,
			iptv_settings_btn.style.display,
			voip_settings_btn".style.display
	];*/
	var ISP_setting = new Array();
	if(isp == "none"){
		ISP_setting = ["", "none", "none", "none", "none", "none", "0", "", "", "none", "none"];
	}
	else if(isp == "unifi_home" || isp == "singtel_others" || isp == "meo" || isp == "hinet"){
		ISP_setting = ["none", "", "none", "none", "none", "none", "4", "", "", "none", "none"];
	}
	else if(isp == "unifi_biz"){
		ISP_setting = ["none", "none", "none", "none", "none", "none", "0", "", "", "none", "none"];
	}
	else if(isp == "singtel_mio"){
		ISP_setting = ["none", "", "", "none", "none", "none", "6", "", "", "none", "none"];
	}
	else if(isp == "m1_fiber" || isp == "maxis_fiber_sp" || isp == "maxis_fiber"){
		ISP_setting = ["none", "none", "", "none", "none", "none", "3", "", "", "none", "none"];
	}
	else if(isp == "maxis_fiber_sp_iptv" || isp == "maxis_fiber_iptv"){
		ISP_setting = ["none", "none", "none", "none", "none", "none", "7", "none", "none", "none", "none"];
	}
	else if(isp == "movistar"){
		ISP_setting = ["none", "", "", "none", "none", "none", "7", "none", "none", "", ""];
	}
	else if(isp == "vodafone"){
		ISP_setting = ["none", "", "", "none", "none", "none", "3", "", "", "none", "none"];
	}
	else if(isp == "manual"){
		ISP_setting = ["none", "none", "none", "", "", "", "6", "", "", "none", "none"];
	}
	
	document.form.switch_wantag.value = isp;
	document.getElementById("wan_stb_x").style.display = ISP_setting[0];
	document.getElementById("wan_iptv_x").style.display = ISP_setting[1];
	document.getElementById("wan_voip_x").style.display = ISP_setting[2];
	document.getElementById("wan_internet_x").style.display = ISP_setting[3];
	document.getElementById("wan_iptv_port4_x").style.display = ISP_setting[4];
	document.getElementById("wan_voip_port3_x").style.display = ISP_setting[5];
	document.form.switch_stb_x.value = ISP_setting[6];
	document.getElementById("mr_enable_field").style.display = ISP_setting[7];
	if(!Qcawifi_support)
		document.getElementById("enable_eff_multicast_forward").style.display = ISP_setting[8];
	else
		document.getElementById("enable_eff_multicast_forward").style.display = "none";

	document.getElementById("iptv_settings_btn").style.display = ISP_setting[9];
	document.getElementById("voip_settings_btn").style.display = ISP_setting[10];

	if(ISP_setting[9] == ""){
		document.getElementById("iptv_title").innerHTML = "IPTV";
		document.getElementById("iptv_port").style.display = "none";
	}
	else{
		if(isp == "vodafone" || isp == "meo")
			document.getElementById("iptv_title").innerHTML = "Bridge Port";
		else
			document.getElementById("iptv_title").innerHTML = "IPTV STB Port";
		document.getElementById("iptv_port").style.display = "";
	}

	if(ISP_setting[10] == ""){
		document.getElementById("voip_title").innerHTML = "VoIP";
		document.getElementById("voip_port").style.display = "none";
	}
	else{
		if(isp == "vodafone")
			document.getElementById("voip_title").innerHTML = "IPTV STB Port";
		else
			document.getElementById("voip_title").innerHTML = "VoIP Port";
		document.getElementById("voip_port").style.display = "";
	}

	if(isp == "movistar"){
		document.getElementById("iptv_configure_status").style.display = "";
		document.getElementById("voip_configure_status").style.display = "";
		if(check_config_state("iptv"))
			document.getElementById("iptv_configure_status").innerHTML = "<#wireless_configured#>";
		else
			document.getElementById("iptv_configure_status").innerHTML = "Unconfigured";

		if(check_config_state("voip"))
			document.getElementById("voip_configure_status").innerHTML = "<#wireless_configured#>";
		else
			document.getElementById("voip_configure_status").innerHTML = "Unconfigured";
	}
	else{
		document.getElementById("iptv_configure_status").style.display = "none";
		document.getElementById("voip_configure_status").style.display = "none";		
	}
}

function validForm(){
	if (!dsl_support){
        if(document.form.switch_wantag.value == "manual"){
		if(document.form.switch_wan1tagid.value == "" && document.form.switch_wan2tagid.value != "")
			document.form.switch_stb_x.value = "3";
		else if(document.form.switch_wan1tagid.value != "" && document.form.switch_wan2tagid.value == "")
			document.form.switch_stb_x.value = "4";
		else if(document.form.switch_wan1tagid.value == "" && document.form.switch_wan2tagid.value == "")
			document.form.switch_stb_x.value = "0";
				
		if(document.form.switch_wan0tagid.value.length > 0 && !validator.rangeNull(document.form.switch_wan0tagid, 2, 4094, ""))
			return false;
                
		if(document.form.switch_wan1tagid.value.length > 0 && !validator.rangeNull(document.form.switch_wan1tagid, 2, 4094, ""))
			return false;           
			
		if(document.form.switch_wan2tagid.value.length > 0 && !validator.rangeNull(document.form.switch_wan2tagid, 2, 4094, ""))
			return false;           

		if(document.form.switch_wan0prio.value.length > 0 && !validator.range(document.form.switch_wan0prio, 0, 7))
			return false;

		if(document.form.switch_wan1prio.value.length > 0 && !validator.range(document.form.switch_wan1prio, 0, 7))
			return false;

		if(document.form.switch_wan2prio.value.length > 0 && !validator.range(document.form.switch_wan2prio, 0, 7))
			return false;
        }
	}
	
	return true;
}

function applyRule(){
	if(dualWAN_support){	// dualwan LAN port should not be equal to IPTV port
		var tmp_pri_if = wans_dualwan_orig.split(" ")[0].toUpperCase();
		var tmp_sec_if = wans_dualwan_orig.split(" ")[1].toUpperCase();
		if (tmp_pri_if == 'LAN' || tmp_sec_if == 'LAN'){
			var port_conflict = false;
			var iptv_port = document.form.switch_stb_x.value;
			if(wans_lanport == iptv_port)
				port_conflict = true;
			else if( (wans_lanport == 1 || wans_lanport == 2) && iptv_port == 5)	
				port_conflict = true;
			else if( (wans_lanport == 3 || wans_lanport == 4) && iptv_port == 6)	
				port_conflict = true;	

			if (port_conflict) {
				alert("<#RouterConfig_IPTV_conflict#>");
				return;
			}
		}
	}

	if(!dsl_support){
		if( (original_switch_stb_x != document.form.switch_stb_x.value) 
		||  (original_switch_wantag != document.form.switch_wantag.value)
		||  (original_switch_wan0tagid != document.form.switch_wan0tagid.value)
		||  (original_switch_wan0prio != document.form.switch_wan0prio.value)
        ||  (original_switch_wan1tagid != document.form.switch_wan1tagid.value)
        ||  (original_switch_wan1prio != document.form.switch_wan1prio.value)
        ||  (original_switch_wan2tagid != document.form.switch_wan2tagid.value)
        ||  (original_switch_wan2prio != document.form.switch_wan2prio.value)){
			FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
		}
		
		load_ISP_profile();			
	} 

	if(validForm()){
		if(document.form.udpxy_enable_x.value != 0 && document.form.udpxy_enable_x.value != ""){	//validate UDP Proxy
			if(!validator.range(document.form.udpxy_enable_x, 1024, 65535)){
				document.form.udpxy_enable_x.focus();
				document.form.udpxy_enable_x.select();
				return false;
			}
		}

		if(document.form.wan_proto_now.disabled==true)
			document.form.wan_proto_now.disabled==false;

		showLoading();
		document.form.submit();			
	}
}

// The input field of UDP proxy does not relate to Mutlicast Routing. 
function disable_udpxy(){
	if(document.form.mr_enable_x.value == 1){
		return change_common_radio(document.form.mr_enable_x, 'RouterConfig', 'mr_enable_x', '1');
	}
	else{	
		return change_common_radio(document.form.mr_enable_x, 'RouterConfig', 'mr_enable_x', '0');
	}	
}

function change_rmvlan(){
	if(document.form.dslx_rmvlan_check.checked == true)
		document.form.dslx_rmvlan.value = 1;
	else
		document.form.dslx_rmvlan.value = 0;
}

var original_wan_proto_now = "";
var original_dnsenable_now = "";
var currentService = "";
var curState = "";//dns_switch
function set_connection(service){
    /*
    connection_type = [iptv_connection_type, voip_connection_type];
     */
	var connection_type = new Array();
	if(document.form.switch_wantag.value == "movistar"){
		connection_type = ["static", "dhcp"];
	}

	if(document.form.switch_wantag.value != original_switch_wantag){
		document.form.wan10_proto.value = connection_type[0];
		document.form.wan11_proto.value = connection_type[1];
	}

	currentService = service;
	copy_index_to_unindex(service);
	curState = document.form.wan_dnsenable_x_now.value;
	if(service == "iptv"){
		document.getElementById("con_settings_title").innerHTML = "IPTV Connection Settings";
	}
	else if(service == "voip"){
		document.getElementById("con_settings_title").innerHTML = "VoIP Connection Settings";
	}

	original_wan_proto_now = document.form.wan_proto_now.value;
	original_dnsenable_now = document.form.wan_dnsenable_x_now.value;
	change_wan_type(document.form.wan_proto_now.value);
	document.form.show_pass_1.checked = false;
	switchType(document.form.wan_pppoe_passwd_now, document.form.show_pass_1.checked, true);
	set_wandhcp_switch(document.form.wan_dhcpenable_x_now.value);
	set_dns_switch(document.form.wan_dnsenable_x_now.value);
	show_connection_settings();
}

function show_connection_settings(){
	$("#connection_settings_table").fadeIn(300);
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

function check_config_state(service){
	var wan_proto = "";
	var wan_ipaddr = "";
	var pppoe_username = "";
	var pppoe_passwd = "";
	var wan_ipaddr = "";
	var connection_type = new Array();

	if(document.form.switch_wantag.value == "movistar"){
		connection_type = ["static", "dhcp"];
	}

	if(service == "iptv"){
		if(document.form.switch_wantag.value != original_switch_wantag)
			wan_proto = connection_type[0];
		else
			wan_proto = document.form.wan10_proto.value;
		username = document.form.wan10_pppoe_username.value;
		passwd = document.form.wan10_pppoe_passwd.value;
		wan_ipaddr = document.form.wan10_ipaddr_x.value;
	}
	else if(service == "voip"){
		if(document.form.switch_wantag.value != original_switch_wantag)
			wan_proto =  connection_type[1];
		else
			wan_proto = document.form.wan11_proto.value;
		username = document.form.wan11_pppoe_username.value;
		passwd = document.form.wan11_pppoe_passwd.value;
		wan_ipaddr = document.form.wan11_ipaddr_x.value;
	}

	if(wan_proto == "pppoe" || wan_proto == "pptp" || wan_proto == "l2tp"){
		if(username != "" && passwd != "")
			return true;
		else
			return false;
	}
	else if(wan_proto == "static"){
		if(wan_ipaddr != "" && wan_ipaddr != "0.0.0.0")
			return true;
		else
			return false;
	}
	else /* dhcp */
		return true;
}

function save_connection_settings(){
	/* Validate Conneciton Settings */
	if(document.form.wan_dhcpenable_x_now.value == "0"){// Set IP address by userself
		if(!valid_IP(document.form.wan_ipaddr_x_now, "")) return false;  //WAN IP
		if(!valid_IP(document.form.wan_gateway_x_now, "GW"))return false;  //Gateway IP

		if(document.form.wan_gateway_x_now.value == document.form.wan_ipaddr_x_now.value){
			document.form.wan_ipaddr_x_now.focus();
			alert("<#IPConnection_warning_WANIPEQUALGatewayIP#>");
			return false;
		}
		
		// test if netmask is valid.
		var default_netmask = "";
		var wrong_netmask = 0;
		var netmask_obj = document.form.wan_netmask_x_now;
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

	if(document.form.wan_dnsenable_x_now.value == "0" && document.form.wan_proto_now.value != "dhcp" && document.form.wan_dns1_x_now.value == "" && document.form.wan_dns2_x_now.value == ""){
		document.form.wan_dns1_x_now.focus();
		alert("<#IPConnection_x_DNSServer_blank#>");
		return false;
	}
	
	if(!document.form.wan_dnsenable_x_now.value == "1"){
		if(!valid_IP(document.form.wan_dns1_x_now, "DNS")) return false;  //DNS1
		if(!valid_IP(document.form.wan_dns2_x_now, "DNS")) return false;  //DNS2
	}
	
	if(document.form.wan_proto_now.value == "pppoe"
			|| document.form.wan_proto_now.value == "pptp"
			|| document.form.wan_proto_now.value == "l2tp"
			){
		if(!validator.string(document.form.wan_pppoe_username_now)
				|| !validator.string(document.form.wan_pppoe_passwd_now)
				)
			return false;
		
		if(!validator.numberRange(document.form.wan_pppoe_idletime_now, 0, 4294967295))
			return false;
	}
	
	if(document.form.wan_proto_now.value == "pppoe"){
		if(!validator.numberRange(document.form.wan_pppoe_mtu_now, 576, 1492)
				|| !validator.numberRange(document.form.wan_pppoe_mru_now, 576, 1492))
			return false;
		
		if(!validator.string(document.form.wan_pppoe_service_now)
				|| !validator.string(document.form.wan_pppoe_ac_now))
			return false;
	}

	hide_connection_settings();
	copy_unindex_to_index(currentService);
	if(currentService == "iptv")
		document.getElementById("iptv_configure_status").innerHTML = "<#wireless_configured#>";
	else if(currentService == "voip")
		document.getElementById("voip_configure_status").innerHTML = "<#wireless_configured#>";
}

function hide_connection_settings(){
	$("#connection_settings_table").fadeOut(300);
}

function copy_index_to_unindex(service){
	if(service == "iptv"){
		document.form.wan_proto_now.value = document.form.wan10_proto.value;
		document.form.wan_dhcpenable_x_now.value = document.form.wan10_dhcpenable_x.value;
		document.form.wan_dnsenable_x_now.value = document.form.wan10_dnsenable_x.value;
		document.form.wan_pppoe_username_now.value = document.form.wan10_pppoe_username.value;
		document.form.wan_pppoe_passwd_now.value = document.form.wan10_pppoe_passwd.value;
		document.form.wan_pppoe_idletime_now.value = document.form.wan10_pppoe_idletime.value;
		document.form.wan_pppoe_mtu_now.value = document.form.wan10_pppoe_mtu.value;
		document.form.wan_pppoe_mru_now.value = document.form.wan10_pppoe_mru.value;
		document.form.wan_pppoe_service_now.value = document.form.wan10_pppoe_service.value;
		document.form.wan_pppoe_ac_now.value = document.form.wan10_pppoe_ac.value;
		document.form.wan_pppoe_options_x_now.value = document.form.wan10_pppoe_options_x.value;
		document.form.wan_ipaddr_x_now.value = document.form.wan10_ipaddr_x.value;
		document.form.wan_netmask_x_now.value = document.form.wan10_netmask_x.value;
		document.form.wan_gateway_x_now.value = document.form.wan10_gateway_x.value;
		document.form.wan_dns1_x_now.value = document.form.wan10_dns1_x.value;
		document.form.wan_dns2_x_now.value = document.form.wan10_dns2_x.value;
		document.form.wan_auth_x_now.value = document.form.wan10_auth_x.value;
	}
	else if(service == "voip"){
		document.form.wan_proto_now.value = document.form.wan11_proto.value;
		document.form.wan_dhcpenable_x_now.value = document.form.wan11_dhcpenable_x.value;
		document.form.wan_dnsenable_x_now.value = document.form.wan11_dnsenable_x.value;
		document.form.wan_pppoe_username_now.value = document.form.wan11_pppoe_username.value;
		document.form.wan_pppoe_passwd_now.value = document.form.wan11_pppoe_passwd.value;
		document.form.wan_pppoe_idletime_now.value = document.form.wan11_pppoe_idletime.value;
		document.form.wan_pppoe_mtu_now.value = document.form.wan11_pppoe_mtu.value;
		document.form.wan_pppoe_mru_now.value = document.form.wan11_pppoe_mru.value;
		document.form.wan_pppoe_service_now.value = document.form.wan11_pppoe_service.value;
		document.form.wan_pppoe_ac_now.value = document.form.wan11_pppoe_ac.value;
		document.form.wan_pppoe_options_x_now.value = document.form.wan11_pppoe_options_x.value;
		document.form.wan_ipaddr_x_now.value = document.form.wan11_ipaddr_x.value;
		document.form.wan_netmask_x_now.value = document.form.wan11_netmask_x.value;
		document.form.wan_gateway_x_now.value = document.form.wan11_gateway_x.value;
		document.form.wan_dns1_x_now.value = document.form.wan11_dns1_x.value;
		document.form.wan_dns2_x_now.value = document.form.wan11_dns2_x.value;
		document.form.wan_auth_x_now.value = document.form.wan11_auth_x.value;
	}
}

function copy_unindex_to_index(service){
	if(service == "iptv"){
		document.form.wan10_proto.value = document.form.wan_proto_now.value;
		document.form.wan10_dhcpenable_x.value = document.form.wan_dhcpenable_x_now.value;
		document.form.wan10_dnsenable_x.value = document.form.wan_dnsenable_x_now.value;
		document.form.wan10_pppoe_username.value = document.form.wan_pppoe_username_now.value;
		document.form.wan10_pppoe_passwd.value = document.form.wan_pppoe_passwd_now.value;
		document.form.wan10_pppoe_idletime.value = document.form.wan_pppoe_idletime_now.value;
		document.form.wan10_pppoe_mtu.value = document.form.wan_pppoe_mtu_now.value;
		document.form.wan10_pppoe_mru.value = document.form.wan_pppoe_mru_now.value;
		document.form.wan10_pppoe_service.value = document.form.wan_pppoe_service_now.value;
		document.form.wan10_pppoe_ac.value = document.form.wan_pppoe_ac_now.value;
		document.form.wan10_pppoe_options_x.value = document.form.wan_pppoe_options_x_now.value;
		document.form.wan10_ipaddr_x.value = document.form.wan_ipaddr_x_now.value;
		document.form.wan10_netmask_x.value = document.form.wan_netmask_x_now.value;
		document.form.wan10_gateway_x.value = document.form.wan_gateway_x_now.value;
		document.form.wan10_dns1_x.value = document.form.wan_dns1_x_now.value;
		document.form.wan10_dns2_x.value = document.form.wan_dns2_x_now.value;
		document.form.wan10_auth_x.value = document.form.wan_auth_x_now.value;
	}
	else if(service == "voip"){
		document.form.wan11_proto.value = document.form.wan_proto_now.value;
		document.form.wan11_dhcpenable_x.value = document.form.wan_dhcpenable_x_now.value;
		document.form.wan11_dnsenable_x.value = document.form.wan_dnsenable_x_now.value;
		document.form.wan11_pppoe_username.value = document.form.wan_pppoe_username_now.value;
		document.form.wan11_pppoe_passwd.value = document.form.wan_pppoe_passwd_now.value;
		document.form.wan11_pppoe_idletime.value = document.form.wan_pppoe_idletime_now.value;
		document.form.wan11_pppoe_mtu.value = document.form.wan_pppoe_mtu_now.value;
		document.form.wan11_pppoe_mru.value = document.form.wan_pppoe_mru_now.value;
		document.form.wan11_pppoe_service.value = document.form.wan_pppoe_service_now.value;
		document.form.wan11_pppoe_ac.value = document.form.wan_pppoe_ac_now.value;
		document.form.wan11_pppoe_options_x.value = document.form.wan_pppoe_options_x_now.value;
		document.form.wan11_ipaddr_x.value = document.form.wan_ipaddr_x_now.value;
		document.form.wan11_netmask_x.value = document.form.wan_netmask_x_now.value;
		document.form.wan11_gateway_x.value = document.form.wan_gateway_x_now.value;
		document.form.wan11_dns1_x.value = document.form.wan_dns1_x_now.value;
		document.form.wan11_dns2_x.value = document.form.wan_dns2_x_now.value;
		document.form.wan11_auth_x.value = document.form.wan_auth_x_now.value;
	}
}

function change_wan_type(wan_type){

	if(wan_type == "pppoe"){
		document.getElementById("wan_dhcp_tr").style.display="";
		document.getElementById("dnsenable_tr").style.display = "";

		if(original_switch_wantag != document.form.switch_wantag.value || original_wan_proto_now != document.form.wan_proto_now.value){
			document.form.wan_dhcpenable_x_now.value = "1";
		}
		set_wandhcp_switch(document.form.wan_dhcpenable_x_now.value);

		if(original_switch_wantag != document.form.switch_wantag.value || original_wan_proto_now != document.form.wan_proto_now.value){
			document.form.wan_dnsenable_x_now.value = "1";
		}
		set_dns_switch(document.form.wan_dnsenable_x_now.value);

		if(document.form.wan_dnsenable_x_now.value == "1"){
			inputCtrl(document.form.wan_dns1_x_now, 0);
			inputCtrl(document.form.wan_dns2_x_now, 0);
		}
		else{
			inputCtrl(document.form.wan_dns1_x_now, 1);
			inputCtrl(document.form.wan_dns2_x_now, 1);
		}
		inputCtrl(document.form.wan_auth_x_now, 0);
		inputCtrl(document.form.wan_pppoe_username_now, 1);
		document.getElementById('tr_pppoe_password').style.display = "";
		document.form.wan_pppoe_passwd_now.disabled = false;
		inputCtrl(document.form.wan_pppoe_idletime_now, 1);
		if(document.form.wan_pppoe_idletime_now.value.length == 0)
			document.form.wan_pppoe_idletime_now.value = "0";
		inputCtrl(document.form.wan_pppoe_idletime_check, 1);
		inputCtrl(document.form.wan_pppoe_mtu_now, 1);
		if(document.form.wan_pppoe_mtu_now.value.length == 0)
			document.form.wan_pppoe_mtu_now.value = "1492";
		inputCtrl(document.form.wan_pppoe_mru_now, 1);
		if(document.form.wan_pppoe_mru_now.value.length == 0)
			document.form.wan_pppoe_mru_now.value = "1492";
		inputCtrl(document.form.wan_pppoe_service_now, 1);
		inputCtrl(document.form.wan_pppoe_ac_now, 1);
		
		inputCtrl(document.form.wan_pppoe_options_x_now, 1);
		inputCtrl(document.form.wan_pptp_options_x_now, 0);

		var wan_dhcpenable = parseInt(document.form.wan_dhcpenable_x_now.value);
		document.getElementById('IPsetting').style.display = "";
		inputCtrl(document.form.wan_ipaddr_x_now, !wan_dhcpenable);
		inputCtrl(document.form.wan_netmask_x_now, !wan_dhcpenable);
		inputCtrl(document.form.wan_gateway_x_now, !wan_dhcpenable);

	}
	else if(wan_type == "pptp" || wan_type == "l2tp"){
		document.getElementById("wan_dhcp_tr").style.display="";
		document.getElementById("dnsenable_tr").style.display = "";

		if(original_switch_wantag != document.form.switch_wantag.value || original_wan_proto_now != document.form.wan_proto_now.value){
			document.form.wan_dhcpenable_x_now.value = "0";
		}
		set_wandhcp_switch(document.form.wan_dhcpenable_x_now.value);

		var wan_dhcpenable = parseInt(document.form.wan_dhcpenable_x_now.value);
		document.getElementById('IPsetting').style.display = "";
		inputCtrl(document.form.wan_ipaddr_x_now, !wan_dhcpenable);
		inputCtrl(document.form.wan_netmask_x_now, !wan_dhcpenable);
		inputCtrl(document.form.wan_gateway_x_now, !wan_dhcpenable);

		if(original_switch_wantag != document.form.switch_wantag.value || original_wan_proto_now != document.form.wan_proto_now.value){
			document.form.wan_dnsenable_x_now.value = "0";
		}
		set_dns_switch(document.form.wan_dnsenable_x_now.value);

		if(document.form.wan_dnsenable_x_now.value == "1"){
			inputCtrl(document.form.wan_dns1_x_now, 0);
			inputCtrl(document.form.wan_dns2_x_now, 0);
		}
		else{
			inputCtrl(document.form.wan_dns1_x_now, 1);
			inputCtrl(document.form.wan_dns2_x_now, 1);
		}
		inputCtrl(document.form.wan_auth_x_now, 0);
		inputCtrl(document.form.wan_pppoe_username_now, 1);
		document.getElementById('tr_pppoe_password').style.display = "";
		document.form.wan_pppoe_passwd_now.disabled = false;
		inputCtrl(document.form.wan_pppoe_mtu_now, 0);
		inputCtrl(document.form.wan_pppoe_mru_now, 0);
		inputCtrl(document.form.wan_pppoe_service_now, 0);
		inputCtrl(document.form.wan_pppoe_ac_now, 0);
		inputCtrl(document.form.wan_pppoe_options_x_now, 1);
	
		if(wan_type == "pptp"){
			inputCtrl(document.form.wan_pppoe_idletime_now, 1);
			if(document.form.wan_pppoe_idletime_now.value.length == 0)
				document.form.wan_pppoe_idletime_now.value = "0";
			inputCtrl(document.form.wan_pppoe_idletime_check, 1);
			inputCtrl(document.form.wan_pptp_options_x_now, 1);
		}
		else if(wan_type == "l2tp"){
			inputCtrl(document.form.wan_pppoe_idletime_now, 0);
			inputCtrl(document.form.wan_pppoe_idletime_check, 0);
			inputCtrl(document.form.wan_pptp_options_x_now, 0);
		}
	}
	else if(wan_type == "static"){
		document.getElementById("wan_dhcp_tr").style.display = "none";
		document.form.wan_dhcpenable_x_now.value = "0";
		document.getElementById('IPsetting').style.display = "";
		inputCtrl(document.form.wan_ipaddr_x_now, 1);
		inputCtrl(document.form.wan_netmask_x_now, 1);
		inputCtrl(document.form.wan_gateway_x_now, 1);

		inputCtrl(document.form.wan_auth_x_now, 1);
		inputCtrl(document.form.wan_pppoe_username_now, (document.form.wan_auth_x_now.value != ""));
		document.getElementById('tr_pppoe_password').style.display = (document.form.wan_auth_x_now.value != "") ? "" : "none";
		document.form.wan_pppoe_passwd_now.disabled = (document.form.wan_auth_x_now.value != "") ? false : true;
		inputCtrl(document.form.wan_pppoe_idletime_now, 0);
		inputCtrl(document.form.wan_pppoe_idletime_check, 0);
		inputCtrl(document.form.wan_pppoe_mtu_now, 0);
		inputCtrl(document.form.wan_pppoe_mru_now, 0);
		inputCtrl(document.form.wan_pppoe_service_now, 0);
		inputCtrl(document.form.wan_pppoe_ac_now, 0);
		
		inputCtrl(document.form.wan_pppoe_options_x_now, 0);
		inputCtrl(document.form.wan_pptp_options_x_now, 0);

		document.getElementById("dnsenable_tr").style.display = "none";
		inputCtrl(document.form.wan_dns1_x_now, 1);
		inputCtrl(document.form.wan_dns2_x_now, 1);
	}
	else{	// Automatic IP or 802.11 MD or ""
		document.form.wan_dhcpenable_x_now.value = "1";
		document.getElementById('IPsetting').style.display = "none";		
		inputCtrl(document.form.wan_ipaddr_x_now, 0);
		inputCtrl(document.form.wan_netmask_x_now, 0);
		inputCtrl(document.form.wan_gateway_x_now, 0);
		document.getElementById('IPsetting').style.display = "none";

		document.getElementById("dnsenable_tr").style.display = "";
		if(original_switch_wantag != document.form.switch_wantag.value || original_wan_proto_now != document.form.wan_proto_now.value){
			document.form.wan_dnsenable_x_now.value = "1";
		}
		set_dns_switch(document.form.wan_dnsenable_x_now.value);

		if(document.form.wan_dnsenable_x_now.value == "1"){
			inputCtrl(document.form.wan_dns1_x_now, 0);
			inputCtrl(document.form.wan_dns2_x_now, 0);
		}
		else{
			inputCtrl(document.form.wan_dns1_x_now, 1);
			inputCtrl(document.form.wan_dns2_x_now, 1);
		}

		inputCtrl(document.form.wan_auth_x_now, 1);	
		
		inputCtrl(document.form.wan_pppoe_username_now, (document.form.wan_auth_x_now.value != ""));
		document.getElementById('tr_pppoe_password').style.display = (document.form.wan_auth_x_now.value != "") ? "" : "none";
		document.form.wan_pppoe_passwd_now.disabled = (document.form.wan_auth_x_now.value != "") ? false : true;
		
		inputCtrl(document.form.wan_pppoe_idletime_now, 0);
		inputCtrl(document.form.wan_pppoe_idletime_check, 0);
		inputCtrl(document.form.wan_pppoe_mtu_now, 0);
		inputCtrl(document.form.wan_pppoe_mru_now, 0);
		inputCtrl(document.form.wan_pppoe_service_now, 0);
		inputCtrl(document.form.wan_pppoe_ac_now, 0);
		
		inputCtrl(document.form.wan_pppoe_options_x_now, 0);
		inputCtrl(document.form.wan_pptp_options_x_now, 0);
	}
}

function change_wan_dhcp_enable(wan_dhcpenable_flag){
	if(wan_dhcpenable_flag == "0"){
		$('#dns_switch').find('.iphone_switch').animate({backgroundPosition: -37}, "slow");
		curState = "0";
		document.form.wan_dnsenable_x_now.value = "0";
		document.getElementById("dns_switch").style.pointerEvents = 'none';
		inputCtrl(document.form.wan_dns1_x_now, 1);
		inputCtrl(document.form.wan_dns2_x_now, 1);
	}
	else if(wan_dhcpenable_flag == "1"){
		document.getElementById("dns_switch").style.pointerEvents = 'auto';
	}
}

var curWandhcpState = "";
function set_wandhcp_switch(wan_dhcpenable_flag){
	if(wan_dhcpenable_flag == "0"){
		curWandhcpState = "0";
		$('#wandhcp_switch').find('.iphone_switch').animate({backgroundPosition: -37}, "fast");
	}
	else if(wan_dhcpenable_flag == "1"){
		curWandhcpState = "1";
		$('#wandhcp_switch').find('.iphone_switch').animate({backgroundPosition: 0}, "fast");
	}
}

function set_dns_switch(wan_dnsenable_flag){
	if(wan_dnsenable_flag == "0"){
		$('#dns_switch').find('.iphone_switch').animate({backgroundPosition: -37}, "fast");
		curState = "0";
	}
	else if(wan_dnsenable_flag == "1"){
		$('#dns_switch').find('.iphone_switch').animate({backgroundPosition: 0}, "fast");
		curState = "1";
	}
}

/* password item show or not */
function pass_checked(obj){
	switchType(obj, document.form.show_pass_1.checked, true);
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="hiddenMask" class="popup_bg">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
		<tr>
		<td>
			<div class="drword" id="drword" style="height:110px;"><#Main_alert_proceeding_desc4#> <#Main_alert_proceeding_desc1#>...
				<br/>
				<br/>
	    </div>
		  <div class="drImg"><img src="images/alertImg.png"></div>
			<div style="height:70px;"></div>
		</td>
		</tr>
	</table>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="/Advanced_IPTV_Content.asp">
<input type="hidden" name="next_page" value="/Advanced_IPTV_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_script" value="restart_net">
<input type="hidden" name="action_wait" value="10">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="dslx_rmvlan" value='<% nvram_get("dslx_rmvlan"); %>'>
<input type="hidden" name="ttl_inc_enable" value='<% nvram_get("ttl_inc_enable"); %>'>
<input type="hidden" name="switch_wan3tagid" value="<% nvram_get("switch_wan3tagid"); %>">
<input type="hidden" name="switch_wan3prio" value="<% nvram_get("switch_wan3prio"); %>">
<input type="hidden" name="wan_vpndhcp" value="<% nvram_get("wan_vpndhcp"); %>">
<input type="hidden" name="wan_dhcpenable_x_now" value="">
<input type="hidden" name="wan_dnsenable_x_now" value="">

<input type="hidden" name="wan10_proto" value="<% nvram_get("wan10_proto"); %>">
<input type="hidden" name="wan10_dhcpenable_x" value="<% nvram_get("wan10_dhcpenable_x"); %>">
<input type="hidden" name="wan10_dnsenable_x" value="<% nvram_get("wan10_dnsenable_x"); %>">
<input type="hidden" name="wan10_pppoe_username" value="<% nvram_get("wan10_pppoe_username"); %>">
<input type="hidden" name="wan10_pppoe_passwd" value="<% nvram_get("wan10_pppoe_passwd"); %>">
<input type="hidden" name="wan10_pppoe_idletime" value="<% nvram_get("wan10_pppoe_idletime"); %>">
<input type="hidden" name="wan10_pppoe_mtu" value="<% nvram_get("wan10_pppoe_mtu"); %>">
<input type="hidden" name="wan10_pppoe_mru" value="<% nvram_get("wan10_pppoe_mru"); %>">
<input type="hidden" name="wan10_pppoe_service" value="<% nvram_get("wan10_pppoe_service"); %>">
<input type="hidden" name="wan10_pppoe_ac" value="<% nvram_get("wan10_pppoe_ac"); %>">
<input type="hidden" name="wan10_pppoe_options_x" value="<% nvram_get("wan10_pppoe_options_x"); %>">
<input type="hidden" name="wan10_ipaddr_x" value="<% nvram_get("wan10_ipaddr_x"); %>">
<input type="hidden" name="wan10_netmask_x" value="<% nvram_get("wan10_netmask_x"); %>">
<input type="hidden" name="wan10_gateway_x" value="<% nvram_get("wan10_gateway_x"); %>">
<input type="hidden" name="wan10_dns1_x" value="<% nvram_get("wan10_dns1_x"); %>">
<input type="hidden" name="wan10_dns2_x" value="<% nvram_get("wan10_dns2_x"); %>">
<input type="hidden" name="wan10_auth_x" value="<% nvram_get("wan10_auth_x"); %>">

<input type="hidden" name="wan11_proto" value="<% nvram_get("wan11_proto"); %>">
<input type="hidden" name="wan11_dhcpenable_x" value="<% nvram_get("wan11_dhcpenable_x"); %>">
<input type="hidden" name="wan11_dnsenable_x" value="<% nvram_get("wan11_dnsenable_x"); %>">
<input type="hidden" name="wan11_pppoe_username" value="<% nvram_get("wan11_pppoe_username"); %>">
<input type="hidden" name="wan11_pppoe_passwd" value="<% nvram_get("wan11_pppoe_passwd"); %>">
<input type="hidden" name="wan11_pppoe_idletime" value="<% nvram_get("wan11_pppoe_idletime"); %>">
<input type="hidden" name="wan11_pppoe_mtu" value="<% nvram_get("wan11_pppoe_mtu"); %>">
<input type="hidden" name="wan11_pppoe_mru" value="<% nvram_get("wan11_pppoe_mru"); %>">
<input type="hidden" name="wan11_pppoe_service" value="<% nvram_get("wan11_pppoe_service"); %>">
<input type="hidden" name="wan11_pppoe_ac" value="<% nvram_get("wan11_pppoe_ac"); %>">
<input type="hidden" name="wan11_pppoe_options_x" value="<% nvram_get("wan11_pppoe_options_x"); %>">
<input type="hidden" name="wan11_ipaddr_x" value="<% nvram_get("wan11_ipaddr_x"); %>">
<input type="hidden" name="wan11_netmask_x" value="<% nvram_get("wan11_netmask_x"); %>">
<input type="hidden" name="wan11_gateway_x" value="<% nvram_get("wan11_gateway_x"); %>">
<input type="hidden" name="wan11_dns1_x" value="<% nvram_get("wan11_dns1_x"); %>">
<input type="hidden" name="wan11_dns2_x" value="<% nvram_get("wan11_dns2_x"); %>">
<input type="hidden" name="wan11_auth_x" value="<% nvram_get("wan11_auth_x"); %>">
<input type="hidden" name="quagga_enable" value="<% nvram_get("quagga_enable"); %>">
<input type="hidden" name="mr_altnet_x" value="<% nvram_get("mr_altnet_x"); %>">

<!---- connection settings start  ---->
<div id="connection_settings_table"  class="contentM_connection">
	<table border="0" align="center" cellpadding="5" cellspacing="5">
		<tr>
			<td align="left">
			<span id="con_settings_title" class="formfonttitle">Connection Settings</span>
			<div style="width:630px; height:15px;overflow:hidden;position:relative;left:0px;top:5px;"><img src="/images/New_ui/export/line_export.png"></div>
			<div></div>
			</td>
		</tr>
		<tr>
			<td>
				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
					<tr>
						<th><#Layer3Forwarding_x_ConnectionType_itemname#></th>
						<td align="left">
							<select id="wan_proto_menu" class="input_option" name="wan_proto_now" onchange="change_wan_type(this.value);">
								<option value="dhcp"><#BOP_ctype_title1#></option>
								<option value="static"><#BOP_ctype_title5#></option>
								<option value="pppoe">PPPoE</option>
								<option value="pptp">PPTP</option>
								<option value="l2tp">L2TP</option>										
							</select>						
		  				</td>
					</tr>
		 		</table>
	  		</td>
		</tr>
		<tr id="IPsetting">
			<td>
				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
				<thead>
					<tr>
						<td colspan="2"><#IPConnection_ExternalIPAddress_sectionname#></td>
					</tr>
				</thead>
				<tr id="wan_dhcp_tr">
					<th><#Layer3Forwarding_x_DHCPClient_itemname#></th>
					<td>
						<div class="left" style="width:94px; float:left;" id="wandhcp_switch"></div>
						<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
							<script type="text/javascript">
								$('#wandhcp_switch').iphoneSwitch(document.form.wan_dhcpenable_x_now.value,
									function() {
										curWandhcpState = "1";
										document.form.wan_dhcpenable_x_now.value = "1";
										inputCtrl(document.form.wan_ipaddr_x_now, 0);
										inputCtrl(document.form.wan_netmask_x_now, 0);
										inputCtrl(document.form.wan_gateway_x_now, 0);
										change_wan_dhcp_enable("1")
										return true;
									},
									function() {
										curWandhcpState = "0";
										document.form.wan_dhcpenable_x_now.value = "0";
										inputCtrl(document.form.wan_ipaddr_x_now, 1);
										inputCtrl(document.form.wan_netmask_x_now, 1);
										inputCtrl(document.form.wan_gateway_x_now, 1);
										change_wan_dhcp_enable("0");
										return true;
									}
								);
							</script>
					</td>
				</tr>
				<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,1);"><#IPConnection_ExternalIPAddress_itemname#></a></th>
					<td><input type="text" name="wan_ipaddr_x_now" maxlength="15" class="input_15_table" value="" onKeyPress="return validator.isIPAddr(this, event);" ></td>
				</tr>
				<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,2);"><#IPConnection_x_ExternalSubnetMask_itemname#></a></th>
					<td><input type="text" name="wan_netmask_x_now" maxlength="15" class="input_15_table" value="" onKeyPress="return validator.isIPAddr(this, event);" ></td>
				</tr>
				<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,3);"><#IPConnection_x_ExternalGateway_itemname#></a></th>
					<td><input type="text" name="wan_gateway_x_now" maxlength="15" class="input_15_table" value="" onKeyPress="return validator.isIPAddr(this, event);" ></td>
				</tr>
				</table>
			</td>
		</tr>
		<tr id="DNSsetting">
			<td>
				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
          		<thead>
            	<tr>
             	<td colspan="2"><#IPConnection_x_DNSServerEnable_sectionname#></td>
            	</tr>
          		</thead>
         		<tr id="dnsenable_tr">
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,12);"><#IPConnection_x_DNSServerEnable_itemname#></a></th>
					<td>
						<div class="left" style="width:94px; float:left;" id="dns_switch"></div>
						<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden"></div>
							<script type="text/javascript">
								$('#dns_switch').iphoneSwitch(document.form.wan_dnsenable_x_now.value, 
									function() {
										curState = "1";
										document.form.wan_dnsenable_x_now.value = "1";
										inputCtrl(document.form.wan_dns1_x_now, 0);
										inputCtrl(document.form.wan_dns2_x_now, 0);
										return true;
									},
									function() {
										curState = "0";
										document.form.wan_dnsenable_x_now.value = "0";
										inputCtrl(document.form.wan_dns1_x_now, 1);
										inputCtrl(document.form.wan_dns2_x_now, 1);
										return true;
									}
								);
							</script>
						<div id="yadns_hint" style="display:none;"></div>
					</td>
          		</tr>
          		<tr>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,13);"><#IPConnection_x_DNSServer1_itemname#></a></th>
            		<td><input type="text" maxlength="15" class="input_15_table" name="wan_dns1_x_now" value="" onkeypress="return validator.isIPAddr(this, event)" ></td>
          		</tr>
          		<tr>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,14);"><#IPConnection_x_DNSServer2_itemname#></a></th>
            		<td><input type="text" maxlength="15" class="input_15_table" name="wan_dns2_x_now" value="" onkeypress="return validator.isIPAddr(this, event)" ></td>
          		</tr>
        		</table>
	  		</td>	
		</tr>
		<tr id="PPPsetting" >
			<td>
		  		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
            	<thead>
            		<tr>
              			<td colspan="2"><#PPPConnection_UserName_sectionname#></td>
            		</tr>
            	</thead>
            	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,29);"><#PPPConnection_Authentication_itemname#></a></th>
					<td align="left">
					    <select class="input_option" name="wan_auth_x_now" onChange="change_wan_type(document.form.wan_proto_now.value);">
						    <option value="" <% nvram_match("wan_auth_x_now", "", "selected"); %>><#wl_securitylevel_0#></option>
						    <option value="8021x-md5" <% nvram_match("wan_auth_x_now", "8021x-md5", "selected"); %>>802.1x MD5</option>
					    </select></td>
				</tr>
            	<tr>
             	 	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,4);"><#PPPConnection_UserName_itemname#></a></th>
              		<td><input type="text" maxlength="64" class="input_32_table" name="wan_pppoe_username_now" value="" onkeypress="return validator.isString(this, event)"></td>
            	</tr>
            	<tr id="tr_pppoe_password">
              		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,5);"><#PPPConnection_Password_itemname#></a></th>
              		<td>
					<div style="margin-top:2px;"><input type="password" autocapitalize="off" maxlength="64" class="input_32_table" id="wan_pppoe_passwd_now" name="wan_pppoe_passwd_now" value=""></div>
					<div style="margin-top:1px;"><input type="checkbox" name="show_pass_1" onclick="pass_checked(document.form.wan_pppoe_passwd_now);"><#QIS_show_pass#></div>
					</td>
            	</tr>
				<tr style="display:none">
              		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,6);"><#PPPConnection_IdleDisconnectTime_itemname#></a></th>
              		<td>
                		<input type="text" maxlength="10" class="input_12_table" name="wan_pppoe_idletime_now" value="" onKeyPress="return validator.isNumber(this,event);" />
                		<input type="checkbox" style="margin-left:30;display:none;" name="wan_pppoe_idletime_check" value="" />
              		</td>
            	</tr>
            	<tr>
              		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,7);"><#PPPConnection_x_PPPoEMTU_itemname#></a></th>
              		<td><input type="text" maxlength="5" name="wan_pppoe_mtu_now" class="input_6_table" value="" onKeyPress="return validator.isNumber(this,event);"/></td>
            	</tr>
            	<tr>
              		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,8);"><#PPPConnection_x_PPPoEMRU_itemname#></a></th>
              		<td><input type="text" maxlength="5" name="wan_pppoe_mru_now" class="input_6_table" value="" onKeyPress="return validator.isNumber(this,event);"/></td>
            	</tr>
            	<tr>
              		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,9);"><#PPPConnection_x_ServiceName_itemname#></a></th>
              		<td><input type="text" maxlength="32" class="input_32_table" name="wan_pppoe_service_now" value="" onkeypress="return validator.isString(this, event)"/></td>
            	</tr>
            	<tr>
              		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,10);"><#PPPConnection_x_AccessConcentrator_itemname#></a></th>
              		<td><input type="text" maxlength="32" class="input_32_table" name="wan_pppoe_ac_now" value="<% nvram_get("wan_pppoe_ac_now"); %>" onkeypress="return validator.isString(this, event)"/></td>
            	</tr>
				<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,17);"><#PPPConnection_x_PPTPOptions_itemname#></a></th>
					<td>
						<select name="wan_pptp_options_x_now" class="input_option">
							<option value="" <% nvram_match("wan_pptp_options_x_now", "","selected"); %>><#Auto#></option>
							<option value="-mppc" <% nvram_match("wan_pptp_options_x_now", "-mppc","selected"); %>><#No_Encryp#></option>
							<option value="+mppe-40" <% nvram_match("wan_pptp_options_x_now", "+mppe-40","selected"); %>>MPPE 40</option>
							<option value="+mppe-128" <% nvram_match("wan_pptp_options_x_now", "+mppe-128","selected"); %>>MPPE 128</option>
						</select>
					</td>
				</tr>
				<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,18);"><#PPPConnection_x_AdditionalOptions_itemname#></a></th>
					<td><input type="text" name="wan_pppoe_options_x_now" value="<% nvram_get("wan_pppoe_options_x_now"); %>" class="input_32_table" maxlength="255" onKeyPress="return validator.isString(this, event)" onBlur="validator.string(this)"></td>
				</tr>
          </table>
        </td>
    </tr>
	</table>

	<div style="margin-top:5px;padding-bottom:10px;width:100%;text-align:center;">
		<input class="button_gen" type="button" onclick="hide_connection_settings();" value="<#CTL_Cancel#>">
		<input class="button_gen" type="button" onclick="save_connection_settings();" value="<#CTL_ok#>">
	</div>
</div>
<!---- connection settings end  ---->

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td width="17">&nbsp;</td>
	
	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>
	
    <td valign="top">
	<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td align="left" valign="top">
  <table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
	<tbody>
	<tr>
		  <td bgcolor="#4D595D" valign="top"  >
		  <div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_2#> - IPTV</div>
      <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
      <div id="IPTV_desc" class="formfontdesc" style="display:none;"><#LANHostConfig_displayIPTV_sectiondesc#></div>
      <div id="IPTV_desc_DualWAN" class="formfontdesc" style="display:none;"><#LANHostConfig_displayIPTV_sectiondesc2#></div>
	  
	  
	  <!-- IPTV & VoIP Setting -->
	  
		<!--###HTML_PREP_START###-->		
	  <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
	  	<thead>
			<tr>
				<td colspan="2"><#Port_Mapping_item1#></td>
			</tr>
		</thead>
	    	<tr>
	    	<th width="30%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,28);"><#Select_ISPfile#></a></th>
			<td>
				<select name="switch_wantag" class="input_option" onChange="ISP_Profile_Selection(this.value)">
					<option value="none" <% nvram_match( "switch_wantag", "none", "selected"); %>><#wl_securitylevel_0#></option>
					<option value="unifi_home" <% nvram_match( "switch_wantag", "unifi_home", "selected"); %>>Unifi-Home</option>
					<option value="unifi_biz" <% nvram_match( "switch_wantag", "unifi_biz", "selected"); %>>Unifi-Business</option>
					<option value="singtel_mio" <% nvram_match( "switch_wantag", "singtel_mio", "selected"); %>>Singtel-MIO</option>
					<option value="singtel_others" <% nvram_match( "switch_wantag", "singtel_others", "selected"); %>>Singtel-Others</option>
					<option value="m1_fiber" <% nvram_match("switch_wantag", "m1_fiber", "selected"); %>>M1-Fiber</option>
					<option value="maxis_fiber" <% nvram_match("switch_wantag", "maxis_fiber", "selected"); %>>Maxis-Fiber</option>
					<option value="maxis_fiber_sp" <% nvram_match("switch_wantag", "maxis_fiber_sp", "selected"); %>>Maxis-Fiber-Special</option>
					<option id="movistarOption" value="movistar" <% nvram_match("switch_wantag", "movistar", "selected"); %>>Movistar Triple VLAN</option>
					<option id="meoOption" value="meo" <% nvram_match("switch_wantag", "meo", "selected"); %>>Meo</option>
					<option id="vodafoneOption" value="vodafone" <% nvram_match("switch_wantag", "vodafone", "selected"); %>>Vodafone</option>
					<option value="hinet" <% nvram_match("switch_wantag", "hinet", "selected"); %>>Hinet MOD</option>
<!--					
					<option value="maxis_fiber_iptv" <% nvram_match("switch_wantag", "maxis_fiber_iptv", "selected"); %>>Maxis-Fiber-IPTV</option>
					<option value="maxis_fiber_sp_iptv" <% nvram_match("switch_wantag", "maxis_fiber_sp_iptv", "selected"); %>>Maxis-Fiber-Special-IPTV</option>
-->
					<option value="manual" <% nvram_match( "switch_wantag", "manual", "selected"); %>><#Manual_Setting_btn#></option>
				</select>
			</td>
			</tr>
		<tr id="wan_stb_x">
		<th width="30%"><#Layer3Forwarding_x_STB_itemname#></th>
		<td align="left">
		    <select name="switch_stb_x" class="input_option">
			<option value="0" <% nvram_match( "switch_stb_x", "0", "selected"); %>><#wl_securitylevel_0#></option>
			<option value="1" <% nvram_match( "switch_stb_x", "1", "selected"); %>>LAN1</option>
			<option value="2" <% nvram_match( "switch_stb_x", "2", "selected"); %>>LAN2</option>
			<option value="3" <% nvram_match( "switch_stb_x", "3", "selected"); %>>LAN3</option>
			<option value="4" <% nvram_match( "switch_stb_x", "4", "selected"); %>>LAN4</option>
			<option value="5" <% nvram_match( "switch_stb_x", "5", "selected"); %>>LAN1 & LAN2</option>
			<option value="6" <% nvram_match( "switch_stb_x", "6", "selected"); %>>LAN3 & LAN4</option>
			<option value="7" style="visibility:hidden"></option>
			<option value="8" style="visibility:hidden"></option>
		    </select>
		</td>
		</tr>
		<tr id="wan_iptv_x">
	  	<th id="iptv_title" width="30%">IPTV STB Port</th>
	  	<td><span id="iptv_port">LAN4 </span><span><input id="iptv_settings_btn" class="button_gen_long" type="button" onclick="set_connection('iptv');" value="IPTV Connection"></span><span id="iptv_configure_status" style="display:none;">Unconfigured</span></td>
		</tr>
		<tr id="wan_voip_x">
	  	<th id="voip_title" width="30%">VoIP Port</th>
	  	<td><span id="voip_port">LAN3</span><span><input id="voip_settings_btn" class="button_gen_long" type="button" onclick="set_connection('voip');" value="VoIP Connection"><span id="voip_configure_status" style="display:none;">Unconfigured</span></span>
	  	</td>
		</tr>
		<tr id="wan_internet_x">
	  	<th width="30%"><#Internet#></th>
	  	<td>
			VID&nbsp;<input type="text" name="switch_wan0tagid" class="input_6_table" maxlength="4" value="<% nvram_get( "switch_wan0tagid"); %>" onKeyPress="return validator.isNumber(this, event);" autocorrect="off" autocapitalize="off">&nbsp;&nbsp;&nbsp;&nbsp;
			PRIO&nbsp;<input type="text" name="switch_wan0prio" class="input_3_table" maxlength="1" value="<% nvram_get( "switch_wan0prio"); %>" onKeyPress="return validator.isNumber(this, event);" autocorrect="off" autocapitalize="off">
	  	</td>
		</tr>
	    	<tr id="wan_iptv_port4_x">
	    	<th width="30%">LAN port 4</th>
	  	<td>
			VID&nbsp;<input type="text" name="switch_wan1tagid" class="input_6_table" maxlength="4" value="<% nvram_get( "switch_wan1tagid"); %>" onKeyPress="return validator.isNumber(this, event);" autocorrect="off" autocapitalize="off">&nbsp;&nbsp;&nbsp;&nbsp;
			PRIO&nbsp;<input type="text" name="switch_wan1prio" class="input_3_table" maxlength="1" value="<% nvram_get( "switch_wan1prio"); %>" onKeyPress="return validator.isNumber(this, event);" autocorrect="off" autocapitalize="off">
	  	</td>
		</tr>
		<tr id="wan_voip_port3_x">
	  	<th width="30%">LAN port 3</th>
	  	<td>
			VID&nbsp;<input type="text" name="switch_wan2tagid" class="input_6_table" maxlength="4" value="<% nvram_get( "switch_wan2tagid"); %>" onKeyPress="return validator.isNumber(this, event);" autocorrect="off" autocapitalize="off">&nbsp;&nbsp;&nbsp;&nbsp;
			PRIO&nbsp;<input type="text" name="switch_wan2prio" class="input_3_table" maxlength="1" value="<% nvram_get( "switch_wan2prio"); %>" onKeyPress="return validator.isNumber(this, event);" autocorrect="off" autocapitalize="off">
	  	</td>
		</tr>
		</table>
<!--###HTML_PREP_ELSE###-->
<!--
[DSL-N55U][DSL-N55U-B]
{DSL do not support unifw}
	  <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
	  	<thead>
		<tr>
            	<td colspan="2">Port</td>
            	</tr>
		</thead>
		<tr id="wan_stb_x">
		<th width="30%"><#Layer3Forwarding_x_STB_itemname#></th>
		<td align="left">
		    <select name="switch_stb_x" class="input_option">
			<option value="0" <% nvram_match( "switch_stb_x", "0", "selected"); %>><#wl_securitylevel_0#></option>
			<option value="1" <% nvram_match( "switch_stb_x", "1", "selected"); %>>LAN1</option>
			<option value="2" <% nvram_match( "switch_stb_x", "2", "selected"); %>>LAN2</option>
			<option value="3" <% nvram_match( "switch_stb_x", "3", "selected"); %>>LAN3</option>
			<option value="4" <% nvram_match( "switch_stb_x", "4", "selected"); %>>LAN4</option>
			<option value="5" <% nvram_match( "switch_stb_x", "5", "selected"); %>>LAN1 & LAN2</option>
			<option value="6" <% nvram_match( "switch_stb_x", "6", "selected"); %>>LAN3 & LAN4</option>
		    </select>
		</td>
		</tr>
		</table>
[DSL-AC68U]
	<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
		<thead>
			<tr>
				<td colspan="2">Port</td>
			</tr>
		</thead>
		<tr id="wan_stb_x">
			<th width="30%"><#Layer3Forwarding_x_STB_itemname#></th>
			<td align="left">
				<select name="switch_stb_x" class="input_option">
				<option value="0" <% nvram_match( "switch_stb_x", "0", "selected"); %>><#wl_securitylevel_0#></option>
				<option value="1" <% nvram_match( "switch_stb_x", "1", "selected"); %>>LAN1</option>
				<option value="2" <% nvram_match( "switch_stb_x", "2", "selected"); %>>LAN2</option>
				<option value="3" <% nvram_match( "switch_stb_x", "3", "selected"); %>>LAN3</option>
				<option value="4" <% nvram_match( "switch_stb_x", "4", "selected"); %>>LAN4</option>
				<option value="5" <% nvram_match( "switch_stb_x", "5", "selected"); %>>LAN1 & LAN2</option>
				<option value="6" <% nvram_match( "switch_stb_x", "6", "selected"); %>>LAN3 & LAN4</option>
				</select>
				<input type="checkbox" name="dslx_rmvlan_check" id="dslx_rmvlan_check" value="" onClick="change_rmvlan();"> Remove VLAN TAG from DSL WAN</input>
			</td>
		</tr>
	</table>
-->
<!--###HTML_PREP_END###-->	  
	  

		<!-- End of IPTV & VoIP -->	  
	  
		  <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:10px;">
	  	<thead>
		<tr>
            	<td colspan="2"><#IPConnection_BattleNet_sectionname#></td>
            	</tr>
		</thead>

		<tr>
			<th><#RouterConfig_GWDHCPEnable_itemname#></th>
			<td>
				<select name="dr_enable_x" class="input_option">
				<option value="0" <% nvram_match("dr_enable_x", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
				<option value="1" <% nvram_match("dr_enable_x", "1","selected"); %> >Microsoft</option>
				<option value="2" <% nvram_match("dr_enable_x", "2","selected"); %> >RFC3442</option>
				<option value="3" <% nvram_match("dr_enable_x", "3","selected"); %> >RFC3442 & Microsoft</option>
				</select>
			</td>
		</tr>

			<!--tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,11);"><#RouterConfig_GWMulticastEnable_itemname#></a></th>
				<td>
					<input type="radio" value="1" name="mr_enable_x" class="input" onClick="disable_udpxy();" <% nvram_match("mr_enable_x", "1", "checked"); %>><#checkbox_Yes#>
					<input type="radio" value="0" name="mr_enable_x" class="input" onClick="disable_udpxy();" <% nvram_match("mr_enable_x", "0", "checked"); %>><#checkbox_No#>
				</td>
			</tr-->	
			<tr id="mr_enable_field">
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,11);"><#RouterConfig_GWMulticastEnable_itemname#> (IGMP Proxy)</a></th>
				<td>
          <select name="mr_enable_x" class="input_option">
            <option value="0" <% nvram_match("mr_enable_x", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
           	<option value="1" <% nvram_match("mr_enable_x", "1","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
          </select>
				</td>
			</tr>

					<tr id="enable_eff_multicast_forward" style="display:none;">
						<th><#WLANConfig11b_x_Emf_itemname#></th>
						<td>
                  				<select name="emf_enable" class="input_option">
                    					<option value="0" <% nvram_match("emf_enable", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
                    					<option value="1" <% nvram_match("emf_enable", "1","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
                  				</select>
						</td>
					</tr>
			<!-- 2008.03 James. patch for Oleg's patch. } -->
			<tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(6, 6);"><#RouterConfig_IPTV_itemname#></a></th>
     		<td>
     			<input id="udpxy_enable_x" type="text" maxlength="5" class="input_6_table" name="udpxy_enable_x" value="<% nvram_get("udpxy_enable_x"); %>" onkeypress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off">
     		</td>
     	</tr>
		</table>	

		<div class="apply_gen">
			<input class="button_gen" onclick="applyRule()" type="button" value="<#CTL_apply#>"/>
		</div>
		
	  </td>
	</tr>

	</tbody>	
  </table>		
					
		</td>
	</form>					
				</tr>
			</table>				
			<!--===================================End of Main Content===========================================-->
</td>

    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
