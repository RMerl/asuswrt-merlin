<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE9"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<title><#Web_Title#> - Home Security</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="/aidisk/AiDisk_folder_tree.js"></script>
<style>
.weakness{
	width:650px;
	height:570px;
	position:absolute;
	background-color:#3C3C3C;
	z-index:10;
	margin-left:300px;
	border-radius:10px;
}

.weakness_router_status{
	text-align:center;
	font-size:18px;
	padding:10px;
	font-weight:bold;
}

.weakness_status{
	width:90%;
	background-color:#BEBEBE;
	color:#000;border-collapse:separate !important;
	border-radius:10px;

}

.weakness_status th{
	text-align:left;
	border-bottom:1px solid #4D595D;
	padding-left:15px;
	font-size:14px;
}

.weakness_status td{
	border-bottom:1px solid #4D595D;
	width:100px;
}

.weakness_status td>div{
	/*background-color:#FF7575;*/ /*#1CFE16 for Yes button*/
	border-radius:10px;
	text-align:center;
	padding:3px 0px;
	width:80px;

}

.status_no{
	background-color:#FF7575;
}
.status_no a{
	text-decoration:underline;



}
.status_yes{
	background-color:#1CFE16;
}

</style>
<script>
window.onresize = cal_panel_block;
<% get_AiDisk_status(); %>
var AM_to_cifs = get_share_management_status("cifs");  // Account Management for Network-Neighborhood
var AM_to_ftp = get_share_management_status("ftp");  // Account Management for FTP
var $j = jQuery.noConflict();

function initial(){
	show_menu();
}

function applyRule(){
	showLoading();	
	document.form.submit();
}

function check_weakness(){
	cal_panel_block();
	$j('#weakness_div').fadeIn();
	check_login_name_password();
	check_wireless_password();
	check_wireless_encryption();
	check_upnp();
	check_wan_access();
	check_ping_form_wan();
	check_dmz();
	check_port_trigger();
	check_port_forwarding();
	check_ftp_anonymous();
	check_samba_anonymous();
	check_TM_feature();
}

function close_weakness_status(){
	$j('#weakness_div').fadeOut(100);
}
function enable_whole_security(){
	var action_script_temp = "";
	var unpn_enable = document.form.wan_upnp_enable.value; 
	var wan_access_enable = document.form.misc_http_x.value;
	var wan_ping_enable = document.form.misc_ping_x.value;
	var port_trigger_enable = document.form.autofw_enable_x.value;
	var port_forwarding_enable = document.form.vts_enable_x.value;
	var wrs_enable = document.form.wrs_enable.value;
	var wrs_app_enable = document.form.wrs_app_enable.value;
	var wrs_cc_enable = document.form.wrs_cc_enable.value;
	var wrs_vp_enable = document.form.wrs_vp_enable.value;
	var restart_wan = 0;
	var restart_time = 0;
	var restart_firewall = 0;
	var restart_wrs = 0;
	
	if(unpn_enable == 1){
		document.form.wan_upnp_enable.value = 0;
		document.form.wan_upnp_enable.disabled = false;;
		document.form.wan_unit.disabled = false;;
		restart_wan = 1;
	}
	
	if(wan_access_enable ==1){
		console
		document.form.misc_http_x.value = 0;
		document.form.misc_http_x.disabled = false;;
		restart_time = 1;
	}
	
	if(wan_ping_enable == 1){
		document.form.misc_ping_x.value = 0;
		document.form.misc_ping_x.disabled = false;
		restart_firewall = 1;
	}
	
	if(document.form.dmz_ip.value != ""){
		document.form.dmz_ip.value = "";
		document.form.dmz_ip.disabled = false;
		restart_firewall = 1;
	}

	if(port_trigger_enable == 1){
		document.form.autofw_enable_x.value = 0;
		document.form.autofw_enable_x.disabled = false;
		restart_firewall = 1;
	}
	
	if(port_forwarding_enable == 1){
		document.form.vts_enable_x.value = 0;
		document.form.vts_enable_x.disabled = false;
		restart_firewall = 1;
	}
	
	if(wrs_enable == 0 || wrs_app_enable == 0){
		document.form.wrs_enable.value = 1;
		document.form.wrs_app_enable.value = 1;
		restart_firewall = 1;
		restart_wrs = 1;
	}
	
	if(wrs_cc_enable == 0){
		document.form.wrs_cc_enable.value = 1;
		restart_firewall = 1;
		restart_wrs = 1;
	}
	
	if(wrs_vp_enable == 0){
		document.form.wrs_vp_enable.value = 1;
		restart_firewall = 1;
		restart_wrs = 1;
	}

	if(restart_wan == 1){
		if(action_script_temp == "")
			action_script_temp += "restart_wan_if";
		else	
			action_script_temp += ";restart_wan_if";	
	}
	
	if(restart_time == 1){
		if(action_script_temp == "")
			action_script_temp =+ "restart_time";
		else	
			action_script_temp += ";restart_time";	
	}
	
	if(restart_wrs == 1){
		if(action_script_temp == "")
			action_script_temp += "restart_wrs";
		else	
			action_script_temp += ";restart_wrs";
	}
	
	if(restart_firewall == 1){
		if(action_script_temp == "")
			action_script_temp += "restart_firewall";
		else	
			action_script_temp += ";restart_firewall";	
	}
	
	document.form.action_script.value = action_script_temp;
	document.form.submit();
}
function check_login_name_password(){
	var username = document.form.http_username.value;
	var password = document.form.http_passwd.value;

	if(username == "admin" || password == "admin"){
		$('login_password').innerHTML = "<a href='Advanced_System_Content.asp' target='_blank'>No</a>";
		$('login_password').className = "status_no";	
	}
	else{
		$('login_password').innerHTML = "Yes";
		$('login_password').className = "status_yes";
	}
}

function check_wireless_password(){
	var wireless_password = document.form.wl_wpa_psk.value;
	chkPass(document.form.wl_wpa_psk.value,"http_passwd");
}

function check_wireless_encryption(){
	var encryption_type = document.form.wl_auth_mode_x.value;
	
	if(encryption_type == "psk2" || encryption_type == "pskpsk2" || encryption_type == "wpa2" || encryption_type == "wpawpa2"){		
		$('wireless_encryption').innerHTML = "<#PASS_score3#>";
		$('wireless_encryption').className = "status_yes";
	}
	else{
		$('wireless_encryption').innerHTML = "<a href='Advanced_Wireless_Content.asp' target='_blank'><#PASS_score1#></a>";
		$('wireless_encryption').className = "status_no";	
	}
}

function check_upnp(){
	var unpn_enable = document.form.wan_upnp_enable.value;
	
	if(unpn_enable == 0){
		$('upnp_service').innerHTML = "Yes";
		$('upnp_service').className = "status_yes";
	}
	else{
		$('upnp_service').innerHTML = "<a href='Advanced_WAN_Content.asp' target='_blank'>No</a>";
		$('upnp_service').className = "status_no";
	}
}

function check_wan_access(){
	var wan_access_enable = document.form.misc_http_x.value;

	if(wan_access_enable == 0){
		$('access_from_wan').innerHTML = "Yes";
		$('access_from_wan').className = "status_yes";
	}
	else{
		$('access_from_wan').innerHTML = "<a href='Advanced_System_Content.asp' target='_blank'>No</a>";
		$('access_from_wan').className = "status_no";
	}
}

function check_ping_form_wan(){
	var wan_ping_enable = document.form.misc_ping_x.value;

	if(wan_ping_enable == 0){
		$('ping_from_wan').innerHTML = "Yes";
		$('ping_from_wan').className = "status_yes";
	}
	else{
		$('ping_from_wan').innerHTML = "<a href='Advanced_BasicFirewall_Content.asp' target='_blank'>No</a>";
		$('ping_from_wan').className = "status_no";
	}
}

function check_dmz(){
	if(document.form.dmz_ip.value == ""){
		$('dmz_service').innerHTML = "Yes";
		$('dmz_service').className = "status_yes";
	}
	else{
		$('dmz_service').innerHTML = "<a href='Advanced_Exposed_Content.asp' target='_blank'>No</a>";
		$('dmz_service').className = "status_no";
	}
}

function check_port_trigger(){
	var port_trigger_enable = document.form.autofw_enable_x.value;

	if(port_trigger_enable == 0){
		$('port_tirgger').innerHTML = "Yes";
		$('port_tirgger').className = "status_yes";
	}
	else{
		$('port_tirgger').innerHTML = "<a href='Advanced_PortTrigger_Content.asp' target='_blank'>No</a>";
		$('port_tirgger').className = "status_no";
	}

}

function check_port_forwarding(){
	var port_forwarding_enable = document.form.vts_enable_x.value;

	if(port_forwarding_enable == 0){
		$('port_forwarding').innerHTML = "Yes";
		$('port_forwarding').className = "status_yes";
	}
	else{
		$('port_forwarding').innerHTML = "<a href='Advanced_VirtualServer_Content.asp' target='_blank'>No</a>";
		$('port_forwarding').className = "status_no";
	}
}

function check_ftp_anonymous(){
	var ftp_account_mode = get_manage_type('ftp');		//0: shared mode, 1: account mode
	
	if(ftp_account_mode == 0){
		$('ftp_account').innerHTML = "<a href='Advanced_AiDisk_ftp.asp' target='_blank'>No</a>";
		$('ftp_account').className = "status_no";
	}
	else{
		$('ftp_account').innerHTML = "Yes";
		$('ftp_account').className = "status_yes";
	}
}

function check_samba_anonymous(){
	var samba_account_mode = get_manage_type('cifs');
	
	if(samba_account_mode == 0){
		$('samba_account').innerHTML = "<a href='Advanced_AiDisk_samba.asp' target='_blank'>No</a>";
		$('samba_account').className = "status_no";
	}
	else{
		$('samba_account').innerHTML = "Yes";
		$('samba_account').className = "status_yes";
	}
}

function check_TM_feature(){
	var wrs_enable = document.form.wrs_enable.value;
	var wrs_app_enable = document.form.wrs_app_enable.value;
	var wrs_cc_enable = document.form.wrs_cc_enable.value;
	var wrs_vp_enable = document.form.wrs_vp_enable.value;

	if(wrs_enable == 1 || wrs_app_enable == 1){
		$('wrs_service').innerHTML = "Yes";
		$('wrs_service').className = "status_yes";
	}
	else{
		$('wrs_service').innerHTML = "<a href='ParentalControl_HomeSecurity.asp' target='_blank'>No</a>";
		$('wrs_service').className = "status_no";
	}

	if(wrs_vp_enable == 1){
		$('vp_service').innerHTML = "Yes";
		$('vp_service').className = "status_yes";
	}
	else{
		$('vp_service').innerHTML = "<a href='ParentalControl_HomeSecurity.asp' target='_blank'>No</a>";
		$('vp_service').className = "status_no";
	}
	
	if(wrs_cc_enable == 1){
		$('cc_service').innerHTML = "Yes";
		$('cc_service').className = "status_yes";
	}
	else{
		$('cc_service').innerHTML = "<a href='ParentalControl_HomeSecurity.asp' target='_blank'>No</a>";
		$('cc_service').className = "status_no";
	}
}

function cal_panel_block(){
	var blockmarginLeft;
	if (window.innerWidth)
		winWidth = window.innerWidth;
	else if ((document.body) && (document.body.clientWidth))
		winWidth = document.body.clientWidth;
		
	if (document.documentElement  && document.documentElement.clientHeight && document.documentElement.clientWidth){
		winWidth = document.documentElement.clientWidth;
	}

	if(winWidth >1050){	
		winPadding = (winWidth-1050)/2;	
		winWidth = 1105;
		blockmarginLeft= (winWidth*0.25)+winPadding;
	}
	else if(winWidth <=1050){
		blockmarginLeft= (winWidth)*0.25+document.body.scrollLeft;	
	}

	$("weakness_div").style.marginLeft = blockmarginLeft+"px";
}
</script>
<style>

</style>
</head>

<body onload="initial();" onunload="unload_body();" onselectstart="return false;">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="weakness_div" class="weakness" style="display:none;">
	<table style="width:99%;">
		<tr>
			<td>
				<div class="weakness_router_status">Router Status</div>
			</td>
		</tr>	
		<tr>
			<td>
				<div>
					<table class="weakness_status" cellspacing="0" cellpadding="4" align="center">
						<tr>
							<th>Default router login username and password changed</th>
							<td>
								<div id="login_password"></div>
							</td>					
						</tr>
						<tr>
							<th>Wireless password strength check</th>
							<td>
								<div id="score"></div>
							</td>			
						</tr>
						<tr>
							<th>Wireless encryption strength</th>
							<td>
								<div id="wireless_encryption"></div>
							</td>			
						</tr>
						<tr>
							<th>UPnP service disabled</th>
							<td>
								<div id="upnp_service"></div>
							</td>	
						</tr>
						<tr>
							<th>Web access from WAN disabled</th>
							<td>
								<div id="access_from_wan"></div>
							</td>	
						</tr>
						<tr>
							<th>PING from WAN disabled</th>
							<td>
								<div id="ping_from_wan"></div>
							</td>	
						</tr>
						<tr>
							<th>DMZ disabled</th>
							<td>
								<div id="dmz_service"></div>
							</td>
						</tr>
						<tr>
							<th>Port trigger disabled.</th>
							<td>
								<div id="port_tirgger"></div>
							</td>
						</tr>
						<tr>
							<th>Port forwarding disabled</th>
							<td>
								<div id="port_forwarding"></div>
							</td>		
						</tr>
						<tr>
							<th>Anonymous login to FTP share disabled</th>
							<td>
								<div id="ftp_account"></div>
							</td>		
						</tr>
						<tr>
							<th>Guest login for Network Place Share disabled</th>
							<td>
								<div id="samba_account"></div>
							</td>		
						</tr>
						<tr>
							<th>Malicious website blocked enabled</th>
							<td>
								<div id="wrs_service"></div>
							</td>	
						</tr>
						<tr>
							<th>Vulnerability protection enabled</th>
							<td>
								<div id="vp_service"></div>
							</td>						
						</tr>
						<tr>
							<th>Infected device detecting and blocked enabled</th>
							<td>
								<div id="cc_service"></div>
							</td>						
						</tr>
					</table>
				</div>
			</td>
		</tr>	
		<tr>
			<td>
				<div style="text-align:center;margin-top:20px;">
					<input class="button_gen" type="button" onclick="close_weakness_status();" value="Close">
					<input class="button_gen_long" type="button" onclick="enable_whole_security();" value="Enable whole">
				</div>
			</td>
		</tr>
	</table>

</div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="ParentalControl_HomeSecurity.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_wrs;restart_firewall">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>" disabled>
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wrs_enable" value="<% nvram_get("wrs_enable"); %>">
<input type="hidden" name="wrs_app_enable" value="<% nvram_get("wrs_app_enable"); %>">
<input type="hidden" name="wrs_cc_enable" value="<% nvram_get("wrs_cc_enable"); %>">
<input type="hidden" name="wrs_vp_enable" value="<% nvram_get("wrs_vp_enable"); %>">
<input type="hidden" name="http_username" value="<% nvram_get("http_username"); %>" disabled>
<input type="hidden" name="http_passwd" value="<% nvram_get("http_passwd"); %>" disabled>
<input type="hidden" name="wl_wpa_psk" value="<% nvram_get("wl_wpa_psk"); %>" disabled>
<input type="hidden" name="wl_auth_mode_x" value="<% nvram_get("wl_auth_mode_x"); %>" disabled>
<input type="hidden" name="wan_upnp_enable" value="<% nvram_get("wan_upnp_enable"); %>" disabled>
<input type="hidden" name="wan_unit" value="<% nvram_get("wan_unit"); %>" disabled>
<input type="hidden" name="misc_http_x" value="<% nvram_get("misc_http_x"); %>" disabled>
<input type="hidden" name="misc_ping_x" value="<% nvram_get("misc_ping_x"); %>" disabled>
<input type="hidden" name="dmz_ip" value="<% nvram_get("dmz_ip"); %>" disabled>
<input type="hidden" name="autofw_enable_x" value="<% nvram_get("autofw_enable_x"); %>" disabled>
<input type="hidden" name="vts_enable_x" value="<% nvram_get("vts_enable_x"); %>" disabled>

<table class="content" align="center" cellpadding="0" cellspacing="0" >
	<tr>
		<td width="17">&nbsp;</td>		
		<td valign="top" width="202">				
			<div  id="mainMenu"></div>	
			<div  id="subMenu"></div>		
		</td>					
		<td valign="top">
			<div id="tabMenu" class="submenuBlock"></div>	
		<!--===================================Beginning of Main Content===========================================-->		
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0" >
				<tr>
					<td valign="top" >		
						<table width="730px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
							<tr>
								<td style="background:#4D595D" valign="top">
									<div>&nbsp;</div>
									<div>
										<table width="730px">
											<tr>
												<td align="left">
													<span class="formfonttitle">AiProtection - Home Protection</span>
												</td>
											</tr>
										</table>
									</div>									
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div id="PC_desc">
										<table width="700px" style="margin-left:25px;">
											<tr>
												<td>
													<img id="guest_image" src="/images/New_ui/HomeProtection.png">
												</td>
												<td>&nbsp;&nbsp;</td>
												<td style="font-style:italic;font-size:14px;">
													<span>Home Protection keeps your home network in secure and safe with Trend Micro security. Enable these features to prevent your networking from virus infection and hacker invading</span>					
													<div style="margin-top:5px;">
														<img src="/images/New_ui/Home_Protection_Scenario.png">
													</div>	
												</td>
											</tr>									
										</table>
									</div>
									<!--=====Beginning of Main Content=====-->
									<div style="margin-top:20px;">
										<table style="width:99%;border-collapse:collapse;">
											<tr style="background-color:#444f53;height:120px;">
												<td style="width:25%;border-radius:10px 0px 0px 10px;background-color:#444f53;">
													<div style="text-align:center;background: url('/images/New_ui/AiProtection.png');width:130px;height:130px;margin-left:30px;"></div>	
												</td>
												 <td width="6px">
													<div><img src="/images/cloudsync/line.png"></div>
												</td>
												<td style="padding:10px;">
													<div style="font-size:18px;text-shadow:1px 1px 0px black;">Router weakness scan</div>
													<div style="font-style: italic;font-size: 14px;color:#FC0;height:auto;padding-top:5px;">Check secuirty related configuration for your router.</div>
												</td>
												 <td width="6px">
													<div><img src="/images/cloudsync/line.png"></div>
												</td>
												<td style="width:20%;border-radius:0px 10px 10px 0px;">
													<div>
														<input class="button_gen" type="button" onclick="check_weakness();" value="Scan">
													</div>
												</td>										
											</tr>
											
											<tr style="height:10px;"></tr>
											<tr style="background-color:#444f53;height:120px;">
												<td style="width:25%;border-radius:10px 0px 0px 10px;background-color:#444f53;">
													<div style="text-align:center;background: url('/images/New_ui/AiProtection.png');width:130px;height:130px;margin-left:30px;background-position:0px -187px;"></div>	
												</td>
												 <td width="6px">
													<div style="background:url('/images/cloudsync/line.png') no-repeat;width:6px;height:185px;background-size:4px 185px;";></div>
												</td>
												<td style="padding:10px;">
													<div>
														<table>
															<tr>
																<td style="padding-bottom:10px;">
																	<div style="font-size:18px;text-shadow:1px 1px 0px black;">Malicious sites blocked</div>
																	<div style="font-style: italic;font-size: 14px;color:#FC0;height:auto;padding-top:5px;">Malicious sites blocked allow to limit access contents with spyware, phishing and spam to protect your client device.</div>							
																</td>
															</tr>										
															<tr>
																<td>
																	<div style="font-size:18px;text-shadow:1px 1px 0px black;">Vulnerability Protection</div>
																	<div style="font-style: italic;font-size: 14px;color:#FC0;height:auto;padding-top:5px;">Prevent your network devices from being exposed to the threats on Internet. Enable Vulnerability Protection to prevent threats from outer of home.</div>
																</td>
															</tr>
														</table>
													</div>								
												</td>
												 <td width="6px">
													<div style="background:url('/images/cloudsync/line.png') no-repeat;width:6px;height:185px;background-size:4px 185px;";></div>
												</td>
												<td style="width:20%;border-radius:0px 10px 10px 0px;">
													<div>
														<table>
															<tr>
																<td>
																	<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_command_enable"></div>
																	<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
																		<script type="text/javascript">
																			$j('#radio_command_enable').iphoneSwitch('<% nvram_get("wrs_enable"); %>',
																				function(){
																					document.form.wrs_enable.value = 1;
																					document.form.wrs_app_enable.value = 1;
																					applyRule();																				
																				},
																				function(){
																					document.form.wrs_enable.value = 0;
																					document.form.wrs_app_enable.value = 0;	
																					if(document.form.wrs_cc_enable.value == 1){
																						document.form.wrs_cc_enable.value = 0;
																					}
																					
																					applyRule();
																				},
																						{
																						switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
																				});
																		</script>			
																	</div>
																</td>													
															</tr>
															<tr>
																<td>
																	<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_virtual_patch_enable"></div>
																	<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
																		<script type="text/javascript">
																			$j('#radio_virtual_patch_enable').iphoneSwitch('<% nvram_get("wrs_vp_enable"); %>',
																				function(){
																					document.form.wrs_vp_enable.value = 1;	
																					applyRule();	
																				},
																				function(){
																					document.form.wrs_vp_enable.value = 0;	
																					applyRule();	
																				},
																						{
																						switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
																				});
																		</script>			
																	</div>
																</td>													
															</tr>
														</table>
													</div>
												</td>										
											</tr>
											<tr style="height:10px;"></tr>										
											<tr style="background-color:#444f53;height:120px;">
												<td style="width:25%;border-radius:10px 0px 0px 10px;background-color:#444f53;">
													<div style="text-align:center;background: url('/images/New_ui/AiProtection.png');width:130px;height:140px;margin-left:30px;background-position:0px -385px";></div>	
												</td>
												 <td width="6px">
													<div><img src="/images/cloudsync/line.png"></div>
												</td>
												<td style="padding:10px;">
													<div style="font-size:18px;text-shadow:1px 1px 0px black;">Infected device detection and blocked</div>
													<div style="font-style: italic;font-size: 14px;color:#FC0;height:auto;;padding-top:5px;">Prevents data stealing and device remote controlled form hacker. Any suspicious connections and activities will be identified and blocked to protect your presonal information safe.</div>
												</td>
												 <td width="6px">
													<div><img src="/images/cloudsync/line.png"></div>
												</td>
												<td style="width:20%;border-radius:0px 10px 10px 0px;">
													<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="infect_enable"></div>
													<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
														<script type="text/javascript">
															$j('#infect_enable').iphoneSwitch('<% nvram_get("wrs_cc_enable"); %>',
																function(){
																	document.form.wrs_enable.value = 1;
																	document.form.wrs_app_enable.value = 1;
																	document.form.wrs_cc_enable.value = 1;	
																	applyRule();	
																},
																function(){
																	document.form.wrs_enable.value = 0;
																	document.form.wrs_app_enable.value = 0;
																	document.form.wrs_cc_enable.value = 0;
																	applyRule();
																},
																		{
																switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
															});
														</script>			
													</div>
												</td>										
											</tr>
										</table>
									</div>
									<div style="width:100px;height:48px;margin:10px 0px 0px 625px;background-image:url('images/New_ui/tm_logo.png');"></div>
								</td>
							</tr>
							</tbody>	
						</table>
					</td>         
				</tr>
			</table>				
		<!--===================================Ending of Main Content===========================================-->		
		</td>		
		<td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>
<div id="footer"></div>
</form>
</body>
</html>
