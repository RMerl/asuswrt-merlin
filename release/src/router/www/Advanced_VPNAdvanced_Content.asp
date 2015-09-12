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
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" language="JavaScript" src="/merlin.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<style type="text/css">
.contentM_qis{
	width:740px;	
	margin-top:220px;
	margin-left:380px;
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:200;
	background-color:#2B373B;
	display:none;
	/*behavior: url(/PIE.htc);*/
}
.QISmain{
	width:730px;
	/*font-family:Verdana, Arial, Helvetica, sans-serif;*/
	font-size:14px;
	z-index:200;
	position:relative;
	background-color:balck:
}	
.QISform_wireless{
	width:600px;
	font-size:12px;
	color:#FFFFFF;
	margin-top:10px;
	*margin-left:10px;
}

.QISform_wireless thead{
	font-size:15px;
	line-height:20px;
	color:#FFFFFF;	
}

.QISform_wireless th{
	padding-left:10px;
	*padding-left:30px;
	font-size:12px;
	font-weight:bolder;
	color: #FFFFFF;
	text-align:left; 
}
               
.QISform_wireless li{	
	margin-top:10px;
}
.QISGeneralFont{
	font-family:Segoe UI, Arial, sans-serif;
	margin-top:10px;
	margin-left:70px;
	*margin-left:50px;
	margin-right:30px;
	color:white;
	LINE-HEIGHT:18px;
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

window.onresize = cal_panel_block;
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]

/* initial variables for pptpd start */
var pptpd_clients = '<% nvram_get("pptpd_clients"); %>';
var pptpd_dns1_orig = '<% nvram_get("pptpd_dns1"); %>';
var pptpd_dns2_orig = '<% nvram_get("pptpd_dns2"); %>';
var pptpd_wins1_orig = '<% nvram_get("pptpd_wins1"); %>';
var pptpd_wins2_orig = '<% nvram_get("pptpd_wins2"); %>';
var pptpd_mru_orig = '<% nvram_get("pptpd_mru"); %>';
var pptpd_mtu_orig = '<% nvram_get("pptpd_mtu"); %>';

var origin_lan_ip = '<% nvram_get("lan_ipaddr"); %>';
var lan_ip_subnet = origin_lan_ip.split(".")[0]+"."+origin_lan_ip.split(".")[1]+"."+origin_lan_ip.split(".")[2]+".";
var lan_ip_end = parseInt(origin_lan_ip.split(".")[3]);
var alert_max = "<#vpn_max_clients#>";
var alert_over = "<#vlaue_haigher_than#> ";

var dhcp_enable = '<% nvram_get("dhcp_enable_x"); %>';
var pool_start = '<% nvram_get("dhcp_start"); %>';
var pool_end = '<% nvram_get("dhcp_end"); %>';
var pool_subnet = pool_start.split(".")[0]+"."+pool_start.split(".")[1]+"."+pool_start.split(".")[2]+".";
var pool_start_end = parseInt(pool_start.split(".")[3]);
var pool_end_end = parseInt(pool_end.split(".")[3]);

var static_enable = '<% nvram_get("dhcp_static_x"); %>';
var dhcp_staticlists = '<% nvram_get("dhcp_staticlist"); %>';
var staticclist_row = dhcp_staticlists.split('&#60');
/* initial variables for pptpd end */

/* initial variables for openvpn start */
<% vpn_server_get_parameter(); %>;

var openvpn_clientlist_array = decodeURIComponent('<% nvram_char_to_ascii("", "vpn_server_ccd_val"); %>');
var openvpn_unit = '<% nvram_get("vpn_server_unit"); %>';
var initial_vpn_mode = "<% nvram_get("VPNServer_mode"); %>";

var openvpn_eas = '<% nvram_get("vpn_serverx_start"); %>';
var openvpn_enabled = (openvpn_eas.indexOf(''+(openvpn_unit)) >= 0);

if (openvpn_unit == '1')
	var service_state = (<% sysinfo("pid.vpnserver1"); %> > 0);
else if (openvpn_unit == '2')
	var service_state = (<% sysinfo("pid.vpnserver2"); %> > 0);
else
	var service_state = false;

service_state |= openvpn_enabled;

ciphersarray = [
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
/* initial variables for openvpn end */

function initial(){
	show_menu();

	check_dns_wins();

	add_VPN_mode_Option(document.form.pptpd_mode);
	add_VPN_mode_Option(document.openvpn_form.openvpn_mode);

	if (pptpd_clients != "") {
		document.form._pptpd_clients_start.value = pptpd_clients.split("-")[0];
		document.form._pptpd_clients_end.value = pptpd_clients.split("-")[1];
		document.getElementById('pptpd_subnet').innerHTML = pptpd_clients.split(".")[0] + "." +
				      pptpd_clients.split(".")[1] + "." +
				      pptpd_clients.split(".")[2] + ".";
	}
	
	if (document.form.pptpd_mppe.value == 0)
		document.form.pptpd_mppe.value = (1 | 4 | 8);
	document.form.pptpd_mppe_128.checked = (document.form.pptpd_mppe.value & 1);
	//document.form.pptpd_mppe_56.checked = (document.form.pptpd_mppe.value & 2);
	document.form.pptpd_mppe_40.checked = (document.form.pptpd_mppe.value & 4);
	document.form.pptpd_mppe_no.checked = (document.form.pptpd_mppe.value & 8);

	check_vpn_conflict();
	openvpn_clientlist();

	// Cipher list
	free_options(document.openvpn_form.vpn_server_cipher);
	currentcipher = "<% nvram_get("vpn_server_cipher"); %>";
	add_option(document.openvpn_form.vpn_server_cipher, "Default","default",(currentcipher == "Default"));
	add_option(document.openvpn_form.vpn_server_cipher, "None","none",(currentcipher == "none"));

	for(var i = 0; i < ciphersarray.length; i++){
		add_option(document.openvpn_form.vpn_server_cipher,
			ciphersarray[i][0], ciphersarray[i][0],
			(currentcipher == ciphersarray[i][0]));
	}

	// Set these based on a compound field
	setRadioValue(document.openvpn_form.vpn_server_x_dns, ((document.openvpn_form.vpn_serverx_dns.value.indexOf(''+(openvpn_unit)) >= 0) ? "1" : "0"));

	// Display appropriate page
	change_mode(initial_vpn_mode);

	// Decode into editable format
	openvpn_decodeKeys(0);
}

function add_VPN_mode_Option(obj){
	free_options(obj);

	if(pptpd_support)
		add_option(obj, "PPTP", "pptpd", (initial_vpn_mode == "pptpd"));
	if(openvpnd_support)
		add_option(obj, "OpenVPN", "openvpn", (initial_vpn_mode == "openvpn"));
}

function change_mode(mode){
	if (mode == "pptpd"){
		document.form.style.display = "";
		document.openvpn_form.style.display = "none";
		document.form.pptpd_mode.value = "pptpd";
		document.form.VPNServer_mode.value = "pptpd";
	}else{
		update_visibility();
		document.form.style.display = "none";
		document.openvpn_form.style.display = "";
		document.openvpn_form.openvpn_mode.value = "openvpn";
		document.openvpn_form.VPNServer_mode.value = "openvpn";
	}
}

function openvpn_decodeKeys(entities){
	var expr;

	if (entities == 1)
		expr = new RegExp('&#62;','gm');
	else
		expr = new RegExp('>','gm');

	document.getElementById('edit_vpn_crt_server1_static').value = document.getElementById('edit_vpn_crt_server1_static').value.replace(expr,"\r\n");
	document.getElementById('edit_vpn_crt_server1_ca').value = document.getElementById('edit_vpn_crt_server1_ca').value.replace(expr,"\r\n");
	document.getElementById('edit_vpn_crt_server1_key').value = document.getElementById('edit_vpn_crt_server1_key').value.replace(expr,"\r\n");
	document.getElementById('edit_vpn_crt_server1_crt').value = document.getElementById('edit_vpn_crt_server1_crt').value.replace(expr,"\r\n");
	document.getElementById('edit_vpn_crt_server1_dh').value = document.getElementById('edit_vpn_crt_server1_dh').value.replace(expr,"\r\n");

	document.getElementById('edit_vpn_crt_server2_static').value = document.getElementById('edit_vpn_crt_server2_static').value.replace(expr,"\r\n");
	document.getElementById('edit_vpn_crt_server2_ca').value = document.getElementById('edit_vpn_crt_server2_ca').value.replace(expr,"\r\n");
	document.getElementById('edit_vpn_crt_server2_key').value = document.getElementById('edit_vpn_crt_server2_key').value.replace(expr,"\r\n");
	document.getElementById('edit_vpn_crt_server2_crt').value = document.getElementById('edit_vpn_crt_server2_crt').value.replace(expr,"\r\n");
	document.getElementById('edit_vpn_crt_server2_dh').value = document.getElementById('edit_vpn_crt_server2_dh').value.replace(expr,"\r\n");
}

function openvpn_clientlist(){
	var openvpn_clientlist_row = openvpn_clientlist_array.split('<');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="openvpn_clientlist_table">';
	if(openvpn_clientlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td>';
	else{
		for (var i = 1; i < openvpn_clientlist_row.length; i++){
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


function update_visibility(){
	auth = document.openvpn_form.vpn_server_crypt.value;
	iface = document.openvpn_form.vpn_server_if.value;
	hmac = document.openvpn_form.vpn_server_hmac.value;
	userpass = getRadioValue(document.openvpn_form.vpn_server_userpass_auth);
	dhcp = getRadioValue(document.openvpn_form.vpn_server_dhcp);
	if(auth != "tls")
		ccd = 0;
	else
		ccd = getRadioValue(document.openvpn_form.vpn_server_ccd);
	dns = getRadioValue(document.openvpn_form.vpn_server_x_dns);

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
	showhide("server_tls_crypto_text", (auth != "custom"));		//add by Viz
	showhide("server_custom_crypto_text", (auth == "custom"));
	showhide("server_igncrt", (userpass == 1));

// Since instancing certs/keys would waste many KBs of nvram,
// we instead handle these at the webui level, loading both instances.
	showhide("edit_vpn_crt_server1_ca",(openvpn_unit == "1"));
	showhide("edit_vpn_crt_server1_crt", (openvpn_unit == "1"));
	showhide("edit_vpn_crt_server1_key",(openvpn_unit == "1"));
	showhide("edit_vpn_crt_server1_dh",(openvpn_unit == "1"));
	showhide("edit_vpn_crt_server2_ca",(openvpn_unit == "2"));
	showhide("edit_vpn_crt_server2_crt", (openvpn_unit == "2"));
	showhide("edit_vpn_crt_server2_key",(openvpn_unit == "2"));
	showhide("edit_vpn_crt_server2_dh",(openvpn_unit == "2"));

	showhide("edit_vpn_crt_server1_static", (openvpn_unit == "1"));
	showhide("edit_vpn_crt_server2_static", (openvpn_unit == "2"));

// Hide TLS fields if we are using Secret auth method
	showhide("edit_tls1", (auth == "tls"));
	showhide("edit_tls2", (auth == "tls"));
	showhide("edit_tls3", (auth == "tls"));
	showhide("edit_tls4", (auth == "tls"));

}

function del_openvpnRow(r){
	var i=r.parentNode.parentNode.rowIndex;
	document.getElementById('openvpn_clientlist_table').deleteRow(i);

	var openvpn_clientlist_value = "";
	for(k=0; k<document.getElementById('openvpn_clientlist_table').rows.length; k++){
		for(j=0; j<document.getElementById('openvpn_clientlist_table').rows[k].cells.length-1; j++){
			if(j == 0)
				openvpn_clientlist_value += "<1>";
			else
				openvpn_clientlist_value += ">";
			openvpn_clientlist_value += document.getElementById('openvpn_clientlist_table').rows[k].cells[j].innerHTML;
		}
	}

	openvpn_clientlist_array = openvpn_clientlist_value;
	if(openvpn_clientlist_array == "")
		openvpn_clientlist();
}

function addRow_Group(upper){
	var client_num = document.getElementById('openvpn_clientlist_table').rows.length;
	var item_num = document.getElementById('openvpn_clientlist_table').rows[0].cells.length;

	if(client_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;
	}

	if(document.openvpn_form.vpn_clientlist_commonname_0.value==""){
					alert("<#JS_fieldblank#>");
					document.openvpn_form.vpn_clientlist_commonname_0.focus();
					document.openvpn_form.vpn_clientlist_commonname_0.select();
					return false;
	}
	if(document.openvpn_form.vpn_clientlist_subnet_0.value==""){
					alert("<#JS_fieldblank#>");
					document.openvpn_form.vpn_clientlist_subnet_0.focus();
					document.openvpn_form.vpn_clientlist_subnet_0.select();
					return false;		
	}
	if(document.openvpn_form.vpn_clientlist_netmask_0.value==""){
					alert("<#JS_fieldblank#>");
					document.openvpn_form.vpn_clientlist_netmask_0.focus();
					document.openvpn_form.vpn_clientlist_netmask_0.select();
					return false;		
	}

// Check for duplicate

	if(item_num >=2){
		for(i=0; i<client_num; i++){
			if(document.openvpn_form.vpn_clientlist_commonname_0.value.toLowerCase() == document.getElementById('openvpn_clientlist_table').rows[i].cells[0].innerHTML.toLowerCase()
					&& document.openvpn_form.vpn_clientlist_subnet_0.value == document.getElementById('openvpn_clientlist_table').rows[i].cells[1].innerHTML
					&& document.openvpn_form.vpn_clientlist_netmask_0.value == document.getElementById('openvpn_clientlist_table').rows[i].cells[2].innerHTML){
				alert('<#JS_duplicate#>');
				document.openvpn_form.vpn_clientlist_commonname_0.focus();
				document.openvpn_form.vpn_clientlist_commonname_0.select();
				return false;
			}
		}
	}
	Do_addRow_Group();
}

function Do_addRow_Group(){
	addRow(document.openvpn_form.vpn_clientlist_commonname_0 ,1);
	addRow(document.openvpn_form.vpn_clientlist_subnet_0, 0);
	addRow(document.openvpn_form.vpn_clientlist_netmask_0, 0);
	addRow(document.openvpn_form.vpn_clientlist_push_0, 0);

	document.openvpn_form.vpn_clientlist_push_0.value="0"; //reset selection

	openvpn_clientlist();
}

function addRow(obj, head){
	if(head == 1)
		openvpn_clientlist_array += "<1>";
	else
		openvpn_clientlist_array += ">";

	openvpn_clientlist_array += obj.value;
	obj.value = "";
}

function changeMppe(){
	if (!document.form.pptpd_mppe_128.checked &&
	    //!document.form.pptpd_mppe_56.checked &&
	    !document.form.pptpd_mppe_40.checked)
		document.form.pptpd_mppe_no.checked = true;
}

function pptpd_applyRule(){
	if(!valid_IP(document.form._pptpd_clients_start, "")){
			document.form._pptpd_clients_start.focus();
			document.form._pptpd_clients_start.select();
			return false;		
	}

	if(!validate_number_range(document.form._pptpd_clients_end, 1, 254)){
			document.form._pptpd_clients_end.focus();
			document.form._pptpd_clients_end.select();
			return false;
	}	

	document.form.pptpd_clients.value = document.form._pptpd_clients_start.value + "-" + document.form._pptpd_clients_end.value;

	document.form.pptpd_mppe.value = 0;
	if (document.form.pptpd_mppe_128.checked)
		document.form.pptpd_mppe.value |= 1;
	//if (document.form.pptpd_mppe_56.checked)
	//	document.form.pptpd_mppe.value |= 2;
	if (document.form.pptpd_mppe_40.checked)
		document.form.pptpd_mppe.value |= 4;
	if (document.form.pptpd_mppe_no.checked)
		document.form.pptpd_mppe.value |= 8;


	if(document.form.pptpd_dnsenable_x[0].checked == true){
				document.form.pptpd_dns1.value = "";
				document.form.pptpd_dns2.value = "";
	}else{
			if(document.form.pptpd_dns1.value == "" && document.form.pptpd_dns2.value == ""){
					alert("<#JS_fieldblank#>");
					document.form.pptpd_dns1.focus();
					document.form.pptpd_dns1.select();
					return false;	
			}

			if(!valid_IP_form(document.form.pptpd_dns1, 0) || !valid_IP_form(document.form.pptpd_dns2, 0))
					return false;
	}

	if(document.form.pptpd_winsenable_x[0].checked == true){
				document.form.pptpd_wins1.value = "";
				document.form.pptpd_wins2.value = "";
	}else{
			if(document.form.pptpd_wins1.value == "" && document.form.pptpd_wins2.value == ""){
					alert("<#JS_fieldblank#>");
					document.form.pptpd_wins1.focus();
					document.form.pptpd_wins1.select();
					return false;	
			}		
		
			if(!valid_IP_form(document.form.pptpd_wins1, 0) || !valid_IP_form(document.form.pptpd_wins2, 0))
					return false;
	}

	if(check_pptpd_clients_range() == false)
		return false;

	if(!validator.numberRange(document.form.pptpd_mru, 576, 1492))
		return false;
	if(!validator.numberRange(document.form.pptpd_mtu, 576, 1492))
		return false;

	showLoading();
	document.form.submit();	
}

function check_openvpn_conflict(){		//if conflict with LAN ip & DHCP ip pool & static
	
   if(document.openvpn_form.vpn_server_if.value == 'tun'){	
	if(document.openvpn_form.vpn_server_sn.value==""){
			alert("<#JS_fieldblank#>");
			document.openvpn_form.vpn_server_sn.focus();
			document.openvpn_form.vpn_server_sn.select();
			return false;
		}
	if(!validate_iprange(document.openvpn_form.vpn_server_sn, "")){
			document.openvpn_form.vpn_server_sn.focus();
			document.openvpn_form.vpn_server_sn.select();
			return false;		
	}
		
	var openvpn_server_subnet = document.openvpn_form.vpn_server_sn.value.split(".")[0] + "." +
		   document.openvpn_form.vpn_server_sn.value.split(".")[1] + "." +
		   document.openvpn_form.vpn_server_sn.value.split(".")[2] + ".";
	
	//LAN ip
  if(origin_lan_ip == document.openvpn_form.vpn_server_sn.value){
  		alert("<#vpn_conflict_LANIP#>" + origin_lan_ip);
  		return false;
  }
			
  //LAN ip pool
  if(lan_ip_subnet == openvpn_server_subnet){
  		alert("<#vpn_conflict_DHCPpool#>"+pool_start+" ~ "+pool_end);
  		return false;
  }
		
		//test if netmask is valid.
		var default_netmask = "";
		var wrong_netmask = 0;
		var netmask_obj = document.openvpn_form.vpn_server_nm;
		var netmask_num = inet_network(netmask_obj.value);
		
		if(netmask_num==0){
			var netmask_reverse_num = 0;
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
   }else if(document.openvpn_form.vpn_server_if.value == 'tap' && document.openvpn_form.vpn_server_dhcp.value == '0'){
		if(!valid_IP(document.openvpn_form.vpn_server_r1, "")){
			document.openvpn_form.vpn_server_r1.focus();
			document.openvpn_form.vpn_server_r1.select();
			return false;		
		}
   	if(!valid_IP(document.openvpn_form.vpn_server_r2, "")){
			document.openvpn_form.vpn_server_r2.focus();
			document.openvpn_form.vpn_server_r2.select();
			return false;		
		}	
		var openvpn_clients_start_subnet = document.openvpn_form.vpn_server_r1.value.split(".")[0] + "." +
				   document.openvpn_form.vpn_server_r1.value.split(".")[1] + "." +
				   document.openvpn_form.vpn_server_r1.value.split(".")[2] + ".";
		var openvpn_clients_end_subnet = document.openvpn_form.vpn_server_r2.value.split(".")[0] + "." +
				   document.openvpn_form.vpn_server_r2.value.split(".")[1] + "." +
				   document.openvpn_form.vpn_server_r2.value.split(".")[2] + ".";
		var openvpn_clients_start_ip = parseInt(document.openvpn_form.vpn_server_r1.value.split(".")[3]);
		var openvpn_clients_end_ip = parseInt(document.openvpn_form.vpn_server_r2.value.split(".")[3]);
  	//LAN ip
  	if((lan_ip_subnet == openvpn_clients_start_subnet || lan_ip_subnet == openvpn_clients_end_subnet)
		&& (lan_ip_end >= openvpn_clients_start_ip 
  		&& lan_ip_end <= openvpn_clients_end_ip)){
  		alert("<#vpn_conflict_LANIP#>"+origin_lan_ip);
  		return false;
  	}
  	//DHCP pool
	if(pool_subnet != openvpn_clients_start_subnet){
  		alert(document.openvpn_form.vpn_server_r1.value + " <#JS_validip#>");
  		return false;
	}
	if(pool_subnet != openvpn_clients_end_subnet){
  		alert(document.openvpn_form.vpn_server_r2.value + " <#JS_validip#>");
  		return false;
	}
	//DHCP static IP
	if(dhcp_staticlists != ""){
		for(var i = 1; i < staticclist_row.length; i++){
			var static_subnet ="";
			var static_end ="";					
			var static_ip = staticclist_row[i].split('&#62')[1];
			static_subnet = static_ip.split(".")[0]+"."+static_ip.split(".")[1]+"."+static_ip.split(".")[2]+".";
			static_end = parseInt(static_ip.split(".")[3]);
			if(static_subnet != openvpn_clients_start_subnet){
  						alert(document.openvpn_form.vpn_server_r1.value + " <#JS_validip#>");
  						return false;
  			}	
			if(static_subnet != openvpn_clients_end_subnet){
  						alert(document.openvpn_form.vpn_server_r2.value + " <#JS_validip#>");
  						return false;
  			}				
  		}
	}
   }
   return true;
}

function openvpn_applyRule(){

   if(check_openvpn_conflict()){
	showLoading();

	if (service_state) {
		document.openvpn_form.action_script.value = "restart_vpnserver" + openvpn_unit;
	}

	var client_num = document.getElementById('openvpn_clientlist_table').rows.length;
	var item_num = document.getElementById('openvpn_clientlist_table').rows[0].cells.length;
	var tmp_value = "";
	
	//Viz add 2014.06
	if(document.getElementById("server_reneg").style.display == "none")
			document.openvpn_form.vpn_server_reneg.disabled = true;

	for(i=0; i<client_num; i++){

		// Insert first field (1), which enables the client.
		tmp_value += "<1>"
		for(j=0; j<item_num-1; j++){

			if (j == 3)
				tmp_value += (document.getElementById('openvpn_clientlist_table').rows[i].cells[j].innerHTML == "Yes" ? 1 : 0);
			else
				tmp_value += document.getElementById('openvpn_clientlist_table').rows[i].cells[j].innerHTML;

			if(j != item_num-2)
				tmp_value += ">";
		}
	}
	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<1>")
		tmp_value = "";

	document.openvpn_form.vpn_server_ccd_val.value = tmp_value;

	tmp_value = "";

	for (var i=1; i < 3; i++) {
		if (i == openvpn_unit) {
			if (getRadioValue(document.openvpn_form.vpn_server_x_dns) == 1)
				tmp_value += ""+i+",";
		} else {
			if (document.openvpn_form.vpn_serverx_dns.value.indexOf(''+(i)) >= 0)
				tmp_value += ""+i+","
	}
	}

// TODO: Only restart if instance is running?
	if (tmp_value != document.openvpn_form.vpn_serverx_dns.value) {
		document.openvpn_form.action_script.value += ";restart_dnsmasq";
		document.openvpn_form.vpn_serverx_dns.value = tmp_value;
	}		
		
	document.openvpn_form.submit();
	}
}


// test if Private ip
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
		var ip_num = inet_network(ip_obj.value);	//-1 means nothing
		
		//1~254
		if(obj_name.value.split(".")[3] < 1 || obj_name.value.split(".")[3] > 254){
			alert(obj_name.value+" <#JS_validip#>");
			obj_name.focus();
			return false;
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

function setEnd(){
	var end="";
	var pptpd_clients_subnet = document.form._pptpd_clients_start.value.split(".")[0]
								+"."+document.form._pptpd_clients_start.value.split(".")[1]
								+"."+document.form._pptpd_clients_start.value.split(".")[2]
								+".";
	var pptpd_clients_start_ip = parseInt(document.form._pptpd_clients_start.value.split(".")[3]);	

	document.getElementById('pptpd_subnet').innerHTML = pptpd_clients_subnet;

	end = pptpd_clients_start_ip+9;
	if(end >254)	end = 254;

	if(!end)
		document.form._pptpd_clients_end.value = "";
	else
		document.form._pptpd_clients_end.value = end;
}

function check_vpn_conflict(){		//if conflict with LAN ip & DHCP ip pool & static

	var pptpd_clients_subnet = document.form._pptpd_clients_start.value.split(".")[0] + "." +
				   document.form._pptpd_clients_start.value.split(".")[1] + "." +
				   document.form._pptpd_clients_start.value.split(".")[2] + ".";
	var pptpd_clients_start_ip = parseInt(document.form._pptpd_clients_start.value.split(".")[3]);
	var pptpd_clients_end_ip = parseInt(document.form._pptpd_clients_end.value);

  //LAN ip
  if(lan_ip_subnet == pptpd_clients_subnet 
  		&& lan_ip_end >= pptpd_clients_start_ip 
  		&& lan_ip_end <= pptpd_clients_end_ip ){
  		document.getElementById('pptpd_conflict').innerHTML = "<#vpn_conflict_LANIP#> <b>"+origin_lan_ip+"</b>";
  		return;
  }
	//DHCP pool
	if(pool_subnet == pptpd_clients_subnet
					&& ((pool_start_end >= pptpd_clients_start_ip && pool_start_end <= pptpd_clients_end_ip)								
								|| (pool_end_end >= pptpd_clients_start_ip && pool_end_end <= pptpd_clients_end_ip)								
								|| (pptpd_clients_start_ip >= pool_start_end && pptpd_clients_start_ip <= pool_end_end)
								|| (pptpd_clients_end_ip >= pool_start_end && pptpd_clients_end_ip <= pool_end_end))
					){
  		document.getElementById('pptpd_conflict').innerHTML = "<#vpn_conflict_DHCPpool#> <b>"+pool_start+" ~ "+pool_end+"</b>";
  		return;
	}
	//DHCP static IP
	if(dhcp_staticlists != ""){
			for(var i = 1; i < staticclist_row.length; i++){
					var static_subnet ="";
					var static_end ="";					
					var static_ip = staticclist_row[i].split('&#62')[1];
					static_subnet = static_ip.split(".")[0]+"."+static_ip.split(".")[1]+"."+static_ip.split(".")[2]+".";
					static_end = parseInt(static_ip.split(".")[3]);
					if(static_subnet == pptpd_clients_subnet 
  						&& static_end >= pptpd_clients_start_ip 
  						&& static_end <= pptpd_clients_end_ip){
  							document.getElementById('pptpd_conflict').innerHTML = "<#vpn_conflict_DHCPstatic#> <b>"+static_ip+"</b>";
  							return;
  				}				
  		}
	}
		
	document.getElementById('pptpd_conflict').innerHTML = "";	
}

function check_pptpd_clients_range(){
	var pptpd_clients_start_ip = parseInt(document.form._pptpd_clients_start.value.split(".")[3]);
	var pptpd_clients_end_ip = parseInt(document.form._pptpd_clients_end.value);
	
	if(pptpd_clients_start_ip > pptpd_clients_end_ip){
		alert(alert_over+document.form._pptpd_clients_start.value);
		document.form._pptpd_clients_end.focus();
    document.form._pptpd_clients_end.select();
    setEnd();
		return false;
	}
	
  if( (pptpd_clients_end_ip - pptpd_clients_start_ip) > 9 ){
      alert(alert_max);
			document.form._pptpd_clients_start.focus();
    	document.form._pptpd_clients_start.select();
    	setEnd();
      return false;
  }	
	
	return true;
}

function change_pptpd_radio(obj){
		if(obj.name == 'pptpd_dnsenable_x'){
				if(obj.value == 1){
						document.form.pptpd_dns1.parentNode.parentNode.style.display = "none";
						document.form.pptpd_dns2.parentNode.parentNode.style.display = "none";
				}else{
						document.form.pptpd_dns1.parentNode.parentNode.style.display = "";
						document.form.pptpd_dns2.parentNode.parentNode.style.display = "";					
				}
		}else if(obj.name == 'pptpd_winsenable_x'){
				if(obj.value == 1){
						document.form.pptpd_wins1.parentNode.parentNode.style.display = "none";
						document.form.pptpd_wins2.parentNode.parentNode.style.display = "none";
				}else{
						document.form.pptpd_wins1.parentNode.parentNode.style.display = "";
						document.form.pptpd_wins2.parentNode.parentNode.style.display = "";					
				}			
		}	
}

function check_dns_wins(){
	if(pptpd_dns1_orig == "" && pptpd_dns2_orig == ""){
		document.form.pptpd_dnsenable_x[0].checked = true;
		change_pptpd_radio(document.form.pptpd_dnsenable_x[0]);
	}else{
		document.form.pptpd_dnsenable_x[1].checked = true;
		change_pptpd_radio(document.form.pptpd_dnsenable_x[1]);
	}		
			
	if(pptpd_wins1_orig == "" && pptpd_wins2_orig == ""){
		document.form.pptpd_winsenable_x[0].checked = true;
		change_pptpd_radio(document.form.pptpd_winsenable_x[0]);
	}else{
		document.form.pptpd_winsenable_x[1].checked = true;
		change_pptpd_radio(document.form.pptpd_winsenable_x[1]);
	}	
		
	change_pptpd_radio(document.form.pptpd_winsenable_x);	
}

function set_Keys(auth){
	cal_panel_block();
	if((auth=='tls') || (auth=="secret")){
		$("#tlsKey_panel").fadeIn(300);
	}	
}

function cancel_Key_panel(auth){
	if((auth == 'tls') || (auth == "secret")){
			this.FromObject ="0";
			$("#tlsKey_panel").fadeOut(300);	
			if (openvpn_unit == 1) {
				setTimeout("document.getElementById('edit_vpn_crt_server1_static').value = '<% nvram_clean_get("vpn_crt_server1_static"); %>';", 300);
				setTimeout("document.getElementById('edit_vpn_crt_server1_ca').value = '<% nvram_clean_get("vpn_crt_server1_ca"); %>';", 300);
				setTimeout("document.getElementById('edit_vpn_crt_server1_crt').value = '<% nvram_clean_get("vpn_crt_server1_crt"); %>';", 300);
				setTimeout("document.getElementById('edit_vpn_crt_server1_key').value = '<% nvram_clean_get("vpn_crt_server1_key"); %>';", 300);
				setTimeout("document.getElementById('edit_vpn_crt_server1_dh').value = '<% nvram_clean_get("vpn_crt_server1_dh"); %>';", 300);
			}else{
				setTimeout("document.getElementById('edit_vpn_crt_server2_static').value = '<% nvram_clean_get("vpn_crt_server2_static"); %>';", 300);
				setTimeout("document.getElementById('edit_vpn_crt_server2_ca').value = '<% nvram_clean_get("vpn_crt_server2_ca"); %>';", 300);
				setTimeout("document.getElementById('edit_vpn_crt_server2_crt').value = '<% nvram_clean_get("vpn_crt_server2_crt"); %>';", 300);
				setTimeout("document.getElementById('edit_vpn_crt_server2_key').value = '<% nvram_clean_get("vpn_crt_server2_key"); %>';", 300);
				setTimeout("document.getElementById('edit_vpn_crt_server2_dh').value = '<% nvram_clean_get("vpn_crt_server2_dh"); %>';", 300);
			}
	}

	setTimeout("openvpn_decodeKeys(1);", 400);
}

function save_keys(auth){
	if((auth == 'tls') || (auth == "secret")){
		if (openvpn_unit == "1") {
			document.openvpn_form.vpn_crt_server1_static.value = document.getElementById('edit_vpn_crt_server1_static').value;
			document.openvpn_form.vpn_crt_server1_ca.value = document.getElementById('edit_vpn_crt_server1_ca').value;
			document.openvpn_form.vpn_crt_server1_crt.value = document.getElementById('edit_vpn_crt_server1_crt').value;
			document.openvpn_form.vpn_crt_server1_key.value = document.getElementById('edit_vpn_crt_server1_key').value;
			document.openvpn_form.vpn_crt_server1_dh.value = document.getElementById('edit_vpn_crt_server1_dh').value;
		}else{
			document.openvpn_form.vpn_crt_server2_static.value = document.getElementById('edit_vpn_crt_server2_static').value;
			document.openvpn_form.vpn_crt_server2_ca.value = document.getElementById('edit_vpn_crt_server2_ca').value;
			document.openvpn_form.vpn_crt_server2_crt.value = document.getElementById('edit_vpn_crt_server2_crt').value;
			document.openvpn_form.vpn_crt_server2_key.value = document.getElementById('edit_vpn_crt_server2_key').value;
			document.openvpn_form.vpn_crt_server2_dh.value = document.getElementById('edit_vpn_crt_server2_dh').value;
		}
		this.FromObject ="0";
		$("#tlsKey_panel").fadeOut(300);	
	}
}

function change_vpn_unit(val){
		document.openvpn_form.action_mode.value = "change_vpn_server_unit";
		document.openvpn_form.action = "apply.cgi";
        document.openvpn_form.target = "";
        document.openvpn_form.submit();
}

function cal_panel_block(){
	var blockmarginLeft;
	if (window.innerWidth)
		winWidth = window.innerWidth;
	else if ((document.body) && (document.body.clientWidth))
		winWidth = document.body.clientWidth;
		
	if (document.documentElement  && document.documentElement.clientHeight && document.documentElement.clientWidth){
		winWidth = document.documentElement.clientWidth;
	}

	if(winWidth >1050){	
		winPadding = (winWidth-1050)/2;	
		winWidth = 1105;
		blockmarginLeft= (winWidth*0.15)+winPadding;
	}
	else if(winWidth <=1050){
		blockmarginLeft= (winWidth)*0.15+document.body.scrollLeft;	

	}

	document.getElementById("tlsKey_panel").style.marginLeft = blockmarginLeft+"px";
}
</script>
</head>

<body onload="initial();">

<div id="tlsKey_panel"  class="contentM_qis" style="box-shadow: 3px 3px 10px #000;">
		<table class="QISform_wireless" border=0 align="center" cellpadding="5" cellspacing="0">
			<tr>
				<div class="description_down">Keys and Certificates</div>
			</tr>
			<tr>
				<div style="margin-left:30px; margin-top:10px;">
					<p><#vpn_openvpn_KC_Edit1#> <span style="color:#FFCC00;">----- BEGIN xxx ----- </span>/<span style="color:#FFCC00;"> ----- END xxx -----</span> <#vpn_openvpn_KC_Edit2#>
					<p>Limit: 3499 characters per field
				</div>
				<div style="margin:5px;*margin-left:-5px;"><img style="width: 730px; height: 2px;" src="/images/New_ui/export/line_export.png"></div>
			</tr>			
			<!--===================================Beginning of tls Content===========================================-->
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
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server1_static" name="edit_vpn_crt_server1_static" cols="65" maxlength="3499"><% nvram_clean_get("vpn_crt_server1_static"); %></textarea>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server2_static" name="edit_vpn_crt_server2_static" cols="65" maxlength="3499"><% nvram_clean_get("vpn_crt_server2_static"); %></textarea>
									</td>
								</tr>
								<tr id="edit_tls1">
									<th><#vpn_openvpn_KC_CA#></th>
									<td>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server1_ca" name="edit_vpn_crt_server1_ca" cols="65" maxlength="3499"><% nvram_clean_get("vpn_crt_server1_ca"); %></textarea>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server2_ca" name="edit_vpn_crt_server2_ca" cols="65" maxlength="3499"><% nvram_clean_get("vpn_crt_server2_ca"); %></textarea>
									</td>
								</tr>
								<tr id="edit_tls2">
									<th><#vpn_openvpn_KC_SA#></th>
									<td>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server1_crt" name="edit_vpn_crt_server1_crt" cols="65" maxlength="3499"><% nvram_clean_get("vpn_crt_server1_crt"); %></textarea>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server2_crt" name="edit_vpn_crt_server2_crt" cols="65" maxlength="3499"><% nvram_clean_get("vpn_crt_server2_crt"); %></textarea>
									</td>
								</tr>
								<tr id="edit_tls3">
									<th><#vpn_openvpn_KC_SK#></th>
									<td>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server1_key" name="edit_vpn_crt_server1_key" cols="65" maxlength="3499"><% nvram_clean_get("vpn_crt_server1_key"); %></textarea>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server2_key" name="edit_vpn_crt_server2_key" cols="65" maxlength="3499"><% nvram_clean_get("vpn_crt_server2_key"); %></textarea>
									</td>
								</tr>
								<tr id="edit_tls4">
									<th><#vpn_openvpn_KC_DH#></th>
									<td>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server1_dh" name="edit_vpn_crt_server1_dh" cols="65" maxlength="3499"><% nvram_clean_get("vpn_crt_server1_dh"); %></textarea>
										<textarea rows="8" class="textarea_ssh_table" id="edit_vpn_crt_server2_dh" name="edit_vpn_crt_server2_dh" cols="65" maxlength="3499"><% nvram_clean_get("vpn_crt_server2_dh"); %></textarea>
									</td>
								</tr>
							</table>
			  			</td>
			  		</tr>						
	      			</tbody>						
      				</table>
						<div style="margin-top:5px;width:100%;text-align:center;">
							<input class="button_gen" type="button" onclick="cancel_Key_panel('tls');" value="<#CTL_Cancel#>">
							<input class="button_gen" type="button" onclick="save_keys('tls');" value="Ok">	
						</div>					
          		</td>
      		</tr>
      </table>		
      <!--===================================Ending of tls Content===========================================-->			
</div>
	

<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

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
			
			<!-- form FOR PPTP VPN START -->
			<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
			<input type="hidden" name="current_page" value="Advanced_VPNAdvanced_Content.asp">
			<input type="hidden" name="next_page" value="Advanced_VPNAdvanced_Content.asp">
			<input type="hidden" name="modified" value="0">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_wait" value="5">
			<input type="hidden" name="action_script" value="restart_vpnd">
			<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
			<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
			<input type="hidden" name="wl_ssid" value="<% nvram_get("wl_ssid"); %>">
			<input type="hidden" name="pptpd_clients" value="<% nvram_get("pptpd_clients"); %>">
			<input type="hidden" name="pptpd_mppe" value="<% nvram_get("pptpd_mppe"); %>">			
			<input type="hidden" name="VPNServer_mode" value="<% nvram_get("VPNServer_mode"); %>">
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td valign="top">
						<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle">
							<tbody>
								<tr>
								<td bgcolor="#4D595D" valign="top">
								<div>&nbsp;</div>
								<div class="formfonttitle"><#BOP_isp_heart_item#> - <#vpn_Adv#></div>
								<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
								<div class="formfontdesc"><#PPTP_desc#></div>
								<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
									<thead>
										<tr>
											<td colspan="2">Server Mode</td>
										</tr>
									</thead>
									<tr>
										<th>VPN Server mode</th>
										<td>
											<select id="pptpd_mode" class="input_option" onchange="change_mode(this.value);">
												<option value="pptpd" selected>PPTP</option>
												<option value="openvpn">OpenVPN</option>
											</select>
										</td>
									</tr>

								</table>

								<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
									  	<thead>
									  		<tr>
												<td colspan="3" id="GWStatic"><#t2BC#></td>
									  		</tr>
									  	</thead>
																
										<tr>
											<th><#vpn_broadcast#></th>
											<td>
												<select name="pptpd_broadcast" class="input_option">
													<option class="content_input_fd" value="disable" <% nvram_match("pptpd_broadcast", "disable","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
													<option class="content_input_fd" value="br0"<% nvram_match("pptpd_broadcast", "br0","selected"); %>><#vpn_broadcast_opt1#></option>
													<option class="content_input_fd" value="ppp" <% nvram_match("pptpd_broadcast", "ppp","selected"); %>><#vpn_broadcast_opt2#></option>
													<option class="content_input_fd" value="br0ppp"<% nvram_match("pptpd_broadcast", "br0ppp","selected"); %>><#vpn_broadcast_opt3#></option>
												</select>			
											</td>
										</tr>
							
										<tr>
											<th><#PPPConnection_Authentication_itemname#></th>
											<td>
												<select name="pptpd_chap" class="input_option">
													<option value="0" <% nvram_match("pptpd_chap", "0","selected"); %>><#Auto#></option>
													<option value="1" <% nvram_match("pptpd_chap", "1","selected"); %>>MS-CHAPv1</option>
													<option value="2" <% nvram_match("pptpd_chap", "2","selected"); %>>MS-CHAPv2</option>
												</select>			
											</td>
										</tr>
										<tr>
											<th><#MPPE_Encryp#></th>
                    	<td>
													<input type="checkbox" class="input" name="pptpd_mppe_128" onClick="return changeMppe();">MPPE-128<br>
													<!--input type="checkbox" class="input" name="pptpd_mppe_56" onClick="return changeMppe();">MPPE-56<br-->
													<input type="checkbox" class="input" name="pptpd_mppe_40" onClick="return changeMppe();">MPPE-40<br>
													<input type="checkbox" class="input" name="pptpd_mppe_no" onClick="return changeMppe();"><#No_Encryp#>
											</td>
										</tr>
									 
										<tr>
											<th><#IPConnection_x_DNSServerEnable_itemname#></th>
											<td>
			  									<input type="radio" name="pptpd_dnsenable_x" class="input" value="1" onclick="return change_pptpd_radio(this)" /><#checkbox_Yes#>
			  									<input type="radio" name="pptpd_dnsenable_x" class="input" value="0" onclick="return change_pptpd_radio(this)" /><#checkbox_No#>
											</td>
										</tr>									 								 
									<tr>
										<th><a class="hintstyle" href="javascript:void(0);"><#IPConnection_x_DNSServer1_itemname#></a></th>
										<td><input type="text" maxlength="15" class="input_15_table" name="pptpd_dns1" value="<% nvram_get("pptpd_dns1"); %>" onkeypress="return validator.isIPAddr(this, event)" ></td>
									</tr>

									<tr>
										<th><a class="hintstyle" href="javascript:void(0);"><#IPConnection_x_DNSServer2_itemname#></a></th>
										<td><input type="text" maxlength="15" class="input_15_table" name="pptpd_dns2" value="<% nvram_get("pptpd_dns2"); %>" onkeypress="return validator.isIPAddr(this, event)" ></td>
									</tr>
									<tr>
										<th><#IPConnection_x_WINSServerEnable_itemname#></th>
										<td>
			  								<input type="radio" name="pptpd_winsenable_x" class="input" value="1" onclick="return change_pptpd_radio(this)" /><#checkbox_Yes#>
			  								<input type="radio" name="pptpd_winsenable_x" class="input" value="0" onclick="return change_pptpd_radio(this)" /><#checkbox_No#>
										</td>
									</tr>
									<tr>
										<th><#IPConnection_x_WINSServer1_itemname#></th>
										<td><input type="text" maxlength="15" class="input_15_table" name="pptpd_wins1" value="<% nvram_get("pptpd_wins1"); %>" onkeypress="return validator.isIPAddr(this, event)" ></td>
									</tr>
									<tr>
										<th><#IPConnection_x_WINSServer2_itemname#></th>
										<td><input type="text" maxlength="15" class="input_15_table" name="pptpd_wins2" value="<% nvram_get("pptpd_wins2"); %>" onkeypress="return validator.isIPAddr(this, event)" ></td>
									</tr>
<!-- Yau add mru/mtu-->
                                    <tr>
										<th><a class="hintstyle" href="javascript:void(0);">MRU</a></th>
										<td><input type="text" maxlength="4" class="input_15_table" name="pptpd_mru" value="<% nvram_get("pptpd_mru"); %>" onKeyPress="return validator.isNumber(this,event)" ></td>
                                    </tr>
                                    <tr>
										<th><a class="hintstyle" href="javascript:void(0);">MTU</a></th>
										<td><input type="text" maxlength="4" class="input_15_table" name="pptpd_mtu" value="<% nvram_get("pptpd_mtu"); %>" onKeyPress="return validator.isNumber(this,event)" ></td>
                                    </tr>
<!-- Yau -->
								<tr>
									<th><#vpn_client_ip#></th>
									<td>
										<input type="text" maxlength="15" class="input_15_table" name="_pptpd_clients_start" onBlur="setEnd();check_pptpd_clients_range();check_vpn_conflict();"  onKeyPress="return validator.isIPAddr(this, event);" value=""/> ~
										<span id="pptpd_subnet" style="font-family: Lucida Console;color: #FFF;"></span><input type="text" maxlength="3" class="input_3_table" name="_pptpd_clients_end" onBlur="check_pptpd_clients_range();check_vpn_conflict();" value=""/><span style="color:#FFCC00;"> <#vpn_maximum_clients#></span>
										<br><span id="pptpd_conflict"></span>	
									</td>
								</tr>
								</table>      			
						<!-- manually assigned the DHCP List end-->		
								<div class="apply_gen">
									<input class="button_gen" onclick="pptpd_applyRule()" type="button" value="<#CTL_apply#>"/>
								</div>		
								</td>
							</tr>

							</tbody>
						</table>
					</td>
				</tr>
			</table>	
			</form>
			<!-- form FOR PPTP VPN END -->

			<!-- form FOR OPENVPN START -->
			<form method="post" name="openvpn_form" id="openvpn_ruleForm" action="/start_apply.htm" target="hidden_frame">
			<input type="hidden" name="current_page" value="Advanced_VPNAdvanced_Content.asp">
			<input type="hidden" name="next_page" value="Advanced_VPNAdvanced_Content.asp">
			<input type="hidden" name="modified" value="0">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_script" value="">
			<input type="hidden" name="action_wait" value="10">
			<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
			<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">			
			<input type="hidden" name="vpn_serverx_start" value="<% nvram_get("vpn_serverx_start"); %>">
			<input type="hidden" name="vpn_serverx_dns" value="<% nvram_get("vpn_serverx_dns"); %>">
			<input type="hidden" name="vpn_server_ccd_val" value="<% nvram_get("vpn_server_ccd_val"); %>">
			<input type="hidden" name="vpn_crt_server1_ca" value="<% nvram_clean_get("vpn_crt_server1_ca"); %>">
			<input type="hidden" name="vpn_crt_server1_crt" value="<% nvram_clean_get("vpn_crt_server1_crt"); %>">
			<input type="hidden" name="vpn_crt_server1_key" value="<% nvram_clean_get("vpn_crt_server1_key"); %>">
			<input type="hidden" name="vpn_crt_server1_dh" value="<% nvram_clean_get("vpn_crt_server1_dh"); %>">
			<input type="hidden" name="vpn_crt_server1_static" value="<% nvram_clean_get("vpn_crt_server1_static"); %>">
			<input type="hidden" name="vpn_crt_server2_ca" value="<% nvram_clean_get("vpn_crt_server2_ca"); %>">
			<input type="hidden" name="vpn_crt_server2_crt" value="<% nvram_clean_get("vpn_crt_server2_crt"); %>">
			<input type="hidden" name="vpn_crt_server2_key" value="<% nvram_clean_get("vpn_crt_server2_key"); %>">
			<input type="hidden" name="vpn_crt_server2_dh" value="<% nvram_clean_get("vpn_crt_server2_dh"); %>">
			<input type="hidden" name="vpn_crt_server2_static" value="<% nvram_clean_get("vpn_crt_server2_static"); %>">
			<input type="hidden" name="VPNServer_mode" value="<% nvram_get("VPNServer_mode"); %>">
      		<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
        		<tr>
          			<td valign="top">
            		<table width="760px" border="0" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTitle">
                		<tbody>
                		<tr bgcolor="#4D595D">
                			<td valign="top">
                				<div>&nbsp;</div>
                				<div class="formfonttitle"><#BOP_isp_heart_item#> - <#vpn_Adv#></div>
                				<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
								<div class="formfontdesc">
									<p>In case of problem, see the <a style="font-weight: bolder;text-decoration:underline;" class="hyperlink" href="Main_LogStatus_Content.asp">System Log</a> for any error message related to openvpn.
									<p><br>Visit the OpenVPN <a style="font-weight: bolder; text-decoration:underline;" class="hyperlink" href="http://openvpn.net/index.php/open-source/downloads.html" target="_blank">Download</a> page to get the Windows client.
								</div>

								<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
									<thead>
										<tr>
											<td colspan="2">Server Mode</td>
										</tr>
									</thead>
									<tr>
										<th>VPN Server mode</th>
										<td>
											<select id="openvpn_mode" name="VPNServer_mode_select" class="input_option" onchange="change_mode(this.value);">
												<option value="pptpd">PPTP</option>
												<option value="openvpn" selected>OpenVPN</option>
											</select>
										</td>
									</tr>
									<tr id="client_unit">
										<th>Select server instance</th>
										<td>
											<select name="vpn_server_unit" class="input_option" onChange="change_vpn_unit(this.value);">
												<option value="1" <% nvram_match("vpn_server_unit","1","selected"); %> >Server 1</option>
												<option value="2" <% nvram_match("vpn_server_unit","2","selected"); %> >Server 2</option>
											</select>
										</td>
									</tr>

								</table>

								<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
									<thead>
									<tr>
											<td colspan="2"><#t2BC#></td>
									</tr>
									</thead>

									<tr>
										<th><#vpn_openvpn_interface#></th>
										<td>
											<select name="vpn_server_if" class="input_option" onclick="update_visibility();">
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
										<th><#WLANAuthentication11a_ExAuthDBPortNumber_itemname#><br><i><#Setting_factorydefault_value#> : 1194</i></th>
										<td>
											<input type="text" maxlength="5" class="input_6_table" name="vpn_server_port" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 1, 65535)" value="<% nvram_get("vpn_server_port"); %>" >
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
											<select name="vpn_server_crypt" class="input_option" onclick="update_visibility();">
												<option value="tls" <% nvram_match("vpn_server_crypt","tls","selected"); %> >TLS</option>
												<option value="secret" <% nvram_match("vpn_server_crypt","secret","selected"); %> >Static Key</option>
												<option value="custom" <% nvram_match("vpn_server_crypt","custom","selected"); %> >Custom</option>
											</select>
											<span id="server_tls_crypto_text" onclick="set_Keys('tls');" style="text-decoration:underline;cursor:pointer;">Content modification of Keys & Certificates.</span>
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

									<tr>
										<th><#vpn_openvpn_AuthHMAC#><br><i>(tls-auth)</i></th>
				        					<td>
				        						<select name="vpn_server_hmac" class="input_option">
												<option value="-1" <% nvram_match("vpn_server_hmac","-1","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
												<option value="2" <% nvram_match("vpn_server_hmac","2","selected"); %> >Bi-directional</option>
												<option value="0" <% nvram_match("vpn_server_hmac","0","selected"); %> >Incoming (0)</option>
												<option value="1" <% nvram_match("vpn_server_hmac","1","selected"); %> >Incoming (1)</option>
											</select>
			   							</td>
									</tr>
									
									<tr id="server_snnm">
										<th><#vpn_openvpn_SubnetMsak#></th>
										<td>
											<input type="text" maxlength="15" class="input_15_table" name="vpn_server_sn" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_server_sn"); %>">
											<input type="text" maxlength="15" class="input_15_table" name="vpn_server_nm" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_server_nm"); %>">
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
											<input type="text" maxlength="15" class="input_15_table" name="vpn_server_r1" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_server_r1"); %>">
											<input type="text" maxlength="15" class="input_15_table" name="vpn_server_r2" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_server_r2"); %>">
										</td>
									</tr>

									<tr id="server_local">
										<th><#vpn_openvpn_LocalRemote_IP#></th>
										<td>
											<input type="text" maxlength="15" class="input_15_table" name="vpn_server_local" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_server_local"); %>">
											<input type="text" maxlength="15" class="input_15_table" name="vpn_server_remote" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_server_remote"); %>">
										</td>
									</tr>
								</table>

								<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px;">
									<thead>
									<tr>
										<td colspan="2"><#menu5#></td>
									</tr>
									</thead>

									<tr>
										<th><#vpn_openvpn_PollInterval#><br><i>( <#zero_disable#> )</th>
										<td>
											<input type="text" maxlength="4" class="input_6_table" name="vpn_server_poll" onKeyPress="return validator.isNumber(this,event);" onblur="validate_number_range(this, 0, 1440)" value="<% nvram_get("vpn_server_poll"); %>"> <#Minute#>
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
				        						<select name="vpn_server_cipher" class="input_option">
												<option value="<% nvram_get("vpn_server_cipher"); %>" selected><% nvram_get("vpn_server_cipher"); %></option>
											</select>
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
										<th><#vpn_openvpn_TLSTime#><br><i>( <#MinusOne_default#> )</th>
										<td>
											<input type="text" maxlength="5" class="input_6_table" name="vpn_server_reneg" onblur="validator.numberRange(this, -1, 2147483647)" value="<% nvram_get("vpn_server_reneg"); %>"> <#Second#>
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

								<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" id="openvpn_client_table">
									<thead>
									<tr>
										<td colspan="5">Allowed Clients</td>
									</tr>
									</thead>

									<tr>
										<th width="36%">Common Name</th>
										<th width="20%">Subnet</th>
										<th width="20%"><#IPConnection_x_ExternalSubnetMask_itemname#></th>
										<th width="12%"><#Push#></th>

										<th width="12%"><#list_add_delete#></th>
									</tr>

									<tr>							
										<div id="VPNClientList_Block_PC" class="VPNClientList_Block_PC"></div>
										<td width="36%">
						 					<input type="text" class="input_25_table" maxlength="25" name="vpn_clientlist_commonname_0"  onKeyPress="">
						 				</td>
										<td width="20%">
						 					<input type="text" class="input_15_table" maxlength="15" name="vpn_clientlist_subnet_0"  onkeypress="return validator.isIPAddr(this, event);">
						 				</td>
										<td width="20%">
						 					<input type="text" class="input_15_table" maxlength="15" name="vpn_clientlist_netmask_0"  onkeypress="return validator.isIPAddr(this, event);">
						 				</td>
						 				<td width="12%">
											<select name="vpn_clientlist_push_0" class="input_option">
												<option value="0" selected>No</option>
												<option value="1">Yes</option>
											</select>
										</td>
						 				<td width="12%">
						 					<input class="add_btn" type="button" onClick="addRow_Group(128);" name="vpn_clientlist2" value="">
						 				</td>
									</tr>
								</table>
			     		
								<div id="openvpn_clientlist_Block"></div>

								<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
									<thead>
									<tr>
										<td colspan="2"><#vpn_openvpn_CustomConf#></td>
									</tr>
									</thead>

									<tr>
										<td colspan="2">
											<textarea rows="8" class="textarea_ssh_table" name="vpn_server_custom" cols="55" maxlength="15000"><% nvram_clean_get("vpn_server_custom"); %></textarea>
										</td>
									</tr>
								</table>

								<div class="apply_gen">
									<input name="button" type="button" class="button_gen" onclick="openvpn_applyRule();" value="<#CTL_apply#>"/>
			        			</div>
							</td>
						</tr>
	        			</tbody>
            		</table>
            
            	</td>
       		</tr>
    	</table>
    	</form>
		<!-- form FOR OPENVPN END -->
  
    </td>	
    <td width="10" align="center" valign="top">&nbsp;</td>
  </tr>
</table>



<div id="footer"></div>
</body>
</html>
