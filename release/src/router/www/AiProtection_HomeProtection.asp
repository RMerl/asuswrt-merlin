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
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
<script type="text/javascript" src="/form.js"></script>
<style>
.weakness{
	width:650px;
	height:590px;
	position:absolute;
	background: rgba(0,0,0,0.8);
	z-index:10;
	margin-left:300px;
	border-radius:10px;
	display: none;
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

.alertpreference{
	width:650px;
	height:290px;
	position:absolute;
	background: rgba(0,0,0,0.8);
	z-index:10;
	margin-left:260px;
	border-radius:10px;
	padding:15px 10px 20px 10px;
	display: none;
}
</style>
<script>
window.onresize = function() {
	if(document.getElementById("weakness_div").style.display == "block") {
		cal_panel_block("weakness_div", 0.25);
	}
	if(document.getElementById("alert_preference").style.display == "block") {
		cal_panel_block("alert_preference", 0.25);
	}
	if(document.getElementById("agreement_panel").style.display == "block") {
		cal_panel_block("agreement_panel", 0.25);
	}
} 
<% get_AiDisk_status(); %>
var AM_to_cifs = get_share_management_status("cifs");  // Account Management for Network-Neighborhood
var AM_to_ftp = get_share_management_status("ftp");  // Account Management for FTP

var button_flag = 0;
var ctf_disable = '<% nvram_get("ctf_disable"); %>';
var ctf_fa_mode = '<% nvram_get("ctf_fa_mode"); %>';

function initial(){
	show_menu();
	document.getElementById("_AiProtection_HomeSecurity").innerHTML = '<table><tbody><tr><td><div class="_AiProtection_HomeSecurity"></div></td><td><div style="width:120px;"><#AiProtection_title#></div></td></tr></tbody></table>';
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
	cal_panel_block("weakness_div", 0.25);
	$('#weakness_div').fadeIn();
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
	$('#weakness_div').fadeOut(100);
}

function enable_whole_security(){
    if(!confirm("<#AiProtection_Vulnerability_confirm#>")){
		return false;
	}
	
	var action_script_temp = "";
	var wan0_upnp_enable = document.form.wan0_upnp_enable.value; 
        var wans_dualwan_orig = document.form.wans_dualwan.value;
	if(wans_dualwan_orig.search(" ") == -1)
		var wans_flag = 0;
	else
		var wans_flag = (wans_dualwan_orig.search("none") == -1) ? 1:0;
	if (wans_flag == 1)
		var wan1_upnp_enable = document.form.wan1_upnp_enable.value;
	else
		var wan1_upnp_enable = 0;
	var wan_access_enable = document.form.misc_http_x.value;
	var wan_ping_enable = document.form.misc_ping_x.value;
	var port_trigger_enable = document.form.autofw_enable_x.value;
	var port_forwarding_enable = document.form.vts_enable_x.value;
	var ftp_account_mode = document.form.st_ftp_mode.value;
	var samba_account_mode = document.form.st_samba_mode.value;
	var wrs_cc_enable = document.form.wrs_cc_enable.value;
	var wrs_vp_enable = document.form.wrs_vp_enable.value;
	var wrs_mals_enable = document.form.wrs_mals_enable.value;
	var wps_enable = document.form.wps_enable.value;
	var restart_wan = 0;
	var restart_time = 0;
	var restart_firewall = 0;
	var restart_wrs = 0;
	var restart_wireless = 0;
	var restart_samba = 0;
	var restart_ftp = 0;
	
	if(wps_enable == 1){
		document.form.wps_enable.value = 0;
		document.form.wps_sta_pin.value = "";
		document.form.wps_enable.disabled = false;
		document.form.wps_sta_pin.disabled = false;
		restart_wireless = 1;
		document.form.action_wait.value = "20";
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
	
	if(ftp_account_mode == 1){	
		document.form.st_ftp_mode.value = 2;
		document.form.st_ftp_force_mode.value = 2;
		document.form.st_ftp_mode.disabled = false;
		document.form.st_ftp_force_mode.disabled = false;
		restart_ftp = 1
	}
	
	if(samba_account_mode == 1){
		document.form.st_samba_mode.value = 4;
		document.form.st_samba_force_mode.value = 4;
		document.form.st_samba_mode.disabled = false;
		document.form.st_samba_force_mode.disabled = false;
		restart_samba = 1;
	}

	if(wrs_cc_enable == 0){
		document.form.wrs_cc_enable.value = 1;
//		restart_firewall = 1;
		restart_wrs = 1;
	}
	
	if(wrs_vp_enable == 0){
		document.form.wrs_vp_enable.value = 1;
//		restart_firewall = 1;
		restart_wrs = 1;
	}

	if(wrs_mals_enable == 0){
		document.form.wrs_mals_enable.value = 1;
//		restart_firewall = 1;
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
	
	if(restart_samba == 1 && restart_ftp == 1){
		if(action_script_temp == "")
			action_script_temp += "restart_ftpsamba";
		else	
			action_script_temp += ";restart_ftpsamba";
	}
	else{
		if(restart_samba == 1){
			if(action_script_temp == "")
				action_script_temp += "restart_samba";
			else	
				action_script_temp += ";restart_samba";
		}
	
		if(restart_ftp == 1){
			if(action_script_temp == "")
				action_script_temp += "restart_ftpd";
			else	
				action_script_temp += ";restart_ftpd";
		}	
	}
	
	document.form.action_script.value = action_script_temp;
	document.form.submit();
}
function check_login_name_password(){

	if('<% check_acorpw(); %>' == '1'){
		document.getElementById('login_password').innerHTML = "<a href='Advanced_System_Content.asp' target='_blank'><#checkbox_No#></a>";
		document.getElementById('login_password').className = "status_no";	
		document.getElementById('login_password').onmouseover = function(){overHint(10);}
		document.getElementById('login_password').onmouseout = function(){nd();}
	}
	else{
		document.getElementById('login_password').innerHTML = "<#checkbox_Yes#>";
		document.getElementById('login_password').className = "status_yes";
	}
}

function check_wireless_password(){
	var nScore = '<% check_passwd_strength("wl_key"); %>';
	var oScore = document.getElementById("score");
	if (nScore >= 0 && nScore < 20) { sComplexity = "<#PASS_score0#>"; }
	else if (nScore >= 20 && nScore < 40) { sComplexity = "<#PASS_score1#>"; }
	else if (nScore >= 40 && nScore < 60) { sComplexity = "<#PASS_score2#>"; }
	else if (nScore >= 60 && nScore < 80) { sComplexity = "<#PASS_score3#>"; }
	else if (nScore >= 80 && nScore <= 100) { sComplexity = "<#PASS_score4#>"; }

	if(nScore >= 0 && nScore < 40){
		document.getElementById('score').className = "status_no";			
	}
	else if(nScore >= 40 && nScore <= 100){
		document.getElementById('score').className = "status_yes";		
	}	
	oScore.innerHTML = sComplexity;
	if(document.getElementById('score').className == "status_no")
	{
		document.getElementById('score').onmouseover = function(){overHint(11);}
		document.getElementById('score').onmouseout = function(){nd();}
	}
}

function check_wireless_encryption(){
	if('<% check_wireless_encryption(); %>' == '1'){		
		document.getElementById('wireless_encryption').innerHTML = "<#PASS_score3#>";
		document.getElementById('wireless_encryption').className = "status_yes";
	}
	else{
		document.getElementById('wireless_encryption').innerHTML = "<a href='Advanced_Wireless_Content.asp' target='_blank'><#PASS_score1#></a>";
		document.getElementById('wireless_encryption').className = "status_no";	
		document.getElementById('wireless_encryption').onmouseover = function(){overHint(12);}
		document.getElementById('wireless_encryption').onmouseout = function(){nd();}
	}
}

function check_WPS(){
	var wps_enable = document.form.wps_enable.value;
	if(wps_enable == 0){
		document.getElementById('wps_status').innerHTML = "<#checkbox_Yes#>";
		document.getElementById('wps_status').className = "status_yes";
	}
	else{
		document.getElementById('wps_status').innerHTML = "<a href='Advanced_WWPS_Content.asp' target='_blank'><#checkbox_No#></a>";
		document.getElementById('wps_status').className = "status_no";	
		document.getElementById('wps_status').onmouseover = function(){overHint(25);}
		document.getElementById('wps_status').onmouseout = function(){nd();}
	}
}

function check_upnp(){
	var wan0_upnp_enable = document.form.wan0_upnp_enable.value;
	var wan1_upnp_enable = document.form.wan1_upnp_enable.value;
	
	if(dualwan_enabled){
		if(wan0_upnp_enable == 0 && wan1_upnp_enable == 0){
			document.getElementById('upnp_service').innerHTML = "<#checkbox_Yes#>";
			document.getElementById('upnp_service').className = "status_yes";
		}
		else{
			document.getElementById('upnp_service').innerHTML = "<a href='Advanced_WAN_Content.asp' target='_blank'><#checkbox_No#></a>";
			document.getElementById('upnp_service').className = "status_no";
			document.getElementById('upnp_service').onmouseover = function(){overHint(13);}
			document.getElementById('upnp_service').onmouseout = function(){nd();}
		}
	}
	else{
		if(wan0_upnp_enable == 0){
			document.getElementById('upnp_service').innerHTML = "<#checkbox_Yes#>";
			document.getElementById('upnp_service').className = "status_yes";
		}
		else{
			document.getElementById('upnp_service').innerHTML = "<a href='Advanced_WAN_Content.asp' target='_blank'><#checkbox_No#></a>";
			document.getElementById('upnp_service').className = "status_no";
			document.getElementById('upnp_service').onmouseover = function(){overHint(13);}
			document.getElementById('upnp_service').onmouseout = function(){nd();}
		}
	
	}
}

function check_wan_access(){
	var wan_access_enable = document.form.misc_http_x.value;

	if(wan_access_enable == 0){
		document.getElementById('access_from_wan').innerHTML = "<#checkbox_Yes#>";
		document.getElementById('access_from_wan').className = "status_yes";
	}
	else{
		document.getElementById('access_from_wan').innerHTML = "<a href='Advanced_System_Content.asp' target='_blank'><#checkbox_No#></a>";
		document.getElementById('access_from_wan').className = "status_no";
		document.getElementById('access_from_wan').onmouseover = function(){overHint(14);}
		document.getElementById('access_from_wan').onmouseout = function(){nd();}
	}
}

function check_ping_form_wan(){
	var wan_ping_enable = document.form.misc_ping_x.value;

	if(wan_ping_enable == 0){
		document.getElementById('ping_from_wan').innerHTML = "<#checkbox_Yes#>";
		document.getElementById('ping_from_wan').className = "status_yes";
	}
	else{
		document.getElementById('ping_from_wan').innerHTML = "<a href='Advanced_BasicFirewall_Content.asp' target='_blank'><#checkbox_No#></a>";
		document.getElementById('ping_from_wan').className = "status_no";
		document.getElementById('ping_from_wan').onmouseover = function(){overHint(15);}
		document.getElementById('ping_from_wan').onmouseout = function(){nd();}
	}
}

function check_dmz(){
	if(document.form.dmz_ip.value == ""){
		document.getElementById('dmz_service').innerHTML = "<#checkbox_Yes#>";
		document.getElementById('dmz_service').className = "status_yes";
	}
	else{
		document.getElementById('dmz_service').innerHTML = "<a href='Advanced_Exposed_Content.asp' target='_blank'><#checkbox_No#></a>";
		document.getElementById('dmz_service').className = "status_no";
		document.getElementById('dmz_service').onmouseover = function(){overHint(16);}
		document.getElementById('dmz_service').onmouseout = function(){nd();}
	}
}

function check_port_trigger(){
	var port_trigger_enable = document.form.autofw_enable_x.value;

	if(port_trigger_enable == 0){
		document.getElementById('port_tirgger').innerHTML = "<#checkbox_Yes#>";
		document.getElementById('port_tirgger').className = "status_yes";
	}
	else{
		document.getElementById('port_tirgger').innerHTML = "<a href='Advanced_PortTrigger_Content.asp' target='_blank'><#checkbox_No#></a>";
		document.getElementById('port_tirgger').className = "status_no";
		document.getElementById('port_tirgger').onmouseover = function(){overHint(17);}
		document.getElementById('port_tirgger').onmouseout = function(){nd();}
	}

}

function check_port_forwarding(){
	var port_forwarding_enable = document.form.vts_enable_x.value;

	if(port_forwarding_enable == 0){
		document.getElementById('port_forwarding').innerHTML = "<#checkbox_Yes#>";
		document.getElementById('port_forwarding').className = "status_yes";
	}
	else{
		document.getElementById('port_forwarding').innerHTML = "<a href='Advanced_VirtualServer_Content.asp' target='_blank'><#checkbox_No#></a>";
		document.getElementById('port_forwarding').className = "status_no";
		document.getElementById('port_forwarding').onmouseover = function(){overHint(18);}
		document.getElementById('port_forwarding').onmouseout = function(){nd();}
	}
}

function check_ftp_anonymous(){
	var ftp_account_mode = document.form.st_ftp_mode.value;		//1: shared mode, 2: account mode
	
	if(ftp_account_mode == 1){
		document.getElementById('ftp_account').innerHTML = "<a href='Advanced_AiDisk_ftp.asp' target='_blank'><#checkbox_No#></a>";
		document.getElementById('ftp_account').className = "status_no";
		document.getElementById('ftp_account').onmouseover = function(){overHint(19);}
		document.getElementById('ftp_account').onmouseout = function(){nd();}
	}
	else{
		document.getElementById('ftp_account').innerHTML = "<#checkbox_Yes#>";
		document.getElementById('ftp_account').className = "status_yes";
	}
}

function check_samba_anonymous(){
	var samba_account_mode = document.form.st_samba_mode.value;		//1: shared mode, 4: account mode
	
	if(samba_account_mode == 1){
		document.getElementById('samba_account').innerHTML = "<a href='Advanced_AiDisk_samba.asp' target='_blank'><#checkbox_No#></a>";
		document.getElementById('samba_account').className = "status_no";
		document.getElementById('samba_account').onmouseover = function(){overHint(20);}
		document.getElementById('samba_account').onmouseout = function(){nd();}
	}
	else{
		document.getElementById('samba_account').innerHTML = "<#checkbox_Yes#>";
		document.getElementById('samba_account').className = "status_yes";
	}
}

function check_TM_feature(){
	var wrs_cc_enable = document.form.wrs_cc_enable.value;
	var wrs_vp_enable = document.form.wrs_vp_enable.value;
	var wrs_mals_enable = document.form.wrs_mals_enable.value;

	if(wrs_mals_enable == 1){
		document.getElementById('wrs_service').innerHTML = "<#checkbox_Yes#>";
		document.getElementById('wrs_service').className = "status_yes";
	}
	else{
		document.getElementById('wrs_service').innerHTML = "<a href='AiProtection_HomeProtection.asp' target='_blank'><#checkbox_No#></a>";
		document.getElementById('wrs_service').className = "status_no";
		document.getElementById('wrs_service').onmouseover = function(){overHint(21);}
		document.getElementById('wrs_service').onmouseout = function(){nd();}
	}

	if(wrs_vp_enable == 1){
		document.getElementById('vp_service').innerHTML = "<#checkbox_Yes#>";
		document.getElementById('vp_service').className = "status_yes";
	}
	else{
		document.getElementById('vp_service').innerHTML = "<a href='AiProtection_HomeProtection.asp' target='_blank'><#checkbox_No#></a>";
		document.getElementById('vp_service').className = "status_no";
		document.getElementById('vp_service').onmouseover = function(){overHint(22);}
		document.getElementById('vp_service').onmouseout = function(){nd();}
	}
	
	if(wrs_cc_enable == 1){
		document.getElementById('cc_service').innerHTML = "<#checkbox_Yes#>";
		document.getElementById('cc_service').className = "status_yes";
	}
	else{
		document.getElementById('cc_service').innerHTML = "<a href='AiProtection_HomeProtection.asp' target='_blank'><#checkbox_No#></a>";
		document.getElementById('cc_service').className = "status_no";
		document.getElementById('cc_service').onmouseover = function(){overHint(23);}
		document.getElementById('cc_service').onmouseout = function(){nd();}
	}
}

function show_tm_eula(){
	if(document.form.preferred_lang.value == "JP"){
			$.get("JP_tm_eula.htm", function(data){
				document.getElementById('agreement_panel').innerHTML= data;
			});
	}
	else{
			$.get("tm_eula.htm", function(data){
				document.getElementById('agreement_panel').innerHTML= data;
			});
	}
	dr_advise();
	cal_panel_block("agreement_panel", 0.25);
	$("#agreement_panel").fadeIn(300);
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
	cal_panel_block("alert_preference", 0.25);
	check_smtp_server_type();
	parse_wrs_mail_bit();
	$('#alert_preference').fadeIn(300);
	document.getElementById('mail_address').value = document.form.PM_MY_EMAIL.value;
	document.getElementById('mail_password').value = document.form.PM_SMTP_AUTH_PASS.value;
}

function close_alert_preference(){
	$('#alert_preference').fadeOut(100);
}
var smtpList = new Array();
smtpList = [
	{smtpServer: "smtp.gmail.com", smtpPort: "587", smtpDomain: "gmail.com"},
	{smtpServer: "smtp.aol.com", smtpPort: "587", smtpDomain: "aol.com"},
	{smtpServer: "smtp.qq.com", smtpPort: "587", smtpDomain: "qq.com"},
	{smtpServer: "smtp.163.com", smtpPort: "25", smtpDomain: "163.com"},
	{end: 0}
];
function apply_alert_preference(){
	var address_temp = document.getElementById('mail_address').value;
	var account_temp = document.getElementById('mail_address').value.split("@");
	
	var mail_bit = 0;
	var server_index = document.getElementById("mail_provider").value;

	
	if(address_temp.indexOf('@') != -1){	
		if(account_temp[1] != "gmail.com" && account_temp[1] != "aol.com" && account_temp[1] != "qq.com" && account_temp[1] != "163.com"){
			alert("Wrong mail domain");
			document.getElementById('mail_address').focus();
			return false;
		}
		
		if(document.form.PM_MY_EMAIL.value == "" || document.form.PM_MY_EMAIL.value != address_temp)
			document.form.action_script.value += ";send_confirm_mail";
				
		document.form.PM_MY_EMAIL.value = address_temp;	
	}
	else{	
		if(document.form.PM_MY_EMAIL.value == "" || document.form.PM_MY_EMAIL.value != address_temp)
			document.form.action_script.value += ";send_confirm_mail";
			
		document.form.PM_MY_EMAIL.value = account_temp[0] + "@" +smtpList[server_index].smtpDomain;
	}
	
	if(document.getElementById("mal_website_item").checked)
		mail_bit += 1;
	
	if(document.getElementById("vp_item").checked)
		mail_bit += 2;
	
	if(document.getElementById("cc_item").checked)
		mail_bit += 4;
	
	document.form.wrs_mail_bit.value = mail_bit;
	document.form.PM_SMTP_AUTH_USER.value = account_temp[0];
	document.form.PM_SMTP_AUTH_PASS.value = document.getElementById('mail_password').value;	
	document.form.PM_SMTP_SERVER.value = smtpList[server_index].smtpServer;	
	document.form.PM_SMTP_PORT.value = smtpList[server_index].smtpPort;	
	$('#alert_preference').fadeOut(100);
	document.form.submit();
}

function parse_wrs_mail_bit(){
	var quot = document.form.wrs_mail_bit.value;
	var mail_bit_array =  new Array();
	for(i=0;i<3;i++){
		mail_bit_array[i] = quot%2; 
		quot = parseInt(quot/2);	
	}
	
	document.getElementById("mal_website_item").checked =  mail_bit_array[0] == 1 ? true : false;
	document.getElementById("vp_item").checked =  mail_bit_array[1] == 1 ? true : false;
	document.getElementById("cc_item").checked =  mail_bit_array[2] == 1 ? true : false;
}

function check_smtp_server_type(){
	for(i = 0;i < smtpList.length; i++){
		if(smtpList[i].smtpServer == document.form.PM_SMTP_SERVER.value){
			document.getElementById("mail_provider").value = i;
			break;
		}
	}

}
</script>
</head>

<body onload="initial();" onunload="unload_body();" onselectstart="return false;">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="agreement_panel" class="panel_folder" style="margin-top: -100px;"></div>
<div id="hiddenMask" class="popup_bg" style="z-index:999;">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center"></table>
	<!--[if lte IE 6.5.]><script>alert("<#ALERT_TO_CHANGE_BROWSER#>");</script><![endif]-->
</div>
<div id="weakness_div" class="weakness">
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
				<table style="margin-top:10px;margin-left:auto;margin-right:auto;">
					<tr>
						<td>
							<input class="button_gen" type="button" onclick="close_weakness_status();" value="<#CTL_close#>">
						</td>
						<td>
							<input class="button_gen_long" type="button" onclick="enable_whole_security();" value="<#CTL_secure#>">
						</td>
					</tr>
				</table>
			</td>
		</tr>
	</table>

</div>

<div id="alert_preference" class="alertpreference">
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
									<option value="1">AOL</option>							
									<option value="2">QQ</option>							
									<option value="3">163</option>							
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
						<th><#HSDPAConfig_Password_itemname#></th>
						<td>
							<div>
								<input type="password" class="input_30_table" id="mail_password" maxlength="100" value="" autocorrect="off" autocapitalize="off">
							</div>
						</td>
					</tr>
					<tr>
						<th>Notification Item</th>
						<td>
							<div>							
								<input type="checkbox" class="" id="mal_website_item" value="">
								<label><#AiProtection_sites_blocking#></label>
								<input type="checkbox" class="" id="vp_item" value="">
								<label><#AiProtection_Vulnerability#></label>
								<input type="checkbox" class="" id="cc_item" value="">
								<label><#AiProtection_detection_blocking#></label>
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
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="AiProtection_HomeProtection.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_wrs">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>" disabled>
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wrs_mals_enable" value="<% nvram_get("wrs_mals_enable"); %>">
<input type="hidden" name="wrs_cc_enable" value="<% nvram_get("wrs_cc_enable"); %>">
<input type="hidden" name="wrs_vp_enable" value="<% nvram_get("wrs_vp_enable"); %>">
<input type="hidden" name="wan0_upnp_enable" value="<% nvram_get("wan0_upnp_enable"); %>" disabled>
<input type="hidden" name="wan1_upnp_enable" value="<% nvram_get("wan1_upnp_enable"); %>" disabled>
<input type="hidden" name="wans_dualwan" value="<% nvram_get("wans_dualwan"); %>" disabled>
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
<input type="hidden" name="wrs_mail_bit" value="<% nvram_get("wrs_mail_bit"); %>">
<input type="hidden" name="st_ftp_force_mode" value="<% nvram_get("st_ftp_force_mode"); %>" disabled>
<input type="hidden" name="st_ftp_mode" value="<% nvram_get("st_ftp_mode"); %>" disabled>
<input type="hidden" name="st_samba_force_mode" value="<% nvram_get("st_samba_force_mode"); %>" disabled>
<input type="hidden" name="st_samba_mode" value="<% nvram_get("st_samba_mode"); %>" disabled>

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
													<span class="formfonttitle"><#AiProtection_title#> - <#AiProtection_Home#></span>
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
																			$('#radio_mals_enable').iphoneSwitch('<% nvram_get("wrs_mals_enable"); %>',
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
																			$('#radio_vp_enable').iphoneSwitch('<% nvram_get("wrs_vp_enable"); %>',
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
															$('#radio_cc_enable').iphoneSwitch('<% nvram_get("wrs_cc_enable"); %>',
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
									<div style="width:135px;height:55px;margin: 10px 0 0 600px;background-image:url('images/New_ui/tm_logo_power.png');"></div>
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
