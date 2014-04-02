﻿<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#menu5_6_1_title#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<style type="text/css">
.contentM_qis{
	width:650px;
	height:475px;
	margin-top:-160px;
	margin-left:250px;
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:200;
	background-color:#2B373B;
	display:none;
	behavior: url(/PIE.htc);
}
.QISmain{
	width:730px;
	/*font-family:Verdana, Arial, Helvetica, sans-serif;*/
	font-size:14px;
	z-index:200;
	position:relative;
	background-color:balck:
}	
.QISform_wireless{
	width:600px;
	font-size:12px;
	color:#FFFFFF;
	margin-top:10px;
	*margin-left:10px;
}

.QISform_wireless thead{
	font-size:15px;
	line-height:20px;
	color:#FFFFFF;	
}

.QISform_wireless th{
	padding-left:10px;
	*padding-left:30px;
	font-size:12px;
	font-weight:bolder;
	color: #FFFFFF;
	text-align:left; 
}
               
.QISform_wireless li{	
	margin-top:10px;
}
.QISGeneralFont{
	font-family:Segoe UI, Arial, sans-serif;
	margin-top:10px;
	margin-left:70px;
	*margin-left:50px;
	margin-right:30px;
	color:white;
	LINE-HEIGHT:18px;
}	
.description_down{
	margin-top:10px;
	margin-left:10px;
	padding-left:5px;
	font-weight:bold;
	line-height:140%;
	color:#ffffff;	
}
</style>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var sw_mode_orig = '<% nvram_get("sw_mode"); %>';
if(sw_mode_orig == 3 && '<% nvram_get("wlc_psta"); %>' == 1)
	sw_mode_orig = 4;

function initial(){
	show_menu();
	setScenerion(sw_mode_orig);
	Senario_shift();

	if(!repeater_support)
		$("repeaterMode").style.display = "none";

	if(!psta_support)
		$("mbMode").style.display = "none";
}

function Senario_shift(){
	var isIE = navigator.userAgent.search("MSIE") > -1; 
	if(isIE)
		$("Senario").style.width = "700px";
		$("Senario").style.marginLeft = "5px";
}

function restore_wl_config(prefix){
	var wl_defaults = {
		ssid: {_name: prefix+"ssid", _value: "ASUS"},
		wep: {_name: prefix+"wep_x", _value: "0"},
		key: {_name: prefix+"key", _value: "1"},
		key1: {_name: prefix+"key1", _value: ""},
		key2: {_name: prefix+"key2", _value: ""},
		key3: {_name: prefix+"key3", _value: ""},
		key4: {_name: prefix+"key4", _value: ""},
		auth_mode: {_name: prefix+"auth_mode_x", _value: "open"},
		crypto: {_name: prefix+"crypto", _value: "tkip+aes"},
		wpa_psk: {_name: prefix+"wpa_psk", _value: ""},
		nbw_cap: {_name: prefix+"nbw_cap", _value: "1"},
		bw_cap: {_name: prefix+"bw", _value: "1"},
		hwaddr: {_name: prefix+"hwaddr", _value: ""}
	}
	
	if(prefix.length == 6)
		wl_defaults.ssid._value = "ASUS_Guest" + prefix[4];

	if(prefix[2] == 1)
		wl_defaults.ssid._value = wl_defaults.ssid._value.replace("ASUS", "ASUS_5G");

	for(var i in wl_defaults) 
		set_variable(wl_defaults[i]._name, wl_defaults[i]._value);
}
function restore_wl_config_wep(prefix){
	var wl_defaults = {
		wep: {_name: prefix+"wep_x", _value: "0"},
		key: {_name: prefix+"key", _value: "1"},
		key1: {_name: prefix+"key1", _value: ""},
		key2: {_name: prefix+"key2", _value: ""},
		key3: {_name: prefix+"key3", _value: ""},
		key4: {_name: prefix+"key4", _value: ""},
		nbw_cap: {_name: prefix+"nbw_cap", _value: "1"},
		bw_cap: {_name: prefix+"bw", _value: "1"}
	}

	for(var i in wl_defaults) 
		set_variable(wl_defaults[i]._name, wl_defaults[i]._value);
}
function close_guest_unit(_unit, _subunit){
	var NewInput = document.createElement("input");
	NewInput.type = "hidden";
	NewInput.name = "wl"+ _unit + "." + _subunit +"_bss_enabled";
	NewInput.value = "0";
	document.form.appendChild(NewInput);
}

function saveMode(){
	if(sw_mode_orig == document.form.sw_mode.value){
		alert("<#Web_Title2#> <#op_already_configured#>");
		return false;
	}

  if(document.form.sw_mode.value == 2){
		parent.location.href = '/QIS_wizard.htm?flag=sitesurvey';
		return false;
	}
	else if(document.form.sw_mode.value == 3){
		parent.location.href = '/QIS_wizard.htm?flag=lanip';
		return false;
	}
	else if(document.form.sw_mode.value == 4){
		parent.location.href = '/QIS_wizard.htm?flag=sitesurvey_mb';
		return false;
	}
	else{ // default router
		document.form.lan_proto.value = '<% nvram_default_get("lan_proto"); %>';
		document.form.lan_ipaddr.value = document.form.lan_ipaddr_rt.value;
		document.form.lan_netmask.value = document.form.lan_netmask_rt.value;
		document.form.lan_gateway.value = document.form.lan_ipaddr_rt.value;
		document.form.wlc_psta.value = 0;
		document.form.wlc_psta.disabled = false;

		if(sw_mode_orig == '2' || sw_mode_orig == '4'){
			inputCtrl(document.form.wl0_ssid,1);	
			inputCtrl(document.form.wl0_auth_mode_x,1);	
			inputCtrl(document.form.wl0_crypto,1);	
			inputCtrl(document.form.wl0_wpa_psk,1);

			if(sw_mode_orig == '2'){
				restore_wl_config("wl0.1_");
				restore_wl_config_wep("wl0_");
				
				if(band5g_support){
					restore_wl_config_wep("wl1_");
					restore_wl_config("wl1.1_");
				}
				document.form.w_Setting.value = 0;				
			}
			
			close_guest_unit(0,1);
			if(band5g_support){
				inputCtrl(document.form.wl1_ssid,1);
				inputCtrl(document.form.wl1_crypto,1);
				inputCtrl(document.form.wl1_wpa_psk,1);
				inputCtrl(document.form.wl1_auth_mode_x,1);
				close_guest_unit(1,1);
			}
			
			if(!band5g_support){			
				$('wl_unit_field_1').style.display="none";
				$('wl_unit_field_2').style.display="none";
				$('wl_unit_field_3').style.display="none";	
				$('routerSSID').style.height="370px";				
			}
			
			cal_panel_block();
			$j("#routerSSID").fadeIn(300);
			$j("#forSSID_bg").fadeIn(300);
			$("forSSID_bg").style.visibility = "visible";	
			return true;			
		}
	}

	applyRule();
}

function applyRule(){
	if(document.form.sw_mode.value == 1 && (sw_mode_orig == '2' || sw_mode_orig == '4')){
		if(!validate_string_ssid(document.form.wl0_ssid)){ //validate 2.4G SSID
			document.form.wl0_ssid.focus();
			return false;
		}	

		if(document.form.wl0_wpa_psk.value != ""){
			if(!validate_psk(document.form.wl0_wpa_psk))		//validate 2.4G WPA2 key
				return false;
				
			document.form.wl0_auth_mode_x.value = "psk2";		//
			document.form.wl0_crypto.value = "aes";
		}
		else
			document.form.wl0_auth_mode_x.value = "open";

		if(band5g_support){
			inputCtrl(document.form.wl1_ssid,1);
			inputCtrl(document.form.wl1_crypto,1);
			inputCtrl(document.form.wl1_wpa_psk,1);
			inputCtrl(document.form.wl1_auth_mode_x,1);
			if(!validate_string_ssid(document.form.wl1_ssid)){   //validate 5G SSID
				document.form.wl1_ssid.focus();
				return false;
			}
			
			if(document.form.wl1_wpa_psk.value != ""){
				if(!validate_psk(document.form.wl1_wpa_psk))	//validate 5G WPA2 key
					return false;
			
				document.form.wl1_auth_mode_x.value = "psk2";
				document.form.wl1_crypto.value = "aes";
			}
			else
				document.form.wl1_auth_mode_x.value = "open";
		}			
	}

	showLoading();	
	document.form.target="hidden_frame";
	$("forSSID_bg").style.visibility = "hidden";
	document.form.submit();	
}

function done_validating(action){
	refreshpage();
}

var $j = jQuery.noConflict();
var id_WANunplungHint;

function setScenerion(mode){
	if(mode == '2'){
		document.form.sw_mode.value = 2;
		$j("#Senario").css("height", "");
		$j("#Senario").css("background","url(/images/New_ui/re.jpg) center no-repeat");
		$j("#Senario").css("margin-bottom", "60px");
		$j("#radio2").css("display", "none");
		$j("#Internet_span").css("display", "block");
		$j("#ap-line").css("display", "none");
		$j("#AP").html("<#Device_type_02_RT#>");
		$j("#mode_desc").html("<#OP_RE_desc#><br/><span style=\"color:#FC0\"><#deviceDiscorvy2#></span>");
		$j("#nextButton").attr("value","<#CTL_next#>");
		clearTimeout(id_WANunplungHint);
		$j("#Unplug-hint").css("display", "none");
		document.form.sw_mode_radio[1].checked = true;
	}
	else if(mode == '3'){
		document.form.sw_mode.value = 3;
		$j("#Senario").css("height", "");
		$j("#Senario").css("background","url(/images/New_ui/ap.jpg) center no-repeat");
		$j("#Senario").css("margin-bottom", "60px");
		$j("#radio2").css("display", "none");
		$j("#Internet_span").css("display", "block");
		$j("#ap-line").css("display", "none");
		$j("#AP").html("<#Device_type_02_RT#>");
		$j("#mode_desc").html("<#OP_AP_desc#><br/><span style=\"color:#FC0\"><#deviceDiscorvy3#></span>");
		$j("#nextButton").attr("value","<#CTL_next#>");
		clearTimeout(id_WANunplungHint);
		$j("#Unplug-hint").css("display", "none");
		document.form.sw_mode_radio[2].checked = true;
	}
	else if(mode == '4'){
		document.form.sw_mode.value = 4;
		var pstaDesc =  "<#OP_MB_desc1#>";
				pstaDesc += "<#OP_MB_desc2#>";
				pstaDesc += "<#OP_MB_desc3#>";
				pstaDesc += "<#OP_MB_desc4#>";
				pstaDesc += "<#OP_MB_desc5#>";
				pstaDesc += "<br><#OP_MB_desc6#>";
				pstaDesc += "<br/><span style=\"color:#FC0\"><#deviceDiscorvy4#></span>";

		$j("#Senario").css("height", "300px");
		$j("#Senario").css("background", "url(/images/New_ui/mb.jpg) center no-repeat");
		$j("#Senario").css("margin-bottom", "-20px");
		$j("#radio2").css("display", "none");
		$j("#Internet_span").css("display", "block");
		$j("#ap-line").css("display", "none");
		$j("#AP").html("<#Device_type_02_RT#>");
		$j("#mode_desc").html(pstaDesc);
		$j("#nextButton").attr("value","<#CTL_next#>");
		clearTimeout(id_WANunplungHint);
		$j("#Unplug-hint").css("display", "none");
		document.form.sw_mode_radio[3].checked = true;
	}
	else{ // Default: Router
		document.form.sw_mode.value = 1;
		$j("#Senario").css("height", "");
		$j("#Senario").css("background","url(/images/New_ui/rt.jpg) center no-repeat");
		$j("#Senario").css("margin-bottom", "60px");
		$j("#radio2").hide();
		$j("#Internet_span").hide();
		$j("#ap-line").css("display", "none");	
		$j("#AP").html("<#Internet#>");
		$j("#mode_desc").html("<#OP_GW_desc#>");
		$j("#nextButton").attr("value","<#CTL_next#>");
		document.form.sw_mode_radio[0].checked = true;
	}
}

function Sync_2ghz(band){
	if(band == 2){
		if(document.form.sync_with_2ghz.checked == true){
			document.form.wl1_ssid.value = document.form.wl0_ssid.value; 
			document.form.wl1_wpa_psk.value = document.form.wl0_wpa_psk.value; 
		}
	}
	else
		document.form.sync_with_2ghz.checked = false;
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
		if(isMobile()){
			if(document.body.scrollLeft < 50)
				blockmarginLeft= (winWidth)*0.25+document.body.scrollLeft;
			else if(document.body.scrollLeft >320)
				blockmarginLeft= 320;			
			else
				blockmarginLeft= document.body.scrollLeft;		
		}
		else{
			blockmarginLeft= (winWidth)*0.25+document.body.scrollLeft;	
		}
	}

	$("routerSSID").style.marginLeft = blockmarginLeft+"px";
}
function cancel_SSID_Block(){
	$j("#routerSSID").fadeOut(300);
	$j("#forSSID_bg").fadeOut(300);
	//$("hiddenMask").style.visibility = "hidden";
	//$("forSSID_bg").style.visibility = "hidden";
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="hiddenMask" class="popup_bg">
<table cellpadding="4" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
		<tr>
		<td>
			<div class="drword" id="drword"><#Main_alert_proceeding_desc4#> <#Main_alert_proceeding_desc1#>...
				<br/>
				<br/>
		    </div>
		  <div class="drImg"><img src="images/alertImg.png"></div>
			<div style="height:70px; "></div>
		</td>
		</tr>
	</table>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>
<div id="forSSID_bg" class="popup_bg" style="z-index: 99;"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_all">
<input type="hidden" name="action_wait" value="<% get_default_reboot_time(); %>">
<input type="hidden" name="prev_page" value="Advanced_OperationMode_Content.asp">
<input type="hidden" name="current_page" value="Advanced_OperationMode_Content.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="flag" value="">
<input type="hidden" name="sw_mode" value="<% nvram_get("sw_mode"); %>">
<input type="hidden" name="lan_proto" value="<% nvram_get("lan_proto"); %>">
<input type="hidden" name="lan_ipaddr" value="<% nvram_get("lan_ipaddr"); %>">
<input type="hidden" name="lan_netmask" value="<% nvram_get("lan_netmask"); %>">
<input type="hidden" name="lan_gateway" value="<% nvram_get("lan_gateway"); %>">
<input type="hidden" name="lan_ipaddr_rt" value="<% nvram_get("lan_ipaddr_rt"); %>">
<input type="hidden" name="lan_netmask_rt" value="<% nvram_get("lan_netmask_rt"); %>">
<input type="hidden" name="wl0_auth_mode_x" value="<% nvram_get("wl0_auth_mode_x"); %>" disabled="disabled">
<input type="hidden" name="wl0_crypto" value="<% nvram_get("wl0_crypto"); %>" disabled="disabled">
<input type="hidden" name="wl1_auth_mode_x" value="<% nvram_get("wl1_auth_mode_x"); %>" disabled="disabled">
<input type="hidden" name="wl1_crypto" value="<% nvram_get("wl1_crypto"); %>" disabled="disabled">
<input type="hidden" name="w_Setting" value="1">
<!-- AC66U's repeater mode -->
<input type="hidden" name="wlc_psta" value="<% nvram_get("wlc_psta"); %>" disabled>

<!-- Input SSID and Password block for switching Repeater to Router mode -->
<div id="routerSSID" class="contentM_qis" style="box-shadow: 3px 3px 10px #000;">
	<table class="QISform_wireless" width="400" border=0 align="center" cellpadding="5" cellspacing="0">
		<tr>
			<div class="description_down"><#QKSet_wireless_webtitle#></div>
		</tr>
		<tr>
			<div class="QISGeneralFont" align="left"><#qis_wireless_setting#></div>
		</tr>
		<tr>
			<div style="margin:5px;*margin-left:-5px;"><img style="width: 640px; *width: 640px; height: 2px;" src="/images/New_ui/export/line_export.png"></div>
		</tr>
		<tr>
			<th width="180">2.4GHz - <#Security#></th>
			<td class="QISformtd"></td>
		</tr>
		<tr>
			<th width="180">
				<span onmouseout="return nd();" onclick="openHint(0, 22);" style="cursor:help;"><#QIS_finish_wireless_item1#><img align="right" style="cursor:pointer;margin-top:-14px\9;" src="/images/New_ui/helpicon.png"></span>
			</th>
			<td class="QISformtd">
				<input type="text" id="wl0_ssid" name="wl0_ssid" onkeypress="return is_string(this, event);" onkeyup="Sync_2ghz(2);" style="height:25px;" class="input_32_table" maxlength="32" value="" disabled="disabled">
			</td>
		</tr>
		<tr id="wl_unit_field_0">
			<th width="180">
				<span onmouseout="return nd();" onclick="openHint(0, 23);" style="cursor:help;"><#Network_key#><img align="right" style="cursor:pointer;margin-top:-14px\9;" src="/images/New_ui/helpicon.png"></span>		
			</th>
			<td class="QISformtd">
				<input id="wl0_wpa_psk" name="wl0_wpa_psk" type="password" autocapitalization="off" onBlur="switchType(this,false);" onFocus="switchType(this,true);" value="" onkeyup="Sync_2ghz(2);" style="height:25px;" class="input_32_table" maxlength="65" disabled="disabled">
			</td>
		</tr>
		<tr id="wl_unit_field_1">
			<th width="180">5GHz - <#Security#> </th>
			<td class="QISformtd">
				<span id="syncCheckbox"><input type="checkbox" id="sync_with_2ghz" name="sync_with_2ghz" class="input" onclick="setTimeout('Sync_2ghz(2);',0);"><#qis_ssid_desc#></span>
			</td>
		</tr>
		<tr id="wl_unit_field_2">
			<th width="180">
				<span onmouseout="return nd();" onclick="openHint(0, 22);" style="cursor:help;"><#QIS_finish_wireless_item1#><img align="right" style="cursor:pointer;margin-top:-14px\9;" src="/images/New_ui/helpicon.png"></span>
			</th>
			<td class="QISformtd">
				<input type="text" id="wl1_ssid" name="wl1_ssid" onkeypress="return is_string(this, event);" onkeyup="Sync_2ghz(5);" style="height:25px;" class="input_32_table" maxlength="32" value="" disabled="disabled">
			</td>
		</tr>
		<tr id="wl_unit_field_3">
			<th width="180">
				<span onmouseout="return nd();" onclick="openHint(0, 23);" style="cursor:help;"><#Network_key#><img align="right" style="cursor:pointer;margin-top:-14px\9;" src="/images/New_ui/helpicon.png"></span>
			</th>
			<td class="QISformtd">
				<input id="wl1_wpa_psk" name="wl1_wpa_psk" type="password" autocapitalization="off" onBlur="switchType(this,false);" onFocus="switchType(this,true);" value="" onkeyup="Sync_2ghz(5);" style="height:25px;" class="input_32_table" maxlength="65" disabled="disabled">
			</td>
		</tr>
		<tr>
			<td colspan="2">
				<div class="QISGeneralFont" ><#qis_wireless_setting_skdesc#></div>
			</td>
		</tr>
	</table>
	<div style="margin-top:20px;text-align:center;background-color:#2B373B">
		<input type="button"  value="<#CTL_Cancel#>" class="button_gen" onclick="cancel_SSID_Block();">
		<input type="button" id="btn_next_step" value="<#CTL_apply#>" class="button_gen" onclick="applyRule();">
	</div>
</div>
<!-- End input SSID and Password block for switching Repeater to Router mode -->
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
		<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
			<tr>
				<td valign="top" >			
					<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
						<tr bgcolor="#4D595D" valign="top" style="height:15%;*height:5%">
							<td>
								<div>&nbsp;</div>
								<div class="formfonttitle"><#menu5_6_adv#> - <#menu5_6_1_title#></div>
								<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
								<div class="formfontdesc" style="*margin-bottom:-25px;"><#OP_desc1#></div>
							</td>
						</tr>
						<tr bgcolor="#4D595D" valign="top">
							<td>
								<div style="width:95%; margin:0 auto; padding-bottom:3px;">
									<span style="font-size:16px; font-weight:bold;color:white;text-shadow:1px 1px 0px black">
										<input type="radio" name="sw_mode_radio" class="input" value="1" onclick="setScenerion(1);" <% nvram_match("sw_mode", "1", "checked"); %>><#OP_GW_item#>
										&nbsp;&nbsp;
										<span id="repeaterMode"><input type="radio" name="sw_mode_radio" class="input" value="2" onclick="setScenerion(2);" <% nvram_match("sw_mode", "2", "checked"); %>><#OP_RE_item#></span>
										&nbsp;&nbsp;
										<input type="radio" name="sw_mode_radio" class="input" value="3" onclick="setScenerion(3);" <% nvram_match("sw_mode", "3", "checked"); %>><#OP_AP_item#>
										&nbsp;&nbsp;
										<span id="mbMode"><input type="radio" name="sw_mode_radio" class="input" value="4" onclick="setScenerion(4);" <% nvram_match("sw_mode", "4", "checked"); %>>Media bridge</span>
									</span>
									<div id="mode_desc" style="position:relative;display:block;margin-top:10px;margin-left:5px;height:60px;z-index:75;">
										<#OP_GW_desc#>
									</div>
										<br/><br/>
									<div id="Senario" style="margin-top:40px; margin-bottom:60px;">
										<!--div id="ap-line" style="border:0px solid #333; width:133px; height:41px; position:absolute; background:url(/images/wanlink.gif) no-repeat;"></div-->
										<div id="Unplug-hint" style="border:2px solid red; background-color:#FFF; padding:3px;margin:0px 0px 0px 150px;width:250px; position:absolute; display:block; display:none;"><#web_redirect_suggestion1#></div>
									</div>	
								</div>
								<div class="apply_gen">
									<input name="button" type="button" class="button_gen" onClick="saveMode();" value="<#CTL_onlysave#>">
								</div>
							</td>
						</tr>
					</table>
				</td>
			</tr>
		</table>
			<!--===================================Ending of Main Content===========================================-->
		</td>
		<td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

</form>
<div id="footer"></div>
</body>
</html>
