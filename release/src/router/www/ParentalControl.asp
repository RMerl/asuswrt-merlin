<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<title><#Web_Title#> - <#Parental_Control#></title>
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<link rel="stylesheet" type="text/css" href="ParentalControl.css">
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link rel="stylesheet" type="text/css" href="/calendar/fullcalendar.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/client_function.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
if(parental2_support != -1){
	addNewScript("/calendar/fullcalendar.js");
	addNewScript("/calendar/jquery-ui-1.8.11.custom.min.js");
}

var $j = jQuery.noConflict();
var jData;
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
<% login_state_hook(); %>

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var client_ip = login_ip_str();
var client_mac = login_mac_str();
var leases = [<% dhcp_leases(); %>];	// [[hostname, MAC, ip, lefttime], ...]
var arps = [<% get_arp_table(); %>];		// [[ip, x, x, MAC, x, type], ...]
var arls = [<% get_arl_table(); %>];		// [[MAC, port, x, x], ...]
var ipmonitor = [<% get_static_client(); %>];	// [[IP, MAC, DeviceName, Type, http, printer, iTune], ...]
var networkmap_fullscan = '<% nvram_match("networkmap_fullscan", "0", "done"); %>'; //2008.07.24 Add.  1 stands for complete, 0 stands for scanning.;
var clients_info = getclients();
var parental2_support = rc_support.search("PARENTAL2"); 

var MULTIFILTER_ENABLE = '<% nvram_get("MULTIFILTER_ENABLE"); %>'.replace(/&#62/g, ">");
var MULTIFILTER_MAC = '<% nvram_get("MULTIFILTER_MAC"); %>'.replace(/&#62/g, ">");
var MULTIFILTER_DEVICENAME = '<% nvram_get("MULTIFILTER_DEVICENAME"); %>'.replace(/&#62/g, ">");
var MULTIFILTER_MACFILTER_DAYTIME = '<% nvram_get("MULTIFILTER_MACFILTER_DAYTIME"); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<");
var MULTIFILTER_LANTOWAN_ENABLE = '<% nvram_get("MULTIFILTER_LANTOWAN_ENABLE"); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<");
var MULTIFILTER_LANTOWAN_DESC = '<% nvram_get("MULTIFILTER_LANTOWAN_DESC"); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<");
var MULTIFILTER_LANTOWAN_PORT = '<% nvram_get("MULTIFILTER_LANTOWAN_PORT"); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<");
var MULTIFILTER_LANTOWAN_PROTO = '<% nvram_get("MULTIFILTER_LANTOWAN_PROTO"); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<");

var MULTIFILTER_ENABLE_row = MULTIFILTER_ENABLE.split('>');
var MULTIFILTER_DEVICENAME_row = MULTIFILTER_DEVICENAME.split('>');
var MULTIFILTER_MAC_row = MULTIFILTER_MAC.split('>');
var MULTIFILTER_LANTOWAN_ENABLE_row = MULTIFILTER_LANTOWAN_ENABLE.split('>');
var MULTIFILTER_LANTOWAN_DESC_row = MULTIFILTER_LANTOWAN_DESC.split('>');
var MULTIFILTER_LANTOWAN_PORT_row = MULTIFILTER_LANTOWAN_PORT.split('>');
var MULTIFILTER_LANTOWAN_PROTO_row = MULTIFILTER_LANTOWAN_PROTO.split('>');
var MULTIFILTER_MACFILTER_DAYTIME_row = MULTIFILTER_MACFILTER_DAYTIME.split('>');
var _client;
var StopTimeCount;

function initial(){
	show_menu();
	show_footer();

	if(downsize_support != -1)
		$("guest_image").parentNode.style.display = "none";

	gen_mainTable();
	showLANIPList();
}

/*------------ Mouse event of fake LAN IP select menu {-----------------*/
function setClientIP(devname, macaddr){
	document.form.PC_devicename.value = devname;
	document.form.PC_mac.value = macaddr;
	hideClients_Block();
	over_var = 0;
}

function showLANIPList(){
	var code = "";
	var show_name = "";
	var client_list_array = '<% get_client_detail_info(); %>';	
	var client_list_row = client_list_array.split('<');

	for(var i = 1; i < client_list_row.length; i++){
		var client_list_col = client_list_row[i].split('>');
		if(client_list_col[1] && client_list_col[1].length > 20)
			show_name = client_list_col[1].substring(0, 16) + "..";
		else
			show_name = client_list_col[1];

		if(client_list_col[1])
			code += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\''+client_list_col[1]+'\', \''+client_list_col[3]+'\');"><strong>'+client_list_col[2]+'</strong> ';
		else
			code += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\''+client_list_col[3]+'\', \''+client_list_col[3]+'\');"><strong>'+client_list_col[2]+'</strong> ';
			if(show_name && show_name.length > 0)
				code += '( '+show_name+')';
			code += ' </div></a>';
		}
	code +='<!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
	$("ClientList_Block_PC").innerHTML = code;
}

function pullLANIPList(obj){
	
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		$("ClientList_Block_PC").style.display = 'block';		
		document.form.PC_devicename.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

var over_var = 0;
var isMenuopen = 0;

function hideClients_Block(){
	$("pull_arrow").src = "/images/arrow-down.gif";
	$('ClientList_Block_PC').style.display='none';
	isMenuopen = 0;
	//valid_IP_form(document.form.PC_devicename, 0);
}
/*----------} Mouse event of fake LAN IP select menu-----------------*/

function gen_mainTable(){
	var code = "";
	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="FormTable_table" id="mainTable_table">';
  code +='<tr><th width="5%" height="30px" title="Select all"><input id="selAll" type=\"checkbox\" onclick=\"selectAll(this, 0);\" value=\"\"/></th>';
	code +='<th width="40%"><#ParentalCtrl_username#></th>';
	code +='<th width="25%"><#ParentalCtrl_hwaddr#></th>';
	code +='<th width="10%"><#ParentalCtrl_time#></th>';
	code +='<th width="10%">Add / Delete</th></tr>';

	code +='<tr><td style="border-bottom:2px solid #000;" title="<#WLANConfig11b_WirelessCtrl_button1name#>/<#btn_disable#>"><input type=\"checkbox\" id="newrule_Enable" checked></td>';
	code +='<td style="border-bottom:2px solid #000;"><input type="text" maxlength="32" style="margin-left:10px;float:left;width:255px;" class="input_20_table" name="PC_devicename" onKeyPress="" onClick="hideClients_Block();" onblur="if(!over_var){hideClients_Block();}">';
	code +='<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" onclick="pullLANIPList(this);" title="Select the device name of DHCP clients." onmouseover="over_var=1;" onmouseout="over_var=0;"></td>';
	code +='<td style="border-bottom:2px solid #000;"><input type="text" maxlength="17" class="input_macaddr_table" name="PC_mac" onKeyPress="return is_hwaddr(this,event)"></td>';
	code +='<td style="border-bottom:2px solid #000;">--</td>';
	code +='<td style="border-bottom:2px solid #000;"><input class="url_btn" type="button" onClick="addRow_main(16)" value=""></td></tr>';

	if(MULTIFILTER_DEVICENAME == "" && MULTIFILTER_MAC == "")
		code +='<tr><td style="color:#FFCC00;" colspan="10"><#IPConnection_VSList_Norule#></td>';
	else{
		var Ctrl_enable= "";
		for(var i=0; i<MULTIFILTER_DEVICENAME_row.length; i++){
			code +='<tr id="row'+i+'">';
			code +='<td title="'+ MULTIFILTER_ENABLE_row[i] +'"><input type=\"checkbox\" onclick=\"genEnableArray_main('+i+',this);\" '+genChecked(MULTIFILTER_ENABLE_row[i])+'/></td>';
			code +='<td title="'+MULTIFILTER_DEVICENAME_row[i]+'">'+ MULTIFILTER_DEVICENAME_row[i] +'</td>';
			code +='<td title="'+MULTIFILTER_MAC_row[i]+'">'+ MULTIFILTER_MAC_row[i] +'</td>';
			code +='<td><input class=\"service_btn\" type=\"button\" onclick=\"gen_lantowanTable('+i+');" value=\"\"/></td>';
			code +='<td><input class=\"remove_btn\" type=\"button\" onclick=\"deleteRow_main(this);\" value=\"\"/></td>';
		}
	}
 	code +='</tr></table>';

	// Viz 2012.07.24$("mainTable").style.display = "none";
	$("mainTable").style.display = "";
	$("mainTable").innerHTML = code;
	$j("#mainTable").fadeIn();

	// Viz 2012.07.24$("ctrlBtn").innerHTML = '<input class="button_gen" type="button" onClick="applyRule(0);" value="<#btn_disable#>"><input class="button_gen" type="button" onClick="applyRule(1);" value="<#CTL_apply#>">';
	$("ctrlBtn").innerHTML = '<input class="button_gen" type="button" onClick="applyRule(1);" value="<#CTL_apply#>">';

	/* Viz 2012.07.24
	if(document.form.MULTIFILTER_ALL.value == 0){
		$("mainTable").style.display = "none";
		$("ctrlBtn").innerHTML = '<input class="button_gen" type="button" onClick="applyRule(1);" value="<#WLANConfig11b_WirelessCtrl_button1name#>">'
	}*/
}

function selectAll(obj, tab){
	var tag_name= document.getElementsByTagName('input');	
	var tag = 0;

	for(var i=0;i<tag_name.length;i++){
		if(tag_name[i].type == "checkbox"){
			if(tab == 1){
				tag++;
				if(tag > 7)
					tag_name[i].checked = obj.checked;
			}
			else
				tag_name[i].checked = obj.checked;
		}
	}

	if(obj.checked)
		MULTIFILTER_ENABLE = MULTIFILTER_ENABLE.replace(/0/g, "1");
	else	
		MULTIFILTER_ENABLE = MULTIFILTER_ENABLE.replace(/1/g, "0");
}

function genEnableArray_main(j, obj){
	$("selAll").checked = false;
	MULTIFILTER_ENABLE_row = MULTIFILTER_ENABLE.split('>');

	if(obj.checked){
		obj.parentNode.title = "1";
		MULTIFILTER_ENABLE_row[j] = "1";
	}
	else{
		obj.parentNode.title = "0";
		MULTIFILTER_ENABLE_row[j] = "0";
	}

	MULTIFILTER_ENABLE = "";
	for(i=0; i<MULTIFILTER_ENABLE_row.length; i++){
		MULTIFILTER_ENABLE += MULTIFILTER_ENABLE_row[i];
		if(i<MULTIFILTER_ENABLE_row.length-1)
			MULTIFILTER_ENABLE += ">";
	}	
}

function genEnableArray_lantowan(j, client, obj){
	MULTIFILTER_LANTOWAN_ENABLE_col = MULTIFILTER_LANTOWAN_ENABLE_row[client].split('<');

	if(obj.checked){
		obj.parentNode.title = "1";
		MULTIFILTER_LANTOWAN_ENABLE_col[j] = "1";
	}
	else{
		obj.parentNode.title = "0";
		MULTIFILTER_LANTOWAN_ENABLE_col[j] = "0";
	}
	
	MULTIFILTER_LANTOWAN_ENABLE_row[client] = "";
	for(i=0;i<MULTIFILTER_LANTOWAN_ENABLE_col.length;i++){
		MULTIFILTER_LANTOWAN_ENABLE_row[client] += MULTIFILTER_LANTOWAN_ENABLE_col[i];
		if(i<MULTIFILTER_LANTOWAN_ENABLE_col.length-1)
			MULTIFILTER_LANTOWAN_ENABLE_row[client] += "<";
	}	
}

function applyRule(_on){
	//Viz 2012.07.24document.form.MULTIFILTER_ALL.value = _on;
	document.form.MULTIFILTER_ENABLE.value = MULTIFILTER_ENABLE;
	document.form.MULTIFILTER_MAC.value = MULTIFILTER_MAC;
	document.form.MULTIFILTER_DEVICENAME.value = MULTIFILTER_DEVICENAME;
	document.form.MULTIFILTER_MACFILTER_DAYTIME.value = MULTIFILTER_MACFILTER_DAYTIME;
	document.form.MULTIFILTER_LANTOWAN_ENABLE.value = MULTIFILTER_LANTOWAN_ENABLE;
	document.form.MULTIFILTER_LANTOWAN_DESC.value = MULTIFILTER_LANTOWAN_DESC;
	document.form.MULTIFILTER_LANTOWAN_PORT.value = MULTIFILTER_LANTOWAN_PORT;
	document.form.MULTIFILTER_LANTOWAN_PROTO.value = MULTIFILTER_LANTOWAN_PROTO;

	showLoading();	
	document.form.submit();
}

function genChecked(_flag){
	if(_flag == 1)
		return "checked";
	else
		return "";
}

function showclock(){
	if(StopTimeCount == 1)
		return false;

	JS_timeObj.setTime(systime_millsec);
	systime_millsec += 1000;
	JS_timeObj2 = JS_timeObj.toString();	
	JS_timeObj2 = JS_timeObj2.substring(0,3) + ", " +
	              JS_timeObj2.substring(4,10) + "  " +
				  checkTime(JS_timeObj.getHours()) + ":" +
				  checkTime(JS_timeObj.getMinutes()) + ":" +
				  checkTime(JS_timeObj.getSeconds()) + "  " +
				  JS_timeObj.getFullYear();
	$("system_time").value = JS_timeObj2;
	setTimeout("showclock()", 1000);
	corrected_timezone();
}

function check_macaddr(obj,flag){ //control hint of input mac address

	if(flag == 1){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		$("check_mac").innerHTML="<#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";		
		$("check_mac").style.display = "";
		return false;
	}else if(flag ==2){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		$("check_mac").innerHTML="<#IPConnection_x_illegal_mac#>";		
		$("check_mac").style.display = "";
		return false;		
	}else{	
		$("check_mac") ? $("check_mac").style.display="none" : true;
		return true;
	}	
}

function corrected_timezone(){
	var today = new Date();
	var StrIndex;	
	if(today.toString().lastIndexOf("-") > 0)
		StrIndex = today.toString().lastIndexOf("-");
	else if(today.toString().lastIndexOf("+") > 0)
		StrIndex = today.toString().lastIndexOf("+");

	if(StrIndex > 0){
		if(timezone != today.toString().substring(StrIndex, StrIndex+5)){
			$("timezone_hint").style.display = "";
			$("timezone_hint").innerHTML = "* <#LANHostConfig_x_TimeZone_itemhint#>";
		}
		else
			return;
	}
	else
		return;	
}

function gen_lantowanTable(client){
	_client = client;
	var code = "";
	code +='<div style="margin-bottom:10px;color: #003399;font-family: Verdana;" align="left">';

	if(parental2_support != -1){
		code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="FormTable">';
		code +='<thead><tr><td colspan="6" id="LWFilterList">Active schedule</td></tr></thead>';

		code +='<tr><th width="20%"><#General_x_SystemTime_itemname#></th><td>';
		code +='<input type="text" id="system_time" name="system_time" class="devicepin" value="" readonly="1" style="font-size:12px;width:200px;"><br>';
		code +='<span id="timezone_hint" onclick="location.href=\'Advanced_System_Content.asp\'" style="display:none;text-decoration:underline;cursor:pointer;"></span></td></tr>';

		code +='<tr>';
		code +='<th style="width:40%;height:20px;" align="right">Client</th>';	
		if(MULTIFILTER_DEVICENAME_row[client] != "")
			code +='<td align="left" style="color:#FFF">'+ MULTIFILTER_DEVICENAME_row[client] + '</td></tr>';
		else
			code +='<td align="left" style="color:#FFF">'+ MULTIFILTER_MAC_row[client] + '</td></tr>';		
		code +='</table>';
		code +='</div><div id="calendar" style="margin:0;font-size:13px;margin-top:-10px;"></div>';
		code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="FormTable" style="display:none;">';
	}
	else
		code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="FormTable">';

	code +='<thead><tr><td colspan="6" id="LWFilterList">Active schedule</td></tr></thead>';
	code +='<tr>';
	code +='<th width="40%" height="30px;" align="right">Client</th>';
	if(MULTIFILTER_DEVICENAME_row[client] != "")
		code +='<td align="left" style="color:#FFF">'+ MULTIFILTER_DEVICENAME_row[client] + '</td></tr>';
	else
		code +='<td align="left" style="color:#FFF">'+ MULTIFILTER_MAC_row[client] + '</td></tr>';

	code +='<tr id="url_time">';
	code +='<th width="40%" height="30px;" align="right">Allowed access time</th>';
	code +='<td align="left" style="color:#FFF">';
	code +='<input type="text" maxlength="2" class="input_3_table" name="url_time_x_starthour" onKeyPress="return is_number(this,event)" value='+MULTIFILTER_MACFILTER_DAYTIME_row[client].substr(7,2)+'>:';
	code +='<input type="text" maxlength="2" class="input_3_table" name="url_time_x_startmin" onKeyPress="return is_number(this,event);" onblur="validate_timerange(this, 1);" value='+MULTIFILTER_MACFILTER_DAYTIME_row[client].substr(9,2)+'>-';
	code +='<input type="text" maxlength="2" class="input_3_table" name="url_time_x_endhour" onKeyPress="return is_number(this,event);" onblur="validate_timerange(this, 2);" value='+MULTIFILTER_MACFILTER_DAYTIME_row[client].substr(11,2)+'>:';
	code +='<input type="text" maxlength="2" class="input_3_table" name="url_time_x_endmin" onKeyPress="return is_number(this,event);" onblur="validate_timerange(this, 3);" value='+MULTIFILTER_MACFILTER_DAYTIME_row[client].substr(13,2)+'>';
	code +='</td></tr><tr>';
	code +='<th width="40%" height="25px;" align="right">Allowed access date</th>';
	code +='<td align="left" style="color:#FFF">';
	code +='<input type="checkbox" id="url_date_x_Sun" class="input" onChange="return changeDate();" '+genChecked(MULTIFILTER_MACFILTER_DAYTIME_row[client].substr(0,1))+'><#date_Sun_itemdesc#>';
	code +='<input type="checkbox" id="url_date_x_Mon" class="input" onChange="return changeDate();" '+genChecked(MULTIFILTER_MACFILTER_DAYTIME_row[client].substr(1,1))+'><#date_Mon_itemdesc#>';		
	code +='<input type="checkbox" id="url_date_x_Tue" class="input" onChange="return changeDate();" '+genChecked(MULTIFILTER_MACFILTER_DAYTIME_row[client].substr(2,1))+'><#date_Tue_itemdesc#>';
	code +='<input type="checkbox" id="url_date_x_Wed" class="input" onChange="return changeDate();" '+genChecked(MULTIFILTER_MACFILTER_DAYTIME_row[client].substr(3,1))+'><#date_Wed_itemdesc#>';
	code +='<input type="checkbox" id="url_date_x_Thu" class="input" onChange="return changeDate();" '+genChecked(MULTIFILTER_MACFILTER_DAYTIME_row[client].substr(4,1))+'><#date_Thu_itemdesc#>';
	code +='<input type="checkbox" id="url_date_x_Fri" class="input" onChange="return changeDate();" '+genChecked(MULTIFILTER_MACFILTER_DAYTIME_row[client].substr(5,1))+'><#date_Fri_itemdesc#>';
	code +='<input type="checkbox" id="url_date_x_Sat" class="input" onChange="return changeDate();" '+genChecked(MULTIFILTER_MACFILTER_DAYTIME_row[client].substr(6,1))+'><#date_Sat_itemdesc#>';
	code +='</td></tr></table>';

	code +='<table width="100%" style="margin-top:10px;display:none;" border="1" cellspacing="0" cellpadding="4" align="center" class="PC_table" id="lantowanTable_table">';
	code +='<thead><tr><td colspan="6" id="LWFilterList">LAN to WAN Filter Table</td></tr></thead>';
  code +='<tr><th width="5%" height="30px;"><input id="selAll" type=\"checkbox\" onclick=\"selectAll(this, 1);\"/></th>';
	code +='<th width="35%"><#BM_UserList1#></th>';
	code +='<th width="30%"><#FirewallConfig_LanWanSrcPort_itemname#></th>';
	code +='<th width="20%"><#IPConnection_VServerProto_itemname#></th>';
	code +='<th width="10%">Add / Delete</th></tr>';
	code +='<tr><td style="border-bottom:2px solid #666;"><input type=\"checkbox\" id="newrule_lantowan_Enable" checked></td>';
	code +='<td style="border-bottom:2px solid #666;"><input type="text" maxlength="32" name="lantowan_service" onKeyPress="return is_string(this, event)"></td>';
	code +='<td style="border-bottom:2px solid #666;"><input type="text" maxlength="32" name="lantowan_port" onKeyPress="return is_string(this, event)"></td>';
	code +='<td style="border-bottom:2px solid #666;"><select name="lantowan_proto" class="input"><option value="TCP">TCP</option><option value="TCP ALL">TCP ALL</option><option value="TCP SYN">TCP SYN</option><option value="TCP ACK">TCP ACK</option><option value="TCP FIN">TCP FIN</option><option value="TCP RST">TCP RST</option><option value="TCP URG">TCP URG</option><option value="TCP PSH">TCP PSH</option><option value="UDP">UDP</option></select></td>';
	code +='<td style="border-bottom:2px solid #666;"><input class="url_btn" type="button" onClick="addRow_lantowan('+client+');" value=""></td></tr>';

	if(MULTIFILTER_LANTOWAN_DESC_row[client] == "" && MULTIFILTER_LANTOWAN_PORT_row[client] == "" && MULTIFILTER_LANTOWAN_PROTO_row[client] == "")
		code +='<tr><td style="color:#FFCC00;" colspan="10"><#IPConnection_VSList_Norule#></td>';
	else{
		var MULTIFILTER_LANTOWAN_ENABLE_col = MULTIFILTER_LANTOWAN_ENABLE_row[client].split('<');
		var MULTIFILTER_LANTOWAN_DESC_col = MULTIFILTER_LANTOWAN_DESC_row[client].split('<');
		var MULTIFILTER_LANTOWAN_PORT_col = MULTIFILTER_LANTOWAN_PORT_row[client].split('<');
		var MULTIFILTER_LANTOWAN_PROTO_col = MULTIFILTER_LANTOWAN_PROTO_row[client].split('<');
		for(var i=0; i<MULTIFILTER_LANTOWAN_DESC_col.length; i++){
			code +='<tr id="row'+i+'">';
			code +='<td title="'+MULTIFILTER_LANTOWAN_ENABLE_col[i]+'"><input type=\"checkbox\" onclick=\"genEnableArray_lantowan('+i+','+client+',this);\" '+genChecked(MULTIFILTER_LANTOWAN_ENABLE_col[i])+'></td>';
			code +='<td title="'+MULTIFILTER_LANTOWAN_DESC_col[i]+'">'+ MULTIFILTER_LANTOWAN_DESC_col[i] +'</td>';
			code +='<td title="'+MULTIFILTER_LANTOWAN_PORT_col[i]+'">'+ MULTIFILTER_LANTOWAN_PORT_col[i] +'</td>';
			code +='<td title="'+MULTIFILTER_LANTOWAN_PROTO_col[i]+'">'+ MULTIFILTER_LANTOWAN_PROTO_col[i] +'</td>';
			code +='<td><input class=\"remove_btn\" type=\"button\" onclick=\"deleteRow_lantowan(this,'+client+');\" value=\"\"/></td>';
		}
	}
 	code +='</tr></table>';

	$("mainTable").innerHTML = code;
	$("ctrlBtn").innerHTML = '<input class="button_gen" type="button" onClick="cancel_lantowan('+client+');" value="<#CTL_Cancel#>">';
	$("ctrlBtn").innerHTML += '<input class="button_gen" type="button" onClick="saveto_lantowan('+client+');" value="<#CTL_ok#>">';

	// Viz 2012.07.24$("mainTable").style.display = "none";
	$("mainTable").style.display = "";
	$j("#mainTable").fadeIn();

	if(parental2_support != -1)
		generateCalendar(client);

	StopTimeCount = 0;
	showclock();
}

function regen_lantowan(){
	MULTIFILTER_LANTOWAN_ENABLE = "";
	MULTIFILTER_LANTOWAN_DESC = "";
	MULTIFILTER_LANTOWAN_PORT = "";
	MULTIFILTER_LANTOWAN_PROTO = "";
	MULTIFILTER_MACFILTER_DAYTIME = "";

	for(i=0;i<MULTIFILTER_LANTOWAN_DESC_row.length;i++){
		MULTIFILTER_LANTOWAN_ENABLE += MULTIFILTER_LANTOWAN_ENABLE_row[i];
		MULTIFILTER_LANTOWAN_DESC += MULTIFILTER_LANTOWAN_DESC_row[i];
		MULTIFILTER_LANTOWAN_PORT += MULTIFILTER_LANTOWAN_PORT_row[i];
		MULTIFILTER_LANTOWAN_PROTO += MULTIFILTER_LANTOWAN_PROTO_row[i];
		MULTIFILTER_MACFILTER_DAYTIME += MULTIFILTER_MACFILTER_DAYTIME_row[i];
		if(i<MULTIFILTER_LANTOWAN_DESC_row.length-1){
			MULTIFILTER_LANTOWAN_ENABLE += ">";
			MULTIFILTER_LANTOWAN_DESC += ">";
			MULTIFILTER_LANTOWAN_PORT += ">";
			MULTIFILTER_LANTOWAN_PROTO += ">";
			MULTIFILTER_MACFILTER_DAYTIME += ">";
		}
	}
}

function saveto_lantowan(client){
	StopTimeCount = 1;

	if(parental2_support == -1){
		var starttime = eval(document.form.url_time_x_starthour.value + document.form.url_time_x_startmin.value);
		var endtime = eval(document.form.url_time_x_endhour.value + document.form.url_time_x_endmin.value);
		if(!validate_timerange(document.form.url_time_x_starthour, 0)
			|| !validate_timerange(document.form.url_time_x_startmin, 1)
			|| !validate_timerange(document.form.url_time_x_endhour, 2)
			|| !validate_timerange(document.form.url_time_x_endmin, 3)
			)
			return false;
	
		// cross midnight start
		if(starttime > endtime || starttime == endtime){ 
			alert("The start time must be earlier than end time.");
			return false;  
		}
		// cross midnight end
	
		MULTIFILTER_MACFILTER_DAYTIME_row[client] = "";
		if($("url_date_x_Sun").checked)
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "1";
		else
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "0";
		if($("url_date_x_Mon").checked)
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "1";
		else
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "0";
		if($("url_date_x_Tue").checked)
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "1";
		else
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "0";
		if($("url_date_x_Wed").checked)
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "1";
		else
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "0";
		if($("url_date_x_Thu").checked)
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "1";
		else
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "0";
		if($("url_date_x_Fri").checked)
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "1";
		else
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "0";
		if($("url_date_x_Sat").checked)
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "1";
		else
			MULTIFILTER_MACFILTER_DAYTIME_row[client] += "0";
	
		MULTIFILTER_MACFILTER_DAYTIME_row[client] += document.form.url_time_x_starthour.value;	
		MULTIFILTER_MACFILTER_DAYTIME_row[client] += document.form.url_time_x_startmin.value;
		MULTIFILTER_MACFILTER_DAYTIME_row[client] += document.form.url_time_x_endhour.value;
		MULTIFILTER_MACFILTER_DAYTIME_row[client] += document.form.url_time_x_endmin.value;
	}

	regen_lantowan();
	gen_mainTable();
}

function cancel_lantowan(client){
	StopTimeCount = 1;

	MULTIFILTER_LANTOWAN_ENABLE_row_tmp = MULTIFILTER_LANTOWAN_ENABLE.split('>');
	MULTIFILTER_LANTOWAN_DESC_row_tmp = MULTIFILTER_LANTOWAN_DESC.split('>');
	MULTIFILTER_LANTOWAN_PORT_row_tmp = MULTIFILTER_LANTOWAN_PORT.split('>');
	MULTIFILTER_LANTOWAN_PROTO_row_tmp = MULTIFILTER_LANTOWAN_PROTO.split('>');
	MULTIFILTER_MACFILTER_DAYTIME_row_tmp = MULTIFILTER_MACFILTER_DAYTIME.split('>');

	MULTIFILTER_LANTOWAN_ENABLE_row[client] = MULTIFILTER_LANTOWAN_ENABLE_row_tmp[client];
	MULTIFILTER_LANTOWAN_DESC_row[client] = MULTIFILTER_LANTOWAN_DESC_row_tmp[client];
	MULTIFILTER_LANTOWAN_PORT_row[client] = MULTIFILTER_LANTOWAN_PORT_row_tmp[client];
	MULTIFILTER_LANTOWAN_PROTO_row[client] = MULTIFILTER_LANTOWAN_PROTO_row_tmp[client];
	MULTIFILTER_MACFILTER_DAYTIME_row[client] = MULTIFILTER_MACFILTER_DAYTIME_row_tmp[client];

	gen_mainTable();
}

function addRow_main(upper){
	if(<% nvram_get("MULTIFILTER_ALL"); %> != "1")
		document.form.MULTIFILTER_ALL.value = 1;
	
	var rule_num = $('mainTable_table').rows.length;
	var item_num = $('mainTable_table').rows[0].cells.length;	
	
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}				
	
	if(!validate_string(document.form.PC_devicename))
		return false;
	if(document.form.PC_devicename.value == ""){
		alert("<#JS_fieldblank#>");
		document.form.PC_devicename.focus();
		return false;
	}
	if(document.form.PC_mac.value == ""){
		alert("<#JS_fieldblank#>");
		document.form.PC_mac.focus();
		return false;
	}
	if(MULTIFILTER_MAC.search(document.form.PC_mac.value) > -1){
		alert("<#JS_duplicate#>");
		document.form.PC_mac.focus();
		return false;
	}
	if(!check_macaddr(document.form.PC_mac, check_hwaddr_flag(document.form.PC_mac))){
		document.form.PC_mac.focus();
		document.form.PC_mac.select();
		return false;	
	}	

	if(MULTIFILTER_DEVICENAME != "" || MULTIFILTER_MAC != ""){
		MULTIFILTER_ENABLE += ">";
		MULTIFILTER_DEVICENAME += ">";
		MULTIFILTER_MAC += ">";
	}

	if($("newrule_Enable").checked)
		MULTIFILTER_ENABLE += "1";
	else
		MULTIFILTER_ENABLE += "0";

	MULTIFILTER_DEVICENAME += document.form.PC_devicename.value;
	MULTIFILTER_MAC += document.form.PC_mac.value;

	if(MULTIFILTER_MACFILTER_DAYTIME != "")
		MULTIFILTER_MACFILTER_DAYTIME += ">";

	if(parental2_support != -1)
		MULTIFILTER_MACFILTER_DAYTIME += "<";
	else
		MULTIFILTER_MACFILTER_DAYTIME += "111111100002359";

	if(MULTIFILTER_LANTOWAN_ENABLE != "")
		MULTIFILTER_LANTOWAN_ENABLE += ">"			
	MULTIFILTER_LANTOWAN_ENABLE += "0";

	if(MULTIFILTER_LANTOWAN_DESC != "")
		MULTIFILTER_LANTOWAN_DESC += ">"			
	MULTIFILTER_LANTOWAN_DESC += " ";

	if(MULTIFILTER_LANTOWAN_PORT != "")
		MULTIFILTER_LANTOWAN_PORT += ">"			
	MULTIFILTER_LANTOWAN_PORT += " ";

	if(MULTIFILTER_LANTOWAN_PROTO != "")
		MULTIFILTER_LANTOWAN_PROTO += ">"			
	MULTIFILTER_LANTOWAN_PROTO += " ";

	MULTIFILTER_ENABLE_row = MULTIFILTER_ENABLE.split('>');
	MULTIFILTER_DEVICENAME_row = MULTIFILTER_DEVICENAME.split('>');
	MULTIFILTER_MAC_row = MULTIFILTER_MAC.split('>');
	MULTIFILTER_LANTOWAN_ENABLE_row = MULTIFILTER_LANTOWAN_ENABLE.split('>');
	MULTIFILTER_LANTOWAN_DESC_row = MULTIFILTER_LANTOWAN_DESC.split('>');
	MULTIFILTER_LANTOWAN_PORT_row = MULTIFILTER_LANTOWAN_PORT.split('>');
	MULTIFILTER_LANTOWAN_PROTO_row = MULTIFILTER_LANTOWAN_PROTO.split('>');
	MULTIFILTER_MACFILTER_DAYTIME_row = MULTIFILTER_MACFILTER_DAYTIME.split('>');
	document.form.PC_devicename.value = "";
	document.form.PC_mac.value = "";
	gen_mainTable();
}

function deleteRow_main(r){
  var j=r.parentNode.parentNode.rowIndex;
	$(r.parentNode.parentNode.parentNode.parentNode.id).deleteRow(j);

  var MULTIFILTER_ENABLE_tmp = "";
  var MULTIFILTER_MAC_tmp = "";
  var MULTIFILTER_DEVICENAME_tmp = "";
	for(i=2; i<$('mainTable_table').rows.length; i++){
		MULTIFILTER_ENABLE_tmp += $('mainTable_table').rows[i].cells[0].title;
		MULTIFILTER_DEVICENAME_tmp += $('mainTable_table').rows[i].cells[1].title;
		MULTIFILTER_MAC_tmp += $('mainTable_table').rows[i].cells[2].title;
		if(i != $('mainTable_table').rows.length-1){
			MULTIFILTER_ENABLE_tmp += ">";
			MULTIFILTER_DEVICENAME_tmp += ">";
			MULTIFILTER_MAC_tmp += ">";
		}
	}
	MULTIFILTER_ENABLE = MULTIFILTER_ENABLE_tmp;
	MULTIFILTER_MAC = MULTIFILTER_MAC_tmp;
	MULTIFILTER_DEVICENAME = MULTIFILTER_DEVICENAME_tmp;
	MULTIFILTER_ENABLE_row = MULTIFILTER_ENABLE.split('>');
	MULTIFILTER_MAC_row = MULTIFILTER_MAC.split('>');
	MULTIFILTER_DEVICENAME_row = MULTIFILTER_DEVICENAME.split('>');

	MULTIFILTER_LANTOWAN_ENABLE_row.splice(j-2,1);
	MULTIFILTER_LANTOWAN_DESC_row.splice(j-2,1);
	MULTIFILTER_LANTOWAN_PORT_row.splice(j-2,1);
	MULTIFILTER_LANTOWAN_PROTO_row.splice(j-2,1);
	MULTIFILTER_MACFILTER_DAYTIME_row.splice(j-2,1);
	regen_lantowan();	
	gen_mainTable();
}

function addRow_lantowan(client){
	if(MULTIFILTER_LANTOWAN_DESC_row[client] != "" || MULTIFILTER_LANTOWAN_PROTO_row[client] != "" || MULTIFILTER_LANTOWAN_PORT_row[client] != ""){
		MULTIFILTER_LANTOWAN_ENABLE_row[client] += "<";
		MULTIFILTER_LANTOWAN_DESC_row[client] += "<";	
		MULTIFILTER_LANTOWAN_PORT_row[client] += "<";		
		MULTIFILTER_LANTOWAN_PROTO_row[client] += "<";			
	}
	if($("newrule_lantowan_Enable").checked)
		MULTIFILTER_LANTOWAN_ENABLE_row[client] += "1";
	else
		MULTIFILTER_LANTOWAN_ENABLE_row[client] += "0";
	MULTIFILTER_LANTOWAN_DESC_row[client] += document.form.lantowan_service.value;
	MULTIFILTER_LANTOWAN_PROTO_row[client] += document.form.lantowan_proto.value;
	MULTIFILTER_LANTOWAN_PORT_row[client] += document.form.lantowan_port.value;

	document.form.lantowan_service.value = "";
	document.form.lantowan_port.value = "";
	document.form.lantowan_proto.value = "";
	gen_lantowanTable(client);
}

function deleteRow_lantowan(r, client){
  var i=r.parentNode.parentNode.rowIndex;
	$(r.parentNode.parentNode.parentNode.parentNode.id).deleteRow(i);

	var MULTIFILTER_LANTOWAN_ENABLE_tmp = "";
	var MULTIFILTER_LANTOWAN_DESC_tmp = "";
	var MULTIFILTER_LANTOWAN_PORT_tmp = "";
	var MULTIFILTER_LANTOWAN_PROTO_tmp = "";
	
	for(i=2; i<$('lantowanTable_table').rows.length; i++){
		MULTIFILTER_LANTOWAN_ENABLE_tmp += $('lantowanTable_table').rows[i].cells[0].title;
		MULTIFILTER_LANTOWAN_DESC_tmp += $('lantowanTable_table').rows[i].cells[1].title;
		MULTIFILTER_LANTOWAN_PORT_tmp += $('lantowanTable_table').rows[i].cells[2].title;
		MULTIFILTER_LANTOWAN_PROTO_tmp += $('lantowanTable_table').rows[i].cells[3].title;
		if(i != $('lantowanTable_table').rows.length-1){
			MULTIFILTER_LANTOWAN_ENABLE_tmp += "<";
			MULTIFILTER_LANTOWAN_DESC_tmp += "<";
			MULTIFILTER_LANTOWAN_PORT_tmp += "<";
			MULTIFILTER_LANTOWAN_PROTO_tmp += "<";
		}
	}
	
	MULTIFILTER_LANTOWAN_ENABLE_row[client] = MULTIFILTER_LANTOWAN_ENABLE_tmp;
	MULTIFILTER_LANTOWAN_DESC_row[client] = MULTIFILTER_LANTOWAN_DESC_tmp;
	MULTIFILTER_LANTOWAN_PORT_row[client] = MULTIFILTER_LANTOWAN_PORT_tmp;
	MULTIFILTER_LANTOWAN_PROTO_row[client] = MULTIFILTER_LANTOWAN_PROTO_tmp;

	if(MULTIFILTER_LANTOWAN_DESC_row[client] == "")
		gen_lantowanTable(client);
}
</script>
</head>

<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<!--div id="ParentalCtrlHelp" class="popup_bg" style="display:none;visibility:visible;">
<table cellpadding="5" cellspacing="0" id="loadingBlock" class="loadingBlock" align="center" style="margin:auto;margin-top:50px;">
<tbody>
	<tr>
		<td>
			<object width="640" height="360">
				<div onclick="document.body.style.overflow='auto';document.getElementById('ParentalCtrlHelp').style.display='none';">
					<span style="float:right;margin-bottom:5px;">
						<img align="right" title="Back" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
					</span>
				</div>
				<param name="movie" value="http://www.youtube.com/v/IbsuvSjG0xM&feature=player_embedded&version=3"></param><param name="allowFullScreen" value="true"></param><param name="allowScriptAccess" value="always"></param>
				<embed src="http://www.youtube.com/v/IbsuvSjG0xM&feature=player_embedded&version=3" type="application/x-shockwave-flash" allowfullscreen="true" allowScriptAccess="always" width="640" height="360"></embed>
			</object>
		</td>
	</tr>
</tbody>
</table>
</div-->

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="ParentalControl.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_firewall">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="MULTIFILTER_ALL" value="<% nvram_get("MULTIFILTER_ALL"); %>">
<input type="hidden" name="MULTIFILTER_ENABLE" value="<% nvram_get("MULTIFILTER_ENABLE"); %>">
<input type="hidden" name="MULTIFILTER_MAC" value="<% nvram_get("MULTIFILTER_MAC"); %>">
<input type="hidden" name="MULTIFILTER_DEVICENAME" value="<% nvram_get("MULTIFILTER_DEVICENAME"); %>">
<input type="hidden" name="MULTIFILTER_MACFILTER_DAYTIME" value="<% nvram_get("MULTIFILTER_MACFILTER_DAYTIME"); %>">
<input type="hidden" name="MULTIFILTER_LANTOWAN_ENABLE" value="<% nvram_get("MULTIFILTER_LANTOWAN_ENABLE"); %>">
<input type="hidden" name="MULTIFILTER_LANTOWAN_DESC" value="<% nvram_get("MULTIFILTER_LANTOWAN_DESC"); %>">
<input type="hidden" name="MULTIFILTER_LANTOWAN_PORT" value="<% nvram_get("MULTIFILTER_LANTOWAN_PORT"); %>">
<input type="hidden" name="MULTIFILTER_LANTOWAN_PROTO" value="<% nvram_get("MULTIFILTER_LANTOWAN_PROTO"); %>">
<table class="content" align="center" cellpadding="0" cellspacing="0" >
	<tr>
		<td width="17">&nbsp;</td>		
		<td valign="top" width="202">				
		<div  id="mainMenu"></div>	
		<div  id="subMenu"></div>		
		</td>				
		
    <td valign="top">
	<div id="tabMenu" class="submenuBlock" style="*margin-top:-155px;"></div>
		<!--===================================Beginning of Main Content===========================================-->		
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top" >
		
<table width="730px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle" style="-webkit-border-radius:3px;-moz-border-radius:3px;border-radius:3px;">
	<tbody>
	<tr>
		<td bgcolor="#4D595D" valign="top">
		<div>&nbsp;</div>
		<div class="formfonttitle"><#Parental_Control#></div>
		<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		<!--div class="formfontdesc"><#ParentalCtrl_Desc#></div-->

		<div id="PC_desc">
			<table width="700px" style="margin-left:25px;">
				<tr>
					<td>
						<img id="guest_image" src="/images/New_ui/parental-control.png">
						<div align="center" class="left" style="margin-top:25px;margin-left:43px;width:94px; float:left; cursor:pointer;" id="radio_ParentControl_enable"></div>
						<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
						<script type="text/javascript">
								$j('#radio_ParentControl_enable').iphoneSwitch('<% nvram_get("MULTIFILTER_ALL"); %>',
										function() {
													document.form.MULTIFILTER_ALL.value = 1;
										},
										function() {
													document.form.MULTIFILTER_ALL.value = 0;
										},
										{
													switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
										});
						</script>			
						</div>
					</td>
					<td>&nbsp;&nbsp;</td>
					<td style="font-style: italic;font-size: 14px;">
						<span><#ParentalCtrl_Desc#></span>
						<ol>	
							<li><#ParentalCtrl_Desc1#></li>
							<li><#ParentalCtrl_Desc2#></li>
							<li><#ParentalCtrl_Desc3#></li>
							<li><#ParentalCtrl_Desc4#></li>
							<li>
								<a target="_blank" style="font-weight: bolder; cursor:pointer;text-decoration: underline;" href="http://www.youtube.com/v/IbsuvSjG0xM">Click to open tutorial video.</a>
								<!--span onclick="location.href='#';document.body.style.overflow='hidden';document.getElementById('ParentalCtrlHelp').style.display='';">Click to open tutorial video.</span-->
							</li>
						</ol>		
					</td>
				</tr>
			</table>
		</div>


			<!--=====Beginning of Main Content=====-->
			<table width="95%" border="0" align="center" cellpadding="0" cellspacing="0" style="margin-left:30px;">
				<tr>
					<td valign="top" align="center">
						<!-- client info -->
						<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>
						<div id="VSList_Block"></div>
						<!-- Content -->
						<div id="mainTable"></div>
						<br/>
						<div id="ctrlBtn"></div>
						<!-- Content -->						
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
		<!--===================================Ending of Main Content===========================================-->		
	</td>
		
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</form>
<script>

</script>
</body>
</html>
