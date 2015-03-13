<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
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
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
<style>
.weakness{
	width:650px;
	height:570px;
	position:absolute;
	background: rgba(0,0,0,0.8);
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
	width:100px;

}

.status_no{
	background-color:#FF7575;
}
.status_no a{
	text-decoration:underline;}
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
var button_flag = 0;
var ctf_disable = '<% nvram_get("ctf_disable"); %>';
var ctf_fa_mode = '<% nvram_get("ctf_fa_mode"); %>';

function initial(){
	show_menu();
	document.getElementById("_AiProtection_HomeSecurity").innerHTML = '<table><tbody><tr><td><div class="_AiProtection_HomeSecurity"></div></td><td><div style="width:120px;">AiProtection</div></td></tr></tbody></table>';
	document.getElementById("_AiProtection_HomeSecurity").className = "menu_clicked";
}

function applyRule(){
	if(ctf_disable == 0 && ctf_fa_mode == 2){
		if(!confirm(Untranslated.ctf_fa_hint)){
			return false;
		}	
		else{
			document.form.action_script.value = "reboot";
			document.form.action_wait.value = "<% nvram_get("reboot_time"); %>";
		}	
	}

	showLoading();	
	document.form.submit();
}

function check_weakness(){
	cal_panel_block();
	$j('#weakness_div').fadeIn();
	check_login_name_password();
	check_wireless_password();
	check_wireless_encryption();
	check_WPS();
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
    if(!confirm("<#AiProtection_Vulnerability_confirm#>")){
		return false;
	}
	
	var action_script_temp = "";
	var wan0_upnp_enable = document.form.wan0_upnp_enable.value; 
	var wan1_upnp_enable = document.form.wan1_upnp_enable.value; 
	var wan_access_enable = document.form.misc_http_x.value;
	var wan_ping_enable = document.form.misc_ping_x.value;
	var port_trigger_enable = document.form.autofw_enable_x.value;
	var port_forwarding_enable = document.form.vts_enable_x.value;
	var ftp_account_mode = get_manage_type('ftp');
	var samba_account_mode = get_manage_type('cifs');
	var wrs_cc_enable = document.form.wrs_cc_enable.value;
	var wrs_vp_enable = document.form.wrs_vp_enable.value;
	var wrs_mals_enable = document.form.wrs_mals_enable.value;
	var wps_enable = document.form.wps_enable.value;
	var restart_wan = 0;
	var restart_time = 0;
	var restart_firewall = 0;
	var restart_wrs = 0;
	var restart_wireless = 0;
	
	if(wps_enable == 1){
		document.form.wps_enable.value = 0;
		document.form.wps_sta_pin.value = "";
		document.form.wps_enable.disabled = false;
		document.form.wps_sta_pin.disabled = false;
		restart_wireless = 1;
	}
	
	if(wan0_upnp_enable == 1 || wan1_upnp_enable == 1){		
		document.form.wan0_upnp_enable.value = 0;
		document.form.wan1_upnp_enable.value = 0;
		document.form.wan0_upnp_enable.disabled = false;
		document.form.wan1_upnp_enable.disabled = false;
		restart_wan = 1;
	}
	
	if(wan_access_enable ==1){
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
	
	if(ftp_account_mode == 0 || samba_account_mode == 0){	
		switchAccount();
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

	if(wrs_mals_enable == 0){
		document.form.wrs_mals_enable.value = 1;
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
	
	if(restart_wireless == 1){
		if(action_script_temp == ""){
			action_script_temp += "restart_wireless";
		}	
		else{
			action_script_temp += ";restart_wireless";	
		}	
	}
	
	document.form.action_script.value = action_script_temp;
	document.form.submit();
	if(ftp_account_mode == 0 || samba_account_mode == 0){
		document.samba_Form.submit();
	}		
}
function check_login_name_password(){
	var username = document.form.http_username.value;
	var password = document.form.http_passwd.value;

	if(username == "admin" || password == "admin"){
		$('login_password').innerHTML = "<a href='Advanced_System_Content.asp' target='_blank'><#checkbox_No#></a>";
		$('login_password').className = "status_no";	
		$('login_password').onmouseover = function(){overHint(10);}
		$('login_password').onmouseout = function(){nd();}
	}
	else{
		$('login_password').innerHTML = "<#checkbox_Yes#>";
		$('login_password').className = "status_yes";
	}
}

function check_wireless_password(){
	var wireless_password = document.form.wl_wpa_psk.value;
	chkPass(document.form.wl_wpa_psk.value,"http_passwd");
	if($('score').className == "status_no")
	{
		$('score').onmouseover = function(){overHint(11);}
		$('score').onmouseout = function(){nd();}
	}
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
		$('wireless_encryption').onmouseover = function(){overHint(12);}
		$('wireless_encryption').onmouseout = function(){nd();}
	}
}

function check_WPS(){
	var wps_enable = document.form.wps_enable.value;
	if(wps_enable == 0){
		$('wps_status').innerHTML = "<#checkbox_Yes#>";
		$('wps_status').className = "status_yes";
	}
	else{
		$('wps_status').innerHTML = "<a href='Advanced_WWPS_Content.asp' target='_blank'><#checkbox_No#></a>";
		$('wps_status').className = "status_no";	
		$('wps_status').onmouseover = function(){overHint(25);}
		$('wps_status').onmouseout = function(){nd();}
	}
}

function check_upnp(){
	var wan0_unpn_enable = document.form.wan0_upnp_enable.value;
	var wan1_unpn_enable = document.form.wan1_upnp_enable.value;
	
	if(wan0_unpn_enable == 0 && wan1_unpn_enable == 0){
		$('upnp_service').innerHTML = "<#checkbox_Yes#>";
		$('upnp_service').className = "status_yes";
	}
	else{
		$('upnp_service').innerHTML = "<a href='Advanced_WAN_Content.asp' target='_blank'><#checkbox_No#></a>";
		$('upnp_service').className = "status_no";
		$('upnp_service').onmouseover = function(){overHint(13);}
		$('upnp_service').onmouseout = function(){nd();}
	}
}

function check_wan_access(){
	var wan_access_enable = document.form.misc_http_x.value;

	if(wan_access_enable == 0){
		$('access_from_wan').innerHTML = "<#checkbox_Yes#>";
		$('access_from_wan').className = "status_yes";
	}
	else{
		$('access_from_wan').innerHTML = "<a href='Advanced_System_Content.asp' target='_blank'><#checkbox_No#></a>";
		$('access_from_wan').className = "status_no";
		$('access_from_wan').onmouseover = function(){overHint(14);}
		$('access_from_wan').onmouseout = function(){nd();}
	}
}

function check_ping_form_wan(){
	var wan_ping_enable = document.form.misc_ping_x.value;

	if(wan_ping_enable == 0){
		$('ping_from_wan').innerHTML = "<#checkbox_Yes#>";
		$('ping_from_wan').className = "status_yes";
	}
	else{
		$('ping_from_wan').innerHTML = "<a href='Advanced_BasicFirewall_Content.asp' target='_blank'><#checkbox_No#></a>";
		$('ping_from_wan').className = "status_no";
		$('ping_from_wan').onmouseover = function(){overHint(15);}
		$('ping_from_wan').onmouseout = function(){nd();}
	}
}

function check_dmz(){
	if(document.form.dmz_ip.value == ""){
		$('dmz_service').innerHTML = "<#checkbox_Yes#>";
		$('dmz_service').className = "status_yes";
	}
	else{
		$('dmz_service').innerHTML = "<a href='Advanced_Exposed_Content.asp' target='_blank'><#checkbox_No#></a>";
		$('dmz_service').className = "status_no";
		$('dmz_service').onmouseover = function(){overHint(16);}
		$('dmz_service').onmouseout = function(){nd();}
	}
}

function check_port_trigger(){
	var port_trigger_enable = document.form.autofw_enable_x.value;

	if(port_trigger_enable == 0){
		$('port_tirgger').innerHTML = "<#checkbox_Yes#>";
		$('port_tirgger').className = "status_yes";
	}
	else{
		$('port_tirgger').innerHTML = "<a href='Advanced_PortTrigger_Content.asp' target='_blank'><#checkbox_No#></a>";
		$('port_tirgger').className = "status_no";
		$('port_tirgger').onmouseover = function(){overHint(17);}
		$('port_tirgger').onmouseout = function(){nd();}
	}

}

function check_port_forwarding(){
	var port_forwarding_enable = document.form.vts_enable_x.value;

	if(port_forwarding_enable == 0){
		$('port_forwarding').innerHTML = "<#checkbox_Yes#>";
		$('port_forwarding').className = "status_yes";
	}
	else{
		$('port_forwarding').innerHTML = "<a href='Advanced_VirtualServer_Content.asp' target='_blank'><#checkbox_No#></a>";
		$('port_forwarding').className = "status_no";
		$('port_forwarding').onmouseover = function(){overHint(18);}
		$('port_forwarding').onmouseout = function(){nd();}
	}
}

function check_ftp_anonymous(){
	var ftp_account_mode = get_manage_type('ftp');		//0: shared mode, 1: account mode
	
	if(ftp_account_mode == 0){
		$('ftp_account').innerHTML = "<a href='Advanced_AiDisk_ftp.asp' target='_blank'><#checkbox_No#></a>";
		$('ftp_account').className = "status_no";
		$('ftp_account').onmouseover = function(){overHint(19);}
		$('ftp_account').onmouseout = function(){nd();}
	}
	else{
		$('ftp_account').innerHTML = "<#checkbox_Yes#>";
		$('ftp_account').className = "status_yes";
	}
}

function check_samba_anonymous(){
	var samba_account_mode = get_manage_type('cifs');
	
	if(samba_account_mode == 0){
		$('samba_account').innerHTML = "<a href='Advanced_AiDisk_samba.asp' target='_blank'><#checkbox_No#></a>";
		$('samba_account').className = "status_no";
		$('samba_account').onmouseover = function(){overHint(20);}
		$('samba_account').onmouseout = function(){nd();}
	}
	else{
		$('samba_account').innerHTML = "<#checkbox_Yes#>";
		$('samba_account').className = "status_yes";
	}
}

function check_TM_feature(){
	var wrs_cc_enable = document.form.wrs_cc_enable.value;
	var wrs_vp_enable = document.form.wrs_vp_enable.value;
	var wrs_mals_enable = document.form.wrs_mals_enable.value;

	if(wrs_mals_enable == 1){
		$('wrs_service').innerHTML = "<#checkbox_Yes#>";
		$('wrs_service').className = "status_yes";
	}
	else{
		$('wrs_service').innerHTML = "<a href='AiProtection_HomeProtection.asp' target='_blank'><#checkbox_No#></a>";
		$('wrs_service').className = "status_no";
		$('wrs_service').onmouseover = function(){overHint(21);}
		$('wrs_service').onmouseout = function(){nd();}
	}

	if(wrs_vp_enable == 1){
		$('vp_service').innerHTML = "<#checkbox_Yes#>";
		$('vp_service').className = "status_yes";
	}
	else{
		$('vp_service').innerHTML = "<a href='AiProtection_HomeProtection.asp' target='_blank'><#checkbox_No#></a>";
		$('vp_service').className = "status_no";
		$('vp_service').onmouseover = function(){overHint(22);}
		$('vp_service').onmouseout = function(){nd();}
	}
	
	if(wrs_cc_enable == 1){
		$('cc_service').innerHTML = "<#checkbox_Yes#>";
		$('cc_service').className = "status_yes";
	}
	else{
		$('cc_service').innerHTML = "<a href='AiProtection_HomeProtection.asp' target='_blank'><#checkbox_No#></a>";
		$('cc_service').className = "status_no";
		$('cc_service').onmouseover = function(){overHint(23);}
		$('cc_service').onmouseout = function(){nd();}
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

	if($("weakness_div"))
		$("weakness_div").style.marginLeft = blockmarginLeft+"px";
		
	if($("alert_preference"))
		$("alert_preference").style.marginLeft = blockmarginLeft+"px";	
}

function switchAccount(){
	document.samba_Form.action = "/aidisk/switch_share_mode.asp";
	document.ftp_Form.action = "/aidisk/switch_share_mode.asp";
	document.samba_Form.protocol.value = "cifs";
	document.ftp_Form.protocol.value = "ftp";
	document.samba_Form.mode.value = "account";
	document.ftp_Form.mode.value = "account";
}

function resultOfSwitchShareMode(){
	document.ftp_Form.submit();
	setTimeout("refreshpage();",3000);
}

function show_tm_eula(){
	if(document.form.preferred_lang.value == "JP"){
			$j.get("JP_tm_eula.htm", function(data){
				$('agreement_panel').innerHTML= data;
			});
	}
	else{
			$j.get("tm_eula.htm", function(data){
				$('agreement_panel').innerHTML= data;
			});
	}
	dr_advise();
	cal_agreement_block();
	$j("#agreement_panel").fadeIn(300);
}

function cal_agreement_block(){
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

	$("agreement_panel").style.marginLeft = blockmarginLeft+"px";
}

function cancel(){
	button_flag = 0;
	refreshpage();
}

function eula_confirm(){
	document.form.TM_EULA.value = 1;
	if(button_flag == 1){
		document.form.wrs_mals_enable.value = 1;
	}
	else if(button_flag == 2){
		document.form.wrs_vp_enable.value = 1;
	}
	else{
		document.form.wrs_cc_enable.value = 1;
	}
	
	applyRule();
}

function show_alert_preference(){
	cal_panel_block();
	$j('#alert_preference').fadeIn(300);
	$('mail_address').value = document.form.PM_MY_EMAIL.value;
	$('mail_password').value = document.form.PM_SMTP_AUTH_PASS.value;
}

function close_alert_preference(){
	$j('#alert_preference').fadeOut(100);
}

function apply_alert_preference(){
	var address_temp = $('mail_address').value;
	var account_temp = $('mail_address').value.split("@");
	var smtpList = new Array();
	smtpList = [
		{smtpServer: "smtp.gmail.com", smtpPort: "587", smtpDomain: "gmail.com"},
		{end: 0}
	];
	
	if(address_temp.indexOf('@') != -1){
		if(account_temp[1] != "gmail.com"){
			alert("Wrong mail domain");
			$('mail_address').focus();
			return false;
		}	
		
		document.form.PM_MY_EMAIL.value = address_temp;	
	}
	else{	
		document.form.PM_MY_EMAIL.value = account_temp[0] + "@" +smtpList[0].smtpDomain;
	}
	
	document.form.PM_SMTP_AUTH_USER.value = account_temp[0];
	document.form.PM_SMTP_AUTH_PASS.value = $('mail_password').value;	
	document.form.PM_SMTP_SERVER.value = smtpList[0].smtpServer;	
	document.form.PM_SMTP_PORT.value = smtpList[0].smtpPort;	
	$j('#alert_preference').fadeOut(100);
	document.form.submit();
}
</script>
</head>

<body onload="initial();" onunload="unload_body();" onselectstart="return false;">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="agreement_panel" class="panel_folder" style="margin-top: -100px;display:none;position:absolute;"></div>
<div id="hiddenMask" class="popup_bg" style="z-index:999;">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center"></table>
	<!--[if lte IE 6.5.]><script>alert("<#ALERT_TO_CHANGE_BROWSER#>");</script><![endif]-->
</div>
<div id="weakness_div" class="weakness" style="display:none;">
	<table style="width:99%;">
		<tr>
			<td>
				<div class="weakness_router_status"><#AiProtection_scan#></div>
			</td>
		</tr>	
		<tr>
			<td>
				<div>
					<table class="weakness_status" cellspacing="0" cellpadding="4" align="center">
						<tr>
							<th><#AiProtection_scan_item1#> -</th>
							<td>
								<div id="login_password"></div>
							</td>					
						</tr>
						<tr>
							<th><#AiProtection_scan_item2#> -</th>
							<td>
								<div id="score"></div>
							</td>			
						</tr>
						<tr>
							<th><#AiProtection_scan_item3#> -</th>
							<td>
								<div id="wireless_encryption"></div>
							</td>			
						</tr>
						<tr>
							<th>WPS disabled -</th>
							<td>
								<div id="wps_status"></div>
							</td>			
						</tr>
						<tr>
							<th><#AiProtection_scan_item4#> -</th>
							<td>
								<div id="upnp_service"></div>
							</td>	
						</tr>
						<tr>
							<th><#AiProtection_scan_item5#> -</th>
							<td>
								<div id="access_from_wan"></div>
							</td>	
						</tr>
						<tr>
							<th><#AiProtection_scan_item6#> -</th>
							<td>
								<div id="ping_from_wan"></div>
							</td>	
						</tr>
						<tr>
							<th><#AiProtection_scan_item7#> -</th>
							<td>
								<div id="dmz_service"></div>
							</td>
						</tr>
						<tr>
							<th><#AiProtection_scan_item8#> -</th>
							<td>
								<div id="port_tirgger"></div>
							</td>
						</tr>
						<tr>
							<th><#AiProtection_scan_item9#> -</th>
							<td>
								<div id="port_forwarding"></div>
							</td>		
						</tr>
						<tr>
							<th><#AiProtection_scan_item10#> -</th>
							<td>
								<div id="ftp_account"></div>
							</td>		
						</tr>
						<tr>
							<th><#AiProtection_scan_item11#> -</th>
							<td>
								<div id="samba_account"></div>
							</td>		
						</tr>
						<tr>
							<th><#AiProtection_scan_item12#> -</th>
							<td>
								<div id="wrs_service"></div>
							</td>	
						</tr>
						<tr>
							<th><#AiProtection_scan_item13#> -</th>
							<td>
								<div id="vp_service"></div>
							</td>						
						</tr>
						<tr>
							<th><#AiProtection_detection_blocking#> -</th>
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
				<div style="text-align:center;margin-top:10px;">
					<input class="button_gen" type="button" onclick="close_weakness_status();" value="<#CTL_close#>">
					<input class="button_gen_long" type="button" onclick="enable_whole_security();" value="<#CTL_secure#>">
				</div>
			</td>
		</tr>
	</table>

</div>

<div id="alert_preference" style="width:650px;height:270px;position:absolute;background:rgba(0,0,0,0.8);z-index:10;border-radius:10px;margin-left:260px;padding:15px 10px 20px 10px;display:none">
	<table style="width:99%">
		<tr>
			<th>
				<div style="font-size:16px;"><#AiProtection_alert_pref#></div>		
			</th>		
		</tr>
			<td>
				<div class="formfontdesc" style="font-style: italic;font-size: 14px;"><#AiProtection_HomeDesc1#></div>
			</td>
		<tr>
			<td>
				<table class="FormTable" width="99%" border="1" align="center" cellpadding="4" cellspacing="0">
					<tr>
						<th><#Provider#></th>
						<td>
							<div>
								<select class="input_option" id="mail_provider">
									<option value="0">Google</option>							
								</select>
							</div>
						</td>
					</tr>
					<!--tr>
						<th>Administrator</th>
						<td>
							<div>
								<input type="type" class="input_20_table" name="user_name" value="">
							</div>
						</td>
					</tr-->
					<tr>
						<th>Email</th>
						<td>
							<div>
								<input type="type" class="input_30_table" id="mail_address" value="">
							</div>
						</td>
					</tr>
					<tr>
						<th><#PPPConnection_Password_itemname#></th>
						<td>
							<div>
								<input type="password" class="input_30_table" id="mail_password" maxlength="100" value="">
							</div>
						</td>
					</tr>
				</table>
			</td>
		</tr>
		<tr>
			<td>
				<div style="text-align:center;margin-top:20px;">
					<input class="button_gen" type="button" onclick="close_alert_preference();" value="<#CTL_close#>">
					<input class="button_gen_long" type="button" onclick="apply_alert_preference();" value="<#CTL_apply#>">
				</div>
			</td>		
		</tr>
	</table>
</div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="samba_Form" action="" target="hidden_frame">
<input type="hidden" name="protocol" id="protocol" value="">
<input type="hidden" name="mode" id="mode" value="">
<input type="hidden" name="account" id="account" value="">
</form>
<form method="post" name="ftp_Form" action="" target="hidden_frame">
<input type="hidden" name="protocol" id="protocol" value="">
<input type="hidden" name="mode" id="mode" value="">
<input type="hidden" name="account" id="account" value="">
</form>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="AiProtection_HomeProtection.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_wrs;restart_firewall">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>" disabled>
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wrs_mals_enable" value="<% nvram_get("wrs_mals_enable"); %>">
<input type="hidden" name="wrs_cc_enable" value="<% nvram_get("wrs_cc_enable"); %>">
<input type="hidden" name="wrs_vp_enable" value="<% nvram_get("wrs_vp_enable"); %>">
<input type="hidden" name="http_username" value="<% nvram_get("http_username"); %>" disabled>
<input type="hidden" name="http_passwd" value="<% nvram_get("http_passwd"); %>" disabled>
<input type="hidden" name="wl_wpa_psk" value="<% nvram_get("wl_wpa_psk"); %>" disabled>
<input type="hidden" name="wl_auth_mode_x" value="<% nvram_get("wl_auth_mode_x"); %>" disabled>
<input type="hidden" name="wan0_upnp_enable" value="<% nvram_get("wan0_upnp_enable"); %>" disabled>
<input type="hidden" name="wan1_upnp_enable" value="<% nvram_get("wan1_upnp_enable"); %>" disabled>
<input type="hidden" name="misc_http_x" value="<% nvram_get("misc_http_x"); %>" disabled>
<input type="hidden" name="misc_ping_x" value="<% nvram_get("misc_ping_x"); %>" disabled>
<input type="hidden" name="dmz_ip" value="<% nvram_get("dmz_ip"); %>" disabled>
<input type="hidden" name="autofw_enable_x" value="<% nvram_get("autofw_enable_x"); %>" disabled>
<input type="hidden" name="vts_enable_x" value="<% nvram_get("vts_enable_x"); %>" disabled>
<input type="hidden" name="wps_enable" value="<% nvram_get("wps_enable"); %>" disabled>
<input type="hidden" name="wps_sta_pin" value="<% nvram_get("wps_sta_pin"); %>" disabled>
<input type="hidden" name="TM_EULA" value="<% nvram_get("TM_EULA"); %>">
<input type="hidden" name="PM_SMTP_SERVER" value="<% nvram_get("PM_SMTP_SERVER"); %>">
<input type="hidden" name="PM_SMTP_PORT" value="<% nvram_get("PM_SMTP_PORT"); %>">
<input type="hidden" name="PM_MY_EMAIL" value="<% nvram_get("PM_MY_EMAIL"); %>">
<input type="hidden" name="PM_SMTP_AUTH_USER" value="<% nvram_get("PM_SMTP_AUTH_USER"); %>">
<input type="hidden" name="PM_SMTP_AUTH_PASS" value="">

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
													<span class="formfonttitle">AiProtection - <#AiProtection_Home#></span>
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
													<table>
														<tr>
															<td>
																<div style="width:430px"><#AiProtection_HomeDesc2#></div>
																<div style="width:430px"><a style="text-decoration:underline;" href="http://www.asus.com/us/support/FAQ/1008719/" target="_blank"><#AiProtection_Home#> FAQ</a></div>
															</td>
															<td>
																<div style="width:100px;height:48px;margin-left:-40px;background-image:url('images/New_ui/tm_logo.png');"></div>
															</td>
														</tr>
														<tr>
															<td rowspan="2">
																<div>
																	<img src="/images/New_ui/Home_Protection_Scenario.png">
																</div>
															</td>
														</tr>												
													</table>
												</td>
											</tr>									
										</table>
									</div>
									<!--=====Beginning of Main Content=====-->
									<div style="margin-top:5px;">
										<table style="width:99%;border-collapse:collapse;">
											<tr style="background-color:#444f53;height:120px;">
												<td style="width:25%;border-radius:10px 0px 0px 10px;background-color:#444f53;">
													<div style="text-align:center;background: url('/images/New_ui/AiProtection.png');width:130px;height:130px;margin-left:30px;"></div>	
												</td>
												 <td width="6px">
													<div><img src="/images/cloudsync/line.png"></div>
												</td>
												<td style="padding:10px;">
													<div style="font-size:18px;text-shadow:1px 1px 0px black;"><#AiProtection_scan#></div>
													<div style="font-style: italic;font-size: 14px;color:#FC0;height:auto;padding-top:5px;"><#AiProtection_scan_desc#></div>
												</td>
												 <td width="6px">
													<div><img src="/images/cloudsync/line.png"></div>
												</td>
												<td style="width:20%;border-radius:0px 10px 10px 0px;">
													<div>
														<input class="button_gen" type="button" onclick="check_weakness();" value="<#CTL_scan#>">
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
																	<div style="font-size:18px;text-shadow:1px 1px 0px black;"><#AiProtection_sites_blocking#></div>
																	<div style="font-style: italic;font-size: 14px;color:#FC0;height:auto;padding-top:5px;"><#AiProtection_sites_block_desc#></div>
																</td>
															</tr>										
															<tr>
																<td>
																	<div style="font-size:18px;text-shadow:1px 1px 0px black;"><#AiProtection_Vulnerability#></div>
																	<div style="font-style: italic;font-size: 14px;color:#FC0;height:auto;padding-top:5px;"><#AiProtection_Vulnerability_desc#></div>
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
																	<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_mals_enable"></div>
																	<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
																		<script type="text/javascript">
																			$j('#radio_mals_enable').iphoneSwitch('<% nvram_get("wrs_mals_enable"); %>',
																				function(){																					
																					if(document.form.TM_EULA.value == 0){
																						button_flag = 1;
																						show_tm_eula();
																						return;
																					}
																				
																					document.form.wrs_mals_enable.value = 1;
																					applyRule();																				
																				},
																				function(){
																					document.form.wrs_mals_enable.value = 0;
																					applyRule();
																				}
																			);
																		</script>			
																	</div>
																</td>													
															</tr>
															<tr>
																<td>
																	<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_vp_enable"></div>
																	<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
																		<script type="text/javascript">
																			$j('#radio_vp_enable').iphoneSwitch('<% nvram_get("wrs_vp_enable"); %>',
																				function(){																				
																					if(document.form.TM_EULA.value == 0){
																						button_flag = 2;
																						show_tm_eula();
																						return;
																					}
																				
																					document.form.wrs_vp_enable.value = 1;	
																					applyRule();	
																				},
																				function(){
																					document.form.wrs_vp_enable.value = 0;	
																					applyRule();	
																				}
																			);
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
													<div style="font-size:18px;text-shadow:1px 1px 0px black;"><#AiProtection_detection_blocking#></div>
													<div style="font-style: italic;font-size: 14px;color:#FC0;height:auto;;padding-top:5px;"><#AiProtection_detection_block_desc#></div>
												</td>
												 <td width="6px">
													<div><img src="/images/cloudsync/line.png"></div>
												</td>
												<td style="width:20%;border-radius:0px 10px 10px 0px;">
													<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_cc_enable"></div>
													<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
														<script type="text/javascript">
															$j('#radio_cc_enable').iphoneSwitch('<% nvram_get("wrs_cc_enable"); %>',
																function(){																
																	if(document.form.TM_EULA.value == 0){
																		button_flag = 3;
																		show_tm_eula();
																		return;
																	}
																
																	document.form.wrs_cc_enable.value = 1;	
																	applyRule();	
																},
																function(){
																	document.form.wrs_cc_enable.value = 0;
																	applyRule();
																}
															);
														</script>			
													</div>
													<div style="margin-top:-15px;">
														<input class="button_gen_long" type="button" onclick="show_alert_preference();" value="<#AiProtection_alert_pref#>">
													</div>
												</td>										
											</tr>
										</table>
									</div>
									<div style="width:135px;height:55px;position:absolute;bottom:5px;right:5px;background-image:url('images/New_ui/tm_logo_power.png');"></div>
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
