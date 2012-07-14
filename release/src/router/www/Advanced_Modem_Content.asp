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
<title><#Web_Title#> - <#menu5_4_4#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/wcdma_list.js"></script>
<script type="text/javascript" src="/cdma2000_list.js"></script>
<script type="text/javascript" src="/td-scdma_list.js"></script>
<script type="text/javascript" src="/wimax_list.js"></script>
<script>

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var modem = '<% nvram_get("Dev3G"); %>';
var country = '<% nvram_get("modem_country"); %>';
var isp = '<% nvram_get("modem_isp"); %>';
var apn = '<% nvram_get("modem_apn"); %>';
var dialnum = '<% nvram_get("modem_dialnum"); %>';
var user = '<% nvram_get("modem_user"); %>';
var pass = '<% nvram_get("modem_pass"); %>';
var pin_opt = '<% nvram_get("modem_pincode_opt"); %>';

var modemlist = new Array();
var countrylist = new Array();
var isplist = new Array();

var apnlist = new Array();
var daillist = new Array();
var userlist = new Array();
var passlist = new Array();

var wans_dualwan = '<% nvram_get("wans_dualwan"); %>';
<% wan_get_parameter(); %>

if(dualWAN_support != -1){
	var wan_type_name = wans_dualwan.split(" ")[<% nvram_get("wan_unit"); %>];
	wan_type_name = wan_type_name.toUpperCase();
	switch(wan_type_name){
		case "DSL":
			location.href = "Advanced_DSL_Content.asp";
			break;
		case "WAN":
		case "LAN":
			location.href = "Advanced_WAN_Content.asp";
			break;	
		default:
			break;	
	}
}
	
function initial(){
	show_menu();
	genWANSoption();
	gen_country_list();
	gen_list();
	switch_modem_mode('<% nvram_get("modem_enable"); %>');
	show_ISP_list();
	show_APN_list();
	switch_modem_mode_enable('<% nvram_get("modem_enable"); %>');

	if(dualWAN_support == -1){		
		$("option5").innerHTML = '<table><tbody><tr><td><div id="index_img5"></div></td><td><div style="width:120px;"><#Menu_usb_application#></div></td></tr></tbody></table>';
		$("option5").className = "m5_r";
	}

  if(no_wimax_support >= 0){  	
  	/*document.form.modem_enable_option.remove(3);	//remove WiMAX  	*/
  	for (var i = 0; i < document.form.modem_enable_option.options.length; i++) {
			if (document.form.modem_enable_option.options[i].value == "4") {
				document.form.modem_enable_option.options.remove(i);
				break;
			}
		}
  }

	//pin code
	if(pin_opt) {
		document.form.modem_pincode.disabled = false;
	} else {
		document.form.modem_pincode.disabled = true;
	}
}

function genWANSoption(){
	for(i=0; i<wans_dualwan.split(" ").length; i++)
		document.form.wan_unit.options[i] = new Option(wans_dualwan.split(" ")[i].toUpperCase(), i);
	document.form.wan_unit.selectedIndex = '<% nvram_get("wan_unit"); %>';

	if(wans_dualwan.search(" ") < 0 || wans_dualwan.split(" ")[1] == 'none' || dualWAN_support == -1)
		$("WANscap").style.display = "none";
}

function change_wan_unit(){
	if(document.form.wan_unit.options[document.form.wan_unit.selectedIndex].text == "DSL")
		document.form.current_page.value = "Advanced_DSL_Content.asp";
	else if(document.form.wan_unit.options[document.form.wan_unit.selectedIndex].text == "WAN"||
					document.form.wan_unit.options[document.form.wan_unit.selectedIndex].text == "LAN")
		document.form.current_page.value = "Advanced_WAN_Content.asp";
	else
		return false;	

	FormActions("apply.cgi", "change_wan_unit", "", "");
	document.form.target = "";
	document.form.submit();
}

function show_modem_list(mode){
	if(mode == "4")
		show_4G_modem_list();
	else
		show_3G_modem_list();
}

function show_3G_modem_list(){
	modemlist = new Array(
			"AUTO"
			, "ASUS-T500"
			, "BandLuxe-C120"
			, "BandLuxe-C170"
			, "BandLuxe-C339"
			, "Huawei-E1550"
			, "Huawei-E160G"
			, "Huawei-E161"
			, "Huawei-E169"
			, "Huawei-E176"
			, "Huawei-E220"
			, "Huawei-K3520"
			, "Huawei-ET128"
			, "Huawei-E1800"
			, "Huawei-K4505"
			, "Huawei-E172"
			, "Huawei-E372"
			, "Huawei-E122"
			, "Huawei-E160E"
			, "Huawei-E1552"
			, "Huawei-E173"
			, "Huawei-E1823"
			, "Huawei-E1762"
			, "Huawei-E1750C"
			, "Huawei-E1752Cu"
			//, "MU-Q101"
			, "Alcatel-X200"
			, "Alcatel-Oune-touch-X220S"
			, "AnyData-ADU-510A"
			, "AnyData-ADU-500A"
			, "Onda-MT833UP"
			, "Onda-MW833UP"
			, "MTS-AC2746"
			, "ZTE-AC5710"
			, "ZTE-MU351"
			, "ZTE-MF100"
			, "ZTE-MF636"
			, "ZTE-MF622"
			, "ZTE-MF626"
			, "ZTE-MF632"
			, "ZTE-MF112"
			, "ZTE-MFK3570-Z"
			, "CS15"
			, "CS17"
			, "ICON401"
			);

	free_options($("shown_modems"));
	for(var i = 0; i < modemlist.length; i++){
		$("shown_modems").options[i] = new Option(modemlist[i], modemlist[i]);
		if(modemlist[i] == modem)
			$("shown_modems").options[i].selected = "1";
	}
}

function show_4G_modem_list(){
	modemlist = new Array(
			"AUTO"
			, "Samsung U200"
			//, "Beceem BCMS250"
			);

	free_options($("shown_modems"));
	for(var i = 0; i < modemlist.length; i++){
		$("shown_modems").options[i] = new Option(modemlist[i], modemlist[i]);
		if(modemlist[i] == modem)
			$("shown_modems").options[i].selected = "1";
	}
}

function switch_modem_mode(mode){
	//if(mode != "0" && $("hsdpa_hint").style.display == "none")
	//alert("<#HSDPA_LANtoWAN#>");
	document.form.modem_enable.value = mode;
	show_modem_list(mode);

	if(mode == "1"){ // WCDMA
		inputCtrl(document.form.Dev3G, 1);
		inputCtrl(document.form.modem_country, 1);
		inputCtrl(document.form.modem_isp, 1);
		inputCtrl(document.form.modem_apn, 1);
		if(pin_opt) inputCtrl(document.form.modem_pincode, 1);
		inputCtrl(document.form.modem_dialnum, 1);
		inputCtrl(document.form.modem_user, 1);
		inputCtrl(document.form.modem_pass, 1);
		inputCtrl(document.form.modem_ttlsid, 0);
		//$("hsdpa_hint").style.display = "";
	}
	else if(mode == "2"){ // CDMA2000
		inputCtrl(document.form.Dev3G, 1);
		inputCtrl(document.form.modem_country, 1);
		inputCtrl(document.form.modem_isp, 1);
		inputCtrl(document.form.modem_apn, 1);
		if(pin_opt) inputCtrl(document.form.modem_pincode, 1);
		inputCtrl(document.form.modem_dialnum, 1);
		inputCtrl(document.form.modem_user, 1);
		inputCtrl(document.form.modem_pass, 1);
		inputCtrl(document.form.modem_ttlsid, 0);
		//$("hsdpa_hint").style.display = "";
	}
	else if(mode == "3"){ // TD-SCDMA
		inputCtrl(document.form.Dev3G, 1);
		inputCtrl(document.form.modem_country, 1);
		inputCtrl(document.form.modem_isp, 1);
		inputCtrl(document.form.modem_apn, 1);
		if(pin_opt) inputCtrl(document.form.modem_pincode, 1);
		inputCtrl(document.form.modem_dialnum, 1);
		inputCtrl(document.form.modem_user, 1);
		inputCtrl(document.form.modem_pass, 1);
		inputCtrl(document.form.modem_ttlsid, 0);
		//$("hsdpa_hint").style.display = "";
	}
	else if(mode == "4"){	// WiMAX
		inputCtrl(document.form.Dev3G, 1);
		inputCtrl(document.form.modem_country, 1);
		inputCtrl(document.form.modem_isp, 1);
		inputCtrl(document.form.modem_apn, 0);
		//inputCtrl(document.form.wan_3g_pin, 0);
		inputCtrl(document.form.modem_dialnum, 0);
		inputCtrl(document.form.modem_user, 1);
		inputCtrl(document.form.modem_pass, 1);
		inputCtrl(document.form.modem_ttlsid, 1);
		//$("hsdpa_hint").style.display = "";
	}
	else{		//stop
		inputCtrl(document.form.Dev3G, 0);
		inputCtrl(document.form.modem_country, 0);
		inputCtrl(document.form.modem_isp, 0);
		inputCtrl(document.form.modem_apn, 0);
		if(pin_opt) inputCtrl(document.form.modem_pincode, 0);
		inputCtrl(document.form.modem_dialnum, 0);
		inputCtrl(document.form.modem_user, 0);
		inputCtrl(document.form.modem_pass, 0);
		inputCtrl(document.form.modem_ttlsid, 0);
		//$("hsdpa_hint").style.display = "none";
	}
	//gen_country_list(mode);
}

function switch_modem_mode_enable(mode){
	if(mode == 0){
		inputHideCtrl(document.form.modem_enable_option, 0);
		inputCtrl(document.form.Dev3G, 0);
		inputCtrl(document.form.modem_country, 0);
		inputCtrl(document.form.modem_isp, 0);
		inputCtrl(document.form.modem_apn, 0);
		if(pin_opt) inputCtrl(document.form.modem_pincode, 0);
		inputCtrl(document.form.modem_dialnum, 0);
		inputCtrl(document.form.modem_user, 0);
		inputCtrl(document.form.modem_pass, 0);
		inputCtrl(document.form.modem_ttlsid, 0);
		//$("hsdpa_hint").style.display = "none";
		document.form.modem_enable.value = "0";
	}
	else{
		switch_modem_mode(document.form.modem_enable_option.value);
		gen_list(document.form.modem_enable_option.value);
		show_ISP_list();
		show_APN_list();
	}	
}

function gen_country_list(mode){
	/*if(mode == "1"){
		show_wcdma_country_list();
	}
	else if(mode == "2"){
		show_cdma2000_country_list();
	}
	else if(mode == "3"){
		show_tdscdma_country_list();
	}
	else if(mode == "4"){
		show_4G_country_list();
	}*/
	show_wcdma_country_list();
}

function gen_list(mode){
	/*if(mode == "1"){
		gen_wcdma_list();
	}
	else if(mode == "2"){
		gen_cdma2000_list();
	}
	else if(mode == "3"){
		gen_tdscdma_list();
	}
	else if(mode == "4"){
		gen_4G_list();
	}*/
	gen_wcdma_list();
}

function show_ISP_list(){
	free_options($("modem_isp"));
	$("modem_isp").options.length = isplist.length;

	for(var i = 0; i < isplist.length; i++){
		$("modem_isp").options[i] = new Option(isplist[i], isplist[i]);
		if(isplist[i] == isp)
			$("modem_isp").options[i].selected = 1;
	}
}

function show_APN_list(){
	var ISPlist = $("modem_isp").value;
	var isp_order = -1;

	for(isp_order = 0; isp_order < isplist.length; ++isp_order){
		if(isplist[isp_order] == ISPlist)
			break;
		else if(isp_order == isplist.length-1){
			isp_order = -1;
			break;
		}
	}

	if(isp_order == -1){
		alert("system error");
		return;
	}

	if(document.form.modem_country.value == ""){
		inputHideCtrl(document.form.modem_isp, 0);
		inputHideCtrl(document.form.modem_enable_option, 1);
	}
	else{
		inputHideCtrl(document.form.modem_isp, 1);
		inputHideCtrl(document.form.modem_enable_option, 0);

		// document.form.modem_isp.value = "";
		if(document.form.modem_isp.value == "China Mobile")
			document.form.modem_enable.value = "3";
		else if(document.form.modem_isp.value == "Yota"
				|| document.form.modem_isp.value == "GMC"
				|| document.form.modem_isp.value == "FreshTel"
				){
			document.form.modem_enable.value = "4";
			inputCtrl(document.form.modem_ttlsid, 1);
		}
		else if(daillist[isp_order] == "#777")
			document.form.modem_enable.value = "2";
		else
			document.form.modem_enable.value = "1";
	}

	//switch_modem_mode(document.form.modem_enable.value); //Viz 2012.02
	if(document.form.modem_enable.value == "1"
			|| document.form.modem_enable.value == "2"
			|| document.form.modem_enable.value == "3"
			){
		if(ISPlist == isp
				&& (apn != "" || dialnum != "" || user != "" || pass != "")){
			$("modem_apn").value = apn;
			$("modem_dialnum").value = dialnum;
			$("modem_user").value = user;
			$("modem_pass").value = pass;
		}
		else{
			$("modem_apn").value = apnlist[isp_order];
			$("modem_dialnum").value = daillist[isp_order];
			$("modem_user").value = userlist[isp_order];
			$("modem_pass").value = passlist[isp_order];
		}
	}
	else if(document.form.modem_enable.value == "4"){
		$("modem_apn").value = "";
		$("modem_dialnum").value = "";

		if(ISPlist == isp
				&& (user != "" || pass != "")){
			$("modem_user").value = user;
			$("modem_pass").value = pass;
		}
		else{
			$("modem_user").value = userlist[isp_order];
			$("modem_pass").value = passlist[isp_order];
		}
	}
}

function applyRule(){
	var mode = document.form.modem_enable.value;
	
	//check pin code
	if(document.form.modem_pincode.value != "") {
		if(document.form.modem_pincode.value.search(/^\d{4,8}$/)==-1) {
			alert("<#JS_InvalidPIN#>");
			return;
		}
	}
	
	if(document.form.modem_enable.value != "0"){
		showLoading(); 
		document.form.submit();
	}
	else{
		showLoading(); 
		document.form.submit();
	}
}

function done_validating(action){
	refreshpage();
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="hiddenMask" class="popup_bg">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
		<tr>
		<td>
			<div class="drword" id="drword" style="height:110px;"><#Main_alert_proceeding_desc4#> <#Main_alert_proceeding_desc1#>...
				<br/>
				<br/>
	    </div>
		  <div class="drImg"><img src="images/alertImg.png"></div>
			<div style="height:70px;"></div>
		</td>
		</tr>
	</table>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_Modem_Content.asp">
<input type="hidden" name="next_page" value="Advanced_Modem_Content.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="reboot">
<input type="hidden" name="action_wait" value="<% get_default_reboot_time(); %>">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="modem_enable" value="<% nvram_get("modem_enable"); %>">

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
	<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td align="left" valign="top">
	  <table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle" style="-webkit-border-radius: 3px;-moz-border-radius: 3px;border-radius:3px;">
		<tbody>
		<tr>
			<td bgcolor="#4D595D" valign="top" height="680px">
				<div>&nbsp;</div>
				<div style="width:730px">
					<table width="730px">
						<tr>
							<td align="left">
								<span class="formfonttitle"><#menu5_4_4#></span>
							</td>
							<td align="right">
								<img onclick="go_setting('/APP_Installation.asp')" align="right" style="cursor:pointer;position:absolute;margin-left:-20px;margin-top:-30px;" title="Back to USB Extension" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
							</td>
						</tr>
					</table>
				</div>
				<div style="margin:5px;"><img src="/images/New_ui/export/line_export.png"></div>
	      <div class="formfontdesc"><#HSDPAConfig_hsdpa_enable_hint1#></div>			  

						<table  width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" id="WANscap">
							<thead>
							<tr>
								<td colspan="2">WAN index</td>
							</tr>
							</thead>							
							<tr>
								<th>WAN Type</th>
								<td align="left">
									<select id="" class="input_option" name="wan_unit" onchange="change_wan_unit();">
									</select>
								</td>
							</tr>
						</table>

			  <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px">
					<thead>
					<tr>
						<td colspan="2"><#t2BC#></td>
					</tr>
					</thead>							

					<tr>
						<th><#HSDPAConfig_hsdpa_enable_itemname#></th>
						<td>
							<input type="radio" value="1" onclick="switch_modem_mode_enable(this.value);" name="modem_enable_radio" checked><#checkbox_Yes#>
							<input type="radio" value="0" onclick="switch_modem_mode_enable(this.value);" name="modem_enable_radio" <% nvram_match("modem_enable", "0", "checked"); %>><#checkbox_No#>
						</td>
					</tr>

					<tr>
          	<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,9);"><#HSDPAConfig_Country_itemname#></a></th>
            <td>
            	<select name="modem_country" id="isp_countrys" class="input_option" onfocus="parent.showHelpofDrSurf(21,9);" onchange="gen_list(document.form.modem_enable.value);show_ISP_list();show_APN_list();"></select>
						</td>
					</tr>
                                
          <tr>
          	<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,8);"><#HSDPAConfig_ISP_itemname#></a></th>
            <td>
            	<select name="modem_isp" id="modem_isp" class="input_option" onchange="show_APN_list();"></select>
            </td>
          </tr>

					<tr>
						<th width="40%">
							<a class="hintstyle" href="javascript:void(0);" onclick="openHint(21,1);"><#menu5_4_4#></a>
						</th>
						<td>
							<select name="modem_enable_option" id="modem_enable_option" class="input_option" onchange="switch_modem_mode(this.value);gen_list(this.value);show_ISP_list();show_APN_list();">
								<!--option value="0" <% nvram_match_x("General", "modem_enable", "0", "selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option-->
								<option value="1" <% nvram_match_x("General", "modem_enable", "1", "selected"); %>>WCDMA (UMTS)</option>
								<option value="2" <% nvram_match_x("General", "modem_enable", "2", "selected"); %>>CDMA2000 (EVDO)</option>
								<option value="3" <% nvram_match_x("General", "modem_enable", "3", "selected"); %>>TD-SCDMA</option>
								<option value="4" <% nvram_match_x("General", "modem_enable", "4", "selected"); %>>WiMAX</option>
							</select>
							
							<br/><span id="hsdpa_hint" style="display:none;"><#HSDPAConfig_hsdpa_enable_hint2#></span>
						</td>
					</tr>

          <tr>
						<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,3);"><#HSDPAConfig_private_apn_itemname#></a></th>
            <td>
            	<input id="modem_apn" name="modem_apn" class="input_20_table" type="text" value=""/>
						</td>
					</tr>

					<tr>
						<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,10);"><#HSDPAConfig_DialNum_itemname#></a></th>
						<td>
							<input id="modem_dialnum" name="modem_dialnum" class="input_20_table" type="text" value=""/>
						</td>
					</tr>
					
					<tr>
						<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,2);"><#HSDPAConfig_PIN_itemname#></a></th>
						<td>
							<input id="modem_pincode" name="modem_pincode" class="input_20_table" type="password" autocapitalization="off" maxLength="8" value="<% nvram_get("modem_pincode"); %>"/>
						</td>
					</tr>
                                
					<tr>
						<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,11);"><#HSDPAConfig_Username_itemname#></a></th>
						<td>
						<input id="modem_user" name="modem_user" class="input_20_table" type="text" value="<% nvram_get("modem_user"); %>"/>
						</td>
					</tr>
                                
					<tr>
						<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,12);"><#AiDisk_Password#></a></th>
						<td>
							<input id="modem_pass" name="modem_pass" class="input_20_table" type="password" value="<% nvram_get("modem_pass"); %>"/>
						</td>
					</tr>

					<tr>
						<th>E-mail</th>
						<td>
							<input id="modem_ttlsid" name="modem_ttlsid" class="input_20_table" value="<% nvram_get("modem_ttlsid"); %>"/>
						</td>
					</tr>
                                
					<tr>
						<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,13);"><#HSDPAConfig_USBAdapter_itemname#></a></th>
						<td>
							<select name="Dev3G" id="shown_modems" class="input_option" disabled="disabled"></select>
						</td>
					</tr>
				</table>	
	
				<div class="apply_gen">
					<input class="button_gen" onclick="applyRule()" type="button" value="<#CTL_apply#>"/>
				</div>
			</td>
		</tr>
		</tbody>	
	  </table>
		</td>
	</tr>
	</table>				
			<!--===================================End of Main Content===========================================-->
	</td>
  <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>
</form>					

<div id="footer"></div>
</body>
</html>
