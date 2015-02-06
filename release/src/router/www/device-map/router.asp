<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title></title>
<link href="/NM_style.css" rel="stylesheet" type="text/css" />
<link href="/form_style.css" rel="stylesheet" type="text/css" />
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();
<% wl_get_parameter(); %>
var flag = '<% get_parameter("flag"); %>';
var smart_connect_flag_t;

if(yadns_support){
	var yadns_enable = '<% nvram_get("yadns_enable_x"); %>';
	var yadns_mode = '<% nvram_get("yadns_mode"); %>';
	var yadns_clients = [ <% yadns_clients(); %> ];
}

function initial(){
	if(sw_mode == 2){
		if('<% nvram_get("wl_unit"); %>' == '<% nvram_get("wlc_band"); %>' && '<% nvram_get("wl_subunit"); %>' != '1'){
			tabclickhandler('<% nvram_get("wl_unit"); %>');
		}
		else if('<% nvram_get("wl_unit"); %>' != '<% nvram_get("wlc_band"); %>' && '<% nvram_get("wl_subunit"); %>' == '1'){
			tabclickhandler('<% nvram_get("wl_unit"); %>');
		}
	}
	else if(sw_mode == 4){
		if('<% nvram_get("wl_unit"); %>' == '<% nvram_get("wlc_band"); %>'){
			$("WLnetworkmap").style.display = "none";
			$("applySecurity").style.display = "none";
			$("WLnetworkmap_re").style.display = "";
		}
		else{
			if('<% nvram_get("wl_subunit"); %>' != '-1'){
				tabclickhandler('<% nvram_get("wl_unit"); %>');
			}
			else{
				tabclickhandler('<% nvram_get("wlc_band"); %>');
			}
		}
	}
	else{
		if("<% nvram_get("wl_subunit"); %>" != "-1"){
			tabclickhandler(0);
		}
	}

	if("<% nvram_get("wl_unit"); %>" == "-1"){
		tabclickhandler(0);
	}

	// modify wlX.1_ssid(SSID to end clients) under repeater mode
	if((parent.sw_mode == 2 || parent.sw_mode == 4) && '<% nvram_get("wlc_band"); %>' == '<% nvram_get("wl_unit"); %>')
		document.form.wl_subunit.value = 1;
	else
		document.form.wl_subunit.value = -1;
	
	if(smart_connect_support){
		document.getElementById("smartcon_enable_field").style.display = '';
		document.getElementById("smartcon_enable_line").style.display = '';
	}

	if(band5g_support){
		$("t0").style.display = "";
		$("t1").style.display = "";
		if(wl_info.band5g_2_support){
			$("t2").style.display = "";
			tab_reset(0);
		}

		// disallow to use the other band as a wireless AP
		if(parent.sw_mode == 4 && !localAP_support){
			for(var x=0; x<wl_info.wl_if_total;x++){
				if(x != '<% nvram_get("wlc_band"); %>')
					$('t'+parseInt(x)).style.display = 'none';			
			}
		}
	}
	else{
		$("t0").style.display = "";	
	}

	change_tabclick();
	if($("t1").className == "tabclick_NW" && 	parent.Rawifi_support)	//no exist Rawifi
		$("wl_txbf_tr").style.display = "";		//Viz Add 2011.12 for RT-N56U Ralink 			

	document.form.wl_ssid.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_ssid"); %>');
	document.form.wl_wpa_psk.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_wpa_psk"); %>');
	document.form.wl_key1.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_key1"); %>');
	document.form.wl_key2.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_key2"); %>');
	document.form.wl_key3.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_key3"); %>');
	document.form.wl_key4.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_key4"); %>');

	/* Viz banned 2012.06
	if(document.form.wl_wpa_psk.value.length <= 0)
		document.form.wl_wpa_psk.value = "<#wireless_psk_fillin#>";
	*/
	
	limit_auth_method();
	wl_auth_mode_change(1);
	show_LAN_info();

	if(smart_connect_support && parent.sw_mode != 4){

		if(flag == '')
			smart_connect_flag_t = '<% nvram_get("smart_connect_x"); %>';
		else
			smart_connect_flag_t = flag;	

			document.form.smart_connect_x.value = smart_connect_flag_t;		
			change_smart_connect(smart_connect_flag_t);	
	}

	if(parent.sw_mode == 4)
		parent.show_middle_status('<% nvram_get("wlc_auth_mode"); %>', 0);		
	else
		parent.show_middle_status(document.form.wl_auth_mode_x.value, parseInt(document.form.wl_wep_x.value));

	flash_button();	
}

function change_tabclick(){
	switch('<% nvram_get("wl_unit"); %>'){
		case '0': $("t0").className = "tabclick_NW";
				break;
		case '1': $("t1").className = "tabclick_NW";
				break;
		case '2': $("t2").className = "tabclick_NW";
				break;
	}
}

function tabclickhandler(wl_unit){
	if(wl_unit == 3){
		location.href = "router_status.asp";
	}
	else{
		if((parent.sw_mode == 2 || parent.sw_mode == 4) && '<% nvram_get("wlc_band"); %>' == wl_unit)
			document.form.wl_subunit.value = 1;
		else
			document.form.wl_subunit.value = -1;

		document.form.wl_unit.value = wl_unit;

		if(smart_connect_support){
			var smart_connect_flag = document.form.smart_connect_x.value;
			document.form.current_page.value = "device-map/router.asp?flag=" + smart_connect_flag;
		}else
			document.form.current_page.value = "device-map/router.asp";
		FormActions("/apply.cgi", "change_wl_unit", "", "");
		document.form.target = "";
		document.form.submit();
	}
}

function disableAdvFn(){
	for(var i=8; i>=1; i--)
		$("WLnetworkmap").deleteRow(i);
}

function UIunderRepeater(){
	document.form.wl_auth_mode_x.disabled = true;
	document.form.wl_wep_x.disabled = true;
	document.form.wl_key.disabled = true;
	document.form.wl_asuskey1.disabled = true;
	document.form.wl_wpa_psk.disabled = true;
	document.form.wl_crypto.disabled = true;

	var ssidObj=document.getElementById("wl_ssid");
	ssidObj.name="wlc_ure_ssid";
}

function wl_auth_mode_change(isload){
	var mode = document.form.wl_auth_mode_x.value;

	change_wep_type(mode);
	change_wpa_type(mode);
}

function change_wpa_type(mode){
	var opts = document.form.wl_auth_mode_x.options;
	var new_array;
	var cur_crypto;
	/* enable/disable crypto algorithm */
	if(mode == "wpa" || mode == "wpa2" || mode == "wpawpa2" || mode == "psk" || mode == "psk2" || mode == "pskpsk2")
		inputCtrl(document.form.wl_crypto, 1);
	else
		inputCtrl(document.form.wl_crypto, 0);
	
	/* enable/disable psk passphrase */
	if(mode == "psk" || mode == "psk2" || mode == "pskpsk2")
		inputCtrl(document.form.wl_wpa_psk, 1);
	else
		inputCtrl(document.form.wl_wpa_psk, 0);
	
	/* update wl_crypto */
	for(var i = 0; i < document.form.wl_crypto.length; ++i)
		if(document.form.wl_crypto[i].selected){
			cur_crypto = document.form.wl_crypto[i].value;
			break;
		}
	
	/* Reconstruct algorithm array from new crypto algorithms */
	if(mode == "psk" || mode == "psk2" || mode == "pskpsk2" || mode == "wpa" || mode == "wpa2" || mode == "wpawpa2"){
		/* Save current crypto algorithm */
			if(opts[opts.selectedIndex].text == "WPA-Personal" || opts[opts.selectedIndex].text == "WPA-Enterprise")
				new_array = new Array("TKIP");
			else if(opts[opts.selectedIndex].text == "WPA2-Personal" || opts[opts.selectedIndex].text == "WPA2-Enterprise")
				new_array = new Array("AES");
			else
				new_array = new Array("AES", "TKIP+AES");
		
		free_options(document.form.wl_crypto);
		for(var i = 0; i < new_array.length; i++){
			document.form.wl_crypto[i] = new Option(new_array[i], new_array[i].toLowerCase());
			document.form.wl_crypto[i].value = new_array[i].toLowerCase();
			if(new_array[i].toLowerCase() == cur_crypto)
				document.form.wl_crypto[i].selected = true;
		}
	}
}

function change_wep_type(mode){

	var cur_wep = document.form.wl_wep_x.value;
	var wep_type_array;
	var value_array;
	var show_wep_x = 0;
	
	free_options(document.form.wl_wep_x);
	
	//if(mode == "shared" || mode == "radius"){ //2009.03 magic
	if(mode == "shared"){ //2009.03 magic
		wep_type_array = new Array("WEP-64bits", "WEP-128bits");
		value_array = new Array("1", "2");
		show_wep_x = 1;
	}
	else if(mode == "open" && (document.form.wl_nmode_x.value == 2 || sw_mode == 2)){
		wep_type_array = new Array("None", "WEP-64bits", "WEP-128bits");
		value_array = new Array("0", "1", "2");
		show_wep_x = 1;
	}
	else {
		wep_type_array = new Array("None");
		value_array = new Array("0");
		cur_wep = "0";
	}

	add_options_x2(document.form.wl_wep_x, wep_type_array, value_array, cur_wep);
	inputCtrl(document.form.wl_wep_x, show_wep_x);


	change_wlweptype(document.form.wl_wep_x);
}

function change_wlweptype(wep_type_obj){
	if(wep_type_obj.value == "0"){  //2009.03 magic
		inputCtrl(document.form.wl_key, 0);
		inputCtrl(document.form.wl_asuskey1, 0);
	}
	else{
		inputCtrl(document.form.wl_key, 1);
		inputCtrl(document.form.wl_asuskey1, 1);
	}
	
	wl_wep_change();
}

function wl_wep_change(){
	var mode = document.form.wl_auth_mode_x.value;
	var wep = document.form.wl_wep_x.value;
	if ((mode == "shared" || mode == "open") && wep != "0")
		show_key();
}

function show_key(){
	switchType(document.form.wl_asuskey1,true);

	var wep_type = document.form.wl_wep_x.value;
	var keyindex = document.form.wl_key.value;
	var cur_key_obj = eval("document.form.wl_key"+keyindex);
	var cur_key_length = cur_key_obj.value.length;
	
	if(wep_type == 1){
		if(cur_key_length == 5 || cur_key_length == 10)
			document.form.wl_asuskey1.value = cur_key_obj.value;
		else
			document.form.wl_asuskey1.value = ""; //0000000000
	}
	else if(wep_type == 2){
		if(cur_key_length == 13 || cur_key_length == 26)
			document.form.wl_asuskey1.value = cur_key_obj.value;
		else
			document.form.wl_asuskey1.value = ""; //00000000000000000000000000
	}
	else
		document.form.wl_asuskey1.value = "";
	
}

function show_LAN_info(v){
	var lan_ipaddr_t = '<% nvram_get("lan_ipaddr_t"); %>';
	if(lan_ipaddr_t != '')
		showtext($("LANIP"), lan_ipaddr_t);
	else	
		showtext($("LANIP"), '<% nvram_get("lan_ipaddr"); %>');

	if(yadns_support){
		var mode = (yadns_enable != 0) ? yadns_mode : -1;
		showtext($("yadns_mode"), get_yadns_modedesc(mode));
		for(var i = 0; i < 3; i++){
			var visible = (yadns_enable != 0 && i != mode && yadns_clients[i]) ? true : false;
			var modedesc = visible ? get_yadns_modedesc(i) + ": <#Full_Clients#> " + yadns_clients[i] : "";
			showtext2($("yadns_mode" + i), modedesc, visible);
		}
		$("yadns_status").style.display = "";
	}

	showtext($("PINCode"), '<% nvram_get("secret_code"); %>');
	showtext($("MAC"), '<% nvram_get("lan_hwaddr"); %>');
	showtext($("MAC_wl2"), '<% nvram_get("wl0_hwaddr"); %>');
	if(document.form.wl_unit.value == 1)
		showtext($("MAC_wl5"), '<% nvram_get("wl1_hwaddr"); %>');
	else if(document.form.wl_unit.value == 2)
		showtext($("MAC_wl5_2"), '<% nvram_get("wl2_hwaddr"); %>');

	if(document.form.wl_unit.value == 0){
		$("macaddr_wl5").style.display = "none";
		if(wl_info.band5g_2_support)
			$("macaddr_wl5_2").style.display = "none";	
		if(!band5g_support)
			$("macaddr_wl2_title").style.display = "none";
	}
	else if (document.form.wl_unit.value == 1){
		$("macaddr_wl2").style.display = "none";
		$("macaddr_wl5_2").style.display = "none";
		if(wl_info.band5g_2_support)
			$("macaddr_wl5_title").innerHTML = "5GHz-1 ";

	}
	else if (document.form.wl_unit.value == 2){
		$("macaddr_wl2").style.display = "none";
		$("macaddr_wl5").style.display = "none";
	}
	if(smart_connect_support){
		if(v == '1'){
			showtext($("MAC_wl2"), '<% nvram_get("wl0_hwaddr"); %>');
			showtext($("MAC_wl5"), '<% nvram_get("wl1_hwaddr"); %>');
			showtext($("MAC_wl5_2"), '<% nvram_get("wl2_hwaddr"); %>');
			$("macaddr_wl5_title").innerHTML = "5GHz-1 ";
			$("macaddr_wl2").style.display = "";
			$("macaddr_wl5").style.display = "";
			$("macaddr_wl5_2").style.display = "";
			parent.document.getElementById("statusframe").height = 760;
		}else{
			parent.document.getElementById("statusframe").height = 735;
		}
	}
}

var secs;
var timerID = null;
var timerRunning = false;
var timeout = 1000;
var delay = 500;
var stopFlag=0;

function detect_qtn_ready(){
	if(parent.qtn_state_t != "1")
		setTimeout('detect_qtn_ready();', 1000);
	else
		document.form.submit();
}

function submitForm(){
	var auth_mode = document.form.wl_auth_mode_x.value;

	if(document.form.wl_wpa_psk.value == "<#wireless_psk_fillin#>")
		document.form.wl_wpa_psk.value = "";
		
	if(!validator.stringSSID(document.form.wl_ssid))
		return false;
	
	stopFlag = 1;
	document.form.current_page.value = "/index.asp";
	document.form.next_page.value = "/index.asp";
	
	if(auth_mode == "psk" || auth_mode == "psk2" || auth_mode == "pskpsk2"){
		if(!validator.psk(document.form.wl_wpa_psk))
			return false;
	}
	else if(auth_mode == "wpa" || auth_mode == "wpa2" || auth_mode == "wpawpa2" || auth_mode == "radius"){
		document.form.target = "";
		document.form.next_page.value = "/Advanced_WSecurity_Content.asp";
	}
	else{
		if(!validator.wlKey(document.form.wl_asuskey1))
			return false;
	}
	
	var wep11 = eval('document.form.wl_key'+document.form.wl_key.value);
	wep11.value = document.form.wl_asuskey1.value;
	
	if((auth_mode == "shared" || auth_mode == "wpa" || auth_mode == "wpa2" || auth_mode == "wpawpa2" || auth_mode == "radius")
			&& document.form.wps_enable.value == "1"){
		document.form.wps_enable.value = "0";
	}
	document.form.wsc_config_state.value = "1";

	parent.showLoading();
	if(based_modelid == "RT-AC87U" && "<% nvram_get("wl_unit"); %>" == "1"){
		parent.stopFlag = '0';
		detect_qtn_ready();
	}else
		document.form.submit();	

	return true;
}

function clean_input(obj){
	if(obj.value == "<#wireless_psk_fillin#>")
			obj.value = "";
}

function gotoSiteSurvey(){
	if(sw_mode == 2)
		parent.location.href = '/QIS_wizard.htm?flag=sitesurvey&band='+'<% nvram_get("wl_unit"); %>';
	else
		parent.location.href = '/QIS_wizard.htm?flag=sitesurvey_mb';
}

function startPBCmethod(){
	return 0;
}

function wpsPBC(obj){
	return 0;
}

function manualSetup(){
	return 0;	
}

function tab_reset(v){
	var tab_array1 = document.getElementsByClassName("tab_NW");
	var tab_array2 = document.getElementsByClassName("tabclick_NW");

	var tab_width = Math.floor(270/(wl_info.wl_if_total+1));
	var i = 0;
	while(i < tab_array1.length){
		tab_array1[i].style.width=tab_width+'px';
		tab_array1[i].style.display = "";
	i++;
	}
	if(typeof tab_array2[0] != "undefined"){
		tab_array2[0].style.width=tab_width+'px';
		tab_array2[0].style.display = "";
	}
	if(v == 0){
		$("span0").innerHTML = "2.4GHz";
		if(wl_info.band5g_2_support){
			$("span1").innerHTML = "5GHz-1";
			$("span2").innerHTML = "5GHz-2";
		}else{
			$("span1").innerHTML = "5GHz";
			$("t2").style.display = "none";
		}
	}else if(v == 1){	//Smart Connect
		$("span0").innerHTML = "Tri-band Smart Connect";
		$("t1").style.display = "none";
		$("t2").style.display = "none";				
		$("t0").style.width = (tab_width*wl_info.wl_if_total) +'px';
	}
	else if(v == 2){ //5GHz Smart Connect
		$("span0").innerHTML = "2.4GHz";
		$("span1").innerHTML = "5GHz Smart Connect";
		$("t2").style.display = "none";	
		$("t1").style.width = "140px";
	}
}

function change_smart_connect(v){
	document.form.smart_connect_x.value = v;
	show_LAN_info(v);
	switch(v){
		case '0':
				tab_reset(0);	
				break;
		case '1': 
				if('<% nvram_get("wl_unit"); %>' != 0)
					tabclickhandler(0);
				else
					tab_reset(1);
				break;
		case '2': 
				if(!('<% nvram_get("wl_unit"); %>' == 0 || '<% nvram_get("wl_unit"); %>' == 1))
					tabclickhandler(1);
				else
					tab_reset(2);
				break;
	}
}
</script>
</head>
<body class="statusbody" onload="initial();">
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="form" action="/start_apply2.htm">
<input type="hidden" name="current_page" value="device-map/router.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_wait" value="8">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="wps_enable" value="<% nvram_get("wps_enable"); %>">
<input type="hidden" name="wsc_config_state" value="<% nvram_get("wsc_config_state"); %>">
<input type="hidden" name="wl_key1" value="">
<input type="hidden" name="wl_key2" value="">
<input type="hidden" name="wl_key3" value="">
<input type="hidden" name="wl_key4" value="">
<input type="hidden" name="wl_ssid_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_ssid"); %>">
<input type="hidden" name="wlc_ure_ssid_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wlc_ure_ssid"); %>" disabled>
<input type="hidden" name="wl_wpa_psk_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_wpa_psk"); %>">
<input type="hidden" name="wl_auth_mode_orig" value="<% nvram_get("wl_auth_mode_x"); %>">
<input type="hidden" name="wl_wep_x_orig" value="<% nvram_get("wl_wep_x"); %>">
<input type="hidden" name="wl_key_type" value="<% nvram_get("wl_key_type"); %>"><!--Lock Add 1125 for ralink platform-->
<input type="hidden" name="wl_key_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key"); %>">
<input type="hidden" name="wl_key1_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key1"); %>">
<input type="hidden" name="wl_key2_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key2"); %>">
<input type="hidden" name="wl_key3_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key3"); %>">
<input type="hidden" name="wl_key4_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key4"); %>">
<input type="hidden" name="wl_nmode_x" value="<% nvram_get("wl_nmode_x"); %>"><!--Lock Add 20091210 for n only-->
<input type="hidden" name="wps_band" value="<% nvram_get("wps_band"); %>">
<input type="hidden" name="wl_unit" value="<% nvram_get("wl_unit"); %>">
<input type="hidden" name="wl_subunit" value="-1">
<input type="hidden" name="wl_radio" value="<% nvram_get("wl_radio"); %>">
<input type="hidden" name="wl_txbf" value="<% nvram_get("wl_txbf"); %>">
<input type="hidden" name="smart_connect_x" value="<% nvram_get("smart_connect_x"); %>">

<table border="0" cellpadding="0" cellspacing="0">
<tr>
	<td>		
		<table width="100px" border="0" align="left" style="margin-left:8px;" cellpadding="0" cellspacing="0">
			<td>
				<div id="t0" class="tab_NW" align="center" style="font-weight: bolder;display:none; margin-right:2px; width:90px;" onclick="tabclickhandler(0)">
					<span id="span0" style="cursor:pointer;font-weight: bolder;">2.4GHz</span>
				</div>
			</td>
			<td>
				<div id="t1" class="tab_NW" align="center" style="font-weight: bolder;display:none; margin-right:2px; width:90px;" onclick="tabclickhandler(1)">
					<span id="span1" style="cursor:pointer;font-weight: bolder;">5GHz</span>
				</div>
			</td>
			<td>
				<div id="t2" class="tab_NW" align="center" style="font-weight: bolder;display:none; margin-right:2px; width:90px;" onclick="tabclickhandler(2)">
					<span id="span2" style="cursor:pointer;font-weight: bolder;">5GHz-2</span>
				</div>
			</td>
			<td>
				<div id="t3" class="tab_NW" align="center" style="font-weight: bolder; margin-right:2px; width:90px;" onclick="tabclickhandler(3)">
					<span id="span3" style="cursor:pointer;font-weight: bolder;">Status</span>
				</div>
			</td>
		</table>
	</td>
</tr>

<tr>
	<td>
		<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" class="table1px" id="WLnetworkmap_re" style="display:none">
		  <tr>
		    <td height="50" style="padding:10px 15px 0px 15px;">
		    	<p class="formfonttitle_nwm" style="float:left;"><#APSurvey_action_search_again_hint2#></p>
					<br />
			  	<input type="button" class="button_gen" onclick="gotoSiteSurvey();" value="<#QIS_rescan#>" style="float:right;">
			  	<!--input type="button" class="button_gen" onclick="manualSetup();" value="<#Manual_Setting_btn#>" style="float:right;"-->
     			<img style="margin-top:5px; *margin-top:-10px; visibility:hidden;" src="/images/New_ui/networkmap/linetwo2.png">
		    </td>
		  </tr>
		</table>

		<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" class="table1px" id="WLnetworkmap">
  		<tr id="smartcon_enable_field" style="display:none">
			  	<td>
			  	<div><table><tr>
			  		<td style="padding:8px 5px 0px 0px;">
			  			<p class="formfonttitle_nwm" >Smart Connect: </p>
			  		</td>
			  		<td>
					<div id="smartcon_enable_block" style="display:none;">
			    		<span style="color:#FFF;" id="smart_connect_enable_word">&nbsp;&nbsp;</span>
			    		<input type="button" name="enableSmartConbtn" id="enableSmartConbtn" value="" class="button_gen" onClick="change_smart_connect();">
			    		<br>
			    	</div>
			    		<div class="left" style="width: 94px;" id="radio_smartcon_enable"></div>
						<div class="clear"></div>					
						<script type="text/javascript">
								var flag = '<% get_parameter("flag"); %>';

							if(flag == '')
								smart_connect_flag_t = '<% nvram_get("smart_connect_x"); %>';
							else
								smart_connect_flag_t = flag;

								$j('#radio_smartcon_enable').iphoneSwitch(smart_connect_flag_t>0, 
								 function() {
									change_smart_connect('1');
								 },
								 function() {
									change_smart_connect('0');
								 }
							);
						</script>			  			
			  		</td>
			  	</tr></table></div>
		  	  </td>			
  		</tr>  		
  		<tr id="smartcon_enable_line" style="display:none"><td><img style="margin-top:-2px; *margin-top:-10px;"src="/images/New_ui/networkmap/linetwo2.png"></td></tr>
  		<tr>
    			<td style="padding:5px 10px 0px 10px; ">
  	  			<p class="formfonttitle_nwm" ><#Wireless_name#>(SSID)</p>
      			<input style="*margin-top:-7px; width:260px;" id="wl_ssid" type="text" name="wl_ssid" value="<% nvram_get("wl_ssid"); %>" maxlength="32" size="22" class="input_25_table" >
      			<img style="margin-top:5px; *margin-top:-10px;"src="/images/New_ui/networkmap/linetwo2.png">
    			</td>
  		</tr>  
  		<tr>
    			<td style="padding:5px 10px 0px 10px; *padding:1px 10px 0px 10px;">
    					<p class="formfonttitle_nwm" ><#WLANConfig11b_AuthenticationMethod_itemname#></p>
				  		<select style="*margin-top:-7px;" name="wl_auth_mode_x" class="input_option" onChange="authentication_method_change(this);">
							<option value="open"    <% nvram_match("wl_auth_mode_x", "open",   "selected"); %>>Open System</option>
							<option value="shared"  <% nvram_match("wl_auth_mode_x", "shared", "selected"); %>>Shared Key</option>
							<option value="psk"     <% nvram_match("wl_auth_mode_x", "psk",    "selected"); %>>WPA-Personal</option>
							<option value="psk2"    <% nvram_match("wl_auth_mode_x", "psk2",   "selected"); %>>WPA2-Personal</option>
							<option value="pskpsk2" <% nvram_match("wl_auth_mode_x", "pskpsk2","selected"); %>>WPA-Auto-Personal</option>
							<option value="wpa"     <% nvram_match("wl_auth_mode_x", "wpa",    "selected"); %>>WPA-Enterprise</option>
							<option value="wpa2"    <% nvram_match("wl_auth_mode_x", "wpa2",   "selected"); %>>WPA2-Enterprise</option>
							<option value="wpawpa2" <% nvram_match("wl_auth_mode_x", "wpawpa2","selected"); %>>WPA-Auto-Enterprise</option>
							<option value="radius"  <% nvram_match("wl_auth_mode_x", "radius", "selected"); %>>Radius with 802.1x</option>
				  		</select>
							<img style="display:none;margin-top:-30px;margin-left:185px;cursor:pointer;" id="wl_nmode_x_hint" src="/images/alertImg.png" width="30px" onClick="parent.overlib(parent.helpcontent[0][24], parent.FIXX, 870, parent.FIXY, 350);" onMouseOut="parent.nd();">
	  					<img style="margin-top:5px; *margin-top:-10px;"src="/images/New_ui/networkmap/linetwo2.png">
    			</td>
  		</tr>
  		<tr id='all_related_wep' style='display:none;'>
			<td style="padding:5px 10px 0px 10px; *padding:1px 10px 0px 10px;">
				<p class="formfonttitle_nwm" ><#WLANConfig11b_WEPType_itemname#></p>
	  			<select style="*margin-top:-7px;" name="wl_wep_x" id="wl_wep_x" class="input_option" onchange="change_wlweptype(this);">
						<option value="0" <% nvram_match("wl_wep_x", "0", "selected"); %>><#wl_securitylevel_0#></option>
						<option value="1" <% nvram_match("wl_wep_x", "1", "selected"); %>>WEP-64bits</option>
						<option value="2" <% nvram_match("wl_wep_x", "2", "selected"); %>>WEP-128bits</option>
	  			</select>	  			
	  			<img style="margin-top:5px; *margin-top:-10px;"src="/images/New_ui/networkmap/linetwo2.png">
			</td>
  		</tr>
  		<tr id='all_wep_key' style='display:none;'>
    			<td style="padding:5px 10px 0px 10px; *padding:1px 10px 0px 10px;">
    				<p class="formfonttitle_nwm" ><#WLANConfig11b_WEPDefaultKey_itemname#></p>
      				<select style="*margin-top:-7px;" name="wl_key" class="input_option" onchange="show_key();">
					<option value="1" <% nvram_match("wl_key", "1", "selected"); %>>1</option>
					<option value="2" <% nvram_match("wl_key", "2", "selected"); %>>2</option>
					<option value="3" <% nvram_match("wl_key", "3", "selected"); %>>3</option>
					<option value="4" <% nvram_match("wl_key", "4", "selected"); %>>4</option>
      			</select>      			
	  			<img style="margin-top:5px; *margin-top:-10px;" src="/images/New_ui/networkmap/linetwo2.png">
    			</td>
  		</tr>
  		<tr id='asus_wep_key'>
    			<td style="padding:5px 10px 0px 10px; ">
	    			<p class="formfonttitle_nwm" ><#WLANConfig11b_WEPKey_itemname#>
						</p>
							<input id="wl_asuskey1" name="wl_asuskey1" style="width:260px;*margin-top:-7px;" type="password" autocapitalization="off" onBlur="switchType(this, false);" onFocus="switchType(this, true);" onKeyUp="return change_wlkey(this, 'WLANConfig11b');" value="" maxlength="27" class="input_25_table">
      			<img style="margin-top:5px; *margin-top:-10px;"src="/images/New_ui/networkmap/linetwo2.png">
    			</td>
  		</tr>
  		<tr id='wl_crypto' style='display:none;'>
			<td style="padding:5px 10px 0px 10px; *padding:1px 10px 0px 10px;">
	  			<p class="formfonttitle_nwm" ><#WLANConfig11b_WPAType_itemname#></p>
	  			<select style="*margin-top:-7px;" name="wl_crypto" class="input_option" onchange="wl_auth_mode_change(0);">
					<!--option value="tkip" <% nvram_match("wl_crypto", "tkip", "selected"); %>>TKIP</option-->
					<option value="aes" <% nvram_match("wl_crypto", "aes", "selected"); %>>AES</option>
					<option value="tkip+aes" <% nvram_match("wl_crypto", "tkip+aes", "selected"); %>>TKIP+AES</option>
	  			</select>	  			
	  			<img style="margin-top:5px; *margin-top:-10px;"src="/images/New_ui/networkmap/linetwo2.png">
			</td>
  		</tr>
  		<tr id='wl_wpa_psk_tr' style='display:none'>
    			<td style="padding:5px 10px 0px 10px;">
      			<p class="formfonttitle_nwm" ><#WPA-PSKKey#>
						</p>	
							<input id="wl_wpa_psk" name="wl_wpa_psk" style="width:260px;*margin-top:-7px;" type="password" autocapitalization="off" onBlur="switchType(this, false);" onFocus="switchType(this, true);" value="" maxlength="64" class="input_25_table"/>
      			<img style="margin-top:5px; *margin-top:-10px;"src="/images/New_ui/networkmap/linetwo2.png">
    			</td>
  		</tr>

  		<tr id="wl_radio_tr" style="display:none">
			<td style="padding:5px 10px 0px 10px;">
	    			<p class="formfonttitle_nwm" style="float:left;"><#Wireless_Radio#></p>
				<div class="left" style="width:94px; float:right;" id="radio_wl_radio"></div>
				<div class="clear"></div>
				<script type="text/javascript">
					//var $j = jQuery.noConflict();
					$j('#radio_wl_radio').iphoneSwitch('<% nvram_get("wl_radio"); %>', 
							 function() {
								document.form.wl_radio.value = "1";
							 },
							 function() {
								document.form.wl_radio.value = "0";
							 }
						);
				</script>
      			<img style="margin-top:5px; *margin-top:-10px;"src="/images/New_ui/networkmap/linetwo2.png">
			</td>
  		</tr>
  		<!--   Viz add 2011.12 for RT-N56U Ralink  start {{ -->
  		<tr id="wl_txbf_tr" style="display:none">
			<td style="padding:5px 10px 0px 10px;">
	    			<p class="formfonttitle_nwm" style="float:left;">AiRadar</p>
				<div class="left" style="width:94px; float:right;" id="radio_wl_txbf"></div>
				<div class="clear"></div>
				<script type="text/javascript">
					var $j = jQuery.noConflict();
					$j('#radio_wl_txbf').iphoneSwitch('<% nvram_get("wl_txbf"); %>', 
							 function() {
								document.form.wl_txbf.value = "1";
								return true;
							 },
							 function() {
								document.form.wl_txbf.value = "0";
								return true;
							 }
						);
				</script>
      			<img style="margin-top:5px; *margin-top:-10px;"src="/images/New_ui/networkmap/linetwo2.png">
			</td>
  		</tr>  		
  		<!--   Viz add 2011.12 for RT-N56U Ralink   end  }} -->			
 		</table>		
  	</td>
</tr>

<tr>
	<td> 			
 		<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" class="table1px">
  		<tr id="apply_tr">
    			<td style="border-bottom:5px #2A3539 solid;padding:5px 10px 5px 10px;">
    				<input id="applySecurity" type="button" class="button_gen" value="<#CTL_apply#>" onclick="submitForm();" style="margin-left:90px;">
    			</td>
  		</tr>
  		<tr>
    			<td style="padding:10px 10px 0px 10px;">
    				<p class="formfonttitle_nwm" ><#LAN_IP#></p>
    				<p style="padding-left:10px; margin-top:3px; *margin-top:-5px; margin-right:10px; background-color:#444f53; line-height:20px;" id="LANIP"></p>
      			<img style="margin-top:5px; *margin-top:-10px;" src="/images/New_ui/networkmap/linetwo2.png">
    			</td>
  		</tr>  
  		<tr>
    			<td style="padding:5px 10px 0px 10px;">
    				<p class="formfonttitle_nwm" ><#PIN_code#></p>
    				<p style="padding-left:10px; margin-top:3px; *margin-top:-5px; margin-right:10px; background-color:#444f53; line-height:20px;" id="PINCode"></p>
      			<img style="margin-top:5px; *margin-top:-10px;" src="/images/New_ui/networkmap/linetwo2.png">
    			</td>
  		</tr>
  		<tr id="yadns_status" style="display:none;">
    			<td style="padding:5px 10px 0px 10px;">
    				<p class="formfonttitle_nwm" ><#YandexDNS#></p>
    				<a href="/YandexDNS.asp" target="_parent">
    				<p style="padding-left:10px; margin-top:3px; *margin-top:-5px; margin-right:10px; background-color:#444f53; line-height:20px;" id="yadns_mode"></p>
    				<p style="padding-left:10px; margin-top:3px; *margin-top:-5px; margin-right:10px; background-color:#444f53; line-height:20px;" id="yadns_mode0"></p>
    				<p style="padding-left:10px; margin-top:3px; *margin-top:-5px; margin-right:10px; background-color:#444f53; line-height:20px;" id="yadns_mode1"></p>
    				<p style="padding-left:10px; margin-top:3px; *margin-top:-5px; margin-right:10px; background-color:#444f53; line-height:20px;" id="yadns_mode2"></p>
    				</a>
      			<img style="margin-top:5px; *margin-top:-10px;" src="/images/New_ui/networkmap/linetwo2.png">
    			</td>
  		</tr>
  		<tr>
    			<td style="padding:5px 10px 0px 10px;">
    				<p class="formfonttitle_nwm" >LAN <#MAC_Address#></p>
    				<p style="padding-left:10px; margin-top:3px; *margin-top:-5px; padding-bottom:3px; margin-right:10px; background-color:#444f53; line-height:20px;" id="MAC"></p>
    				<img style="margin-top:5px; *margin-top:-10px;" src="/images/New_ui/networkmap/linetwo2.png">
    			</td>
  		</tr>     
  		<tr id="macaddr_wl2">
    			<td style="padding:5px 10px 0px 10px;">
    				<p class="formfonttitle_nwm" >Wireless <span id="macaddr_wl2_title">2.4GHz </span><#MAC_Address#></p>
    				<p style="padding-left:10px; margin-bottom:5px; margin-top:3px; *margin-top:-5px; padding-bottom:3px; margin-right:10px; background-color:#444f53; line-height:20px;" id="MAC_wl2"></p>
    			</td>
  		</tr>     
  		<tr id="macaddr_wl5">
    			<td style="padding:5px 10px 0px 10px;">
    				<p class="formfonttitle_nwm" >Wireless <span id="macaddr_wl5_title">5GHz </span><#MAC_Address#></p>
    				<p style="padding-left:10px; margin-bottom:5px; margin-top:3px; *margin-top:-5px; padding-bottom:3px; margin-right:10px; background-color:#444f53; line-height:20px;" id="MAC_wl5"></p>
    			</td>
  		</tr>
  		<tr id="macaddr_wl5_2" style="display:none;">
    			<td style="padding:5px 10px 0px 10px;">
    				<p class="formfonttitle_nwm" >Wireless <span id="macaddr_wl5_title">5GHz-2 </span><#MAC_Address#></p>
    				<p style="padding-left:10px; margin-bottom:5px; margin-top:3px; *margin-top:-5px; padding-bottom:3px; margin-right:10px; background-color:#444f53; line-height:20px;" id="MAC_wl5_2"></p>
    			</td>
  		</tr>  
		</table>
	</td>
</tr>
</table>			
</form>
<form method="post" name="WPSForm" id="WPSForm" action="/stawl_apply.htm">
<input type="hidden" name="current_page" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="flag" value="">
</form>

<form method="post" name="stopPINForm" id="stopPINForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="wsc_config_command" value="<% nvram_get("wsc_config_command"); %>">
</form>
</body>
</html>
