<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<title><#Web_Title#> - Bandwidth Limiter</title><!--untranslated-->
<link rel="stylesheet" type="text/css" href="ParentalControl.css">
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="state.js"></script>
<script type="text/javascript" src="popup.js"></script>
<script type="text/javascript" src="general.js"></script>
<script type="text/javascript" src="help.js"></script>
<script type="text/javascript" src="validator.js"></script>
<script type="text/javascript" src="js/jquery.js"></script>
<script type="text/javascript" src="switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="client_function.js"></script>
<style>
#switch_menu{
	text-align:right
}
#switch_menu span{
	border-radius:4px;
	font-size:16px;
	padding:3px;
}
.click:hover{
	box-shadow:0px 0px 5px 3px white;
	background-color:#97CBFF;
}
.clicked{
	background-color:#2894FF;
	box-shadow:0px 0px 5px 3px white;

}
.click{
	background:#8E8E8E;
}
</style>
<script>
var qos_bw_rulelist = "<% nvram_get("qos_bw_rulelist"); %>".replace(/&#62/g, ">").replace(/&#60/g, "<");
var over_var = 0;
var isMenuopen = 0;
var ctf_disable = '<% nvram_get("ctf_disable"); %>';
var ctf_fa_mode = '<% nvram_get("ctf_fa_mode"); %>';

function initial(){
	show_menu();
	genMain_table();
	if(document.form.qos_enable.value == 1)
		showhide("list_table",1);
	else
		showhide("list_table",1);
	
	showLANIPList();	
}

function pullLANIPList(obj){
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		document.getElementById("ClientList_Block_PC").style.display = 'block';		
		document.form.PC_devicename.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

function hideClients_Block(){
	document.getElementById("pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById('ClientList_Block_PC').style.display='none';
	isMenuopen = 0;

}
var PC_mac = "";
function setClientIP(devname, macaddr){
	document.form.PC_devicename.value = devname;
	PC_mac = macaddr;
	hideClients_Block();
	over_var = 0;
}
	
function deleteRow_main(obj){
	var item_index = obj.parentNode.parentNode.rowIndex;
		document.getElementById(obj.parentNode.parentNode.parentNode.parentNode.id).deleteRow(item_index);

	var target_mac = obj.parentNode.parentNode.children[1].title;
	var qos_bw_rulelist_row = qos_bw_rulelist.split("<");
	var qos_bw_rulelist_temp = "";
	var priority = 0;
	for(i=0;i<qos_bw_rulelist_row.length;i++){
		var qos_bw_rulelist_col = qos_bw_rulelist_row[i].split(">");	
			if(qos_bw_rulelist_col[1] != target_mac){
				var string_temp = qos_bw_rulelist_row[i].substring(0,qos_bw_rulelist_row[i].length-1) + priority;	// reorder priority number
				priority++;	
				
				if(qos_bw_rulelist_temp == ""){
					qos_bw_rulelist_temp += string_temp;			
				}
				else{
					qos_bw_rulelist_temp += "<" + string_temp;	
				}			
			}
				
	}

	qos_bw_rulelist = qos_bw_rulelist_temp;
	genMain_table();
}

function addRow_main(obj, length){	
	var enable_checkbox = $(obj.parentNode).siblings()[0].children[0];
	var invalid_char = "";
	var qos_bw_rulelist_row =  qos_bw_rulelist.split("<");
	var max_priority = 0;
	if(qos_bw_rulelist != "")
		max_priority = qos_bw_rulelist_row.length;	
	
	if(!validator.string(document.form.PC_devicename))
		return false;
	
	if(document.form.PC_devicename.value == ""){
		alert("<#JS_fieldblank#>");
		document.form.PC_devicename.focus();
		return false;
	}
	
	if(qos_bw_rulelist.search(PC_mac) > -1 && PC_mac != ""){		//check same target
		alert("<#JS_duplicate#>");
		document.form.PC_devicename.focus();
		PC_mac = "";
		return false;
	}
	
	if(qos_bw_rulelist.search(document.form.PC_devicename.value) > -1){
		alert("<#JS_duplicate#>");
		document.form.PC_devicename.focus();
		return false;
	}
	
	if(document.getElementById("download_rate").value == ""){
		alert("<#JS_fieldblank#>");
		document.getElementById("download_rate").focus();
		return false;
	}
	
	if(document.getElementById("upload_rate").value == ""){
		alert("<#JS_fieldblank#>");
		document.getElementById("upload_rate").focus();
		return false;
	}
		
	for(var i = 0; i < document.form.PC_devicename.value.length; ++i){
		if(document.form.PC_devicename.value.charAt(i) == '<' || document.form.PC_devicename.value.charAt(i) == '>'){
			invalid_char += document.form.PC_devicename.value.charAt(i);
			document.form.PC_devicename.focus();
			alert("<#JS_validstr2#> ' "+invalid_char + " '");
			return false;			
		}
	}
	
	if(document.form.PC_devicename.value.indexOf('-') != -1
	|| document.form.PC_devicename.value.indexOf('~') != -1
	||(document.form.PC_devicename.value.indexOf(':') != -1 && document.form.PC_devicename.value.indexOf('.') != -1)){
		var space_count = 0;
		space_count = document.form.PC_devicename.value.split(" ").length - 1;
		for(i=0;i < space_count;i++){		// filter space
			document.form.PC_devicename.value = document.form.PC_devicename.value.replace(" ", "");
		}
		
		document.form.PC_devicename.value = document.form.PC_devicename.value.replace(":", "-");
		document.form.PC_devicename.value = document.form.PC_devicename.value.replace("~", "-");
	}

	if(qos_bw_rulelist == ""){
		qos_bw_rulelist += enable_checkbox.checked ? 1:0;
	}	
	else{
		qos_bw_rulelist += "<";
		qos_bw_rulelist += enable_checkbox.checked ? 1:0;
	}	

	if(PC_mac == "")
		qos_bw_rulelist += ">" + document.form.PC_devicename.value + ">";
	else
		qos_bw_rulelist += ">" + PC_mac + ">";
	
	qos_bw_rulelist += document.getElementById("download_rate").value*1024 + ">" + document.getElementById("upload_rate").value*1024;
	qos_bw_rulelist += ">" + max_priority;
	PC_mac = "";
	max_priority++;
	document.form.PC_devicename.value = "";
	genMain_table();	
}
					 
function genMain_table(){
	var qos_bw_rulelist_row = qos_bw_rulelist.split("<");
	var code = "";	
	code += '<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="FormTable_table" id="mainTable_table">';
	code += '<thead><tr>';
	code += '<td colspan="5"><#ConnectedClient#>&nbsp;(<#List_limit#>&nbsp;32)</td>';
	code += '</tr></thead>';	
	code += '<tbody>';
	code += '<tr>';
	code += '<th width="5%" height="30px" title="<#select_all#>">';
	code += '<input id="selAll" type="checkbox" onclick="enable_check(this);">';
	code += '</th>';
	code += '<th width="45%"><#NetworkTools_target#></th>';
	code += '<th width="20%"><#download_bandwidth#></th>';
	code += '<th width="20%"><#upload_bandwidth#></th>';
	code += '<th width="10%"><#list_add_delete#></th>';
	code += '</tr>';
	
	code += '<tr id="main_element">';	
	code += '<td style="border-bottom:2px solid #000;" title="<#WLANConfig11b_WirelessCtrl_button1name#>/<#btn_disable#>">';
	code += '<input type="checkbox" checked>';
	code += '</td>';
	code += '<td style="border-bottom:2px solid #000;">';
	code += '<input type="text" style="margin-left:10px;float:left;width:255px;" class="input_20_table" name="PC_devicename" onkeyup="device_filter(this);" onblur="if(!over_var){hideClients_Block();}" placeholder="<#AiProtection_client_select#>" autocorrect="off" autocapitalize="off" autocomplete="off">';
	code += '<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" onclick="pullLANIPList(this);" title="<#select_client#>" onmouseover="over_var=1;" onmouseout="over_var=0;">';
	code += '<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>';	
	code += '</td>';
	code += '<td style="border-bottom:2px solid #000;text-align:right;"><input type="text" id="download_rate" class="input_6_table" maxlength="6" onkeypress="return bandwidth_code(this, event);"><span style="margin: 0 5px;color:#FFF;">Mb/s</span></td>';
	code += '<td style="border-bottom:2px solid #000;text-align:right;"><input type="text" id="upload_rate" class="input_6_table" maxlength="6" onkeypress="return bandwidth_code(this, event);"><span style="margin: 0 5px;color:#FFF;">Mb/s</span></td>';
	code += '<td style="border-bottom:2px solid #000;"><input class="url_btn" type="button" onclick="addRow_main(this, 32)" value=""></td>';
	code += '</tr>';
	
	if(qos_bw_rulelist == ""){
		code += '<tr><td style="color:#FFCC00;" colspan="10"><#IPConnection_VSList_Norule#></td></tr>';
	}
	else{
		for(k=0;k< qos_bw_rulelist_row.length;k++){
			var qos_bw_rulelist_col = qos_bw_rulelist_row[k].split('>');
			var apps_client_name = "";

			var apps_client_mac = qos_bw_rulelist_col[1];
			var clientObj = clientList[apps_client_mac];
			if(clientObj == undefined) {
				apps_client_name = "";
			}
			else {
				apps_client_name = (clientObj.nickName == "") ? clientObj.name : clientObj.nickName;
			}

			code += '<tr>';
			code += '<td title="<#WLANConfig11b_WirelessCtrl_button1name#>/<#btn_disable#>">';
			if(qos_bw_rulelist_col[0] == 1)
				code += '<input id="'+k+'" type="checkbox" onclick="enable_check(this)" checked>';
			else	
				code += '<input id="'+k+'" type="checkbox" onclick="enable_check(this)">';
							
			code += '</td>';	

			if(apps_client_name != "")
				code += '<td title="' + apps_client_mac + '">'+ apps_client_name + '<br>(' +  apps_client_mac +')</td>';
			else
				code += '<td title="' + apps_client_mac + '">' + apps_client_mac + '</td>';
			
			code += '<td style="text-align:center;">'+qos_bw_rulelist_col[2]/1024+' Mb/s</td>';
			
			code += '<td style="text-align:center;">'+qos_bw_rulelist_col[3]/1024+' Mb/s</td>';

			code += '<td><input class="remove_btn" type="button" onclick="deleteRow_main(this);"></td>';
			code += '</tr>';
		}
	}
	
	code += '</tbody>';	
	code += '</table>';
	document.getElementById('mainTable').innerHTML = code;
	showLANIPList();
}

function bandwidth_code(o,event){
	var keyPressed = event.keyCode ? event.keyCode : event.which;
	var target = o.value.split(".");
	
	if (validator.isFunctionButton(event))
		return true;
		
	if((keyPressed == 46) && (target.length > 1))
		return false;

	if((target.length > 1) && (target[1].length > 0))
		return false;

	if ((keyPressed == 46) || (keyPressed > 47 && keyPressed < 58))
		return true;
	else
		return false;
}

function device_filter(obj){
	var target_obj = document.getElementById("ClientList_Block_PC");
	if(obj.value == ""){
		hideClients_Block();
		showLANIPList();
	}
	else{
		obj.src = "/images/arrow-top.gif"
		document.getElementById("ClientList_Block_PC").style.display = 'block';		
		document.form.PC_devicename.focus();
		var code = "";
		for(var i = 0; i < clientList.length; i += 1) {
			var clientObj = clientList[clientList[i]];
			var clientName = (clientObj.nickName == "") ? clientObj.name : clientObj.nickName;
			if(clientList[i].indexOf(obj.value) == -1 && clientName.indexOf(obj.value) == -1)
				continue;
			
			code += '<a title=' + clientList[i] + '><div style="height:auto;" onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\'' + clientName + '\', \'' + clientObj.mac + '\');"><strong>' + clientName + '</strong> ';
			code += ' </div></a>';
		}		
		
		document.getElementById("ClientList_Block_PC").innerHTML = code;		
	}
}

function showLANIPList(){
	var code = "";
	for(var i = 0; i < clientList.length; i += 1) {
		var clientObj = clientList[clientList[i]];
		var clientName = (clientObj.nickName == "") ? clientObj.name : clientObj.nickName;

		code += '<a title=' + clientList[i] + '><div style="height:auto;" onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\'' + clientName + '\', \'' + clientObj.mac + '\');"><strong>' + clientName + '</strong> ';
		code += ' </div></a>';
	}
	
	code +="<!--[if lte IE 6.5]><script>alert(\"<#ALERT_TO_CHANGE_BROWSER#>\");</script><![endif]-->";	
	document.getElementById("ClientList_Block_PC").innerHTML = code;
}

function pullLANIPList(obj){
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		document.getElementById("ClientList_Block_PC").style.display = 'block';		
		document.form.PC_devicename.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

var qos_enable_ori = "<% nvram_get("qos_enable"); %>";					
function applyRule(){
	var qos_bw_rulelist_row = "";
	if(document.form.PC_devicename.value != ""){
		alert("You must press add icon to add a new rule first.");	//untranslated
		return false;
	}
	
	document.form.qos_bw_rulelist.value = qos_bw_rulelist;
	document.form.qos_enable.value = 1;
	document.form.qos_type.value = 2;
	if(ctf_disable == 0 && ctf_fa_mode == 2){
		document.form.action_script.value = "reboot";
		document.form.action_wait.value = "<% nvram_get("reboot_time"); %>";
	}

	showLoading();	
	document.form.submit();
}

function switchPage(page){
	if(page == "1")	
		location.href = "/QoS_EZQoS.asp";
	else if(page == "2")	
		location.href = "/Bandwidth_Limiter.asp";
	else
		return false;
}

function enable_check(obj){
	var qos_bw_rulelist_row = qos_bw_rulelist.split("<");
	var rulelist_row_temp = "";
	for(i=0;i<qos_bw_rulelist_row.length;i++){
		var qos_bw_rulelist_col = qos_bw_rulelist_row[i].split(">");
		var rulelist_col_temp = "";
		for(j=0;j<qos_bw_rulelist_col.length;j++){
			if(i == obj.id && j == 0){
				qos_bw_rulelist_col[j] = obj.checked ? 1 : 0;
			}
			else if(obj.id == "selAll" && j == 0){
				qos_bw_rulelist_col[j] = obj.checked ? 1 : 0;
			}
		
			rulelist_col_temp += qos_bw_rulelist_col[j];
			if(j != qos_bw_rulelist_col.length-1)
				rulelist_col_temp += ">";
		}

		rulelist_row_temp += rulelist_col_temp;
		if(i != qos_bw_rulelist_row.length-1)
			rulelist_row_temp += "<";
			
		rulelist_col_temp = "";	
	}
	
	qos_bw_rulelist = rulelist_row_temp;
}
</script>
</head>

<body onload="initial();" onunload="unload_body();" onselectstart="return false;">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="agreement_panel" class="panel_folder" style="margin-top: -100px;display:none;position:absolute;"></div>
<div id="hiddenMask" class="popup_bg" style="z-index:999;">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center"></table>
	<!--[if lte IE 6.5]><script>alert("<#ALERT_TO_CHANGE_BROWSER#>");</script><![endif]-->
</div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Bandwidth_Limiter.asp">
<input type="hidden" name="next_page" value="Bandwidth_Limiter.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_qos">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>" disabled>
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="qos_enable" value="<% nvram_get("qos_enable"); %>">
<input type="hidden" name="qos_type" value="<% nvram_get("qos_type"); %>">
<input type="hidden" name="qos_bw_rulelist" value="">
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
						<tr>
							<td bgcolor="#4D595D" valign="top">
								<div style="margin: 5px 0">
									<table width="730px">
										<tr>
											<td align="left" class="formfonttitle">
												<div  style="width:400px"><#menu5_3_2#> - Bandwidth Limiter</div><!--untranslated-->
											</td>
											<td align="right">
												<div>
													<select onchange="switchPage(this.options[this.selectedIndex].value)" class="input_option">
														<option value="1" ><#Adaptive_QoS_Conf#></option>
														<option value="2" selected>Bandwidth Limiter</option><!--untranslated-->											
													</select>	    
												</div>
											</td>
										</tr>
									</table>
								</div>
								<div style="margin:0px 0px 10px 5px;"><img src="/images/New_ui/export/line_export.png"></div>
								<div id="PC_desc">
									<table width="700px" style="margin-left:25px;">
										<tr>
											<td>
												<img id="guest_image" src="/images/New_ui/Bandwidth_Limiter.png">
											</td>
											<td>&nbsp;&nbsp;</td>
											<td style="font-size: 14px;">
												<!--untranslated-->
												<div>Bandwidth Limiter allows you to control the max connection speed of the client device. You can select the host name from target or fill in IP address / IP Range / MAC address for limited speed profile setting.</div>
												<div><a style="text-decoration:underline;" href="http://www.asus.com/support/FAQ/1013333/" target="_blank">Bandwidth Limiter FAQ</a></div>	
											</td>
										</tr>
									</table>
								</div>
								<br>
						<!--=====Beginning of Main Content=====-->			
								<table id="list_table" width="100%" border="0" align="center" cellpadding="0" cellspacing="0" style="display:none">
									<tr>
										<td valign="top" align="center">
											<div id="mainTable" style="margin-top:10px;"></div> 
											<div id="ctrlBtn" style="text-align:center;margin-top:20px;">
												<input class="button_gen" type="button" onclick="applyRule();" value="<#CTL_apply#>">																							
											</div>
										</td>	
									</tr>
								</table>
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

<div id="footer"></div>
</form>
</body>
</html>

