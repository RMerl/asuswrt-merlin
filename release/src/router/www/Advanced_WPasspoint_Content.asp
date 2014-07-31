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
<title><#Web_Title#> - Passpoint</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

<% login_state_hook(); %>
<% wl_get_parameter(); %>

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var radio_2 = '<% nvram_get("wl0_radio"); %>';
var wl0_nmode_x = '<% nvram_get("wl0_nmode_x"); %>';
var wl0_macmode = '<% nvram_get("wl0_macmode"); %>';
if(band5g_support){
	var radio_5 = '<% nvram_get("wl1_radio"); %>';
	var wl1_nmode_x = '<% nvram_get("wl1_nmode_x"); %>';
	var wl1_macmode = '<% nvram_get("wl1_macmode"); %>';
}	

function initial(){
	show_menu();
	
	$('ACL_disabled_hint').innerHTML = Untranslated.Guest_Network_enable_ACL;
	$('enable_macfilter').innerHTML = "<#enable_macmode#>";	
	if("<% nvram_get("wl_unit"); %>" == "0" && radio_2 != 1)
		$('2g_radio_hint').style.display ="";
	if(!band5g_support){
		$("wl_unit_field").style.display = "none";		
	}
	else{
		if("<% nvram_get("wl_unit"); %>" == "1" && radio_5 != 1)
			$('5g_radio_hint').style.display ="";
	}

	// special case after modifing GuestNetwork/Passpoint
	if("<% nvram_get("wl_unit"); %>" == "-1" && "<% nvram_get("wl_subunit"); %>" == "-1"){
		change_wl_unit();
	}

	if(document.form.preferred_lang.value == "JP"){    //use unique font-family for JP
		$('2g_radio_hint').style.fontFamily = "MS UI Gothic,MS P Gothic";
		$('5g_radio_hint').style.fontFamily = "MS UI Gothic,MS P Gothic";		
	}	
	$("wl_channel_field").style.display = "none";
	$("wl_nctrlsb_field").style.display = "none";
						
	if(sw_mode == "3") inputCtrl(document.form.wl_lanaccess, 0);
				
	change_guest_unit("<% nvram_get("wl_unit"); %>", 3);	
	if(document.form.wl_gmode_protection.value == "auto")
		document.form.wl_gmode_check.checked = true;
	else
		document.form.wl_gmode_check.checked = false;	
				
}

function applyRule(){
	if(validForm()){				

		if(document.form.wl_hs2en_x[1].checked)
			document.form.wl_hs2en.value = "0";
		else
			document.form.wl_hs2en.value = "1";				

		showLoading();
		document.form.submit();
	}
}

function validForm(){		
	return true;
}

function goToACLFilter(){
	if(sw_mode == 2 || sw_mode == 4) return false;
	
	var page_temp = "";
	document.titleForm.wl_unit.disabled = false;
	document.titleForm.wl_unit.value = document.form.wl_unit.value;
	var macmode;
	if(document.form.wl_unit.value == "1")
		macmode = wl1_macmode;
	else
		macmode = wl0_macmode;
	if(macmode == "disabled")
		page_temp = "Advanced_ACL_Content.asp?af=enable_mac";
	else 
		page_temp = "Advanced_ACL_Content.asp?af=wl_maclist_x_0";
		
	if(document.titleForm.current_page.value == "")
		document.titleForm.current_page.value = page_temp;
	if(document.titleForm.next_page.value == "")
		document.titleForm.next_page.value = page_temp;
			
	document.titleForm.action_mode.value = "change_wl_unit";
	document.titleForm.action = "apply.cgi";
	document.titleForm.target = "";
	document.titleForm.submit();
}

function change_guest_unit(_unit, _subunit){
	var gn_array, macmode, idx;
	if(_unit == "1")
	{
		gn_array = gn_array_5g;
		macmode = wl1_macmode;
		document.form.wl_nmode_x.value = wl1_nmode_x;
		if("<% nvram_get("wl1.3_hs2en"); %>" == "1")
			document.form.wl_hs2en_x[0].checked = true;
		else
			document.form.wl_hs2en_x[1].checked = true;
	}
	else
	{	
		gn_array = gn_array_2g;
		document.form.wl_nmode_x.value = wl0_nmode_x;
		if("<% nvram_get("wl0.3_hs2en"); %>" == "1")
			document.form.wl_hs2en_x[0].checked = true;
		else
			document.form.wl_hs2en_x[1].checked = true;
	}
	idx = _subunit - 1;
	limit_auth_method(_unit);	
	document.form.wl_unit.value = _unit;
	document.form.wl_subunit.value = _subunit;	
	//$("wl_vifname").innerHTML = document.form.wl_subunit.value;	
	document.form.wl_bss_enabled.value = decodeURIComponent(gn_array[idx][0]);
	document.form.wl_ssid.value = decodeURIComponent(gn_array[idx][1]);
	document.form.wl_auth_mode_x.value = decodeURIComponent(gn_array[idx][2]);
	wl_auth_mode_change(1);
	document.form.wl_crypto.value = decodeURIComponent(gn_array[idx][3]);
	document.form.wl_wpa_psk.value = decodeURIComponent(gn_array[idx][4]);
	document.form.wl_wep_x.value = decodeURIComponent(gn_array[idx][5]);
	document.form.wl_key.value = decodeURIComponent(gn_array[idx][6]);
	document.form.wl_key1.value = decodeURIComponent(gn_array[idx][7]);
	document.form.wl_key2.value = decodeURIComponent(gn_array[idx][8]);
	document.form.wl_key3.value = decodeURIComponent(gn_array[idx][9]);
	document.form.wl_key4.value = decodeURIComponent(gn_array[idx][10]);
	//document.form.wl_expire.value = decodeURIComponent(gn_array[idx][11]);
	document.form.wl_lanaccess.value = decodeURIComponent(gn_array[idx][12]);	

	wl_wep_change();
	//change_wl_expire_radio();

}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_WPasspoint_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WPasspoint_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_subunit" value="3"><!-- tmo_support: RT-AC68U fixed wlx.3 -->
<input type="hidden" name="wl_hs2en" value="">
<input type="hidden" name="wl_ssid_org" value="<% nvram_char_to_ascii("WLANConfig11b",  "wl_ssid"); %>">
<input type="hidden" name="wl_wpa_psk_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_wpa_psk"); %>">
<input type="hidden" name="wl_key1_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key1"); %>">
<input type="hidden" name="wl_key2_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key2"); %>">
<input type="hidden" name="wl_key3_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key3"); %>">
<input type="hidden" name="wl_key4_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key4"); %>">
<input type="hidden" name="wl_phrase_x_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_phrase_x"); %>">
<input type="hidden" name="wl_gmode_protection" value="<% nvram_get("wl_gmode_protection"); %>" disabled>

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
		
<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
<tbody>
		<tr>
		  <td bgcolor="#4D595D" valign="top"  >
		  <div>&nbsp;</div>
		  <div class="formfonttitle">Passpoint (Hotspot) - Passpoint</div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc">Passpoint transforms the way users connect to Wi-Fi. This technology makes the process of finding and connecting to Wi-Fi networks seamless. Passpoint also delivers greater security protection to better safeguard your data. Your T-Mobile SIM card provides the necessary authentication to automatically connect your device to the <#Web_Title2#> Router.
				<br><br>If your T-Mobile handset is Passpoint capable, it should automatically connect to your <#Web_Title2#> Router.
		  </div>
		  
					<!-- setting table -->
			<table width="99%" border="1" align="center" style="margin-top:10px;margin-bottom:20px;" cellpadding="4" cellspacing="0" id="gnset_table" class="FormTable">
				<tr id="wl_unit_field">
					<th><#Interface#></th>
					<td>
						<select name="wl_unit" class="input_option" onChange="change_wl_unit();">
							<option class="content_input_fd" value="0" <% nvram_match("wl_unit", "0","selected"); %>>2.4GHz</option>
							<option class="content_input_fd" value="1" <% nvram_match("wl_unit", "1","selected"); %>>5GHz</option>
						</select>
						<br>
						<span id="2g_radio_hint" style="font-size: 14px;display:none;color:#FC0;margin-left:5px;">* <#GuestNetwork_Radio_Status#>	<a style="font-family:Lucida Console;color:#FC0;text-decoration:underline;cursor:pointer;" onclick="_change_wl_unit_status(0);"><#btn_go#></a></span>
						<span id="5g_radio_hint" style="font-size: 14px;display:none;color:#FC0;margin-left:5px;">* <#GuestNetwork_Radio_Status#>	<a style="font-family:Lucida Console;color:#FC0;text-decoration:underline;cursor:pointer;" onclick="_change_wl_unit_status(1);"><#btn_go#></a></span>	
					</td>
		  	</tr>

				<tr style="display:none">
					<td>
						<span><span><input type="hidden" name="wl_wpa_gtk_rekey" value="<% nvram_get("wl_wpa_gtk_rekey"); %>" disabled></span></span>
					</td>
		  	</tr>

				<!--tr>
					<th><#Guest_network_index#></th>
					<td>
						<p id="wl_vifname"></p>
					</td>
		  	</tr-->

				<tr style="display:none">
					<th><#Guest_Network_enable#></th>
					<td>
						<select id="wl_bss_enabled_field" name="wl_bss_enabled" class="input_option">
							<option class="content_input_fd" value="0" <% nvram_match("wl_bss_enabled", "0","selected"); %>><#checkbox_No#></option>
							<option class="content_input_fd" value="1" <% nvram_match("wl_bss_enabled", "1","selected"); %>><#checkbox_Yes#></option>
						</select>			
					</td>
		  	</tr>

				<tr>
					<th width="30%">Enable Passpoint</th>
					<td>
							<input type="radio" name="wl_hs2en_x" value="1"><#checkbox_Yes#>
							<input type="radio" name="wl_hs2en_x" value="0"><#checkbox_No#>
					</td>
				</tr>

				<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 1);"><#QIS_finish_wireless_item1#></a></th>
					<td>
						<input type="text" maxlength="32" class="input_32_table" name="wl_ssid" value="<% nvram_get("wl_ssid"); %>" onkeypress="return is_string(this, event)">
					</td>
		  	</tr>
			  
				<!-- Hidden and disable item, start -->
			  <tr style="display:none">
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 4);"><#WLANConfig11b_x_Mode11g_itemname#></a></th>
					<td>									
						<select name="wl_nmode_x" class="input_option" onChange="wireless_mode_change(this);" disabled>
							<option value="0" <% nvram_match("wl_nmode_x", "0","selected"); %>><#Auto#></option>
							<option value="1" <% nvram_match("wl_nmode_x", "1","selected"); %>>N Only</option>
							<option value="2" <% nvram_match("wl_nmode_x", "2","selected"); %>>Legacy</option>
						</select>
						<input type="checkbox" name="wl_gmode_check" id="wl_gmode_check" value="" onClick="wl_gmode_protection_check();"> b/g Protection</input>
						<!--span id="wl_nmode_x_hint" style="display:none"><#WLANConfig11n_automode_limition_hint#></span-->
					</td>
			  </tr>

				<tr id="wl_channel_field">
					<th><a id="wl_channel_select" class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 3);"><#WLANConfig11b_Channel_itemname#></a></th>
					<td>
				 		<select name="wl_channel" class="input_option" onChange="insertExtChannelOption();" disabled>
							<% select_channel("WLANConfig11b"); %>
				 		</select>
					</td>
			  </tr>
			  
			 	<tr id="wl_bw_field" style="display:none;">
			   	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 14);"><#WLANConfig11b_ChannelBW_itemname#></a></th>
			   	<td>				    			
						<select name="wl_bw" class="input_option" onChange="insertExtChannelOption();" disabled>
							<option class="content_input_fd" value="0" <% nvram_match("wl_bw", "0","selected"); %>>20 MHz</option>
							<option class="content_input_fd" value="1" <% nvram_match("wl_bw", "1","selected"); %>>20/40 MHz</option>
							<option class="content_input_fd" value="2" <% nvram_match("wl_bw", "2","selected"); %>>40 MHz</option>
						</select>				
			   	</td>
			 	</tr>
				<!-- Hidden and disable item, end -->

			  <tr id="wl_nctrlsb_field">
			  	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 15);"><#WLANConfig11b_EChannel_itemname#></a></th>
		   		<td>
					<select name="wl_nctrlsb" class="input_option" disabled>
						<option value="lower" <% nvram_match("wl_nctrlsb", "lower", "selected"); %>>lower</option>
						<option value="upper"<% nvram_match("wl_nctrlsb", "upper", "selected"); %>>upper</option>
					</select>
					</td>
		  	</tr>
			  
		  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 5);"><#WLANConfig11b_AuthenticationMethod_itemname#></a></th>
					<td>
				  		<select name="wl_auth_mode_x" class="input_option" onChange="authentication_method_change(this);">
								<option value="open"    <% nvram_match("wl_auth_mode_x", "open",   "selected"); %>>Open System</option>
								<option value="shared"  <% nvram_match("wl_auth_mode_x", "shared", "selected"); %>>Shared Key</option>
								<option value="psk"     <% nvram_match("wl_auth_mode_x", "psk",    "selected"); %>>WPA-Personal</option>
								<option value="psk2"    <% nvram_match("wl_auth_mode_x", "psk2",   "selected"); %>>WPA2-Personal</option>
								<option value="pskpsk2" <% nvram_match("wl_auth_mode_x", "pskpsk2","selected"); %>>WPA-Auto-Personal</option>
				  		</select>
				  		<br>
				  		<span id="wl_nmode_x_hint" style="display:none;"><#WLANConfig11n_automode_limition_hint#></span>
					</td>
		  	</tr>
			  	
		  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 6);"><#WLANConfig11b_WPAType_itemname#></a></th>
					<td>		
			  		<select name="wl_crypto" class="input_option" onChange="authentication_method_change(this);">
							<option value="aes" <% nvram_match("wl_crypto", "aes", "selected"); %>>AES</option>
							<option value="tkip+aes" <% nvram_match("wl_crypto", "tkip+aes", "selected"); %>>TKIP+AES</option>
			  		</select>
					</td>
		  	</tr>
			  
		  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 7);"><#WLANConfig11b_x_PSKKey_itemname#></a></th>
					<td>
			  		<input type="text" name="wl_wpa_psk" maxlength="64" class="input_32_table" value="<% nvram_get("wl_wpa_psk"); %>">
					</td>
		  	</tr>
			  		  
		  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 9);"><#WLANConfig11b_WEPType_itemname#></a></th>
					<td>
			  		<select name="wl_wep_x" class="input_option" onChange="wep_encryption_change(this);">
							<option value="0" <% nvram_match("wl_wep_x", "0", "selected"); %>><#wl_securitylevel_0#></option>
							<option value="1" <% nvram_match("wl_wep_x", "1", "selected"); %>>WEP-64bits</option>
							<option value="2" <% nvram_match("wl_wep_x", "2", "selected"); %>>WEP-128bits</option>
			  		</select>
			  		<span name="key_des"></span>
					</td>
		  	</tr>
			  
		  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 10);"><#WLANConfig11b_WEPDefaultKey_itemname#></a></th>
					<td>		
				 		<select name="wl_key" class="input_option"  onChange="wep_key_index_change(this);">
							<option value="1" <% nvram_match("wl_key", "1","selected"); %>>1</option>
							<option value="2" <% nvram_match("wl_key", "2","selected"); %>>2</option>
							<option value="3" <% nvram_match("wl_key", "3","selected"); %>>3</option>
							<option value="4" <% nvram_match("wl_key", "4","selected"); %>>4</option>
			  		</select>
					</td>
			 	</tr>
			  
			 	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 18);"><#WLANConfig11b_WEPKey1_itemname#></th>
					<td><input type="text" name="wl_key1" id="wl_key1" maxlength="32" class="input_32_table" value="<% nvram_get("wl_key1"); %>" onKeyUp="return change_wlkey(this, 'WLANConfig11b');"></td>
			 	</tr>
			  
			 	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 18);"><#WLANConfig11b_WEPKey2_itemname#></th>
					<td><input type="text" name="wl_key2" id="wl_key2" maxlength="32" class="input_32_table" value="<% nvram_get("wl_key2"); %>" onKeyUp="return change_wlkey(this, 'WLANConfig11b');"></td>
			 	</tr>
			  
			 	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 18);"><#WLANConfig11b_WEPKey3_itemname#></th>
					<td><input type="text" name="wl_key3" id="wl_key3" maxlength="32" class="input_32_table" value="<% nvram_get("wl_key3"); %>" onKeyUp="return change_wlkey(this, 'WLANConfig11b');"></td>
			 	</tr>
			  
			 	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 18);"><#WLANConfig11b_WEPKey4_itemname#></th>
					<td><input type="text" name="wl_key4" id="wl_key4" maxlength="32" class="input_32_table" value="<% nvram_get("wl_key4"); %>" onKeyUp="return change_wlkey(this, 'WLANConfig11b');"></td>
		  	</tr>

			 	<tr style="display:none">
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 8);"><#WLANConfig11b_x_Phrase_itemname#></a></th>
					<td>
				  		<input type="text" name="wl_phrase_x" maxlength="64" class="input_32_table" value="<% nvram_get("wl_phrase_x"); %>" onKeyUp="return is_wlphrase('WLANConfig11b', 'wl_phrase_x', this);">
					</td>
			 	</tr>

			 	<!-- tr>
					<th><#Access_Time#></th>
					<td>
         		<input type="radio" value="1" name="wl_expire_radio" class="content_input_fd" onClick="">
						<input type="text" maxlength="2" name="wl_expire_hr" class="input_3_table"  value="" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 0, 23)"> Hr
						<input type="text" maxlength="2" name="wl_expire_min" class="input_3_table"  value="" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 0, 59)"> Min
         		<input type="radio" value="0" name="wl_expire_radio" class="content_input_fd" onClick="">Limitless
					</td>
			 	</tr-->

			 	<tr>
					<th><#Access_Intranet#></th>
					<td>
				 		<select name="wl_lanaccess" class="input_option">
							<option value="on" <% nvram_match("wl_lanaccess", "on","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							<option value="off" <% nvram_match("wl_lanaccess", "off","selected"); %>><#btn_disable#></option>
			  		</select>
					</td>
			 	</tr>

				<tr id="mac_filter_guest">
					<th id="enable_macfilter"></th>
					<td>
						<select name="wl_macmode_option" class="input_option">
							<option class="content_input_fd" value="" <% nvram_match("wl_macmode", "","selected"); %>><#checkbox_Yes#></option>
							<option class="content_input_fd" value="disabled" <% nvram_match("wl_macmode", "disabled","selected"); %>><#checkbox_No#></option>
						</select>
						&nbsp;
						<span id="ACL_enabled_hint" style="cursor:pointer;display:none;text-decoration:underline;" onclick="goToACLFilter();"><#FirewallConfig_MFList_groupitemname#></span>
						<span id="ACL_disabled_hint" style="cursor:pointer;display:none;text-decoration:underline;" onclick="goToACLFilter();"></span>				
					</td>
				</tr>
			</table>
		
			<div id="submitBtn" class="apply_gen">
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
<!--===================================Ending of Main Content===========================================-->		

	</td>
		
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
