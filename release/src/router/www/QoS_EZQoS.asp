<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#EZQoS#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();

function initial(){
	show_menu();
	if(downsize_4m_support)
		$("guest_image").parentNode.style.display = "none";
	
	if(document.form.qos_enable.value==1){
		$('upload_tr').style.display = "";
		$('download_tr').style.display = "";		
		if(bwdpi_support)
			$('qos_type_tr').style.display = "";	
	}else{
		$('upload_tr').style.display = "none";
		$('download_tr').style.display = "none";		

		if(bwdpi_support)
			$('qos_type_tr').style.display = "none";
	}

	init_changeScale("qos_obw");
	init_changeScale("qos_ibw");	
	addOnlineHelp($("faq"), ["ASUSWRT", "QoS"]);

	if(bwdpi_support){
		if(document.form.qos_enable.value == 1){
			if(document.form.qos_type.value == 0){		//Traditional Type
				document.getElementById("settingSelection").length = 1;
				add_option($("settingSelection"), '<#qos_user_prio#>', 3, 0);
				add_option($("settingSelection"), '<#qos_user_rules#>', 4, 0);
			}
			else{		//Intelligence Type
				add_option($("settingSelection"), 'Intelligence Type', 2, 0);
			
			}
		}
	}
	else{
		document.getElementById("settingSelection").length = 1;
		add_option($("settingSelection"), '<#qos_user_prio#>', 3, 0);
		add_option($("settingSelection"), '<#qos_user_rules#>', 4, 0);
	}
}

function init_changeScale(_obj_String){
	if($(_obj_String).value > 999){
		$(_obj_String+"_scale").value = "Mb/s";
		$(_obj_String).value = Math.round(($(_obj_String).value/1024)*100)/100;
	}
}

function changeScale(_obj_String){
	if($(_obj_String+"_scale").value == "Mb/s")
		$(_obj_String).value = Math.round(($(_obj_String).value/1024)*100)/100;
	else
		$(_obj_String).value = Math.round($(_obj_String).value*1024);
}

function switchPage(page){
	if(page == "1")	
		location.href = "/QoS_EZQoS.asp";
	else if(page == "2")	
		location.href = "/AiStream_Intelligence.asp";
	else if(page == "3")	
		location.href = "/Advanced_QOSUserRules_Content.asp";
	else if(page == "4")	
		location.href = "/Advanced_QOSUserPrio_Content.asp";
	else
		return false;
}

function submitQoS(){
	if(document.form.qos_enable.value == 1){
		if(document.form.qos_obw.value.length == 0 || document.form.qos_obw.value == 0){
				alert("<#JS_fieldblank#>");
				document.form.qos_obw.focus();
				document.form.qos_obw.select();
				return;
		}
		if(document.form.qos_ibw.value.length == 0 || document.form.qos_ibw.value == 0){
				alert("<#JS_fieldblank#>");
				document.form.qos_ibw.focus();
				document.form.qos_ibw.select();
				return;
		}
		
		if(document.form.qos_type.value != document.form.qos_type_orig.value){
			FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");	
		}
		else{
			document.form.action_script.value = "restart_qos;restart_firewall";
		}	
	}	

	if($("qos_obw_scale").value == "Mb/s")
		document.form.qos_obw.value = Math.round(document.form.qos_obw.value*1024);
	if($("qos_ibw_scale").value == "Mb/s")
		document.form.qos_ibw.value = Math.round(document.form.qos_ibw.value*1024);
  
	if(document.form.qos_enable.value != document.form.qos_enable_orig.value)
		FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");	

	parent.showLoading();
	document.form.submit();		
}

function change_qos_type(value){
	if(value == 0){		//Traditional
		$('int_type').checked = false;
		$('trad_type').checked = true;	
		if(document.form.qos_type_orig.value == 0 && document.form.qos_enable_orig.value != 0)
			document.form.action_script.value = "restart_qos;restart_firewall";
		else{
			document.form.action_script.value = "reboot";
			document.form.next_page.value = "Advanced_QOSUserRules_Content.asp";
		}	
		
		
	}	
	else{		//Intelligence
		$('int_type').checked = true;
		$('trad_type').checked = false;
		if(document.form.qos_type_orig.value == 1 && document.form.qos_enable_orig.value != 0)
			document.form.action_script.value = "restart_qos;restart_firewall";
		else{
			document.form.action_script.value = "reboot";
			document.form.next_page.value = "AiStream_Intelligence.asp";
		}	
	}

	document.form.qos_type.value = value;
}
</script>
</head>

<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="/QoS_EZQoS.asp">
<input type="hidden" name="next_page" value="/QoS_EZQoS.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="flag" value="">
<input type="hidden" name="qos_enable" value="<% nvram_get("qos_enable"); %>">
<input type="hidden" name="qos_enable_orig" value="<% nvram_get("qos_enable"); %>">
<input type="hidden" name="qos_type" value="<% nvram_get("qos_type"); %>">
<input type="hidden" name="qos_type_orig" value="<% nvram_get("qos_type"); %>">
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
			<table width="95%" border="0" align="left" cellpadding="0" cellspacing="0" class="FormTitle" id="FormTitle" style="height:820px;">
				<tr>
					<td bgcolor="#4D595D" valign="top">
						<table width="760px" border="0" cellpadding="4" cellspacing="0">
							<tr>
								<td bgcolor="#4D595D" valign="top">
									<table width="100%">
										<tr>
											<td  class="formfonttitle" align="left">								
												<div>QoS</div>
											</td>
											<td align="right" >
												<div>
													<select id="settingSelection" onchange="switchPage(this.options[this.selectedIndex].value)" class="input_option">
														<option value="1">Configuration</option>										
													</select>	    
												</div>
											</td>	
										</tr>
									</table>	
								</td>
							</tr>
							<tr>
								<td height="5" bgcolor="#4D595D" valign="top"><img src="images/New_ui/export/line_export.png" /></td>
							</tr>
							<tr>
								<td height="30" align="left" valign="top" bgcolor="#4D595D">
									<div>
										<table width="650px">
											<tr>
												<td width="130px">
													<img id="guest_image" src="/images/New_ui/QoS.png">
												</td>
												<td style="font-style: italic;font-size: 14px;">
													<div class="formfontdesc" style="line-height:20px;"><#ezqosDesw#></div>
													<div class="formfontdesc">
														<a id="faq" href="" target="_blank" style="text-decoration:underline;">QoS FAQ</a>
													</div>
												</td>
											</tr>
										</table>
									</div>
								</td>
							</tr>							
							<tr>
								<td valign="top">
									<table style="margin-left:3px;" width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
										<tr>
											<th>Enable Smart QoS</th>
											<td>
												<div class="left" style="width:94px; float:left; cursor:pointer;" id="radio_qos_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
													<script type="text/javascript">
														$j('#radio_qos_enable').iphoneSwitch('<% nvram_get("qos_enable"); %>', 
															 function() {
																document.form.qos_enable.value = 1;
																if(document.form.qos_enable_orig.value != 1){
																	document.form.action_script.value = "reboot";
																	if($('int_type').checked == true)
																		document.form.next_page.value = "AiStream_Intelligence.asp";
																	else
																		document.form.next_page.value = "Advanced_QOSUserRules_Content.asp";
																}																
																
																$('upload_tr').style.display = "";
																$('download_tr').style.display = "";

																if(bwdpi_support){
																	$('qos_type_tr').style.display = "";																															
																}	
															 },
															 function() {
																document.form.qos_enable.value = 0;
																if(document.form.qos_enable_orig.value != 0)
																	document.form.action_script.value = "reboot";
																
																$('upload_tr').style.display = "none";
																$('download_tr').style.display = "none";
	
																if(bwdpi_support)
																	$('qos_type_tr').style.display = "none";
															 },
															 {
																switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
															 }
														);
													</script>			
												</div>	
											</td>
										</tr>										
										<tr id="upload_tr">
											<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 2);"><#upload_bandwidth#></a></th>
											<td>
												<input type="text" maxlength="10" id="qos_obw" name="qos_obw" onKeyPress="return is_number(this,event);" class="input_15_table" value="<% nvram_get("qos_obw"); %>">
												<select id="qos_obw_scale" class="input_option" style="width:87px;" onChange="changeScale('qos_obw');">
													<option value="Kb/s">Kb/s</option>
													<option value="Mb/s" selected>Mb/s</option>
												</select>
											</td>
										</tr>											
										<tr id="download_tr">
											<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 2);"><#download_bandwidth#></a></th>
											<td>
												<input type="text" maxlength="10" id="qos_ibw" name="qos_ibw" onKeyPress="return is_number(this,event);" class="input_15_table" value="<% nvram_get("qos_ibw"); %>">
												<select id="qos_ibw_scale" class="input_option" style="width:87px;" onChange="changeScale('qos_ibw');">
													<option value="Kb/s">Kb/s</option>
													<option value="Mb/s" selected>Mb/s</option>
												</select>
											</td>
										</tr>										
										<tr id="qos_type_tr" style="display:none">
											<th>QoS Type</th>
											<td>
												<input id="int_type" value="1" onClick="change_qos_type(this.value);" type="radio" <% nvram_match("qos_type", "1","checked"); %>><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 6);">Intelligence</a>												
												<input id="trad_type" value="0" onClick="change_qos_type(this.value);" type="radio" <% nvram_match("qos_type", "0","checked"); %>><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 7);">Traditional</a>
											</td>
										</tr>								
									</table>
								</td>
							</tr>	
							
							<tr>
								<td height="50" >
									<div style=" *width:136px;margin-left:300px;" class="titlebtn" align="center" onClick="submitQoS()"><span><#CTL_apply#></span></div>
								</td>
							</tr>										
						</table>
					</td>  
				</tr>
			</table>
		<!--===================================End of Main Content===========================================-->
		</td>	
	</tr>
</table>


<div id="footer"></div>
</body>
</html>
