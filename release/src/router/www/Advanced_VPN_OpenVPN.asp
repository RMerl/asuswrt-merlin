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
<title><#Web_Title#> - <#BOP_isp_heart_item#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="menu_style.css">
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" language="JavaScript" src="/merlin.js"></script>
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script language="JavaScript" type="text/javascript" src="/form.js"></script>
<style type="text/css">
.contentM_qis{
	width:740px;	
	margin-top:280px;
	margin-left:380px;
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:200;
	background-color:#2B373B;
	box-shadow: 3px 3px 10px #000;
	display:none;
	/*behavior: url(/PIE.htc);*/
}

.QISform_wireless{
	width:600px;
	font-size:12px;
	color:#FFFFFF;
	margin-top:10px;
	*margin-left:10px;
}

.QISform_wireless th{
	padding-left:10px;
	*padding-left:30px;
	font-size:12px;
	font-weight:bolder;
	color: #FFFFFF;
	text-align:left; 
}
.description_down{
	margin-top:10px;
	margin-left:10px;
	padding-left:5px;
	font-weight:bold;
	line-height:140%;
	color:#ffffff;	
}
</style>
<script>
window.onresize = function() {
	if(document.getElementById("tlsKey_panel").style.display == "block") {
		cal_panel_block("tlsKey_panel", 0.15);
	}
} 

<% wanlink(); %>
<% vpn_server_get_parameter(); %>;

var vpn_server_clientlist_array_ori = '<% nvram_char_to_ascii("","vpn_serverx_clientlist"); %>';
var vpn_server_clientlist_array = decodeURIComponent(vpn_server_clientlist_array_ori);
var openvpn_unit = '<% nvram_get("vpn_server_unit"); %>';
var vpn_server_mode = 'openvpn';	// Hardcoded for this page, as we support both simultaneously
var openvpn_eas = '<% nvram_get("vpn_serverx_start"); %>';
var vpn_server_enable = (openvpn_eas.indexOf(''+(openvpn_unit)) >= 0) ? "1" : "0";

var service_state = "";
if (openvpn_unit == '1')
	service_state = '<% nvram_get("vpn_server1_state"); %>';
else if (openvpn_unit == '2')
	service_state = '<% nvram_get("vpn_server2_state"); %>';
else
	service_state = false;	

var openvpnd_connected_clients = [];
var openvpn_clientlist_array = decodeURIComponent('<% nvram_char_to_ascii("", "vpn_server_ccd_val"); %>');
var ciphersarray = [
		["AES-128-CBC"],
		["AES-192-CBC"],
		["AES-256-CBC"],
		["BF-CBC"],
		["CAST5-CBC"],
		["CAMELLIA-128-CBC"],
		["CAMELLIA-192-CBC"],
		["CAMELLIA-256-CBC"],
		["DES-CBC"],
		["DES-EDE-CBC"],
		["DES-EDE3-CBC"],
		["DESX-CBC"],
		["IDEA-CBC"],
		["RC2-40-CBC"],
		["RC2-64-CBC"],
		["RC2-CBC"],
		["RC5-CBC"],
		["SEED-CBC"]
];

function initial(){
	var currentcipher = "<% nvram_get("vpn_server_cipher"); %>";

	show_menu();		
	addOnlineHelp(document.getElementById("faq"), ["ASUSWRT", "VPN"]);

	formShowAndHide(vpn_server_enable, vpn_server_mode);
	//check DUT is belong to private IP.

	if(realip_support){
		if(!external_ip){
			document.getElementById("privateIP_notes").innerHTML = "<#vpn_privateIP_hint#>"
			document.getElementById("privateIP_notes").style.display = "";			
		}
	}
	else if(validator.isPrivateIP(wanlink_ipaddr())){	
		document.getElementById("privateIP_notes").innerHTML = "<#vpn_privateIP_hint#>"
		document.getElementById("privateIP_notes").style.display = "";
	}

	/*Advanced Setting start */
	allowed_openvpn_clientlist();
	
	//generate select option of cipher list
	add_option(document.form.vpn_server_cipher, "Default","default",(currentcipher == "default"));
	add_option(document.form.vpn_server_cipher, "None","none",(currentcipher == "none"));	
	for(var i = 0; i < ciphersarray.length; i += 1){
		add_option(document.form.vpn_server_cipher, ciphersarray[i][0], ciphersarray[i][0], (currentcipher == ciphersarray[i][0]));
	}
	// We don't use the global switch, so set it to current instance state instead
	document.form.VPNServer_enable.value = vpn_server_enable;
	// Set this based on a compound field
	setRadioValue(document.form.vpn_server_x_dns, ((document.form.vpn_serverx_dns.value.indexOf(''+(openvpn_unit)) >= 0) ? "1" : "0"));

	updateCRTValue();
	update_visibility();

	/*Advanced Setting end */
}

function formShowAndHide(server_enable, server_type) {
	if(server_enable == 1){
		document.getElementById("trVPNServerMode").style.display = "";
		document.getElementById("selSwitchMode").value = "1";
		document.getElementById('openvpn_export').style.display = "";	
		document.getElementById('OpenVPN_setting').style.display = "";
		document.getElementById("divAdvanced").style.display = "none";
		if(vpn_server_enable == '0' || vpn_server_mode == 'pptpd')
			document.getElementById('openvpn_export').style.display = "none";
		else
			document.getElementById('openvpn_export').style.display = "";	

		if(service_state == false || service_state != '2')
			document.getElementById('export_div').style.display = "none";

		if(!email_support)
			document.getElementById('exportViaEmail').style.display = "none";
					
		showopenvpnd_clientlist();
		update_vpn_client_state();
		openvpnd_connected_status();
		check_vpn_server_state();
		document.getElementById("divApply").style.display = "";
	}
	else{
		document.getElementById("trVPNServerMode").style.display = "none";
		document.getElementById("openvpn_export").style.display = "none";
		document.getElementById("OpenVPN_setting").style.display = "none";
		document.getElementById("divAdvanced").style.display = "none";
		if(vpn_server_mode != "openvpn") {
			document.getElementById("divApply").style.display = "none";
		}
	}
}

function openvpnd_connected_status(){
	var rule_num = document.getElementById("openvpnd_clientlist_table").rows.length;
	var username_status = "";
	for(var x=0; x < rule_num; x++){
		var ind = x;
		username_status = "conn"+ind;
		if(openvpnd_connected_clients.length >0){
			if(document.getElementById(username_status)) {
				document.getElementById(username_status).innerHTML = '<#Disconnected#>';
			}
			for(var y=0; y<openvpnd_connected_clients.length; y++){
				if(document.getElementById("openvpnd_clientlist_table").rows[x].cells[1].title == openvpnd_connected_clients[y].username){
					document.getElementById(username_status).innerHTML = '<a class="hintstyle2" href="javascript:void(0);" onClick="showOpenVPNClients(\''+openvpnd_connected_clients[y].username+'\');"><#Connected#></a>';
					break;
				}		
			}
		}else if(document.getElementById(username_status)){
			document.getElementById(username_status).innerHTML = '<#Disconnected#>';
		}	
	}
}

function applyRule(){
	var confirmFlag = true;

	/* Advanced setting start */
	var check_openvpn_conflict = function () {		//if conflict with LAN ip & DHCP ip pool & static
		var origin_lan_ip = '<% nvram_get("lan_ipaddr"); %>';
		var lan_ip_subnet = origin_lan_ip.split(".")[0]+"."+origin_lan_ip.split(".")[1]+"."+origin_lan_ip.split(".")[2]+".";
		var lan_ip_end = parseInt(origin_lan_ip.split(".")[3]);
		var pool_start = '<% nvram_get("dhcp_start"); %>';
		var pool_end = '<% nvram_get("dhcp_end"); %>';
		var dhcp_staticlists = '<% nvram_get("dhcp_staticlist"); %>';
		var staticclist_row = dhcp_staticlists.split('&#60');
		var netmask_obj = document.form.vpn_server_nm;
		var vpnSubnet = document.form.vpn_server_sn;
		var pool_start = '<% nvram_get("dhcp_start"); %>';
		var pool_subnet = pool_start.split(".")[0]+"."+pool_start.split(".")[1]+"."+pool_start.split(".")[2]+".";

		if(document.form.vpn_server_if.value == 'tun'){ 	
			if(vpnSubnet.value == ""){
				alert("<#JS_fieldblank#>");
				vpnSubnet.focus();
				vpnSubnet.select();
				return false;
			}
			
			if(!validator.ipRange(vpnSubnet, "")){
				vpnSubnet.focus();
				vpnSubnet.select();
				return false;		
			}
				
			var openvpn_server_subnet = vpnSubnet.value.split(".")[0] 
								+ "." + vpnSubnet.value.split(".")[1] 
								+ "." + vpnSubnet.value.split(".")[2] 
								+ ".";

			//LAN ip
			if(origin_lan_ip == vpnSubnet.value) {
				alert("<#vpn_conflict_LANIP#> " + origin_lan_ip);
				vpnSubnet.focus();
				vpnSubnet.select();
				return false;
			}

			//LAN ip pool
			if(lan_ip_subnet == openvpn_server_subnet) {
				alert("<#vpn_conflict_DHCPpool#>"+pool_start+" ~ "+pool_end);
				vpnSubnet.focus();
				vpnSubnet.select();
				return false;
			}
		
			//mask must be 255.255.0.0 ~ 255.255.255.248 openvpn limiting
			if(!validator.maskRange("255.255.0.0", "255.255.255.248", netmask_obj.value)) {
				alert("Netmask range must be 255.255.0.0 (/16) ~ 255.255.255.248 (/29)");
				netmask_obj.focus();
				netmask_obj.select();
				return false;
			}
			
			//check subnet/netmask combination whether it invalid or not
			
			if(!validator.subnetAndMaskCombination(vpnSubnet.value, netmask_obj.value)) {
				alert(vpnSubnet.value + " / " + netmask_obj.value + " combination is invalid");
				vpnSubnet.focus();
				vpnSubnet.select();
				return false;
			}
		}
		else if(document.form.vpn_server_if.value == 'tap' && document.form.vpn_server_dhcp.value == '0'){
			if(!validator.isLegalIP(document.form.vpn_server_r1, "")){		
				document.form.vpn_server_r1.focus();
				document.form.vpn_server_r1.select();
				return false;		
			}
			
			if(document.form.vpn_server_r1.value.split(".")[3] == 255){		//*.*.*.255 can't be IP in the IP pool
				alert(document.form.vpn_server_r1.value + " <#JS_validip#>");
				document.form.vpn_server_r1.focus();
				document.form.vpn_server_r1.select();
				return false;
			}
			
			if(!validator.isLegalIP(document.form.vpn_server_r2, "")){
				document.form.vpn_server_r2.focus();
				document.form.vpn_server_r2.select();
				return false;		
			}
			
			if(document.form.vpn_server_r2.value.split(".")[3] == 255){		//*.*.*.255 can't be IP in the IP pool
				alert(document.form.vpn_server_r2.value + " <#JS_validip#>");
				document.form.vpn_server_r2.focus();
				document.form.vpn_server_r2.select();
				return false;	
			}
			
			var openvpn_clients_start_subnet = document.form.vpn_server_r1.value.split(".")[0] + "." 
											 + document.form.vpn_server_r1.value.split(".")[1] + "." 
											 + document.form.vpn_server_r1.value.split(".")[2] + ".";
			var openvpn_clients_end_subnet = document.form.vpn_server_r2.value.split(".")[0] + "." 
										   + document.form.vpn_server_r2.value.split(".")[1] + "." 
										   + document.form.vpn_server_r2.value.split(".")[2] + ".";
			var openvpn_clients_start_ip = parseInt(document.form.vpn_server_r1.value.split(".")[3]);
			var openvpn_clients_end_ip = parseInt(document.form.vpn_server_r2.value.split(".")[3]);

			//LAN ip
			if( (lan_ip_subnet == openvpn_clients_start_subnet || lan_ip_subnet == openvpn_clients_end_subnet) 
			 && (lan_ip_end >= openvpn_clients_start_ip && lan_ip_end <= openvpn_clients_end_ip)) {			
				alert("<#vpn_conflict_LANIP#> "+origin_lan_ip);
				document.form.vpn_server_r1.focus();
				document.form.vpn_server_r1.select();
				return false;
			}
			
			if(openvpn_clients_end_ip < openvpn_clients_start_ip){
				alert(document.form.vpn_server_r2.value + " <#JS_validip#>");
				document.form.vpn_server_r2.focus();
				document.form.vpn_server_r2.select();
				return false;
			}

			//DHCP pool
			if(pool_subnet != openvpn_clients_start_subnet) {
				alert(document.form.vpn_server_r1.value + " <#JS_validip#>");
				document.form.vpn_server_r1.focus();
				document.form.vpn_server_r1.select();
				return false;
			}
			
			if(pool_subnet != openvpn_clients_end_subnet) {
				alert(document.form.vpn_server_r2.value + " <#JS_validip#>");
				document.form.vpn_server_r2.focus();
				document.form.vpn_server_r2.select();
				return false;
			}

			//DHCP static IP
			if(dhcp_staticlists != "") {
				for(var i = 1; i < staticclist_row.length; i +=1 ) {
					var static_ip = staticclist_row[i].split('&#62')[1];
					var static_subnet = static_ip.split(".")[0]+"."+static_ip.split(".")[1]+"."+static_ip.split(".")[2]+".";
					var static_end = parseInt(static_ip.split(".")[3]);					
					if(static_subnet != openvpn_clients_start_subnet) {
						alert(document.form.vpn_server_r1.value + " <#JS_validip#>");
						document.form.vpn_server_r1.focus();
						document.form.vpn_server_r1.select();
						return false;
					}
					if(static_subnet != openvpn_clients_end_subnet) {
						alert(document.form.vpn_server_r2.value + " <#JS_validip#>");
						document.form.vpn_server_r2.focus();
						document.form.vpn_server_r2.select();
						return false;
					}				
				}
			}
		}
		
		return true;
	};

	/* Advanced setting end */
	if(confirmFlag && check_openvpn_conflict() ) {
		document.openvpnTLSKeyForm.vpn_crt_server1_ca.disabled = true;
		document.openvpnTLSKeyForm.vpn_crt_server1_crt.disabled = true;
		document.openvpnTLSKeyForm.vpn_crt_server1_key.disabled = true;
		document.openvpnTLSKeyForm.vpn_crt_server1_dh.disabled = true;
		document.openvpnTLSKeyForm.vpn_crt_server1_crl.disabled = true;
		document.openvpnTLSKeyForm.vpn_crt_server1_extra.disabled = true;
		document.openvpnTLSKeyForm.vpn_crt_server1_static.disabled = true;
		document.openvpnTLSKeyForm.vpn_crt_server2_ca.disabled = true;
		document.openvpnTLSKeyForm.vpn_crt_server2_crt.disabled = true;
		document.openvpnTLSKeyForm.vpn_crt_server2_key.disabled = true;
		document.openvpnTLSKeyForm.vpn_crt_server2_dh.disabled = true;
		document.openvpnTLSKeyForm.vpn_crt_server2_crl.disabled = true;
		document.openvpnTLSKeyForm.vpn_crt_server2_static.disabled = true;
		document.openvpnTLSKeyForm.vpn_crt_server2_extra.disabled = true;

		var get_group_value = function () {
			var rule_num = document.getElementById("openvpnd_clientlist_table").rows.length;
			var item_num = document.getElementById("openvpnd_clientlist_table").rows[0].cells.length;
			var tmp_value = "";	

			for(var i = 1; i < rule_num; i += 1) {
				tmp_value += "<"		
				for(var j = 1; j < item_num - 1; j += 1) {
					if(document.getElementById("openvpnd_clientlist_table").rows[i].cells[j].innerHTML.lastIndexOf("...") < 0) {
						tmp_value += document.getElementById("openvpnd_clientlist_table").rows[i].cells[j].innerHTML;
					}
					else {
						tmp_value += document.getElementById("openvpnd_clientlist_table").rows[i].cells[j].title;
					}					
					if(j != item_num - 2)
						tmp_value += ">";
				}
			}
			if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
				tmp_value = "";
						
			return tmp_value;
		};

	if (service_state) {
		document.form.action_script.value = "restart_vpnserver" + openvpn_unit;
	}
		if(document.form.VPNServer_enable.value == "1") {
			document.form.VPNServer_mode.value = 'openvpn';
			document.form.action_script.value = "restart_chpass;restart_vpnserver" + openvpn_unit;
			document.form.vpn_serverx_clientlist.value = get_group_value();
			/* Advanced setting start */
			//Viz add 2014.06
			if(document.getElementById("server_reneg").style.display == "none")
				document.form.vpn_server_reneg.disabled = true;

			var getAdvancedValue = function () {
				var client_num = document.getElementById("openvpn_clientlist_table").rows.length;
				var item_num = document.getElementById("openvpn_clientlist_table").rows[0].cells.length;
				var tmp_value = "";		
				for(var i = 0; i < client_num; i += 1) {
					// Insert first field (1), which enables the client.
					tmp_value += "<1>";
					for(var j = 0; j < item_num - 1; j += 1) {
						if (j == 3)
							tmp_value += (document.getElementById("openvpn_clientlist_table").rows[i].cells[j].innerHTML == "Yes" ? 1 : 0);
						else
							tmp_value += document.getElementById("openvpn_clientlist_table").rows[i].cells[j].innerHTML;

						if(j != item_num - 2)
							tmp_value += ">";
					}
				}
				
				if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<1>")
					tmp_value = "";

				document.form.vpn_server_ccd_val.value = tmp_value;
				
				tmp_value = "";
				for (var i = 1; i < 3; i += 1) {
					if (i == openvpn_unit) {
						if (getRadioValue(document.form.vpn_server_x_dns) == 1)
							tmp_value += ""+i+",";
					} else {
						if (document.form.vpn_serverx_dns.value.indexOf(''+(i)) >= 0)
							tmp_value += ""+i+","
					}
				}	

				if (tmp_value != document.form.vpn_serverx_dns.value) {
					document.form.action_script.value += ";restart_dnsmasq";
					document.form.vpn_serverx_dns.value = tmp_value;
				}
			}();
			/* Advanced setting end */	
		}
		else {		//disable server
			document.form.action_script.value = "stop_vpnserver" + openvpn_unit;
			document.form.vpn_serverx_clientlist.value = get_group_value();
		}	
		
		showLoading();
		document.form.submit();
	}
}

function addRow(obj, head){
	if(head == 1)
		vpn_server_clientlist_array += "<" /*&#60*/
	else
		vpn_server_clientlist_array += ">" /*&#62*/

	vpn_server_clientlist_array += obj.value;
	obj.value = "";
}

function validForm(){
	var valid_username = document.form.vpn_server_clientlist_username;
	var valid_password = document.form.vpn_server_clientlist_password;

	if(valid_username.value == "") {
		alert("<#JS_fieldblank#>");
		valid_username.focus();
		return false;		
	}
	else if(!Block_chars(valid_username, [" ", "@", "*", "+", "|", ":", "?", "<", ">", ",", ".", "/", ";", "[", "]", "\\", "=", "\"", "&" ])) {
		return false;		
	}

	if(valid_password.value == "") {
		alert("<#JS_fieldblank#>");
		valid_password.focus();
		return false;
	}
	else if(!Block_chars(valid_password, ["<", ">", "&"])) {
		return false;		
	}
		
	return true;
}

function addRow_Group(upper){
	var username_obj = document.form.vpn_server_clientlist_username;
	var password_obj = document.form.vpn_server_clientlist_password;
	var rule_num = document.getElementById("openvpnd_clientlist_table").rows.length;
	var item_num = document.getElementById("openvpnd_clientlist_table").rows[0].cells.length;		
	if(rule_num >= upper + 1) {
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}

	if(validForm()){
		//Viz check same rule  //match(username) is not accepted
		if(item_num >= 2) {
			for(var i = 0; i < rule_num; i += 1) {
				if(username_obj.value == document.getElementById("openvpnd_clientlist_table").rows[i].cells[1].title) {
					alert("<#JS_duplicate#>");
					username_obj.focus();
					username_obj.select();
					return false;
				}	
			}
		}
		
		addRow(username_obj ,1);
		addRow(password_obj, 0);
		showopenvpnd_clientlist();
		openvpnd_connected_status();
	}
}

function del_Row(rowdata){
	var i = rowdata.parentNode.parentNode.rowIndex;
	document.getElementById("openvpnd_clientlist_table").deleteRow(i);
  	var vpn_server_clientlist_value = "";
  	var rowLength = document.getElementById("openvpnd_clientlist_table").rows.length;
	for(var k = 1; k < rowLength; k += 1){
		vpn_server_clientlist_value += "<";
		vpn_server_clientlist_value += document.getElementById("openvpnd_clientlist_table").rows[k].cells[1].innerHTML;
		vpn_server_clientlist_value += ">";
		vpn_server_clientlist_value += document.getElementById("openvpnd_clientlist_table").rows[k].cells[2].innerHTML;
	}

	vpn_server_clientlist_array = vpn_server_clientlist_value;
	if(vpn_server_clientlist_array == "") {
		showopenvpnd_clientlist();
		openvpnd_connected_status();
	}
}

var overlib_str2 = new Array();	//Viz add 2013.10 for record longer VPN client username/pwd for OpenVPN
var overlib_str3 = new Array();	//Viz add 2013.10 for record longer VPN client username/pwd for OpenVPN
function showopenvpnd_clientlist(){
	var vpn_server_clientlist_row = vpn_server_clientlist_array.split('<');
	var code = "";
	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="openvpnd_clientlist_table">';
	code +='<tr id="row0"><td width="15%" id="conn0"></td><td width="35%" title="<% nvram_get("http_username"); %>"><% nvram_get("http_username"); %></td><td width="35%" style="text-align:center;">-</td><td width="15%" style="text-align:center;">-</td></tr>';
	if(vpn_server_clientlist_row.length > 1){
		for(var i = 1; i < vpn_server_clientlist_row.length; i++){
			overlib_str2[i] = "";
			overlib_str3[i] = "";
			code +='<tr id="row'+i+'">';
			var vpn_server_clientlist_col = vpn_server_clientlist_row[i].split('>');
			code +='<td width="15%" id="conn'+i+'"></td>';
			for(var j = 0; j < vpn_server_clientlist_col.length; j++){
				if(j == 0){
					if(vpn_server_clientlist_col[0].length >32){
						overlib_str2[i] += vpn_server_clientlist_col[0];
						vpn_server_clientlist_col[0] = vpn_server_clientlist_col[0].substring(0, 30)+"...";
						code +='<td width="35%" title="'+overlib_str2[i]+'">'+ vpn_server_clientlist_col[0] +'</td>';
					}else{
						code +='<td width="35%" title="'+vpn_server_clientlist_col[0]+'">'+ vpn_server_clientlist_col[0] +'</td>';
					}	
				}
				else if(j ==1){
					if(vpn_server_clientlist_col[1].length >32){
						overlib_str3[i] += vpn_server_clientlist_col[1];
						vpn_server_clientlist_col[1] = vpn_server_clientlist_col[1].substring(0, 30)+"...";
						code +='<td width="35%" title="'+overlib_str3[i]+'">'+ vpn_server_clientlist_col[1] +'</td>';
					}else{
						code +='<td width="35%">'+ vpn_server_clientlist_col[1] +'</td>';
					}
				} 
			}
			
			code +='<td width="15%">';
			code +='<input class="remove_btn" onclick="del_Row(this, \'openvpnd\');" value=""/></td></tr>';
		}
	}
	
	code +='</table>';
	document.getElementById("openvpnd_clientlist_Block").innerHTML = code;
}

function parseOpenVPNClients(client_status){		//192.168.123.82:46954 10.8.0.6 pine\n	
	openvpnd_connected_clients = [];
	var Loginfo = client_status;
	if (Loginfo == "") {return;}

	Loginfo = Loginfo.replace('\r\n', '\n');
	Loginfo = Loginfo.replace('\n\r', '\n');
	Loginfo = Loginfo.replace('\r', '\n');
	
	var lines = Loginfo.split('\n');
	for (i = 0; i < lines.length; i++){
		var fields = lines[i].split(' ');
		if ( fields.length != 3 ) continue;
		openvpnd_connected_clients.push({"username":fields[2],"remoteIP":fields[0],"VPNIP":fields[1]});
	}		
}

function showOpenVPNClients(uname){
	var statusmenu = "";
	var statustitle = "";
	statustitle += "<div style=\"text-decoration:underline;\">VPN IP ( Remote IP )</div>";
	_caption = statustitle;
	
	for (i = 0; i < openvpnd_connected_clients.length; i++){
		if(uname == openvpnd_connected_clients[i].username){		
				statusmenu += "<div>"+openvpnd_connected_clients[i].VPNIP+" \t( "+openvpnd_connected_clients[i].remoteIP+" )</div>";
		}				
	}
	
	return overlib(statusmenu, WIDTH, 260, OFFSETX, -360, LEFT, STICKY, CAPTION, _caption, CLOSETITLE, '');	
}

function check_vpn_server_state(){
	if(vpn_server_enable == '1' && vpn_server_mode == 'openvpn' && service_state != '2'){
		document.getElementById('export_div').style.display = "none";
		document.getElementById('openvpn_initial').style.display = "";
		update_vpn_server_state();
	}		
}

function update_vpn_server_state() {
	$.ajax({
		url: '/ajax_openvpn_server.asp',
		dataType: 'script',

		error: function(xhr) {
			setTimeout("update_vpn_server_state();", 1000);
		},

		success: function() {
			if(vpnd_state != '2' && (vpnd_errno == '1' || vpnd_errno == '2')){
				document.getElementById('openvpn_initial').style.display = "none";    						
				document.getElementById('openvpn_error_message').innerHTML = "<span><#vpn_openvpn_fail1#></span>";
				document.getElementById('openvpn_error_message').style.display = "";
			}
			else if(vpnd_state != '2' && vpnd_errno == '4'){
				document.getElementById('openvpn_initial').style.display = "none";
				document.getElementById('openvpn_error_message').innerHTML = "<span><#vpn_openvpn_fail2#></span>";
				document.getElementById('openvpn_error_message').style.display = "";
			}
			else if(vpnd_state != '2' && vpnd_errno == '5'){
				document.getElementById('openvpn_initial').style.display = "none";
				document.getElementById('openvpn_error_message').innerHTML = "<span><#vpn_openvpn_fail3#></span>";
				document.getElementById('openvpn_error_message').style.display = "";
			}
			else if(vpnd_state == '-1' && vpnd_errno == '0'){
				document.getElementById('openvpn_initial').style.display = "none";
				document.getElementById('openvpn_error_message').innerHTML = "<span><#vpn_openvpn_fail4#></span>";
				document.getElementById('openvpn_error_message').style.display = "";
			}
			else if(vpnd_state != '2'){
				setTimeout("update_vpn_server_state();", 1000);
			}
			else{	// OpenVPN server ready , vpn_server1_state==2
				setTimeout("location.href='Advanced_VPN_OpenVPN.asp';", 1000);
				return;
			}
		}
	});	
}

function showMailPanel(){
	var checker = {
		server: document.mailConfigForm.PM_SMTP_SERVER.value,
		mailPort: document.mailConfigForm.PM_SMTP_PORT.value,
		user: document.mailConfigForm.PM_SMTP_AUTH_USER.value,
		pass: document.mailConfigForm.PM_SMTP_AUTH_PASS.value,
		end: 0
	}

	if(checker.server == "" || checker.mailPort == "" || checker.user == "" || checker.pass == ""){
		$("#mailConfigPanelContainer").fadeIn(300);
		$("#mailSendPanelContainer").fadeOut(300);
	}
	else{
		$("#mailConfigPanelContainer").fadeOut(300);
		$("#mailSendPanelContainer").fadeIn(300);
	}
}

function switchMode(mode){
	if(mode == "1"){		//general setting
		document.getElementById("OpenVPN_setting").style.display = "";
		if(vpn_server_enable == "1" && vpn_server_mode == "openvpn"){
			document.getElementById("openvpn_export").style.display = "";
		}
		else{
			document.getElementById("openvpn_export").style.display = "none";
		}
		
		document.getElementById("divAdvanced").style.display = "none";
	}	
	else{
		document.getElementById("OpenVPN_setting").style.display = "none";
		document.getElementById("openvpn_export").style.display = "none";
		document.getElementById("divAdvanced").style.display = "";
	}
}

function enable_openvpn(state){
	var tmp_value = "";

	for (var i=1; i < 3; i++) {
		if (i == openvpn_unit) {
			if (state == 1)
				tmp_value += ""+i+",";
		} else {
			if (document.form.vpn_serverx_start.value.indexOf(''+(i)) >= 0)
				tmp_value += ""+i+","
		}
	}
	document.form.vpn_serverx_start.value = tmp_value;
}

/* Advanced Setting start */ 
function change_vpn_unit(val){
	FormActions("apply.cgi", "change_vpn_server_unit", "", "");
	document.form.target = "";
	document.form.submit();
}

function update_visibility(){
	var auth = document.form.vpn_server_crypt.value;
	var iface = document.form.vpn_server_if.value;
	var hmac = document.form.vpn_server_hmac.value;
	userpass = getRadioValue(document.form.vpn_server_userpass_auth);
	var dhcp = getRadioValue(document.form.vpn_server_dhcp);
	var dns = getRadioValue(document.form.vpn_server_x_dns);
	if(auth != "tls")
		ccd = 0;
	else
		ccd = getRadioValue(document.form.vpn_server_ccd);
		
	showhide("server_authhmac", (auth != "secret"));
	showhide("server_snnm", ((auth == "tls") && (iface == "tun")));
	showhide("server_plan", ((auth == "tls") && (iface == "tun")));
	showhide("server_local", ((auth == "secret") && (iface == "tun")));
	showhide("server_reneg", (auth != "secret"));		//add by Viz 2014.06
	showhide("server_ccd", (auth == "tls"));
	showhide("server_c2c", ccd);
	showhide("server_ccd_excl", ccd);
	showhide("openvpn_client_table", ccd);
	showhide("openvpn_clientlist_Block", ccd);	
	showhide("server_pdns", ((auth == "tls") && (dns == 1)));
	showhide("server_dhcp",((auth == "tls") && (iface == "tap")));
	showhide("server_range", ((dhcp == 0) && (auth == "tls") && (iface == "tap")));
	showhide("server_tls_crypto_text", ((auth == "tls") || (auth == "secret")));		//add by Viz
	showhide("server_custom_crypto_text", (auth == "custom"));
	showhide("server_igncrt", (userpass == 1));
}

function set_Keys() {
	cal_panel_block("tlsKey_panel", 0.15);
	$("#tlsKey_panel").fadeIn(300);
}

function updateCRTValue(){
	$.ajax({
		url: '/ajax_openvpn_server.asp',
		dataType: 'script',
		timeout: 1500,
		error: function(xhr){
			setTimeout("updateCRTValue();",1000);
		},
		success: function(){
				if (openvpn_unit == 1) {
					document.openvpnTLSKeyForm.edit_vpn_crt_server_ca.value = vpn_crt_server1_ca[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
					document.openvpnTLSKeyForm.edit_vpn_crt_server_crt.value = vpn_crt_server1_crt[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
					document.openvpnTLSKeyForm.edit_vpn_crt_server_key.value = vpn_crt_server1_key[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
					document.openvpnTLSKeyForm.edit_vpn_crt_server_dh.value = vpn_crt_server1_dh[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
					document.openvpnTLSKeyForm.edit_vpn_crt_server_crl.value = vpn_crt_server1_crl[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
					document.openvpnTLSKeyForm.edit_vpn_crt_server_static.value = vpn_crt_server1_static[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
					document.openvpnTLSKeyForm.edit_vpn_crt_server_extra.value = vpn_crt_server1_extra[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
				} else if (openvpn_unit == 2) {
					document.openvpnTLSKeyForm.edit_vpn_crt_server_ca.value = vpn_crt_server2_ca[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
					document.openvpnTLSKeyForm.edit_vpn_crt_server_crt.value = vpn_crt_server2_crt[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
					document.openvpnTLSKeyForm.edit_vpn_crt_server_key.value = vpn_crt_server2_key[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
					document.openvpnTLSKeyForm.edit_vpn_crt_server_dh.value = vpn_crt_server2_dh[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
					document.openvpnTLSKeyForm.edit_vpn_crt_server_crl.value = vpn_crt_server2_crl[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
					document.openvpnTLSKeyForm.edit_vpn_crt_server_static.value = vpn_crt_server2_static[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
					document.openvpnTLSKeyForm.edit_vpn_crt_server_extra.value = vpn_crt_server2_extra[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
				}
		}
	})
}

function addRow_Group_Advanced(upper){
	var client_num = document.getElementById("openvpn_clientlist_table").rows.length;
	var item_num = document.getElementById("openvpn_clientlist_table").rows[0].cells.length;
	if(client_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;
	}

	if(document.form.vpn_clientlist_commonname_0.value==""){
		alert("<#JS_fieldblank#>");
		document.form.vpn_clientlist_commonname_0.focus();
		document.form.vpn_clientlist_commonname_0.select();
		return false;
	}
	
	if(document.form.vpn_clientlist_subnet_0.value==""){
		alert("<#JS_fieldblank#>");
		document.form.vpn_clientlist_subnet_0.focus();
		document.form.vpn_clientlist_subnet_0.select();
		return false;		
	}
	
	if(document.form.vpn_clientlist_netmask_0.value==""){
		alert("<#JS_fieldblank#>");
		document.form.vpn_clientlist_netmask_0.focus();
		document.form.vpn_clientlist_netmask_0.select();
		return false;		
	}

// Check for duplicate
	if(item_num >=2){
		for(i=0; i<client_num; i++){
			if(document.form.vpn_clientlist_commonname_0.value.toLowerCase() == document.getElementById("openvpn_clientlist_table").rows[i].cells[0].innerHTML.toLowerCase()
			&& document.form.vpn_clientlist_subnet_0.value == document.getElementById("openvpn_clientlist_table").rows[i].cells[1].innerHTML
			&& document.form.vpn_clientlist_netmask_0.value == document.getElementById("openvpn_clientlist_table").rows[i].cells[2].innerHTML){
				alert('<#JS_duplicate#>');
				document.form.vpn_clientlist_commonname_0.focus();
				document.form.vpn_clientlist_commonname_0.select();
				return false;
			}
		}
	}
	
	do_addRow_Group();
}

function do_addRow_Group(){
	addRowAdvanced(document.form.vpn_clientlist_commonname_0 ,1);
	addRowAdvanced(document.form.vpn_clientlist_subnet_0, 0);
	addRowAdvanced(document.form.vpn_clientlist_netmask_0, 0);
	addRowAdvanced(document.form.vpn_clientlist_push_0, 0);
	document.form.vpn_clientlist_push_0.value="0"; //reset selection
	allowed_openvpn_clientlist();
}

function addRowAdvanced(obj, head){
	if(head == 1)
		openvpn_clientlist_array += "<1>";
	else
		openvpn_clientlist_array += ">";

	openvpn_clientlist_array += obj.value;
	obj.value = "";
}

function allowed_openvpn_clientlist(){
	var openvpn_clientlist_row = openvpn_clientlist_array.split('<');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="openvpn_clientlist_table">';
	if(openvpn_clientlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td>';
	else{
		for(var i = 1; i < openvpn_clientlist_row.length; i++){
			code +='<tr id="row'+i+'">';
			var openvpn_clientlist_col = openvpn_clientlist_row[i].split('>');
			var wid=[0, 36, 20, 20, 12];
				//skip field[0] as it contains "1" for "Enabled".
			for (var j = 1; j < openvpn_clientlist_col.length; j++){
				if (j == 4)
					code +='<td width="'+wid[j]+'%">'+ ((openvpn_clientlist_col[j] == 1 || openvpn_clientlist_col[j] == 'Yes') ? 'Yes' : 'No') +'</td>';
				else
					code +='<td width="'+wid[j]+'%">'+ openvpn_clientlist_col[j] +'</td>';

			}
			
			code +='<td width="12%">';
			code +='<input class="remove_btn" onclick="del_openvpnRow(this);" value=""/></td>';
		}
	}
	
	code +='</table>';
	document.getElementById("openvpn_clientlist_Block").innerHTML = code;
}

function del_openvpnRow(r) {
	var i = r.parentNode.parentNode.rowIndex;
	document.getElementById("openvpn_clientlist_table").deleteRow(i);

	var openvpn_clientlist_value = "";
	var rowLength = document.getElementById("openvpn_clientlist_table").rows.length;
	for(var k = 0; k < rowLength; k += 1) {
		for(var j = 0; j < document.getElementById("openvpn_clientlist_table").rows[k].cells.length - 1; j += 1){
			if(j == 0)
				openvpn_clientlist_value += "<1>";
			else
				openvpn_clientlist_value += ">";
			
			openvpn_clientlist_value += document.getElementById("openvpn_clientlist_table").rows[k].cells[j].innerHTML;
		}
	}

	openvpn_clientlist_array = openvpn_clientlist_value;
	if(openvpn_clientlist_array == "")
		allowed_openvpn_clientlist();
}

function cancel_Key_panel() {
	this.FromObject ="0";
	$("#tlsKey_panel").fadeOut(300);	
}


function save_keys() {
	if (openvpn_unit == "1") {
		document.openvpnTLSKeyForm.vpn_crt_server1_ca.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_ca.value;
		document.openvpnTLSKeyForm.vpn_crt_server1_crt.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_crt.value;
		document.openvpnTLSKeyForm.vpn_crt_server1_key.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_key.value;
		document.openvpnTLSKeyForm.vpn_crt_server1_dh.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_dh.value;
		document.openvpnTLSKeyForm.vpn_crt_server1_crl.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_crl.value;
		document.openvpnTLSKeyForm.vpn_crt_server1_static.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_static.value;
		document.openvpnTLSKeyForm.vpn_crt_server1_extra.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_extra.value;
		document.openvpnTLSKeyForm.vpn_crt_server1_static.disabled = false;
		document.openvpnTLSKeyForm.vpn_crt_server1_ca.disabled = false;
		document.openvpnTLSKeyForm.vpn_crt_server1_crt.disabled = false;
		document.openvpnTLSKeyForm.vpn_crt_server1_key.disabled = false;
		document.openvpnTLSKeyForm.vpn_crt_server1_dh.disabled = false;
		document.openvpnTLSKeyForm.vpn_crt_server1_crl.disabled = false;
		document.openvpnTLSKeyForm.vpn_crt_server1_extra.disabled = false;
	} else {
		document.openvpnTLSKeyForm.vpn_crt_server2_ca.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_ca.value;
		document.openvpnTLSKeyForm.vpn_crt_server2_crt.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_crt.value;
		document.openvpnTLSKeyForm.vpn_crt_server2_key.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_key.value;
		document.openvpnTLSKeyForm.vpn_crt_server2_dh.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_dh.value;
		document.openvpnTLSKeyForm.vpn_crt_server2_crl.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_crl.value;
		document.openvpnTLSKeyForm.vpn_crt_server2_static.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_static.value;
		document.openvpnTLSKeyForm.vpn_crt_server2_extra.value = document.openvpnTLSKeyForm.edit_vpn_crt_server_extra.value;
		document.openvpnTLSKeyForm.vpn_crt_server2_static.disabled = false;
		document.openvpnTLSKeyForm.vpn_crt_server2_ca.disabled = false;
		document.openvpnTLSKeyForm.vpn_crt_server2_crt.disabled = false;
		document.openvpnTLSKeyForm.vpn_crt_server2_key.disabled = false;
		document.openvpnTLSKeyForm.vpn_crt_server2_dh.disabled = false;
		document.openvpnTLSKeyForm.vpn_crt_server2_crl.disabled = false;
		document.openvpnTLSKeyForm.vpn_crt_server2_extra.disabled = false;
	}
	document.openvpnTLSKeyForm.submit();
	cancel_Key_panel();
}
/* Advanced Setting end */ 

function update_vpn_client_state() {
	$.ajax({
		url: '/ajax_openvpn_client_status.xml',
		dataType: 'xml',

		error: function(xml) {
			setTimeout("update_vpn_client_state();", 1000);
		},

		success: function(xml) {
			var vpnserverXML = xml.getElementsByTagName("vpnserver");
			var client_status = vpnserverXML[0].firstChild.nodeValue;
			parseOpenVPNClients(client_status);
			openvpnd_connected_status();
			setTimeout("update_vpn_client_state();", 3000);
		}
	});	
}
</script>
</head>
<body onload="initial();">
<div id="tlsKey_panel"  class="contentM_qis">
	<!--===================================Beginning of tls Content===========================================-->
	<table class="QISform_wireless" border=0 align="center" cellpadding="5" cellspacing="0">
		<form method="post" name="openvpnTLSKeyForm" action="/start_apply.htm" target="hidden_frame">
		<input type="hidden" name="current_page" value="Advanced_VPN_OpenVPN.asp">
		<input type="hidden" name="next_page" value="Advanced_VPN_OpenVPN.asp">
		<input type="hidden" name="modified" value="0">
		<input type="hidden" name="flag" value="background">
		<input type="hidden" name="action_mode" value="apply">
		<input type="hidden" name="action_script" value="saveNvram">
		<input type="hidden" name="action_wait" value="1">
		<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
		<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
		<input type="hidden" name="vpn_crt_server1_ca" value="" disabled>
		<input type="hidden" name="vpn_crt_server1_crt" value="" disabled>
		<input type="hidden" name="vpn_crt_server1_key" value="" disabled>
		<input type="hidden" name="vpn_crt_server1_dh" value="" disabled>
		<input type="hidden" name="vpn_crt_server1_crl" value="" disabled>
		<input type="hidden" name="vpn_crt_server1_extra" value="" disabled>
		<input type="hidden" name="vpn_crt_server2_ca" value="" disabled>
		<input type="hidden" name="vpn_crt_server2_crt" value="" disabled>
		<input type="hidden" name="vpn_crt_server2_key" value="" disabled>
		<input type="hidden" name="vpn_crt_server2_dh" value="" disabled>
		<input type="hidden" name="vpn_crt_server2_crl" value="" disabled>
		<input type="hidden" name="vpn_crt_server2_extra" value="" disabled>
		<input type="hidden" name="vpn_crt_server1_static" value="" disabled>
		<input type="hidden" name="vpn_crt_server2_static" value="" disabled>
		<tr>
			<div class="description_down"><#vpn_openvpn_Keys_Cert#></div>
		</tr>
		<tr>
			<div style="margin-left:30px; margin-top:10px;">
				<p><#vpn_openvpn_KC_Edit1#> <span style="color:#FFCC00;">----- BEGIN xxx ----- </span>/<span style="color:#FFCC00;"> ----- END xxx -----</span> <#vpn_openvpn_KC_Edit2#>
				<p>Limit: 3999 characters per field
			</div>
			<div style="margin:5px;*margin-left:-5px;"><img style="width: 730px; height: 2px;" src="/images/New_ui/export/line_export.png"></div>
		</tr>				
		<tr>
			<td valign="top">
				<table width="700px" border="0" cellpadding="4" cellspacing="0">
					<tbody>
					<tr>
						<td valign="top">
							<table width="100%" id="page1_tls" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable">
								<tr>
									<th><#vpn_openvpn_KC_StaticK#></th>
									<td>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server_static" name="edit_vpn_crt_server_static" cols="65" maxlength="3999"></textarea>
									</td>
								</tr>
								<tr id="edit_tls1">
									<th><#vpn_openvpn_KC_CA#></th>
									<td>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server_ca" name="edit_vpn_crt_server_ca" cols="65" maxlength="3999"></textarea>
									</td>
								</tr>
								<tr id="edit_tls2">
									<th><#vpn_openvpn_KC_SA#></th>
									<td>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server_crt" name="edit_vpn_crt_server_crt" cols="65" maxlength="3999"></textarea>
									</td>
								</tr>
								<tr id="edit_tls3">
									<th><#vpn_openvpn_KC_SK#></th>
									<td>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server_key" name="edit_vpn_crt_server_key" cols="65" maxlength="3999"></textarea>
									</td>
								</tr>
								<tr id="edit_tls4">
									<th><#vpn_openvpn_KC_DH#></th>
									<td>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server_dh" name="edit_vpn_crt_server_dh" cols="65" maxlength="3999"></textarea>
									</td>
								</tr>
								<tr id="edit_tls5">
									<th>Certificate Revocation List (Optional)</th>
									<td>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server_crl" name="edit_vpn_crt_server_crl" cols="65" maxlength="3999"></textarea>
									</td>
								</tr>
								<tr id="edit_tls6">
									<th>Extra Chain Certificates (Optional)</th>
										<td><textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server_extra" name="edit_vpn_crt_server_extra" cols="65" maxlength="3999"></textarea>
									</td>
								</tr>
							</table>
						</td>
					</tr>						
					</tbody>						
				</table>
				<div style="margin-top:5px;width:100%;text-align:center;">
					<input class="button_gen" type="button" onclick="cancel_Key_panel();" value="<#CTL_Cancel#>">
					<input class="button_gen" type="button" onclick="save_keys();" value="<#CTL_onlysave#>">	
				</div>					
			</td>
		</tr>
	</form>
	</table>
	<!--===================================Ending of tls Content===========================================-->			
</div>
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_VPN_OpenVPN.asp">
<input type="hidden" name="next_page" value="Advanced_VPN_OpenVPN.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="15">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="VPNServer_enable" value="<% nvram_get("VPNServer_enable"); %>">
<input type="hidden" name="VPNServer_mode" value="<% nvram_get("VPNServer_mode"); %>">
<input type="hidden" name="vpn_serverx_clientlist" value="">
<input type="hidden" name="vpn_serverx_start" value="<% nvram_get("vpn_serverx_start"); %>">
<input type="hidden" name="vpn_serverx_dns" value="<% nvram_get("vpn_serverx_dns"); %>">
<input type="hidden" name="vpn_server_ccd_val" value="">
<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="17">&nbsp;</td>		
		<td valign="top" width="202">				
			<div id="mainMenu"></div>	
			<div id="subMenu"></div>		
		</td>						
		<td valign="top">
			<div id="tabMenu" class="submenuBlock"></div>
			<!--===================================Beginning of Main Content===========================================-->
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td valign="top" >
						<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle" >
							<tbody>
							<tr>
								<td bgcolor="#4D595D" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle"><#BOP_isp_heart_item#> - OpenVPN</div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div id="privateIP_notes" class="formfontdesc" style="display:none;color:#FFCC00;"></div>
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
										<thead>
										<tr>
											<td colspan="2"><#t2BC#></td>
										</tr>
										</thead>				
										<tr>
											<th>Server instance</th>
											<td>
												<select id="openvpn_unit" name="vpn_server_unit" class="input_option" onChange="change_vpn_unit(this.value);">
													<option value="1" <% nvram_match("vpn_server_unit","1","selected"); %> >Server 1</option>
													<option value="2" <% nvram_match("vpn_server_unit","2","selected"); %> >Server 2</option>
												</select>
											</td>
										</tr>
										<tr>
											<th><#vpn_enable#></th>
											<td>
												<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_VPNServer_enable"></div>												
												<script type="text/javascript">													
													$('#radio_VPNServer_enable').iphoneSwitch(vpn_server_enable,
													function(){
														enable_openvpn(1);
														document.form.VPNServer_enable.value = "1";
														formShowAndHide(1, "openvpn");
													},
													function(){
														enable_openvpn(0);
														document.form.VPNServer_enable.value = "0";
														formShowAndHide(0, "openvpn");
													}
													);
												</script>			
											</td>			
										</tr>
										<tr id="trVPNServerMode">
											<th><#vpn_Adv#></th>
											<td>
												<select id="selSwitchMode" onchange="switchMode(this.options[this.selectedIndex].value)" class="input_option">
													<option value="1" selected><#menu5_1_1#></option>
													<option value="2"><#menu5#></option>									
												</select>		
											</td>											
										</tr>
										<tr id="openvpn_export" style="display:none;">
											<th><#vpn_export_ovpnfile#></th>
											<td>
												<div id="export_div">
													<input id="exportToLocal" class="button_gen" type="button" value="<#btn_Export#>" />
													<input id="exportViaEmail" class="button_gen" type="button" value="via Email" style="display:none;"/><!-- Viz hide it temp 2014.06 -->
												</div>
												<script type="text/javascript">
													document.getElementById("exportToLocal").onclick = function(){
													location.href = 'client<% nvram_get("vpn_server_unit"); %>.ovpn';
													}
													document.getElementById("exportViaEmail").onclick = function(){
														showMailPanel();
													}	
												</script>
              									<div id="openvpn_initial" style="display:none;margin-left:5px;">
              										<span>              												
              											 <#vpn_openvpn_init#>
              											 <img src="images/InternetScan.gif" />
              										</span>              											
              									</div>
              									<div id="openvpn_error_message" style="display:none;margin-left:5px;"></div>	
            								</td>
          								</tr>
									</table>
									<div id="OpenVPN_setting" style="display:none;margin-top:8px;">
										<div class="formfontdesc">
											<#vpn_openvpn_desc1#>&nbsp;<#vpn_openvpn_desc3#>&nbsp;<#vpn_openvpn_desc2#> <#menu5#>
											<br />
											<ol>
												<li><a href="http://www.asus.com/support/Knowledge-Detail/11/2/RTAC68U/1A935B95-C237-4281-AE86-C824737D11F9/" target="_blank" style="text-decoration:underline;">Windows</a>
												<li><a href="http://www.asus.com/support/Knowledge-Detail/11/2/RTAC68U/C77ADCBF-F5C4-46B4-8A0D-B64F09AB881F/" target="_blank" style="text-decoration:underline;">Mac OS</a>
												<li><a href="http://www.asus.com/support/Knowledge-Detail/11/2/RTAC68U/37EC8F08-3F50-4F82-807E-6D2DCFE5146A/" target="_blank" style="text-decoration:underline;">iPhone/iPad</a>
												<li><a href="http://www.asus.com/support/Knowledge-Detail/11/2/RTAC68U/8DCA7DA6-E5A0-40C2-8AED-B9361E89C844/" target="_blank" style="text-decoration:underline;">Android</a>
											<ol>
										</div>										
										<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;">
											<thead>
											<tr>
												<td colspan="4"><#Username_Pwd#>&nbsp;(<#List_limit#>&nbsp;16)</td>
											</tr>
											</thead>								
											<tr>
												<th><#PPPConnection_x_WANLink_itemname#></th>
												<th><#HSDPAConfig_Username_itemname#></th>
												<th><#HSDPAConfig_Password_itemname#></th>
												<th><#list_add_delete#></th>
											</tr>			  
											<tr>
												<td width="15%" style="text-align:center;">-
												</td>
												<td width="35%">
													<input type="text" class="input_25_table" maxlength="64" name="vpn_server_clientlist_username" onKeyPress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off">
												</td>
												<td width="35%">
													<input type="text" class="input_25_table" maxlength="64" name="vpn_server_clientlist_password" onKeyPress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off">
												</td>
												<td width="15%">
													<div><input type="button" class="add_btn" onClick="addRow_Group(16);" value=""></div>
												</td>
											</tr>	 			  
										</table>        
														
										<div id="openvpnd_clientlist_Block"></div>	
									</div>

									<div id="divAdvanced" style="display:none;margin-top:8px;">
										<div class="formfontdesc">
											<p><#vpn_openvpn_desc3#><br />
											<p><#vpn_openvpn_hint1#><br />
											<p><#vpn_openvpn_hint2#><br />
											<p><#vpn_openvpn_hint3#>
										</div>
										<!-- Advanced setting table start-->
										<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px;">
											<thead>
											<tr>
												<td colspan="2"><#menu5#></td>
											</tr>
											</thead>
											<tr style="display:none;">
												<th>Service state</th>
												<td>
													<div class="left" style="width:94px; float:left; cursor:pointer;" id="radio_service_enable"></div>
													<script type="text/javascript">
														if (openvpn_unit == '1')
															var service_state_advanced = (<% sysinfo("pid.vpnserver1"); %> > 0);
														else if (openvpn_unit == '2')
															var service_state_advanced = (<% sysinfo("pid.vpnserver2"); %> > 0);
														else
															var service_state_advanced = false;
															
														$('#radio_service_enable').iphoneSwitch(service_state_advanced,
															function() {
																document.form.action_script.value = "start_vpnserver"+openvpn_unit;
																parent.showLoading();
																document.form.submit();
															},
															function() {
																document.form.action_script.value = "stop_vpnserver"+openvpn_unit;
																parent.showLoading();
																document.form.submit();
															}
														);
													</script>
													<span>Warning: any unsaved change will be lost.</span>
												</td>
											</tr>
											<tr>
												<th><#vpn_openvpn_interface#></th>
												<td>
													<select name="vpn_server_if" class="input_option" onChange="update_visibility();">
														<option value="tap" <% nvram_match("vpn_server_if","tap","selected"); %> >TAP</option>
														<option value="tun" <% nvram_match("vpn_server_if","tun","selected"); %> >TUN</option>
													</select>
												</td>
											</tr>
											<tr>
												<th><#IPConnection_VServerProto_itemname#></th>
												<td>
													<select name="vpn_server_proto" class="input_option">
														<option value="tcp-server" <% nvram_match("vpn_server_proto","tcp-server","selected"); %> >TCP</option>
														<option value="udp" <% nvram_match("vpn_server_proto","udp","selected"); %> >UDP</option>
													</select>
												</td>
											</tr>
											<tr>
												<th>Server Port</th>
												<td>
													<input type="text" maxlength="5" class="input_6_table" name="vpn_server_port" onKeyPress="return validator.isNumber(this,event);" onblur="validator.numberRange(this, 1, 65535)" value="<% nvram_get("vpn_server_port"); %>" autocorrect="off" autocapitalize="off">
													<span style="color:#FC0">(<#Setting_factorydefault_value#> : 1194)</span>
												</td>
											</tr>
											<tr>
												<th>Let the OS manage socket buffers</th>
												<td>
													<input type="radio" name="vpn_server_sockbuf" class="input" value="1" <% nvram_match_x("", "vpn_server_sockbuf", "1", "checked"); %>><#checkbox_Yes#>
													<input type="radio" name="vpn_server_sockbuf" class="input" value="0" <% nvram_match_x("", "vpn_server_sockbuf", "0", "checked"); %>><#checkbox_No#>
												</td>
											</tr>
											<tr>
												<th><#menu5_5#></th>
												<td>
													<select name="vpn_server_firewall" class="input_option">
														<option value="auto" <% nvram_match("vpn_server_firewall","auto","selected"); %> ><#Auto#></option>
														<option value="external" <% nvram_match("vpn_server_firewall","external","selected"); %> ><#External#></option>
														<option value="custom" <% nvram_match("vpn_server_firewall","custom","selected"); %> ><#Custom#></option>
													</select>
												</td>
											</tr>
											<tr>
												<th><#vpn_openvpn_Auth#></th>
												<td>
													<select name="vpn_server_crypt" class="input_option" onChange="update_visibility();">
														<option value="tls" <% nvram_match("vpn_server_crypt","tls","selected"); %> >TLS</option>
														<option value="secret" <% nvram_match("vpn_server_crypt","secret","selected"); %> >Static Key</option>
														<option value="custom" <% nvram_match("vpn_server_crypt","custom","selected"); %> >Custom</option>
													</select>
													<span id="server_tls_crypto_text" onclick="set_Keys();" style="text-decoration:underline;cursor:pointer;"><#vpn_openvpn_ModifyKeys#></span>
													<span id="server_custom_crypto_text"><#vpn_openvpn_MustManual#></span>
												</td>
											</tr>
											<tr>
												<th>Username/Password Authentication</th>
												<td>
													<input type="radio" name="vpn_server_userpass_auth" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_userpass_auth", "1", "checked"); %>><#checkbox_Yes#>
													<input type="radio" name="vpn_server_userpass_auth" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_userpass_auth", "0", "checked"); %>><#checkbox_No#>
												</td>
											</tr>
											<tr id="server_igncrt">
												<th><#vpn_openvpn_AuthOnly#></th>
												<td>
													<input type="radio" name="vpn_server_igncrt" class="input" value="1" <% nvram_match_x("", "vpn_server_igncrt", "1", "checked"); %>><#checkbox_Yes#>
													<input type="radio" name="vpn_server_igncrt" class="input" value="0" <% nvram_match_x("", "vpn_server_igncrt", "0", "checked"); %>><#checkbox_No#>
												</td>
											</tr>
											<tr id="server_authhmac">
												<th><#vpn_openvpn_AuthHMAC#></th>
												<td>
													<select name="vpn_server_hmac" class="input_option">
														<option value="-1" <% nvram_match("vpn_server_hmac","-1","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
														<option value="2" <% nvram_match("vpn_server_hmac","2","selected"); %> >Bi-directional</option>
														<option value="0" <% nvram_match("vpn_server_hmac","0","selected"); %> >Incoming (0)</option>
														<option value="1" <% nvram_match("vpn_server_hmac","1","selected"); %> >Incoming (1)</option>
													</select>
													<span style="color:#FC0">(TLS-Auth)</span>
												</td>
											</tr>			
											<tr id="server_snnm">
												<th><#vpn_openvpn_SubnetMsak#></th>
												<td>
													<input type="text" maxlength="15" class="input_15_table" name="vpn_server_sn" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_server_sn"); %>" autocorrect="off" autocapitalize="off">
													<input type="text" maxlength="15" class="input_15_table" name="vpn_server_nm" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_server_nm"); %>" autocorrect="off" autocapitalize="off">
												</td>
											</tr>
											<tr id="server_dhcp">
												<th><#vpn_openvpn_dhcp#></th>
												<td>
													<input type="radio" name="vpn_server_dhcp" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_dhcp", "1", "checked"); %>><#checkbox_Yes#>
													<input type="radio" name="vpn_server_dhcp" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_dhcp", "0", "checked"); %>><#checkbox_No#>
												</td>
		 									</tr>
											<tr id="server_range">
												<th><#vpn_openvpn_ClientPool#></th>
												<td>
													<input type="text" maxlength="15" class="input_15_table" name="vpn_server_r1" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_server_r1"); %>" autocorrect="off" autocapitalize="off">
													<input type="text" maxlength="15" class="input_15_table" name="vpn_server_r2" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_server_r2"); %>" autocorrect="off" autocapitalize="off">
												</td>
											</tr>
											<tr id="server_local">
												<th><#vpn_openvpn_LocalRemote_IP#></th>
												<td>
													<input type="text" maxlength="15" class="input_15_table" name="vpn_server_local" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_server_local"); %>" autocorrect="off" autocapitalize="off">
													<input type="text" maxlength="15" class="input_15_table" name="vpn_server_remote" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_server_remote"); %>" autocorrect="off" autocapitalize="off">
												</td>
											</tr>
											<tr>
												<th><#vpn_openvpn_PollInterval#></th>
												<td>
													<input type="text" maxlength="4" class="input_6_table" name="vpn_server_poll" onKeyPress="return validator.isNumber(this,event);" onblur="validator.numberRange(this, 0, 1440)" value="<% nvram_get("vpn_server_poll"); %>" autocorrect="off" autocapitalize="off"> <#Minute#>
													<span style="color:#FC0">(<#zero_disable#>)</span>
												</td>
											</tr>
											<tr id="server_plan">
												<th><#vpn_openvpn_PushLAN#></th>
												<td>
													<input type="radio" name="vpn_server_plan" class="input" value="1" <% nvram_match_x("", "vpn_server_plan", "1", "checked"); %>><#checkbox_Yes#>
													<input type="radio" name="vpn_server_plan" class="input" value="0" <% nvram_match_x("", "vpn_server_plan", "0", "checked"); %>><#checkbox_No#>
												</td>
		 									</tr>
											<tr>
												<th><#vpn_openvpn_RedirectInternet#></th>
												<td>
													<input type="radio" name="vpn_server_rgw" class="input" value="1" <% nvram_match_x("", "vpn_server_rgw", "1", "checked"); %>><#checkbox_Yes#>
													<input type="radio" name="vpn_server_rgw" class="input" value="0" <% nvram_match_x("", "vpn_server_rgw", "0", "checked"); %>><#checkbox_No#>
												</td>
		 									</tr>
											<tr>
												<th><#vpn_openvpn_ResponseDNS#></th>
												<td>
													<input type="radio" name="vpn_server_x_dns" class="input" value="1" onclick="update_visibility();"><#checkbox_Yes#>
													<input type="radio" name="vpn_server_x_dns" class="input" value="0" onclick="update_visibility();"><#checkbox_No#>
												</td>
		 									</tr>
											<tr id="server_pdns">
												<th><#vpn_openvpn_AdvDNS#></th>
												<td>
													<input type="radio" name="vpn_server_pdns" class="input" value="1" <% nvram_match_x("", "vpn_server_pdns", "1", "checked"); %>><#checkbox_Yes#>
													<input type="radio" name="vpn_server_pdns" class="input" value="0" <% nvram_match_x("", "vpn_server_pdns", "0", "checked"); %>><#checkbox_No#>
												</td>
		 									</tr>
											<tr>
												<th><#vpn_openvpn_Encrypt#></th>
												<td>
													<select name="vpn_server_cipher" class="input_option"></select>
												</td>
											</tr>
											<tr>
												<th><#vpn_openvpn_Compression#></th>
												<td>
													<select name="vpn_server_comp" class="input_option">
														<option value="-1" <% nvram_match("vpn_server_comp","-1","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
														<option value="no" <% nvram_match("vpn_server_comp","no","selected"); %> ><#wl_securitylevel_0#></option>
														<option value="yes" <% nvram_match("vpn_server_comp","yes","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
														<option value="adaptive" <% nvram_match("vpn_server_comp","adaptive","selected"); %> ><#Adaptive#></option>
													</select>
												</td>
											</tr>
											<tr id="server_reneg">
												<th><#vpn_openvpn_TLSTime#></th>
												<td>
													<input type="text" maxlength="5" class="input_6_table" name="vpn_server_reneg" onblur="validator.range(this, -1, 2147483647)" value="<% nvram_get("vpn_server_reneg"); %>" autocorrect="off" autocapitalize="off"> <#Second#>
													<span style="color:#FC0">(<#Setting_factorydefault_value#> : -1)</span>
												</td>
											</tr>
											<tr id="server_ccd">
												<th><#vpn_openvpn_SpecificOpt#></th>
												<td>
													<input type="radio" name="vpn_server_ccd" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_ccd", "1", "checked"); %>><#checkbox_Yes#>
													<input type="radio" name="vpn_server_ccd" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_ccd", "0", "checked"); %>><#checkbox_No#>
												</td>
		 									</tr>
											<tr id="server_c2c">
												<th><#vpn_openvpn_ClientMutual#></th>
												<td>
													<input type="radio" name="vpn_server_c2c" class="input" value="1" <% nvram_match_x("", "vpn_server_c2c", "1", "checked"); %>><#checkbox_Yes#>
													<input type="radio" name="vpn_server_c2c" class="input" value="0" <% nvram_match_x("", "vpn_server_c2c", "0", "checked"); %>><#checkbox_No#>
												</td>
											</tr>
											<tr id="server_ccd_excl">
												<th><#vpn_openvpn_ClientSpecified#></th>
												<td>
													<input type="radio" name="vpn_server_ccd_excl" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_ccd_excl", "1", "checked"); %>><#checkbox_Yes#>
													<input type="radio" name="vpn_server_ccd_excl" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_ccd_excl", "0", "checked"); %>><#checkbox_No#>
												</td>
		 									</tr>
										</table>
										<!-- Advanced setting table end-->
										<!-- Allowed Clients setting table start-->
										<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" id="openvpn_client_table">
											<thead>
											<tr>
												<td colspan="5">Allowed Clients</td>
											</tr>
											</thead>
											<tr>
												<th width="36%"><#HSDPAConfig_Username_itemname#></th>
												<th width="20%"><#IPConnection_ExternalIPAddress_itemname#></th>
												<th width="20%"><#IPConnection_x_ExternalSubnetMask_itemname#></th>
												<th width="12%"><#Push#></th>
												<th width="12%"><#list_add_delete#></th>
											</tr>
											<tr>							
												<div id="VPNClientList_Block_PC" class="VPNClientList_Block_PC"></div>
												<td width="36%">
								 					<input type="text" class="input_25_table" maxlength="25" name="vpn_clientlist_commonname_0" autocorrect="off" autocapitalize="off">
								 				</td>
												<td width="20%">
								 					<input type="text" class="input_15_table" maxlength="15" name="vpn_clientlist_subnet_0"  onkeypress="return validator.isIPAddr(this, event);" autocorrect="off" autocapitalize="off">
								 				</td>
												<td width="20%">
								 					<input type="text" class="input_15_table" maxlength="15" name="vpn_clientlist_netmask_0"  onkeypress="return validator.isIPAddr(this, event);" autocorrect="off" autocapitalize="off">
								 				</td>
								 				<td width="12%">
													<select name="vpn_clientlist_push_0" class="input_option">
														<option value="0" selected>No</option>
														<option value="1">Yes</option>
													</select>
												</td>
								 				<td width="12%">
								 					<input class="add_btn" type="button" onClick="addRow_Group_Advanced(128);" name="vpn_clientlist2" value="">
								 				</td>
											</tr>
										</table>
										<div id="openvpn_clientlist_Block"></div>
										<!-- Allowed Clients setting table end-->
										
										<!-- Custom setting table start-->									
										<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
											<thead>
											<tr>
												<td><#vpn_openvpn_CustomConf#></td>
											</tr>
											</thead>

											<tr>
												<td>
													<textarea rows="8" class="textarea_ssh_table" name="vpn_server_custom" cols="55" maxlength="15000"><% nvram_clean_get("vpn_server_custom"); %></textarea>
												</td>
											</tr>
										</table>
										<!-- Custom setting table end-->
									</div>

									<div id="divApply" class="apply_gen" style="display:none;">
										<input class="button_gen" onclick="applyRule()" type="button" value="<#CTL_apply#>"/>
									</div>
								</td>
							</tr>
							</tbody>
						</table>
					</td>
				</tr>
			</table>		
		</td>
	</tr>
</table>
</form>

<div id="mailSendPanelContainer" class="hiddenPanelContainer">
	<div class="hiddenPanel">
		<form method="post" name="mailSendForm" action="/start_apply.htm" target="hidden_frame">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_script" value="restart_sendmail">
			<input type="hidden" name="action_wait" value="5">
			<input type="hidden" name="flag" value="background">
			<input type="hidden" name="PM_MAIL_SUBJECT" value="My ovpn file">
			<input type="hidden" name="PM_MAIL_FILE" value="/www/client<% nvram_get("vpn_server_unit"); %>.ovpn">
			<input type="hidden" name="PM_LETTER_CONTENT" value="Here is the ovpn file.">

			<div class="panelTableTitle">
				<div>Send</div>
				<img style="width: 100%; height: 2px;" src="/images/New_ui/export/line_export.png">
			</div>

			<table border=0 align="center" cellpadding="5" cellspacing="0" class="FormTable panelTable">
				<tr>
					<th>PM_MAIL_TARGET</th>
					<td valign="top">
						<input type="text" class="input_32_table" name="PM_MAIL_TARGET" value="" autocorrect="off" autocapitalize="off">
					</td>
				</tr>
			</table>

			<div class="panelSubmiter">
				<input id="mailSendPannelCancel" class="button_gen" type="button" value="<#CTL_Cancel#>">
				<input id="mailSendPannelSubmiter" class="button_gen" type="button" value="Send">
				<img id="mailSendLoadingIcon" style="margin-left:5px;display:none;" src="/images/InternetScan.gif">
				<script>
					document.getElementById("mailSendPannelCancel").onclick = function(){
						$("#mailSendPanelContainer").fadeOut(300);
					}
					document.getElementById("mailSendPannelSubmiter").onclick = function(){
						// ToDo: validator.
						$("#mailSendLoadingIcon").fadeIn(200);
						document.mailSendForm.submit();
						setTimeout(function(){
							document.mailSendForm.PM_MAIL_TARGET.value = "";
							$("#mailSendLoadingIcon").fadeOut(200);
							$("#mailSendPanelContainer").fadeOut(300);
						}, document.mailSendForm.action_wait.value*1000);
					}
				</script>
			</div>
		</form>
	</div>
</div>

<div id="mailConfigPanelContainer" class="hiddenPanelContainer">
	<div class="hiddenPanel">
		<form method="post" name="mailConfigForm" action="/start_apply.htm" target="hidden_frame">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_script" value="saveNvram">
			<input type="hidden" name="action_wait" value="3">
			<input type="hidden" name="PM_SMTP_SERVER" value="<% nvram_get("PM_SMTP_SERVER"); %>">
			<input type="hidden" name="PM_SMTP_PORT" value="<% nvram_get("PM_SMTP_PORT"); %>">
			<input type="hidden" name="PM_SMTP_AUTH_USER" value="<% nvram_get("PM_SMTP_AUTH_USER"); %>">
			<input type="hidden" name="PM_SMTP_AUTH_PASS" value="<% nvram_get("PM_SMTP_AUTH_PASS"); %>">
			<input type="hidden" name="PM_MY_NAME" value="<% nvram_get("PM_MY_NAME"); %>">
			<input type="hidden" name="PM_MY_EMAIL" value="<% nvram_get("PM_MY_EMAIL"); %>">
	
			<div class="panelTableTitle">
				<div>Setup mail server</div>
				<img style="width: 100%; height: 2px;" src="/images/New_ui/export/line_export.png">
			</div>

			<table border=0 align="center" cellpadding="5" cellspacing="0" class="FormTable panelTable">
				<tr>
					<th>PM_SMTP_SERVER</th>
					<td valign="top">
				    <select style="width:350px;" name="PM_SMTP_SERVER_TMP" class="input_option">
							<option value="smtp.gmail.com" <% nvram_match( "PM_SMTP_SERVER", "smtp.gmail.com", "selected"); %>>Google Gmail</option>
				    </select>
						<script>
							var smtpList = new Array()
							smtpList = [
								{smtpServer: "smtp.gmail.com", smtpPort: "587", smtpDomain: "gmail.com"},
								{end: 0}
							];

							document.mailConfigForm.PM_SMTP_SERVER_TMP.onchange = function(){
								document.mailConfigForm.PM_SMTP_PORT_TMP.value = smtpList[this.selectedIndex].smtpPort;
								document.mailConfigForm.PM_SMTP_AUTH_USER_TMP.value = "";		
								document.mailConfigForm.PM_SMTP_AUTH_PASS_TMP.value = "";		
								document.mailConfigForm.PM_MY_NAME_TMP.value = "";		
								document.mailConfigForm.PM_MY_EMAIL_TMP.value = "";		
							}
						</script>
					</td>
				</tr>
				<input type="hidden" name="PM_SMTP_PORT_TMP" value="<% nvram_get("PM_SMTP_PORT"); %>">  				      			
				<tr>
					<th>PM_SMTP_AUTH_USER</th>
					<td valign="top">
						<input type="text" class="input_32_table" name="PM_SMTP_AUTH_USER_TMP" value="<% nvram_get("PM_SMTP_AUTH_USER"); %>" autocorrect="off" autocapitalize="off">
						<script>
							document.mailConfigForm.PM_SMTP_AUTH_USER_TMP.onkeyup = function(){
								document.mailConfigForm.PM_MY_NAME_TMP.value = this.value;
								document.mailConfigForm.PM_MY_EMAIL_TMP.value = this.value + "@" + smtpList[document.mailConfigForm.PM_SMTP_SERVER_TMP.selectedIndex].smtpDomain;
							}
						</script>
					</td>
				</tr>    				      			
				<tr>
					<th>PM_SMTP_AUTH_PASS</th>
					<td valign="top">
						<input type="password" class="input_32_table" name="PM_SMTP_AUTH_PASS_TMP" maxlength="100" value="" autocorrect="off" autocapitalize="off">
					</td>
				</tr>    				      			
				<tr>
					<th>PM_MY_NAME (Optional)</th>
					<td valign="top">
						<input type="text" class="input_32_table" name="PM_MY_NAME_TMP" value="<% nvram_get("PM_MY_NAME"); %>" autocorrect="off" autocapitalize="off">
					</td>
				</tr>    				      			
				<tr>
					<th>PM_MY_EMAIL (Optional)</th>
					<td valign="top">
						<input type="text" class="input_32_table" name="PM_MY_EMAIL_TMP" value="<% nvram_get("PM_MY_EMAIL"); %>" autocorrect="off" autocapitalize="off">
					</td>
				</tr>    				      			
			</table>

			<div class="panelSubmiter">
				<input id="mailConfigPannelCancel" class="button_gen" type="button" value="<#CTL_Cancel#>">
				<input id="mailConfigPannelSubmiter" class="button_gen" type="button" value="<#CTL_onlysave#>">
				<img id="mailConfigLoadingIcon" style="margin-left:5px;display:none;" src="/images/InternetScan.gif">
				<script>
					document.getElementById("mailConfigPannelCancel").onclick = function(){
						$("#mailConfigPanelContainer").fadeOut(300);
					}
					document.getElementById("mailConfigPannelSubmiter").onclick = function(){
						// ToDo: validator.

						document.mailConfigForm.PM_SMTP_SERVER.value = document.mailConfigForm.PM_SMTP_SERVER_TMP.value;
						if (document.mailConfigForm.PM_SMTP_PORT_TMP.value == "")
							document.mailConfigForm.PM_SMTP_PORT.value = smtpList[0].smtpPort;
						else
							document.mailConfigForm.PM_SMTP_PORT.value = document.mailConfigForm.PM_SMTP_PORT_TMP.value;
						document.mailConfigForm.PM_SMTP_AUTH_USER.value = document.mailConfigForm.PM_SMTP_AUTH_USER_TMP.value;
						document.mailConfigForm.PM_SMTP_AUTH_PASS.value = document.mailConfigForm.PM_SMTP_AUTH_PASS_TMP.value;
						document.mailConfigForm.PM_MY_NAME.value = document.mailConfigForm.PM_MY_NAME_TMP.value;
						document.mailConfigForm.PM_MY_EMAIL.value = document.mailConfigForm.PM_MY_EMAIL_TMP.value;

						$("#mailConfigLoadingIcon").fadeIn(200);
						document.mailConfigForm.submit();
						setTimeout(function(){
							$("#mailConfigLoadingIcon").fadeOut(200);
							showMailPanel();
						}, document.mailConfigForm.action_wait.value*1000);
					}
				</script>
			</div>
		</form>
	</div>
</div>
<div id="footer"></div>
</body>
</html>
