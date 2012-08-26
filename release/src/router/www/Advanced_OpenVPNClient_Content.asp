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
<title>ASUS Wireless Router <#Web_Title#> - OpenVPN Client Settings</title>
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

var vpn_clientlist_array ='<% nvram_get("vpn_client1_ccd_val"); %>';

/*** TODO

- Handle server stop/restart
- Client status
- Dropdown to select client1 or 2
- Redirect Internet trafic settings
- See if client1_bridge_warn_text involves additional code
- vpn_client1_local: double-check what must be hidden by update_visibility - we have two local
- Use unique IDs for <tr>
- vpn_client1_tlsremote/cn not hiding/showing appropriately
- Test all hide/show rules
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

	// Cipher list
	free_options(document.form.cipher_select);
	currentcipher = "<% nvram_get("vpn_client1_cipher"); %>";
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
	setRadioValue(document.form.vpn_client_eas1, ((document.form.vpn_client_eas.value.indexOf(''+(i+1)) >= 0) ? "1" : "0"));

	update_visibility();
}


function update_visibility(){

// TODO: re-check any logical op that might have been done on radioValue returns (0|1)

	fw = document.form.vpn_client1_firewall.value;
	auth = document.form.vpn_client1_crypt.value;
	iface = document.form.vpn_client1_if.value;
	bridge = getRadioValue(document.form.vpn_client1_bridge);
	nat = getRadioValue(document.form.vpn_client1_nat);
	hmac = document.form.vpn_client1_hmac.value;
	rgw = getRadioValue(document.form.vpn_client1_rgw);
	userauth = (getRadioValue(document.form.vpn_client1_userauth) == 1) && (auth == 'tls') ? 1 : 0;
	useronly = userauth && getRadioValue(document.form.vpn_client1_useronly);

	showhide("vpn_client1_userauth", ((auth == "tls") ? 1 : 0));
	showhide("vpn_client1_hmac", ((auth == "tls") ? 1 : 0));
	showhide("client1_custom_crypto_text",((auth == "custom") ? 1 : 0));
	showhide("vpn_client1_username", userauth);
	showhide("vpn_client1_password", userauth);
	showhide("vpn_client1_useronly", userauth);

	showhide("client1_ca_warn_text", useronly);
	showhide("vpn_client1_bridge", (iface == "tap") ? 1 : 0);

	showhide("client1_bridge_warn_text", (bridge == 0) ? 1 : 0);
	showhide("vpn_client1_nat", ((fw != "custom") && (iface == "tun" || bridge == 0)) ? 1 : 0);
	showhide("client1_nat_warn_text", (fw != "custom" && ((nat == 0) || (auth == "secret" && iface == "tun")))  ? 1 : 0);

	showhide("vpn_client1_local", (iface == "tun" && auth == "secret") ? 1 : 0);
	showhide("vpn_client1_local", (iface == "tap" && (bridge == 0 && auth == "secret")) ? 1 : 0);

	showhide("vpn_client1_adns", (auth == "tls")? 1 : 0);
	showhide("vpn_client1_reneg", (auth == "tls")? 1 : 0);
	showhide("client1_gateway_label", (iface == "tap" && rgw > 0) ? 1 : 0);
	showhide("vpn_client1_gw", (iface == "tap" && rgw > 0) ? 1 : 0);
	showhide("vpn_client1_tlsremote", (auth == "tls") ? 1 : 0);

	showhide("vpn_client1_cn", ((auth == "tls") && (getRadioValue(document.form.vpn_client1_tlsremote) == 1)) ? 1 : 0);
	showhide("client1_cn_label", ((auth == "tls") && (getRadioValue(document.form.vpn_client1_tlsremote) == 1)) ? 1 : 0);

}


function setRadioValue(theObj,theObjValue) {

	for (var i=0; i<theObj.length; i++) {
		if (theObj[i].value==theObjValue) {
			theObj[i].checked = true;
		}
	}
}


function getRadioValue(theObj) {

        for (var i=0; i<theObj.length; i++) {
                if (theObj[i].checked)
			return theObj[i].value;
        }
	return 0;
}


function applyRule(){

	showLoading();

// Set enabled server(s) (currently only 1 supported)
	document.form.vpn_client_eas.value = "";
	if (getRadioValue(document.form.vpn_client_eas1) == 1)
		document.form.vpn_client_eas.value += "1";

	document.form.submit();

}


</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_OpenVPNClient_Content.asp">
<input type="hidden" name="next_page" value="Advanced_OpenVPNClient_Content.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_vpnclient1">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="SystemCmd" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="vpn_client_eas" value="<% nvram_get("vpn_client_eas"); %>">
<input type="hidden" name="vpn_client_dns" value="<% nvram_get("vpn_client_dns"); %>">
<input type="hidden" name="vpn_client1_cipher" value="<% nvram_get("vpn_client1_cipher"); %>">
<input type="hidden" name="vpn_client1_ccd_val" value="<% nvram_get("vpn_client1_ccd_val"); %>">


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
                <div class="formfonttitle">OpenVPN Client Settings</div>
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
							<input type="radio" name="vpn_client_eas1" class="input" value="1"><#checkbox_Yes#>
							<input type="radio" name="vpn_client_eas1" class="input" value="0"><#checkbox_No#>
						</td>
 					</tr>

					<tr>
						<th>Interface Type</th>
			        		<td>
			       				<select name="vpn_client1_if"  onclick="update_visibility();" class="input_option">
								<option value="tap" <% nvram_match("vpn_client1_if","tap","selected"); %> >TAP</option>
								<option value="tun" <% nvram_match("vpn_client1_if","tun","selected"); %> >TUN</option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>Protocol</th>
			        		<td>
			       				<select name="vpn_client1_proto" class="input_option">
								<option value="tcp-server" <% nvram_match("vpn_client1_proto","tcp-server","selected"); %> >TCP</option>
								<option value="udp" <% nvram_match("vpn_client1_proto","udp","selected"); %> >UDP</option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>Server Address and Port</th>
						<td>
							<input type="text" maxlength="15" class="input_15_table" name="vpn_client1_addr" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_client1_addr"); %>">
							<input type="text" maxlength="5" class="input_6_table" name="vpn_client1_port" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 65535)" value="<% nvram_get("vpn_client1_port"); %>" >
						</td>
					</tr>

					<tr>
						<th>Firewall</th>
			        	<td>
			        		<select name="vpn_client1_firewall" class="input_option" onclick="update_visibility();" >
								<option value="auto" <% nvram_match("vpn_client1_firewall","auto","selected"); %> >Automatic</option>
								<option value="external" <% nvram_match("vpn_client1_firewall","external","selected"); %> >External only</option>
								<option value="custom" <% nvram_match("vpn_client1_firewall","custom","selected"); %> >Custom</option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>Authorization Mode</th>
			        	<td>
			        		<select name="vpn_client1_crypt" class="input_option" onclick="update_visibility();">
								<option value="tls" <% nvram_match("vpn_client1_crypt","tls","selected"); %> >TLS</option>
								<option value="secret" <% nvram_match("vpn_client1_crypt","secret","selected"); %> >Static Key</option>
								<option value="custom" <% nvram_match("vpn_client1_crypt","custom","selected"); %> >Custom</option>
							</select>
							<span id="client1_custom_crypto_text">(Must be manually configured!)</span>

			   			</td>
					</tr>

					<tr id="vpn_client1_userauth">
						<th>Username/Password Authentication</th>
						<td>
							<input type="radio" name="vpn_client1_userauth" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_client1_userauth", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_client1_userauth" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_client1_userauth", "0", "checked"); %>><#checkbox_No#>
						</td>
 					</tr>

					<tr id="vpn_client1_username">
						<th>Username</th>
						<td>
							<input type="text" maxlength="50" class="input_32_table" name="vpn_client1_username" value="<% nvram_get("vpn_client1_username"); %>" >
						</td>
					</tr>
					<tr id="vpn_client1_password">
						<th>Password</th>
						<td>
							<input type="text" maxlength="50" class="input_32_table" name="vpn_client1_password" value="<% nvram_get("vpn_client1_password"); %>">
						</td>
					</tr>
					<tr id="vpn_client1_useronly">
						<th>Username Auth. Only<br><i>(Must define certificate authority)</i></th>
						<td>
							<input type="radio" name="vpn_client1_useronly" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_client1_useronly", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_client1_useronly" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_client1_useronly", "0", "checked"); %>><#checkbox_No#>
							<span id="client1_ca_warn_text">Warning: You must define a Certificate Authority.</span>
						</td>
 					</tr>

					<tr id="vpn_client1_hmac">
						<th>Extra HMAC authorization<br><i>(tls-auth)</i></th>
			        	<td>
			        		<select name="vpn_client1_hmac" class="input_option">
								<option value="-1" <% nvram_match("vpn_client1_hmac","-1","selected"); %> >Disabled</option>
								<option value="2" <% nvram_match("vpn_client1_hmac","2","selected"); %> >Bi-directional</option>
								<option value="0" <% nvram_match("vpn_client1_hmac","0","selected"); %> >Incoming (0)</option>
								<option value="1" <% nvram_match("vpn_client1_hmac","1","selected"); %> >Incoming (1)</option>
							</select>
			   			</td>
					</tr>

					<tr id="vpn_client1_bridge">
						<th>Server is on the same subnet</th>
						<td>
							<input type="radio" name="vpn_client1_bridge" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_client1_bridge", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_client1_bridge" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_client1_bridge", "0", "checked"); %>><#checkbox_No#>
							<span id="client1_bridge_warn_text">Warning: Cannot bridge distinct subnets. Defaulting to routed mode.</span>
						</td>
 					</tr>

					<tr id="vpn_client1_nat">
						<th>Create NAT on tunnel<br><i>(Router must be configured manually)</i></th>
						<td>
							<input type="radio" name="vpn_client1_nat" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_client1_nat", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_client1_nat" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_client1_nat", "0", "checked"); %>><#checkbox_No#>
							<span id="client1_nat_warn_text">Routes must be configured manually.</span>

						</td>
 					</tr>

					<tr id="vpn_client1_local">
						<th>Local/remote endpoint addresses</th>
						<td>
							<input type="text" maxlength="15" class="input_15_table" name="vpn_client1_local" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_client1_local"); %>">
							<input type="text" maxlength="15" class="input_15_table" name="vpn_client1_remote" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_client1_remote"); %>">
						</td>
					</tr>

					<tr>
						<th>Tunnel address/netmask</th>
						<td>
							<input type="text" maxlength="15" class="input_15_table" name="vpn_client1_local" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_client1_local"); %>">
							<input type="text" maxlength="15" class="input_15_table" name="vpn_client1_nm" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_client1_nm"); %>">
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
							<input type="text" maxlength="4" class="input_6_table" name="vpn_client1_poll" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 0, 1440)" value="<% nvram_get("vpn_client1_poll"); %>">
						</td>
					</tr>

					<tr>
						<th>Redirect Internet traffic</th>
						<td>
							<input type="radio" name="vpn_client1_rgw" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_client1_rgw", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_client1_rgw" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_client1_rgw", "0", "checked"); %>><#checkbox_No#>
							<label style="padding-left:3em;" id="client1_gateway_label">Gateway:</label><input type="text" maxlength="15" class="input_15_table" id="vpn_client1_gw" name="vpn_client1_gw" onkeypress="return is_ipaddr(this, event);" value="<% nvram_get("vpn_client1_gw"); %>">
						</td>
 					</tr>

					<tr id="vpn_client1_adns">
						<th>Accept DNS Configuration</th>
						<td>
			        		<select name="vpn_client1_comp" class="input_option">
								<option value="0" <% nvram_match("vpn_client1_adns","0","selected"); %> >Disabled</option>
								<option value="1" <% nvram_match("vpn_client1_adns","1","selected"); %> >Relaxed</option>
								<option value="2" <% nvram_match("vpn_client1_adns","2","selected"); %> >Strict</option>
								<option value="3" <% nvram_match("vpn_client1_adns","3","selected"); %> >Exclusive</option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>Encryption cipher</th>
			        	<td>
			        		<select name="cipher_select" class="input_option">
								<option value="<% nvram_get("vpn_client1_cipher"); %>" selected><% nvram_get("vpn_client1_cipher"); %></option>
							</select>
			   			</td>
					</tr>

					<tr>
						<th>Compression</th>
			        	<td>
			        		<select name="vpn_client1_comp" class="input_option">
								<option value="-1" <% nvram_match("vpn_client1_comp","-1","selected"); %> >Disabled</option>
								<option value="no" <% nvram_match("vpn_client1_comp","no","selected"); %> >None</option>
								<option value="yes" <% nvram_match("vpn_client1_comp","yes","selected"); %> >Enabled</option>
								<option value="adaptive" <% nvram_match("vpn_client1_comp","adaptive","selected"); %> >Adaptive</option>
							</select>
			   			</td>
					</tr>

					<tr id="vpn_client1_reneg">
						<th>TLS Renegotiation Time<br><i>(in seconds, -1 for default)</th>
						<td>
							<input type="text" maxlength="5" class="input_6_table" name="vpn_client1_reneg" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, -1, 2147483647)" value="<% nvram_get("vpn_client1_reneg"); %>">
						</td>
					</tr>

					<tr>
						<th>Connection Retry<br><i>(in seconds, -1 for infinite)</th>
						<td>
							<input type="text" maxlength="5" class="input_6_table" name="vpn_client1_retry" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, -1, 32767)" value="<% nvram_get("vpn_client1_retry"); %>">
						</td>
					</tr>

					<tr id="vpn_client1_tlsremote1">
						<th>Verify Server Certificate</th>
						<td>
							<input type="radio" name="vpn_client1_tlsremote" class="input" value="1" <% nvram_match_x("", "vpn_client1_tlsremote", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_client1_tlsremote" class="input" value="0" <% nvram_match_x("", "vpn_client1_tlsremote", "0", "checked"); %>><#checkbox_No#>
							<label style="padding-left:3em;" id="client1_cn_label">Common name:</label><input type="text" maxlength="64" class="input_15_table" id="vpn_client1_cn" name="vpn_client1_cn" value="<% nvram_get("vpn_client1_cn"); %>">
						</td>
 					</tr>

					<tr>
						<th>Custom Configuration</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_client1_custom" cols="55" maxlength="1024"><% nvram_get("vpn_client1_custom"); %></textarea>
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

