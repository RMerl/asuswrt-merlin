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
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

var vpn_clientlist_array ='<% nvram_get("vpn_server1_ccd_val"); %>';

/*** TODO

- Replace "Push" dropdown with checkbox if possible
- Handle server stop/start
- Client status
- Dropdown to select server1 or 2

***/

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
	free_options(document.form.cipher_select);
	currentcipher = "<% nvram_get("vpn_server1_cipher"); %>";
	add_option(document.form.cipher_select,	"default","Default",(currentcipher == "Default"));
	add_option(document.form.cipher_select,	"none","None",(currentcipher == "none"));

	for(var i = 0; i < ciphersarray.length; i++){
		add_option(document.form.cipher_select,
			ciphersarray[i][0], ciphersarray[i][0],
			(currentcipher == ciphersarray[i][0]));
	}

	// TODO: Handle a second server instance (for now set to first one)
	i = 0;

	// Set these based on a compound field
	setRadioValue(document.form.vpn_server_eas1, ((document.form.vpn_server_eas.value.indexOf(''+(i+1)) >= 0) ? "1" : "0"));
	setRadioValue(document.form.vpn_server1_dns, ((document.form.vpn_server_dns.value.indexOf(''+(i+1)) >= 0) ? "1" : "0"));

	update_visibility();
}


function vpn_clientlist(){
	var vpn_clientlist_row = vpn_clientlist_array.split('&#60');
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="vpn_clientlist_table">';
	if(vpn_clientlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td>';
	else{
		for(var i = 1; i < vpn_clientlist_row.length; i++){
			code +='<tr id="row'+i+'">';
			var vpn_clientlist_col = vpn_clientlist_row[i].split('&#62');
			var wid=[36, 20, 20, 12];
				for(var j = 0; j < vpn_clientlist_col.length; j++){
					if (j == 3)
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
	auth = document.form.vpn_server1_crypt.value;
	iface = document.form.vpn_server1_if.value;
	hmac = document.form.vpn_server1_hmac.value;
	dhcp = document.form.vpn_server1_dhcp;
	ccd = document.form.vpn_server1_ccd;
	dns = document.form.vpn_server1_dns;

	showhide("vpn_server1_snnm", ((auth == "tls") && (iface == "tun")) ? 1 : 0);
	showhide("vpn_server1_plan", ((auth == "tls") && (iface == "tun")) ? 1 : 0);
	showhide("vpn_server1_local", ((auth == "secret") && (iface == "tun")) ? 1 : 0);
	showhide("vpn_server1_ccd", ((auth == "tls") ? 1 : 0));

	showhide("vpn_server1_c2c",getRadioValue(ccd));
	showhide("vpn_server1_ccd_excl",getRadioValue(ccd));
	showhide("vpn_client_table",getRadioValue(ccd));
	showhide("vpn_clientlist_Block",getRadioValue(ccd));

	showhide("vpn_server1_pdns", ((auth == "tls") && (getRadioValue(dns)));
	showhide("vpn_server1_dhcp",((auth == "tls") && (iface.value == "tap")) ? 1 : 0);
	showhide("vpn_server1_range",getRadioValue(dhcp));
	showhide("server1_custom_crypto_text",((auth == "custom") ? 1 : 0));
}


function addRow(obj, head){
	if(head == 1)
		vpn_clientlist_array += "&#60";
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
				vpn_clientlist_value += "&#60";
			else
				vpn_clientlist_value += "&#62";
			vpn_clientlist_value += $('vpn_clientlist_table').rows[k].cells[j].innerHTML;
		}
	}

	vpn_clientlist_array = vpn_clientlist_value;
	if(vpn_clientlist_array == "")
		vpn_clientlist();
}


function setRadioValue(obj,val) {
	for (var i=0; i<obj.length; i++) {
		if (obj[i].value==val)
			obj[i].checked = true;
	}
}


function getRadioValue(obj) {
	for (var i=0; i<obj.length; i++) {
		if (obj[i].checked)
			return obj[i].value;
	}
	return 0;
}


function applyRule(){

	showLoading();

	var client_num = $('vpn_clientlist_table').rows.length;
	var item_num = $('vpn_clientlist_table').rows[0].cells.length;
	var tmp_value = "";

	for(i=0; i<client_num; i++){
		tmp_value += "<"
		for(j=0; j<item_num-1; j++){

			if (j == 3)
				tmp_value += ($('vpn_clientlist_table').rows[i].cells[j].innerHTML == "Yes" ? 1 : 0);
			else
				tmp_value += $('vpn_clientlist_table').rows[i].cells[j].innerHTML;

			if(j != item_num-2)
				tmp_value += ">";
		}
	}

	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
		tmp_value = "";

	document.form.vpn_server1_ccd_val.value = tmp_value;

// Set enabled server(s) (currently only 1 supported)
	document.form.vpn_server_eas.value = "";
	if (getRadioValue(document.form.vpn_server_eas1) == 1)
		document.form.vpn_server_eas.value += "1";

	document.form.vpn_server_dns.value
	tmp_value = "";
	if (getRadioValue(document.form.vpn_server1_dns) == 1)
		tmp_value += "1";

	if (tmp_value != document.form.vpn_server_dns.value)
		document.form.action_script += ";restart_dnsmasq";

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
<input type="hidden" name="action_script" value="restart_vpnserver1">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="SystemCmd" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="vpn_server_eas" value="<% nvram_get("vpn_server_eas"); %>">
<input type="hidden" name="vpn_server_dns" value="<% nvram_get("vpn_server_dns"); %>">
<input type="hidden" name="vpn_server1_cipher" value="<% nvram_get("vpn_server1_cipher"); %>">
<input type="hidden" name="vpn_server1_ccd_val" value="<% nvram_get("vpn_server1_ccd_val"); %>">


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

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Basic Settings</td>
						</tr>
					</thead>
					<tr>
						<th>Start with WAN</th>
						<td>
							<input type="radio" name="vpn_server_eas1" class="input" value="1"><#checkbox_Yes#>
							<input type="radio" name="vpn_server_eas1" class="input" value="0"><#checkbox_No#>
						</td>
 					</tr>

					<tr>
						<th>Interface Type</th>
			        		<td>
			       				<select name="vpn_server1_if" class="input_option" onclick="update_visibility();">
								<option value="tap" <% nvram_match("vpn_server1_if","tap","selected"); %> >TAP</option>
								<option value="tun" <% nvram_match("vpn_server1_if","tun","selected"); %> >TUN</option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>Protocol</th>
			        		<td>
			       				<select name="vpn_server1_proto" class="input_option">
								<option value="tcp-server" <% nvram_match("vpn_server1_proto","tcp-server","selected"); %> >TCP</option>
								<option value="udp" <% nvram_match("vpn_server1_proto","udp","selected"); %> >UDP</option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>Port<br><i>Default: 1194</i></th>
						<td>
							<input type="text" maxlength="5" class="input_6_table" name="vpn_server1_port" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 65535)" value="<% nvram_get("vpn_server1_port"); %>" >
						</td>
					</tr>

					<tr>
						<th>Firewall</th>

			        	<td>
			        		<select name="vpn_server1_firewall" class="input_option">
								<option value="auto" <% nvram_match("vpn_server1_firewall","auto","selected"); %> >Automatic</option>
								<option value="external" <% nvram_match("vpn_server1_firewall","external","selected"); %> >External only</option>
								<option value="custom" <% nvram_match("vpn_server1_firewall","custom","selected"); %> >Custom</option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>Authorization Mode</th>
			        	<td>
			        		<select id="vpn_server1_crypt1" name="vpn_server1_crypt" class="input_option" onclick="update_visibility();">
								<option value="tls" <% nvram_match("vpn_server1_crypt","tls","selected"); %> >TLS</option>
								<option value="secret" <% nvram_match("vpn_server1_crypt","secret","selected"); %> >Static Key</option>
								<option value="custom" <% nvram_match("vpn_server1_crypt","custom","selected"); %> >Custom</option>
							</select>
							<span id="server1_custom_crypto_text">(Must be manually configured!)</span>
			   			</td>
					</tr>

					<tr>
						<th>Extra HMAC authorization<br><i>(tls-auth)</i></th>
			        		<td>
			        			<select name="vpn_server1_hmac" class="input_option">
								<option value="-1" <% nvram_match("vpn_server1_hmac","-1","selected"); %> >Disabled</option>
								<option value="2" <% nvram_match("vpn_server1_hmac","2","selected"); %> >Bi-directional</option>
								<option value="0" <% nvram_match("vpn_server1_hmac","0","selected"); %> >Incoming (0)</option>
								<option value="1" <% nvram_match("vpn_server1_hmac","1","selected"); %> >Incoming (1)</option>
							</select>
			   			</td>
					</tr>
					<tr id="vpn_server1_snnm">
						<th>VPN Subnet / Netmask</th>
						<td>
							<input type="text" maxlength="15" class="input_15_table" name="vpn_server1_sn" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_server1_sn"); %>">
							<input type="text" maxlength="15" class="input_15_table" name="vpn_server1_nm" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_server1_nm"); %>">
						</td>
					</tr>

					<tr id="vpn_server1_dhcp">
						<th>Allocate from DHCP</th>
						<td>
							<input type="radio" name="vpn_server1_dhcp" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_server1_dhcp", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server1_dhcp" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_server11_dhcp", "0", "checked"); %>><#checkbox_No#>
						</td>
 					</tr>

					<tr id="vpn_server1_range">
						<th>DHCP Address Pool</th>
						<td>
							<input type="text" maxlength="15" class="input_15_table" name="vpn_server1_r1" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_server1_r1"); %>">
							<input type="text" maxlength="15" class="input_15_table" name="vpn_server1_r2" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_server1_r2"); %>">
						</td>
					</tr>

					<tr id="vpn_server1_local">
						<th>Local/remote endpoint addresses</th>
						<td>
							<input type="text" maxlength="15" class="input_15_table" name="vpn_server1_local" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_server1_local"); %>">
							<input type="text" maxlength="15" class="input_15_table" name="vpn_server1_remote" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_server1_remote"); %>">
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
							<input type="text" maxlength="4" class="input_6_table" name="vpn_server1_poll" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 0, 1440)" value="<% nvram_get("vpn_server1_poll"); %>">
						</td>
					</tr>

					<tr id="vpn_server1_plan">
						<th>Push LAN to clients</th>
						<td>
							<input type="radio" name="vpn_server1_plan" class="input" value="1" <% nvram_match_x("", "vpn_server1_plan", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server1_plan" class="input" value="0" <% nvram_match_x("", "vpn_server1_plan", "0", "checked"); %>><#checkbox_No#>
						</td>
 					</tr>

					<tr>
						<th>Direct clients to redirect Internet traffic</th>
						<td>
							<input type="radio" name="vpn_server1_rgw" class="input" value="1" <% nvram_match_x("", "vpn_server1_rgw", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server1_rgw" class="input" value="0" <% nvram_match_x("", "vpn_server1_rgw", "0", "checked"); %>><#checkbox_No#>
						</td>
 					</tr>

					<tr>
						<th>Respond to DNS</th>
						<td>
							<input type="radio" name="vpn_server1_dns" class="input" value="1" onclick="update_visibility();"><#checkbox_Yes#>
							<input type="radio" name="vpn_server1_dns" class="input" value="0" onclick="update_visibility();"><#checkbox_No#>
						</td>
 					</tr>


					<tr id="vpn_server1_pdns">
						<th>Advertise DNS to clients</th>
						<td>
							<input type="radio" name="vpn_server1_pdns" class="input" value="1" <% nvram_match_x("", "vpn_server1_pdns", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server1_pdns" class="input" value="0" <% nvram_match_x("", "vpn_server1_pdns", "0", "checked"); %>><#checkbox_No#>
						</td>
 					</tr>

					<tr>
						<th>Encryption cipher</th>
			        	<td>
			        		<select name="cipher_select" class="input_option">
								<option value="<% nvram_get("vpn_server1_cipher"); %>" selected><% nvram_get("vpn_server1_cipher"); %></option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>Compression</th>
			        	<td>
			        		<select name="vpn_server1_comp" class="input_option">
								<option value="-1" <% nvram_match("vpn_server1_comp","-1","selected"); %> >Disabled</option>
								<option value="no" <% nvram_match("vpn_server1_comp","no","selected"); %> >None</option>
								<option value="yes" <% nvram_match("vpn_server1_comp","yes","selected"); %> >Enabled</option>
								<option value="adaptive" <% nvram_match("vpn_server1_comp","adaptive","selected"); %> >Adaptive</option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>TLS Renegotiation Time<br><i>(in seconds, -1 for default)</th>
						<td>
							<input type="text" maxlength="5" class="input_6_table" name="vpn_server1_reneg" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, -1, 2147483647)" value="<% nvram_get("vpn_server1_reneg"); %>">
						</td>
					</tr>

					<tr id="vpn_server1_ccd">
						<th>Manage Client-Specific Options</th>
						<td>
							<input type="radio" name="vpn_server1_ccd" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_server1_ccd", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server1_ccd" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_server1_ccd", "0", "checked"); %>><#checkbox_No#>
						</td>
 					</tr>

					<tr id="vpn_server1_c2c">
						<th>Allow Client &lt;-&gt; Client</th>
						<td>
							<input type="radio" name="vpn_server1_c2c" class="input" value="1" <% nvram_match_x("", "vpn_server1_c2c", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server1_c2c" class="input" value="0" <% nvram_match_x("", "vpn_server1_c2c", "0", "checked"); %>><#checkbox_No#>
						</td>
					</tr>

					<tr id="vpn_server1_ccd_excl">
						<th>Allow only specified clients</th>
						<td>
							<input type="radio" name="vpn_server1_ccd_excl" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_server1_ccd_excl", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_server1_ccd_excl" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_server1_ccd_excl", "0", "checked"); %>><#checkbox_No#>
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
							<textarea rows="8" class="textarea_ssh_table" name="vpn_server1_custom" cols="55" maxlength="1024"><% nvram_get("vpn_server1_custom"); %></textarea>
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

