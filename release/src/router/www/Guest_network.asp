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
<title><#Web_Title#> - <#Guest_Network#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link href="other.css"  rel="stylesheet" type="text/css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/md5.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

var radio_2 = '<% nvram_get("wl0_radio"); %>';
var radio_5 = '<% nvram_get("wl1_radio"); %>';
<% radio_status(); %>

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var modify_mode = '<% get_parameter("flag"); %>';

<% login_state_hook(); %>
<% wl_get_parameter(); %>

wl_channel_list_2g = '<% channel_list_2g(); %>';
wl_channel_list_5g = '<% channel_list_5g(); %>';

function initial(){
	show_menu();	
	insertExtChannelOption();		
	wl_auth_mode_change(1);

	if(downsize_support != -1)
		$("guest_image").parentNode.style.display = "none";

	mbss_display_ctrl();
	gen_gntable();
	if(modify_mode == 1)
		guest_divctrl(1);		
	else
		guest_divctrl(0);

	document.form.wl_ssid.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_ssid"); %>');
	document.form.wl_wpa_psk.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_wpa_psk"); %>');
	document.form.wl_key1.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_key1"); %>');
	document.form.wl_key2.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_key2"); %>');
	document.form.wl_key3.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_key3"); %>');
	document.form.wl_key4.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_key4"); %>');
	document.form.wl_phrase_x.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_phrase_x"); %>');
	document.form.wl_channel.value = document.form.wl_channel_orig.value;


	if(document.form.wl_gmode_protection.value == "auto")
		document.form.wl_gmode_check.checked = true;
	else
		document.form.wl_gmode_check.checked = false;

	if(band5g_support == -1 || no5gmssid_support != -1){
		$("guest_table5").style.display = "none";
	}

	$("wl_vifname").innerHTML = document.form.wl_subunit.value;
	change_wl_expire_radio();
	
	if(radio_2 != 1){
		$('2g_radio_hint').style.display ="";
	}
	if(radio_5 != 1){
		$('5g_radio_hint').style.display ="";
	}

	if(document.form.preferred_lang.value == "JP"){    //use unique font-family for JP
		$('2g_radio_hint').style.fontFamily = "MS UI Gothic,MS P Gothic";
		$('5g_radio_hint').style.fontFamily = "MS UI Gothic,MS P Gothic";
	}	
}

function change_wl_expire_radio(){
	if(document.form.wl_expire.value > 0){
		document.form.wl_expire_hr.value = Math.floor(document.form.wl_expire.value/3600);
		document.form.wl_expire_min.value  = Math.floor((document.form.wl_expire.value%3600)/60);
		document.form.wl_expire_radio[0].checked = 1;
		document.form.wl_expire_radio[1].checked = 0;
	}
	else{
		document.form.wl_expire_radio[0].checked = 0;
		document.form.wl_expire_radio[1].checked = 1;
	}
}

function translate_auth(flag){
	if(flag == "open")
		return "Open System";
	else if(flag == "shared")
		return "Shared Key";
	else if(flag == "psk")
		return "WPA-Personal";
	else if(flag == "psk2")
 		return "WPA2-Personal";
	else if(flag == "pskpsk2")
		return "WPA-Auto-Personal";
	else
		return "unknown Auth";
}

function gen_gntable_tr(unit, gn_array){	
	var GN_band = "";
	var htmlcode = "";
	if(unit == 0) GN_band = 2;
	if(unit == 1) GN_band = 5;
	
	htmlcode += '<table align="left" style="margin:auto;margin-left:20px;border-collapse:collapse;">';
	htmlcode += '<tr><th align="left" style="width:200px;">';
	htmlcode += '<table id="GNW_'+GN_band+'G" class="gninfo_th_table" align="left" style="margin:auto;border-collapse:collapse;">';
	htmlcode += '<tr><th align="left" style="height:28px;"><#QIS_finish_wireless_item1#></th></tr>';
	htmlcode += '<tr><th align="left" style="height:28px;"><#WLANConfig11b_AuthenticationMethod_itemname#></th></tr>';
	htmlcode += '<tr><th align="left" style="height:28px;"><#Network_key#></th></tr>';
	htmlcode += '<tr><th align="left" style="height:28px;"><#mssid_time_remaining#></th></tr>';
	if(sw_mode != "3"){
			htmlcode += '<tr><th align="left" style="width:20%;height:28px;"><#Access_Intranet#></th></tr>';
	}
	htmlcode += '<tr><th align="left" style="height:28px;"></th></tr>';		
	htmlcode += '</table></th>';
	
	for(var i=0; i<gn_array.length; i++){			
			var subunit = i+1;
			htmlcode += '<td style="padding-right:50px"><table id="GNW_'+GN_band+'G'+i+'" class="gninfo_table" align="center" style="margin:auto;border-collapse:collapse;">';			
			if(gn_array[i][0] == "1"){
					htmlcode += '<tr><td align="center" class="gninfo_table_top"></td></tr>';
					if(gn_array[i][1].length < 21)
							htmlcode += '<tr><td align="center" onclick="change_guest_unit('+ unit +','+ subunit +');">'+ gn_array[i][1] +'</td></tr>';
					else
							htmlcode += '<tr><td align="center" onclick="change_guest_unit('+ unit +','+ subunit +');"><span onmouseover="return overlib(\''+ gn_array[i][1] +'\', HAUTO);" onmouseout="nd();">'+ gn_array[i][1].substring(0,17) +'...</span></td></tr>';
					
					htmlcode += '<tr><td align="center" onclick="change_guest_unit('+ unit +','+ subunit +');">'+ translate_auth(gn_array[i][2]) +'</td></tr>';
					
					if(gn_array[i][2].indexOf("psk") >= 0)
							htmlcode += '<tr><td align="center" onclick="change_guest_unit('+ unit +','+ subunit +');">'+ gn_array[i][4] +'</td></tr>';
					else if(gn_array[i][2] == "open" && gn_array[i][5] == "0")
							htmlcode += '<tr><td align="center" onclick="change_guest_unit('+ unit +','+ subunit +');">None</td></tr>';
					else{
							var key_index = parseInt(gn_array[i][6])+6;
							htmlcode += '<tr><td align="center" onclick="change_guest_unit('+ unit +','+ subunit +');">'+ gn_array[i][key_index] +'</td></tr>';
					}
					
					if(gn_array[i][11] == 0)
							htmlcode += '<tr><td align="center" onclick="change_guest_unit('+ unit +','+ subunit +');"><#Limitless#></td></tr>';
					else{
							var expire_hr = Math.floor(gn_array[i][13]/3600);
							var expire_min = Math.floor((gn_array[i][13]%3600)/60);
							if(expire_hr > 0)
									htmlcode += '<tr><td align="center" onclick="change_guest_unit('+ unit +','+ subunit +');"><b id="expire_hr_'+i+'">'+ expire_hr + '</b> Hr <b id="expire_min_'+i+'">' + expire_min +'</b> Min</td></tr>';
							else{
									if(expire_min > 0)
											htmlcode += '<tr><td align="center" onclick="change_guest_unit('+ unit +','+ subunit +');"><b id="expire_min_'+i+'">' + expire_min +'</b> Min</td></tr>';
									else
											htmlcode += '<tr><td align="center" onclick="change_guest_unit('+ unit +','+ subunit +');"><b id="expire_min_'+i+'">< 1</b> Min</td></tr>';
							}				
					}					
					
			}else{					
					htmlcode += '<tfoot><tr rowspan="3"><td align="center"><input type="button" class="button_gen" value="<#WLANConfig11b_WirelessCtrl_button1name#>" onclick="create_guest_unit('+ unit +','+ subunit +');"></td></tr></tfoot>';
			}														
			
			if(sw_mode != "3"){
					if(gn_array[i][0] == "1")
							htmlcode += '<tr><td align="center" onclick="change_guest_unit('+ unit +','+ subunit +');">'+ gn_array[i][12] +'</td></tr>';
			}
										
			if(gn_array[i][0] == "1"){
					htmlcode += '<tr><td align="center" class="gninfo_table_bottom"></td></tr>';
					htmlcode += '<tfoot><tr><td align="center"><input type="button" class="button_gen" value="<#btn_disable#>" onclick="close_guest_unit('+ unit +','+ subunit +');"></td></tr></tfoot>';
			}
			htmlcode += '</table></td>';		
	}	
		
	htmlcode += '</tr></table>';


	return htmlcode;
}

function _change_wl_unit_status(__unit){
	document.titleForm.current_page.value = "Advanced_WAdvanced_Content.asp?af=wl_radio";
	document.titleForm.next_page.value = "Advanced_WAdvanced_Content.asp?af=wl_radio";
	change_wl_unit_status(__unit);
}

function gen_gntable(){
	var htmlcode = ""; 
	var htmlcode5 = ""; 
		
	htmlcode += '<table style="margin-left:20px;margin-top:25px;" width="95%" align="center" cellpadding="4" cellspacing="0" class="gninfo_head_table" id="gninfo_table_2g">';
	htmlcode += '<tr id="2g_title"><td align="left" style="color:#5AD;font-size:16px; border-bottom:1px dashed #AAA;"><span>2.4GHz</span>';
	htmlcode += '<span id="2g_radio_hint" style="font-size: 14px;display:none;color:#FC0;margin-left:17px;">* <#GuestNetwork_Radio_Status#>	<a style="font-family:Lucida Console;color:#FC0;text-decoration:underline;cursor:pointer;" onclick="_change_wl_unit_status(0);"><#btn_go#></a></span></td></tr>';
	htmlcode += gen_gntable_tr(0, gn_array_2g);
	htmlcode += '</table>';
	$("guest_table2").innerHTML = htmlcode;
	
	htmlcode5 += '<table style="margin-left:20px;margin-top:25px;" width="95%" align="center" cellpadding="4" cellspacing="0" class="gninfo_head_table" id="gninfo_table_5g">';
	htmlcode5 += '<tr id="5g_title"><td align="left" style="color:#5AD; font-size:16px; border-bottom:1px dashed #AAA;"><span>5GHz</span>';
	htmlcode5 += '<span id="5g_radio_hint" style="font-size: 14px;display:none;color:#FC0;margin-left:17px;">* <#GuestNetwork_Radio_Status#>	<a style="font-family:Lucida Console;color:#FC0;text-decoration:underline;cursor:pointer;" onclick="_change_wl_unit_status(1);"><#btn_go#></a></span></td></tr>';
	htmlcode5 += gen_gntable_tr(1, gn_array_5g);
	htmlcode5 += '</table>';		
	$("guest_table5").innerHTML = htmlcode5;
}

function add_options_value(o, arr, orig){
	if(orig == arr)
		add_option(o, "mbss_"+arr, arr, 1);
	else
		add_option(o, "mbss_"+arr, arr, 0);
}

function applyRule(){
	if(document.form.wl_wpa_psk.value == "<#wireless_psk_fillin#>")
		document.form.wl_wpa_psk.value = "";

	if(validForm()){
		showLoading();
		inputCtrl(document.form.wl_crypto, 1);
		inputCtrl(document.form.wl_wpa_psk, 1);
		inputCtrl(document.form.wl_wep_x, 1);
		inputCtrl(document.form.wl_key, 1);
		inputCtrl(document.form.wl_key1, 1);
		inputCtrl(document.form.wl_key2, 1);
		inputCtrl(document.form.wl_key3, 1);
		inputCtrl(document.form.wl_key4, 1);
		inputCtrl(document.form.wl_phrase_x, 1);

		if(document.form.wl_expire_radio[0].checked)
			document.form.wl_expire.value = document.form.wl_expire_hr.value*3600 + document.form.wl_expire_min.value*60;
		else
			document.form.wl_expire.value = 0;

		if(wl6_support != -1)
			document.form.action_wait.value = parseInt(document.form.action_wait.value)+10;			// extend waiting time for BRCM new driver

		if(Rawifi_support != -1)
			document.form.action_wait.value = parseInt(document.form.action_wait.value)+5;			// extend waiting time for RaLink
		
		document.form.submit();
	}
}

function validForm(){
	var auth_mode = document.form.wl_auth_mode_x.value;
	
	if(!validate_string_ssid(document.form.wl_ssid))
		return false;
	
	if(document.form.wl_wep_x.value != "0")
		if(!validate_wlphrase('WLANConfig11b', 'wl_phrase_x', document.form.wl_phrase_x))
			return false;	
	if(auth_mode == "psk" || auth_mode == "psk2" || auth_mode == "pskpsk2"){ //2008.08.04 lock modified
		if(!validate_psk(document.form.wl_wpa_psk))
			return false;
	}
	else{
		var cur_wep_key = eval('document.form.wl_key'+document.form.wl_key.value);		
		if(auth_mode != "radius" && !validate_wlkey(cur_wep_key))
			return false;
	}	
	return true;
}

function done_validating(action){
	refreshpage();
}

function change_key_des(){
	var objs = getElementsByName_iefix("span", "key_des");
	var wep_type = document.form.wl_wep_x.value;
	var str = "";
	
	if(wep_type == "1")
		str = "(<#WLANConfig11b_WEPKey_itemtype1#>)";
	else if(wep_type == "2")
		str = "(<#WLANConfig11b_WEPKey_itemtype2#>)";
	
	for(var i = 0; i < objs.length; ++i)
		showtext(objs[i], str);
}

function validate_wlphrase(s, v, obj){
	if(!validate_string(obj)){
		is_wlphrase(s, v, obj);
		return(false);
	}
	
	return true;
}

function disableAdvFn(){
	for(var i=16; i>=1; i--)
		$("WLgeneral").deleteRow(i);
}

function guest_divctrl(flag){	
	if(flag == 1){
		$("guest_table2").style.display = "none";
		if(band5g_support != -1)
			$("guest_table5").style.display = "none";
		$("gnset_table").style.display = "";
		if(sw_mode == "3")
				inputCtrl(document.form.wl_lanaccess, 0);
		$("applyButton").style.display = "";
	}
	else{
		$("guest_table2").style.display = "";
		if(band5g_support == -1 || no5gmssid_support != -1)
				$("guest_table5").style.display = "none";
		else
				$("guest_table5").style.display = "";
		$("gnset_table").style.display = "none";
		$("applyButton").style.display = "none";
	}
}

function mbss_display_ctrl(){
	// generate options
	if(wl_vifnames != ""){
		$("wl_channel_field").style.display = "none";
		$("wl_nctrlsb_field").style.display = "none";
			for(var i=1; i<multissid_support+1; i++)
				add_options_value(document.form.wl_subunit, i, '<% nvram_get("wl_subunit"); %>');
	}
	else{
		$("gnset_table").style.display = "none";
		$("guest_table2").style.display = "none";
		if(band5g_support != -1)
			$("guest_table5").style.display = "none";
		$("applyButton").style.display = "none";
		$("applyButton").innerHTML = "Not support!";
		$("applyButton").style.fontSize = "25px";
		$("applyButton").style.marginTop = "125px";
	}
}

function close_guest_unit(_unit, _subunit){
	var NewInput = document.createElement("input");
	NewInput.type = "hidden";
	NewInput.name = "wl"+ _unit + "." + _subunit +"_bss_enabled";
	NewInput.value = "0";
	document.unitform.appendChild(NewInput);
	if(Rawifi_support != -1)
			document.unitform.action_wait.value = parseInt(document.unitform.action_wait.value)+5;			// extend waiting time for RaLink
	document.unitform.submit();
}

function change_guest_unit(_unit, _subunit){
	document.form.wl_unit.value = _unit;
	document.form.wl_subunit.value = _subunit;
	document.form.current_page.value = "Guest_network.asp?flag=1";
	document.form.next_page.value = "Guest_network.asp?flag=1";
	FormActions("apply.cgi", "change_wl_unit", "", "");
	document.form.target = "";
	if(Rawifi_support != -1)
			document.form.action_wait.value = parseInt(document.form.action_wait.value)+5;			// extend waiting time for RaLink	
	document.form.submit();
}

function create_guest_unit(_unit, _subunit){
	var NewInput = document.createElement("input");
	NewInput.type = "hidden";
	NewInput.name = "wl"+ _unit + "." + _subunit +"_bss_enabled";
	NewInput.value = "1";
	document.unitform.appendChild(NewInput);
	document.unitform.wl_unit.value = _unit;
	document.unitform.wl_subunit.value = _subunit;
	if(Rawifi_support != -1)
			document.unitform.action_wait.value = parseInt(document.unitform.action_wait.value)+5;			// extend waiting time for RaLink	
	document.unitform.submit();
}

function genBWTable(_unit){
	cur = '<% nvram_get("wl_bw"); %>';

	if(document.form.wl_nmode_x.value == 2){
		var bws = new Array("1");
		var bwsDesc = new Array("20 MHz");
	}
	else if(_unit == 0){
		var bws = new Array(0, 1, 2);
		var bwsDesc = new Array("20/40 MHz", "20 MHz", "40 MHz");
	}
	else{
		var bws = new Array(0, 1, 2, 3);
		var bwsDesc = new Array("20/40/80 MHz", "20 MHz", "40 MHz", "80 MHz");
	}

	document.form.wl_bw.length = bws.length;
	for (var i in bws) {
		document.form.wl_bw[i] = new Option(bwsDesc[i], bws[i]);
		document.form.wl_bw[i].value = bws[i];

		if (bws[i] == cur) {
			document.form.wl_bw[i].selected = true;
		}
	}
}
</script>
</head>

<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="hiddenMask" class="popup_bg">
	<table cellpadding="4" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
		<tr>
		<td>
			<div class="drword" id="drword"><#Main_alert_proceeding_desc4#> <#Main_alert_proceeding_desc1#>...
				<br/>
				<br/>
		    </div>
		  <div class="drImg"><img src="images/alertImg.png"></div>
			<div style="height:70px; "></div>
		</td>
		</tr>
	</table>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="unitform" action="/start_apply2.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Guest_network.asp">
<input type="hidden" name="next_page" value="Guest_network.asp">
<input type="hidden" name="wl_unit" value="<% nvram_get("wl_unit"); %>">
<input type="hidden" name="wl_subunit" value="<% nvram_get("wl_subunit"); %>">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_wait" value="8">
</form>
<form method="post" name="form" action="/start_apply2.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="wan_route_x" value="<% nvram_get("wan_route_x"); %>">
<input type="hidden" name="wan_nat_x" value="<% nvram_get("wan_nat_x"); %>">
<input type="hidden" name="current_page" value="Guest_network.asp">
<input type="hidden" name="next_page" value="Guest_network.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_wait" value="8">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="wl_country_code" value="<% nvram_get("wl0_country_code"); %>" disabled>
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_ssid_org" value="<% nvram_char_to_ascii("WLANConfig11b",  "wl_ssid"); %>">
<input type="hidden" name="wl_wpa_psk_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_wpa_psk"); %>">
<input type="hidden" name="wl_key1_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key1"); %>">
<input type="hidden" name="wl_key2_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key2"); %>">
<input type="hidden" name="wl_key3_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key3"); %>">
<input type="hidden" name="wl_key4_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key4"); %>">
<input type="hidden" name="wl_phrase_x_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_phrase_x"); %>">
<input type="hidden" name="x_RegulatoryDomain" value="<% nvram_get("x_RegulatoryDomain"); %>" readonly="1">
<input type="hidden" name="wl_wme" value="<% nvram_get("wl_wme"); %>">
<input type="hidden" name="wl_nctrlsb_old" value="<% nvram_get("wl_nctrlsb"); %>">
<input type="hidden" name="wl_key_type" value='<% nvram_get("wl_key_type"); %>'> <!--Lock Add 2009.03.10 for ralink platform-->
<input type="hidden" name="wl_channel_orig" value='<% nvram_get("wl_channel"); %>'>
<input type="hidden" name="wl_expire" value='<% nvram_get("wl_expire"); %>'>

<input type="hidden" name="wl_gmode_protection" value="<% nvram_get("wl_gmode_protection"); %>" disabled>
<input type="hidden" name="wl_wpa_mode" value="<% nvram_get("wl_wpa_mode"); %>" disabled>
<input type="hidden" name="wl_mode_x" value="<% nvram_get("wl_mode_x"); %>" disabled>
<select name="wl_subunit" class="input_option" onChange="change_wl_unit();" style="display:none"></select>

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td width="17">&nbsp;</td>
	
	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>
	
	<td height="380" valign="top">
	  <div id="tabMenu" class="submenuBlock" style="*margin-top:-155px;"></div>

<!--===================================Beginning of Main Content===========================================-->
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
  <tr>
	<td align="left" valign="top" >
	  <table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" style="-webkit-border-radius:3px;-moz-border-radius:3px;border-radius:3px;" id="FormTitle">
		<tbody>
		<tr>
		  <td bgcolor="#4D595D" valign="top" id="table_height"  >
		  <div>&nbsp;</div>

		  <div class="formfonttitle"><#Guest_Network#></div>
	    <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

			<div>
				<table width="650px" style="margin:25px;">
					<tr>
						<td width="120px">
							<img id="guest_image" src="/images/New_ui/network_config.png">
						</td>
						<td>
							<div class="formfontdesc" style="font-style: italic;font-size: 14px;"><#GuestNetwork_desc#></div>
							
						</td>
					</tr>
				</table>
			</div>
			
			<!-- info table -->
			<div id="guest_table2"></div>
			<div id="guest_table5" style="margin-top:250px;"></div>

			<!-- setting table -->
			<table width="80%" border="1" align="center" style="margin-top:10px;margin-bottom:20px;" cellpadding="4" cellspacing="0" id="gnset_table" class="FormTable">
				<tr id="wl_unit_field" style="display:none">
					<th><#Interface#></th>
					<td>
						<select name="wl_unit" class="input_option" onChange="change_wl_unit();" style="display:none">
							<option class="content_input_fd" value="0" <% nvram_match("wl_unit", "0","selected"); %>>2.4GHz</option>
							<option class="content_input_fd" value="1" <% nvram_match("wl_unit", "1","selected"); %>>5GHz</option>
						</select>			
						<p id="wl_ifname">2.4GHz</p>
					</td>
		  	</tr>

				<tr style="display:none">
					<td>
						<span><span><input type="hidden" name="wl_wpa_gtk_rekey" value="<% nvram_get("wl_wpa_gtk_rekey"); %>" disabled></span></span>
					</td>
		  	</tr>

				<tr>
					<th><#Guest_network_index#></th>
					<td>
						<p id="wl_vifname"></p>
					</td>
		  	</tr>

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
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 1);"><#QIS_finish_wireless_item1#></a></th>
					<td>
						<input type="text" maxlength="32" class="input_32_table" name="wl_ssid" value="<% nvram_get("wl_ssid"); %>" onkeypress="return is_string(this, event)">
					</td>
		  	</tr>
			  
				<!-- Hidden and disable item, start -->
			  <tr style="display:none">
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 4);"><#WLANConfig11b_x_Mode11g_itemname#></a></th>
					<td>									
						<select name="wl_nmode_x" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_nmode_x');" disabled>
							<option value="0" <% nvram_match("wl_nmode_x", "0","selected"); %>><#Auto#></option>
							<option value="1" <% nvram_match("wl_nmode_x", "1","selected"); %>>N Only</option>
							<option value="2" <% nvram_match("wl_nmode_x", "2","selected"); %>>Legacy</option>
						</select>
						<input type="checkbox" name="wl_gmode_check" id="wl_gmode_check" value="" onClick="return change_common(this, 'WLANConfig11b', 'wl_gmode_check', '1')"> b/g Protection</input>
						<span id="wl_nmode_x_hint" style="display:none"><#WLANConfig11n_automode_limition_hint#></span>
					</td>
			  </tr>

				<tr id="wl_channel_field">
					<th><a id="wl_channel_select" class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 3);"><#WLANConfig11b_Channel_itemname#></a></th>
					<td>
				 		<select name="wl_channel" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_channel')" disabled>
							<% select_channel("WLANConfig11b"); %>
				 		</select>
					</td>
			  </tr>
			  
			 	<tr id="wl_bw_field" style="display:none;">
			   	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 14);"><#WLANConfig11b_ChannelBW_itemname#></a></th>
			   	<td>				    			
						<select name="wl_bw" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_bw')" disabled>
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
					<select name="wl_nctrlsb" class="input_option">
						<option value="lower" <% nvram_match("wl_nctrlsb", "lower", "selected"); %>>lower</option>
						<option value="upper"<% nvram_match("wl_nctrlsb", "upper", "selected"); %>>upper</option>
					</select>
					</td>
		  	</tr>
			  
		  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 5);"><#WLANConfig11b_AuthenticationMethod_itemname#></a></th>
					<td>
				  		<select name="wl_auth_mode_x" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_auth_mode_x');">
								<option value="open"    <% nvram_match("wl_auth_mode_x", "open",   "selected"); %>>Open System</option>
								<option value="shared"  <% nvram_match("wl_auth_mode_x", "shared", "selected"); %>>Shared Key</option>
								<option value="psk"     <% nvram_match("wl_auth_mode_x", "psk",    "selected"); %>>WPA-Personal</option>
								<option value="psk2"    <% nvram_match("wl_auth_mode_x", "psk2",   "selected"); %>>WPA2-Personal</option>
								<option value="pskpsk2" <% nvram_match("wl_auth_mode_x", "pskpsk2","selected"); %>>WPA-Auto-Personal</option>
				  		</select>
					</td>
		  	</tr>
			  	
		  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 6);"><#WLANConfig11b_WPAType_itemname#></a></th>
					<td>		
			  		<select name="wl_crypto" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_crypto')">
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
			  		<select name="wl_wep_x" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_wep_x');">
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
				 		<select name="wl_key" class="input_option"  onChange="return change_common(this, 'WLANConfig11b', 'wl_key');">
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

			 	<tr>
					<th><#Access_Time#></th>
					<td>
         		<input type="radio" value="1" name="wl_expire_radio" class="content_input_fd" onClick="">
						<input type="text" maxlength="2" name="wl_expire_hr" class="input_3_table"  value="" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 0, 23)"> Hr
						<input type="text" maxlength="2" name="wl_expire_min" class="input_3_table"  value="" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 0, 59)"> Min
         		<input type="radio" value="0" name="wl_expire_radio" class="content_input_fd" onClick="">Limitless
					</td>
			 	</tr>

			 	<tr>
					<th><#Access_Intranet#></th>
					<td>
				 		<select name="wl_lanaccess" class="input_option">
							<option value="on" <% nvram_match("wl_lanaccess", "on","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							<option value="off" <% nvram_match("wl_lanaccess", "off","selected"); %>><#btn_disable#></option>
			  		</select>
					</td>
			 	</tr>
			</table>

			<div class="apply_gen" id="applyButton">
				<input type="button" class="button_gen" value="<#CTL_Cancel#>" onclick="guest_divctrl(0);">
				<input type="button" class="button_gen" value="<#CTL_apply#>" onclick="applyRule();">
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
	
	<td width="10" align="center" valign="top"></td>
  </tr>
</table>

<div id="footer"></div>
</body>
</html>
