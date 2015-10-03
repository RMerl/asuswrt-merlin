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
<title><#Web_Title#> - <#menu3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>

var webdav_acc_lock = '<% nvram_get("webdav_acc_lock"); %>';
var enable_webdav_lock = '<% nvram_get("enable_webdav_lock"); %>';

function initial(){
	show_menu();
	if(webdav_acc_lock == 1){
		document.getElementById("accIcon").src = "/images/cloudsync/account_block_icon.png";
		document.getElementById("unlockBtn").style.display = "";
	}
	
	if(enable_webdav_lock == '0'){	
		inputCtrl(document.form.webdav_lock_times, 0);
		inputCtrl(document.form.webdav_lock_interval, 0);
	}

	if(!rrsut_support)
		document.getElementById("rrsLink").style.display = "none";
		
	if(sw_mode == 2 || sw_mode == 3 || sw_mode == 4){
		document.getElementById("smart_sync_link").style.display = "none";
		document.getElementById("rrsLink").style.display = "none";
	}	

	if(aicloudipk_support){
		document.form.action_script.value = "restart_setting_webdav";
	}
}

function applyRule(){
	if(	validator.numberRange(document.form.webdav_lock_times, 1, 10)
		&&validator.numberRange(document.form.webdav_lock_interval, 1, 60)
		&&validator.numberRange(document.form.webdav_http_port, 1, 65535)
		&&validator.numberRange(document.form.webdav_https_port, 1, 65535)
		&&isPortConflict_webdav(document.form.webdav_http_port)
		&&isPortConflict_webdav(document.form.webdav_https_port)
		&&document.form.webdav_http_port.value!=document.form.webdav_https_port.value
	){
		document.form.webdav_http_port.value = parseInt(document.form.webdav_http_port.value);	
		document.form.webdav_https_port.value = parseInt(document.form.webdav_https_port.value);	
		showLoading();	
		document.form.submit();	
	}
}

function isPortConflict_webdav(obj){
	if(obj.value == '<% nvram_get("login_port"); %>'){
		alert("<#portConflictHint#> HTTP LAN port.");
		obj.focus();
		return false;
	}	
	else if(obj.value == '<% nvram_get("dm_http_port"); %>'){
		alert("<#portConflictHint#> Download Master.");
		obj.focus();
		return false;
	}	
	else if(obj.value == '<% nvram_get("misc_httpsport_x"); %>'){
		alert("<#portConflictHint#> [<#FirewallConfig_x_WanWebPort_itemname#>(HTTPS)].");
		obj.focus();
		return false;
	}	
	else if(obj.value == '<% nvram_get("misc_httpport_x"); %>'){
		alert("<#portConflictHint#> [<#FirewallConfig_x_WanWebPort_itemname#>(HTTP)].");
		obj.focus();
		return false;
	}
	
	return true;	
}

function unlockAcc(){
	document.form.webdav_acc_lock.value = 0;
	showLoading();	
	document.form.submit();	
}
</script>
</head>
<body onload="initial();" onunload="return unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="cloud_settings.asp">
<input type="hidden" name="next_page" value="cloud_settings.asp">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_webdav">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="enable_webdav_lock" value="<% nvram_get("enable_webdav_lock"); %>">
<input type="hidden" name="webdav_acc_lock" value="<% nvram_get("webdav_acc_lock"); %>">
<table border="0" align="center" cellpadding="0" cellspacing="0" class="content">
	<tr>
		<td valign="top" width="17">&nbsp;</td>
		<!--=====Beginning of Main Menu=====-->
		<td valign="top" width="202">
			<div id="mainMenu"></div>
			<div id="subMenu"></div>
		</td>
		<td valign="top">
			<div id="tabMenu" class="submenuBlock">
				<table border="0" cellspacing="0" cellpadding="0">
					<tbody>
					<tr>
						<td>
							<a href="cloud_main.asp"><div class="tab"><span>AiCloud 2.0</span></div></a>
						</td>
						<td>
							<a id="smart_sync_link" href="cloud_sync.asp"><div class="tab"><span><#smart_sync#></span></div></a>
						</td>
						<td>
							<a id="rrsLink" href="cloud_router_sync.asp"><div class="tab"><span><#Server_Sync#></span></div></a>
						</td>
						<td>
							<div class="tabclick"><span><#Settings#></span></div>
						</td>
						<td>
							<a href="cloud_syslog.asp"><div class="tab"><span><#Log#></span></div></a>
						</td>
					</tr>
					</tbody>
				</table>
			</div>
<!--==============Beginning of hint content=============-->
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
			  <tr>
					<td align="left" valign="top">
					  <table width="100%" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
							<tr>
							  <td bgcolor="#4D595D" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle">AiCloud 2.0 - <#Settings#></div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

								  <div class="formfontdesc" style="font-style: italic;font-size: 14px;">
										<#AiCloud_PWD_Mechanism#><br>
										<#AiCloud_PWD_note1#><br>
										<#AiCloud_PWD_note2#><br>
										<#AiCloud_PWD_note3#><br>
									</div>

									<table width="100%" style="border-collapse:collapse;">

									  <tr bgcolor="#444f53">
									    <td colspan="5" class="cloud_main_radius">
												<div style="padding:30px;font-size:18px;word-break:break-all;border-style:dashed;border-radius:10px;border-width:1px;border-color:#999;">
													<div><#AiCloud_PWD_enable#></div>

													<div align="center" class="left" style="margin-top:-26px;margin-left:320px;width:94px; float:left; cursor:pointer;" id="radio_enable_webdav_lock"></div>
													<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
													<script type="text/javascript">
														$('#radio_enable_webdav_lock').iphoneSwitch('<% nvram_get("enable_webdav_lock"); %>',
															function() {
																document.form.enable_webdav_lock.value = 1;
																inputCtrl(document.form.webdav_lock_times, 1);
																inputCtrl(document.form.webdav_lock_interval, 1);
															},
															function() {
																document.form.enable_webdav_lock.value = 0;
																inputCtrl(document.form.webdav_lock_times, 0);
																inputCtrl(document.form.webdav_lock_interval, 0);
															}
														);
													</script>			
													</div>
													
													<div>
														<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
		  											<tr>
     													<th style="white-space:normal;"><#AiCloud_lock_time#></th>
															<td style="text-align:left;">
																<input type="text" name="webdav_lock_times" class="input_3_table" maxlength="2" onblur="validator.numberRange(this, 1, 10);" value="<% nvram_get("webdav_lock_times"); %>" autocorrect="off" autocapitalize="off">
															</td>
														</tr>	
														<tr>
     													<th><#AiCloud_lock_interval#></th>
															<td style="text-align:left;">
																<input type="text" name="webdav_lock_interval" class="input_3_table" maxlength="2" onblur="validator.numberRange(this, 1, 60);" value="<% nvram_get("webdav_lock_interval"); %>" autocorrect="off" autocapitalize="off"> <#Minute#>
															</td>
														</tr>
														</table>
													</div>																			
													<div style="margin-bottom:-15px">
														<table> 
															<tr>
																<td>
																	<span><#AiCloud_account_status#></span>
																	<img style="margin-top:10px;margin-bottom:-15px" id="accIcon" width="40px" src="/images/cloudsync/account_icon.png">
																	<span style="font-size:16px;font-weight:bolder;"><% nvram_get("http_username"); %></span>
																</td>
																<td>
																	<input id="unlockBtn" style="margin-top:10px;display:none;" class="button_gen" onclick="unlockAcc();" type="button" value="<#AiCloud_account_unlock#>"/>
																</td>
															</tr>
														</table>
													</div>
												</div>
											</td>
									  </tr>

									  <tr height="20px">
											<td colspan="3">
											</td>
									  </tr>

									  <tr bgcolor="#444f53">
									    <td colspan="5" class="cloud_main_radius">
												<div style="padding:30px;font-size:18px;word-break:break-all;border-style:dashed;border-radius:10px;border-width:1px;border-color:#999;">
													<#AiCloud_webport#> <input type="text" name="webdav_https_port" class="input_6_table" maxlength="5" onKeyPress="return validator.isNumber(this,event);" value="<% nvram_get("webdav_https_port"); %>" autocorrect="off" autocapitalize="off">
													<br>
													<br>
													<#AiCloud_streamport#> <input type="text" name="webdav_http_port" class="input_6_table" maxlength="5" onKeyPress="return validator.isNumber(this,event);" value="<% nvram_get("webdav_http_port"); %>" autocorrect="off" autocapitalize="off">
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
				</tr>
			</table>
		</td>
		<td width="20"></td>
	</tr>
</table>
<div id="footer"></div>
</form>

</body>
</html>
