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
<title><#Web_Title#> - <#menu3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();
<% login_state_hook(); %>

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
var webdav_acc_lock = '<% nvram_get("webdav_acc_lock"); %>';
var enable_webdav_lock = '<% nvram_get("enable_webdav_lock"); %>';

function initial(){
	show_menu();
	if(webdav_acc_lock == 1){
		$("accIcon").src = "/images/cloudsync/account_block_icon.png";
		$("unlockBtn").style.display = "";
	}

	if(enable_webdav_lock == 0){
		inputCtrl(document.form.webdav_lock_times, 0);
		inputCtrl(document.form.webdav_lock_interval, 0);
	}
}

function applyRule(){
	validate_number_range(document.form.webdav_lock_times, 1, 10);
	validate_number_range(document.form.webdav_lock_interval, 1, 60);
	showLoading();	
	document.form.submit();	
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
							<a href="cloud_main.asp"><div class="tab"><span>AiCloud</span></div></a>
						</td>
						<td>
							<a href="cloud_sync.asp"><div class="tab"><span>Smart Sync</span></div></a>
						</td>
						<td>
							<div class="tabclick"><span>Settings</span></div>
						</td>
						<td>
							<a href="cloud_syslog.asp"><div class="tab"><span>Log</span></div></a>
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
									<div class="formfonttitle">AiCloud - Settings</div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

								  <div class="formfontdesc" style="font-style: italic;font-size: 14px;">
										Password protection mechanism
										The password protection mechanism allows AiCloud to prevent account from Brute-Force Attack.
										You can define allowable account/password login retry attempts.
										For example, 3 times / 2mins refers to user could only 3 chances to try the account and password in 2 minutes, once exceed,  the AiCloud account will be locked, and administrator need to enter this page to unlock it.
									</div>

									<table width="100%" style="border-collapse:collapse;">

									  <tr bgcolor="#444f53">
									    <td colspan="5" class="cloud_main_radius">
												<div style="padding:30px;font-size:18px;word-break:break-all;border-style:dashed;border-radius:10px;border-width:1px;border-color:#999;">
													<div>Enable password protection mechanism.</div>

													<div align="center" class="left" style="margin-top:-26px;margin-left:320px;width:94px; float:left; cursor:pointer;" id="radio_enable_webdav_lock"></div>
													<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
													<script type="text/javascript">
														$j('#radio_enable_webdav_lock').iphoneSwitch('<% nvram_get("enable_webdav_lock"); %>',
															function() {
																document.form.enable_webdav_lock.value = 1;
																inputCtrl(document.form.webdav_lock_times, 1);
																inputCtrl(document.form.webdav_lock_interval, 1);
															},
															function() {
																document.form.enable_webdav_lock.value = 0;
																inputCtrl(document.form.webdav_lock_times, 0);
																inputCtrl(document.form.webdav_lock_interval, 0);
															},
															{
																switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
															}
														);
													</script>			
													</div>

													<input type="text" name="webdav_lock_times" class="input_3_table" maxlength="2" onblur="validate_number_range(this, 1, 10);" value="<% nvram_get("webdav_lock_times"); %>"> times in 
													<input type="text" name="webdav_lock_interval" class="input_3_table" maxlength="2" onblur="validate_number_range(this, 1, 60);" value="<% nvram_get("webdav_lock_interval"); %>"> minutes the AiCloud account will be locked.
							            <br/>
							            <br/>
													<div class="apply_gen" style="background-color:#444F53;">
														<input style="margin-left:10px;" class="button_gen" onclick="applyRule();" type="button" value="<#CTL_apply#>"/>
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
													AiCloud account information:
													<div style="padding:15px;font-size:16px;">
														<img id="accIcon" width="40px" src="/images/cloudsync/account_icon.png">
														<div style="margin-top:-30px;margin-left:50px;font-size:16px;font-weight:bolder;">admin</div>
														<input id="unlockBtn" style="display:none;margin-top:-27px;margin-left:120px;" class="button_gen_short" onclick="unlockAcc();" type="button" value="Unlock"/>
													</div>
												</div>
											</td>
									  </tr>

									</table>

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
