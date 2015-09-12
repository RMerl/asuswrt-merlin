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
<title><#Web_Title#> - <#menu5_4_4#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<style>
#ClientList_Block_PC{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	*margin-top:26px;	
	margin-left:2px;
	*margin-left:-189px;
	width:181px;
	text-align:left;	
	height:auto;
	overflow-y:auto;
	z-index:200;
	padding: 1px;
	display:none;
}
#ClientList_Block_PC div{
	background-color:#576D73;
	height:auto;
	*height:20px;
	line-height:20px;
	text-decoration:none;
	font-family: Lucida Console;
	padding-left:2px;
}

#ClientList_Block_PC a{
	background-color:#EFEFEF;
	color:#FFF;
	font-size:12px;
	font-family:Arial, Helvetica, sans-serif;
	text-decoration:none;	
}
#ClientList_Block_PC div:hover, #ClientList_Block a:hover{
	background-color:#3366FF;
	color:#FFFFFF;
	cursor:default;
}	
</style>	
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/wcdma_list.js"></script>
<script type="text/javaScript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>

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
var protolist = new Array();
var isplist = new Array();
var apnlist = new Array();
var daillist = new Array();
var userlist = new Array();
var passlist = new Array();

/* start of DualWAN */
var wans_dualwan = '<% nvram_get("wans_dualwan"); %>';
var usb_modem_enable = 0;
<% wan_get_parameter(); %>
if(dualWAN_support){
	usb_modem_enable = (usb_index >= 0)? 1:0;
}
else{
	usb_modem_enable = ('<% nvram_get("modem_enable"); %>' != "0")? 1:0;
}
var modem_android_orig = '<% nvram_get("modem_android"); %>';


function genWANSoption(){
	for(i=0; i<wans_dualwan.split(" ").length; i++){
	var wans_dualwan_NAME = wans_dualwan.split(" ")[i].toUpperCase();
		//MODELDEP: DSL-N55U, DSL-N55U-B, DSL-AC68U, DSL-AC68R
		if(wans_dualwan_NAME == "LAN" && 
			(productid == "DSL-N55U" || productid == "DSL-N55U-B" || productid == "DSL-AC68U" || productid == "DSL-AC68R"))	
			wans_dualwan_NAME = "Ethernet WAN";
		else if(wans_dualwan_NAME == "LAN")
			wans_dualwan_NAME = "Ethernet LAN";
		document.form.wan_unit.options[i] = new Option(wans_dualwan_NAME, i);
	}
	document.form.wan_unit.selectedIndex = '<% nvram_get("wan_unit"); %>';
}
/* end of DualWAN */ 
	
function initial(){
	show_menu();

	if(dualWAN_support && '<% nvram_get("wans_dualwan"); %>'.search("none") < 0){
		genWANSoption();
	}
	else{
		document.form.wan_unit.disabled = true;
		document.getElementById("WANscap").style.display = "none";
	}

	if(usb_modem_enable){
		document.getElementById("modem_android_tr").style.display="";
		if(modem_android_orig == "0"){
			switch_modem_mode('<% nvram_get("modem_enable"); %>');
			gen_country_list();
			reloadProfile();
			inputCtrl(document.form.modem_autoapn, 1);
			change_apn_mode();
		}
		else{
			hide_usb_settings(1);
			document.getElementById("android_desc").style.display="";
		}
	}
	else{
		hide_usb_settings();
	}

	if(!dualWAN_support)
		document.form.wans_dualwan.disalbed = true;

	$('#usb_modem_switch').iphoneSwitch(usb_modem_enable,
		function() {
			if(dualWAN_support)
				document.form.wans_dualwan.value = wans_dualwan_array[0]+" usb";
			document.getElementById("modem_android_tr").style.display="";
			if(document.form.modem_android.value == "0"){
				switch_modem_mode(document.form.modem_enable.value);
				gen_country_list();
				reloadProfile();
				inputCtrl(document.form.modem_autoapn, 1);
				inputCtrl(document.form.modem_authmode, 1);
				change_apn_mode();
			}
			else{
				document.getElementById("android_desc").style.display="";					
				hide_usb_settings(1);
			}				
		},
		function() {
			if(dualWAN_support){
				if(usb_index == 0)
						document.form.wans_dualwan.value = wans_dualwan_array[1]+" none";
					else
						document.form.wans_dualwan.value = wans_dualwan_array[0]+" none";
			}
			document.getElementById("modem_android_tr").style.display="none";
			hide_usb_settings();
		}
	);

	if(!dualWAN_support){
		document.getElementById("_APP_Installation").innerHTML = '<table><tbody><tr><td><div class="_APP_Installation"></div></td><td><div style="width:120px;"><#Menu_usb_application#></div></td></tr></tbody></table>';
		document.getElementById("_APP_Installation").className = "menu_clicked";
	}

	if(!wimax_support){
		for (var i = 0; i < document.form.modem_enable_option.options.length; i++) {
			if (document.form.modem_enable_option.options[i].value == "4") {
				document.form.modem_enable_option.options.remove(i);
				break;
			}
		}
	}

	check_dongle_status();

}

function reloadProfile(){
	if(document.form.modem_enable.value == 0)
		return 0;

	gen_list();
	show_ISP_list();
	show_APN_list();
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

	free_options(document.getElementById("shown_modems"));
	for(var i = 0; i < modemlist.length; i++){
		if(modemlist[i] == "AUTO")
			document.getElementById("shown_modems").options[i] = new Option("<#Auto#>", modemlist[i]);
		else	
			document.getElementById("shown_modems").options[i] = new Option(modemlist[i], modemlist[i]);
			
		if(modemlist[i] == modem)
			document.getElementById("shown_modems").options[i].selected = "1";
	}
}

function show_4G_modem_list(){
	modemlist = new Array(
			"AUTO"
			, "Samsung U200"
			//, "Beceem BCMS250"
			);

	free_options(document.getElementById("shown_modems"));
	for(var i = 0; i < modemlist.length; i++){
		if(modemlist[i] == "AUTO")
			document.getElementById("shown_modems").options[i] = new Option("<#Auto#>", modemlist[i]);
		else	
			document.getElementById("shown_modems").options[i] = new Option(modemlist[i], modemlist[i]);
		if(modemlist[i] == modem)
			document.getElementById("shown_modems").options[i].selected = "1";
	}
}

function switch_modem_mode(mode){
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
		inputCtrl(document.form.modem_mtu, 1);
		//document.getElementById("hsdpa_hint").style.display = "";
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
		inputCtrl(document.form.modem_mtu, 1);
		//document.getElementById("hsdpa_hint").style.display = "";
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
		inputCtrl(document.form.modem_mtu, 1);
		//document.getElementById("hsdpa_hint").style.display = "";
	}
	else if(mode == "4"){	// WiMAX
		inputCtrl(document.form.Dev3G, 1);
		inputCtrl(document.form.modem_country, 1);
		inputCtrl(document.form.modem_isp, 1);
		inputCtrl(document.form.modem_apn, 0);
		if(pin_opt) inputCtrl(document.form.modem_pincode, 1);
		inputCtrl(document.form.modem_dialnum, 0);
		inputCtrl(document.form.modem_user, 1);
		inputCtrl(document.form.modem_pass, 1);
		inputCtrl(document.form.modem_ttlsid, 1);
		inputCtrl(document.form.modem_mtu, 1);
		//document.getElementById("hsdpa_hint").style.display = "";
	}
	else{	// Disable (mode == 0)
		inputCtrl(document.form.Dev3G, 0);
		inputCtrl(document.form.modem_enable_option, 0);
		inputCtrl(document.form.modem_country, 0);
		inputCtrl(document.form.modem_isp, 0);
		inputCtrl(document.form.modem_apn, 0);
		if(pin_opt) inputCtrl(document.form.modem_pincode, 0);
		inputCtrl(document.form.modem_dialnum, 0);
		inputCtrl(document.form.modem_user, 0);
		inputCtrl(document.form.modem_pass, 0);
		inputCtrl(document.form.modem_ttlsid, 0);
		inputCtrl(document.form.modem_mtu, 0);
		//document.getElementById("hsdpa_hint").style.display = "none";
		document.form.modem_enable.value = "0";
	}
}

function show_ISP_list(){
	var removeItem = 0;
	free_options(document.form.modem_isp);
	document.form.modem_isp.options.length = isplist.length;

	for(var i = 0; i < isplist.length; i++){
	  if(protolist[i] == 4 && !wimax_support){
			document.form.modem_isp.options.length = document.form.modem_isp.options.length - 1;

			if(document.form.modem_isp.options.length > 0)
				continue;
			else{
				alert(Untranslated.ISP_not_support);
				document.form.modem_country.focus();
				document.form.modem_country.selectedIndex = countrylist.length-1;
				break;
			}
		}
		else
			document.form.modem_isp.options[i] = new Option(isplist[i], isplist[i]);

		if(isplist[i] == isp)
			document.form.modem_isp.options[i].selected = 1;
	}
}

function show_APN_list(){
	var ISPlist = document.form.modem_isp.value;
	var Countrylist = document.form.modem_country.value;

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
	
	/* use manual or location */
	if(document.form.modem_country.value == ""){
		inputCtrl(document.form.modem_isp, 0);
		inputCtrl(document.form.modem_enable_option, 1);
	}
	else{
		inputHideCtrl(document.form.modem_isp, 1);
		inputHideCtrl(document.form.modem_enable_option, 0);
		if(protolist[isp_order] == "")
			protolist[isp_order] = 1;
	}

	if(Countrylist == ""){
		if('<% nvram_get("modem_enable"); %>' == document.getElementById('modem_enable_option').value){
			document.getElementById("modem_apn").value = apn;
			document.getElementById("modem_dialnum").value = dialnum;
			document.getElementById("modem_user").value = user;
			document.getElementById("modem_pass").value = pass;
		}
		else{
			document.getElementById("modem_apn").value = apnlist[isp_order];
			document.getElementById("modem_dialnum").value = daillist[isp_order];
			document.getElementById("modem_user").value = userlist[isp_order];
			document.getElementById("modem_pass").value = passlist[isp_order];
		}
	}
	else if(protolist[isp_order] != "4"){
		if(ISPlist == isp && Countrylist == country && (apn != "" || dialnum != "" || user != "" || pass != "")){
			if(typeof(apnlist[isp_order]) == 'object' && apnlist[isp_order].constructor == Array){
				document.getElementById("pull_arrow").style.display = '';
				showLANIPList(isp_order);
			}
			else{
				document.getElementById("pull_arrow").style.display = 'none';
				document.getElementById('ClientList_Block_PC').style.display = 'none';
			}

			document.getElementById("modem_apn").value = apn;
			document.getElementById("modem_dialnum").value = dialnum;
			document.getElementById("modem_user").value = user;
			document.getElementById("modem_pass").value = pass;
		}
		else{
			if(typeof(apnlist[isp_order]) == 'object' && apnlist[isp_order].constructor == Array){
				document.getElementById("pull_arrow").style.display = '';
				showLANIPList(isp_order);
			}
			else{
				document.getElementById("pull_arrow").style.display = 'none';
				document.getElementById('ClientList_Block_PC').style.display = 'none';
				document.getElementById("modem_apn").value = apnlist[isp_order];
			}

			document.getElementById("modem_dialnum").value = daillist[isp_order];
			document.getElementById("modem_user").value = userlist[isp_order];
			document.getElementById("modem_pass").value = passlist[isp_order];
		}
	}
	else{
		document.getElementById("modem_apn").value = "";
		document.getElementById("modem_dialnum").value = "";

		if(ISPlist == isp	&& (user != "" || pass != "")){
			document.getElementById("modem_user").value = user;
			document.getElementById("modem_pass").value = pass;
		}
		else{
			document.getElementById("modem_user").value = userlist[isp_order];
			document.getElementById("modem_pass").value = passlist[isp_order];
		}
	}

	if(document.form.modem_country.value != ""){
		document.form.modem_enable.value = protolist[isp_order];
		switch_modem_mode(document.form.modem_enable.value);
	}
}

function applyRule(){
	var mode = document.form.modem_enable.value;
	
	//check pin code
	if(pin_opt && document.form.modem_pincode.value != ""){
		if(document.form.modem_pincode.value.search(/^\d{4,8}$/)==-1) {
			alert("<#JS_InvalidPIN#>");
			return;
		}
	}

	if(document.form.modem_country.value == ""){
		var valueStr = "";
		document.form.modem_isp.disabled = false;;
		document.form.modem_isp.options.length = 1;
		document.form.modem_isp.options[0] = new Option(valueStr, valueStr, false, true);
	}

	showLoading(); 
	document.form.submit();
}

/*------------ Mouse event of fake LAN IP select menu {-----------------*/
function setClientIP(apnAddr){
	document.form.modem_apn.value = apnAddr;
	hideClients_Block();
	over_var = 0;
}

function showLANIPList(isp_order){
	var code = "";
	var show_name = "";

	for(var i = 0; i < apnlist[isp_order].length; i++){
		var apnlist_col = apnlist[isp_order][i].split('&&');
		code += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\''+apnlist_col[1]+'\');"><strong>'+apnlist_col[0]+'</strong></div></a>';

		if(i == 0)
			document.form.modem_apn.value = apnlist_col[1];
	}
	code +='<!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
	document.getElementById("ClientList_Block_PC").innerHTML = code;
}

function pullLANIPList(obj){
	
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		document.getElementById("ClientList_Block_PC").style.display = 'block';		
		document.form.modem_apn.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

var over_var = 0;
var isMenuopen = 0;
function hideClients_Block(){
	document.getElementById("pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById('ClientList_Block_PC').style.display='none';
	isMenuopen = 0;
}
/*----------} Mouse event of fake LAN IP select menu-----------------*/

var dsltmp_transmode = "<% nvram_get("dsltmp_transmode"); %>";
function change_wan_unit(obj){
	if(!dualWAN_support) return;
	
	if(obj.options[obj.selectedIndex].text == "DSL"){
		if(dsltmp_transmode == "atm")
			document.form.current_page.value = "Advanced_DSL_Content.asp";
		else //ptm
			document.form.current_page.value = "Advanced_VDSL_Content.asp";	
	}else if(document.form.dsltmp_transmode){
		document.form.dsltmp_transmode.style.display = "none";
	}

	if(obj.options[obj.selectedIndex].text == "WAN" ||	obj.options[obj.selectedIndex].text == "Ethernet LAN"){
		document.form.current_page.value = "Advanced_WAN_Content.asp";
	}else	if(obj.options[obj.selectedIndex].text == "USB") {
		return false;
	}

	FormActions("apply.cgi", "change_wan_unit", "", "");
	document.form.target = "";
	document.form.submit();
}


function done_validating(action){
	refreshpage();
}

function check_dongle_status(){
	 $.ajax({
    	url: '/ajax_ddnscode.asp',
    	dataType: 'script', 

    	error: function(xhr){
      		check_dongle_status();
    	},
    	success: function(response){
			if(pin_status != "" && pin_status != "0")
				document.getElementById("pincode_status").style.display = "";
			else	
				document.getElementById("pincode_status").style.display = "none";
				
			setTimeout("check_dongle_status();",5000);
       }
   });
}

function hide_usb_settings(_flag){
	inputCtrl(document.form.modem_autoapn, 0);
	inputCtrl(document.form.modem_enable_option, 0);
	inputCtrl(document.form.modem_country, 0);
	inputCtrl(document.form.modem_isp, 0);
	inputCtrl(document.form.modem_apn, 0);
	if(pin_opt) inputCtrl(document.form.modem_pincode, 0);
	inputCtrl(document.form.modem_dialnum, 0);
	inputCtrl(document.form.modem_user, 0);
	inputCtrl(document.form.modem_pass, 0);
	inputCtrl(document.form.modem_authmode, 0);
	inputCtrl(document.form.modem_ttlsid, 0);
	inputCtrl(document.form.Dev3G, 0);
	inputCtrl(document.form.modem_mtu, (typeof(_flag) != 'undefined' && _flag) ? 1 : 0);
	document.getElementById("modem_enable_div_tr").style.display = "none";
	document.getElementById("modem_apn_div_tr").style.display = "none";
	document.getElementById("modem_dialnum_div_tr").style.display = "none";
	document.getElementById("modem_user_div_tr").style.display = "none";
	document.getElementById("modem_pass_div_tr").style.display = "none";
}

function select_usb_device(obj){
	if(obj.selectedIndex == 0){
		inputCtrl(document.form.modem_autoapn, 1);
		inputCtrl(document.form.modem_authmode, 1);
		switch_modem_mode(document.form.modem_enable_option.value);
		gen_country_list();
		reloadProfile();
		change_apn_mode();
		document.getElementById("android_desc").style.display="none";
	}
	else{
		document.getElementById("android_desc").style.display="";
		hide_usb_settings(1);
	}

}

function change_apn_mode(){
	if(document.form.modem_autoapn.value == "1"){//Automatic
		var modem_enable_str = "";
		inputCtrl(document.form.modem_country, 0);
		inputCtrl(document.form.modem_isp, 0);
		inputCtrl(document.form.modem_enable_option, 0);
		inputCtrl(document.form.modem_apn, 0);
		inputCtrl(document.form.modem_dialnum, 0);
		inputCtrl(document.form.modem_user, 0);
		inputCtrl(document.form.modem_pass, 0);
		document.getElementById("modem_enable_div_tr").style.display = "";
		document.getElementById("modem_apn_div_tr").style.display = "";
		document.getElementById("modem_dialnum_div_tr").style.display = "";
		document.getElementById("modem_user_div_tr").style.display = "";
		document.getElementById("modem_pass_div_tr").style.display = "";	
		if(document.form.modem_enable.value == "1")
			mdoem_enable_str = "WCDMA (UMTS)";
		else if(document.form.modem_enable.value == "2")
			mdoem_enable_str = "CDMA2000 (EVDO)";
		else if(document.form.modem_enable.value == "3")
			mdoem_enable_str = "TD-SCDMA";
		else if(document.form.modem_enable.value == "4")
			mdoem_enable_str = "WiMAX";
		document.getElementById("modem_enable_div").innerHTML = mdoem_enable_str;
		document.getElementById("modem_apn_div").innerHTML = apn;
		document.getElementById("modem_dialnum_div").innerHTML = dialnum;
		document.getElementById("modem_user_div").innerHTML = user;
		document.getElementById("modem_pass_div").innerHTML = pass;	
	}
	else{//Manual
		inputCtrl(document.form.modem_country, 1);
		if(document.form.modem_country.value == ""){
			inputCtrl(document.form.modem_isp, 0);
			inputCtrl(document.form.modem_enable_option, 1);			
		}
		else{
			inputCtrl(document.form.modem_isp, 1);
			inputCtrl(document.form.modem_enable_option, 0);
		}
		inputCtrl(document.form.modem_apn, 1);
		inputCtrl(document.form.modem_dialnum, 1);
		inputCtrl(document.form.modem_user, 1);
		inputCtrl(document.form.modem_pass, 1);
		document.getElementById("modem_enable_div_tr").style.display = "none";
		document.getElementById("modem_apn_div_tr").style.display = "none";
		document.getElementById("modem_dialnum_div_tr").style.display = "none";
		document.getElementById("modem_user_div_tr").style.display = "none";
		document.getElementById("modem_pass_div_tr").style.display = "none";
	}
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

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame" autocomplete="off">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_Modem_Content.asp">
<input type="hidden" name="next_page" value="Advanced_Modem_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="reboot">
<input type="hidden" name="action_wait" value="<% get_default_reboot_time(); %>">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="modem_enable" value="<% nvram_get("modem_enable"); %>">
<input type="hidden" name="wans_dualwan" value="<% nvram_get("wans_dualwan"); %>">

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
								<span class="formfonttitle"><#menu5_4_4#> / <#usb_tethering#></span>
							</td>
							<td align="right">
								<img onclick="go_setting('/APP_Installation.asp')" align="right" style="cursor:pointer;position:absolute;margin-left:-20px;margin-top:-30px;" title="<#Menu_usb_application#>" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
							</td>
						</tr>
					</table>
				</div>
				<div style="margin:5px;"><img src="/images/New_ui/export/line_export.png"></div>
	      		<div class="formfontdesc"><#HSDPAConfig_hsdpa_enable_hint1#></div>
					<table  width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" id="WANscap">
						<thead>
							<tr>
								<td colspan="2"><#wan_index#></td>
							</tr>
						</thead>
						<tr>
							<th><#wan_type#></th>
							<td align="left">
								<select class="input_option" name="wan_unit" onchange="change_wan_unit(this);"></select>
								<!--select id="dsltmp_transmode" name="dsltmp_transmode" class="input_option" style="margin-left:7px;" onChange="change_dsl_transmode(this);">
									<option value="atm" <% nvram_match("dsltmp_transmode", "atm", "selected"); %>>ADSL WAN (ATM)</option>
									<option value="ptm" <% nvram_match("dsltmp_transmode", "ptm", "selected"); %>>VDSL WAN (PTM)</option>
								</select-->
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
						<th><#enable_usb_mode#></th>
						<td>
							<div class="left" style="width:94px; float:left; cursor:pointer;" id="usb_modem_switch"></div>
							<div class="clear" style="height:32px; width:74px; position: relative; overflow: hidden"></div>
						</td>
					</tr>

					<tr id="modem_android_tr" style="display:none;">
						<th><#select_usb_device#></th>
						<td align="left">
							<select id="modem_android" name="modem_android" class="input_option" onChange="select_usb_device(this);">
								<option value="0" <% nvram_match("modem_android", "0", "selected"); %>><#menu5_4_4#></option>
								<option value="1" <% nvram_match("modem_android", "1", "selected"); %>><#Android_phone#></option>
							</select>
							<div  class="formfontdesc" id="android_desc" style="display:none; color:#FFCC00;margin-top:5px;">
								<#usb_tethering_hint0#>
								<ol style="margin-top: 0px;">
								<li><#usb_tethering_hint1#></li>
								<li><#usb_tethering_hint2#></li>
								<li><#usb_tethering_hint3#></li>
								</ol>
							</div>
						</td>
					</tr>

					<tr>
						<th width="40%">APN Configuration</th><!--untranslated-->
						<td>
							<select name="modem_autoapn" id="modem_autoapn" class="input_option" onchange="change_apn_mode();">
								<option value="1" <% nvram_match("modem_autoapn", "1","selected"); %>>Automatic</option><!--untranslated-->
								<option value="0" <% nvram_match("modem_autoapn", "0","selected"); %>><#Manual_Setting_btn#></option>
							</select>
						</td>
					</tr>					

					<tr>
          				<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,9);"><#HSDPAConfig_Country_itemname#></a></th>
            			<td>
            				<select name="modem_country" class="input_option" onchange="switch_modem_mode(document.form.modem_enable_option.value);reloadProfile();"></select>
						</td>
					</tr>
                                
			        <tr>
			         	<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,8);"><#HSDPAConfig_ISP_itemname#></a></th>
			            <td>
			            	<select name="modem_isp" class="input_option" onchange="show_APN_list();"></select>
			            </td>
			        </tr>

					<tr>
						<th width="40%">
							<a class="hintstyle" href="javascript:void(0);" onclick="openHint(21,1);"><#menu5_4_4#></a>
						</th>
						<td>
							<select name="modem_enable_option" id="modem_enable_option" class="input_option" onchange="switch_modem_mode(this.value);reloadProfile();">
								<option value="1" <% nvram_match("modem_enable", "1", "selected"); %>>WCDMA (UMTS)</option>
								<option value="2" <% nvram_match("modem_enable", "2", "selected"); %>>CDMA2000 (EVDO)</option>
								<option value="3" <% nvram_match("modem_enable", "3", "selected"); %>>TD-SCDMA</option>
								<option value="4" <% nvram_match("modem_enable", "4", "selected"); %>>WiMAX</option>
							</select>
							
							<br/><span id="hsdpa_hint" style="display:none;"><#HSDPAConfig_hsdpa_enable_hint2#></span>
						</td>
					</tr>

          			<tr id="modem_enable_div_tr" style="display:none;">
						<th><#menu5_4_4#></th>
	            		<td>
							<div id="modem_enable_div" style="color:#FFFFFF; margin-left:1px;"></div>
						</td>
					</tr>					

          			<tr>
						<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,3);"><#HSDPAConfig_private_apn_itemname#></a></th>
            			<td>
            				<input id="modem_apn" name="modem_apn" class="input_20_table" maxlength="32" type="text" value="" autocorrect="off" autocapitalize="off"/>
           					<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;*margin-left:-3px;*margin-top:1px;" onclick="pullLANIPList(this);" title="<#select_APN_service#>" onmouseover="over_var=1;" onmouseout="over_var=0;">
							<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>
						</td>
					</tr>

          			<tr id="modem_apn_div_tr" style="display:none;">
						<th><#HSDPAConfig_private_apn_itemname#></th>
	            		<td>
							<div id="modem_apn_div" style="color:#FFFFFF; margin-left:1px;"></div>
						</td>
					</tr>

					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(21,10);"><#HSDPAConfig_DialNum_itemname#></a></th>
						<td>
							<input id="modem_dialnum" name="modem_dialnum" class="input_20_table" maxlength="32" type="text" value="" autocorrect="off" autocapitalize="off"/>
						</td>
					</tr>
                       
          			<tr id="modem_dialnum_div_tr" style="display:none;">
						<th><#HSDPAConfig_DialNum_itemname#></th>
	            		<td>
							<div id="modem_dialnum_div" style="color:#FFFFFF; margin-left:1px;"></div>
						</td>
					</tr>

					<tr>
						<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,11);"><#HSDPAConfig_Username_itemname#></a></th>
						<td>
						<input id="modem_user" name="modem_user" class="input_20_table" maxlength="32" type="text" value="<% nvram_get("modem_user"); %>" autocorrect="off" autocapitalize="off"/>
						</td>
					</tr>

          			<tr id="modem_user_div_tr" style="display:none;">
						<th><#HSDPAConfig_Username_itemname#></th>
	            		<td>
							<div id="modem_user_div" style="color:#FFFFFF; margin-left:1px;"></div>
						</td>
					</tr>					
                                
					<tr>
						<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,12);"><#PPPConnection_Password_itemname#></a></th>
						<td>
							<input id="modem_pass" name="modem_pass" class="input_20_table" maxlength="32" type="password" value="<% nvram_get("modem_pass"); %>" autocorrect="off" autocapitalize="off"/>
						</td>
					</tr>

          			<tr id="modem_pass_div_tr" style="display:none;">
						<th><#PPPConnection_Password_itemname#></th>
	            		<td>
							<div id="modem_pass_div" style="color:#FFFFFF; margin-left:1px;"></div>
						</td>
					</tr>

					<tr>
						<th><#PPPConnection_Authentication_itemname#></th>
						<td>
							<select name="modem_authmode" id="modem_authmode" class="input_option">
								<option value="0" <% nvram_match("modem_authmode", "0", "selected"); %>><#wl_securitylevel_0#></option>
								<option value="1" <% nvram_match("modem_authmode", "1", "selected"); %>>PAP</option>
								<option value="2" <% nvram_match("modem_authmode", "2", "selected"); %>>CHAP</option>
								<option value="3" <% nvram_match("modem_authmode", "3", "selected"); %>>PAP / CHAP</option>
							</select>
						</td>
					</tr>									

					<tr style="display:none;">
						<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,2);"><#PIN_code#></a></th>
						<td>
							<input id="modem_pincode" name="modem_pincode" class="input_20_table" style="margin-left:0px;" type="password" maxLength="8" value="<% nvram_get("modem_pincode"); %>" autocorrect="off" autocapitalize="off"/>
							<br><span id="pincode_status" style="display:none;"><#pincode_wrong#></span>
						</td>
					</tr>

					<tr>
						<th>E-mail</th>
						<td>
							<input id="modem_ttlsid" name="modem_ttlsid" class="input_20_table" maxlength="64" value="<% nvram_get("modem_ttlsid"); %>"/>
						</td>
					</tr>
                                
					<tr>
						<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,13);"><#HSDPAConfig_USBAdapter_itemname#></a></th>
						<td>
							<select name="Dev3G" id="shown_modems" class="input_option" disabled="disabled"></select>
						</td>
					</tr>

					<tr>
						<th>USB MTU</th>
						<td>
							<input type="text" maxlength="5" name="modem_mtu" class="input_6_table" value="<% nvram_get("modem_mtu"); %>" onKeyPress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off"/>
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
