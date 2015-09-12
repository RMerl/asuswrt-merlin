<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7, IE=EmulateIE10" />
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
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type='text/javascript'>var fanctrl_info = [<% get_fanctrl_info(); %>];
curr_coreTmp_2 = "<% sysinfo("temperature.2"); %>".replace("&deg;C", "");
curr_coreTmp_5 = "<% sysinfo("temperature.5"); %>".replace("&deg;C", ""); 
curr_coreTmp_cpu = "<% get_cpu_temperature(); %>";
var coreTmp_2 = new Array();
var coreTmp_5 = new Array();
var coreTmp_cpu = new Array();
coreTmp_2 = [curr_coreTmp_2];
coreTmp_5 = [curr_coreTmp_5];
coreTmp_cpu = [curr_coreTmp_cpu];
var wl_control_channel = <% wl_control_channel(); %>;

function initial(){
	var code1, code2;
	show_menu();
	document.form.fanctrl_fullspeed_temp_unit.selectedIndex = cookie.get("CoreTmpUnit");
	update_coretmp();

	code1 = '<br>Legend: <span style="color: #FF9900;">2.4 GHz</span> - <span style="color: #33CCFF;">5 GHz</span>';
	code2 = '<br>Current Temperatures: <span id="coreTemp_2" style="text-align:center; font-weight:bold;color:#FF9900"></span> - <span id="coreTemp_5" style="text-align:center; font-weight:bold;color:#33CCFF"></span>';

	if(curr_coreTmp_cpu != "") {
		code1 += ' - <span style="color: #00FF33;">CPU</span>';
		code2 += ' - <span id="coreTemp_cpu" style="text-align:center; font-weight:bold;color:#00FF33"></span>';
	}

	document.getElementById("legend").innerHTML = code1 + code2;
}

function update_coretmp(e){
  $.ajax({
    url: '/ajax_coretmp.asp',
    dataType: 'script', 
	
    error: function(xhr){
      update_coretmp();
    },
    success: function(response){
			updateNum(curr_coreTmp_2, curr_coreTmp_5, curr_coreTmp_cpu);
			setTimeout("update_coretmp();", 5000);
		}    
  });
}


function updateNum(_coreTmp_2, _coreTmp_5, _cpuTemp){

	if(document.form.fanctrl_fullspeed_temp_unit.value == 1){
		document.getElementById("coreTemp_2").innerHTML = (_coreTmp_2 == 0 ? "disabled" : Math.round(_coreTmp_2*9/5+32) + " °F");
		document.getElementById("coreTemp_5").innerHTML = (_coreTmp_5 == 0 ? "disabled" : Math.round(_coreTmp_5*9/5+32) + " °F");
		if (_cpuTemp != "")
			document.getElementById("coreTemp_cpu").innerHTML = Math.round(_cpuTemp*9/5+32) + " °F";
	}
	else{
		document.getElementById("coreTemp_2").innerHTML = (_coreTmp_2 == 0 ? "disabled" : _coreTmp_2 + " °C");
		document.getElementById("coreTemp_5").innerHTML = (_coreTmp_5 == 0 ? "disabled" : _coreTmp_5 + " °C");
		if (_cpuTemp != "")
			document.getElementById("coreTemp_cpu").innerHTML = _cpuTemp + " °C";
	}
}

function applyRule(){

	showLoading();
	document.form.submit();
}

function changeTempUnit(num){
	cookie.set("CoreTmpUnit", num, 365);
	refreshpage();
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
			<input type="hidden" name="modified" value="0">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_wait" value="5">
			<input type="hidden" name="action_script" value="">
			<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
			<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
			<input type="hidden" name="wl_ssid" value="<% nvram_get("wl_ssid"); %>">
			<input type="hidden" name="wl0_TxPower_orig" value="<% nvram_get("wl0_TxPower"); %>" disabled>
			<input type="hidden" name="wl1_TxPower_orig" value="<% nvram_get("wl1_TxPower"); %>" disabled>
			<input type="hidden" name="btn_led_mode" value="<% nvram_get("btn_led_mode"); %>">
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td valign="top">
						<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
								<tr bgcolor="#4D595D" style="height:10px">
								  <td valign="top">
									  <div>&nbsp;</div>
									  <div class="formfonttitle"><#menu5_6#> - Performance tuning</div>
									  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									  <!--div class="formfontdesc"><#PerformaceTuning_desc#></div-->
									</td>
								</tr>

								<tr>
									<td bgcolor="#4D595D" valign="top">
										<table width="99%" border="0" align="center" cellpadding="0" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
											<thead>
											<tr>
												<td colspan="2">Cooler status</td>
											</tr>
											</thead>
											
											<tr>
												<td valign="top">
													<div style="margin-left:-10px;">
														<!--========= svg =========-->
														<!--[if IE]>
															<div id="svg-table" align="left">
															<object id="graph" src="fan.svg" classid="image/svg+xml" width="740" height="400">
															</div>
														<![endif]-->
														<!--[if !IE]>-->
															<object id="graph" data="fan.svg" type="image/svg+xml" width="740" height="400">
														<!--<![endif]-->
															</object>
											 			<!--========= svg =========-->
													</div>
												</td>
											</tr>
										</table>
										<div id="legend"></div>
									</td>
								</tr>
								<tr>
									<td bgcolor="#4D595D" valign="top">
										<table width="99%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
											<thead>
											<tr>
												<td colspan="2">System adjustment</td>
											</tr>
											</thead>

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
									</td>
								</tr>

								<tr valign="top" style="height:10px">
									<td bgcolor="#4D595D" valign="top">
										<div class="apply_gen">
											<input class="button_gen" onclick="applyRule();" type="button" value="<#CTL_apply#>"/>
										</div>
									</td>
								</tr>

								<tr valign="top">
									<td bgcolor="#4D595D" valign="top">
										<div>
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
