<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<title><#Web_Title#> - <#YandexDNS#></title>
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

var $j = jQuery.noConflict();
var jData;
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
<% login_state_hook(); %>

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var client_ip = login_ip_str();
var client_mac = login_mac_str();

var yadns_rule_list = '<% nvram_get("yadns_rulelist"); %>'.replace(/&#60/g, "<");
var yadns_rule_list_row = yadns_rule_list.split('<');

function initial(){
	show_menu();
	show_footer();

	if(downsize_4m_support)
		$("guest_image").parentNode.style.display = "none";
	
	if(!ParentalCtrl2_support){		
		$('FormTitle').style.webkitBorderRadius = "3px";
		$('FormTitle').style.MozBorderRadius = "3px";
		$('FormTitle').style.BorderRadius = "3px";	
	}
	
	gen_mainTable();	
	showLANIPList();

	// yadns_enable_x 0: disable, 1: enable, show mainTable & Protection level when enable,	otherwise hide it
	showhide("yadns_mode",document.form.yadns_enable_x.value);	
	showhide("mainTable", document.form.yadns_enable_x.value);
}

/*------------ Mouse event of fake LAN IP select menu {-----------------*/
function setClientIP(devname, macaddr){
	document.form.rule_devname.value = devname;
	document.form.rule_mac.value = macaddr;
	hideClients_Block();
	over_var = 0;
}

function showLANIPList(){
	var htmlCode = "";
	for(var i=0; i<clientList.length;i++){
		var clientObj = clientList[clientList[i]];

		if(clientObj.ip == "offline") clientObj.ip = "";
		if(clientObj.name.length > 30) clientObj.name = clientObj.name.substring(0, 28) + "..";

		htmlCode += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\'';
		htmlCode += clientObj.name;
		htmlCode += '\', \'';
		htmlCode += clientObj.mac;
		htmlCode += '\');"><strong>';
		htmlCode += clientObj.name;
		htmlCode += clientObj.ip + '</strong>&nbsp;&nbsp;(' + clientObj.name + ')';
		htmlCode += '</strong></div></a><!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
	}

	$("ClientList_Block_PC").innerHTML = htmlCode;
}

function pullLANIPList(obj){
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		$("ClientList_Block_PC").style.display = 'block';		
		document.form.rule_devname.focus();		
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
	//valid_IP_form(document.form.rule_devname, 0);
}
/*----------} Mouse event of fake LAN IP select menu-----------------*/

function gen_modeselect(name, value, onchange){
	var code = "";
	code +='<select class="input_option" name="'+name+'" value="'+value+'" onchange="'+onchange+'">';
	code +='<option value="1"'+(value == 1 ? "selected" : "")+'><#YandexDNS_mode1#></option>';
	code +='<option value="2"'+(value == 2 ? "selected" : "")+'><#YandexDNS_mode2#></option>';
	code +='<option value="0"'+(value == 0 ? "selected" : "")+'><#YandexDNS_mode0#></option></select>';
	return code;
}

function gen_mainTable(){
	var code = "";
	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="FormTable_table" id="mainTable_table">';
	code +='<thead><tr><td colspan="4"><#ConnectedClient#>&nbsp;(<#List_limit#>&nbsp;16)</td></tr></thead>';
	code +='<tr><th width="40%"><#ParentalCtrl_username#></th>';
	code +='<th width="20%"><#ParentalCtrl_hwaddr#></th>';
	code +='<th width="15%"><#YandexDNS_mode#></th>';
	code +='<th width="10%"><#list_add_delete#></th></tr>';

	code +='<tr><td style="border-bottom:2px solid #000;"><input type="text" maxlength="32" style="margin-left:10px;float:left;width:255px;" class="input_20_table" name="rule_devname" onkeypress="return is_alphanum(this,event);" onClick="hideClients_Block();" onblur="if(!over_var){hideClients_Block();}">';
	code +='<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" onclick="pullLANIPList(this);" title="<#select_client#>" onmouseover="over_var=1;" onmouseout="over_var=0;">';
	code +='<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div></td>';
	code +='<td style="border-bottom:2px solid #000;"><input type="text" maxlength="17" class="input_macaddr_table" name="rule_mac" onKeyPress="return is_hwaddr(this,event)"></td>';
	code +='<td style="border-bottom:2px solid #000;">'+gen_modeselect("rule_mode", "-1", "")+'</td>';
	code +='<td style="border-bottom:2px solid #000;"><input class="url_btn" type="button" onClick="addRow_main(16)" value=""></td></tr>';

	if(yadns_rule_list_row == "")
		code +='<tr><td style="color:#FFCC00;" colspan="10"><#IPConnection_VSList_Norule#></td>';
	else{
		for(i=1; i<yadns_rule_list_row.length; i++){
			var ruleArray = yadns_rule_list_row[i].split('&#62');
			var rule_devname = ruleArray[0];
			var rule_mac = ruleArray[1];
			var rule_mode = ruleArray[2];

			code +='<tr id="row'+i+'">';
			code +='<td title="'+rule_devname+'">'+ rule_devname +'</td>';
			code +='<td title="'+rule_mac+'">'+ rule_mac +'</td>';
			code +='<td>'+gen_modeselect("rule_mode"+i, rule_mode, "changeRow_main(this);")+'</td>';
			code +='<td><input class=\"remove_btn\" type=\"button\" onclick=\"deleteRow_main(this);\" value=\"\"/></td>';
		}
	}

	code +='</tr></table>';

	$("mainTable").style.display = "";
	$("mainTable").innerHTML = code;
	$j("#mainTable").fadeIn();
	showLANIPList();
}

function applyRule(){
	var yadns_rulelist_tmp = "";

	for(i=1; i<yadns_rule_list_row.length; i++){
		yadns_rulelist_tmp += "<";
		yadns_rulelist_tmp += yadns_rule_list_row[i].replace(/&#62/g, ">" );
	}
	document.form.yadns_rulelist.value = yadns_rulelist_tmp;

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
	}else if(flag == 2){
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

function addRow_main(upper){
	var rule_num = $('mainTable_table').rows.length;
	var item_num = $('mainTable_table').rows[0].cells.length;	

	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}				

	if(!validate_string(document.form.rule_devname))
		return false;

	if(document.form.rule_devname.value == ""){
		alert("<#JS_fieldblank#>");
		document.form.rule_devname.focus();
		return false;
	}

	if(document.form.rule_mac.value == ""){
		alert("<#JS_fieldblank#>");
		document.form.rule_mac.focus();
		return false;
	}

	/*if(MULTIFILTER_MAC.search(document.form.rule_mac.value) > -1){
		alert("<#JS_duplicate#>");
		document.form.rule_mac.focus();
		return false;
	}*/

	if(!check_macaddr(document.form.rule_mac, check_hwaddr_flag(document.form.rule_mac))){
		document.form.rule_mac.focus();
		document.form.rule_mac.select();
		return false;	
	}

	yadns_rule_list += "<";
	yadns_rule_list += document.form.rule_devname.value + "&#62";
	yadns_rule_list += document.form.rule_mac.value + "&#62";
	yadns_rule_list += document.form.rule_mode.value;

	yadns_rule_list_row = yadns_rule_list.split('<');

	gen_mainTable();
}

function deleteRow_main(r){
	var j=r.parentNode.parentNode.rowIndex;
	$(r.parentNode.parentNode.parentNode.parentNode.id).deleteRow(j);

	var yadns_devname = "";
	var yadns_mac = "";
	var yadns_mode = "";
	var yadns_rulelist_tmp = "";

	for(i=3; i<$('mainTable_table').rows.length; i++){
		yadns_devname = $('mainTable_table').rows[i].cells[0].title;
		yadns_mac = $('mainTable_table').rows[i].cells[1].title;
		yadns_mode = $('mainTable_table').rows[i].cells[2].childNodes[0].value;

		yadns_rulelist_tmp += "<";
		yadns_rulelist_tmp += yadns_devname + "&#62";
		yadns_rulelist_tmp += yadns_mac + "&#62";
		yadns_rulelist_tmp += yadns_mode;
	}

	yadns_rule_list = yadns_rulelist_tmp;
	yadns_rule_list_row = yadns_rulelist_tmp.split('<');

	gen_mainTable();
}

function changeRow_main(r){
	var yadns_devname = "";
	var yadns_mac = "";
	var yadns_mode = "";
	var yadns_rulelist_tmp = "";

	for(i=3; i<$('mainTable_table').rows.length; i++){
		yadns_devname = $('mainTable_table').rows[i].cells[0].title;
		yadns_mac = $('mainTable_table').rows[i].cells[1].title;
		yadns_mode = $('mainTable_table').rows[i].cells[2].childNodes[0].value;

		yadns_rulelist_tmp += "<";
		yadns_rulelist_tmp += yadns_devname + "&#62";
		yadns_rulelist_tmp += yadns_mac + "&#62";
		yadns_rulelist_tmp += yadns_mode;
	}

	yadns_rule_list = yadns_rulelist_tmp;
	yadns_rule_list_row = yadns_rulelist_tmp.split('<');

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
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_yadns">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="yadns_enable_x" value="<% nvram_get("yadns_enable_x"); %>">
<input type="hidden" name="yadns_rulelist" value="">

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
		<td bgcolor="#4D595D" valign="top">
		<div>&nbsp;</div>
		<div class="formfonttitle"><#YandexDNS#></div>
		<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		<!--div class="formfontdesc"><#ParentalCtrl_Desc#></div-->

		<div id="yadns_desc" style="margin-bottom:10px;">
			<table width="700px" style="margin-left:25px;">
				<tr>
					<td>
						<img id="guest_image" src="/images/New_ui/<#YandexDNS_logo#>">
					</td>
					<td>&nbsp;&nbsp;</td>
					<td style="font-style: italic;font-size: 14px;">
						<div>
						<#YandexDNS_desc#><br>
						<#YandexDNS_mode1#>: <#YandexDNS_desc1#><br>
						<#YandexDNS_mode2#>: <#YandexDNS_desc2#>
						</div>
						<p>
						<a target="_blank" style="font-weight: bolder; cursor:pointer;text-decoration: underline;" href="http://dns.yandex.ru"><#YandexDNS#> FAQ</a>
					</td>
				</tr>
			</table>
		</div>
			<!--=====Beginning of Main Content=====-->
			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
				<tr>
					<th><#YandexDNS_enable#></th>
					<td>
						<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_yadns_enable"></div>
						<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
							<script type="text/javascript">
								$j('#radio_yadns_enable').iphoneSwitch(document.form.yadns_enable_x.value,
									function(){
										document.form.yadns_enable_x.value = 1;
										showhide("yadns_mode",1);	
										showhide("mainTable",1);	
									},
									function(){
										document.form.yadns_enable_x.value = 0;
										showhide("yadns_mode",0);	
										showhide("mainTable",0);	
									},
										{
											switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
									});
							</script>			
						</div>
					</td>			
				</tr>
				<tr id="yadns_mode">
					<th><#YandexDNS_mode#></th>
					<td>
						<select name="yadns_mode" class="input_option">
							<option value="1" <% nvram_match("yadns_mode", "1", "selected"); %>><#YandexDNS_mode1#></option>
							<option value="2" <% nvram_match("yadns_mode", "2", "selected"); %>><#YandexDNS_mode2#></option>
							<option value="0" <% nvram_match("yadns_mode", "0", "selected"); %>><#YandexDNS_mode0#></option>
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
						<!-- Content -->						
					</td>	
				</tr>
			</table>
			<div class="apply_gen">
				<input name="button" type="button" class="button_gen" onclick="applyRule()" value="<#CTL_apply#>"/>
			</div>
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
