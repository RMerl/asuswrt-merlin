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
<title><#Web_Title#> - Facebook Wi-Fi<!--untranslated--></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script type="text/javascript" src="state.js"></script>
<script type="text/javascript" src="general.js"></script>
<script type="text/javascript" src="popup.js"></script>
<script type="text/javascript" src="help.js"></script>
<script type="text/javascript" src="validator.js"></script>
<script type="text/javascript" src="js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="/form.js"></script>
<style>
.fbwifi_page_setting {
	width: 500px;
	position:absolute;
	background-color: #293438;
	border-radius: 10px;
	box-shadow: 3px 3px 10px #000;
	z-index:10;
	margin-left:300px;
	border-radius:10px;
	display: none;
	font-family: Arial, Helvetica, sans-serif;
	font-size: 16px;
	z-index: 100;
}
.fbwifi_page_setting_title {
	background-color: #232E32;
	height: 20px;
	padding: 10px 25px;
	border-radius: 10px 10px 0 0;
	border-bottom-width: 1px;
	border-bottom-color: #3D474B;
	border-bottom-style: solid;
}
.fbwifi_page_setting_text {
	background-color: #293438;
	padding: 10px 50px;
	height: 80px;
}
.fbwifi_page_setting_link {
	color: #5AD;
	text-decoration: underline;
	cursor: pointer;
	font-weight: bolder;
}
.fbwifi_page_setting_bottom {
	background-color: #232E32;
	height: 30px;
	padding: 15px 0;
	border-radius: 0 0 10px 10px;
	border-bottom-width: 1px;
	border-bottom-color: #3D474B;
	border-bottom-style: solid;
	text-align: center;
}
.full_screen_bg {
	position:fixed;
	width: 100%;
	height: 100%;
	left: 0;
    top: 0;
	z-index: 100;
	display: none;
	background-color: #444F53;
    filter: alpha(opacity=94);
    opacity: .94;
}
</style>
<script>
var wanstate = -1;
var wansbstate = -1;
var wanauxstate = -1;
var curState = 0;
var fbwifi_enable = '<% nvram_get("fbwifi_enable"); %>';
var fbwifi_2g_index = '<% nvram_get("fbwifi_2g"); %>';
var fbwifi_5g_index = '<% nvram_get("fbwifi_5g"); %>';
var fbwifi_5g_2_index = '<% nvram_get("fbwifi_5g_2"); %>';
if(fbwifi_5g_2_index == "")
	fbwifi_5g_2_index = "off";
var fbwifi_id = '<% nvram_get("fbwifi_id"); %>';
window.onresize = function() {
	if(document.getElementById("fbwifi_page_setting").style.display == "block") {
		cal_panel_block("fbwifi_page_setting", 0.35);
	}
} 
function initial(){
	show_menu();

	//find empty guestnetwork
	var find_empty_wl_idx = function(_gn_array, _idx) {
		var empty_idx = "off";
		var wl_vifnames = '<% nvram_get("wl_vifnames"); %>';
		var gn_count = wl_vifnames.split(" ").length;
		var gn_interface = "";
		for(var wl_idx = 0; wl_idx < gn_count; wl_idx += 1) {
			if(_gn_array[wl_idx][0] == "0") {
				empty_idx = "wl" + _idx + "." + (wl_idx + 1);
				break;
			}
		}
		return empty_idx;
	};
	
	if(wl_info.band2g_support && fbwifi_2g_index == "off") {
		fbwifi_2g_index = find_empty_wl_idx(gn_array_2g, "0");
	}
	if(wl_info.band5g_support && fbwifi_5g_index == "off") {
		fbwifi_5g_index = find_empty_wl_idx(gn_array_5g, "1");
	}
	if(wl_info.band5g_2_support && fbwifi_5g_2_index == "off") {
		fbwifi_5g_2_index = find_empty_wl_idx(gn_array_5g_2, "2");
	}

	if(fbwifi_enable == "on") {
		fbwifiShowAndHide(1);
	}
	else {
		fbwifiShowAndHide(0);
	}

	var show_fbwifi_page_flag = cookie.get("fbwifi_page_flag");
	if(show_fbwifi_page_flag == null && fbwifi_enable == "on") {
		show_fbwifi_page_setting();
		$("#full_screen_bg").fadeIn(300);
		configure_fbwifi();
	}
}
function show_fbwifi_page_setting() {
	cal_panel_block("fbwifi_page_setting", 0.35);
	$('#fbwifi_page_setting').fadeIn();
	cookie.set("fbwifi_page_flag", true, 30);
}
function close_fbwifi_page_setting() {
	$('#fbwifi_page_setting').fadeOut(100);
	$("#full_screen_bg").fadeOut(300);
}
function authentication_method_change_fbwifi(obj) {

	var mode = obj.value;
	/* enable/disable crypto algorithm */
	if(mode == "wpa2" || mode == "wpawpa2" || mode == "psk2" || mode == "pskpsk2") {
		inputCtrl(document.form.fbwifi_crypto,  1);
	}
	else {
		inputCtrl(document.form.fbwifi_crypto,  0);
	}

	/* enable/disable psk passphrase */
	if(mode === "psk2" || mode === "pskpsk2") {
		inputCtrl(document.form.fbwifi_wpa_psk, 1);
	}
	else {
		inputCtrl(document.form.fbwifi_wpa_psk, 0);
	}

	/* update wl_crypto */
	var algos, cur;
	if(mode !== "open") {
		/* Save current crypto algorithm */
		for(var i = 0; i < document.form.fbwifi_crypto.length; i += 1) {
			if(document.form.fbwifi_crypto[i].selected){
				cur = document.form.fbwifi_crypto[i].value;
				break;
			}
		}

		if(mode == "wpa2" ||  mode == "psk2") {
			algos = new Array("AES");
		}
		else {
			algos = new Array("AES", "TKIP+AES");
		}

		free_options(document.form.fbwifi_crypto);
		for(var j = 0; j < algos.length; j += 1) {
			document.form.fbwifi_crypto[j] = new Option(algos[j], algos[j].toLowerCase());
			document.form.fbwifi_crypto[j].value = algos[j].toLowerCase();	
			if(algos[j].toLowerCase() == cur) {
				document.form.fbwifi_crypto[j].selected = true;
			}
		}
	}
}
function fbwifiShowAndHide(flag) {
	if(flag == 0) {
		fbwifi_enable = "off";
		inputCtrl(document.form.fbwifi_ssid, 0);
		inputCtrl(document.form.fbwifi_unit, 0);
		inputCtrl(document.form.fbwifi_auth_mode_x, 0);
		inputCtrl(document.form.fbwifi_crypto, 0);
		inputCtrl(document.form.fbwifi_wpa_psk, 0);
		document.getElementById("trFBPageSetting").style.display = "none";
	}
	else {
		fbwifi_enable = "on";
		var fbwifi_ssid = document.form.fbwifi_ssid.value;
		var fbwifi_auth_mode_x = document.form.fbwifi_auth_mode_x.value;
		var fbwifi_crypto = document.form.fbwifi_crypto.value;
		var fbwifi_wpa_psk = document.form.fbwifi_wpa_psk.value;

		//setting form value
		document.form.fbwifi_ssid.value = fbwifi_ssid;
		free_options(document.form.fbwifi_unit);

		var add_band_item = function(_band) {
			var option = document.createElement("option");
			option.text = _band;
			option.value = _band;
			document.form.fbwifi_unit.add(option);
		};
		if(wl_info.band2g_support) {
			add_band_item("2.4GHz");
		}
		if(wl_info.band5g_support) {
			add_band_item("5GHz");
		}
		if(wl_info.band5g_2_support) {
			add_band_item("5GHz-2");
		}
		if(wl_info.band5g_support && !wl_info.band5g_2_support) {
			add_band_item("2.4GHz+5GHz");
		}
		else if(wl_info.band5g_support && wl_info.band5g_2_support) {
			add_band_item("2.4GHz+5GHz+5GHz-2");
		}

		var remove_band_item = function(_band) {
			var selectobject = document.form.fbwifi_unit;
			for (var i = 0; i < selectobject.length; i += 1) {
				if (selectobject.options[i].value == _band)
					selectobject.remove(i);
			}
		};
		
		var fbwifi_hint_array = [
			"<#Facebook_WiFi_unit_hint0#>",
			"<#Facebook_WiFi_unit_hint1#>",
			"<#Facebook_WiFi_unit_hint2#>"
		];

		document.getElementById("fbwifi_unit_hint").innerHTML = "";
		if(wl_info.band2g_support && wl_info.band5g_support && wl_info.band5g_2_support) {
			if(fbwifi_2g_index == "off") {
				remove_band_item("2.4GHz");
				remove_band_item("2.4GHz+5GHz+5GHz-2");
				document.getElementById("fbwifi_unit_hint").innerHTML += fbwifi_hint_array[0];
			}
			if(fbwifi_5g_index == "off") {
				remove_band_item("5GHz");
				remove_band_item("2.4GHz+5GHz+5GHz-2");
				document.getElementById("fbwifi_unit_hint").innerHTML += fbwifi_hint_array[1];
			}
			if(fbwifi_5g_2_index == "off") {
				remove_band_item("5GHz-2");
				remove_band_item("2.4GHz+5GHz+5GHz-2");
				document.getElementById("fbwifi_unit_hint").innerHTML += fbwifi_hint_array[2];
			}
		}
		else if(wl_info.band2g_support && wl_info.band5g_support) {
			if(fbwifi_2g_index == "off") {
				remove_band_item("2.4GHz");
				remove_band_item("2.4GHz+5GHz");
				document.getElementById("fbwifi_unit_hint").innerHTML = fbwifi_hint_array[0];
			}
			if(fbwifi_5g_index == "off") {
				remove_band_item("5GHz");
				remove_band_item("2.4GHz+5GHz");
				document.getElementById("fbwifi_unit_hint").innerHTML = fbwifi_hint_array[1];
			}
		}
		else {
			if(fbwifi_2g_index == "off") {
				remove_band_item("2.4GHz");
				document.getElementById("fbwifi_unit_hint").innerHTML = fbwifi_hint_array[0];
			}
		}

		var band_length = document.form.fbwifi_unit.length;
		if(band_length > 0) {
			document.form.fbwifi_unit[0].selected = "selected";
		}

		var fbwifi_unit = "";
		var fbwifi_unit_subunit_2g = document.form.fbwifi_2g.value;
		var fbwifi_unit_subunit_5g = document.form.fbwifi_5g.value;
		var fbwifi_unit_subunit_5g_2 = document.form.fbwifi_5g_2.value;

		if(wl_info.band2g_support && wl_info.band5g_support && wl_info.band5g_2_support) {
			if(fbwifi_unit_subunit_2g != "off" && fbwifi_unit_subunit_5g != "off" && fbwifi_unit_subunit_5g_2 != "off") {
				fbwifi_unit = "2.4GHz+5GHz+5GHz-2";
				document.form.fbwifi_unit.value = fbwifi_unit;
			}
			else if(fbwifi_unit_subunit_2g != "off" && fbwifi_unit_subunit_5g == "off" && fbwifi_unit_subunit_5g_2 == "off") {
				fbwifi_unit = "2.4GHz";
				document.form.fbwifi_unit.value = fbwifi_unit;
			}
			else if(fbwifi_unit_subunit_2g == "off" && fbwifi_unit_subunit_5g != "off" && fbwifi_unit_subunit_5g_2 == "off") {
				fbwifi_unit = "5GHz";
				document.form.fbwifi_unit.value = fbwifi_unit;
			}
			else if(fbwifi_unit_subunit_2g == "off" && fbwifi_unit_subunit_5g == "off" && fbwifi_unit_subunit_5g_2 != "off") {
				fbwifi_unit = "5GHz-2";
				document.form.fbwifi_unit.value = fbwifi_unit;
			}
		}
		else if(wl_info.band2g_support && wl_info.band5g_support) {
			if(fbwifi_unit_subunit_2g != "off" && fbwifi_unit_subunit_5g != "off") {
				fbwifi_unit = "2.4GHz+5GHz";
				document.form.fbwifi_unit.value = fbwifi_unit;
			}
			else if(fbwifi_unit_subunit_2g != "off" && fbwifi_unit_subunit_5g == "off") {
				fbwifi_unit = "2.4GHz";
				document.form.fbwifi_unit.value = fbwifi_unit;
			}
			else if(fbwifi_unit_subunit_2g == "off" && fbwifi_unit_subunit_5g != "off") {
				fbwifi_unit = "5GHz";
				document.form.fbwifi_unit.value = fbwifi_unit;
			}
		}

		
		document.form.fbwifi_auth_mode_x.value = fbwifi_auth_mode_x;
		document.form.fbwifi_crypto.value = fbwifi_crypto;
		document.form.fbwifi_wpa_psk.value = fbwifi_wpa_psk;

		//handle form display
		inputCtrl(document.form.fbwifi_ssid, 1);
		inputCtrl(document.form.fbwifi_unit, 1);
		inputCtrl(document.form.fbwifi_auth_mode_x, 1);
		authentication_method_change_fbwifi(document.form.fbwifi_auth_mode_x);
		if(document.form.fbwifi_enable.value == "off") {
			document.getElementById("trFBPageSetting").style.display = "none";
		}
		else {
			document.getElementById("trFBPageSetting").style.display = "";
			document.getElementById("divFBSetting").style.display = "none";
			document.getElementById("divGettingID").style.display = "none";
			document.getElementById("divWanState").style.display = "none";
			if(fbwifi_id == "off") {
				//check wan state
				update_wan_status();
			}
			else {
				document.getElementById("divFBSetting").style.display = "";
			}
		}		
	}
}

function configure_fbwifi() {
	var url = "https://www.facebook.com/wifiauth/config?gw_id=" + fbwifi_id;
	window.open(url, '_blank', 'toolbar=no, location=no, menubar=no, top=0, left=0, width=700, height=600');
}

function checkFBWiFiID() {
	$.ajax({
		url: '/ajax_fbwifi.asp',
		dataType: 'script', 
		error: function(xhr) {
			checkFBWiFiID();
		},
		success: function(response) {
			if(ajax_fbwifi_id == "off") {
				document.getElementById("divGettingID").style.display = "";
				setTimeout("checkFBWiFiID();", 500);
			}
			else {
				fbwifi_id = ajax_fbwifi_id;
				document.getElementById("divFBSetting").style.display = "";
				document.getElementById("divGettingID").style.display = "none";
			}
		}
	});
}

function update_wan_status() {
	$.ajax({
		url: '/status.asp',
		dataType: 'script', 

		error: function(xhr) {
			setTimeout("update_wan_status();", 500);
		},
		success: function(response) {
			//wan connect
			if(wanstate == 2 && wansbstate == 0 && wanauxstate == 0) {
				document.getElementById("divWanState").style.display = "none";
				//check ID state
				checkFBWiFiID();
			}
			else {
				document.getElementById("divWanState").style.display = "";
				setTimeout("update_wan_status();", 500);
			}
		}
	});
}
function applyRuleFBWiFi(){
	var auth_mode = document.form.fbwifi_auth_mode_x.value;

	var validForm = function () {	
		if(!validator.stringSSID(document.form.fbwifi_ssid))	
			return false;
		
		if(auth_mode == "psk2" || auth_mode == "pskpsk2") {
			if(!validator.psk(document.form.fbwifi_wpa_psk, ""))
				return false;
		}
		return true;
	};

	if(fbwifi_enable == "on") {
		if(validForm()) {
			showLoading();

			document.form.fbwifi_2g.value = "off";
			if(wl_info.band5g_support)
				document.form.fbwifi_5g.value = "off";
			if(wl_info.band5g_2_support)
				document.form.fbwifi_5g_2.value = "off";
			var fbwifi_unit = document.form.fbwifi_unit.value;
			switch(fbwifi_unit) {
				case "2.4GHz" :
					document.form.fbwifi_2g.value = fbwifi_2g_index;
					break;
				case "5GHz" :
					document.form.fbwifi_5g.value = fbwifi_5g_index;
					break;
				case "5GHz-2" :
					document.form.fbwifi_5g_2.value = fbwifi_5g_2_index;
					break;
				case "2.4GHz+5GHz" :
					document.form.fbwifi_2g.value = fbwifi_2g_index;
					document.form.fbwifi_5g.value = fbwifi_5g_index;
					break;
				case "2.4GHz+5GHz+5GHz-2" :
					document.form.fbwifi_2g.value = fbwifi_2g_index;
					document.form.fbwifi_5g.value = fbwifi_5g_index;
					document.form.fbwifi_5g_2.value = fbwifi_5g_2_index;
					break;

			}

			document.form.fbwifi_enable.value = fbwifi_enable;
			document.form.fbwifi_ssid.value = document.form.fbwifi_ssid.value;
			document.form.fbwifi_auth_mode_x.value = document.form.fbwifi_auth_mode_x.value;
			document.form.fbwifi_crypto.value = document.form.fbwifi_crypto.value; 
			document.form.fbwifi_wpa_psk.value = document.form.fbwifi_wpa_psk.value;
			document.form.action_script.value += ";start_fbwifi";
			document.form.submit();
		}
	}
	else {
		cookie.unset("fbwifi_page_flag");
		showLoading();
		document.form.action_script.value += ";stop_fbwifi";
		document.form.fbwifi_enable.value = fbwifi_enable;
		document.form.fbwifi_2g.value = "off";
		if(wl_info.band5g_support)
			document.form.fbwifi_5g.value = "off";
		if(wl_info.band5g_2_support)
			document.form.fbwifi_5g_2.value = "off";
		document.form.submit();
	}		
}
function agreement_cancel(){
	$("#agreement_panel").fadeOut(300);
	$('#iphone_switch').animate({backgroundPosition: -37}, "slow", function() {});
	document.getElementById("hiddenMask").style.visibility = "hidden";
	curState = 0;
}
function agreement_confirm() {
	$("#agreement_panel").fadeOut(300);
	document.getElementById("hiddenMask").style.visibility = "hidden";
	fbwifiShowAndHide(1);
}
</script>

</head>

<body onload="initial();" onunLoad="return unload_body();">

<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="agreement_panel" class="panel_folder" style="margin-top: -100px;">
	<div class="machineName" style="font-family:Microsoft JhengHei;font-size:12pt;font-weight:bolder; margin-top:25px;margin-left:30px;height:35px;"><#Facebook_WiFi_disclaim_title#></div>
	<div class="folder_tree">
		<#Facebook_WiFi_disclaim#>
		<br><br>
		1. <#Facebook_WiFi_disclaim1#>
		<br><br>
		2. <#Facebook_WiFi_disclaim2#>
		<br><br>
		3. <#Facebook_WiFi_disclaim3#>
		<br><br>
		4. <#Facebook_WiFi_disclaim4#>
	</div>
	<div style="background-image:url(images/Tree/bg_02.png);background-repeat:no-repeat;height:90px;">
		<input class="button_gen_long" type="button" style="margin-left:20%;margin-top:18px;" onclick="agreement_cancel();" value="<#CTL_Disagree#>">
		<input class="button_gen_long" type="button"  onclick="agreement_confirm();" value="<#CTL_Agree#>">
	</div>
</div>
<div id="hiddenMask" class="popup_bg" style="z-index:999;">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
	</table>
	<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="form" action="/start_apply2.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Guest_network_fbwifi.asp">
<input type="hidden" name="next_page" value="Guest_network_fbwifi.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_script" value="set_fbwifi_profile;restart_wireless">
<input type="hidden" name="action_wait" value="15">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="fbwifi_enable" value='<% nvram_get("fbwifi_enable"); %>'>
<input type="hidden" name="fbwifi_2g" value="<% nvram_get("fbwifi_2g"); %>">
<input type="hidden" name="fbwifi_5g" value="<% nvram_get("fbwifi_5g"); %>">
<input type="hidden" name="fbwifi_5g_2" value="<% nvram_get("fbwifi_5g_2"); %>">
<div id="full_screen_bg" class="full_screen_bg" onselectstart="return false;"></div>
<div id="fbwifi_page_setting" class="fbwifi_page_setting">
	<div class="fbwifi_page_setting_title">
		<#Facebook_WiFi_page#>
	</div>
	<div class="fbwifi_page_setting_text">
		<#Facebook_WiFi_page_desc#>
		<span class="fbwifi_page_setting_link" onclick="configure_fbwifi();"><#Facebook_page#></span>
	</div>
	<div class="fbwifi_page_setting_bottom">
		<input class="button_gen" type="button" onclick="close_fbwifi_page_setting();" value="<#CTL_close#>">
	</div>
</div>
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
			<div id="captive_portal_setting" class="captive_portal_setting_bg"></div>
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td align="left" valign="top">
						<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
							<tr>
								<td bgcolor="#4D595D" valign="top">
									
									<table width="98%" align="center" cellpadding="4" cellspacing="0" class="gninfo_head_table" id="gninfo_table_FBWiFi">
										<tr id="FBWiFi_title">
											<td align="left" style="color:#5AD; font-size:16px; border-bottom:1px dashed #AAA;" colspan="2">
												<span>Facebook Wi-Fi<!--untranslated--></span>
												<span id="FBWiFi_radio_hint" style="font-size: 14px;display:none;color:#FC0;margin-left:17px;">* <#GuestNetwork_Radio_Status#><a style="font-family:Lucida Console;color:#FC0;text-decoration:underline;cursor:pointer;" onclick="_change_wl_unit_status(0);"><#btn_go#></a>
												</span>
											</td>
										</tr>
										<tr>
											<td width="50%">
												<span style="line-height: 20px;" ><#Facebook_WiFi_desc#></span>
											</td>
											<td width="50%">
												<ul style="margin: 5px;line-height: 20px;">
													<li>
														<a style="font-weight: bolder;text-decoration: underline;" href="https://www.facebook.com/business/facebook-wifi" target="_blank"><#Facebook_WiFi_FAQ1#></a>
													</li>
													<li>
														<a style="font-weight: bolder;text-decoration: underline;" href="https://www.facebook.com/business/a/facebook-wifi/businessowner" target="_blank"><#Facebook_WiFi_FAQ2#></a>
													</li>
													<li>
														<a style="font-weight: bolder;text-decoration: underline;" href="https://www.facebook.com/help/364458366957655/" target="_blank"><#Facebook_WiFi_FAQ3#></a>
													</li>
													<li>
														<a style="font-weight: bolder;text-decoration: underline;" href="https://www.facebook.com/help/126760650808045/" target="_blank"><#Facebook_WiFi_FAQ4#></a>
													</li>
												</ul>
											</td>
										</tr>
										<tr>
											<td colspan="2">
												<div style="color:#FC0;">Please notice that current Facebook Wi-Fi only blocks the access from http:// connections before checking in.<!--untranslated--></div>
											</td>
										</tr>
									</table>
									<table id="tbFBWiFiSetting" width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
										<tr>
											<th><#Facebook_WiFi_enable#></th>
											<td>
												<div align="center" class="left" style="float:left; cursor:pointer;" id="radio_fbwifi_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden;">
												<script type="text/javascript">
													var FBWiFiEnable = 0;
													curState = 0;
													if(document.form.fbwifi_enable.value == "on") {
														FBWiFiEnable = 1;
														curState = 1;
													}
													$('#radio_fbwifi_enable').iphoneSwitch(FBWiFiEnable,
														function() {
															var fbwifi_2g_flag = (fbwifi_2g_index == "off") ? true : false;
															var fbwifi_5g_flag = (fbwifi_5g_index == "off") ? true : false;
															var fbwifi_5g_2_flag = (fbwifi_5g_2_index == "off") ? true : false;
															var fbwifi_flag = false;
															if(wl_info.band5g_2_support) {
																if(fbwifi_2g_flag && fbwifi_5g_flag && fbwifi_5g_2_flag) {
																	fbwifi_flag = true;
																}
															}
															else {
																if(fbwifi_2g_flag && fbwifi_5g_flag) {
																	fbwifi_flag = true;
																}
															}
															if(fbwifi_flag) {
																alert("<#Facebook_WiFi_unit_hint_all#>");
																$('#iphone_switch').animate({backgroundPosition: -37}, "slow", function() {});
																return false;
															}

															if(document.form.fbwifi_enable.value == "off") {
																$("#agreement_panel").fadeIn(300);	
																dr_advise();
																cal_panel_block("agreement_panel", 0.25);
																curState = 1;
															}
															else {
																curState = 1
																fbwifiShowAndHide(1);
															}
														},
														function() {
															fbwifiShowAndHide(0);	
															curState = 0;	
														});
												</script>			
												</div>
											</td>
										</tr>
										<tr>
											<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 1);"><#QIS_finish_wireless_item1#></a></th>
											<td>
												<input type="text" maxlength="32" class="input_32_table" name="fbwifi_ssid" value="<% nvram_get("fbwifi_ssid"); %>" onkeypress="return is_string(this, event)">
											</td>
										</tr>
										<tr>
											<th>Frequency</th>
											<td>
												<select name="fbwifi_unit" class="input_option"></select>
												<div id="fbwifi_unit_hint" style="color:#FFCC00;"></div>
											</td>
										</tr>
										<tr>
											<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 5);"><#WLANConfig11b_AuthenticationMethod_itemname#></a></th>
											<td>
												<select name="fbwifi_auth_mode_x" class="input_option" onChange="authentication_method_change_fbwifi(this);">
													<option value="open" <% nvram_match("fbwifi_auth_mode_x", "open", "selected"); %>>Open System</option>
													<option value="psk2" <% nvram_match("fbwifi_auth_mode_x", "psk2", "selected"); %>>WPA2-Personal</option>
													<option value="pskpsk2" <% nvram_match("fbwifi_auth_mode_x", "pskpsk2", "selected"); %>>WPA-Auto-Personal</option>
													<option value="wpa2" <% nvram_match("fbwifi_auth_mode_x", "wpa2", "selected"); %>>WPA2-Enterprise</option>
													<option value="wpawpa2" <% nvram_match("fbwifi_auth_mode_x", "wpawpa2", "selected"); %>>WPA-Auto-Enterprise</option>
												</select>
											</td>
										</tr>
										<tr>
											<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 6);"><#WLANConfig11b_WPAType_itemname#></a></th>
											<td>		
												<select name="fbwifi_crypto" class="input_option">
													<option value="aes">AES</option>
													<option value="tkip+aes">TKIP+AES</option>
												</select>
											</td>
										</tr>	  
										<tr>
											<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 7);"><#WLANConfig11b_x_PSKKey_itemname#></a></th>
											<td >
												<input type="text" name="fbwifi_wpa_psk" maxlength="64" class="input_32_table" value="<% nvram_get("fbwifi_wpa_psk"); %>">
											</td>
										</tr>
										<tr id="trFBPageSetting">
											<th><#Facebook_WiFi_page#></th>
											<td>
												<div id="divFBSetting">
													<div>
														<input type="button" name="btFBSetting" class="button_gen" value="Setting" onclick="configure_fbwifi();">
													</div>
													<div style="margin-top:-30px;margin-bottom:9px;margin-left:130px;font-size:16px;color:#FFCC00;">
														<#Facebook_WiFi_page_connection#>
													</div>
												</div>
												<div id="divGettingID">
													<img src="images/InternetScan.gif" />
													<div style="margin-top:-20px;margin-left:30px;font-size:16px;color:#FFCC00;">
														<#Facebook_WiFi_connecting#>
													</div>
												</div>
												<div id="divWanState" style="font-size:16px;color:#FFCC00;">
														<#Facebook_WiFi_disconnected#>
												</div>
												
											</td>
										</tr>
									</table>
									<div style="margin-top:10px;text-align:center">
										<input type="button" name="btFBWiFiApply" class="button_gen" value="<#CTL_apply#>" onclick="applyRuleFBWiFi();">
									</div>
								</td>
							</tr>
							</tbody>
						</table>
					</td>
				</tr>
			</table>
		</td>
		<td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>
<!--===================================End of Main Content===========================================-->
</form>
<div id="footer"></div>
</body>
</html>
