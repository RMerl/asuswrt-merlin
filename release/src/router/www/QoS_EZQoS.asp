<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge" />
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#EZQoS#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();

function initial(){
	show_menu();
	if(downsize_4m_support || downsize_8m_support)
			document.getElementById("guest_image").parentNode.style.display = "none";
	
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

	if(bwdpi_support){
		$('content_title').innerHTML = "<#Adaptive_QoS#> - <#Adaptive_QoS_Conf#>";
		if(document.form.qos_enable.value == 1){
			if(document.form.qos_type.value == 0){		//Traditional Type
				document.getElementById("settingSelection").length = 1;
				add_option($("settingSelection"), '<#qos_user_rules#>', 3, 0);
				add_option($("settingSelection"), '<#qos_user_prio#>', 4, 0);
			}
			else{		//Adaptive Type
				add_option($("settingSelection"), "<#EzQoS_type_adaptive#>", 2, 0);		
			}
		}
		else{		// hide select option if qos disable
			document.getElementById('settingSelection').style.display = "none";	
		}
	}
	else{
		$('content_title').innerHTML = "<#Menu_TrafficManager#> - QoS";
		document.getElementById('function_desc').innerHTML = "<#ezqosDesw#>";
		document.getElementById("settingSelection").length = 1;
		add_option($("settingSelection"), '<#qos_user_rules#>', 3, 0);
		add_option($("settingSelection"), '<#qos_user_prio#>', 4, 0);
	}
	
	init_changeScale();
	//addOnlineHelp($("faq"), ["ASUSWRT", "QoS"]);
}

function init_changeScale(){
	var upload = document.form.qos_obw.value;
	var download = document.form.qos_ibw.value;
	
	document.form.obw.value = upload/1024;
	document.form.ibw.value = download/1024;
}

function switchPage(page){
	if(page == "1")	
		location.href = "/QoS_EZQoS.asp";
	else if(page == "2")	
		location.href = "/AdaptiveQoS_Adaptive.asp";
	else if(page == "3")	
		location.href = "/Advanced_QOSUserRules_Content.asp";
	else if(page == "4")	
		location.href = "/Advanced_QOSUserPrio_Content.asp";
	else
		return false;
}

function submitQoS(){
	if(document.form.qos_enable.value == 0 && document.form.qos_enable_orig.value == 0){
		return false;
	}

	if(document.form.qos_enable.value == 1){
		if(document.form.obw.value.length == 0){	//To check field is empty
			alert("<#JS_fieldblank#>");
			document.form.obw.focus();
			document.form.obw.select();
			return;
		}
		else if( document.form.obw.value == 0){		// To check field is 0
			alert("Upload Bandwidth can not be 0");
			document.form.obw.focus();
			document.form.obw.select();
			return;
		
		}
		else if(document.form.obw.value.split(".").length > 2){		//To check more than two point symbol
			alert("The format of field of upload bandwidth is invalid");
			document.form.obw.focus();
			document.form.obw.select();
			return;	
		}
		
		if(document.form.ibw.value.length == 0){
			alert("<#JS_fieldblank#>");
			document.form.ibw.focus();
			document.form.ibw.select();
			return;
		}
		else if(document.form.ibw.value == 0){
			alert("Download Bandwidth can not be 0");
			document.form.ibw.focus();
			document.form.ibw.select();
			return;
		}
		else if(document.form.ibw.value.split(".").length > 2){
			alert("The format of field of download bandwidth is invalid");
			document.form.ibw.focus();
			document.form.ibw.select();
			return;	
		}
		
		if(document.form.qos_type.value != document.form.qos_type_orig.value){
			FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");	
		}
		else{
			document.form.action_script.value = "restart_qos;restart_firewall";
		}	
	
		document.form.qos_obw.disabled = false;
		document.form.qos_ibw.disabled = false;
		document.form.qos_obw.value = document.form.obw.value*1024;
		document.form.qos_ibw.value = document.form.ibw.value*1024;
	}	

	if(document.form.qos_enable.value != document.form.qos_enable_orig.value){
		if(Rawifi_support || Qcawifi_support){
			document.form.action_script.value = "restart_qos;restart_firewall";
		}
		else{		//Broadcom
			FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
		}
	}

	showLoading();
	document.form.submit();		
}

function change_qos_type(value){
	if(value == 0){		//Traditional
		$('int_type').checked = false;
		$('trad_type').checked = true;	
		if(document.form.qos_type_orig.value == 0 && document.form.qos_enable_orig.value != 0){
			document.form.action_script.value = "restart_qos;restart_firewall";
		}	
		else{
			document.form.action_script.value = "reboot";
			document.form.next_page.value = "Advanced_QOSUserRules_Content.asp";
		}		
	}	
	else{		//Adaptive
		$('int_type').checked = true;
		$('trad_type').checked = false;
		if(document.form.qos_type_orig.value == 1 && document.form.qos_enable_orig.value != 0)
			document.form.action_script.value = "restart_qos;restart_firewall";
		else{
			document.form.action_script.value = "reboot";
			document.form.next_page.value = "AdaptiveQoS_Adaptive.asp";
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
<input type="hidden" name="qos_obw" value="<% nvram_get("qos_obw"); %>" disabled>
<input type="hidden" name="qos_ibw" value="<% nvram_get("qos_ibw"); %>" disabled>
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
										<tr style="height:30px;">
											<td  class="formfonttitle" align="left">								
												<div id="content_title"><#Adaptive_QoS#> - QoS</div>
											</td>
											<td align="right" >
												<div>
													<select id="settingSelection" onchange="switchPage(this.options[this.selectedIndex].value)" class="input_option">
														<option value="1"><#Adaptive_QoS_Conf#></option>										
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
										<table style="width:700px;margin-left:25px;">
											<tr>
												<td style="width:130px">
													<div id="guest_image" style="background: url(images/New_ui/QoS.png);width: 143px;height: 87px;"></div>
												</td>
												<td>&nbsp&nbsp</td>
												<td style="font-style: italic;font-size: 14px;">
													<div id="function_desc" class="formfontdesc" style="line-height:20px;">
														<#EzQoS_desc#>
														<ul>
															<li><#EzQoS_desc_Adaptive#></li>
															<li><#EzQoS_desc_Traditional#></li>
														</ul>
														<#EzQoS_desc_note#>
													</div>
													<div class="formfontdesc">
														<a id="faq" href="http://www.asus.com/us/support/FAQ/1008718/" target="_blank" style="text-decoration:underline;">QoS FAQ</a>
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
											<th><#EzQoS_smart_enable#></th>
											<td>
												<div class="left" style="width:94px; float:left; cursor:pointer;" id="radio_qos_enable"></div>
													<script type="text/javascript">
														$j('#radio_qos_enable').iphoneSwitch('<% nvram_get("qos_enable"); %>', 
															 function() {
																document.form.qos_enable.value = 1;
																if(document.form.qos_enable_orig.value != 1){
																	if($('int_type').checked == true && bwdpi_support)
																		document.form.next_page.value = "AdaptiveQoS_Adaptive.asp";
																	else
																		document.form.next_page.value = "Advanced_QOSUserRules_Content.asp";
																}																
																
																$('upload_tr').style.display = "";
																$('download_tr').style.display = "";

																if(bwdpi_support){
																	$('qos_type_tr').style.display = "";
																	$('qos_enable_hint').style.display = "";
																}	
															 },
															 function() {
																document.form.qos_enable.value = 0;																
																$('upload_tr').style.display = "none";
																$('download_tr').style.display = "none";
	
																if(bwdpi_support){
																	$('qos_type_tr').style.display = "none";
																	$('qos_enable_hint').style.display = "none";
																}	
															 }
														);
													</script>			
												<div id="qos_enable_hint" style="color:#FC0;margin:5px 0px 0px 100px;display:none">Enabling Adaptive QoS may take several minutes.</div>
											</td>
										</tr>										
										<tr id="upload_tr">
											<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 2);"><#upload_bandwidth#></a></th>
											<td>
												<input type="text" maxlength="10" id="obw" name="obw" onKeyPress="return validator.isNumberFloat(this,event);" class="input_15_table" value="">
												<label style="margin-left:5px;">Mb/s</label>
											</td>
										</tr>											
										<tr id="download_tr">
											<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 2);"><#download_bandwidth#></a></th>
											<td>
												<input type="text" maxlength="10" id="ibw" name="ibw" onKeyPress="return validator.isNumberFloat(this,event);" class="input_15_table" value="">
												<label style="margin-left:5px;">Mb/s</label>
											</td>
										</tr>										
										<tr id="qos_type_tr" style="display:none">
											<th>QoS Type</th>
											<td>
												<input id="int_type" value="1" onClick="change_qos_type(this.value);" type="radio" <% nvram_match("qos_type", "1","checked"); %>><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 6);"><#EzQoS_type_adaptive#></a>
												<input id="trad_type" value="0" onClick="change_qos_type(this.value);" type="radio" <% nvram_match("qos_type", "0","checked"); %>><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 7);"><#EzQoS_type_traditional#></a>
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
