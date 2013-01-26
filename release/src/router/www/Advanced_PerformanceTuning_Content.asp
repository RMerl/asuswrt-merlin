<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE10" />
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
curr_coreTmp_2 = "<% sysinfo("temperature.2"); %>".replace("&deg;C", "");
curr_coreTmp_5 = "<% sysinfo("temperature.5"); %>".replace("&deg;C", ""); 
var coreTmp_2 = new Array();
var coreTmp_5 = new Array();
coreTmp_2 = [curr_coreTmp_2];
coreTmp_5 = [curr_coreTmp_5];
var wl_control_channel = <% wl_control_channel(); %>;
var $j = jQuery.noConflict();
var MaxTxPower_2;
var MaxTxPower_5;
var HW_MAX_LIMITATION_2 = 501;
var HW_MIN_LIMITATION_2 = 9;
var HW_MAX_LIMITATION_5 = 251;
var HW_MIN_LIMITATION_5 = 9;

function initial(){
	show_menu();
	document.form.fanctrl_fullspeed_temp_unit.selectedIndex = getCookie("CoreTmpUnit");
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


function updateNum(_coreTmp_2, _coreTmp_5){

	if(document.form.fanctrl_fullspeed_temp_unit.value == 1){
		$("coreTemp_2").innerHTML = (_coreTmp_2 == 0 ? "disabled" : Math.round(_coreTmp_2*9/5+32) + " °F");
		$("coreTemp_5").innerHTML = (_coreTmp_5 == 0 ? "disabled" : Math.round(_coreTmp_5*9/5+32) + " °F");
	}
	else{
		$("coreTemp_2").innerHTML = (_coreTmp_2 == 0 ? "disabled" : _coreTmp_2 + " °C");
		$("coreTemp_5").innerHTML = (_coreTmp_5 == 0 ? "disabled" : _coreTmp_5 + " °C");
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

	$("TxPowerHint_2").style.display = "none";
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
									  <!--div class="formfontdesc"><#PerformaceTuning_desc#></div-->
									  <div class="formfontdesc">Fine tune the radio power to enhance/decrease the coverage and change the cooler spin mode.Please note: If the output power is increased for long distance signal transmission, the client also need to use high power card to get the best performance.</div>
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
										<br>Legend: <span style="color: #FF9900;">2.4 GHz</span> - <span style="color: #33CCFF;">5 GHz</span>
										<br>Current Temperatures:<span id="coreTemp_2" style="text-align:center; font-weight:bold;color:#FF9900"></span> - <span id="coreTemp_5" style="text-align:center; font-weight:bold;color:#33CCFF"></span>

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
<!--
											
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
-->
											<tr>
												<th>Temperature unit</th>
												<td>
													<select name="fanctrl_fullspeed_temp_unit" class="input_option" onchange="changeTempUnit(this.value)">
														<option class="content_input_fd" value="0">°C</option>
														<option class="content_input_fd" value="1">°F</option>
													</select>
												</td>
											</tr>
											
										</table>
										<!-- div class="apply_gen">
											<input class="button_gen" onclick="applyRule();" type="button" value="<#CTL_apply#>"/>
										</div -->
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
