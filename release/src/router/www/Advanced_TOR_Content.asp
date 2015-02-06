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

<title><#Web_Title#> - TOR Settings</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<style>
#pull_arrow{
 	float:center;
 	cursor:pointer;
 	border:2px outset #EFEFEF;
 	background-color:#CCC;
 	padding:3px 2px 4px 0px;
}
#TORMACList{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	margin-top:24px;
	margin-left:220px;
	*margin-left:-150px;
	width:255px;
	*width:259px;
	text-align:left;
	height:auto;
	overflow-y:auto;
	z-index:200;
	padding: 1px;
	display:none;
}
#TORMACList div{
	background-color:#576D73;
	height:20px;
	line-height:20px;
	text-decoration:none;
	padding-left:2px;
}
#TORMACList a{
	background-color:#EFEFEF;
	color:#FFF;
	font-size:12px;
	font-family:Arial, Helvetica, sans-serif;
	text-decoration:none;	
}

#TORMACList div:hover, #ClientList_Block a:hover{
	background-color:#3366FF;
	color:#FFFFFF;
	cursor:default;
}
</style>
<script>
var $j = jQuery.noConflict();

var Tor_redir_list_str = '<% nvram_get("Tor_redir_list"); %>';
var Tor_enable = '<% nvram_get("Tor_enable"); %>';

function initial(){
	show_menu();
	show_tor_settings(Tor_enable);
	show_tor_redir_list();
	setTimeout("showLANMacList();", 1000);		
}

function show_tor_redir_list(){
	var tor_redir_array = Tor_redir_list_str.split('&#60');
	var code = "";

	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table" id="tor_redir_list_table">'; 
	if(tor_redir_array.length == 1){
		code +='<tr><td style="color:#FFCC00;"><#IPConnection_VSList_Norule#></td>';
	}		
	else{
		for(var i = 1; i < tor_redir_array.length; i++){
			code +='<tr id="row'+i+'">';
			code +='<td width="80%">'+ tor_redir_array[i] +'</td>';	
			code +='<td width="20%"><input type="button" class=\"remove_btn\" onclick=\"deleteRow(this);\" value=\"\"/></td></tr>';
		}
	}
	
	code +='</tr></table>';	
	document.getElementById("tor_maclist_Block").innerHTML = code;
}

function deleteRow(r){
	var tor_redir_list_value = "";
  	var i=r.parentNode.parentNode.rowIndex;

  	document.getElementById('tor_redir_list_table').deleteRow(i);
	for(i = 0; i < document.getElementById('tor_redir_list_table').rows.length; i++){
		tor_redir_list_value += "&#60";
		tor_redir_list_value += document.getElementById('tor_redir_list_table').rows[i].cells[0].innerHTML;
	}
	
	Tor_redir_list_str = tor_redir_list_value;
	if(Tor_redir_list_str == "")
		show_tor_redir_list();
}

function addRow(obj){
	var device_num = document.getElementById('tor_redir_list_table').rows.length;

	if(obj.value == ""){
		alert("<#JS_fieldblank#>");
		obj.focus();
		obj.select();
		return false;
	}else if (!check_macaddr(obj, check_hwaddr_flag(obj))){
		obj.focus();
		obj.select();		
		return false;
	}
		
	for(i = 0; i < device_num; i++){
		if(obj.value.toLowerCase() == document.getElementById('tor_redir_list_table').rows[i].cells[0].innerHTML.toLowerCase()){
			alert("<#JS_duplicate#>");
			return false;
		}
	}
	
	Tor_redir_list_str += "&#60"		
	Tor_redir_list_str += obj.value;
	obj.value = "";
	show_tor_redir_list();
}

function applyRule(){
		var device_num = document.getElementById('tor_redir_list_table').rows.length;
		var value = "";

		for(i = 0; i < device_num; i++){
			value += "<"		
			value += document.getElementById('tor_redir_list_table').rows[i].cells[0].innerHTML;
		}

		if(value == "<"+"<#IPConnection_VSList_Norule#>" || value == "<"){
			value = "";	
		}		

		if(document.form.redirect_rule.selectedIndex == 0)
			document.form.Tor_redir_list.value = "";
		else
			document.form.Tor_redir_list.value = value;

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
	}else if(flag ==2){
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

function setClientMac(num){
	document.form.tor_maclist_0.value = num;
	hideClients_Block();
	over_var = 0;
}

function showLANMacList(){
	var htmlCode = "";
	for(var i=0; i<clientList.length;i++){
		var clientObj = clientList[clientList[i]];

		if(clientObj.mac == "offline") clientObj.mac = "";
		if(clientObj.name.length > 30) clientObj.name = clientObj.name.substring(0, 28) + "..";

		htmlCode += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientMac(\'';
		htmlCode += clientObj.mac;
		htmlCode += '\');"><strong>'+clientObj.name+'</strong>';
		htmlCode += ' ( '+clientObj.mac+' )';
		htmlCode += '</div></a><!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
	}

	$("TORMACList").innerHTML = htmlCode;
}

var over_var = 0;
var isMenuopen = 0;

function pullLANMacList(obj){	
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		document.getElementById("TORMACList").style.display = 'block';		
		document.form.tor_maclist_0.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

function hideClients_Block(){
	document.getElementById("pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById('TORMACList').style.display='none';
	isMenuopen = 0;
}

function check_macaddr(obj,flag){ //control hint of input mac address
	if(flag == 1){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac").innerHTML="<br><br><#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";
		document.getElementById("check_mac").style.display = "";
		return false;	
	}else if(flag == 2){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac").innerHTML="<br><br><#IPConnection_x_illegal_mac#>";
		document.getElementById("check_mac").style.display = "";
		return false;			
	}else{
		document.getElementById("check_mac") ? document.getElementById("check_mac").style.display="none" : true;
		return true;
	} 	
}

function showMacTable(value){
	if(value == "1"){
		document.getElementById("MainTable2").style.display = "";
		document.getElementById("tor_maclist_Block").style.display = "";
	}
	else{
		document.getElementById("MainTable2").style.display = "none";
		document.getElementById("tor_maclist_Block").style.display = "none";
	}
}

function show_tor_settings(value){
	if(value == "1"){
		document.getElementById("tor_socks_tr").style.display = "";
		document.getElementById("tor_trans_tr").style.display = "";
		document.getElementById("tor_dns_tr").style.display = "";
		document.getElementById("tor_redir_tr").style.display = "";
		if(Tor_redir_list_str != ""){
			document.form.redirect_rule.selectedIndex = 1;
			showMacTable("1");
		}
		else{
			document.form.redirect_rule.selectedIndex = 0;
			showMacTable("0");
		}		
	}
	else{
		document.getElementById("tor_socks_tr").style.display = "none";
		document.getElementById("tor_trans_tr").style.display = "none";
		document.getElementById("tor_dns_tr").style.display = "none";
		document.getElementById("tor_redir_tr").style.display = "none";
		showMacTable("0");
	}
}

</script>
</head>

<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="17">&nbsp;</td>		
		<td valign="top" width="202">	
			<div  id="mainMenu"></div>	
			<div  id="subMenu"></div>		
		</td>						
    <td valign="top">
	<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
<input type="hidden" name="current_page" value="Advanced_TOR_Content.asp">
<input type="hidden" name="next_page" value="Advanced_TOR_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="action_script" value="restart_tor">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="Tor_redir_list" value=''>

<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top" >
		
		<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
		<tbody>
			<tr>
				<td bgcolor="#4D595D" valign="top">
					<div>&nbsp;</div>
					<div class="formfonttitle">VPN - TOR Settings</div>
					<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
					<div class="formfontdesc"></div>
					<table id="MainTable1" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
						<thead>
						  <tr>
							<td colspan="2"><#t2BC#></td>
						  </tr>
						</thead>
						<tr>
							<th>TOR</th>
							<td>
								<select id="tor_enable_select" name="Tor_enable" class="input_option" onchange="show_tor_settings(this.value);">
									<option value="1" <% nvram_match("Tor_enable", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
									<option value="0" <% nvram_match("Tor_enable", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								</select>		
							</td>
					 	</tr>
						<tr id="tor_socks_tr" style="display:none;">
				  			<th>Socks Port</th>
				  			<td>
				  				<input type="text" maxlength="5" name="Tor_socksport" class="input_6_table" value="<% nvram_get(" Tor_socksport"); %>" onKeyPress="return validator.isNumber(this,event);" onBlur="return validator.numberRange(this, 0, 65535);">
				  			</td>
						</tr>					 	
						<tr id="tor_trans_tr" style="display:none;">
				  			<th>Trans Port</th>
				  			<td>
				  				<input type="text" maxlength="5" name="Tor_transport" class="input_6_table" value="<% nvram_get(" Tor_transport"); %>" onKeyPress="return validator.isNumber(this,event);" onBlur="return validator.numberRange(this, 0, 65535);">
				  			</td>
						</tr>		
						<tr id="tor_dns_tr" style="display:none;">
				  			<th>DNS Port</th>
				  			<td>
				  				<input type="text" maxlength="5" name="Tor_dnsport" class="input_6_table" value="<% nvram_get(" Tor_dnsport"); %>" onKeyPress="return validator.isNumber(this,event);" onBlur="return validator.numberRange(this, 0, 65535);">
							</td>
						</tr>
						<tr id="tor_redir_tr" style="display:none;">
				  			<th>Redirect all user from</th>
				  			<td>
								<select id="tor_enable_select" name="redirect_rule" class="input_option" onchange="showMacTable(this.value);">
									<option value="0">LAN(br0)</option>
									<option value="1">Only Specified MACs</option>
								</select>				  			
				  			</td>
						</tr>															
					</table>
					<table id="MainTable2" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable_table" style="display:none;">
						<thead>
							<tr>
								<td colspan="4">Specified MAC List</td>
							</tr>
						</thead>		
						<tr>
							<th width="80%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,10);">MAC</th>
							<th class="edit_table" width="20%"><#list_add_delete#></th>		
						</tr>
						<tr>
							<td width="80%">
								<input type="text" style="margin-left:220px;float:left;" maxlength="17" class="input_macaddr_table" name="tor_maclist_0" onKeyPress="return validator.isHWAddr(this,event)">
								<img style="float:left;" id="pull_arrow" height="14px;" src="/images/arrow-down.gif" onclick="pullLANMacList(this);" title="Select the MAC address of the device." onmouseover="over_var=1;" onmouseout="over_var=0;">
								<div id="TORMACList" class="TORMACList"></div>
								</div>
							</td>
							<td width="20%">	
								<input type="button" class="add_btn" onClick="addRow(tor_maclist_0);" value="">
							</td>
						</tr>
					</table> 
					<div id="tor_maclist_Block"></div>			
					<div id="submitBtn" class="apply_gen">
						<input class="button_gen" onclick="applyRule()" type="button" value="<#CTL_apply#>"/>
					</div>				
				</td>
			</tr>
		</tbody>
		</table>
		</td>
</form>
        </tr>
      </table>				
		<!--===================================Ending of Main Content===========================================-->		
	</td>	
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>

</body>
</html>
