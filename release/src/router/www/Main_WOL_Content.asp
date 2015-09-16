<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge">
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Network_Tools#> - <#NetworkTools_WOL#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="/device-map/device-map.css">
<style>
#ClientList_Block_PC{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	*margin-top:26px;	
	margin-left:159px;
	*margin-left:-189px;
	width:255px;
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
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script type="text/javascript" src="/jquery.xdomainajax.js"></script>
<script>
var wollist_array = '<% nvram_get("wollist"); %>';
var manually_wol_list_array = new Array();
Object.prototype.getKey = function(value) {
	for(var key in this) {
		if(this[key] == value) {
			return key;
		}
	}
	return null;
};
function initial(){
	show_menu();

	var wollist_row = wollist_array.split('&#60');
	for(var i = 1; i < wollist_row.length; i += 1) {
		var wollist_col = wollist_row[i].split('&#62');
		manually_wol_list_array[wollist_col[1]] = wollist_col[0];
	}

	showwollist();
	showLANIPList();
}

function onSubmitCtrl(o, s) {
	if(document.form.destIP.value == ""){
		alert("<#JS_fieldblank#>");
		document.form.destIP.focus();
		return false;
	}

	if(check_hwaddr_flag(document.form.destIP) != 0){
		alert("<#IPConnection_x_illegal_mac#>");
		document.form.destIP.focus();
		return false;
	}

	document.form.action_mode.value = s;
	updateOptions();
}

function updateOptions(){
	document.form.SystemCmd.value = "ether-wake -i br0 -b " + document.form.destIP.value;
	document.form.submit();
	document.getElementById("cmdBtn").disabled = true;
	document.getElementById("cmdBtn").style.color = "#666";
	document.getElementById("loadingIcon").style.display = "";
	setTimeout("checkCmdRet();", 500);
}


var _responseLen;
var noChange = 0;
function checkCmdRet(){
	$.ajax({
		url: '/cmdRet_check.htm',
		dataType: 'html',
		
		error: function(xhr){
			setTimeout("checkCmdRet();", 1000);
		},
		success: function(response){
			if(response.search("XU6J03M6") != -1){
				document.getElementById("loadingIcon").style.display = "none";
				document.getElementById("cmdBtn").disabled = false;
				document.getElementById("cmdBtn").style.color = "#FFF";
				return false;
			}

			if(_responseLen == response.length)
				noChange++;
			else
				noChange = 0;

			if(noChange > 5){
				document.getElementById("loadingIcon").style.display = "none";
				document.getElementById("cmdBtn").disabled = false;
				document.getElementById("cmdBtn").style.color = "#FFF";
				setTimeout("checkCmdRet();", 1000);
			}
			else{
				document.getElementById("cmdBtn").disabled = true;
				document.getElementById("cmdBtn").style.color = "#666";
				document.getElementById("loadingIcon").style.display = "";
				setTimeout("checkCmdRet();", 1000);
			}

			_responseLen = response.length;
		}
	});
}


function showwollist(){
	var code = "";

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="wollist_table">';
	if(Object.keys(manually_wol_list_array).length == 0)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		//user icon
		var userIconBase64 = "NoIcon";
		var clientName, deviceType, deviceVender;
		Object.keys(manually_wol_list_array).forEach(function(key) {
			clientName = manually_wol_list_array[key];
			if(clientList[key]) {
				clientName = (clientList[key].nickName == "") ? clientList[key].name : clientList[key].nickName;
				manually_wol_list_array[key] = clientName;
			}
			var clientMac = key;
			code +='<tr>';
			code +='<td width="80%" align="center">';
			if(clientList[clientMac]) {
				deviceType = clientList[clientMac].type;
				deviceVender = clientList[clientMac].dpiVender;
			}
			else {
				deviceType = 0;
				deviceVender = "";
			}
			code += '<table style="width:100%;"><tr><td style="width:40%;height:56px;border:0px;float:right;">';	
			if(clientList[clientMac] == undefined) {
				code += '<div style="height:56px;" class="clientIcon type0" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'\', \'WOL\')"></div>';
			}
			else {
				if(usericon_support) {
					userIconBase64 = getUploadIcon(clientMac.replace(/\:/g, ""));
				}
				if(userIconBase64 != "NoIcon") {
					code += '<div style="width:80px;text-align:center;" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'\', \'WOL\')"><img class="imgUserIcon_card" src="' + userIconBase64 + '"></div>';
				}
				else if( (deviceType != "0" && deviceType != "6") || deviceVender == "") {
					code += '<div style="height:56px;" class="clientIcon type' + deviceType + '" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'\', \'WOL\')"></div>';
				}
				else if(deviceVender != "" ) {
					var venderIconClassName = getVenderIconClassName(deviceVender.toLowerCase());
					if(venderIconClassName != "") {
						code += '<div style="height:56px;" class="venderIcon ' + venderIconClassName + '" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'\', \'WOL\')"></div>';
					}
					else {
						code += '<div style="height:56px;" class="clientIcon type' + deviceType + '" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'\', \'WOL\')"></div>';
					}
				}
			}
			code += '</td><td style="width:60%;border:0px;">';
			code += '<div>' + clientName + '</div>';
			code += '<div style="font-weight:bold;cursor:pointer;text-decoration:underline;font-family:Lucida Console;" onclick="document.form.destIP.value=this.innerHTML;">' + clientMac + '</div>';
			code += '</td></tr></table>';
			code +='</td>';
			code +='<td width="20%">';
			code +='<input class="remove_btn" onclick="del_Row(this, \'' + clientMac + '\');" value=""/></td></tr>';
		});
	}

  	code +='</table>';
	document.getElementById("wollist_Block").innerHTML = code;
}

function addRow_Group(upper){
	var rule_num = document.getElementById('wollist_table').rows.length;
	var item_num = document.getElementById('wollist_table').rows[0].cells.length;		
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}			

	if(document.form.wollist_macAddr.value==""){
		alert("<#JS_fieldblank#>");
		document.form.wollist_macAddr.focus();
		document.form.wollist_macAddr.select();			
		return false;
	}else if(!check_macaddr(document.form.wollist_macAddr, check_hwaddr_flag(document.form.wollist_macAddr))){
		document.form.wollist_macAddr.focus();
		document.form.wollist_macAddr.select();	
		return false;	
	}
	
	if(item_num >=2){
		for(i=0; i<rule_num; i++){	
			if(manually_wol_list_array[document.form.wollist_macAddr.value] != null){
				alert("<#JS_duplicate#>");
				document.form.wollist_macAddr.focus();
				document.form.wollist_macAddr.select();
				return false;
			}
		}
	}		
	
	var clientObj = clientList[document.form.wollist_macAddr.value.toUpperCase()];
	var clientName = "New device";
	if(clientObj) {
		clientName = (clientObj.nickName == "") ? clientObj.name : clientObj.nickName;
	}
	manually_wol_list_array[document.form.wollist_macAddr.value.toUpperCase()] = clientName;
	document.form.wollist_macAddr.value = "";
	showwollist();				
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

function del_Row(r, delMac){
	var i = r.parentNode.parentNode.rowIndex;
	delete manually_wol_list_array[delMac];
	document.getElementById('wollist_table').deleteRow(i);

	if(Object.keys(manually_wol_list_array).length == 0)
		showwollist();
}

function showLANIPList(){
	if(clientList.length == 0){
		setTimeout(function() {
			genClientList();
			showLANIPList();
		}, 500);
		return false;
	}

	var htmlCode = "";
	for(var i=0; i<clientList.length;i++){
		var clientObj = clientList[clientList[i]];
		var clientName = (clientObj.nickName == "") ? clientObj.name : clientObj.nickName;
		htmlCode += '<a title=' + clientList[i] + '><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\'';
		htmlCode += clientObj.mac;
		htmlCode += '\');"><strong>';
		htmlCode += clientName;
		htmlCode += '</strong></div></a><!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
	}

	document.getElementById("ClientList_Block_PC").innerHTML = htmlCode;
}

function setClientIP(_macaddr){
	document.form.wollist_macAddr.value = _macaddr;
	hideClients_Block();
	over_var = 0;
}

var over_var = 0;
var isMenuopen = 0;
function hideClients_Block(){
	document.getElementById("pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById('ClientList_Block_PC').style.display='none';
	isMenuopen = 0;
}

function pullLANIPList(obj){
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		document.getElementById("ClientList_Block_PC").style.display = 'block';		
		document.form.wollist_macAddr.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

function applyRule(){
	var rule_num = document.getElementById('wollist_table').rows.length;
	var item_num = document.getElementById('wollist_table').rows[0].cells.length;
	var tmp_value = "";

	Object.keys(manually_wol_list_array).forEach(function(key) {
		tmp_value += "<" + manually_wol_list_array[key] + ">"  + key;
	});

	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
		tmp_value = "";	

	document.wolform.wollist.value = tmp_value;
	showLoading();
	document.wolform.submit();
}
</script>
</head>
<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="GET" name="form" action="/apply.cgi" target="hidden_frame"> 
<input type="hidden" name="current_page" value="Main_WOL_Content.asp">
<input type="hidden" name="next_page" value="Main_WOL_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="SystemCmd" onkeydown="onSubmitCtrl(this, ' Refresh ')" value="">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="17">&nbsp;</td>
		<td valign="top" width="202">
			<div id="mainMenu"></div>
			<div id="subMenu"></div>
		</td>	
		<td valign="top">
			<div id="tabMenu" class="submenuBlock"></div>		
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td align="left" valign="top">				
						<table width="760px" border="0" cellpadding="5" cellspacing="0" bordercolor="#6b8fa3"  class="FormTitle" id="FormTitle">		
							<tr>
								<td bgcolor="#4D595D" colspan="3" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle"><#Network_Tools#> - <#NetworkTools_WOL#></div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div class="formfontdesc"><#smart_access2#></div>	
									<div class="formfontdesc"><#smart_access3#></div>	
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
										<tr>
											<th width="20%"><#NetworkTools_target#></th>
											<td>
												<input type="text" class="input_20_table" maxlength="17" name="destIP" value="" placeholder="ex: <% nvram_get("lan_hwaddr"); %>" onKeyPress="return validator.isHWAddr(this,event);" autocorrect="off" autocapitalize="off">
												<input class="button_gen" id="cmdBtn" onClick="onSubmitCtrl(this, ' Refresh ')" type="button" value="<#NetworkTools_WOL_btn#>">
												<img id="loadingIcon" style="display:none;" src="/images/InternetScan.gif"></span>
											</td>										
										</tr>
									</table>

									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;">
									  	<thead>
									  		<tr>
												<td colspan="2" id="GWStatic"><#NetworkTools_Offline#>&nbsp;(<#List_limit#>&nbsp;32)</td>
									  		</tr>
									  	</thead>
						
									  	<tr>
								  		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,10);">Client Name (MAC address)<!--untranslated--></a></th>
						        		<th><#list_add_delete#></th>
									  	</tr>			  
									  	<tr>
									  			<!-- client info -->
																					  		
				            			<td width="80%">
											<input type="text" class="input_20_table" maxlength="17" name="wollist_macAddr" onClick="hideClients_Block();" autocorrect="off" autocapitalize="off" style="margin-left:-12px;width:255px;" onKeyPress="return validator.isHWAddr(this,event)" placeholder="ex: <% nvram_get("lan_hwaddr"); %>">
											<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;*margin-left:-3px;*margin-top:1px;" onclick="pullLANIPList(this);" title="<#select_device_name#>" onmouseover="over_var=1;" onmouseout="over_var=0;">
											<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>	
				            			</td>
				            			<td width="20%">
											<div> 
												<input type="button" class="add_btn" onClick="addRow_Group(32);" value="">
											</div>
				            			</td>
									  	</tr>	 			  
									  </table>        			

								  <div id="wollist_Block"></div>

			           	<div class="apply_gen">
			           		<input type="button" name="button" class="button_gen" onclick="applyRule();" value="<#CTL_apply#>"/>
		            	</div>
								</td>
							</tr>
						</table>
					</td>
				</tr>
			</table>
			<!--===================================Ending of Main Content===========================================-->
		</td>
		<td width="10" align="center" valign="top"></td>
	</tr>
</table>
</form>

<form method="post" name="wolform" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Main_WOL_Content.asp">
<input type="hidden" name="next_page" value="Main_WOL_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="action_script" value="restart_time">
<input type="hidden" name="wollist" value="<% nvram_get("wollist"); %>">
</form>

<div id="footer"></div>
</body>
</html>
