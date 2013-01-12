<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>ASUS Wireless Router <#Web_Title#> - OpenVPN Server Settings</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script type="text/javascript" language="JavaScript" src="/detect.js"></script>
<script type="text/javascript" language="JavaScript" src="/merlin.js"></script>

<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();

wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
<% vpn_server_get_parameter(); %>;

var vpn_clientlist_array ='<% nvram_get("vpn_server_ccd_val"); %>';
vpn_unit = '<% nvram_get("vpn_server_unit"); %>';

if (vpn_unit == 1)
	var service_state = (<% sysinfo("pid.vpnserver1"); %> > 0);
else if (vpn_unit == 2)
	var service_state = (<% sysinfo("pid.vpnserver2"); %> > 0);
else
	var service_state = false;

ciphersarray = [
		["AES-128-CBC"],
		["AES-128-CFB"],
		["AES-128-OFB"],
		["AES-192-CBC"],
		["AES-192-CFB"],
		["AES-192-OFB"],
		["AES-256-CBC"],
		["AES-256-CFB"],
		["AES-256-OFB"],
		["BF-CBC"],
		["BF-CFB"],
		["BF-OFB"],
		["CAST5-CBC"],
		["CAST5-CFB"],
		["CAST5-OFB"],
		["DES-CBC"],
		["DES-CFB"],
		["DES-EDE3-CBC"],
		["DES-EDE3-CFB"],
		["DES-EDE3-OFB"],
		["DES-EDE-CBC"],
		["DES-EDE-CFB"],
		["DES-EDE-OFB"],
		["DES-OFB"],
		["DESX-CBC"],
		["IDEA-CBC"],
		["IDEA-CFB"],
		["IDEA-OFB"],
		["RC2-40-CBC"],
		["RC2-64-CBC"],
		["RC2-CBC"],
		["RC2-CFB"],
		["RC2-OFB"],
		["RC5-CBC"],
		["RC5-CBC"],
		["RC5-CFB"],
		["RC5-CFB"],
		["RC5-OF"]
];

function initial()
{
	show_menu();

	vpn_clientlist();

	// Cipher list
	free_options(document.form.vpn_server_cipher);
	currentcipher = "<% nvram_get("vpn_server_cipher"); %>";
	add_option(document.form.vpn_server_cipher, "Default","default",(currentcipher == "Default"));
	add_option(document.form.vpn_server_cipher, "None","none",(currentcipher == "none"));

	for(var i = 0; i < ciphersarray.length; i++){
		add_option(document.form.vpn_server_cipher,
			ciphersarray[i][0], ciphersarray[i][0],
			(currentcipher == ciphersarray[i][0]));
	}

	// Set these based on a compound field
	setRadioValue(document.form.vpn_server_x_eas, ((document.form.vpn_serverx_eas.value.indexOf(''+(vpn_unit)) >= 0) ? "1" : "0"));
	setRadioValue(document.form.vpn_server_x_dns, ((document.form.vpn_serverx_dns.value.indexOf(''+(vpn_unit)) >= 0) ? "1" : "0"));

	update_visibility();
}


function vpn_clientlist(){
	var vpn_clientlist_row = vpn_clientlist_array.split('&#60');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="vpn_clientlist_table">';
	if(vpn_clientlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td>';
	else{
		for (var i = 1; i < vpn_clientlist_row.length; i++){
			code +='<tr id="row'+i+'">';
			var vpn_clientlist_col = vpn_clientlist_row[i].split('&#62');
			var wid=[0, 36, 20, 20, 12];

				//skip field[0] as it contains "1" for "Enabled".
				for (var j = 1; j < vpn_clientlist_col.length; j++){
					if (j == 4)
						code +='<td width="'+wid[j]+'%">'+ (vpn_clientlist_col[j] == 1 ? 'Yes' : 'No') +'</td>';
					else
						code +='<td width="'+wid[j]+'%">'+ vpn_clientlist_col[j] +'</td>';

				}
				code +='<td width="12%"><!--input class="edit_btn" onclick="edit_Row(this);" value=""/-->';
				code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td>';
		}
	}
	code +='</table>';
	$("vpn_clientlist_Block").innerHTML = code;
}


function update_visibility(){
	auth = document.form.vpn_server_crypt.value;
	iface = document.form.vpn_server_if.value;
	hmac = document.form.vpn_server_hmac.value;
	dhcp = getRadioValue(document.form.vpn_server_dhcp);
	ccd = getRadioValue(document.form.vpn_server_ccd);
	dns = getRadioValue(document.form.vpn_server_x_dns);

	showhide("server_snnm", ((auth == "tls") && (iface == "tun")) ? 1 : 0);
	showhide("server_plan", ((auth == "tls") && (iface == "tun")) ? 1 : 0);
	showhide("server_local", ((auth == "secret") && (iface == "tun")) ? 1 : 0);
	showhide("server_ccd", (auth == "tls") ? 1 : 0);

	showhide("server_c2c", ccd);
	showhide("server_ccd_excl", ccd);
	showhide("vpn_client_table", ccd);
	showhide("vpn_clientlist_Block", ccd);

	showhide("server_pdns", ((auth == "tls") && (dns == 1)) ? 1 : 0);
	showhide("server_dhcp",((auth == "tls") && (iface == "tap")) ? 1 : 0);
	showhide("server_range", (dhcp == 0) ? 1 : 0);
	showhide("server_custom_crypto_text", (auth == "custom") ? 1 : 0);
}


function addRow(obj, head){
	if(head == 1)
		vpn_clientlist_array += "&#601&#62";
	else
		vpn_clientlist_array += "&#62";

	vpn_clientlist_array += obj.value;
	obj.value = "";
}


function addRow_Group(upper){
	var client_num = $('vpn_clientlist_table').rows.length;
	var item_num = $('vpn_clientlist_table').rows[0].cells.length;

	if(client_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;
	}

	if(document.form.vpn_clientlist_commonname_0.value=="" || document.form.vpn_clientlist_subnet_0.value=="" ||
			document.form.vpn_clientlist_netmask_0.value==""){
					alert("<#JS_fieldblank#>");
					document.form.vpn_clientlist_commonname_0.focus();
					document.form.vpn_clientlist_commonname_0.select();
					return false;
	}

// Check for duplicate

	if(item_num >=2){
		for(i=0; i<client_num; i++){
			if(document.form.vpn_clientlist_commonname_0.value.toLowerCase() == $('vpn_clientlist_table').rows[i].cells[0].innerHTML.toLowerCase()){
				alert('<#JS_duplicate#>');
				document.form.vpn_clientlist__0.value =="";
				return false;
			}
		}
	}
	Do_addRow_Group();
}


function Do_addRow_Group(){
	addRow(document.form.vpn_clientlist_commonname_0 ,1);
	addRow(document.form.vpn_clientlist_subnet_0, 0);
	addRow(document.form.vpn_clientlist_netmask_0, 0);
	addRow(document.form.vpn_clientlist_push_0, 0);

	document.form.vpn_clientlist_push_0.value="0";

	vpn_clientlist();
}

function edit_Row(r){
	var i=r.parentNode.parentNode.rowIndex;

	document.form.vpn_clientlist_commonname_0.value = $('vpn_clientlist_table').rows[i].cells[0].innerHTML;
	document.form.vpn_clientlist_subnet_0.value = $('vpn_clientlist_table').rows[i].cells[1].innerHTML;
	document.form.vpn_clientlist_netmask_0.value = $('vpn_clientlist_table').rows[i].cells[2].innerHTML;
	document.form.vpn_clientlist_push_0.value = $('vpn_clientlist_table').rows[i].cells[3].innerHTML;

	del_Row(r);
}


function del_Row(r){
	var i=r.parentNode.parentNode.rowIndex;
	$('vpn_clientlist_table').deleteRow(i);

	var vpn_clientlist_value = "";
	for(k=0; k<$('vpn_clientlist_table').rows.length; k++){
		for(j=0; j<$('vpn_clientlist_table').rows[k].cells.length-1; j++){
			if(j == 0)
				vpn_clientlist_value += "&#601&#62";
			else
				vpn_clientlist_value += "&#62";
			vpn_clientlist_value += $('vpn_clientlist_table').rows[k].cells[j].innerHTML;
		}
	}

	vpn_clientlist_array = vpn_clientlist_value;
	if(vpn_clientlist_array == "")
		vpn_clientlist();
}

function applyRule(){

	showLoading();

	if (service_state) {
		document.form.action_script.value = "restart_vpnserver"+vpn_unit;
	}

	var client_num = $('vpn_clientlist_table').rows.length;
	var item_num = $('vpn_clientlist_table').rows[0].cells.length;
	var tmp_value = "";

	for(i=0; i<client_num; i++){

		// Insert first field (1), which enables the client.
		tmp_value += "<1>"
		for(j=0; j<item_num-1; j++){

			if (j == 3)
				tmp_value += ($('vpn_clientlist_table').rows[i].cells[j].innerHTML == "Yes" ? 1 : 0);
			else
				tmp_value += $('vpn_clientlist_table').rows[i].cells[j].innerHTML;

			if(j != item_num-2)
				tmp_value += ">";
		}
	}
	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<1>")
		tmp_value = "";

	document.form.vpn_server_ccd_val.value = tmp_value;


	tmp_value = "";

	for (var i=1; i < 3; i++) {
		if (i == vpn_unit) {
			if (getRadioValue(document.form.vpn_server_x_eas) == 1)
				tmp_value += ""+i+",";
		} else {
			if (document.form.vpn_serverx_eas.value.indexOf(''+(i)) >= 0)
				tmp_value += ""+i+","
		}
	}
	document.form.vpn_serverx_eas.value = tmp_value;

	tmp_value = "";

	for (var i=1; i < 3; i++) {
		if (i == vpn_unit) {
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
	document.form.submit();

}


function change_vpn_unit(val){
        FormActions("apply.cgi", "change_vpn_server_unit", "", "");
        document.form.target = "";
        document.form.submit();
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_OpenVPNServer_Content.asp">
<input type="hidden" name="next_page" value="Advanced_OpenVPNServer_Content.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="SystemCmd" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="vpn_serverx_eas" value="<% nvram_get("vpn_serverx_eas"); %>">
<input type="hidden" name="vpn_serverx_dns" value="<% nvram_get("vpn_serverx_dns"); %>">
<input type="hidden" name="vpn_server_ccd_val" value="<% nvram_get("vpn_server_ccd_val"); %>">


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
          <td valign="top">
            <table width="760px" border="0" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTitle" id="FormTitle">
                <tbody>
                <tr bgcolor="#4D595D">
                <td valign="top">
                <div>&nbsp;</div>
                <div class="formfonttitle">OpenVPN Server Settings</div>
                <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		<div class="formfontdesc">
			<p>Before starting the service make sure you properly configure it, including
			   the required <a style="font-weight: bolder;text-decoration:underline;" class="hyperlink" href="Advanced_OpenVPN_Keys.asp">keys</a>,<br> otherwise you will be unable to turn it on.
			<p><br>In case of problem, see the <a style="font-weight: bolder;text-decoration:underline;" class="hyperlink" href="Main_LogStatus_Content.asp">System Log</a> for any error message related to openvpn.
			<p><br>Visit the OpenVPN <a style="font-weight: bolder; text-decoration:underline;" class="hyperlink" href="http://openvpn.net/index.php/open-source/downloads.html" target="_blank">Download</a> page to get the Windows client.
		</div>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Basic Settings</td>
						</tr>
					</thead>

					<tr id="client_unit">
						<th>Select server instance</th>
						<td>
			        		<select name="vpn_server_unit" class="input_option" onChange="change_vpn_unit(this.value);">
								<option value="1" <% nvram_match("vpn_server_unit","1","selected"); %> >Server 1</option>
								<option value="2" <% nvram_match("vpn_server_unit","2","selected"); %> >Server 2</option>
							</select>
			   			</td>
					</tr>
					<tr id="service_enable_button">
						<th>Service state</th>
						<td>
							<div class="left" style="width:94px; float:left; cursor:pointer;" id="radio_service_enable"></div>
							<script type="text/javascript">

								$j('#radio_service_enable').iphoneSwitch(service_state,
									function() {
										document.form.action_script.value = "start_vpnserver"+vpn_unit;
										parent.showLoading();
										document.form.submit();
										return true;
									},
									function() {
										document.form.action_script.value = "stop_vpnserver"+vpn_unit;
										parent.showLoading();
										document.form.submit();
										return true;
									},
									{
										switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
									}
								);
							</script>
							<span>Warning: any unsaved change will be lost.</span>
						</td>
					</tr>

					<tr>
						<th>Start with WAN</th>
						<td>
							<input type="radio" name="vpn_server_x_eas" class="input" value="1"><#checkbox_Yes#>
							<input type="radio" name="vpn_server_x_eas" class="input" value="0"><#checkbox_No#>
						</td>
 					</tr>

					<tr>
						<th>Interface Type</th>
			        		<td>
			       				<select name="vpn_server_if" class="input_option" onclick="update_visibility();">
								<option value="tap" <% nvram_match("vpn_server_if","tap","selected"); %> >TAP</option>
								<option value="tun" <% nvram_match("vpn_server_if","tun","selected"); %> >TUN</option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>Protocol</th>
			        		<td>
			       				<select name="vpn_server_proto" class="input_option">
								<option value="tcp-server" <% nvram_match("vpn_server_proto","tcp-server","selected"); %> >TCP</option>
								<option value="udp" <% nvram_match("vpn_server_proto","udp","selected"); %> >UDP</option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>Port<br><i>Default: 1194</i></th>
						<td>
							<input type="text" maxlength="5" class="input_6_table" name="vpn_server_port" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 65535)" value="<% nvram_get("vpn_server_port"); %>" >
						</td>
					</tr>

					<tr>
						<th>Firewall</th>

			        	<td>
			        		<select name="vpn_server_firewall" class="input_option">
								<option value="auto" <% nvram_match("vpn_server_firewall","auto","selected"); %> >Automatic</option>
								<option value="external" <% nvram_match("vpn_server_firewall","external","selected"); %> >External only</option>
								<option value="custom" <% nvram_match("vpn_server_firewall","custom","selected"); %> >Custom</option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>Authorization Mode</th>
			        	<td>
			        		<select name="vpn_server_crypt" class="input_option" onclick="update_visibility();">
								<option value="tls" <% nvram_match("vpn_server_crypt","tls","selected"); %> >TLS</option>
								<option value="secret" <% nvram_match("vpn_server_crypt","secret","selected"); %> >Static Key</option>
								<option value="custom" <% nvram_match("vpn_server_crypt","custom","selected"); %> >Custom</option>
							</select>
							<span id="server_custom_crypto_text">Must be manually configured.</span>
			   			</td>
					</tr>

					<tr>
						<th>Extra HMAC authorization<br><i>(tls-auth)</i></th>
			        		<td>
			        			<select name="vpn_server_hmac" class="input_option">
								<option value="-1" <% nvram_match("vpn_server_hmac","-1","selected"); %> >Disabled</option>
								<option value="2" <% nvram_match("vpn_server_hmac","2","selected"); %> >Bi-directional</option>
								<option value="0" <% nvram_match("vpn_server_hmac","0","selected"); %> >Incoming (0)</option>
								<option value="1" <% nvram_match("vpn_server_hmac","1","selected"); %> >Incoming (1)</option>
							</select>
			   			</td>
					</tr>
					<tr id="server_snnm">
						<th>VPN Subnet / Netmask</th>
						<td>
							<input type="text" maxlength="15" class="input_15_table" name="vpn_server_sn" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_server_sn"); %>">
							<input type="text" maxlength="15" class="input_15_table" name="vpn_server_nm" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_server_nm"); %>">
						</td>
					</tr>

					<tr id="server_dhcp">
						<th>Allocate from DHCP</th>
						<td>
							<input type="radio" name="vpn_server_dhcp" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_dhcp", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server_dhcp" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_dhcp", "0", "checked"); %>><#checkbox_No#>
						</td>
 					</tr>

					<tr id="server_range">
						<th>Client Address Pool</th>
						<td>
							<input type="text" maxlength="15" class="input_15_table" name="vpn_server_r1" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_server_r1"); %>">
							<input type="text" maxlength="15" class="input_15_table" name="vpn_server_r2" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_server_r2"); %>">
						</td>
					</tr>

					<tr id="server_local">
						<th>Local/remote endpoint addresses</th>
						<td>
							<input type="text" maxlength="15" class="input_15_table" name="vpn_server_local" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_server_local"); %>">
							<input type="text" maxlength="15" class="input_15_table" name="vpn_server_remote" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_server_remote"); %>">
						</td>
					</tr>
				</table>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Advanced Settings</td>
						</tr>
					</thead>

					<tr>
						<th>Poll Interval<br><i>(in minutes, 0 to disable)</th>
						<td>
							<input type="text" maxlength="4" class="input_6_table" name="vpn_server_poll" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 0, 1440)" value="<% nvram_get("vpn_server_poll"); %>">
						</td>
					</tr>

					<tr id="server_plan">
						<th>Push LAN to clients</th>
						<td>
							<input type="radio" name="vpn_server_plan" class="input" value="1" <% nvram_match_x("", "vpn_server_plan", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server_plan" class="input" value="0" <% nvram_match_x("", "vpn_server_plan", "0", "checked"); %>><#checkbox_No#>
						</td>
 					</tr>

					<tr>
						<th>Direct clients to redirect Internet traffic</th>
						<td>
							<input type="radio" name="vpn_server_rgw" class="input" value="1" <% nvram_match_x("", "vpn_server_rgw", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server_rgw" class="input" value="0" <% nvram_match_x("", "vpn_server_rgw", "0", "checked"); %>><#checkbox_No#>
						</td>
 					</tr>

					<tr>
						<th>Respond to DNS</th>
						<td>
							<input type="radio" name="vpn_server_x_dns" class="input" value="1" onclick="update_visibility();"><#checkbox_Yes#>
							<input type="radio" name="vpn_server_x_dns" class="input" value="0" onclick="update_visibility();"><#checkbox_No#>
						</td>
 					</tr>


					<tr id="server_pdns">
						<th>Advertise DNS to clients</th>
						<td>
							<input type="radio" name="vpn_server_pdns" class="input" value="1" <% nvram_match_x("", "vpn_server_pdns", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server_pdns" class="input" value="0" <% nvram_match_x("", "vpn_server_pdns", "0", "checked"); %>><#checkbox_No#>
						</td>
 					</tr>

					<tr>
						<th>Encryption cipher</th>
			        	<td>
			        		<select name="vpn_server_cipher" class="input_option">
								<option value="<% nvram_get("vpn_server_cipher"); %>" selected><% nvram_get("vpn_server_cipher"); %></option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>Compression</th>
			        	<td>
			        		<select name="vpn_server_comp" class="input_option">
								<option value="-1" <% nvram_match("vpn_server_comp","-1","selected"); %> >Disabled</option>
								<option value="no" <% nvram_match("vpn_server_comp","no","selected"); %> >None</option>
								<option value="yes" <% nvram_match("vpn_server_comp","yes","selected"); %> >Enabled</option>
								<option value="adaptive" <% nvram_match("vpn_server_comp","adaptive","selected"); %> >Adaptive</option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>TLS Renegotiation Time<br><i>(in seconds, -1 for default)</th>
						<td>
							<input type="text" maxlength="5" class="input_6_table" name="vpn_server_reneg" onblur="validate_range(this, -1, 2147483647)" value="<% nvram_get("vpn_server_reneg"); %>">
						</td>
					</tr>

					<tr id="server_ccd">
						<th>Manage Client-Specific Options</th>
						<td>
							<input type="radio" name="vpn_server_ccd" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_ccd", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server_ccd" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_ccd", "0", "checked"); %>><#checkbox_No#>
						</td>
 					</tr>

					<tr id="server_c2c">
						<th>Allow Client &lt;-&gt; Client</th>
						<td>
							<input type="radio" name="vpn_server_c2c" class="input" value="1" <% nvram_match_x("", "vpn_server_c2c", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server_c2c" class="input" value="0" <% nvram_match_x("", "vpn_server_c2c", "0", "checked"); %>><#checkbox_No#>
						</td>
					</tr>

					<tr id="server_ccd_excl">
						<th>Allow only specified clients</th>
						<td>
							<input type="radio" name="vpn_server_ccd_excl" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_ccd_excl", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server_ccd_excl" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_server_ccd_excl", "0", "checked"); %>><#checkbox_No#>
						</td>
 					</tr>

					</table>

					<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" id="vpn_client_table">
						<thead>
						<tr>
							<td colspan="5">Allowed Clients</td>
						</tr>
						</thead>

						<tr>
							<th width="36%">Common Name</th>
							<th width="20%">Subnet</th>
							<th width="20%">Netmask</th>
							<th width="12%">Push</th>
							<th width="12%">Add / Delete</th>
						</tr>

						<tr>
							<!-- client info -->
							<div id="VPNClientList_Block_PC" class="VPNClientList_Block_PC"></div>

							<td width="36%">
						 		<input type="text" class="input_25_table" maxlength="25" name="vpn_clientlist_commonname_0"  onKeyPress="">
						 	</td>
							<td width="20%">
						 		<input type="text" class="input_15_table" maxlength="15" name="vpn_clientlist_subnet_0"  onkeypress="return is_ipaddr(this, event);">
						 	</td>
							<td width="20%">
						 		<input type="text" class="input_15_table" maxlength="15" name="vpn_clientlist_netmask_0"  onkeypress="return is_ipaddr(this, event);">
						 	</td>
						 	<td width="12%">
								<select name="vpn_clientlist_push_0" class="input_option">
									<option value="0" selected>No</option>
									<option value="1">Yes</option>
								</select>
							</td>
						 	<td width="12%">
						 		<input class="add_btn" type="button" onClick="addRow_Group(32);" name="vpn_clientlist2" value="">
						 	</td>
						</tr>
					</table>
			     	<div id="vpn_clientlist_Block"></div>

					<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" id="vpn_client_table">
						<thead>
						<tr>
							<td colspan="2">Custom configuration</td>
						</tr>
						</thead>
						<tr>
						<td colspan="2">
							<textarea rows="8" class="textarea_ssh_table" name="vpn_server_custom" cols="55" maxlength="15000"><% nvram_clean_get("vpn_server_custom"); %></textarea>
						</td>
					</tr>

					</table>

					<div class="apply_gen">
						<input name="button" type="button" class="button_gen" onclick="applyRule();" value="<#CTL_apply#>"/>
			        </div>
				</td></tr>
	        </tbody>
            </table>
            </form>
            </td>

       </tr>
      </table>
      <!--===================================Ending of Main Content===========================================-->
    </td>
    <td width="10" align="center" valign="top">&nbsp;</td>
  </tr>
</table>
<div id="footer"></div>
</body>
</html>

