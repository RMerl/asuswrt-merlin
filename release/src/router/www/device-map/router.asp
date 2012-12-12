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
<script type="text/javascript" src="formcontrol.js"></script>
<script type="text/javascript" src="/ajax.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>

var $j = jQuery.noConflict();
<% wl_get_parameter(); %>

function initial(){
	$("t0").className = <% nvram_get("wl_unit"); %> ? "tab_NW" : "tabclick_NW";
	$("t1").className = <% nvram_get("wl_unit"); %> ? "tabclick_NW" : "tab_NW";

	if(sw_mode == 2){
		if(parent.psta_support == -1){
			if('<% nvram_get("wl_unit"); %>' == '<% nvram_get("wlc_band"); %>' && '<% nvram_get("wl_subunit"); %>' != '1'){
				tabclickhandler('<% nvram_get("wl_unit"); %>');
			}
			else if('<% nvram_get("wl_unit"); %>' != '<% nvram_get("wlc_band"); %>' && '<% nvram_get("wl_subunit"); %>' == '1'){
				tabclickhandler('<% nvram_get("wl_unit"); %>');
			}
		}
		else{
			if('<% nvram_get("wl_unit"); %>' == '<% nvram_get("wlc_band"); %>'){
				$("WLnetworkmap").style.display = "none";
				$("applySecurity").style.display = "none";
				$("WLnetworkmap_re").style.display = "";
			}
			else{
				if('<% nvram_get("wl_subunit"); %>' != '-1'){
					tabclickhandler('<% nvram_get("wl_unit"); %>');
				}
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
	if(parent.sw_mode == 2 && '<% nvram_get("wlc_band"); %>' == '<% nvram_get("wl_unit"); %>')
		document.form.wl_subunit.value = 1;
	else
		document.form.wl_subunit.value = -1;
	
	if(band5g_support != -1){
		$("t0").style.display = "";
		$("t1").style.display = "";
		/* allow to use the other band as a wireless AP
		if(sw_mode == 2 && parent.psta_support != -1)
			$('t'+((parseInt(<% nvram_get("wlc_band"); %>+1))%2)).style.display = 'none';
		*/
	}

	if($("t1").className == "tabclick_NW" && 	parent.Rawifi_support != -1)	//no exist Rawifi
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
	
	if(sw_mode == 2){				
			//remove Crypto: WPA & RADIUS
			for(i=document.form.wl_auth_mode_x.length-1;i>=0;i--){
					var authmode_opt = document.form.wl_auth_mode_x.options[i].value.toString();
					if(authmode_opt.match('wpa') || authmode_opt.match('radius'))
      					document.form.wl_auth_mode_x.remove(i);      									
  		}
  }
	
	wl_auth_mode_change(1);
	show_LAN_info();
	if(parent.psta_support != -1 && parent.sw_mode == 2)
		parent.show_middle_status('<% nvram_get("wlc_auth_mode"); %>', '', 0);		
	else
		parent.show_middle_status(document.form.wl_auth_mode_x.value, document.form.wl_wpa_mode.value, parseInt(document.form.wl_wep_x.value));
	
	flash_button();
	automode_hint();		
}

function tabclickhandler(wl_unit){
	if(parent.sw_mode == 2 && '<% nvram_get("wlc_band"); %>' == wl_unit)
		document.form.wl_subunit.value = 1;
	else
		document.form.wl_subunit.value = -1;

	document.form.wl_unit.value = wl_unit;
	document.form.current_page.value = "device-map/router.asp";
	FormActions("/apply.cgi", "change_wl_unit", "", "");
	document.form.target = "";
	document.form.submit();
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
	var opts = document.form.wl_auth_mode_x.options;
	var new_array;
	var cur_crypto;
	var cur_key_index, cur_key_obj;
	
	//if(mode == "open" || mode == "shared" || mode == "radius"){ //2009.03 magic
	if(mode == "open" || mode == "shared"){ //2009.03 magic
		//blocking("all_related_wep", 1);
		$("all_related_wep").style.display = "";
		$("all_wep_key").style.display = "";
		$("asus_wep_key").style.display = "";
		change_wep_type(mode);
	}
	else{
		//blocking("all_related_wep", 0);
		$("all_related_wep").style.display = "none";
		$("all_wep_key").style.display = "none";
		$("asus_wep_key").style.display = "none";
	}
	
	/* enable/disable crypto algorithm */
	if(mode == "wpa" || mode == "wpa2" || mode == "wpawpa2" || mode == "psk" || mode == "psk2" || mode == "pskpsk2")
		$("wl_crypto").style.display = "";	
	else
		$("wl_crypto").style.display = "none";
	
	/* enable/disable psk passphrase */
	if(mode == "psk" || mode == "psk2" || mode == "pskpsk2")
		$("wl_wpa_psk_tr").style.display = "";
	else
		$("wl_wpa_psk_tr").style.display = "none";
	
	/* update wl_crypto */
	for(var i = 0; i < document.form.wl_crypto.length; ++i)
		if(document.form.wl_crypto[i].selected){
			cur_crypto = document.form.wl_crypto[i].value;
			break;
		}
	
	/* Reconstruct algorithm array from new crypto algorithms */
	if(mode == "psk" || mode == "psk2" || mode == "pskpsk2"){
		/* Save current crypto algorithm */
			if(opts[opts.selectedIndex].text == "WPA-Personal")
				new_array = new Array("TKIP");
			else if(opts[opts.selectedIndex].text == "WPA2-Personal")
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
	else if(mode == "wpa" || mode == "wpawpa2"){
		if(opts[opts.selectedIndex].text == "WPA-Enterprise")
			new_array = new Array("TKIP");
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
	else if(mode == "wpa2"){
		new_array = new Array("AES");
		
		free_options(document.form.wl_crypto);
		for(var i = 0; i < new_array.length; i++){
			document.form.wl_crypto[i] = new Option(new_array[i], new_array[i].toLowerCase());
			document.form.wl_crypto[i].value = new_array[i].toLowerCase();
			if(new_array[i].toLowerCase() == cur_crypto)
				document.form.wl_crypto[i].selected = true;
		}
	}
	
	/* Save current network key index */
	for(var i = 0; i < document.form.wl_key.length; ++i)
		if(document.form.wl_key[i].selected){
			cur_key_index = document.form.wl_key[i].value;
			break;
		}
	
	/* Define new network key indices */
	//if(mode == "psk" || mode == "wpa" || mode == "wpa2" || mode == "radius")
	if(mode == "psk" || mode == "psk2" || mode == "pskpsk2" || mode == "wpa" || mode == "wpa2" || mode == "wpawpa2")
		new_array = new Array("2", "3");
	else{
		new_array = new Array("1", "2", "3", "4");
		
		if(!isload)
			cur_key_index = "1";
	}
	
	/* Reconstruct network key indices array from new network key indices */
	free_options(document.form.wl_key);
	for(var i = 0; i < new_array.length; i++){
		document.form.wl_key[i] = new Option(new_array[i], new_array[i]);
		document.form.wl_key[i].value = new_array[i];
		if(new_array[i] == cur_key_index)
			document.form.wl_key[i].selected = true;
	}
	
	wl_wep_change();
}

function change_wep_type(mode){

	var cur_wep = document.form.wl_wep_x.value;
	var wep_type_array;
	var value_array;
	
	free_options(document.form.wl_wep_x);
	
	//if(mode == "shared" || mode == "radius"){ //2009.03 magic
	if(mode == "shared"){ //2009.03 magic
		wep_type_array = new Array("WEP-64bits", "WEP-128bits");
		value_array = new Array("1", "2");
	}
	else{
		wep_type_array = new Array("None", "WEP-64bits", "WEP-128bits");
		value_array = new Array("0", "1", "2");
	}
	
	add_options_x2(document.form.wl_wep_x, wep_type_array, value_array, cur_wep);
	
	
	if(mode == "open"){ //Lock Modified 20091230;
		if(document.form.wl_wep_x.value == 0){
			document.form.wl_wep_x.selectedIndex = 0;
		}
	}
	
	if(mode == "psk" || mode == "psk2" || mode == "pskpsk2" || mode == "wpa" || mode == "wpa2" || mode == "wpawpa2") //2009.03 magic
		document.form.wl_wep_x.value = "0";
	
	change_wlweptype(document.form.wl_wep_x);
}

function change_wlweptype(wep_type_obj){
	var mode = document.form.wl_auth_mode_x.value; //2009.03 magic
	
	//if(wep_type_obj.value == "0" || mode == "radius") //2009.03 magic
	if(wep_type_obj.value == "0"){  //2009.03 magic
		$("all_wep_key").style.display = "none";
		$("asus_wep_key").style.display = "none";
	}
	else{
		if(document.form.wl_nmode_x.value == 1 && document.form.wl_wep_x.value != 0){
			nmode_limitation2();
		}
		$("all_wep_key").style.display = "";
		$("asus_wep_key").style.display = "";
	}
	
	wl_wep_change();
	automode_hint();
}

function wl_wep_change(){
	var mode = document.form.wl_auth_mode_x.value;
	var wep = document.form.wl_wep_x.value;
	
	if(mode == "psk" || mode == "psk2" || mode == "pskpsk2" || mode == "wpa" || mode == "wpa2" || mode == "wpawpa2"){
		if(mode == "psk" || mode == "psk2" || mode == "pskpsk2"){
			$("wl_crypto").style.display = "";
			$("wl_wpa_psk_tr").style.display = "";
		}
		
		$("all_wep_key").style.display = "none";
		$("asus_wep_key").style.display = "none";
	}
	else{
		$("wl_crypto").style.display = "none";
		$("wl_wpa_psk_tr").style.display = "none";
		
		//if(mode == "radius") //2009.03 magic
		//	blocking("all_related_wep", 0); //2009.03 magic
		//else //2009.03 magic
		//	blocking("all_related_wep", 1);
		
		if(wep == "0" || mode == "radius"){
			$("all_wep_key").style.display = "none";
			$("asus_wep_key").style.display = "none";
		}
		else{
			$("all_wep_key").style.display = "";
			$("asus_wep_key").style.display = "";
			show_key();
		}
	}
	change_key_des();
}

function change_key_des(){
	var objs = getElementsByName_iefix("span", "key_des");
	var wep_type = document.form.wl_wep_x.value;
	var str = "";
	
	if(wep_type == "1")
		str = " (<#WLANConfig11b_WEPKey_itemtype1#>)";
	else if(wep_type == "2")
		str = " (<#WLANConfig11b_WEPKey_itemtype2#>)";
	
	str += ":";
	
	for(var i = 0; i < objs.length; ++i)
		showtext(objs[i], str);
}

function show_key(){
	if(document.form.wl_asuskey1_text)
			switchType_IE(document.form.wl_asuskey1_text);

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

function show_LAN_info(){
	var lan_ipaddr_t = '<% nvram_get("lan_ipaddr_t"); %>';
	if(lan_ipaddr_t != '')
		showtext($("LANIP"), lan_ipaddr_t);
	else	
		showtext($("LANIP"), '<% nvram_get("lan_ipaddr"); %>');

	showtext($("PINCode"), '<% nvram_get("secret_code"); %>');
	showtext($("MAC"), '<% nvram_get("lan_hwaddr"); %>');
	showtext($("MAC_wl2"), '<% nvram_get("wl0_hwaddr"); %>');
	showtext($("MAC_wl5"), '<% nvram_get("wl1_hwaddr"); %>');

	if(document.form.wl_unit.value == 0){
		$("macaddr_wl5").style.display = "none";
		if(band5g_support == -1)
			$("macaddr_wl2_title").style.display = "none";
	}
	else
		$("macaddr_wl2").style.display = "none";
}

function show_wepkey_help(){
	if(document.form.wl_wep_x.value == 1)
		parent.showHelpofDrSurf(0, 12);
	else if(document.form.wl_wep_x.value == 2)
		parent.showHelpofDrSurf(0, 13);
}

var secs;
var timerID = null;
var timerRunning = false;
var timeout = 1000;
var delay = 500;
var stopFlag=0;

function submitForm(){
	var auth_mode = document.form.wl_auth_mode_x.value;

	if(document.form.wl_wpa_psk.value == "<#wireless_psk_fillin#>")
		document.form.wl_wpa_psk.value = "";
		
	if(!validate_string_ssid(document.form.wl_ssid))
		return false;
	
	if(auth_mode == "psk" || auth_mode == "psk2" || auth_mode == "pskpsk2"){
		if(!validate_psk(document.form.wl_wpa_psk))
			return false;
	}
	else if(auth_mode == "wpa" || auth_mode == "wpa2" || auth_mode == "wpawpa2" || auth_mode == "radius"){
		document.form.target = "";
		document.form.next_page.value = "/Advanced_WSecurity_Content.asp";
	}
	else{
		if(!validate_wlkey(document.form.wl_asuskey1))
			return false;
	}
	
	stopFlag = 1;
	document.form.current_page.value = "/index.asp";
	document.form.next_page.value = "/index.asp";
	
	var wep11 = eval('document.form.wl_key'+document.form.wl_key.value);
	wep11.value = document.form.wl_asuskey1.value;
	
	if((auth_mode == "shared" || auth_mode == "wpa" || auth_mode == "wpa2" || auth_mode == "wpawpa2" || auth_mode == "radius")
			&& document.form.wps_enable.value == "1"){
		document.form.wps_enable.value = "0";
	}
	document.form.wsc_config_state.value = "1";

	if(wl6_support != -1)
		document.form.action_wait.value = parseInt(document.form.action_wait.value)+10;			// extend waiting time for BRCM new driver

	parent.showLoading();
	document.form.submit();	
	return true;
}

function startPBCmethod(){
}

function wpsPBC(obj){
}

function nmode_limitation2(){ //Lock add 2009.11.05 for TKIP limitation in n mode.
	if(document.form.wl_nmode_x.value == "1"){
		if(document.form.wl_auth_mode_x.selectedIndex == 0 && (document.form.wl_wep_x.selectedIndex == "1" || document.form.wl_wep_x.selectedIndex == "2")){
			alert("<#WLANConfig11n_nmode_limition_hint#>");
			document.form.wl_auth_mode_x.selectedIndex = 3;
			document.form.wl_wpa_mode.value = 2;
		}
		else if(document.form.wl_auth_mode_x.selectedIndex == 1){
			alert("<#WLANConfig11n_nmode_limition_hint#>");
			document.form.wl_auth_mode_x.selectedIndex = 3;
			document.form.wl_wpa_mode.value = 2;
		}
		else if(document.form.wl_auth_mode_x.selectedIndex == 2){
			alert("<#WLANConfig11n_nmode_limition_hint#>");
			document.form.wl_auth_mode_x.selectedIndex = 3;
			document.form.wl_wpa_mode.value = 2;
		}
		else if(document.form.wl_auth_mode_x.selectedIndex == 5){
			alert("<#WLANConfig11n_nmode_limition_hint#>");
			document.form.wl_auth_mode_x.selectedIndex = 6;
		}
		wl_auth_mode_change(1);
	}
}

var isNotIE = (navigator.userAgent.search("MSIE") == -1); 
function switchType(_method){
	if(isNotIE){
		document.form.wl_asuskey1.type = _method ? "text" : "password";
		document.form.wl_wpa_psk.type = _method ? "text" : "password";
	}
}

function switchType_IE(obj){
		if(isNotIE) return;		
		
		var tmp = "";
		tmp = obj.value;
		if(obj.id.indexOf('text') < 0){		//password
					if(obj.id.indexOf('psk') >= 0){							
							obj.style.display = "none";
							document.getElementById('wl_wpa_psk_text').style.display = "";
							document.getElementById('wl_wpa_psk_text').value = tmp;
							document.getElementById('wl_wpa_psk_text').focus();
					}
					if(obj.id.indexOf('asuskey') >= 0){
							obj.style.display = "none";
							document.getElementById('wl_asuskey1_text').style.display = "";
							document.getElementById('wl_asuskey1_text').value = tmp;						
							document.getElementById('wl_asuskey1_text').focus();
					}	
		}else{														//text					
					if(obj.id.indexOf('psk') >= 0){
							obj.style.display = "none";
							document.getElementById('wl_wpa_psk').style.display = "";
							document.getElementById('wl_wpa_psk').value = tmp;
					}
					if(obj.id.indexOf('asuskey') >= 0){
							obj.style.display = "none";
							document.getElementById('wl_asuskey1').style.display = "";
							document.getElementById('wl_asuskey1').value = tmp;						
					}					
		}
}

function clean_input(obj){
	if(obj.value == "<#wireless_psk_fillin#>")
			obj.value = "";
}

function change_authmode(o, s, v){
	change = 1;
	pageChanged = 1;	
	if(document.form.wl_wpa_psk_text)
			switchType_IE(document.form.wl_wpa_psk_text);
	
	if(v == "wl_auth_mode_x"){ /* Handle AuthenticationMethod Change */
		wl_auth_mode_change(0);
		if(o.value == "psk" || o.value == "psk2" || o.value == "pskpsk2" || o.value == "wpa" || o.value == "wpawpa2"){
			opts = document.form.wl_auth_mode_x.options;			
			if(opts[opts.selectedIndex].text == "WPA-Personal"){
				document.form.wl_wpa_mode.value="1";
			}
			else if(opts[opts.selectedIndex].text == "WPA2-Personal")
				document.form.wl_wpa_mode.value="2";
			else if(opts[opts.selectedIndex].text == "WPA-Auto-Personal")
				document.form.wl_wpa_mode.value="0";
			else if(opts[opts.selectedIndex].text == "WPA-Enterprise")
				document.form.wl_wpa_mode.value="3";
			else if(opts[opts.selectedIndex].text == "WPA-Auto-Enterprise")
				document.form.wl_wpa_mode.value="4";

		}
		else if(o.value == "shared"){ //2009.03.10 Lock
			document.form.wl_key.focus();
		}
		nmode_limitation();

	}
	
	automode_hint();
	return true;
}


function gotoSiteSurvey(){
	parent.location.href = '/QIS_wizard.htm?flag=sitesurvey&band='+'<% nvram_get("wl_unit"); %>';
}
</script>
</head>
<body class="statusbody" onload="initial();">
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="form" action="/start_apply2.htm">
<input type="hidden" name="current_page" value="device-map/router.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_wait" value="8">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="wps_enable" value="<% nvram_get("wps_enable"); %>">
<input type="hidden" name="wsc_config_state" value="<% nvram_get("wsc_config_state"); %>">
<input type="hidden" name="wl_wpa_mode" value="<% nvram_get("wl_wpa_mode"); %>">
<input type="hidden" name="wl_key1" value="">
<input type="hidden" name="wl_key2" value="">
<input type="hidden" name="wl_key3" value="">
<input type="hidden" name="wl_key4" value="">
<input type="hidden" name="wl_ssid_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_ssid"); %>">
<input type="hidden" name="wlc_ure_ssid_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wlc_ure_ssid"); %>" disabled>
<input type="hidden" name="wl_wpa_psk_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_wpa_psk"); %>">
<input type="hidden" name="wl_auth_mode_orig" value="<% nvram_get("wl_auth_mode_x"); %>">
<input type="hidden" name="wl_wpa_mode_orig" value="<% nvram_get("wl_wpa_mode"); %>">
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

<table border="0" cellpadding="0" cellspacing="0">
<tr>
	<td>		
		<table width="100px" border="0" align="left" style="margin-left:8px;" cellpadding="0" cellspacing="0">
  		<td>
				<div id="t0" class="tabclick_NW" align="center" style="font-weight: bolder;display:none; margin-right:2px; width:90px;" onclick="tabclickhandler(0)">
					<span id="span1" style="cursor:pointer;font-weight: bolder;">2.4GHz</span>
				</div>
			</td>
  		<td>
				<div id="t1" class="tab_NW" align="center" style="font-weight: bolder;display:none; margin-right:2px; width:90px;" onclick="tabclickhandler(1)">
					<span id="span1" style="cursor:pointer;font-weight: bolder;">5GHz</span>
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
		    	<input type="button" class="button_gen" onclick="gotoSiteSurvey();" value="<#btn_go#>" style="float:right;">
     			<img style="margin-top:5px; *margin-top:-10px; visibility:hidden;" src="/images/New_ui/networkmap/linetwo2.png">
		    </td>
		  </tr>
		</table>

		<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" class="table1px" id="WLnetworkmap">
  		<tr>
    			<td style="padding:5px 10px 0px 10px; ">
  	  			<p class="formfonttitle_nwm" ><#Wireless_name#>(SSID)</p>
      			<input style="*margin-top:-7px; width:260px;" id="wl_ssid" type="text" name="wl_ssid" onfocus="parent.showHelpofDrSurf(0, 1);" value="<% nvram_get("wl_ssid"); %>" maxlength="32" size="22" class="input_25_table" >
      			<img style="margin-top:5px; *margin-top:-10px;"src="/images/New_ui/networkmap/linetwo2.png">
    			</td>
  		</tr>  
  		<tr>
    			<td style="padding:5px 10px 0px 10px; *padding:1px 10px 0px 10px;">
    					<p class="formfonttitle_nwm" ><#WLANConfig11b_AuthenticationMethod_itemname#></p>
				  		<select style="*margin-top:-7px;" name="wl_auth_mode_x" class="input_option" onChange="return change_authmode(this, 'WLANConfig11b', 'wl_auth_mode_x');">
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
	  			<select style="*margin-top:-7px;" name="wl_wep_x" id="wl_wep_x" class="input_option" onfocus="parent.showHelpofDrSurf(0, 9);" onchange="change_wlweptype(this);">
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
      				<select style="*margin-top:-7px;" name="wl_key" class="input_option" onfocus="parent.showHelpofDrSurf(0, 10);" onchange="show_key();">
        				<option value="1" <% nvram_match("wl_key", "1", "selected"); %>>Key1</option>
        				<option value="2" <% nvram_match("wl_key", "2", "selected"); %>>Key2</option>
        				<option value="3" <% nvram_match("wl_key", "3", "selected"); %>>Key3</option>
        				<option value="4" <% nvram_match("wl_key", "4", "selected"); %>>Key4</option>
      			</select>      			
	  			<img style="margin-top:5px; *margin-top:-10px;" src="/images/New_ui/networkmap/linetwo2.png">
    			</td>
  		</tr>
  		<tr id='asus_wep_key'>
    			<td style="padding:5px 10px 0px 10px; ">
	    			<p class="formfonttitle_nwm" ><#WLANConfig11b_WEPKey_itemname#>
						</p>
						<span id="sta_asuskey1_span">
							<input id="wl_asuskey1" name="wl_asuskey1" style="width:260px;*margin-top:-7px;" type="password" autocapitalization="off" onBlur="switchType(false);" onFocus="switchType(true);switchType_IE(this);show_wepkey_help();" onKeyUp="return change_wlkey(this, 'WLANConfig11b');" value="" maxlength="27" class="input_25_table">
							<input id="wl_asuskey1_text" name="wl_asuskey1_text" style="width:260px;*margin-top:-7px;display:none;" type="text" autocapitalization="off"  onClick="clean_input(this);" onBlur="switchType_IE(this);parent.showHelpofDrSurf(0, 7);" value="" maxlength="27" class="input_25_table"/>
						</span>
      			<img style="margin-top:5px; *margin-top:-10px;"src="/images/New_ui/networkmap/linetwo2.png">
    			</td>
  		</tr>
  		<tr id='wl_crypto' style='display:none;'>
			<td style="padding:5px 10px 0px 10px; *padding:1px 10px 0px 10px;">
	  			<p class="formfonttitle_nwm" ><#WLANConfig11b_WPAType_itemname#></p>
	  			<select style="*margin-top:-7px;" name="wl_crypto" class="input_option" onfocus="parent.showHelpofDrSurf(0, 6);" onchange="wl_auth_mode_change(0);">
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
      			<span id="sta_wpa_psk_span">
							<input id="wl_wpa_psk" name="wl_wpa_psk" style="width:260px;*margin-top:-7px;" type="password" autocapitalization="off" onBlur="switchType(false);" onFocus="switchType(true);switchType_IE(this);parent.showHelpofDrSurf(0, 7);" value="" maxlength="64" class="input_25_table"/>
							<input id="wl_wpa_psk_text" name="wl_wpa_psk_text" style="width:260px;*margin-top:-7px;display:none;" type="text" autocapitalization="off" onClick="clean_input(this);" onBlur="switchType_IE(this);parent.showHelpofDrSurf(0, 7);" value="" maxlength="64" class="input_25_table"/>
						</span>
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
							 },
							 {
								switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
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
							 },
							 {
								switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
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
    			<td style="border-bottom:3px #15191b solid;padding:0px 10px 5px 10px;">
    				<input id="applySecurity" type="button" class="button_gen" value="<#CTL_apply#>" onclick="submitForm();" style="margin-left:90px;">
    			</td>
  		</tr>
  		<tr>
    			<td style="padding:5px 10px 0px 10px;">
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
    				<p style="padding-left:10px; margin-top:3px; *margin-top:-5px; padding-bottom:3px; margin-right:10px; background-color:#444f53; line-height:20px;" id="MAC_wl2"></p>
    				<img style="margin-top:5px; *margin-top:-10px;" src="/images/New_ui/networkmap/linetwo2.png">
    			</td>
  		</tr>     
  		<tr id="macaddr_wl5">
    			<td style="padding:5px 10px 0px 10px;">
    				<p class="formfonttitle_nwm" >Wireless 5GHz <#MAC_Address#></p>
    				<p style="padding-left:10px; margin-top:3px; *margin-top:-5px; padding-bottom:3px; margin-right:10px; background-color:#444f53; line-height:20px;" id="MAC_wl5"></p>
    				<img style="margin-top:5px; *margin-top:-10px;" src="/images/New_ui/networkmap/linetwo2.png">
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
