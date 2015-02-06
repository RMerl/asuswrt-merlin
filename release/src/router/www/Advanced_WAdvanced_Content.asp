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
<script type="text/javascript" src="/jquery.js"></script>
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


</style>
<script>
var $j = jQuery.noConflict();//var flag = 0;

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

var flag_week = 0;
var flag_weekend = 0;
var flag_initial =0;
var wl_version = "<% nvram_get("wl_version"); %>";
var sdk_version_array = new Array();
sdk_version_array = wl_version.split(".");
var sdk_6 = sdk_version_array[0] == "6" ? true:false
var sdk_7 = sdk_version_array[0] == "7" ? true:false
var wl_user_rssi_onload = '<% nvram_get("wl_user_rssi"); %>';

function initial(){
	show_menu();
	register_event();
	
	if(userRSSI_support)
		changeRSSI(wl_user_rssi_onload);
	else
		$("rssiTr").style.display = "none";

	if(!band5g_support)
		$("wl_unit_field").style.display = "none";

	regen_band(document.form.wl_unit);

	if(sw_mode == "2"){
		var _rows = $("WAdvTable").rows;
		for(var i=0; i<_rows.length; i++){
			if(_rows[i].className.search("rept") == -1){
				_rows[i].style.display = "none";
				_rows[i].disabled = true;
			}
		}

		// enable_wme_check(document.form.wl_wme);
		return false;
	}

	$("wl_rate").style.display = "none";

	if(Rawifi_support){
		inputCtrl(document.form.wl_noisemitigation, 0);
	}
	else if(Qcawifi_support){
		// FIXME
		$("DLSCapable").style.display = "none";	
		$("PktAggregate").style.display = "none";
		inputCtrl(document.form.wl_noisemitigation, 0);
	}else{
		// BRCM
		$("DLSCapable").style.display = "none";	
		$("PktAggregate").style.display = "none";
		
		if('<% nvram_get("wl_unit"); %>' == '1' || sdk_6){	// MODELDEP: for Broadcom SDK 6.x model
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
		if(	based_modelid == "RT-AC3200" ||
			based_modelid == "RT-AC69U" || based_modelid == "TM-AC1900" ||
			based_modelid == "RT-AC56S" || based_modelid == "RT-AC56U" ||
			based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "DSL-AC68U" ||
			based_modelid == "RT-AC87U" || based_modelid == "EA-AC87")
		{
			$('wl_txbf_desc').innerHTML = "802.11ac Beamforming";
			inputCtrl(document.form.wl_txbf, 1);
		}	

		if(	based_modelid == "RT-AC3200" ||
			based_modelid == "RT-AC69U" || based_modelid == "TM-AC1900" ||
			based_modelid == "RT-AC56S" || based_modelid == "RT-AC56U" ||
			based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "DSL-AC68U" ||
			based_modelid == "RT-AC87U" || based_modelid == "EA-AC87")
		{
			inputCtrl(document.form.wl_itxbf, 1);
		}
				
		if( based_modelid == "RT-AC55U")
			inputCtrl(document.form.traffic_5g, 1);
	}
	else{ // 2.4GHz
		if(	based_modelid == "RT-AC3200" ||
			based_modelid == "RT-N18U" ||
			based_modelid == "RT-N65U" ||
			based_modelid == "RT-AC69U" || based_modelid == "TM-AC1900" ||
			based_modelid == "RT-AC87U" ||
			based_modelid == "RT-AC55U" || based_modelid == "4G-AC55U" ||
			based_modelid == "RT-AC56S" || based_modelid == "RT-AC56U" || 
			based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "DSL-AC68U")
		{
			inputCtrl(document.form.usb_usb3, 1);
		}	

		if(	based_modelid == "RT-AC3200" ||
			based_modelid == "RT-N18U" ||
			based_modelid == "RT-AC69U" || based_modelid == "TM-AC1900" ||
			based_modelid == "RT-AC87U" ||
			based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "DSL-AC68U")
		{
			if(based_modelid == "RT-N18U" && bootLoader_ver < 2000)
				inputCtrl(document.form.wl_turbo_qam, 0);
			else
				inputCtrl(document.form.wl_turbo_qam, 1);
				
			$('wl_txbf_desc').innerHTML = "<#WLANConfig11b_x_ExpBeam#>";
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
		add_option(document.form.wl_mrate_x,
			mcast_rates[i][0], mcast_rates[i][1],
			(mcast_rate == mcast_rates[i][1]) ? 1 : 0);
	}

	if('<% nvram_get("wl_unit"); %>' == '1'){ // 5GHz
		var reg_mode = '<% nvram_get("wl_reg_mode"); %>';
	        add_option(document.form.wl_reg_mode, "802.11h", "strict_h",
	                (reg_mode == "strict_h") ? 1 : 0);
	        add_option(document.form.wl_reg_mode, "802.11d+h", "h",
	                (reg_mode == "h") ? 1 : 0);
	}


	if(repeater_support || psta_support){		//with RE mode
		$("DLSCapable").style.display = "none";	
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
		
	loadDateTime();
	adjust_tx_power();	
	setFlag_TimeFiled();	
	check_Timefield_checkbox();
	control_TimeField();		
	
	if(svc_ready == "0")
		$('svc_hint_div').style.display = "";	
	corrected_timezone();	
	
	if(based_modelid == "RT-AC87U" && '<% nvram_get("wl_unit"); %>' == '1'){	//for RT-AC87U 5G Advanced setting
		$("wl_mrate_select").style.display = "none";
		$("wl_plcphdr_field").style.display = "none";
		$("ampdu_rts_tr").style.display = "none";
		$("rts_threshold").style.display = "none";
		$("wl_frameburst_field").style.display = "none";
		$("wl_wme_apsd_field").style.display = "none";
		$("wl_ampdu_mpdu_field").style.display = "none";
		$("wl_ack_ratio_field").style.display = "none";
		document.getElementById('wl_80211h_tr').style.display = "";
		$("wl_regmode_field").style.display = "none";
	}
	
	/*Airtime fairness, only for Broadcom ARM platform, except RT-AC87U 5G*/
	if(	based_modelid == "RT-N18U" ||
		based_modelid == "RT-AC56U" || based_modelid == "RT-AC56S" ||
		based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "DSL-AC68U" ||
		based_modelid == "RT-AC69U" || based_modelid == "TM-AC1900" ||
		based_modelid == "RT-AC87U" || based_modelid == "RT-AC3200"){
		
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
}

function adjust_tx_power(){
	var power_value_old = document.form.wl_TxPower.value;
	var power_value_new = document.form.wl_txpower.value;
	var translated_value = 0;
	
	if(!power_support){
		$("wl_txPower_field").style.display = "none";
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

			$('slider').children[0].style.width = translated_value + "%";
			$('slider').children[1].style.left = translated_value + "%";
			document.form.wl_txpower.value = translated_value;
		}
		else{
			$('slider').children[0].style.width = power_value_new + "%";
			$('slider').children[1].style.left = power_value_new + "%";
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
	
	if(!validator.timeRange(document.form.wl_radio_time_x_starthour, 0) || !validator.timeRange(document.form.wl_radio_time2_x_starthour, 0)
			|| !validator.timeRange(document.form.wl_radio_time_x_startmin, 1) || !validator.timeRange(document.form.wl_radio_time2_x_startmin, 1)
			|| !validator.timeRange(document.form.wl_radio_time_x_endhour, 2) || !validator.timeRange(document.form.wl_radio_time2_x_endhour, 2)
			|| !validator.timeRange(document.form.wl_radio_time_x_endmin, 3) || !validator.timeRange(document.form.wl_radio_time2_x_endmin, 3)
			){
		return false;
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
	
	if(sw_mode != 2)
		updateDateTime();	

	return true;
}

function done_validating(action){
	refreshpage();
}

function disableAdvFn(row){
	for(var i=row; i>=3; i--){
		$("WAdvTable").deleteRow(i);
	}
}

function loadDateTime(){
	document.form.wl_radio_date_x_Sun.checked = getDateCheck(document.form.wl_radio_date_x.value, 0);
	document.form.wl_radio_date_x_Mon.checked = getDateCheck(document.form.wl_radio_date_x.value, 1);
	document.form.wl_radio_date_x_Tue.checked = getDateCheck(document.form.wl_radio_date_x.value, 2);
	document.form.wl_radio_date_x_Wed.checked = getDateCheck(document.form.wl_radio_date_x.value, 3);
	document.form.wl_radio_date_x_Thu.checked = getDateCheck(document.form.wl_radio_date_x.value, 4);
	document.form.wl_radio_date_x_Fri.checked = getDateCheck(document.form.wl_radio_date_x.value, 5);
	document.form.wl_radio_date_x_Sat.checked = getDateCheck(document.form.wl_radio_date_x.value, 6);
	document.form.wl_radio_time_x_starthour.value = getTimeRange(document.form.wl_radio_time_x.value, 0);
	document.form.wl_radio_time_x_startmin.value = getTimeRange(document.form.wl_radio_time_x.value, 1);
	document.form.wl_radio_time_x_endhour.value = getTimeRange(document.form.wl_radio_time_x.value, 2);
	document.form.wl_radio_time_x_endmin.value = getTimeRange(document.form.wl_radio_time_x.value, 3);
	document.form.wl_radio_time2_x_starthour.value = getTimeRange(document.form.wl_radio_time2_x.value, 0);
	document.form.wl_radio_time2_x_startmin.value = getTimeRange(document.form.wl_radio_time2_x.value, 1);
	document.form.wl_radio_time2_x_endhour.value = getTimeRange(document.form.wl_radio_time2_x.value, 2);
	document.form.wl_radio_time2_x_endmin.value = getTimeRange(document.form.wl_radio_time2_x.value, 3);
}
function control_TimeField(){		//control time of week & weekend field when wireless radio is down , Jieming added 2012/08/22
	if(!document.form.wl_radio[0].checked){
		$("wl_sched_enable").style.display = "none";
		$('enable_date_week_tr').style.display="none";
		$('enable_time_week_tr').style.display="none";
		$('enable_date_weekend_tr').style.display="none";
		$('enable_time_weekend_tr').style.display="none";
	}
	else{
		$("wl_sched_enable").style.display = "";
		if(!document.form.wl_timesched[0].checked){
			$('enable_date_week_tr').style.display="none";
			$('enable_time_week_tr').style.display="none";
			$('enable_date_weekend_tr').style.display="none";
			$('enable_time_weekend_tr').style.display="none";
		}
		else{
			$('enable_date_week_tr').style.display="";
			$('enable_time_week_tr').style.display="";
			$('enable_date_weekend_tr').style.display="";
			$('enable_time_weekend_tr').style.display="";	
		}
	}
}
function check_Timefield_checkbox(){			// To check the checkbox od Date is checked or not and control Time field disabled or not, Jieming add at 2012/10/05
	if(document.form.wl_radio_date_x_Mon.checked == true 
		|| document.form.wl_radio_date_x_Tue.checked == true
		|| document.form.wl_radio_date_x_Wed.checked == true
		|| document.form.wl_radio_date_x_Thu.checked == true
		|| document.form.wl_radio_date_x_Fri.checked == true){
			if(flag_week != 1 || flag_initial == 0){
				inputCtrl(document.form.wl_radio_time_x_starthour,1);
				inputCtrl(document.form.wl_radio_time_x_startmin,1);
				inputCtrl(document.form.wl_radio_time_x_endhour,1);
				inputCtrl(document.form.wl_radio_time_x_endmin,1);
				document.form.wl_radio_time_x.disabled = false;
				flag_week =1;
			}
	}
	else{
			if(flag_week != 0 || flag_initial == 0){
				inputCtrl(document.form.wl_radio_time_x_starthour,0);
				inputCtrl(document.form.wl_radio_time_x_startmin,0);
				inputCtrl(document.form.wl_radio_time_x_endhour,0);
				inputCtrl(document.form.wl_radio_time_x_endmin,0);
				document.form.wl_radio_time_x.disabled = true;
				$('enable_time_week_tr').style.display ="";
				flag_week = 0;
			}
	}
		
	if(document.form.wl_radio_date_x_Sun.checked == true || document.form.wl_radio_date_x_Sat.checked == true){
		if(flag_weekend != 1 || flag_initial == 0){
			inputCtrl(document.form.wl_radio_time2_x_starthour,1);
			inputCtrl(document.form.wl_radio_time2_x_startmin,1);
			inputCtrl(document.form.wl_radio_time2_x_endhour,1);
			inputCtrl(document.form.wl_radio_time2_x_endmin,1);
			document.form.wl_radio_time2_x.disabled = false;
			flag_weekend =1;
		}
	}
	else{
		if(flag_weekend != 0 || flag_initial == 0){
			inputCtrl(document.form.wl_radio_time2_x_starthour,0);
			inputCtrl(document.form.wl_radio_time2_x_startmin,0);
			inputCtrl(document.form.wl_radio_time2_x_endhour,0);
			inputCtrl(document.form.wl_radio_time2_x_endmin,0);
			document.form.wl_radio_time2_x.disabled = true;
			$("enable_time_weekend_tr").style.display = "";
			flag_weekend =0;
		}
	}
	flag_initial = 1;
}

function updateDateTime(){
	document.form.wl_radio_date_x.value = setDateCheck(
		document.form.wl_radio_date_x_Sun,
		document.form.wl_radio_date_x_Mon,
		document.form.wl_radio_date_x_Tue,
		document.form.wl_radio_date_x_Wed,
		document.form.wl_radio_date_x_Thu,
		document.form.wl_radio_date_x_Fri,
		document.form.wl_radio_date_x_Sat);
	document.form.wl_radio_time_x.value = setTimeRange(
		document.form.wl_radio_time_x_starthour,
		document.form.wl_radio_time_x_startmin,
		document.form.wl_radio_time_x_endhour,
		document.form.wl_radio_time_x_endmin);
	document.form.wl_radio_time2_x.value = setTimeRange(
		document.form.wl_radio_time2_x_starthour,
		document.form.wl_radio_time2_x_startmin,
		document.form.wl_radio_time2_x_endhour,
		document.form.wl_radio_time2_x_endmin);
}
function setFlag_TimeFiled(){
	if(document.form.wl_radio_date_x_Mon.checked == true 
		|| document.form.wl_radio_date_x_Tue.checked == true
		|| document.form.wl_radio_date_x_Wed.checked == true
		|| document.form.wl_radio_date_x_Thu.checked == true
		|| document.form.wl_radio_date_x_Fri.checked == true){
			flag_week = 1;
		}
	else{
			flag_week = 0;
	}

	if(document.form.wl_radio_date_x_Sun.checked == true || document.form.wl_radio_date_x_Sat.checked == true){
		flag_weekend = 1;
	}
	else{
		flag_weekend = 0;
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
		$('ampdu_rts_tr').style.display = "";
		if(document.form.wl_ampdu_rts.value == 1){
			document.form.wl_rts.disabled = false;
			$('rts_threshold').style.display = "";
		}	
		else{
			document.form.wl_rts.disabled = true;
			$('rts_threshold').style.display = "none";
		}	
	}
	else{
		document.form.wl_ampdu_rts.disabled = true;
		$('ampdu_rts_tr').style.display = "none";
	}
}

function register_event(){
	$j(function() {
		$j( "#slider" ).slider({
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
	
	$('slider').children[0].style.width = power_value + "%";
	$('slider').children[1].style.left = power_value + "%";
	document.form.wl_txpower.value = power_value;
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
<input type="hidden" name="wl_radio_date_x" value="<% nvram_get("wl_radio_date_x"); %>">
<input type="hidden" name="wl_radio_time_x" value="<% nvram_get("wl_radio_time_x"); %>">
<input type="hidden" name="wl_radio_time2_x" value="<% nvram_get("wl_radio_time2_x"); %>">
<input type="hidden" name="wl_subunit" value="-1">
<input type="hidden" name="wl_amsdu" value="<% nvram_get("wl_amsdu"); %>">
<input type="hidden" name="wl0_country_code" value="<% nvram_get("wl0_country_code"); %>" disabled>
<input type="hidden" name="wl_HW_switch" value="<% nvram_get("wl_HW_switch"); %>" disabled>
<input type="hidden" name="wl_TxPower" value="<% nvram_get("wl_TxPower"); %>" >
<input type="hidden" name="wl1_80211h_orig" value="<% nvram_get("wl1_80211h"); %>" >
<input type="hidden" name="acs_dfs" value="<% nvram_get("acs_dfs"); %>">
<input type="hidden" name="w_Setting" value="1">

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
		 				<div class="formfontdesc"><#WLANConfig11b_display5_sectiondesc#></div>
		 				<div id="svc_hint_div" style="display:none;margin-left:5px;"><span onClick="location.href='Advanced_System_Content.asp?af=ntp_server0'" style="color:#FFCC00;text-decoration:underline;cursor:pointer;"><#General_x_SystemTime_syncNTP#></span></div>
		  			<div id="timezone_hint_div" style="margin-left:5px;display:none;"><span id="timezone_hint" onclick="location.href='Advanced_System_Content.asp?af=time_zone_select'" style="color:#FFCC00;text-decoration:underline;cursor:pointer;"></span></div>	

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
			  				<input type="radio" value="1" name="wl_radio" class="input" onClick="control_TimeField(1);" <% nvram_match("wl_radio", "1", "checked"); %>><#checkbox_Yes#>
			    			<input type="radio" value="0" name="wl_radio" class="input" onClick="control_TimeField(0);" <% nvram_match("wl_radio", "0", "checked"); %>><#checkbox_No#>
			  			</td>
					</tr>

					<tr id="wl_sched_enable">
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 23);"><#WLANConfig11b_x_SchedEnable_itemname#></a></th>
			  			<td>
			  				<input type="radio" value="1" name="wl_timesched" class="input" onClick="control_TimeField();return change_common_radio(this, 'WLANConfig11b', 'wl_timesched', '1');" <% nvram_match("wl_timesched", "1", "checked"); %>><#checkbox_Yes#>
			    			<input type="radio" value="0" name="wl_timesched" class="input" onClick="control_TimeField();return change_common_radio(this, 'WLANConfig11b', 'wl_timesched', '0')" <% nvram_match("wl_timesched", "0", "checked"); %>><#checkbox_No#>
			  			</td>
					</tr>

					<tr id="enable_date_week_tr">
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 2);"><#WLANConfig11b_x_RadioEnableDate_itemname#> (week days)</a></th>
			  			<td>
								
							<input type="checkbox" class="input" name="wl_radio_date_x_Mon" onclick="check_Timefield_checkbox()"><#date_Mon_itemdesc#>
							<input type="checkbox" class="input" name="wl_radio_date_x_Tue" onclick="check_Timefield_checkbox()"><#date_Tue_itemdesc#>
							<input type="checkbox" class="input" name="wl_radio_date_x_Wed" onclick="check_Timefield_checkbox()"><#date_Wed_itemdesc#>
							<input type="checkbox" class="input" name="wl_radio_date_x_Thu" onclick="check_Timefield_checkbox()"><#date_Thu_itemdesc#>
							<input type="checkbox" class="input" name="wl_radio_date_x_Fri" onclick="check_Timefield_checkbox()"><#date_Fri_itemdesc#>						
							<span id="blank_warn" style="display:none;"><#JS_Shareblanktest#></span>	
			  			</td>
					</tr>
					<tr id="enable_time_week_tr" >
			  			<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(3, 3);"><#WLANConfig11b_x_RadioEnableTime_itemname#></a></th>
			  			<td>
			  				<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time_x_starthour" onKeyPress="return validator.isNumber(this,event)" onblur="validator.timeRange(this, 0);" > :
							<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time_x_startmin" onKeyPress="return validator.isNumber(this,event)" onblur="validator.timeRange(this, 1);"> -
							<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time_x_endhour" onKeyPress="return validator.isNumber(this,event)" onblur="validator.timeRange(this, 2);"> :
							<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time_x_endmin" onKeyPress="return validator.isNumber(this,event)" onblur="validator.timeRange(this, 3);">
						</td>
					</tr>
					<tr id="enable_date_weekend_tr">
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 2);"><#WLANConfig11b_x_RadioEnableDate_itemname#> (weekend)</a></th>
			  			<td>
							<input type="checkbox" class="input" name="wl_radio_date_x_Sat" onclick="check_Timefield_checkbox()"><#date_Sat_itemdesc#>
							<input type="checkbox" class="input" name="wl_radio_date_x_Sun" onclick="check_Timefield_checkbox()"><#date_Sun_itemdesc#>					
							<span id="blank_warn" style="display:none;"><#JS_Shareblanktest#></span>	
			  			</td>
					</tr>
					<tr id="enable_time_weekend_tr">
			  			<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(3, 3);"><#WLANConfig11b_x_RadioEnableTime_itemname#></a></th>
			  			<td>
			  				<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time2_x_starthour" onKeyPress="return validator.isNumber(this,event)" onblur="validator.timeRange(this, 0);"> :
							<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time2_x_startmin" onKeyPress="return validator.isNumber(this,event)" onblur="validator.timeRange(this, 1);"> -
							<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time2_x_endhour" onKeyPress="return validator.isNumber(this,event)" onblur="validator.timeRange(this, 2);"> :
							<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time2_x_endmin" onKeyPress="return validator.isNumber(this,event)" onblur="validator.timeRange(this, 3);">
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
								Disconnect clients with RSSI lower than
			  				<input type="text" maxlength="3" name="wl_user_rssi" class="input_3_table" value="<% nvram_get("wl_user_rssi"); %>">
								dB
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
				  				<option value="default" <% nvram_match("wl_rateset", "default","selected"); %>>Default</option>
				  				<option value="all" <% nvram_match("wl_rateset", "all","selected"); %>>All</option>
				  				<option value="12" <% nvram_match("wl_rateset", "12","selected"); %>>1, 2 Mbps</option>
							</select>
						</td>
					</tr>
					<tr id="wl_plcphdr_field">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,20);"><#WLANConfig11n_PremblesType_itemname#></a></th>
						<td>
						<select name="wl_plcphdr" class="input_option">
							<option value="long" <% nvram_match("wl_plcphdr", "long", "selected"); %>>Long</option>
							<option value="short" <% nvram_match("wl_plcphdr", "short", "selected"); %>>Short</option>
<!-- auto mode applicable for STA only
							<option value="auto" <% nvram_match("wl_plcphdr", "auto", "selected"); %>><#Auto#></option>
-->
						</select>
						</td>
					</tr>
					<tr>
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 9);"><#WLANConfig11b_x_Frag_itemname#></a></th>
			  			<td>
			  				<input type="text" maxlength="4" name="wl_frag" id="wl_frag" class="input_6_table" value="<% nvram_get("wl_frag"); %>" onKeyPress="return validator.isNumber(this,event)">
						</td>
					</tr>
					<tr id='ampdu_rts_tr'>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,30);">AMPDU RTS</a></th>
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
			  				<input type="text" maxlength="4" name="wl_rts" class="input_6_table" value="<% nvram_get("wl_rts"); %>" onKeyPress="return validator.isNumber(this,event)">
			  			</td>
					</tr>
					<tr id="wl_dtim_field">
			  			<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(3, 11);"><#WLANConfig11b_x_DTIM_itemname#></a></th>
						<td>
			  				<input type="text" maxlength="3" name="wl_dtim" class="input_6_table" value="<% nvram_get("wl_dtim"); %>" onKeyPress="return validator.isNumber(this,event)">
						</td>			  
					</tr>
					<tr id="wl_bcn_field">
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 12);"><#WLANConfig11b_x_Beacon_itemname#></a></th>
						<td>
							<input type="text" maxlength="4" name="wl_bcn" class="input_6_table" value="<% nvram_get("wl_bcn"); %>" onKeyPress="return validator.isNumber(this,event)">
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

					<tr> <!-- MODELDEP: RT-AC3200 / RT-AC68U / RT-AC68U_V2 / RT-AC69U / TM-AC1900 / DSL-AC68U Only  -->
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,29);"><#WLANConfig11b_x_ReduceUSB3#></a></th>
						<td>
							<select name="usb_usb3" class="input_option">
								<option value="1" <% nvram_match("usb_usb3", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								<option value="0" <% nvram_match("usb_usb3", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>
					
					<tr> <!-- MODELDEP: RT-AC55U -->
						<th><a class="hintstyle" href="javascript:void(0);"">Enable accurate traffic counter</a></th>
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
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,28);"><#WLANConfig11b_x_TurboQAM#></a></th>
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
						<th><a class="hintstyle" href="javascript:void(0);" onClick="">IEEE 802.11h support</a></th>
						<td>
							<select name="wl1_80211h" class="input_option">
									<option value="0" <% nvram_match("wl1_80211h", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
									<option value="1" <% nvram_match("wl1_80211h", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>
					
					
					<!--Broadcom ARM platform only, except RT-AC87U(5G) -->
					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,32);">Airtime Fairness</a></th>
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

					<tr id="wl_regmode_field">
						<th>Regulation mode</th>
						<td>
							<select name="wl_reg_mode" class="input_option">
									<option value="off" <% nvram_match("wl_reg_mode", "off","selected"); %> >Off (default)</option>
									<option value="d" <% nvram_match("wl_reg_mode", "d","selected"); %> >802.11d</option>
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
											<input id="wl_txpower" name="wl_txpower" type="text" maxlength="3" class="input_3_table" value="<% nvram_get("wl_txpower"); %>" style="margin-left:-10px;" onkeyup="set_power(this.value);"> %
										</td>					
										<td style="border:0px;">
											<div id="slider" style="width:200px;"></div>
										</td>
									</tr>
								</table>
							</div>
						</td>
					</tr>
				</table>
					
						<div class="apply_gen">
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
