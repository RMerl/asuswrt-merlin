<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<title><#Web_Title#> - DNS-based Filtering</title>
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
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/client_function.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>

<% login_state_hook(); %>

var dnsfilter_rule_list = '<% nvram_get("dnsfilter_rulelist"); %>'.replace(/&#60/g, "<");
var dnsfilter_rule_list_row = dnsfilter_rule_list.split('<');


function initial(){
	show_menu();
	show_footer();

	if(!ParentalCtrl2_support){		
		document.getElementById('FormTitle').style.webkitBorderRadius = "3px";
		document.getElementById('FormTitle').style.MozBorderRadius = "3px";
		document.getElementById('FormTitle').style.BorderRadius = "3px";	
	}
	
	gen_mainTable();	
	setTimeout("showLANIPList();", 1000);

	// dnsfilter_enable_x 0: disable, 1: enable, show mainTable & Protection level when enable, otherwise hide it
	showhide("dnsfilter_mode",document.form.dnsfilter_enable_x.value);
	showhide("dnsfilter_custom1", document.form.dnsfilter_enable_x.value);
	showhide("dnsfilter_custom2", document.form.dnsfilter_enable_x.value);
	showhide("dnsfilter_custom3", document.form.dnsfilter_enable_x.value);
	showhide("mainTable", document.form.dnsfilter_enable_x.value);
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
		htmlCode += clientObj.ip + '</strong>&nbsp;&nbsp;(' + clientObj.name + ')';
		htmlCode += '</strong></div></a><!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';
	}

	document.getElementById("ClientList_Block_PC").innerHTML = htmlCode;
}

function pullLANIPList(obj){
	if(isMenuopen == 0){
		obj.src = "/images/arrow-top.gif"
		document.getElementById("ClientList_Block_PC").style.display = 'block';
		document.form.rule_devname.focus();
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
	//valid_IP_form(document.form.rule_devname, 0);
}

function gen_modeselect(name, value, onchange){
	var code = "";
	code +='<select class="input_option" name="'+name+'" value="'+value+'" onchange="'+onchange+'">';
	code +='<option value="0"'+(value == 0 ? "selected" : "")+'>No Filtering</option>';
	code +='<option value="11"'+(value == 11 ? "selected" : "")+'>Router</option>';
	code +='<option value="1"'+(value == 1 ? "selected" : "")+'>OpenDNS Home</option>';
	code +='<option value="7"'+(value == 7 ? "selected" : "")+'>OpenDNS Family</option>';
	code +='<option value="2"'+(value == 2 ? "selected" : "")+'>Norton Safe</option>';
	code +='<option value="3"'+(value == 3 ? "selected" : "")+'>Norton Family</option>';
	code +='<option value="4"'+(value == 4 ? "selected" : "")+'>Norton Children</option>';
	code +='<option value="5"'+(value == 5 ? "selected" : "")+'>Yandex Safe</option>';
	code +='<option value="6"'+(value == 6 ? "selected" : "")+'>Yandex Family</option>';
	code +='<option value="12"'+(value == 12? "selected" : "")+'>Comodo Secure DNS</option>';
	code +='<option value="8"'+(value == 8 ? "selected" : "")+'>Custom 1</option>';
	code +='<option value="9"'+(value == 9 ? "selected" : "")+'>Custom 2</option>';
	code +='<option value="10"'+(value == 10 ? "selected" : "")+'>Custom 3</option>';
	code +='</select>';
	return code;
}

function gen_mainTable(){
	var code = "";

	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="FormTable_table" id="mainTable_table">';
	code +='<thead><tr><td colspan="4"><#ConnectedClient#>&nbsp;(<#List_limit#>&nbsp;64)</td></tr></thead>';
	code +='<tr><th width="40%"><#ParentalCtrl_username#></th>';
	code +='<th width="20%"><#ParentalCtrl_hwaddr#></th>';
	code +='<th width="15%">Filter Mode</th>';
	code +='<th width="10%"><#list_add_delete#></th></tr>';

	code +='<tr><td style="border-bottom:2px solid #000;"><input type="text" maxlength="32" style="margin-left:10px;float:left;width:255px;" class="input_20_table" name="rule_devname" onkeypress="return is_alphanum(this,event);" onClick="hideClients_Block();" onblur="if(!over_var){hideClients_Block();};validator.safeName(this);">';
	code +='<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" onclick="pullLANIPList(this);" title="<#select_client#>" onmouseover="over_var=1;" onmouseout="over_var=0;">';
	code +='<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div></td>';
	code +='<td style="border-bottom:2px solid #000;"><input type="text" maxlength="17" class="input_macaddr_table" name="rule_mac" onKeyPress="return validator.isHWAddr(this,event)"></td>';
	code +='<td style="border-bottom:2px solid #000;">'+gen_modeselect("rule_mode", "-1", "")+'</td>';
	code +='<td style="border-bottom:2px solid #000;"><input class="url_btn" type="button" onClick="addRow_main(64)" value=""></td></tr>';

	if(dnsfilter_rule_list_row == "")
		code +='<tr><td style="color:#FFCC00;" colspan="10"><#IPConnection_VSList_Norule#></td>';
	else{
		for(i=1; i<dnsfilter_rule_list_row.length; i++){
			var ruleArray = dnsfilter_rule_list_row[i].split('&#62');
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

	document.getElementById("mainTable").style.display = "";
	document.getElementById("mainTable").innerHTML = code;
	$("#mainTable").fadeIn();
	showLANIPList();
}

function applyRule(){
	var dnsfilter_rulelist_tmp = "";

	for(i=1; i<dnsfilter_rule_list_row.length; i++){
		dnsfilter_rulelist_tmp += "<";
		dnsfilter_rulelist_tmp += dnsfilter_rule_list_row[i].replace(/&#62/g, ">" );
	}
	document.form.dnsfilter_rulelist.value = dnsfilter_rulelist_tmp;

	showLoading();
	document.form.submit();
}

function check_macaddr(obj,flag){ //control hint of input mac address
	if(flag == 1){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac").innerHTML="<#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";
		document.getElementById("check_mac").style.display = "";
		return false;
	}else if(flag == 2){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac").innerHTML="<#IPConnection_x_illegal_mac#>";
		document.getElementById("check_mac").style.display = "";
		return false;
	}else{
		document.getElementById("check_mac") ? document.getElementById("check_mac").style.display="none" : true;
		return true;
	}
}

function addRow_main(upper){
	var rule_num = document.getElementById('mainTable_table').rows.length;
	var item_num = document.getElementById('mainTable_table').rows[0].cells.length;

	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;
	}

	if(!validator.string(document.form.rule_devname))
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

	if(!check_macaddr(document.form.rule_mac, check_hwaddr_flag(document.form.rule_mac))){
		document.form.rule_mac.focus();
		document.form.rule_mac.select();
		return false;
	}

	dnsfilter_rule_list += "<";
	dnsfilter_rule_list += document.form.rule_devname.value + "&#62";
	dnsfilter_rule_list += document.form.rule_mac.value + "&#62";
	dnsfilter_rule_list += document.form.rule_mode.value;

	dnsfilter_rule_list_row = dnsfilter_rule_list.split('<');

	gen_mainTable();
}

function deleteRow_main(r){
	var j=r.parentNode.parentNode.rowIndex;
	document.getElementById(r.parentNode.parentNode.parentNode.parentNode.id).deleteRow(j);

	var dnsfilter_devname = "";
	var dnsfilter_mac = "";
	var dnsfilter_mode = "";
	var dnsfilter_rulelist_tmp = "";

	for(i=3; i<document.getElementById('mainTable_table').rows.length; i++){
		dnsfilter_devname = document.getElementById('mainTable_table').rows[i].cells[0].title;
		dnsfilter_mac = document.getElementById('mainTable_table').rows[i].cells[1].title;
		dnsfilter_mode = document.getElementById('mainTable_table').rows[i].cells[2].childNodes[0].value;

		dnsfilter_rulelist_tmp += "<";
		dnsfilter_rulelist_tmp += dnsfilter_devname + "&#62";
		dnsfilter_rulelist_tmp += dnsfilter_mac + "&#62";
		dnsfilter_rulelist_tmp += dnsfilter_mode;
	}

	dnsfilter_rule_list = dnsfilter_rulelist_tmp;
	dnsfilter_rule_list_row = dnsfilter_rulelist_tmp.split('<');

	gen_mainTable();
}

function changeRow_main(r){
	var dnsfilter_devname = "";
	var dnsfilter_mac = "";
	var dnsfilter_mode = "";
	var dnsfilter_rulelist_tmp = "";

	for(i=3; i<document.getElementById('mainTable_table').rows.length; i++){
		dnsfilter_devname = document.getElementById('mainTable_table').rows[i].cells[0].title;
		dnsfilter_mac = document.getElementById('mainTable_table').rows[i].cells[1].title;
		dnsfilter_mode = document.getElementById('mainTable_table').rows[i].cells[2].childNodes[0].value;

		dnsfilter_rulelist_tmp += "<";
		dnsfilter_rulelist_tmp += dnsfilter_devname + "&#62";
		dnsfilter_rulelist_tmp += dnsfilter_mac + "&#62";
		dnsfilter_rulelist_tmp += dnsfilter_mode;
	}

	dnsfilter_rule_list = dnsfilter_rulelist_tmp;
	dnsfilter_rule_list_row = dnsfilter_rulelist_tmp.split('<');

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
<input type="hidden" name="current_page" value="DNSFilter.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_dnsfilter">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="dnsfilter_enable_x" value="<% nvram_get("dnsfilter_enable_x"); %>">
<input type="hidden" name="dnsfilter_rulelist" value="">

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
		<div class="formfonttitle">DNS-based Filtering</div>
		<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

		<div id="dnsfilter_desc" style="margin-bottom:10px;">
			<table width="700px" style="margin-left:25px;">
				<tr>
					<td>&nbsp;&nbsp;</td>
					<td style="font-style: italic;font-size: 14px;">
						<div>
							<p>DNS-based filtering lets you protect specific LAN devices
							against harmful online content.  The following filtering services
							are currently supported (some of which offer multiple levels of
							protection):
							<ul>
								<li><a target="_blank" style="font-weight: bolder; cursor:pointer;text-decoration: underline;" href="http://www.opendns.com/home-internet-security/">OpenDNS</a>
								<ul><li>Home = Regular OpenDNS server (manually configurable through their portal)
								<li>Family = Family Shield (pre-configured to block adult content)</ul>
								<li><a target="_blank" style="font-weight: bolder; cursor:pointer;text-decoration: underline;" href="https://dns.norton.com/">Norton Connect Safe</a> (for home usage only)
								<ul><li>Safe = Malicious content<li>Family = Malicious + Sexual content<li>Children = Malicious + Sexual + Mature content</ul>
								<li><a target="_blank" style="font-weight: bolder; cursor:pointer;text-decoration: underline;" href="http://dns.yandex.com"><#YandexDNS#></a>
								<ul><li>Safe = Malicious content<li>Family = Malicious + Sexual content</ul>
								<li><a target="_blank" style="font-weight: bolder; cursor:pointer;text-decoration: underline;" href="http://www.comodo.com/secure-dns/">Comodo Secure DNS</a>
								<ul><li>Protects against malicious content</ul>
							</ul>
							<br>"No Filtering" will disable/bypass the filter, and "Router" will force clients to use the DNS provided
							    by the router's DHCP server (or, the router itself if it's not defined).
						</div>
					</td>
				</tr>
			</table>
		</div>
			<!--=====Beginning of Main Content=====-->
			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
				<tr>
					<th>Enable DNS-based Filtering</th>
					<td>
						<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_dnsfilter_enable"></div>
						<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
							<script type="text/javascript">
								$('#radio_dnsfilter_enable').iphoneSwitch(document.form.dnsfilter_enable_x.value,
									function(){
										document.form.dnsfilter_enable_x.value = 1;
										showhide("dnsfilter_mode",1);
										showhide("dnsfilter_custom1",1);
										showhide("dnsfilter_custom2",1);
										showhide("dnsfilter_custom3",1);
										showhide("mainTable",1);
									},
									function(){
										document.form.dnsfilter_enable_x.value = 0;
										showhide("dnsfilter_mode",0);
										showhide("dnsfilter_custom1",0);
										showhide("dnsfilter_custom2",0);
										showhide("dnsfilter_custom3",0);
										showhide("mainTable",0);
									},
										{
											switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
									});
							</script>
						</div>
					</td>
				</tr>
				<tr id="dnsfilter_mode">
					<th>Global Filter Mode</th>
					<td>
						<select name="dnsfilter_mode" class="input_option">
							<option value="0" <% nvram_match("dnsfilter_mode", "0", "selected"); %>>No filtering</option>
							<option value="11" <% nvram_match("dnsfilter_mode", "11", "selected"); %>>Router</option>
							<option value="1" <% nvram_match("dnsfilter_mode", "1", "selected"); %>>OpenDNS Home</option>
							<option value="7" <% nvram_match("dnsfilter_mode", "7", "selected"); %>>OpenDNS Family</option>
							<option value="2" <% nvram_match("dnsfilter_mode", "2", "selected"); %>>Norton Safe</option>
							<option value="3" <% nvram_match("dnsfilter_mode", "3", "selected"); %>>Norton Family</option>
							<option value="4" <% nvram_match("dnsfilter_mode", "4", "selected"); %>>Norton Children</option>
							<option value="5" <% nvram_match("dnsfilter_mode", "5", "selected"); %>>Yandex Safe</option>
							<option value="6" <% nvram_match("dnsfilter_mode", "6", "selected"); %>>Yandex Family</option>
							<option value="12" <% nvram_match("dnsfilter_mode", "12", "selected"); %>>Comodo Secure DNS</option>
							<option value="8" <% nvram_match("dnsfilter_mode", "8", "selected"); %>>Custom 1</option>
							<option value="9" <% nvram_match("dnsfilter_mode", "9", "selected"); %>>Custom 2</option>
							<option value="10" <% nvram_match("dnsfilter_mode", "10", "selected"); %>>Custom 3</option>
						</select>
					</td>
				</tr>
				<tr id="dnsfilter_custom1">
					<th width="200">Custom (user-defined) DNS 1</th>
					<td>
						<input type="text" maxlength="15" class="input_15_table" name="dnsfilter_custom1" value="<% nvram_get("dnsfilter_custom1"); %>" onKeyPress="return validator.isIPAddr(this,event)">
					</td>
				</tr>
                                <tr id="dnsfilter_custom2">
                                        <th width="200">Custom (user-defined) DNS 2</th>
                                        <td>
                                                <input type="text" maxlength="15" class="input_15_table" name="dnsfilter_custom2" value="<% nvram_get("dnsfilter_custom2"); %>" onKeyPress="return validator.isIPAddr(this,event)">
                                        </td>
                                </tr>
                                <tr id="dnsfilter_custom3">
                                        <th width="200">Custom (user-defined) DNS 3</th>
                                        <td>
                                                <input type="text" maxlength="15" class="input_15_table" name="dnsfilter_custom3" value="<% nvram_get("dnsfilter_custom3"); %>" onKeyPress="return validator.isIPAddr(this,event)">
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
