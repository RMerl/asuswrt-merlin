<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7" />
<meta name="svg.render.forceflash" content="false" />	
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - Performance tuning</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script src='svg.js' data-path="/svghtc/" data-debug="false"></script>	
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type='text/javascript'>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
var fanctrl_info = <% get_fanctrl_info(); %>;
var curr_rxData = fanctrl_info[3];
var curr_coreTmp_2 = convertTemp(fanctrl_info[1], fanctrl_info[2], 0);
//var curr_coreTmp_2 = fanctrl_info[1];
var curr_coreTmp_5 = fanctrl_info[2];
var coreTmp_2 = new Array();
var coreTmp_5 = new Array();
coreTmp_2 = [curr_coreTmp_2];
coreTmp_5 = [curr_coreTmp_5];
var wl_control_channel = <% wl_control_channel(); %>;
var $j = jQuery.noConflict();
var MaxTxPower_2;
var MaxTxPower_5;
var flag = 0;;
var HW_MAX_LIMITATION_2 = 501;
var HW_MIN_LIMITATION_2 = 9;
var HW_MAX_LIMITATION_5 = 251;
var HW_MIN_LIMITATION_5 = 9;
var fanctrl_fullspeed_temp_orig = convertTemp('<% nvram_get("fanctrl_fullspeed_temp"); %>', 0, 0);
var fanctrl_period_temp_orig = convertTemp('<% nvram_get("fanctrl_period_temp"); %>', 0, 0);
var fanctrl_fullspeed_temp_orig_F = Math.round(fanctrl_fullspeed_temp_orig*9/5+32);
var fanctrl_period_temp_orig_F = Math.round(fanctrl_period_temp_orig*9/5+32);

function initial(){
	show_menu();
	update_coretmp();

	if(getCookie("CoreTmpUnit") == 1){
		$("unitDisplay1").innerHTML = "°F";
		$("unitDisplay2").innerHTML = "°F";
		document.form.fanctrl_fullspeed_temp.value = fanctrl_fullspeed_temp_orig_F;
		document.form.fanctrl_period_temp.value = fanctrl_period_temp_orig_F;
	}		
	else{
		$("unitDisplay1").innerHTML = "°C";
		$("unitDisplay2").innerHTML = "°C";
		document.form.fanctrl_fullspeed_temp.value = fanctrl_fullspeed_temp_orig;
		document.form.fanctrl_period_temp.value = fanctrl_period_temp_orig;
	}
	document.form.fanctrl_fullspeed_temp_unit.selectedIndex = getCookie("CoreTmpUnit");

	if(power_support < 0){
		inputHideCtrl(document.form.wl0_TxPower, 0);
		inputHideCtrl(document.form.wl1_TxPower, 0);
	}
}

function update_coretmp(e){
  $j.ajax({
    url: '/ajax_coretmp.asp',
    dataType: 'script', 
	
    error: function(xhr){
      update_coretmp();
    },
    success: function(response){
			updateNum(curr_coreTmp_2, curr_coreTmp_5);
			setTimeout("update_coretmp();", 5000);
		}    
  });
}

function convertTemp(__coreTmp_2, __coreTmp_5, _method){
	if(_method == 0)
		return parseInt(__coreTmp_2)*0.5+20;
	else
		return (parseInt(__coreTmp_2)-20)*2;
}

function updateNum(_coreTmp_2, _coreTmp_5){
	curr_coreTmp_2 = convertTemp(_coreTmp_2, _coreTmp_5, 0);

	if(document.form.fanctrl_fullspeed_temp_unit.value == 1){
		$("coreTemp_2").innerHTML = Math.round(_coreTmp_2*9/5+32) + " °F";
		$("coreTemp_5").innerHTML = Math.round(_coreTmp_5*9/5+32) + " °F";
	}
	else{
		$("coreTemp_2").innerHTML = _coreTmp_2 + " °C";
		$("coreTemp_5").innerHTML = _coreTmp_5 + " °C";
	}
}

function applyRule(){
	if(parseInt(document.form.wl0_TxPower.value) > HW_MAX_LIMITATION_2){
		$("TxPowerHint_2").style.display = "";
		document.form.wl0_TxPower.focus();
		return false;
	}

	var wlcountry = '<% nvram_get("wl0_country_code"); %>';
	if(wlcountry == 'US' || wlcountry == 'CN' || wlcountry == 'TW')
		HW_MAX_LIMITATION_5 = 501;
	else
		HW_MAX_LIMITATION_5 = 251;

	if(parseInt(document.form.wl1_TxPower.value) > HW_MAX_LIMITATION_5){
		$("TxPowerHint_5").style.display = "";
		document.form.wl1_TxPower.focus();
		return false;
	}
/*
	if(parseInt(document.form.wl0_TxPower.value) > 80 && flag < 2){
		$("TxPowerHint_2").style.display = "";
		document.form.wl0_TxPower.focus();
		flag++;
		return false;
	}
	else
*/
		$("TxPowerHint_2").style.display = "none";
/*
	if(parseInt(document.form.wl1_TxPower.value) > 80 && flag < 2){
		$("TxPowerHint_5").style.display = "";
		document.form.wl1_TxPower.focus();
		flag++;
		return false;
	}
	else
*/
		$("TxPowerHint_5").style.display = "none";

	if(parseInt(document.form.wl0_TxPower.value) > parseInt(document.form.wl0_TxPower_orig.value) 
		|| parseInt(document.form.wl1_TxPower.value) > parseInt(document.form.wl1_TxPower_orig.value))
	  FormActions("start_apply.htm", "apply", "set_wltxpower;reboot", "<% get_default_reboot_time(); %>");
	else{
		if(document.form.wl0_TxPower.value != document.form.wl0_TxPower_orig.value 
			|| document.form.wl1_TxPower.value != document.form.wl1_TxPower_orig.value)
			document.form.action_script.value = "restart_wireless";
	
		if(document.form.fanctrl_mode.value != document.form.fanctrl_mode_orig.value 
			|| document.form.fanctrl_fullspeed_temp.value != document.form.fanctrl_fullspeed_temp_orig.value
			|| document.form.fanctrl_period_temp.value != document.form.fanctrl_period_temp_orig.value
			|| document.form.fanctrl_dutycycle.value != document.form.fanctrl_dutycycle_orig.value){
			if(document.form.action_script.value != "")
				document.form.action_script.value += ";";
			document.form.action_script.value += "restart_fanctrl";
		}
	}
	
	if(parseInt(document.form.fanctrl_period_temp.value) > parseInt(document.form.fanctrl_fullspeed_temp.value)){
		alert("This value could not exceed "+document.form.fanctrl_fullspeed_temp.value);
		document.form.fanctrl_period_temp.focus();
		return false;
	}

	if(document.form.fanctrl_fullspeed_temp_unit.value == "1"){
		document.form.fanctrl_fullspeed_temp.value = Math.round((document.form.fanctrl_fullspeed_temp.value-32)*5/9);
		document.form.fanctrl_period_temp.value = Math.round((document.form.fanctrl_period_temp.value-32)*5/9);
	}
	
	/*if(validate_number_range(document.form.fanctrl_fullspeed_temp, 25, 70) 
		&& validate_number_range(document.form.fanctrl_period_temp, 25, 55)){
		document.form.fanctrl_fullspeed_temp.value = convertTemp(document.form.fanctrl_fullspeed_temp.value, 0, 1);
		document.form.fanctrl_period_temp.value = convertTemp(document.form.fanctrl_period_temp.value, 0, 1);
		Math.round(document.form.fanctrl_fullspeed_temp.value);
		Math.round(document.form.fanctrl_period_temp.value);
		showLoading();
		document.form.submit();
	}
	else{
		if(document.form.fanctrl_fullspeed_temp_unit.value == "1"){
			document.form.fanctrl_fullspeed_temp.value = fanctrl_fullspeed_temp_orig_F;
			document.form.fanctrl_period_temp.value = fanctrl_period_temp_orig_F;
		}
		else{
			document.form.fanctrl_fullspeed_temp.value = fanctrl_fullspeed_temp_orig;
			document.form.fanctrl_period_temp.value = fanctrl_period_temp_orig;
		}
	}*/
	showLoading();
	document.form.submit();
}

function changeTempUnit(num){
	setCookie(num);
	refreshpage();
}

function setCookie(num){
	document.cookie = "CoreTmpUnit=" + num;
}

function getCookie(c_name)
{
	if (document.cookie.length > 0){ 
		c_start=document.cookie.indexOf(c_name + "=")
		if (c_start!=-1){ 
			c_start=c_start + c_name.length+1 
			c_end=document.cookie.indexOf(";",c_start)
			if (c_end==-1) c_end=document.cookie.length
			return unescape(document.cookie.substring(c_start,c_end))
		} 
	}
	return null
}
</script>
</head>

<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
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
			<input type="hidden" name="current_page" value="Advanced_PerformanceTuning_Content.asp">
			<input type="hidden" name="next_page" value="Advanced_PerformanceTuning_Content.asp">
			<input type="hidden" name="next_host" value="">
			<input type="hidden" name="modified" value="0">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_wait" value="5">
			<input type="hidden" name="action_script" value="">
			<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
			<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
			<input type="hidden" name="wl_ssid" value="<% nvram_get("wl_ssid"); %>">
			<input type="hidden" name="wl0_TxPower_orig" value="<% nvram_get("wl0_TxPower"); %>" disabled>
			<input type="hidden" name="wl1_TxPower_orig" value="<% nvram_get("wl1_TxPower"); %>" disabled>
			<input type="hidden" name="fanctrl_mode_orig" value="<% nvram_get("fanctrl_mode"); %>" disabled>
			<input type="hidden" name="fanctrl_fullspeed_temp_orig" value="<% nvram_get("fanctrl_fullspeed_temp"); %>" disabled>
			<input type="hidden" name="fanctrl_period_temp_orig" value="<% nvram_get("fanctrl_period_temp"); %>" disabled>
			<input type="hidden" name="fanctrl_dutycycle_orig" value="<% nvram_get("fanctrl_dutycycle"); %>" disabled>

			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td valign="top" >
						<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
								<tr>
								  <td bgcolor="#4D595D" valign="top">
									  <div>&nbsp;</div>
									  <div class="formfonttitle"><#menu5_6_adv#> - Performance tuning</div>
									  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									  <div class="formfontdesc"><#PerformaceTuning_desc#></div>
									</td>
								</tr>

								<tr>
									<td bgcolor="#4D595D">
										<table width="99%" border="0" align="center" cellpadding="0" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
											<thead>
											<tr>
												<td colspan="2">Cooler status</td>
											</tr>
											</thead>
											
											<tr>
												<td>
													<div style="margin-left:-10px;">
														<!--========= svg =========-->
														<!--[if IE]>
															<div id="svg-table" align="left">
															<object id="graph" src="fan.svg" classid="image/svg+xml" width="740" height="300">
															</div>
														<![endif]-->
														<!--[if !IE]>-->
															<object id="graph" data="fan.svg" type="image/svg+xml" width="740" height="300">
														<!--<![endif]-->
															</object>
											 			<!--========= svg =========-->
													</div>
												</td>
											</tr>
										</table>
									</td>
					  		</tr>

								<tr style="display:none;">
									<td bgcolor="#4D595D">
						    	 	<table width="735px" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable_NWM">
								  		<tr>
								  			<th style="text-align:center; width:35%;height:25px;">2.4GHz</th>
								  			<th style="text-align:center; width:35%;">5GHz</th>
								  			<th style="text-align:center; width:30%;">Unit</th>
								  		</tr>
		
								  		<tr>
								  			<td style="text-align:center; background-color:#111;"><span id="coreTemp_2" style="font-weight:bold;color:#FF9000"></span></td>
								 				<td style="text-align:center; background-color:#111;"><span id="coreTemp_5" style="font-weight:bold;color:#33CCFF"></span></td>
								  			<td style="text-align:center; background-color:#111;">
													<!--select name="fanctrl_fullspeed_temp_unit" class="input_option" onchange="changeTempUnit(this.value)" style="background-color:#111;">
														<option class="content_input_fd" value="0">°C</option>
														<option class="content_input_fd" value="1">°F</option>
													</select-->			
												</td>
								    	</tr>
										</table>
									</td>
					  		</tr>

								<tr>
									<td bgcolor="#4D595D">
										<table width="99%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
											<thead>
											<tr>
												<td colspan="2">System adjustment</td>
											</tr>
											</thead>
											
											<tr>
												<th>2.4GHz Transmit radio power</th>
												<td>
													<input type="text" name="wl0_TxPower" maxlength="3" class="input_3_table" value="<% nvram_get("wl0_TxPower"); %>"> mW
													<span id="TxPowerHint_2" style="margin-left:10px;display:none;">This value could not exceed 80</span>
												</td>
											</tr>
				            
											<tr>
												<th>5GHz Transmit radio power</th>
												<td>
													<input type="text" name="wl1_TxPower" maxlength="3" class="input_3_table" value="<% nvram_get("wl1_TxPower"); %>"> mW
													<span id="TxPowerHint_5" style="margin-left:10px;display:none;">This value could not exceed 80</span>
												</td> 
											</tr>

											<tr style="display:none">
												<th>Cooler rotate mode</th>
												<td>
													<select name="fanctrl_mode" class="input_option">
														<option class="content_input_fd" value="0" <% nvram_match("fanctrl_mode", "0", "selected"); %>><#Automatic_cooler#></option>
														<option class="content_input_fd" value="1" <% nvram_match("fanctrl_mode", "1", "selected"); %>>Manually</option>
													</select>			
												</td>
											</tr>

											<tr>
												<th>Temperature unit</th>
												<td>
													<select name="fanctrl_fullspeed_temp_unit" class="input_option" onchange="changeTempUnit(this.value)">
														<option class="content_input_fd" value="0">°C</option>
														<option class="content_input_fd" value="1">°F</option>
													</select>			
												</td>
											</tr>
											
											<tr style="display:none">
												<th>Cooler full speed spin</th>
												<td>Temperature over
													<input type="text" name="fanctrl_fullspeed_temp" maxlength="3" class="input_3_table" value="<% nvram_get("fanctrl_fullspeed_temp"); %>">
													<span style="color:#FFF" id="unitDisplay1">°C</span>
												</td>
											</tr>

											<tr style="display:none">
												<th>Cooler periodically spin</th>
												<td>Temperature over
													<input type="text" name="fanctrl_period_temp" maxlength="3" class="input_3_table" value="<% nvram_get("fanctrl_period_temp"); %>">
													<span style="color:#FFF" id="unitDisplay2">°C</span>
												</td>
											</tr>
				            
											<!--tr>
												<th>Spin duty cycle</th>
												<td> 
													<select name="fanctrl_dutycycle" class="input_option">
														<option class="content_input_fd" value="0" <% nvram_match("fanctrl_dutycycle", "1", "selected"); %>>Auto</option>
														<option class="content_input_fd" value="1" <% nvram_match("fanctrl_dutycycle", "1", "selected"); %>>10%</option>
														<option class="content_input_fd" value="2" <% nvram_match("fanctrl_dutycycle", "2", "selected"); %>>20%</option>
														<option class="content_input_fd" value="3" <% nvram_match("fanctrl_dutycycle", "3", "selected"); %>>30%</option>
														<option class="content_input_fd" value="4" <% nvram_match("fanctrl_dutycycle", "4", "selected"); %>>40%</option>
														<option class="content_input_fd" value="5" <% nvram_match("fanctrl_dutycycle", "5", "selected"); %>>50%</option>
														<option class="content_input_fd" value="6" <% nvram_match("fanctrl_dutycycle", "6", "selected"); %>>60%</option>
														<option class="content_input_fd" value="7" <% nvram_match("fanctrl_dutycycle", "7", "selected"); %>>70%</option>
														<option class="content_input_fd" value="8" <% nvram_match("fanctrl_dutycycle", "8", "selected"); %>>80%</option>
														<option class="content_input_fd" value="9" <% nvram_match("fanctrl_dutycycle", "9", "selected"); %>>90%</option>
														<option class="content_input_fd" value="10" <% nvram_match("fanctrl_dutycycle", "10", "selected"); %>>100%</option>
													</select>										
												</td> 
											</tr-->
											<tr-->
												<th>Spin duty cycle</th>
												<td> 
													<select name="fanctrl_dutycycle" class="input_option">
														<option class="content_input_fd" value="0" <% nvram_match("fanctrl_dutycycle", "1", "selected"); %>><#Auto#></option>
														<option class="content_input_fd" value="1" <% nvram_match("fanctrl_dutycycle", "1", "selected"); %>>50%</option>
														<option class="content_input_fd" value="2" <% nvram_match("fanctrl_dutycycle", "2", "selected"); %>>67%</option>
														<option class="content_input_fd" value="3" <% nvram_match("fanctrl_dutycycle", "3", "selected"); %>>75%</option>
														<option class="content_input_fd" value="4" <% nvram_match("fanctrl_dutycycle", "4", "selected"); %>>80%</option>
													</select>										
												</td> 
											</tr-->
										</table>
										<div class="apply_gen">
											<input class="button_gen" onclick="applyRule();" type="button" value="<#CTL_apply#>"/>
										</div>
									</td>
					  		</tr>
							</tbody>
						</table>
					</td>
				</tr>
			</table>
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>
</form>
<div id="footer"></div>
</body>
</html>
