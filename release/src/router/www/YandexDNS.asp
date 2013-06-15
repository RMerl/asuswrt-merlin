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

var yadns_rule_list = '<% nvram_get("yadns_rulelist"); %>'.replace(/&#62/g, ">");
var yadns_rule_list_row = yadns_rule_list.split('>');
var MULTIFILTER_LANTOWAN_ENABLE_row = "";
var MULTIFILTER_LANTOWAN_DESC_row = "";

var _client;
var StopTimeCount;

function initial(){
	show_menu();
	show_footer();

	if(downsize_4m_support)
		$("guest_image").parentNode.style.display = "none";
	
	gen_mainTable();
	showLANIPList();
	
	// yadns_enable_x 0: disable, 1: enable, show mainTable & Protection level when enable,	otherwise hide it
	showhide("protection_level",document.form.yadns_enable_x.value);	
	showhide("mainTable", document.form.yadns_enable_x.value);
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
	code +='<th width="40%"><#ParentalCtrl_username#></th>';
	code +='<th width="20%"><#ParentalCtrl_hwaddr#></th>';
	code +='<th width="15%">Protection level</th>';
	code +='<th width="10%">Add / Delete</th></tr>';

	code +='<tr><td style="border-bottom:2px solid #000;"><input type="text" maxlength="32" style="margin-left:10px;float:left;width:255px;" class="input_20_table" name="PC_devicename" onKeyPress="" onClick="hideClients_Block();" onblur="if(!over_var){hideClients_Block();}">';
	code +='<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" onclick="pullLANIPList(this);" title="Select the device name of DHCP clients." onmouseover="over_var=1;" onmouseout="over_var=0;">';
	code +='<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div></td>';
	code +='<td style="border-bottom:2px solid #000;"><input type="text" maxlength="17" class="input_macaddr_table" name="PC_mac" onKeyPress="return is_hwaddr(this,event)"></td>';
	code +='<td style="border-bottom:2px solid #000;"><select class="input_option" name="PC_protection_level"><option value="1">Secure mode</option><option value="2">Family mode</option><option value="0">Undefended</option></select></td>';
	code +='<td style="border-bottom:2px solid #000;"><input class="url_btn" type="button" onClick="addRow_main(16)" value=""></td></tr>';

	if(yadns_rule_list == "")
		code +='<tr><td style="color:#FFCC00;" colspan="10"><#IPConnection_VSList_Norule#></td>';
	else{

		var Ctrl_enable= "";
		for(var i=0; i<yadns_rule_list_row.length-1; i++){
			var PC_devicename = yadns_rule_list_row[i].split('&#60')[1];
			var PC_mac = yadns_rule_list_row[i].split('&#60')[2];
			var protection_level = "";
			if(yadns_rule_list_row[i].split('&#60')[3] == 0)
				protection_level = "Undefended";
			else if(yadns_rule_list_row[i].split('&#60')[3] == 1)	 
				protection_level = "Secure mode";
			else
				protection_level = "Family mode";
			
			code +='<tr id="row'+i+'">';
			code +='<td title="'+PC_devicename+'">'+ PC_devicename +'</td>';
			code +='<td title="'+PC_mac+'">'+ PC_mac +'</td>';
			code +='<td title="'+yadns_rule_list_row[i].split('&#60')[3]+'">'+ protection_level +'</td>';
			code +='<td><input class=\"remove_btn\" type=\"button\" onclick=\"deleteRow_main(this);\" value=\"\"/></td>';
		}
	}
	
 	code +='</tr></table>';

	$("mainTable").style.display = "";
	$("mainTable").innerHTML = code;
	$j("#mainTable").fadeIn();
	
	$("ctrlBtn").innerHTML = '<input class="button_gen" type="button" onClick="applyRule(1);" value="<#CTL_apply#>">';
}

function applyRule(_on){
	var yadns_rule_list_temp = "";
	if(document.form.yadns_enable_x.value == 0){
		inputCtrl(document.form.yadns_mode, 0);
		document.form.wan0_dnsenable_x.value = 1;
		document.form.wan0_dns1_x.value = "";		
	}
	else{
		document.form.wan0_dnsenable_x.value = 0;
		if(document.form.yadns_mode.value == 0)
			document.form.wan0_dns1_x.value = "77.88.8.7";
		else if(document.form.yadns_mode.value == 1)
			document.form.wan0_dns1_x.value = "77.88.8.88";
		else
			document.form.wan0_dns1_x.value = "77.88.8.8";
	}
	
	for(i=0; i<yadns_rule_list_row.length - 1; i++){
		yadns_rule_list_temp += yadns_rule_list_row[i].replace(/&#60/g, "<" );
		yadns_rule_list_temp += ">";	
	}

	document.form.yadns_rulelist.value = yadns_rule_list_temp;
	showLoading();	
	document.form.submit();
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

function addRow_main(upper){
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
	

	/*if(MULTIFILTER_MAC.search(document.form.PC_mac.value) > -1){
		alert("<#JS_duplicate#>");
		document.form.PC_mac.focus();
		return false;
	}*/
	
	if(!check_macaddr(document.form.PC_mac, check_hwaddr_flag(document.form.PC_mac))){
		document.form.PC_mac.focus();
		document.form.PC_mac.select();
		return false;	
	}	
	
	yadns_rule_list +="&#60" + document.form.PC_devicename.value;
	yadns_rule_list +="&#60" + document.form.PC_mac.value;
	yadns_rule_list +="&#60" + document.form.PC_protection_level.value;
	yadns_rule_list +=">" ;
	
	yadns_rule_list_row = yadns_rule_list.split('>');
	
	gen_mainTable();
}

function deleteRow_main(r){
  var j=r.parentNode.parentNode.rowIndex;

	$(r.parentNode.parentNode.parentNode.parentNode.id).deleteRow(j);

	var yadns_device_name_tmp = "";
	var yadns_mac_tmp = "";
	var yadns_protection_level_tmp = "";
	var yadns_rule_list_tmp = "";

	for(i=2; i<$('mainTable_table').rows.length; i++){
		yadns_device_name_tmp = $('mainTable_table').rows[i].cells[0].title;
		yadns_mac_tmp = $('mainTable_table').rows[i].cells[1].title;
		yadns_protection_level_tmp = $('mainTable_table').rows[i].cells[2].title;  

		yadns_rule_list_tmp += "&#60";
		yadns_rule_list_tmp += yadns_device_name_tmp + "&#60";
		yadns_rule_list_tmp += yadns_mac_tmp + "&#60";
		yadns_rule_list_tmp += yadns_protection_level_tmp;
		yadns_rule_list_tmp += ">";
		
		/*if(i != $('mainTable_table').rows.length-1){
			yadns_device_name_tmp += ">";
			yadns_mac_tmp += ">";
			yadns_protection_level_tmp += ">";
		}*/

	}

	yadns_rule_list_row = yadns_rule_list_tmp.split('>');
	/*MULTIFILTER_ENABLE = MULTIFILTER_ENABLE_tmp;
	MULTIFILTER_MAC = MULTIFILTER_MAC_tmp;
	MULTIFILTER_DEVICENAME = MULTIFILTER_DEVICENAME_tmp;
	MULTIFILTER_ENABLE_row = MULTIFILTER_ENABLE.split('>');
	MULTIFILTER_MAC_row = MULTIFILTER_MAC.split('>');
	MULTIFILTER_DEVICENAME_row = MULTIFILTER_DEVICENAME.split('>');*/

	/*MULTIFILTER_LANTOWAN_ENABLE_row.splice(j-2,1);
	MULTIFILTER_LANTOWAN_DESC_row.splice(j-2,1);
	MULTIFILTER_LANTOWAN_PORT_row.splice(j-2,1);
	MULTIFILTER_LANTOWAN_PROTO_row.splice(j-2,1);
	MULTIFILTER_MACFILTER_DAYTIME_row.splice(j-2,1);*/
	regen_lantowan();	
	gen_mainTable();
}

</script>
</head>

<body onload="initial();" onunload="unload_body();" onselectstart="return false;">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="YandexDNS.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_firewall">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="yadns_enable_x" value="<% nvram_get("yadns_enable_x"); %>">
<input type="hidden" name="yadns_rulelist" value="">
<input type="hidden" name="wan0_dnsenable_x" value="<% nvram_get("wan0_dnsenable_x"); %>">
<input type="hidden" name="wan0_dns1_x" value="<% nvram_get("wan0_dns1_x"); %>">
<input type="hidden" name="wan0_dns2_x" value="<% nvram_get("wan0_dns2_x"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0" >
	<tr>
		<td width="17">&nbsp;</td>		
		<td valign="top" width="202">				
		<div  id="mainMenu"></div>	
		<div  id="subMenu"></div>		
		</td>				
		
    <td valign="top">
	<div id="tabMenu" class="submenuBlock" style="*margin-top:-155px;">
		<table border="0" cellspacing="0" cellpadding="0">
				<tbody>
					<tr>
						<td>
							<a href="ParentalControl.asp"><div class="tab" id="tab_ParentalControl"><span>Parental Control</span></div></a>		
						</td>
						<td>
							<div class="tabclick"><span>Yandex.DNS</span></div>
						</td>
					</tr>
			</tbody>
		</table>
	</div>
		<!--===================================Beginning of Main Content===========================================-->		
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0" >
	<tr>
		<td valign="top" >
		
<table width="730px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle" style="-webkit-border-radius:3px;-moz-border-radius:3px;border-radius:3px;">
	<tbody>
	<tr>
		<td bgcolor="#4D595D" valign="top">
		<div>&nbsp;</div>
		<div class="formfonttitle">Yandex.DNS</div>
		<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		<!--div class="formfontdesc"><#ParentalCtrl_Desc#></div-->

		<div id="PC_desc" style="margin-bottom:10px;">
			<table width="700px" style="margin-left:25px;">
				<tr>
					<td>
						<img id="guest_image" src="/images/New_ui/yandex.jpg">
					</td>					
					<td style="font-style: italic;font-size: 14px;">
							<div>Yandex.DNS description</div>
							<div>
								<a id="faq" href="" target="_blank" style="text-decoration:underline;">QoS FAQ</a>							
							</div>	
					</td>
				</tr>
			</table>
		</div>


			<!--=====Beginning of Main Content=====-->
			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
				<thead>
						  <tr>
							<td colspan="2"><#t2BC#></td>
						  </tr>
						  </thead>	
				<tr>
					<th>Enable Yandex.DNS</th>
					<td>
						<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_ParentControl_enable"></div>
						<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
							<script type="text/javascript">
								$j('#radio_ParentControl_enable').iphoneSwitch(document.form.yadns_enable_x.value,
									function(){
										document.form.yadns_enable_x.value = 1;
										showhide("protection_level",1);	
										showhide("mainTable",1);	
									},
									function(){
										document.form.yadns_enable_x.value = 0;
										showhide("protection_level",0);	
										showhide("mainTable",0);	
									},
										{
											switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
									});
							</script>			
						</div>
					</td>			
				</tr>
				<tr id="protection_level">
					<th>Protection Level</th>
					<td>
						<select name="yadns_mode" class="input_option">
							<option value="1" <% nvram_match("yadns_mode", "1", "selected"); %>>Secure mode (Recommended)</option>
							<option value="2" <% nvram_match("yadns_mode", "2", "selected"); %>>Family mode</option>
							<option value="0" <% nvram_match("yadns_mode", "0", "selected"); %>>Undefended</option>					
						</select>
					</td>
				</tr>		
			</table>			
							
			<table id="list_table" width="100%" border="0" align="center" cellpadding="0" cellspacing="0" >
				<tr>
					<td valign="top" align="center">
						<!-- client info -->
						<div id="VSList_Block"></div>
						<!-- Content -->
						<div id="mainTable" style="margin-top:10px;"></div>
						<!--br/-->
						<div id="hintBlock" style="width:650px;display:none;"></div>
						<br>
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
