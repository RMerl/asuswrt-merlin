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
<title><#Web_Title#> - <#menu5_6_1_title#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
<script language="JavaScript" type="text/javascript" src="/form.js"></script>
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
	box-shadow: 3px 3px 10px #000;
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
var sw_mode_orig = '<% nvram_get("sw_mode"); %>';
var wlc_express_orig = '<% nvram_get("wlc_express"); %>';
if((sw_mode_orig == 2 || sw_mode_orig == 3) && '<% nvram_get("wlc_psta"); %>' == 1)
	sw_mode_orig = 4;

window.onresize = function() {
	if(document.getElementById("routerSSID").style.display == "block") {
		cal_panel_block("routerSSID", 0.25);
	}
} 
if(sw_mode_orig == 3 && '<% nvram_get("wlc_psta"); %>' == 2)
	sw_mode_orig = 2;

function initial(){
	show_menu();
	setScenerion(sw_mode_orig, document.form.wlc_express.value);
	Senario_shift();

	if(downsize_4m_support || downsize_8m_support)
		document.getElementById("Senario").style.display = "none";
		
	if(productid.indexOf("RP") != -1){
		document.getElementById("routerMode").style.display = "none";
		document.getElementById("sw_mode1_radio").disabled = true;
		
		if(band5g_support){
			document.getElementById("rp_express_2g").style.display = "";
			document.getElementById("rp_express_5g").style.display = "";
		}
	}	

	if(!repeater_support){
		document.getElementById("repeaterMode").style.display = "none";
		document.getElementById("sw_mode2_radio").disabled = true;
	}

	if(!psta_support){
		document.getElementById("mbMode").style.display = "none";
		document.getElementById("sw_mode4_radio").disabled = true;
	}
}

function Senario_shift(){
	var isIE = navigator.userAgent.search("MSIE") > -1; 
	if(isIE)
		document.getElementById("Senario").style.width = "700px";
		document.getElementById("Senario").style.marginLeft = "5px";
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
		if(document.form.sw_mode.value != 2){				
			alert("<#Web_Title2#> <#op_already_configured#>");
			return false;
		}
		else{		//Repeater, Express Way 2.4 GHz, Express Way 5 GHz
			if(wlc_express_orig == document.form.sw_mode.value){
				alert("<#Web_Title2#> <#op_already_configured#>");
				return false;
			}
		}
	}

  if(document.form.sw_mode.value == 2){
		if(document.form.wlc_express.value == 1)
			parent.location.href = '/QIS_wizard.htm?flag=sitesurvey_exp2';
		else if(document.form.wlc_express.value == 2)	
			parent.location.href = '/QIS_wizard.htm?flag=sitesurvey_exp5';
		else	
			parent.location.href = '/QIS_wizard.htm?flag=sitesurvey_rep';
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
				if(wl_info.band5g_2_support){
					restore_wl_config_wep("wl2_");
					restore_wl_config("wl2.1_");
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
			
			if(wl_info.band5g_2_support){
				inputCtrl(document.form.wl2_ssid,1);
				inputCtrl(document.form.wl2_crypto,1);
				inputCtrl(document.form.wl2_wpa_psk,1);
				inputCtrl(document.form.wl2_auth_mode_x,1);
				close_guest_unit(2,1);
			}	

			if(!band5g_support){			
				document.getElementById('wl_unit_field_1').style.display="none";
				document.getElementById('wl_unit_field_2').style.display="none";
				document.getElementById('wl_unit_field_3').style.display="none";	
				document.getElementById('routerSSID').style.height="370px";				
			}

			if(wl_info.band5g_2_support){
				document.getElementById("wl_unit_field_4").style.display = "";
				document.getElementById("wl_unit_field_5").style.display = "";
				document.getElementById("wl_unit_field_6").style.display = "";	
				document.getElementById("wl_unit_field_1_1").innerHTML = '5 GHz-1 - <#Security#>';
				document.getElementById("syncCheckbox").innerHTML = "<#qis_ssid_desc1#>";
				document.getElementById("syncCheckbox_5_2").innerHTML = "<#qis_ssid_desc2#>";
				document.getElementById('routerSSID').style.height="520px";
			}

			if(smart_connect_support){
				document.getElementById("smart_connect_table").style.display="";
				document.getElementById('routerSSID').style.height="620px";
				change_smart_con('<% nvram_get("smart_connect_x"); %>');
			}
			
			cal_panel_block("routerSSID", 0.25);
			$("#routerSSID").fadeIn(300);
			$("#forSSID_bg").fadeIn(300);
			document.getElementById("forSSID_bg").style.visibility = "visible";	
			return true;			
		}
	}

	applyRule();
}

function applyRule(){
	if(document.form.sw_mode.value == 1 && (sw_mode_orig == '2' || sw_mode_orig == '4')){
		if(!validator.stringSSID(document.form.wl0_ssid)){ //validate 2.4G SSID
			document.form.wl0_ssid.focus();
			return false;
		}	

		if(document.form.wl0_wpa_psk.value != ""){
			if(is_KR_sku){
				if(!validator.psk_KR(document.form.wl0_wpa_psk))           //validate 2.4G WPA2 key
                                        return false;
			}
			else{
				if(!validator.psk(document.form.wl0_wpa_psk))		//validate 2.4G WPA2 key
					return false;
			}		
		
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
			if(!validator.stringSSID(document.form.wl1_ssid)){   //validate 5G SSID
				document.form.wl1_ssid.focus();
				return false;
			}
			
			if(document.form.wl1_wpa_psk.value != ""){
				if(is_KR_sku){
					if(!validator.psk_KR(document.form.wl1_wpa_psk))           //validate 2.4G WPA2 key
						return false;
				}
				else{
					if(!validator.psk(document.form.wl1_wpa_psk))	//validate 5G WPA2 key
						return false;
				}
			
				document.form.wl1_auth_mode_x.value = "psk2";
				document.form.wl1_crypto.value = "aes";
			}
			else
				document.form.wl1_auth_mode_x.value = "open";
		}

		if(wl_info.band5g_2_support){
			inputCtrl(document.form.wl2_ssid,1);
			inputCtrl(document.form.wl2_crypto,1);
			inputCtrl(document.form.wl2_wpa_psk,1);
			inputCtrl(document.form.wl2_auth_mode_x,1);
			if(!validator.stringSSID(document.form.wl2_ssid)){   //validate 5G-2 SSID
				document.form.wl2_ssid.focus();
				return false;
			}
			
			if(document.form.wl2_wpa_psk.value != ""){
				if(is_KR_sku){
					if(!validator.psk_KR(document.form.wl2_wpa_psk))           //validate 2.4G WPA2 key
						return false;
				}
				else{
					if(!validator.psk(document.form.wl2_wpa_psk))	//validate 5G-2 WPA2 key
						return false;
				}			

				document.form.wl1_auth_mode_x.value = "psk2";
				document.form.wl1_crypto.value = "aes";
			}
			else
				document.form.wl1_auth_mode_x.value = "open";
		}
			
	}

	showLoading();	
	document.form.target="hidden_frame";
	document.getElementById("forSSID_bg").style.visibility = "hidden";
	document.form.submit();	
}

function done_validating(action){
	refreshpage();
}

var id_WANunplungHint;
/*
Router:	             sw_mode: 1, wlc_express: 0, wlc_psta: 0
Repeater:            sw_mode: 2, wlc_express: 0, wlc_psta: 0
Express Way 2.4 GHz: sw_mode: 2, wlc_express: 1, wlc_psta: 0
Express Way 5 GHz:   sw_mode: 2, wlc_express: 2, wlc_psta: 0
AP mode:             sw_mode: 3, wlc_express: 0, wlc_psta: 0
Media Bridge:        sw_mode: 3, wlc_express: 0, wlc_psta: 1
*/

function setScenerion(mode, express){
	if(mode == '2'){
		document.form.sw_mode.value = 2;
		$("#Senario").css({"height": "", "background": "url(/images/New_ui/re.jpg) center no-repeat", "margin-bottom": "30px"});
		
		clearTimeout(id_WANunplungHint);
		$("#Unplug-hint").css("display", "none");
		if(express == 1){	//Express Way 2.4 GHz
			document.form.sw_mode_radio[2].checked = true;
			document.form.wlc_express.value = 1;
			//untranslated string
			$("#mode_desc").html("In Express Way-2.4GHz mode, <#Web_Title2#> wireless connect to an existing 2.4 GHz WiFi channel and extend the wireless coverage by 5 GHz WiFi channel only.<br/><span style=\"color:#FC0\"><#deviceDiscorvy2#></span>");
		}
		else if(express == 2){	//Express Way 5 GHz
			document.form.sw_mode_radio[3].checked = true;
			document.form.wlc_express.value = 2;
			//untranslated string
			$("#mode_desc").html("In Express Way-5GHz mode, <#Web_Title2#> wireless connect to an existing 5 GHz WiFi channel and extend the wireless coverage by 2.4 GHz WiFi channel only. (Your router must support 5GHz WiFi channel.)<br/><span style=\"color:#FC0\"><#deviceDiscorvy2#></span>");
		}	
		else{		// Repeater
			document.form.sw_mode_radio[1].checked = true;
			document.form.wlc_express.value = 0;
			$("#mode_desc").html("<#OP_RE_desc#><br/><span style=\"color:#FC0\"><#deviceDiscorvy2#></span>");
		}
	}
	else if(mode == '3'){		// AP mode
		document.form.sw_mode.value = 3;
		document.form.wlc_express.value = 0;
		$("#Senario").css({"height": "", "background": "url(/images/New_ui/ap.jpg) center no-repeat", "margin-bottom": "30px"});
		if(findasus_support){
			$("#mode_desc").html("<#OP_AP_desc#><br/><span style=\"color:#FC0\"><#OP_AP_hint#></span>");
		}else{
			$("#mode_desc").html("<#OP_AP_desc#><br/><span style=\"color:#FC0\"><#deviceDiscorvy3#></span>");
		}
		clearTimeout(id_WANunplungHint);
		$("#Unplug-hint").css("display", "none");
		document.form.sw_mode_radio[4].checked = true;
	}
	else if(mode == '4'){		// Media Bridge
		document.form.sw_mode.value = 4;
		document.form.wlc_express.value = 0;
		var pstaDesc =  "<#OP_MB_desc1#>";
			pstaDesc += "<#OP_MB_desc2#>";
			pstaDesc += "<#OP_MB_desc3#>";
			pstaDesc += "<#OP_MB_desc4#>";
			pstaDesc += "<#OP_MB_desc5#>";
			pstaDesc += "<br><#OP_MB_desc6#>";
			pstaDesc += "<br/><span style=\"color:#FC0\"><#deviceDiscorvy4#></span>";

		$("#Senario").css({"height": "300px", "background": "url(/images/New_ui/mb.jpg) center no-repeat", "margin-bottom": "-40px"});
		$("#mode_desc").html(pstaDesc);
		clearTimeout(id_WANunplungHint);
		$("#Unplug-hint").css("display", "none");
		document.form.sw_mode_radio[5].checked = true;
	}
	else{ // Default: Router
		document.form.sw_mode.value = 1;
		document.form.wlc_express.value = 0;
		$("#Senario").css({"height": "", "background": "url(/images/New_ui/rt.jpg) center no-repeat", "margin-bottom": "30px"});
		$("#mode_desc").html("<#OP_GW_desc#>");
		document.form.sw_mode_radio[0].checked = true;
	}
}

function Sync_2ghz(band){
	if(band == 2){
		if(document.form.sync_with_2ghz.checked == true){
			document.form.wl1_wpa_psk.value = document.form.wl0_wpa_psk.value;
			document.form.wl1_ssid.value = document.form.wl0_ssid.value + "_5G";
		}
	}
	else
		document.form.sync_with_2ghz.checked = false;
}

function Sync_5ghz(band){
	if(band == 2){
		if(document.form.sync_with_5ghz.checked == true){
			document.form.wl2_wpa_psk.value = document.form.wl1_wpa_psk.value;
			document.form.wl2_ssid.value = document.form.wl1_ssid.value + "-2";
		}
	}
	else if (band == 'smartcon'){
			document.form.wl2_ssid.value = document.form.wl1_ssid.value; 
			document.form.wl2_wpa_psk.value = document.form.wl1_wpa_psk.value; 				
	}
	else
		document.form.sync_with_5ghz.checked = false;
}

function cancel_SSID_Block(){
	$("#routerSSID").fadeOut(300);
	$("#forSSID_bg").fadeOut(300);
	//document.getElementById("hiddenMask").style.visibility = "hidden";
	//document.getElementById("forSSID_bg").style.visibility = "hidden";
}

function change_smart_con(v){
	if(v == '1'){
		document.getElementById("wl_unit_field_1").style.display = "none";
		document.getElementById("wl_unit_field_2").style.display = "none";
		document.getElementById("wl_unit_field_3").style.display = "none";
		document.getElementById("wl_unit_field_4").style.display = "none";
		document.getElementById("wl_unit_field_5").style.display = "none";
		document.getElementById("wl_unit_field_6").style.display = "none";
		document.getElementById("wl0_desc_name").innerHTML = "Tri-band Smart Connect Security";
		document.getElementById("wl0_desc_name").style = "padding-bottom:10px";
		document.getElementById("wl0_ssid").onkeyup = function(){Sync_2ghz('Tri-band');};
		document.getElementById("wl0_wpa_psk").onkeyup = function(){Sync_2ghz('Tri-band');};
		document.getElementById('routerSSID').style.height="450px";
		document.form.smart_connect_x.value = 1;
	}else if (v == '0'){
		document.getElementById("wl_unit_field_1").style.display = "";
		document.getElementById("wl_unit_field_2").style.display = "";
		document.getElementById("wl_unit_field_3").style.display = "";
		document.getElementById("wl_unit_field_4").style.display = "";
		document.getElementById("wl_unit_field_5").style.display = "";
		document.getElementById("wl_unit_field_6").style.display = "";
		document.getElementById("wl0_desc_name").innerHTML = "2.4 GHz - <#Security#>";
		document.getElementById("wl0_ssid").onkeyup = function(){Sync_2ghz(2);};
		document.getElementById("wl0_wpa_psk").onkeyup = function(){Sync_2ghz(2);};
		document.getElementById('routerSSID').style.height="620px";
		document.form.smart_connect_x.value = 0;
	}
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
<input type="hidden" name="wl2_auth_mode_x" value="<% nvram_get("wl2_auth_mode_x"); %>" disabled="disabled">
<input type="hidden" name="wl2_crypto" value="<% nvram_get("wl2_crypto"); %>" disabled="disabled">
<input type="hidden" name="smart_connect_x" value="<% nvram_get("smart_connect_x"); %>">
<input type="hidden" name="w_Setting" value="1">
<!-- AC66U's repeater mode -->
<input type="hidden" name="wlc_psta" value="<% nvram_get("wlc_psta"); %>" disabled>
<input type="hidden" name="wlc_express" value="<% nvram_get("wlc_express"); %>" disabled>

<!-- Input SSID and Password block for switching Repeater to Router mode -->
<div id="routerSSID" class="contentM_qis">
	<table id="smart_connect_table" style="display:none;" class="QISSmartCon_table">
		<tr>
			<td width="200px">
			<div id="smart_connect_image" style="background: url(/images/New_ui/smart_connect.png); width: 130px; height: 87px; margin-left:115px; margin-top:20px; no-repeat;"></div>
			</td>
			<td>			
				<table style="margin-left:30px; margin-top:20px;">
						<td style="font-style:normal;font-size:13px;font-weight:bold;" >
							<input type="radio" value="1" id="smart_connect_t" name="smart_connect_t" class="input" onclick="return change_smart_con(this.value)" <% nvram_match("smart_connect_x", "1", "checked"); %>>Tri-band Smart Connect
						</td>
					</tr>
					<tr>
						<td style="font-style:normal;font-size:13px;font-weight:bold;">
							<input type="radio" value="0" name="smart_connect_t" class="input" onclick="return change_smart_con(this.value)" <% nvram_match("smart_connect_x", "0", "checked"); %>>Standard Setup
						</td>
					</tr>									
				</table>
			</td>
		</tr>
	</table>
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
			<th width="180" id="wl0_desc_name">2.4GHz - <#Security#></th>
			<td class="QISformtd"></td>
		</tr>
		<tr>
			<th width="180">
				<span onmouseout="return nd();" onclick="openHint(0, 22);" style="cursor:help;"><#QIS_finish_wireless_item1#><img align="right" style="cursor:pointer;margin-top:-14px\9;" src="/images/New_ui/helpicon.png"></span>
			</th>
			<td class="QISformtd">
				<input type="text" id="wl0_ssid" name="wl0_ssid" onkeypress="return validator.isString(this, event);" onkeyup="Sync_2ghz(2);" style="height:25px;" class="input_32_table" maxlength="32" value="" disabled="disabled" autocorrect="off" autocapitalize="off">
			</td>
		</tr>
		<tr id="wl_unit_field_0">
			<th width="180">
				<span onmouseout="return nd();" onclick="openHint(0, 23);" style="cursor:help;"><#Network_key#><img align="right" style="cursor:pointer;margin-top:-14px\9;" src="/images/New_ui/helpicon.png"></span>		
			</th>
			<td class="QISformtd">
				<input id="wl0_wpa_psk" name="wl0_wpa_psk" type="password" onBlur="switchType(this,false);" onFocus="switchType(this,true);" value="" onkeyup="Sync_2ghz(2);" style="height:25px;" class="input_32_table" maxlength="65" disabled="disabled" autocorrect="off" autocapitalize="off">
			</td>
		</tr>
		<tr id="wl_unit_field_1">
			<th id="wl_unit_field_1_1" width="180">5GHz - <#Security#> </th>
			<td class="QISformtd">
				<input type="checkbox" id="sync_with_2ghz" name="sync_with_2ghz" class="input" onclick="setTimeout('Sync_2ghz(2);',0);" checked="checked"><span id="syncCheckbox"><#qis_ssid_desc#></span>
			</td>
		</tr>
		<tr id="wl_unit_field_2">
			<th width="180">
				<span onmouseout="return nd();" onclick="openHint(0, 22);" style="cursor:help;"><#QIS_finish_wireless_item1#><img align="right" style="cursor:pointer;margin-top:-14px\9;" src="/images/New_ui/helpicon.png"></span>
			</th>
			<td class="QISformtd">
				<input type="text" id="wl1_ssid" name="wl1_ssid" onkeypress="return validator.isString(this, event);" onkeyup="Sync_2ghz(5);" style="height:25px;" class="input_32_table" maxlength="32" value="" disabled="disabled" autocorrect="off" autocapitalize="off">
			</td>
		</tr>
		<tr id="wl_unit_field_3">
			<th width="180">
				<span onmouseout="return nd();" onclick="openHint(0, 23);" style="cursor:help;"><#Network_key#><img align="right" style="cursor:pointer;margin-top:-14px\9;" src="/images/New_ui/helpicon.png"></span>
			</th>
			<td class="QISformtd">
				<input id="wl1_wpa_psk" name="wl1_wpa_psk" type="password" onBlur="switchType(this,false);" onFocus="switchType(this,true);" value="" onkeyup="Sync_2ghz(5);" style="height:25px;" class="input_32_table" maxlength="65" disabled="disabled" autocorrect="off" autocapitalize="off">
			</td>
		</tr>
		<tr id="wl_unit_field_4" style="display:none;">
			<th width="180">5 GHz-2 - <#Security#> </th>
			<td class="QISformtd" id="wl_unit_field_4_2">
				<input type="checkbox" id="sync_with_5ghz" name="sync_with_5ghz" tabindex="8" class="input" onclick="setTimeout('Sync_5ghz(2);',0);" checked="checked"><span id="syncCheckbox_5_2"><#qis_ssid_desc#></span>
			</td>
		</tr>
		<tr id="wl_unit_field_5" style="display:none;">
			<th width="180"><span onmouseout="return nd();" onclick="openHint(0, 22);" style="cursor:help;"><#QIS_finish_wireless_item1#><img align="right" style="cursor:pointer;margin-top:-14px\9;" src="/images/New_ui/helpicon.png"></span></th>
			<td class="QISformtd">
				<input type="text" id="wl2_ssid" name="wl2_ssid" tabindex="9" onkeypress="return validator.isString(this, event);" onkeyup="Sync_5ghz(5);" style="height:25px;" class="input_32_table" maxlength="32" value="" disabled="disabled" autocorrect="off" autocapitalize="off"/>
			</td>
		</tr>
		<tr id="wl_unit_field_6" style="display:none;">
			<th width="180"><span onmouseout="return nd();" onclick="openHint(0, 23);" style="cursor:help;"><#Network_key#><img align="right" style="cursor:pointer;margin-top:-14px\9;" src="/images/New_ui/helpicon.png"></span></th>
			<td class="QISformtd">
				<input id="wl2_wpa_psk" name="wl2_wpa_psk" tabindex="10" type="password" onBlur="switchType(this, false);" onFocus="switchType(this, true);" value="" disabled="disabled" onkeyup="Sync_5ghz(5);" style="height:25px;" class="input_32_table" maxlength="64" autocorrect="off" autocapitalize="off">
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
								<div class="formfonttitle"><#menu5_6#> - <#menu5_6_1_title#></div>
								<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
								<div class="formfontdesc"><#OP_desc1#></div>
							</td>
						</tr>
						<tr bgcolor="#4D595D" valign="top" style="height:15%">
							<td>
								<div style="width:95%; margin:0 auto; padding-bottom:3px;">
									<span style="font-size:16px; font-weight:bold;color:white;text-shadow:1px 1px 0px black">
										<span id="routerMode"><input type="radio" id="sw_mode1_radio" name="sw_mode_radio" class="input" value="1" onclick="setScenerion(1, 0);" <% nvram_match("sw_mode", "1", "checked"); %>><label for="sw_mode1_radio"><#OP_GW_item#></label></span>
										&nbsp;&nbsp;
										<span id="repeaterMode"><input id="sw_mode2_radio" type="radio" name="sw_mode_radio" class="input" value="2" onclick="setScenerion(2, 0);" <% nvram_match("sw_mode", "2", "checked"); %>><label for="sw_mode2_radio"><#OP_RE_item#></label></span>
										<span id="rp_express_2g" style="display:none"><input id="sw_mode2_0_radio" type="radio" name="sw_mode_radio" class="input" value="2" onclick="setScenerion(2, 1);" ><label for="sw_mode2_0_radio">Express Way 2.4 GHz</label></span><!--untranslated string-->
										<span id="rp_express_5g" style="display:none"><input id="sw_mode2_1_radio" type="radio" name="sw_mode_radio" class="input" value="2" onclick="setScenerion(2, 2);" ><label for="sw_mode2_1_radio">Express Way 5 GHz</label></span><!--untranslated string-->
										&nbsp;&nbsp;
										<input type="radio" id="sw_mode3_radio" name="sw_mode_radio" class="input" value="3" onclick="setScenerion(3, 0);" <% nvram_match("sw_mode", "3", "checked"); %>><label for="sw_mode3_radio"><#OP_AP_item#></label>
										&nbsp;&nbsp;
										<span id="mbMode"><input id="sw_mode4_radio" type="radio" name="sw_mode_radio" class="input" value="4" onclick="setScenerion(4, 0);" <% nvram_match("sw_mode", "4", "checked"); %>><label for="sw_mode4_radio">Media Bridge</label></span>
									</span>
									<br/><br/>
									<span style="word-wrap:break-word;word-break:break-all"><label id="mode_desc"></label></span>
								</div>
							</td>
						</tr>
						<tr bgcolor="#4D595D" valign="top" style="height:70%">
                 			<td>
							    <div id="Senario" >
								<div id="Unplug-hint" style="border:2px solid red; background-color:#FFF; padding:3px;margin:0px 0px 0px 150px;width:250px; position:absolute; display:block; display:none;"><#web_redirect_suggestion1#></div>
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
