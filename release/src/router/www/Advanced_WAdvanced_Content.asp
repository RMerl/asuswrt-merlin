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
<title><#Web_Title#> - <#menu5_1_6#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
var flag = 0;
var based_modelid = '<% nvram_get("productid"); %>';
var hw_modelid = '<% nvram_get("hardware_version"); %>';

<% login_state_hook(); %>
<% wl_get_parameter(); %>

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var mcast_rates = [
	["HTMIX 6.5/15", "14", 0, 1],
	["HTMIX 13/30",	 "15", 0, 1],
	["HTMIX 19.5/45","16", 0, 1],
	["HTMIX 13/30",	 "17", 0, 1],
	["HTMIX 26/60",	 "18", 0, 1],
	["HTMIX 130/144","13", 0, 1],
	["OFDM 6",	 "4",  0, 0],
	["OFDM 9",	 "5",  0, 0],
	["OFDM 12",	 "7",  0, 0],
	["OFDM 18",	 "8",  0, 0],
	["OFDM 24",	 "9",  0, 0],
	["OFDM 36",	 "10", 0, 0],
	["OFDM 48",	 "11", 0, 0],
	["OFDM 54",	 "12", 0, 0],
	["CCK 1",	 "1",  1, 0],
	["CCK 2",	 "2",  1, 0],
	["CCK 5.5",	 "3",  1, 0],
	["CCK 11",	 "6",  1, 0]
];

function initial(){
	show_menu();
	load_body();
	
	if(sw_mode == "2"){
		disableAdvFn(17);
		change_common(document.form.wl_wme, "WLANConfig11b", "wl_wme");
	}

	$("wl_rate").style.display = "none";
	if(wifi_hw_sw_support != -1) {
		$("wl_rf_enable").style.display = "none";	
	}
	
	if(band5g_support == -1){	
		$("wl_unit_field").style.display = "none";
	}	

	if(Rawifi_support == -1){ // BRCM == without rawifi
		$("DLSCapable").style.display = "none";	
		$("PktAggregate").style.display = "none";
		$("enable_wl_multicast_forward").style.display = "";
		if('<% nvram_get("wl_unit"); %>' == '1'){
			inputCtrl(document.form.wl_noisemitigation, 0);
		}
	}else{
		inputCtrl(document.form.wl_noisemitigation, 0);
	}	

	var mcast_rate = '<% nvram_get("wl_mrate_x"); %>';
	var mcast_unit = '<% nvram_get("wl_unit"); %>';
	//free_options(document.form.wl_mrate_x);
	for (var i = 0; i < mcast_rates.length; i++) {
		if (mcast_unit == '1' && mcast_rates[i][2]) // 5Ghz && CCK
			continue;
		if (Rawifi_support == -1 && mcast_rates[i][3]) // BCM && HTMIX
			continue;
		add_option(document.form.wl_mrate_x,
			mcast_rates[i][0], mcast_rates[i][1],
			(mcast_rate == mcast_rates[i][1]) ? 1 : 0);
	}

	if(repeater_support != -1){		//with RE mode
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
	
	if(power_support < 0)
		inputHideCtrl(document.form.wl_TxPower, 0);

	check_Timefield_checkbox();
	control_TimeField();

	if(document.form.wl0_country_code.value == "EU" && document.form.wl_unit.value == 0){
		$("maxTxPower").innerHTML = "100";
	}
}

function applyRule(){
	if(validForm()){
		showLoading();
		document.form.submit();
	}
}

var errFlag=0
function validForm(){
	if(sw_mode != "2"){
		if(!validate_range(document.form.wl_frag, 256, 2346)
				|| !validate_range(document.form.wl_rts, 0, 2347)
				|| !validate_range(document.form.wl_dtim, 1, 255)
				|| !validate_range(document.form.wl_bcn, 20, 1000)
				){
			return false;
		}	
	}
	
	if(!validate_timerange(document.form.wl_radio_time_x_starthour, 0) || !validate_timerange(document.form.wl_radio_time2_x_starthour, 0)
			|| !validate_timerange(document.form.wl_radio_time_x_startmin, 1) || !validate_timerange(document.form.wl_radio_time2_x_startmin, 1)
			|| !validate_timerange(document.form.wl_radio_time_x_endhour, 2) || !validate_timerange(document.form.wl_radio_time2_x_endhour, 2)
			|| !validate_timerange(document.form.wl_radio_time_x_endmin, 3) || !validate_timerange(document.form.wl_radio_time2_x_endmin, 3)
			)
		return false;
	
	if(document.form.wl_radio[0].checked == true 
			&& document.form.wl_radio_date_x_Sun.checked == false
			&& document.form.wl_radio_date_x_Mon.checked == false
			&& document.form.wl_radio_date_x_Tue.checked == false
			&& document.form.wl_radio_date_x_Wed.checked == false
			&& document.form.wl_radio_date_x_Thu.checked == false
			&& document.form.wl_radio_date_x_Fri.checked == false
			&& document.form.wl_radio_date_x_Sat.checked == false){
				document.form.wl_radio_date_x_Sun.focus();
				$('blank_warn').style.display = "";
				return false;
	}
		
	if(power_support != -1){		
		// CE@2.4GHz
		if(document.form.wl0_country_code.value == "EU" && document.form.wl_unit.value == 0){
			if(document.form.wl_TxPower.value > 100 && errFlag < 2){
				alert("Due to CE regulation, the value of TxPower cannot over 100.")
				document.form.wl_TxPower.focus();
				errFlag++;
				return false;
			}
			else if(document.form.wl_TxPower.value > 200 && errFlag > 1)
				document.form.wl_TxPower.value = 200;
		}

		if(!validate_range(document.form.wl_TxPower, 1, 200)){
			document.form.wl_TxPower.value = 200;
			document.form.wl_TxPower.focus();
			document.form.wl_TxPower.select();
			return false;
		}

		// MODELDEP
		if(based_modelid == "RT-N12HP" || (based_modelid == "RT-N12" && hw_modelid == "RTN12HP-1.0.1.2")){
		  FormActions("start_apply.htm", "apply", "set_wltxpower;reboot", "<% get_default_reboot_time(); %>");
		}
		else if(based_modelid == "RT-AC66U" || based_modelid == "RT-N66U"){
			FormActions("start_apply.htm", "apply", "set_wltxpower;restart_wireless", "15");
		}
  }		
	
	updateDateTime(document.form.current_page.value);	
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
function check_Timefield_checkbox(){	// To check Date checkbox checked or not and control Time field disabled or not, Jieming add at 2012/10/05
	if(document.form.wl_radio_date_x_Mon.checked == true 
		|| document.form.wl_radio_date_x_Tue.checked == true
		|| document.form.wl_radio_date_x_Wed.checked == true
		|| document.form.wl_radio_date_x_Thu.checked == true
		|| document.form.wl_radio_date_x_Fri.checked == true	){		
			inputCtrl(document.form.wl_radio_time_x_starthour,1);
			inputCtrl(document.form.wl_radio_time_x_startmin,1);
			inputCtrl(document.form.wl_radio_time_x_endhour,1);
			inputCtrl(document.form.wl_radio_time_x_endmin,1);
			document.form.wl_radio_time_x.disabled = false;
	}
	else{
			inputCtrl(document.form.wl_radio_time_x_starthour,0);
			inputCtrl(document.form.wl_radio_time_x_startmin,0);
			inputCtrl(document.form.wl_radio_time_x_endhour,0);
			inputCtrl(document.form.wl_radio_time_x_endmin,0);
			document.form.wl_radio_time_x.disabled = true;
			$('enable_time_week_tr').style.display ="";
	}
		
	if(document.form.wl_radio_date_x_Sun.checked == true || document.form.wl_radio_date_x_Sat.checked == true){
		inputCtrl(document.form.wl_radio_time2_x_starthour,1);
		inputCtrl(document.form.wl_radio_time2_x_startmin,1);
		inputCtrl(document.form.wl_radio_time2_x_endhour,1);
		inputCtrl(document.form.wl_radio_time2_x_endmin,1);
		document.form.wl_radio_time2_x.disabled = false;
	}
	else{
		inputCtrl(document.form.wl_radio_time2_x_starthour,0);
		inputCtrl(document.form.wl_radio_time2_x_startmin,0);
		inputCtrl(document.form.wl_radio_time2_x_endhour,0);
		inputCtrl(document.form.wl_radio_time2_x_endmin,0);
		document.form.wl_radio_time2_x.disabled = true;
		$("enable_time_weekend_tr").style.display = ""; 
	}
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="wan_route_x" value="<% nvram_get("wan_route_x"); %>">
<input type="hidden" name="wan_nat_x" value="<% nvram_get("wan_nat_x"); %>">

<input type="hidden" name="wl_nmode_x" value="<% nvram_get("wl_nmode_x"); %>">
<input type="hidden" name="wl_gmode_protection_x" value="<% nvram_get("wl_gmode_protection_x"); %>">

<input type="hidden" name="current_page" value="Advanced_WAdvanced_Content.asp">
<input type="hidden" name="next_page" value="SaveRestart.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_radio_date_x" value="<% nvram_get("wl_radio_date_x"); %>">
<input type="hidden" name="wl_radio_time_x" value="<% nvram_get("wl_radio_time_x"); %>">
<input type="hidden" name="wl_radio_time2_x" value="<% nvram_get("wl_radio_time2_x"); %>">
<input type="hidden" name="wl_subunit" value="-1">
<input type="hidden" name="wl_amsdu" value="<% nvram_get("wl_amsdu"); %>">
<input type="hidden" name="wl_TxPower_orig" value="<% nvram_get("wl_TxPower"); %>" disabled>
<input type="hidden" name="wl0_country_code" value="<% nvram_get("wl0_country_code"); %>" disabled>

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

					<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable" id="WAdvTable">	

					<tr id="wl_unit_field">
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
			  				<input type="radio" value="1" name="wl_radio" class="input" onClick="control_TimeField();return change_common_radio(this, 'WLANConfig11b', 'wl_radio', '1');" <% nvram_match("wl_radio", "1", "checked"); %>><#checkbox_Yes#>
			    			<input type="radio" value="0" name="wl_radio" class="input" onClick="control_TimeField();return change_common_radio(this, 'WLANConfig11b', 'wl_radio', '0')" <% nvram_match("wl_radio", "0", "checked"); %>><#checkbox_No#>
			  			</td>
					</tr>

					<tr id="enable_date_week_tr">
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 2);"><#WLANConfig11b_x_RadioEnableDate_itemname#> (week days)</a></th>
			  			<td>
							<input type="checkbox" class="input" name="wl_radio_date_x_Mon" onChange="return changeDate();" onclick="check_Timefield_checkbox()"><#date_Mon_itemdesc#>
							<input type="checkbox" class="input" name="wl_radio_date_x_Tue" onChange="return changeDate();" onclick="check_Timefield_checkbox()"><#date_Tue_itemdesc#>
							<input type="checkbox" class="input" name="wl_radio_date_x_Wed" onChange="return changeDate();" onclick="check_Timefield_checkbox()"><#date_Wed_itemdesc#>
							<input type="checkbox" class="input" name="wl_radio_date_x_Thu" onChange="return changeDate();" onclick="check_Timefield_checkbox()"><#date_Thu_itemdesc#>
							<input type="checkbox" class="input" name="wl_radio_date_x_Fri" onChange="return changeDate();" onclick="check_Timefield_checkbox()"><#date_Fri_itemdesc#>						
							<span id="blank_warn" style="display:none;"><#JS_Shareblanktest#></span>	
			  			</td>
					</tr>
					<tr id="enable_time_week_tr" >
			  			<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(3, 3);"><#WLANConfig11b_x_RadioEnableTime_itemname#></a></th>
			  			<td>
			  				<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time_x_starthour" onKeyPress="return is_number(this,event)" onblur="validate_timerange(this, 0);" > :
							<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time_x_startmin" onKeyPress="return is_number(this,event)" onblur="validate_timerange(this, 1);"> -
							<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time_x_endhour" onKeyPress="return is_number(this,event)" onblur="validate_timerange(this, 2);"> :
							<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time_x_endmin" onKeyPress="return is_number(this,event)" onblur="validate_timerange(this, 3);">
						</td>
					</tr>
					<tr id="enable_date_weekend_tr">
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 2);"><#WLANConfig11b_x_RadioEnableDate_itemname#> (weekend)</a></th>
			  			<td>
							<input type="checkbox" class="input" name="wl_radio_date_x_Sat" onChange="return changeDate();" onclick="check_Timefield_checkbox()"><#date_Sat_itemdesc#>
							<input type="checkbox" class="input" name="wl_radio_date_x_Sun" onChange="return changeDate();" onclick="check_Timefield_checkbox()"><#date_Sun_itemdesc#>					
							<span id="blank_warn" style="display:none;"><#JS_Shareblanktest#></span>	
			  			</td>
					</tr>
					<tr id="enable_time_weekend_tr">
			  			<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(3, 3);"><#WLANConfig11b_x_RadioEnableTime_itemname#></a></th>
			  			<td>
			  				<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time2_x_starthour" onKeyPress="return is_number(this,event)" onblur="validate_timerange(this, 0);"> :
							<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time2_x_startmin" onKeyPress="return is_number(this,event)" onblur="validate_timerange(this, 1);"> -
							<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time2_x_endhour" onKeyPress="return is_number(this,event)" onblur="validate_timerange(this, 2);"> :
							<input type="text" maxlength="2" class="input_3_table" name="wl_radio_time2_x_endmin" onKeyPress="return is_number(this,event)" onblur="validate_timerange(this, 3);">
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
							<select name="wl_rate" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_rate')">
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
					<tr id="wl_mrate_select">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 7);"><#WLANConfig11b_MultiRateAll_itemname#></a></th>
						<td>
							<select name="wl_mrate_x" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_mrate_x')">
								<option value="0" <% nvram_match("wl_mrate_x", "0", "selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
							</select>
						</td>
					</tr>
					<tr style="display:none;">
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 8);"><#WLANConfig11b_DataRate_itemname#></a></th>
			  			<td>
			  				<select name="wl_rateset" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_rateset')">
				  				<option value="default" <% nvram_match("wl_rateset", "default","selected"); %>>Default</option>
				  				<option value="all" <% nvram_match("wl_rateset", "all","selected"); %>>All</option>
				  				<option value="12" <% nvram_match("wl_rateset", "12","selected"); %>>1, 2 Mbps</option>
							</select>
						</td>
					</tr>
					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,20);"><#WLANConfig11n_PremblesType_itemname#></a></th>
						<td>
						<select name="wl_plcphdr" class="input_option" onchange="return change_common(this, 'WLANConfig11b', 'wl_plcphdr')">
							<option value="long" <% nvram_match("wl_plcphdr", "long", "selected"); %>>Long</option>
							<option value="short" <% nvram_match("wl_plcphdr", "short", "selected"); %>>Short</option>
							<option value="auto" <% nvram_match("wl_plcphdr", "auto", "selected"); %>><#Auto#></option>
						</select>
						</td>
					</tr>
					<tr>
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 9);"><#WLANConfig11b_x_Frag_itemname#></a></th>
			  			<td>
			  				<input type="text" maxlength="4" name="wl_frag" id="wl_frag" class="input_6_table" value="<% nvram_get("wl_frag"); %>" onKeyPress="return is_number(this,event)" onChange="page_changed()">
						</td>
					</tr>
					<tr>
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 10);"><#WLANConfig11b_x_RTS_itemname#></a></th>
			  			<td>
			  				<input type="text" maxlength="4" name="wl_rts" class="input_6_table" value="<% nvram_get("wl_rts"); %>" onKeyPress="return is_number(this,event)">
			  			</td>
					</tr>
					<tr>
			  			<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(3, 11);"><#WLANConfig11b_x_DTIM_itemname#></a></th>
						<td>
			  				<input type="text" maxlength="3" name="wl_dtim" class="input_6_table" value="<% nvram_get("wl_dtim"); %>" onKeyPress="return is_number(this,event)">
						</td>			  
					</tr>
					<tr>
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 12);"><#WLANConfig11b_x_Beacon_itemname#></a></th>
						<td>
							<input type="text" maxlength="4" name="wl_bcn" class="input_6_table" value="<% nvram_get("wl_bcn"); %>" onKeyPress="return is_number(this,event)">
						</td>
					</tr>
					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 13);"><#WLANConfig11b_x_TxBurst_itemname#></a></th>
						<td>
							<select name="wl_frameburst" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_frameburst')">
								<option value="off" <% nvram_match("wl_frameburst", "off","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								<option value="on" <% nvram_match("wl_frameburst", "on","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>
					<tr id="PktAggregate"><!-- RaLink Only -->
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 16);"><#WLANConfig11b_x_PktAggregate_itemname#></a></th>
						<td>
							<select name="wl_PktAggregate" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_PktAggregate')">
								<option value="0" <% nvram_match("wl_PktAggregate", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								<option value="1" <% nvram_match("wl_PktAggregate", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>

			<!--Greenfield by Lock Add in 2008.10.01 -->
					<!-- RaLink Only tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 19);"><#WLANConfig11b_x_HT_OpMode_itemname#></a></th>
						<td>
							<select id="wl_HT_OpMode" class="input_option" name="wl_HT_OpMode" onChange="return change_common(this, 'WLANConfig11b', 'wl_HT_OpMode')">
								<option value="0" <% nvram_match("wl_HT_OpMode", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								<option value="1" <% nvram_match("wl_HT_OpMode", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
							</div>
						</td>
					</tr-->
			<!--Greenfield by Lock Add in 2008.10.01 -->
					<!-- WMM setting start  -->
					<tr>
			  			<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(3, 14);"><#WLANConfig11b_x_WMM_itemname#></a></th>
			  			<td>
							<select name="wl_wme" id="wl_wme" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_wme')">			  	  				
			  	  				<option value="auto" <% nvram_match("wl_wme", "auto", "selected"); %>><#Auto#></option>
			  	  				<option value="on" <% nvram_match("wl_wme", "on", "selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
			  	  				<option value="off" <% nvram_match("wl_wme", "off", "selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>			  	  			
							</select>
			  			</td>
					</tr>
					<tr>
			  			<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,15);"><#WLANConfig11b_x_NOACK_itemname#></a></th>
			  			<td>
							<select name="wl_wme_no_ack" id="wl_wme_no_ack" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_wme_no_ack')">
			  	  				<option value="off" <% nvram_match("wl_wme_no_ack", "off","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
			  	  				<option value="on" <% nvram_match("wl_wme_no_ack", "on","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
			  			</td>
					</tr>

					<tr id="enable_wl_multicast_forward" style="display:none;">
						<th>Wireless Multicast Forwarding</th>
						<td>
                  				<select name="wl_wmf_bss_enable" class="input_option">
                    					<option value="0" <% nvram_match("wl_wmf_bss_enable", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
                    					<option value="1" <% nvram_match("wl_wmf_bss_enable", "1","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
                  				</select>
						</td>
					</tr>

					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,17);"><#WLANConfig11b_x_APSD_itemname#></a></th>
						<td>
                  				<select name="wl_wme_apsd" class="input_option" onchange="return change_common(this, 'WLANConfig11b', 'wl_wme_apsd')">
                    					<option value="off" <% nvram_match("wl_wme_apsd", "off","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
                    					<option value="on" <% nvram_match("wl_wme_apsd", "on","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
                  				</select>
						</td>
					</tr>					
					<!-- WMM setting end  -->

					<tr id="DLSCapable"> <!-- RaLink Only  -->
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,18);"><#WLANConfig11b_x_DLS_itemname#></a></th>
						<td>
							<select name="wl_DLSCapable" class="input_option" onChange="return change_common(this, 'WLANConfig11b', 'wl_DLSCapable')">
								<option value="0" <% nvram_match("wl_DLSCapable", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								<option value="1" <% nvram_match("wl_DLSCapable", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>

					<tr id="noiseReduction"> <!-- BRCM Only  -->
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,21);">Enhanced interference management</a></th>
						<td>
							<select name="wl_noisemitigation" class="input_option" onChange="">
								<option value="0" <% nvram_match("wl_noisemitigation", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								<option value="1" <% nvram_match("wl_noisemitigation", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>

					<!-- RaLink Only : Original at wireless-General page By Viz 2011.08 -->
					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 17);"><#WLANConfig11b_TxPower_itemname#></a></th>
						<td>
		  				<input type="text" maxlength="3" name="wl_TxPower" class="input_3_table" value="<% nvram_get("wl_TxPower"); %>" onKeyPress="return is_number(this, event);"> mW
							<br><span style="">Set the capability for transmission power. The maximum value is <span id="maxTxPower">200</span>mW and the real transmission power  will be dynamically adjusted to meet regional regulations</span>
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
