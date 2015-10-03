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
<title><#Web_Title#> - <#menu5_1_6#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/calendar/jquery-ui.js"></script> 
<style>
.ui-slider {
	position: relative;
	text-align: left;
}
.ui-slider .ui-slider-handle {
	position: absolute;
	width: 12px;
	height: 12px;
}
.ui-slider .ui-slider-range {
	position: absolute;
}
.ui-slider-horizontal {
	height: 6px;
}

.ui-widget-content {
	border: 2px solid #000;
	background-color:#000;
}
.ui-state-default,
.ui-widget-content .ui-state-default,
.ui-widget-header .ui-state-default {
	border: 1px solid ;
	background: #e6e6e6;
	margin-top:-4px;
	margin-left:-6px;
}

/* Corner radius */
.ui-corner-all,
.ui-corner-top,
.ui-corner-left,
.ui-corner-tl {
	border-top-left-radius: 4px;
}
.ui-corner-all,
.ui-corner-top,
.ui-corner-right,
.ui-corner-tr {
	border-top-right-radius: 4px;
}
.ui-corner-all,
.ui-corner-bottom,
.ui-corner-left,
.ui-corner-bl {
	border-bottom-left-radius: 4px;
}
.ui-corner-all,
.ui-corner-bottom,
.ui-corner-right,
.ui-corner-br {
	border-bottom-right-radius: 4px;
}

.ui-slider-horizontal .ui-slider-range {
	top: 0;
	height: 100%;
}

#slider .ui-slider-range {
	background: #93E7FF; 
	border-top-left-radius: 3px;
	border-top-right-radius: 1px;
	border-bottom-left-radius: 3px;
	border-bottom-right-radius: 1px;
}
#slider .ui-slider-handle { border-color: #93E7FF; }
.parental_th{
	color:white;
	background:#2F3A3E;
	cursor: pointer;
	width:160px;
	height:22px;
	border-bottom:solid 1px black;
	border-right:solid 1px black;
} 
.parental_th:hover{
	background:rgb(94, 116, 124);
	cursor: pointer;
}

.checked{
	background-color:#9CB2BA;
	width:82px;
	border-bottom:solid 1px black;
	border-right:solid 1px black;
}

.disabled{
	width:82px;
	border-bottom:solid 1px black;
	border-right:solid 1px black;
	background-color:#475A5F;
}

  #selectable .ui-selecting { background: #FECA40; }
  #selectable .ui-selected { background: #F39814; color: white; }
  #selectable .ui-unselected { background: gray; color: green; }
  #selectable .ui-unselecting { background: green; color: black; }
  #selectable { border-spacing:0px; margin-left:0px;margin-top:0px; padding: 0px; width:100%;}
  #selectable td { height: 22px; }

</style>
<script>
//Get boot loader version and convert type form string to Integer
var bl_version = '<% nvram_get("bl_version"); %>';
var bl_version_array = bl_version.split(".");
var bootLoader_ver = "";
for(i=0;i<bl_version_array.length;i++){
	bootLoader_ver +=bl_version_array[i];
}

<% wl_get_parameter(); %>
var mcast_rates = [
	["HTMIX 6.5/15", "14", 0, 1, 1],
	["HTMIX 13/30",	 "15", 0, 1, 1],
	["HTMIX 19.5/45","16", 0, 1, 1],
	["HTMIX 13/30",	 "17", 0, 1, 2],
	["HTMIX 26/60",	 "18", 0, 1, 2],
	["HTMIX 130/144","13", 0, 1, 2],
	["OFDM 6",	 "4",  0, 0, 1],
	["OFDM 9",	 "5",  0, 0, 1],
	["OFDM 12",	 "7",  0, 0, 1],
	["OFDM 18",	 "8",  0, 0, 1],
	["OFDM 24",	 "9",  0, 0, 1],
	["OFDM 36",	 "10", 0, 0, 1],
	["OFDM 48",	 "11", 0, 0, 1],
	["OFDM 54",	 "12", 0, 0, 1],
	["CCK 1",	 "1",  1, 0, 1],
	["CCK 2",	 "2",  1, 0, 1],
	["CCK 5.5",	 "3",  1, 0, 1],
	["CCK 11",	 "6",  1, 0, 1]
];

var sdk_6 = sdk_version_array[0] == "6" ? true:false
var sdk_7 = sdk_version_array[0] == "7" ? true:false
var wl_user_rssi_onload = '<% nvram_get("wl_user_rssi"); %>';
var reboot_needed_time = eval("<% get_default_reboot_time(); %>");
var orig_region = '<% nvram_get("location_code"); %>';
var array = new Array(7);
var clock_type = "";
var wifi_schedule_value = '<% nvram_get("wl_sched"); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<");
function initial(){
	show_menu();
	register_event();
	init_array(array);
	init_cookie();
	count_time();

	if(userRSSI_support)
		changeRSSI(wl_user_rssi_onload);
	else
		document.getElementById("rssiTr").style.display = "none";

	if(!band5g_support)
		document.getElementById("wl_unit_field").style.display = "none";

	regen_band(document.form.wl_unit);

	if(sw_mode == "2"){
		var _rows = document.getElementById("WAdvTable").rows;
		for(var i=0; i<_rows.length; i++){
			if(_rows[i].className.search("rept") == -1){
				_rows[i].style.display = "none";
				_rows[i].disabled = true;
			}
		}

		// enable_wme_check(document.form.wl_wme);
		return false;
	}

	document.getElementById("wl_rate").style.display = "none";

	if(Rawifi_support){
		inputCtrl(document.form.wl_noisemitigation, 0);
	}
	else if(Qcawifi_support){
		// FIXME
		document.getElementById("DLSCapable").style.display = "none";	
		document.getElementById("PktAggregate").style.display = "none";
		inputCtrl(document.form.wl_noisemitigation, 0);
	}else{
		// BRCM
		document.getElementById("DLSCapable").style.display = "none";	
		document.getElementById("PktAggregate").style.display = "none";
		
		if('<% nvram_get("wl_unit"); %>' == '1' || sdk_version_array[0] >= 6 ){	// MODELDEP: for Broadcom SDK 6.x model
			inputCtrl(document.form.wl_noisemitigation, 0);
		}
	}

	if(wifi_hw_sw_support){ //For N55U
		if(document.form.wl_HW_switch.value == "1"){
			document.form.wl_radio[0].disabled = true;
		}
	}
	
	// MODELDEP: for AC ser
	if(Rawifi_support){
		inputCtrl(document.form.wl_ampdu_mpdu, 0);
		inputCtrl(document.form.wl_ack_ratio, 0);
	}else if(Qcawifi_support){
		// FIXME
		inputCtrl(document.form.wl_ampdu_mpdu, 0);
		inputCtrl(document.form.wl_ack_ratio, 0);
	}else{
		if (sdk_6){
			// for BRCM new SDK 6.x
			inputCtrl(document.form.wl_ampdu_mpdu, 1);
			inputCtrl(document.form.wl_ack_ratio, 1);
		}else if(sdk_7){
			// for BRCM new SDK 7.x
			inputCtrl(document.form.wl_ampdu_mpdu, 1);
			inputCtrl(document.form.wl_ack_ratio, 0);
		}else{
			inputCtrl(document.form.wl_ampdu_mpdu, 0);
			inputCtrl(document.form.wl_ack_ratio, 0);
		}
	}
	
	inputCtrl(document.form.wl_turbo_qam, 0);
	inputCtrl(document.form.wl_txbf, 0);
	inputCtrl(document.form.wl_itxbf, 0);
	inputCtrl(document.form.usb_usb3, 0);
	inputCtrl(document.form.traffic_5g, 0);

	if('<% nvram_get("wl_unit"); %>' == '1' || '<% nvram_get("wl_unit"); %>' == '2'){ // 5GHz up
		if(	based_modelid == "RT-AC3200" || based_modelid == "RT-AC69U" ||
			based_modelid == "RT-AC56S" || based_modelid == "RT-AC56U" ||
			based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "DSL-AC68U" ||
			based_modelid == "RT-AC87U" || based_modelid == "EA-AC87" ||
			based_modelid == "RT-AC88U" || based_modelid == "RT-AC3100" || 
			based_modelid == "RT-AC5300" || based_modelid == "RT-AC1200G" || based_modelid == "RT-AC1200G+")
		{
			document.getElementById('wl_txbf_desc').innerHTML = "802.11ac Beamforming";
			inputCtrl(document.form.wl_txbf, 1);
			inputCtrl(document.form.wl_itxbf, 1);
		}	
				
		if( based_modelid == "RT-AC55U" || based_modelid == "RT-AC55UHP")
			inputCtrl(document.form.traffic_5g, 1);
			
		if( based_modelid == "RT-AC88U" || based_modelid == "RT-AC3100" || based_modelid == "RT-AC5300"){
			inputCtrl(document.form.wl_turbo_qam, 1);
			$("#turbo_qam_title").html("Modulation Scheme");		//untranslated string
			var desc =  ["Up to MCS 9 (802.11ac)", "Up to MCS 11 (NitroQAM/1024-QAM)"];
			var value = ["1", "2"];
			add_options_x2(document.form.wl_turbo_qam, desc, value, '<% nvram_get("wl_turbo_qam"); %>');
			$('#turbo_qam_hint').click(function(){openHint(3,33);});
		}		
	}
	else{ // 2.4GHz
		if(	based_modelid == "RT-AC3200" ||
			based_modelid == "RT-N18U" ||
			based_modelid == "RT-N65U" ||
			based_modelid == "RT-AC69U" ||
			based_modelid == "RT-AC87U" ||
			based_modelid == "RT-AC55U" || based_modelid == "RT-AC55UHP" || based_modelid == "4G-AC55U" ||
			based_modelid == "RT-AC56S" || based_modelid == "RT-AC56U" || 
			based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "DSL-AC68U" ||
			based_modelid == "RT-AC88U" || based_modelid == "RT-AC3100" || 
			based_modelid == "RT-AC5300")
		{
			inputCtrl(document.form.usb_usb3, 1);
		}	

		if(	based_modelid == "RT-AC3200" ||
			based_modelid == "RT-N18U" ||
			based_modelid == "RT-AC69U" ||
			based_modelid == "RT-AC87U" ||
			based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "DSL-AC68U" ||
			based_modelid == "RT-AC88U" || based_modelid == "RT-AC3100" || 
			based_modelid == "RT-AC5300")
		{
			if(based_modelid == "RT-N18U" && bootLoader_ver < 2000)
				inputCtrl(document.form.wl_turbo_qam, 0);
			else{
				inputCtrl(document.form.wl_turbo_qam, 1);
				if( based_modelid == "RT-AC88U" || based_modelid == "RT-AC3100" || based_modelid == "RT-AC5300"){
					$("#turbo_qam_title").html("Modulation Scheme");		//untranslated string
					var desc = ["Up to MCS 7 (802.11n)", "Up to MCS 9 (TurboQAM/256-QAM)", "Up to MCS 11 (NitroQAM/1024-QAM)"];
					var value = ["0", "1", "2"];
					add_options_x2(document.form.wl_turbo_qam, desc, value, '<% nvram_get("wl_turbo_qam"); %>');
					$('#turbo_qam_hint').click(function(){openHint(3,33);});
				}	
			}
				
			document.getElementById('wl_txbf_desc').innerHTML = "<#WLANConfig11b_x_ExpBeam#>";
			inputCtrl(document.form.wl_txbf, 1);
			inputCtrl(document.form.wl_itxbf, 1);
		}	
	}

	var mcast_rate = '<% nvram_get("wl_mrate_x"); %>';
	var mcast_unit = '<% nvram_get("wl_unit"); %>';
	var HtTxStream;
	if (mcast_unit == 1)
		HtTxStream = '<% nvram_get("wl1_HT_TxStream"); %>';
	else
		HtTxStream = '<% nvram_get("wl0_HT_TxStream"); %>';
	for (var i = 0; i < mcast_rates.length; i++) {
		if (mcast_unit == '1' && mcast_rates[i][2]) // 5Ghz && CCK
			continue;
		if (!Rawifi_support && !Qcawifi_support && mcast_rates[i][3]) // BCM && HTMIX
			continue;
		if (Rawifi_support && HtTxStream < mcast_rates[i][4]) // ralink && HtTxStream
			continue;
		add_option(document.form.wl_mrate_x, mcast_rates[i][0], mcast_rates[i][1], (mcast_rate == mcast_rates[i][1]) ? 1 : 0);
	}

	if(repeater_support || psta_support){		//with RE mode
		document.getElementById("DLSCapable").style.display = "none";	
	}	

	if(document.form.wl_nmode_x.value == "0" || document.form.wl_nmode_x.value == "1"){		//auto , n only		
		inputCtrl(document.form.wl_frag, 0);
		document.form.wl_wme.value = "on";
		inputCtrl(document.form.wl_wme, 0);
		document.form.wl_wme_no_ack.value = "off";
		inputCtrl(document.form.wl_wme_no_ack, 0);
	}
	else{
		inputCtrl(document.form.wl_frag, 1);
		inputCtrl(document.form.wl_wme, 1);
		inputCtrl(document.form.wl_wme_no_ack, 1);
	}	
		
	adjust_tx_power();	
	if(svc_ready == "0")
		document.getElementById('svc_hint_div').style.display = "";	
	
	corrected_timezone();	
	
	if(based_modelid == "RT-AC87U" && '<% nvram_get("wl_unit"); %>' == '1'){	//for RT-AC87U 5G Advanced setting
		document.getElementById("wl_mrate_select").style.display = "none";
		document.getElementById("wl_plcphdr_field").style.display = "none";
		document.getElementById("ampdu_rts_tr").style.display = "none";
		document.getElementById("rts_threshold").style.display = "none";
		document.getElementById("wl_frameburst_field").style.display = "none";
		document.getElementById("wl_wme_apsd_field").style.display = "none";
		document.getElementById("wl_ampdu_mpdu_field").style.display = "none";
		document.getElementById("wl_ack_ratio_field").style.display = "none";
		//document.getElementById('wl_80211h_tr').style.display = "";
		document.getElementById("wl_regmode_field").style.display = "none";
	}
	
	/*Airtime fairness, only for Broadcom ARM platform, except RT-AC87U 5G*/
	if(	based_modelid == "RT-N18U" ||
		based_modelid == "RT-AC56U" || based_modelid == "RT-AC56S" ||
		based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "DSL-AC68U" ||
		based_modelid == "RT-AC69U" || based_modelid == "RT-AC87U" || based_modelid == "RT-AC3200" ||
		based_modelid == "RT-AC88U" || based_modelid == "RT-AC3100" || based_modelid == "RT-AC5300" ||
		based_modelid == "RT-AC1200G+"){
		
		inputCtrl(document.form.wl_atf, 1);
		if(based_modelid == "RT-AC87U" && '<% nvram_get("wl_unit"); %>' == '1')	
			inputCtrl(document.form.wl_atf, 0);
	}
	else{
		inputCtrl(document.form.wl_atf, 0);
	}
	
	if(based_modelid != "RT-AC87U"){
		check_ampdu_rts();
	}

	if(Rawifi_support || Qcawifi_support)	//brcm : 3 ; else : 1
		document.getElementById("wl_dtim_th").onClick = function (){openHint(3, 4);}
	else
		document.getElementById("wl_dtim_th").onClick = function (){openHint(3, 11);}
	
	/*location_code Setting*/		
	if(location_list_support){
		generate_region();
		document.getElementById('region_tr').style.display = "";
	}

	control_TimeField();
}

/* MODELDEP by Territory Code */
function generate_region(){
	//var region_name = ["Asia", "Australia", "Brazil", "Canada", "China", "Europe", "Japan", "Korea", "Malaysia", "Middle East", "Russia", "Singapore", "Turkey", "Taiwan", "Ukraine", "United States"];
	//var region_value = ["APAC", "AU", "BZ", "CA", "CN", "EU", "JP", "KR", "MY", "ME", "RU", "SG", "TR", "TW", "UA", "US"];
	var region_name = ["Asia", "China", "Europe", "Korea", "Russia", "Singapore", "United States"];	//Viz mod 2015.06.15
	var region_value = ["AP", "CN", "EU", "KR", "RU", "SG", "US"]; //Viz mod 2015.06.15
	var current_region = '<% nvram_get("location_code"); %>';
	var is_CN_sku = (function(){
		if( productid !== "RT-AC87U" && productid !== "RT-AC68U" && productid !== "RT-AC66U" && productid !== "RT-N66U" && productid !== "RT-N18U" && productid != "RT-AC51U" &&
			productid !== "RT-N12+" && productid !== "RT-N12D1" && productid !== "RT-N12HP_B1" && productid !== "RT-N12HP" && productid !== "RT-AC55U" && productid !== "RT-AC1200" && productid != "RT-AC51U" &&
			productid !== "RT-AC88U" && productid !== "RT-AC5300" && productid !== "RT-AC55U"
		  )	return false;
		return ('<% nvram_get("territory_code"); %>'.search("CN") == -1) ? false : true;
	})();

	if(current_region == '')
		current_region = ttc.split("/")[0];

	if(is_CN_sku){
		region_name.push("Australia");
		region_value.push("XX");
	} 

	if(productid == "RT-AC51U"){
		var idx = region_value.getIndexByValue("US");
		region_value.splice(idx, 1);
		region_name.splice(idx, 1);
	}

	add_options_x2(document.form.location_code, region_name, region_value, current_region);
}

function adjust_tx_power(){
	var power_value_old = document.form.wl_TxPower.value;
	var power_value_new = document.form.wl_txpower.value;
	var translated_value = 0;
	
	if(!power_support){
		document.getElementById("wl_txPower_field").style.display = "none";
	}
	else{
		if(power_value_old != ""){
			translated_value = parseInt(power_value_old/80*100);
			if(translated_value >=100){
				translated_value = 100;
			}
			else if(translated_value <=1){
				translated_value = 1;			
			}

			document.getElementById('slider').children[0].style.width = translated_value + "%";
			document.getElementById('slider').children[1].style.left = translated_value + "%";
			document.form.wl_txpower.value = translated_value;
		}
		else{
			document.getElementById('slider').children[0].style.width = power_value_new + "%";
			document.getElementById('slider').children[1].style.left = power_value_new + "%";
			document.form.wl_txpower.value = power_value_new;
		}
	}
}

function changeRSSI(_switch){
	if(_switch == 0){
		document.getElementById("rssiDbm").style.display = "none";
		document.form.wl_user_rssi.value = 0;
	}
	else{
		document.getElementById("rssiDbm").style.display = "";
		if(wl_user_rssi_onload == 0)
			document.form.wl_user_rssi.value = "-70";
		else
			document.form.wl_user_rssi.value = wl_user_rssi_onload;
	}
}

function applyRule(){
	if(validForm()){
		if(wifi_hw_sw_support && !Qcawifi_support) { //For N55U
			document.form.wl_HW_switch.value = "0";
			document.form.wl_HW_switch.disabled = false;
		}
		
		if(power_support){
			document.form.wl_TxPower.value = "";	
		}		

		if(document.form.usb_usb3.disabled == false && document.form.usb_usb3.value != '<% nvram_get("usb_usb3"); %>'){
			FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
		}

		if("<% nvram_get("wl_unit"); %>" == "1" && "<% nvram_get("wl1_country_code"); %>" == "EU" && based_modelid == "RT-AC87U"){	//for EU RT-AC87U 5G Advanced setting
			if(document.form.wl1_80211h[0].selected && "<% nvram_get("wl1_chanspec"); %>" == "0")	//Interlocking set acs_dfs="0" while disabled 802.11h and wl1_chanspec="0"(Auto)
				document.form.acs_dfs.value = "0";
		}
		
		if(location_list_support){
			if(orig_region.length >= 0 && orig_region != document.form.location_code.value){
				document.form.action_script.value = "reboot";
				document.form.action_wait.value = reboot_needed_time;
			}				
		}
		
		document.form.wl_sched.value = wifi_schedule_value;	
		showLoading();
		document.form.submit();
	}
}

function validForm(){
	if(sw_mode != "2"){
		if(!validator.range(document.form.wl_frag, 256, 2346)
				|| !validator.range(document.form.wl_rts, 0, 2347)
				|| !validator.range(document.form.wl_dtim, 1, 255)
				|| !validator.range(document.form.wl_bcn, 20, 1000)
				){
			return false;
		}	
	}
		
	if(power_support && !Rawifi_support && !Qcawifi_support){
		// MODELDEP
		if(hw_ver.search("RTN12HP") != -1){
		  FormActions("start_apply.htm", "apply", "set_wltxpower;reboot", "<% get_default_reboot_time(); %>");
		}
		else if(based_modelid == "RT-AC66U" || based_modelid == "RT-N66U" || based_modelid == "RT-N18U"){
			FormActions("start_apply.htm", "apply", "set_wltxpower;restart_wireless", "15");
		}	
	}

	if(userRSSI_support){
		if(document.form.wl_user_rssi.value != 0){
			if(!validator.range(document.form.wl_user_rssi, -90, -70)){
				document.form.wl_user_rssi.focus();
				return false;			
			}
		}
	}

	return true;
}

function done_validating(action){
	refreshpage();
}

function disableAdvFn(row){
	for(var i=row; i>=3; i--){
		document.getElementById("WAdvTable").deleteRow(i);
	}
}

function enable_wme_check(obj){
	if(obj.value == "off"){
		inputCtrl(document.form.wl_wme_no_ack, 0);
		if(!Rawifi_support && !Qcawifi_support)
			inputCtrl(document.form.wl_igs, 0);
		
		inputCtrl(document.form.wl_wme_apsd, 0);
	}
	else{
		if(document.form.wl_nmode_x.value == "0" || document.form.wl_nmode_x.value == "1"){	//auto, n only
			document.form.wl_wme_no_ack.value = "off";
			inputCtrl(document.form.wl_wme_no_ack, 0);
		}else		
			inputCtrl(document.form.wl_wme_no_ack, 1);
		
		if(!Rawifi_support && !Qcawifi_support)
			inputCtrl(document.form.wl_igs, 1);

		inputCtrl(document.form.wl_wme_apsd, 1);
	}
}

/* AMPDU RTS for AC model, Jieming added at 2013.08.26 */
function check_ampdu_rts(){
	if(document.form.wl_nmode_x.value != 2 && band5g_11ac_support){
		document.getElementById('ampdu_rts_tr').style.display = "";
		if(document.form.wl_ampdu_rts.value == 1){
			document.form.wl_rts.disabled = false;
			document.getElementById('rts_threshold').style.display = "";
		}	
		else{
			document.form.wl_rts.disabled = true;
			document.getElementById('rts_threshold').style.display = "none";
		}	
	}
	else{
		document.form.wl_ampdu_rts.disabled = true;
		document.getElementById('ampdu_rts_tr').style.display = "none";
	}
}

function register_event(){
	$(function() {
		$( "#slider" ).slider({
			orientation: "horizontal",
			range: "min",
			min:1,
			max: 100,
			value:100,
			slide:function(event, ui){
				document.getElementById('wl_txpower').value = ui.value; 
			},
			stop:function(event, ui){
				set_power(ui.value);	  
			}
		}); 
	});
	
	var array_temp = new Array(7);
	var checked = 0
	var unchecked = 0;
	init_array(array_temp);

  $(function() {
    $( "#selectable" ).selectable({
		filter:'td',
		selecting: function(event, ui){
					
		},
		unselecting: function(event, ui){
			
		},
		selected: function(event, ui){	
			id = ui.selected.getAttribute('id');
			column = parseInt(id.substring(0,1), 10);
			row = parseInt(id.substring(1,3), 10);	

			array_temp[column][row] = 1;
			if(array[column][row] == 1){
				checked = 1;
			}
			else if(array[column][row] == 0){
				unchecked = 1;
			}
		},
		unselected: function(event, ui){

		},		
		stop: function(event, ui){
			if((checked == 1 && unchecked == 1) || (checked == 0 && unchecked == 1)){
				for(i=0;i<7;i++){
					for(j=0;j<24;j++){
						if(array_temp[i][j] == 1){
						array[i][j] = array_temp[i][j];					
						array_temp[i][j] = 0;		//initialize
						if(j < 10){
							j = "0" + j;						
						}		
							id = i.toString() + j.toString();					
							document.getElementById(id).className = "checked";					
						}
					}
				}									
			}
			else if(checked == 1 && unchecked == 0){
				for(i=0;i<7;i++){
					for(j=0;j<24;j++){
						if(array_temp[i][j] == 1){
						array[i][j] = 0;					
						array_temp[i][j] = 0;
						
						if(j < 10){
							j = "0" + j;						
						}
							id = i.toString() + j.toString();											
							document.getElementById(id).className = "disabled";												
						}
					}
				}			
			}
		
			checked = 0;
			unchecked = 0;
		}		
	});		
  });
}

function set_power(power_value){	
	if(power_value < 1){
		power_value = 1;
		alert("The minimun value of power is 1");
	}
	
	if(power_value > 100){
		power_value = 100;
		alert("The maximun value of power is 1");
	}
	
	document.getElementById('slider').children[0].style.width = power_value + "%";
	document.getElementById('slider').children[1].style.left = power_value + "%";
	document.form.wl_txpower.value = power_value;
}

function init_array(arr){
	for(i=0;i<7;i++){
		arr[i] = new Array(24);

		for(j=0;j<24;j++){
			arr[i][j] = 0;
		}
	}
}

function init_cookie(){
	if(document.cookie.indexOf('clock_type') == -1)		//initialize
		document.cookie = "clock_type=1";		
			
	x = document.cookie.split(';');
	for(i=0;i<x.length;i++){
		if(x[i].indexOf('clock_type') != -1){
			clock_type = x[i].substring(x[i].length-1, x[i].length);			
		}	
	}
}

//draw time slot at first time
function redraw_selected_time(obj){
	var start_day = 0;
	var end_day = 0;
	var start_time = "";
	var end_time = "";
	var time_temp = "";
	var duration = "";
	var id = "";

	for(i=0;i<obj.length;i++){
		time_temp = obj[i];
		start_day = parseInt(time_temp.substring(0,1), 10);
		end_day =  parseInt(time_temp.substring(1,2), 10);
		start_time =  parseInt(time_temp.substring(2,4), 10);
		end_time =  parseInt(time_temp.substring(4,6), 10);
		if((start_day == end_day) && (end_time - start_time) < 0)	//for Sat 23 cross to Sun 00
			end_day = 7;

		if(start_day == end_day){			// non cross day
			duration = end_time - start_time;
			if(duration == 0)	//for whole selected
				duration = 7*24;
			
			while(duration >0){
				array[start_day][start_time] = 1;
				if(start_time < 10)
					start_time = "0" + start_time;
								
				id = start_day.toString() + start_time.toString();
				document.getElementById(id).className = "checked";
				start_time++;
				if(start_time == 24){
					start_time = 0;
					start_day++;
					if(start_day == 7)
						start_day = 0;
				}
	
				duration--;
				id = "";		
			}	
		}else{			// cross day
			var duration_day = 0;
			if(end_day - start_day < 0)
				duration_day = 7 - start_day;
			else
				duration_day = end_day - start_day;
		
			duration = (24 - start_time) + (duration_day - 1)*24 + end_time;
			while(duration > 0){
				array[start_day][start_time] = 1;
				if(start_time < 10)
					start_time = "0" + start_time;
				
				id = start_day.toString() + start_time.toString();
				document.getElementById(id).className = "checked";
				start_time++;
				if(start_time == 24){
					start_time = 0;
					start_day++;
					if(start_day == 7)
						start_day = 0;		
				}
				
				duration--;
				id = "";	
			}		
		}	
	}
}

function select_all(){
	var full_flag = 1;
	for(i=0;i<7;i++){
		for(j=0;j<24;j++){
			if(array[i][j] ==0){ 
				full_flag = 0;
				break;
			}
		}
		
		if(full_flag == 0){
			break;
		}
	}

	if(full_flag == 1){
		for(i=0;i<7;i++){
			for(j=0;j<24;j++){
				array[i][j] = 0;
				if(j<10){
					j = "0"+j;
				}
		
				id = i.toString() + j.toString();
				document.getElementById(id).className = "disabled";
			}
		}	
	}
	else{
		for(i=0;i<7;i++){
			for(j=0;j<24;j++){
				if(array[i][j] == 1)
					continue;
				else{	
					array[i][j] = 1;
					if(j<10){
						j = "0"+j;
					}
			
					id = i.toString() + j.toString();
					document.getElementById(id).className = "checked";
				}
			}
		}
	}
}

function select_all_day(day){
	var check_flag = 0
	day = day.substring(4,5);
	for(i=0;i<24;i++){
		if(array[day][i] == 0){
			check_flag = 1;			
		}			
	}
	
	if(check_flag == 1){
		for(j=0;j<24;j++){
			array[day][j] = 1;
			if(j<10){
				j = "0"+j;
			}
		
			id = day + j;
			document.getElementById(id).className = "checked";	
		}
	}
	else{
		for(j=0;j<24;j++){
			array[day][j] = 0;
			if(j<10){
				j = "0"+j;
			}
		
			id = day + j;
			document.getElementById(id).className = "disabled";	
		}
	}
}

function select_all_time(time){
	var check_flag = 0;
	time_int = parseInt(time, 10);	
	for(i=0;i<7;i++){
		if(array[i][time] == 0){
			check_flag = 1;			
		}			
	}
	
	if(time<10){
		time = "0"+time;
	}

	if(check_flag == 1){
		for(i=0;i<7;i++){
			array[i][time_int] = 1;
			
		id = i + time;
		document.getElementById(id).className = "checked";
		}
	}
	else{
		for(i=0;i<7;i++){
			array[i][time_int] = 0;

		id = i + time;
		document.getElementById(id).className = "disabled";
		}
	}
}

function change_clock_type(type){
	document.cookie = "clock_type="+type;
	if(type == 1)
		var array_time = ["00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24"];
	else
		var array_time = ["12", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12"];

	for(i=0;i<24;i++){
		if(type == 1)
			document.getElementById(i).innerHTML = array_time[i] +" ~ "+ array_time[i+1];
		else{
			if(i<11 || i == 23)
				document.getElementById(i).innerHTML = array_time[i] +" ~ "+ array_time[i+1] + " AM";
			else
				document.getElementById(i).innerHTML = array_time[i] +" ~ "+ array_time[i+1] + " PM";
		}	
	}
}

function save_wifi_schedule(){
	var flag = 0;
	var start_day = 0;
	var end_day = 0;
	var start_time = 0;
	var end_time = 0;
	var time_temp = "";
	
	for(i=0;i<7;i++){
		for(j=0;j<24;j++){
			if(array[i][j] == 1){
				if(flag == 0){
					flag =1;
					start_day = i;
					if(j<10)
						j = "0" + j;
						
					start_time = j;				
				}
			}
			else{
				if(flag == 1){
					flag =0;
					end_day = i;
					if(j<10)
						j = "0" + j;
					
					end_time = j;		
					if(time_temp != "")
						time_temp += "<";
				
					time_temp += start_day.toString() + end_day.toString() + start_time.toString() + end_time.toString();
				}
			}
		}	
	}
	
	if(flag == 1){
		if(time_temp != "")
			time_temp += "<";
									
		time_temp += start_day.toString() + "0" + start_time.toString() + "00";	
	}

	if(time_temp == ""){
		alert("If you want to deny WiFi radio all time, you should check the '<#WLANConfig11b_x_RadioEnable_itemname#>' to No");
		return false;
	}	
	
	wifi_schedule_value = time_temp;
	document.getElementById("schedule_block").style.display = "none";
	document.getElementById("titl_desc").style.display = "";
	document.getElementById("WAdvTable").style.display = "";
	document.getElementById("apply_btn").style.display = "";
}

function cancel_wifi_schedule(client){
	init_array(array);
	document.getElementById("schedule_block").style.display = "none";
	document.getElementById("titl_desc").style.display = "";
	document.getElementById("WAdvTable").style.display = "";
	document.getElementById("apply_btn").style.display = "";
}

function count_time(){		// To count system time
	systime_millsec += 1000;
	setTimeout("count_time()", 1000);
}

function showclock(){
	JS_timeObj.setTime(systime_millsec);
	JS_timeObj2 = JS_timeObj.toString();	
	JS_timeObj2 = JS_timeObj2.substring(0,3) + ", " +
	              JS_timeObj2.substring(4,10) + "  " +
				  checkTime(JS_timeObj.getHours()) + ":" +
				  checkTime(JS_timeObj.getMinutes()) + ":" +
				  checkTime(JS_timeObj.getSeconds()) + "  " +
				  JS_timeObj.getFullYear();
	document.getElementById("system_time").value = JS_timeObj2;
	setTimeout("showclock()", 1000);
	
	if(svc_ready == "0")
		document.getElementById('svc_hint_div').style.display = "";
	corrected_timezone();
}


function show_wifi_schedule(){
	document.getElementById("titl_desc").style.display = "none";
	document.getElementById("WAdvTable").style.display = "none";
	document.getElementById("apply_btn").style.display = "none";
	document.getElementById("schedule_block").style.display = "";

	var array_date = ["Select All", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
	var array_time_id = ["00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23"];
	if(clock_type == "1")
		var array_time = ["00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24"];
	else
		var array_time = ["12am", "1am", "2am", "3am", "4am", "5am", "6am", "7am", "8am", "9am", "10am", "11am", "12pm", "1pm", "2pm", "3pm", "4pm", "5pm", "6pm", "7pm", "8pm", "9pm", "10pm", "11pm", "12am"];
	
	var code = "";
	
	wifi_schedule_row = wifi_schedule_value.split('<');

	code +='<div style="margin-bottom:10px;color: #003399;font-family: Verdana;" align="left">';
	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="FormTable">';
	code +='<thead><tr><td colspan="6" id="LWFilterList"><#ParentalCtrl_Act_schedule#></td></tr></thead>';
	code +='<tr><th style="width:40%;height:20px;" align="right"><#General_x_SystemTime_itemname#></th>';	
	code +='<td align="left" style="color:#FFF"><input type="text" id="system_time" name="system_time" class="devicepin" value="" readonly="1" style="font-size:12px;width:200px;"></td></tr>';		
	code +='</table><table id="main_select_table">';
	code +='<table  id="selectable" class="table_form" >';
	code += "<tr>";
	for(i=0;i<8;i++){
		if(i == 0)
			code +="<th class='parental_th' onclick='select_all();'>"+array_date[i]+"</th>";	
		else
			code +="<th id=col_"+(i-1)+" class='parental_th' onclick='select_all_day(this.id);'>"+array_date[i]+"</th>";			
	}
	
	code += "</tr>";
	for(i=0;i<24;i++){
		code += "<tr>";
		code +="<th id="+i+" class='parental_th' onclick='select_all_time(this.id)'>"+ array_time[i] + " ~ " + array_time[i+1] +"</th>";
		for(j=0;j<7;j++){
			code += "<td id="+ j + array_time_id[i] +" class='disabled' ></td>";		
		}
		
		code += "</tr>";			
	}
	
	code +='</table></table></div>';
	document.getElementById("mainTable").innerHTML = code;

	register_event();
	redraw_selected_time(wifi_schedule_row);
	
	var code_temp = "";
	code_temp = '<table style="width:350px;margin-left:20px;"><tr>';
	code_temp += "<td><div style=\"width:95px;font-family:Arial,sans-serif,Helvetica;font-size:18px;\"><#Clock_Format#></div></td>";
	code_temp += '<td><div>';
	code_temp += '<select id="clock_type_select" class="input_option" onchange="change_clock_type(this.value);">';
	code_temp += '<option value="0" >12-hour</option>';
	code_temp += '<option value="1" >24-hour</option>';
	code_temp += '</select>';
	code_temp += '</div></td>';
	code_temp += '<td><div align="left" style="font-family:Arial,sans-serif,Helvetica;font-size:18px;margin:0px 5px 0px 30px;">Allow</div></td>';
	code_temp += '<td><div style="width:90px;height:20px;background:#9CB2BA;"></div></td>';
	code_temp += '<td><div align="left" style="font-family:Arial,sans-serif,Helvetica;font-size:18px;margin:0px 5px 0px 30px;">Deny</div></td>';
	code_temp += '<td><div style="width:90px;height:20px;border:solid 1px #000"></div></td>';
	code_temp += '</tr></table>';
	document.getElementById('hintBlock').innerHTML = code_temp;
	document.getElementById('hintBlock').style.marginTop = "10px";
	document.getElementById('hintBlock').style.display = "";
	document.getElementById("ctrlBtn").innerHTML = '<input class="button_gen" type="button" onClick="cancel_wifi_schedule();" value="<#CTL_Cancel#>">';
	document.getElementById("ctrlBtn").innerHTML += '<input class="button_gen" type="button" onClick="save_wifi_schedule();" value="<#CTL_ok#>">';  
	document.getElementById('clock_type_select')[clock_type].selected = true;		// set clock type by cookie
	
	document.getElementById("mainTable").style.display = "";
	$("#mainTable").fadeIn();
	showclock();
}

function control_TimeField(){
	if(document.form.wl_radio[0].checked){
		document.getElementById("wl_sched_enable").style.display = "";
		document.form.wl_timesched.disabled = false;
		if(document.form.wl_timesched[0].checked){
			document.getElementById("time_setting").style.display = "";
			document.form.wl_sched.disabled = false;
		}
		else{
			document.getElementById("time_setting").style.display = "none";
			document.form.wl_sched.disabled = true;
		}
	}
	else{
		document.getElementById("wl_sched_enable").style.display = "none";
		document.form.wl_timesched.disabled = true;
	}
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>


<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>"><input type="hidden" name="wl_nmode_x" value="<% nvram_get("wl_nmode_x"); %>">
<input type="hidden" name="wl_gmode_protection_x" value="<% nvram_get("wl_gmode_protection_x"); %>">
<input type="hidden" name="current_page" value="Advanced_WAdvanced_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WAdvanced_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_subunit" value="-1">
<input type="hidden" name="wl_amsdu" value="<% nvram_get("wl_amsdu"); %>">
<input type="hidden" name="wl0_country_code" value="<% nvram_get("wl0_country_code"); %>" disabled>
<input type="hidden" name="wl_HW_switch" value="<% nvram_get("wl_HW_switch"); %>" disabled>
<input type="hidden" name="wl_TxPower" value="<% nvram_get("wl_TxPower"); %>" >
<input type="hidden" name="wl1_80211h_orig" value="<% nvram_get("wl1_80211h"); %>" >
<input type="hidden" name="acs_dfs" value="<% nvram_get("acs_dfs"); %>">
<input type="hidden" name="w_Setting" value="1">
<input type="hidden" name="wl_sched" value="<% nvram_get("wl_sched"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="17">&nbsp;</td>
		
		<td valign="top" width="202">				
		<div  id="mainMenu"></div>	
		<div  id="subMenu"></div>		
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
		  			<div class="formfonttitle"><#menu5_1#> - <#menu5_1_6#></div>
		  			<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		 				<div id="titl_desc" class="formfontdesc"><#WLANConfig11b_display5_sectiondesc#></div>
		 				<div id="svc_hint_div" style="display:none;margin-left:5px;"><span onClick="location.href='Advanced_System_Content.asp?af=ntp_server0'" style="color:#FFCC00;text-decoration:underline;cursor:pointer;"><#General_x_SystemTime_syncNTP#></span></div>
		  			<div id="timezone_hint_div" style="margin-left:5px;display:none;"><span id="timezone_hint" onclick="location.href='Advanced_System_Content.asp?af=time_zone_select'" style="color:#FFCC00;text-decoration:underline;cursor:pointer;"></span></div>	


					<div id="schedule_block" style="display:none">
						<div id="hintBlock" style="width: 650px; margin-top: 10px;">
							<table style="width:350px;"><tbody><tr><td><div style="width:95px;font-family:Arial,sans-serif,Helvetica;font-size:18px;"><#Clock_Format#></div></td><td><div><select id="clock_type_select" class="input_option" onchange="change_clock_type(this.value);"><option value="0">12-hour</option><option value="1">24-hour</option></select></div></td><td><div align="left" style="font-family:Arial,sans-serif,Helvetica;font-size:18px;margin:0px 5px 0px 30px;">Allow</div></td><td><div style="width:90px;height:20px;background:#9CB2BA;"></div></td><td><div align="left" style="font-family:Arial,sans-serif,Helvetica;font-size:18px;margin:0px 5px 0px 30px;">Deny</div></td><td><div style="width:90px;height:20px;border:solid 1px #000"></div></td></tr></tbody></table>
						</div>
						<div id="mainTable" style="padding:0 20px 20px 20px;"></div>
						<div id="ctrlBtn" style="text-align:center;"></div>
					</div>

					
					<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable" id="WAdvTable">	

					<tr id="wl_unit_field" class="rept">
						<th><#Interface#></th>
						<td>
							<select name="wl_unit" class="input_option" onChange="change_wl_unit();">
								<option class="content_input_fd" value="0" <% nvram_match("wl_unit", "0","selected"); %>>2.4GHz</option>
								<option class="content_input_fd" value="1"<% nvram_match("wl_unit", "1","selected"); %>>5GHz</option>
							</select>			
						</td>
					</tr>
					<tr id="wl_rf_enable">
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 1);"><#WLANConfig11b_x_RadioEnable_itemname#></a></th>
			  			<td>
			  				<input type="radio" value="1" name="wl_radio" class="input" onClick="control_TimeField();" <% nvram_match("wl_radio", "1", "checked"); %>><#checkbox_Yes#>
			    			<input type="radio" value="0" name="wl_radio" class="input" onClick="control_TimeField();" <% nvram_match("wl_radio", "0", "checked"); %>><#checkbox_No#>
			  			</td>
					</tr>

					<tr id="wl_sched_enable">
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 23);"><#WLANConfig11b_x_SchedEnable_itemname#></a></th>
			  			<td>
			  				<input type="radio" value="1" name="wl_timesched" class="input" onClick="control_TimeField();return change_common_radio(this, 'WLANConfig11b', 'wl_timesched', '1');" <% nvram_match("wl_timesched", "1", "checked"); %>><#checkbox_Yes#>
			    			<input type="radio" value="0" name="wl_timesched" class="input" onClick="control_TimeField();return change_common_radio(this, 'WLANConfig11b', 'wl_timesched', '0')" <% nvram_match("wl_timesched", "0", "checked"); %>><#checkbox_No#>
							<span id="time_setting" style="padding-left:20px;cursor:pointer;text-decoration:underline" onclick="show_wifi_schedule();">Time Setting</span>
						</td>
					</tr>

					<tr>
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 5);"><#WLANConfig11b_x_IsolateAP_itemname#></a></th>
			  			<td>
							<input type="radio" value="1" name="wl_ap_isolate" class="input" onClick="return change_common_radio(this, 'WLANConfig11b', 'wl_ap_isolate', '1')" <% nvram_match("wl_ap_isolate", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" value="0" name="wl_ap_isolate" class="input" onClick="return change_common_radio(this, 'WLANConfig11b', 'wl_ap_isolate', '0')" <% nvram_match("wl_ap_isolate", "0", "checked"); %>><#checkbox_No#>
			  			</td>
					</tr>
					
					<tr id="wl_rate">
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 6);"><#WLANConfig11b_DataRateAll_itemname#></a></th>
			  			<td>
							<select name="wl_rate" class="input_option">
				  				<option value="0" <% nvram_match("wl_rate", "0","selected"); %>><#Auto#></option>
				  				<option value="1000000" <% nvram_match("wl_rate", "1000000","selected"); %>>1</option>
				  				<option value="2000000" <% nvram_match("wl_rate", "2000000","selected"); %>>2</option>
				  				<option value="5500000" <% nvram_match("wl_rate", "5500000","selected"); %>>5.5</option>
				  				<option value="6000000" <% nvram_match("wl_rate", "6000000","selected"); %>>6</option>
				  				<option value="9000000" <% nvram_match("wl_rate", "9000000","selected"); %>>9</option>
				  				<option value="11000000" <% nvram_match("wl_rate", "11000000","selected"); %>>11</option>
				  				<option value="12000000" <% nvram_match("wl_rate", "12000000","selected"); %>>12</option>
				  				<option value="18000000" <% nvram_match("wl_rate", "18000000","selected"); %>>18</option>
				  				<option value="24000000" <% nvram_match("wl_rate", "24000000","selected"); %>>24</option>
				  				<option value="36000000" <% nvram_match("wl_rate", "36000000","selected"); %>>36</option>
				  				<option value="48000000" <% nvram_match("wl_rate", "48000000","selected"); %>>48</option>
				  				<option value="54000000" <% nvram_match("wl_rate", "54000000","selected"); %>>54</option>
							</div>
			  			</td>
					</tr>

					<tr id="rssiTr" class="rept">
		  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 31);"><#Roaming_assistant#></a></th>
						<td>
							<select id="wl_user_rssi_option" class="input_option" onchange="changeRSSI(this.value);">
								<option value="1"><#WLANConfig11b_WirelessCtrl_button1name#></option>
								<option value="0" <% nvram_match("wl_user_rssi", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
							</select>
							<span id="rssiDbm" style="color:#FFF">
								<!-- untranslated -->Disconnect clients with RSSI lower than
			  				<input type="text" maxlength="3" name="wl_user_rssi" class="input_3_table" value="<% nvram_get("wl_user_rssi"); %>" autocorrect="off" autocapitalize="off">
								dBm
							</span>
						</td>
					</tr>

					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 22);"><#WLANConfig11b_x_IgmpSnEnable_itemname#></a></th>
						<td>
							<select name="wl_igs" class="input_option">
								<option value="1" <% nvram_match("wl_igs", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
								<option value="0" <% nvram_match("wl_igs", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
							</select>
						</td>
					</tr>
					<tr id="wl_mrate_select">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 7);"><#WLANConfig11b_MultiRateAll_itemname#></a></th>
						<td>
							<select name="wl_mrate_x" class="input_option">
								<option value="0" <% nvram_match("wl_mrate_x", "0", "selected"); %>><#Auto#></option>
							</select>
						</td>
					</tr>
					<tr style="display:none;">
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 8);"><#WLANConfig11b_DataRate_itemname#></a></th>
			  			<td>
			  				<select name="wl_rateset" class="input_option">
				  				<option value="default" <% nvram_match("wl_rateset", "default","selected"); %>><#Setting_factorydefault_value#></option>
				  				<option value="all" <% nvram_match("wl_rateset", "all","selected"); %>><#All#></option>
				  				<option value="12" <% nvram_match("wl_rateset", "12","selected"); %>>1, 2 Mbps</option>
							</select>
						</td>
					</tr>
					<tr id="wl_plcphdr_field">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,20);"><#WLANConfig11n_PremblesType_itemname#></a></th>
						<td>
						<select name="wl_plcphdr" class="input_option">
							<option value="long" <% nvram_match("wl_plcphdr", "long", "selected"); %>><#WLANConfig11n_PremblesType_long#></option>
							<option value="short" <% nvram_match("wl_plcphdr", "short", "selected"); %>><#WLANConfig11n_PremblesType_short#></option>
<!-- auto mode applicable for STA only
							<option value="auto" <% nvram_match("wl_plcphdr", "auto", "selected"); %>><#Auto#></option>
-->
						</select>
						</td>
					</tr>
					<tr>
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 9);"><#WLANConfig11b_x_Frag_itemname#></a></th>
			  			<td>
			  				<input type="text" maxlength="4" name="wl_frag" id="wl_frag" class="input_6_table" value="<% nvram_get("wl_frag"); %>" onKeyPress="return validator.isNumber(this,event)" autocorrect="off" autocapitalize="off">
						</td>
					</tr>
					<tr id='ampdu_rts_tr'>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,30);">AMPDU RTS</a></th>	<!-- untranslated -->
						<td>
							<select name="wl_ampdu_rts" class="input_option" onchange="check_ampdu_rts();">
								<option value="1" <% nvram_match("wl_ampdu_rts", "1", "selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
								<option value="0" <% nvram_match("wl_ampdu_rts", "0", "selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
							</select>
						</td>
					</tr>
					<tr id="rts_threshold">
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 10);"><#WLANConfig11b_x_RTS_itemname#></a></th>
			  			<td>
			  				<input type="text" maxlength="4" name="wl_rts" class="input_6_table" value="<% nvram_get("wl_rts"); %>" onKeyPress="return validator.isNumber(this,event)" autocorrect="off" autocapitalize="off">
			  			</td>
					</tr>
					<tr id="wl_dtim_field">
			  			<th><a class="hintstyle" id="wl_dtim_th" href="javascript:void(0);" onClick=""><#WLANConfig11b_x_DTIM_itemname#></a></th>
						<td>
			  				<input type="text" maxlength="3" name="wl_dtim" class="input_6_table" value="<% nvram_get("wl_dtim"); %>" onKeyPress="return validator.isNumber(this,event)" autocorrect="off" autocapitalize="off">
						</td>			  
					</tr>
					<tr id="wl_bcn_field">
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 12);"><#WLANConfig11b_x_Beacon_itemname#></a></th>
						<td>
							<input type="text" maxlength="4" name="wl_bcn" class="input_6_table" value="<% nvram_get("wl_bcn"); %>" onKeyPress="return validator.isNumber(this,event)" autocorrect="off" autocapitalize="off">
						</td>
					</tr>
					<tr id="wl_frameburst_field">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 13);"><#WLANConfig11b_x_TxBurst_itemname#></a></th>
						<td>
							<select name="wl_frameburst" class="input_option">
								<option value="off" <% nvram_match("wl_frameburst", "off","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								<option value="on" <% nvram_match("wl_frameburst", "on","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>
					<tr id="PktAggregate"><!-- RaLink Only -->
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 16);"><#WLANConfig11b_x_PktAggregate_itemname#></a></th>
						<td>
							<select name="wl_PktAggregate" class="input_option">
								<option value="0" <% nvram_match("wl_PktAggregate", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								<option value="1" <% nvram_match("wl_PktAggregate", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>

					<!-- WMM setting start  -->
					<tr>
			  			<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(3, 14);"><#WLANConfig11b_x_WMM_itemname#></a></th>
			  			<td>
							<select name="wl_wme" id="wl_wme" class="input_option" onChange="enable_wme_check(this);">			  	  				
			  	  				<option value="auto" <% nvram_match("wl_wme", "auto", "selected"); %>><#Auto#></option>
			  	  				<option value="on" <% nvram_match("wl_wme", "on", "selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
			  	  				<option value="off" <% nvram_match("wl_wme", "off", "selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>			  	  			
							</select>
			  			</td>
					</tr>
					<tr>
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,15);"><#WLANConfig11b_x_NOACK_itemname#></a></th>
			  			<td>
							<select name="wl_wme_no_ack" id="wl_wme_no_ack" class="input_option">
			  	  				<option value="off" <% nvram_match("wl_wme_no_ack", "off","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
			  	  				<option value="on" <% nvram_match("wl_wme_no_ack", "on","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
			  			</td>
					</tr>
					<tr id="wl_wme_apsd_field">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,17);"><#WLANConfig11b_x_APSD_itemname#></a></th>
						<td>
                  				<select name="wl_wme_apsd" class="input_option">
                    					<option value="off" <% nvram_match("wl_wme_apsd", "off","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
                    					<option value="on" <% nvram_match("wl_wme_apsd", "on","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
                  				</select>
						</td>
					</tr>					
					<!-- WMM setting end  -->

					<tr id="DLSCapable"> <!-- RaLink Only  -->
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,18);"><#WLANConfig11b_x_DLS_itemname#></a></th>
						<td>
							<select name="wl_DLSCapable" class="input_option">
								<option value="0" <% nvram_match("wl_DLSCapable", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								<option value="1" <% nvram_match("wl_DLSCapable", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>

					<tr> <!-- BRCM SDK 5.x Only  -->
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,21);"><#WLANConfig11b_x_EnhanInter_itemname#></a></th>
						<td>
							<select name="wl_noisemitigation" class="input_option" onChange="">
								<option value="0" <% nvram_match("wl_noisemitigation", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								<option value="1" <% nvram_match("wl_noisemitigation", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>

					<tr> <!-- MODELDEP: RT-AC3200 / RT-AC68U / RT-AC68U_V2 / RT-AC69U / DSL-AC68U Only  -->
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,29);"><#WLANConfig11b_x_ReduceUSB3#></a></th>
						<td>
							<select name="usb_usb3" class="input_option">
								<option value="1" <% nvram_match("usb_usb3", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								<option value="0" <% nvram_match("usb_usb3", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>
					
					<tr> <!-- MODELDEP: RT-AC55U -->
						<th><a class="hintstyle" href="javascript:void(0);"><#WLANConfig11b_x_traffic_counter#></a></th>
						<td>
							<select name="traffic_5g" class="input_option">
								<option value="1" <% nvram_match("traffic_5g", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
								<option value="0" <% nvram_match("traffic_5g", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
							</select>
						</td>
					</tr>
					

					<!-- [MODELDEP] for Broadcom SDK 6.x -->
					<tr id="wl_ampdu_mpdu_field">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,26);"><#WLANConfig11b_x_AMPDU#></a></th>
						<td>
							<select name="wl_ampdu_mpdu" class="input_option">
									<option value="0" <% nvram_match("wl_ampdu_mpdu", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
									<option value="1" <% nvram_match("wl_ampdu_mpdu", "1","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>					
					<tr id="wl_ack_ratio_field">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,27);"><#WLANConfig11b_x_ACK#></a></th>
						<td>
							<select name="wl_ack_ratio" class="input_option">
									<option value="0" <% nvram_match("wl_ack_ratio", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
									<option value="1" <% nvram_match("wl_ack_ratio", "1","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>
					<tr>
						<th id="turbo_qam_title"><a id="turbo_qam_hint" class="hintstyle" href="javascript:void(0);" onClick="openHint(3,28);"><#WLANConfig11b_x_TurboQAM#></a></th>
						<td>
							<select name="wl_turbo_qam" class="input_option">
									<option value="0" <% nvram_match("wl_turbo_qam", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
									<option value="1" <% nvram_match("wl_turbo_qam", "1","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>
					<!-- [MODELDEP] end -->
					<!--For 5GHz of RT-AC87U  -->
					<tr id="wl_80211h_tr" style="display:none;">
						<th><a class="hintstyle" href="javascript:void(0);" onClick=""><#WLANConfig11b_x_80211H#></a></th>
						<td>
							<select name="wl1_80211h" class="input_option">
									<option value="0" <% nvram_match("wl1_80211h", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
									<option value="1" <% nvram_match("wl1_80211h", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>
					
					
					<!--Broadcom ARM platform only, except RT-AC87U(5G) -->
					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,32);"><#WLANConfig11b_x_Airtime_Fairness#></a></th>
						<td>
							<select name="wl_atf" class="input_option">
									<option value="0" <% nvram_match("wl_atf", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
									<option value="1" <% nvram_match("wl_atf", "1","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>
					
					<tr id="wl_txbf_field">
						<th><a id="wl_txbf_desc" class="hintstyle" href="javascript:void(0);" onClick="openHint(3,24);"><#WLANConfig11b_x_ExpBeam#></a></th>
						<td>
							<select name="wl_txbf" class="input_option">
									<option value="0" <% nvram_match("wl_txbf", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
									<option value="1" <% nvram_match("wl_txbf", "1","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>					
					<tr id="wl_itxbf_field">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,25);"><#WLANConfig11b_x_uniBeam#></a></th>
						<td>
							<select name="wl_itxbf" class="input_option" disabled>
									<option value="0" <% nvram_match("wl_itxbf", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
									<option value="1" <% nvram_match("wl_itxbf", "1","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>					

					<tr id="wl_txPower_field">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 16);"><#WLANConfig11b_TxPower_itemname#></a></th>
						<td>
							<div>
								<table>
									<tr >
										<td style="border:0px;width:60px;">
											<input id="wl_txpower" name="wl_txpower" type="text" maxlength="3" class="input_3_table" value="<% nvram_get("wl_txpower"); %>" style="margin-left:-10px;" onkeyup="set_power(this.value);" autocorrect="off" autocapitalize="off"> %
										</td>					
										<td style="border:0px;">
											<div id="slider" style="width:200px;"></div>
										</td>
									</tr>
								</table>
							</div>
						</td>
					</tr>
					<tr id="region_tr" style="display:none">
						<th><a class="hintstyle" href="javascript:void(0);" onClick=""><#WLANConfig11b_x_Region#></a></th>
						<td><select name="location_code" class="input_option"></select></td>
					</tr>
				</table>
					
						<div class="apply_gen" id="apply_btn">
							<input class="button_gen" onclick="applyRule();" type="button" value="<#CTL_apply#>"/>
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
