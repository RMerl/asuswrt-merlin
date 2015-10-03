
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
<title><#Web_Title#> - WTFast</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="device-map/device-map.css">
<script language="JavaScript" type="text/javascript" src="state.js"></script>
<script language="JavaScript" type="text/javascript" src="general.js"></script>
<script language="JavaScript" type="text/javascript" src="help.js"></script>
<script language="JavaScript" type="text/javascript" src="popup.js"></script>
<script language="JavaScript" type="text/javascript" src="client_function.js"></script>
<script language="JavaScript" type="text/javascript" src="validator.js"></script>
<script type="text/javascript" src="js/jquery.js"></script>
<script type="text/javascript" src="switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="js/fire.js"></script>
<style>
#ClientList_Block_PC{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	*margin-top:22px;	
	margin-left:22px;
	*margin-left:-263px;
	width:220px;
	*width:220px;
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
}

.LogoCircle{
	position:relative;
	float:left;
    border-radius: 50%;
    behavior: url(PIE.htc);
    width: 250px;
    height: 250px;
    background: #000000;
    border: 1px solid #E30101;
    box-shadow: -1px 0 10px #EBA338;
    text-align: center;
    margin-top: 26px;
	margin-left: 255px;
    margin-right: 255px;
}

.Background{
	background:url(images/wtfast_bg.jpg);
	background-repeat: repeat-x;
	float:left;
	position:absolute;
	top:116px;
	z-index:1;
	border-radius: 3px;
}

#see_all{
	color:#6D624C;
	margin-left:20px;
	text-decoration:underline;
}

#see_all:hover{
	color:#FFFFFF;
}

.login_input{
	height: 20px; 
	background-color:#000; 
	color:#FFF; 
	width:220px; 
	border: solid 1px #FFF;
}
</style>
<script>

var rule_default_state = "1";
var cur_rule_enable = rule_default_state;
var rule_enable_array = [];
var MAX_RULE_NUM = 16;
var enable_num = 0;
var wtfast_rulelist = decodeURIComponent('<% nvram_char_to_ascii("", "wtf_rulelist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<");
var wtfast_rulelist_array = new Array();
var wtfast_login = cookie.get("wtfast_login");
var saved_username = decodeURIComponent('<% nvram_char_to_ascii("", "wtf_username"); %>');
var saved_passwd = decodeURIComponent('<% nvram_char_to_ascii("", "wtf_passwd"); %>');


//	[ToDo] This JSON should be stored in the router or get form wtfd.
	var wtfast_status = [<% wtfast_status();%>][0];
/*
	var wtfast_status = [{ 
		"eMail": "", 
		"Account_Type": "",
		"Max_Computers": 0,
		"Server_List": [],
		"Days_Left": 0,
		"Login_status": 0,
		"Session_Hash": ""
	}][0];
*/

var date = new Date();

$.ajaxSetup({
    timeout: 5000
});

function initial(){
	show_menu();
	if(saved_username != "" &&  saved_passwd != ""){
		document.getElementById("wtf_username").value = saved_username;
		document.getElementById("wtf_passwd").value = saved_passwd;
		load_saved_login();
	}
	checkLoginStatus();
}

function create_server_list(index){
	var id = "server_list"+index;
	var select = document.getElementById(id);

	for(var i = 0; i < select.length; i++){
		select.remove(0);
	}

	select.options[select.length] = new Option( "Auto", "auto");
	Object.keys(wtfast_status.Server_List).forEach(function(key) {
		select.options[select.length] = new Option( wtfast_status.Server_List[key], wtfast_status.Server_List[key]);
	});
}

function show_info(){
	var email = wtfast_status.eMail;

	if(email.length > 45){
		document.getElementById("contact_email").title = email;
		email = email.substr(0, 45);
		email = email + "...";
	}
	document.getElementById("contact_email").innerHTML = email;
	document.getElementById("account_type").innerHTML = wtfast_status.Account_Type;
	if(document.getElementById("account_type").innerHTML == "Free"){
		document.getElementById("ended_date_tr").style.display = "none";
	}
	else{
		document.getElementById("ended_date_tr").style.display = "";
		date.setDate(date.getDate() + wtfast_status.Days_Left);
		var date_str = date.getFullYear() + "/" + (date.getMonth() + 1) + "/" + date.getDate();
		document.getElementById("ended_date").innerHTML = date_str;
		document.getElementById("days_left").innerHTML = wtfast_status.Days_Left + "&nbsp" + "Days";
	}
	document.getElementById("max_computers").innerHTML = wtfast_status.Max_Computers;
}

function applyRule(){
	update_rulelist();
	var WTF_Warning_MSG = "There is no rule enabled. Are you sure to apply the settings?";//Untranslated
	if(enable_num == 0){
		if(confirm(WTF_Warning_MSG)){
			document.form.wtf_rulelist.value = wtfast_rulelist;
			showLoading();
			document.form.submit();
		}
		else
			return false;
	}
	else{
		document.form.wtf_rulelist.value = wtfast_rulelist;
		showLoading();
		document.form.submit();
	}
}

function done_validating(action){
	refreshpage();
}

function prevent_lock(rule_num){
	if(document.form.wl_macmode.value == "allow" && rule_num == ""){
		alert("<#FirewallConfig_MFList_accept_hint1#>");
		return false;
	}

	return true;
}


function check_macaddr(obj,flag){
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

function pullLANList(obj){	
	if(isMenuopen == 0){		
		obj.src = "images/arrow-top.gif"
		document.getElementById("ClientList_Block_PC").style.display = 'block';		
		document.form.clientmac_x_0.focus();		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

function addRule(){
	var rule_num = Object.keys(wtfast_rulelist_array).length;
	var mac = document.form.clientmac_x_0.value;
	var rule_enable = cur_rule_enable;

	/*
		verify the total number of enabled clients.
	*/
	if(rule_num == MAX_RULE_NUM){
		alert("<#JS_itemlimit1#> " + MAX_RULE_NUM + " <#JS_itemlimit2#>");
		return false;
	}

	if(cur_rule_enable == "1" && enable_num == wtfast_status.Max_Computers){
		alert("The number of enabled rules reaches the limitiation of the account. The added rule will be default disabled.");
		rule_enable = "0";
	}

	if(mac == ""){
		alert("<#JS_fieldblank#>");
		document.form.clientmac_x_0.focus();
		document.form.clientmac_x_0.select();
		return false;
	}
	else if(check_macaddr(document.form.clientmac_x_0, check_hwaddr_flag(document.form.clientmac_x_0)) == true){
		for(var i = 0; i < wtfast_rulelist_array.length; i++){
			if(wtfast_rulelist_array[i][1] == mac){
				alert("<#JS_duplicate#>");
				document.form.clientmac_x_0.focus();
				document.form.clientmac_x_0.select();
				return false;
			}
		}
	}

	wtfast_rulelist_array.push([rule_enable, document.form.clientmac_x_0.value, document.form.server_list.value]);
	show_rulelist();

}

function update_rulelist(){
	var wtfast_rulelist_value = "";

	Object.keys(wtfast_rulelist_array).forEach(function(key) {
			wtfast_rulelist_value += "<" + wtfast_rulelist_array[key][0] + ">" + wtfast_rulelist_array[key][1] + ">" + wtfast_rulelist_array[key][2];
		});
	wtfast_rulelist = wtfast_rulelist_value;
}

function update_rulelist_array(){
	wtfast_rulelist_array = [];
	var wtfast_rulelist_row = wtfast_rulelist.split('<');
	for(var i = 1; i < wtfast_rulelist_row.length; i ++) {
		var  wtfast_rulelist_col = wtfast_rulelist_row[i].split('>');
		wtfast_rulelist_array[i-1] = [wtfast_rulelist_col[0], wtfast_rulelist_col[1], wtfast_rulelist_col[2]];
	}	
}

function delRule(r){
	var wtfast_rulelist_table = document.getElementById('wtfast_rulelist_table');
	var i = r.parentNode.parentNode.rowIndex;

	delete wtfast_rulelist_array[i];
	wtfast_rulelist_table.deleteRow(i);
	update_rulelist();
	update_rulelist_array();
	show_rulelist();
}

function show_rulelist(){
	var wtfast_rulelist_col = "";
	var code = "";
	enable_num = 0;

	code +='<table width="100%" align="center" cellpadding="4" cellspacing="0"  class="list_table" id="wtfast_rulelist_table">';
	
	if(Object.keys(wtfast_rulelist_array).length == 0)
		code +='<tr><td style="color:#FFCC00;" colspan="4"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		rule_enable_array = [];
		Object.keys(wtfast_rulelist_array).forEach(function(key) {
			code +='<tr id="row'+key+'">';
			rule_enable_array.push(wtfast_rulelist_array[key][0]);
			if(wtfast_rulelist_array[key][0] == "1")
				enable_num++;
			code +='<td style="width:15%">';
			code += '<div class="left" style="width:94px; float:left; cursor:pointer; margin-left:10px;" id="wtfast_rule_enable'+ key + '"></div>';
			code += '<div class="clear"></div></td>';
			code += '<td width="40%" align="center">';
			code += '<table style="width:100%;"><tr><td style="width:60%;height:56px;border:0px;float:right;">';
			var userIconBase64 = "NoIcon";
			var clientName, deviceType, deviceVender, clientIP;
			var clientMac = wtfast_rulelist_array[key][1];
			if(clientList[clientMac]) {
				clientName = (clientList[clientMac].nickName == "") ? clientList[clientMac].name : clientList[clientMac].nickName;
				deviceType = clientList[clientMac].type;
				deviceVender = clientList[clientMac].dpiVender;
				clientIP = clientList[clientMac].ip;
			}
			else {
				clientName = "New device";
				deviceType = 0;
				deviceVender = "";
				clientIP = "";
			}
			if(clientList[clientMac] == undefined) {
				code += '<div style="height:56px;" class="clientIcon type0" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'' + clientIP + '\', \'DHCP\')"></div>';
			}
			else {
				if(usericon_support) {
					userIconBase64 = getUploadIcon(clientMac.replace(/\:/g, ""));
				}
				if(userIconBase64 != "NoIcon") {
					code += '<div style="width:80px;text-align:center;" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'' + clientIP + '\', \'DHCP\')"><img class="imgUserIcon_card" src="' + userIconBase64 + '"></div>';
				}
				else if( (deviceType != "0" && deviceType != "6") || deviceVender == "") {
					code += '<div style="height:56px;" class="clientIcon type' + deviceType + '" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'' + clientIP + '\', \'DHCP\')"></div>';
				}
				else if(deviceVender != "" ) {
					var venderIconClassName = getVenderIconClassName(deviceVender.toLowerCase());
					if(venderIconClassName != "") {
						code += '<div style="height:56px;" class="venderIcon ' + venderIconClassName + '" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'' + clientIP + '\', \'DHCP\')"></div>';
					}
					else {
						code += '<div style="height:56px;" class="clientIcon type' + deviceType + '" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'' + clientIP + '\', \'DHCP\')"></div>';
					}
				}
			}
			code += '</td><td style="width:60%;border:0px;">';
			code += '<div>' + clientName + '</div>';
			code += '<div>' + clientMac + '</div>';
			code += '</td></tr></table>';			
			code += '</td>';
			code += '<td style="width:30%;"><select id="server_list' + key + '" class="input_option" name="server_list'+ key + '" onchange="chagne_server(this);">';
			code += '</select></td>';
			code +='<td style="width:15%"><input class="remove_btn" onclick="delRule(this);" value=""/></td>';
			code += '</tr>';
		});
	}
	code +='</table>';
	document.getElementById('wtfast_rulelist_Block').innerHTML = code;

	if(Object.keys(wtfast_rulelist_array).length > 0){
		Object.keys(wtfast_rulelist_array).forEach(function(key) {
			var obj_id = "#wtfast_rule_enable" + key;

			$(obj_id).iphoneSwitch(wtfast_rulelist_array[key][0],
				function(index) {
					enable_wtfast_rule(index, "1");
				},
				function(index) {
					enable_wtfast_rule(index, "0");
				}
			);

			create_server_list(key);
			var server_list_id = "server_list" + key;
			var select = document.getElementById(server_list_id);

			for(var j = 0; j < select.length; j++){
				if(select[j].value == wtfast_rulelist_array[key][2]){
					select.selectedIndex = j;
					break;
				}
			}
		});
	}
}

function cancel_enable(index){
	var obj_id = "#wtfast_rule_enable" + index;
	$(obj_id).find('.iphone_switch').animate({backgroundPosition: -37}, "slow");
}

function enable_wtfast_rule(index, enable){
	var index_int = parseInt(index);

	if(enable == "1"){
		if(enable_num < wtfast_status.Max_Computers){
			enable_num++;
		}
		else{
			alert("The number of enabled rules reaches the limitiation of the account. You can't enable rule unless you disable one existed enabled rule first.");
			cancel_enable(index);
			return;
		}
	}
	else
		enable_num--;

	rule_enable_array.splice(index_int, 1, enable);
	wtfast_rulelist_array[index_int][0] = enable;
}

function chagne_server(r){
	var key = r.id.substr(11);
	var select = document.getElementById(r.id);
	wtfast_rulelist_array[key][2] = select.value;
}

var over_var = 0;
var isMenuopen = 0;

function hideClients_Block(){
	document.getElementById("pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById("ClientList_Block_PC").style.display="none";
	isMenuopen = 0;
}

function showClientList(){
	var htmlCode = "";
	for(var i=0; i<clientList.length;i++){
		var clientObj = clientList[clientList[i]];
		var clientName = (clientObj.nickName == "") ? clientObj.name : clientObj.nickName;
		var clientIP = "";
		if(clientObj.ip != "offline") {
			clientIP = clientObj.ip;
		}
		htmlCode += '<a title=' + clientList[i] + '><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientmac(\'';
		htmlCode += clientObj.mac;
		htmlCode += '\');"><strong>';
		if(clientName.length > 30) {
			htmlCode += clientName.substring(0, 28) + "..";
		}
		else {
			htmlCode += clientName;
		}
		htmlCode += '</strong></div></a><!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
	}

	document.getElementById("ClientList_Block_PC").innerHTML = htmlCode;
}

function setClientmac(macaddr){
	document.form.clientmac_x_0.value = macaddr;
	hideClients_Block();
	over_var = 0;
}

var isChrome = navigator.userAgent.search("Chrome") > -1;
var isOldIE = navigator.userAgent.search("MSIE") > -1;
function show_login_page(show){
	if(show){
		document.getElementById("WTFast_login_div").style.display = "";
		if(isChrome){
			showFire();
			document.getElementById("fire_pic").style.display = "none";
		}
		else{
			document.getElementById("fire").style.display = "none";
			if(isOldIE){
				document.getElementById("fire_pic").style.top = "509px";
			}
			document.getElementById("fire_pic").style.display = "";
		}
	}
	else{
		document.getElementById("WTFast_login_div").style.display = "none";
		if(isChrome)
			stopFire();
	}
}

function show_management_page(show){
	if(show)
		document.getElementById("FormTitle").style.display = "";
	else
		document.getElementById("FormTitle").style.display = "none";
}
/* 
	[ToDo] callback function to handle the response from wtfast web server.
*/
function checkLoginStatus(){
	if(wtfast_status.Login_status == 1 || (wtfast_status.eMail != undefined && wtfast_status.eMail != "")){
		show_info();
		show_login_page(0);
		show_management_page(1);

		var wtfast_rulelist_row = wtfast_rulelist.split('<');
		for(var i = 1; i < wtfast_rulelist_row.length; i ++) {
			var  wtfast_rulelist_col = wtfast_rulelist_row[i].split('>');
			wtfast_rulelist_array[i-1] = [wtfast_rulelist_col[0], wtfast_rulelist_col[1], wtfast_rulelist_col[2]];
		}
		showClientList();
		show_rulelist();		
		create_server_list("");
		stopFire();

		$.ajax({
			url: "/apply.cgi",
			type: "POST",
			data: {
				action_mode: "wtfast_login",
				wtf_username: $("#wtf_username").val(),
				wtf_passwd: $("#wtf_passwd").val(),
				wtf_account_type: wtfast_status.Account_Type,
				wtf_max_clients: wtfast_status.Max_Computers
			},
			success: function( response ) {
				
			}
		});
	}
	else{
		if(wtfast_status.Error != undefined){
			document.getElementById("error_msg").style.display = "";
			document.getElementById("error_msg").innerHTML = "* " + wtfast_status.Error;
		}

		show_login_page(1);
		show_management_page(0);
	}
}

function load_saved_login() {

	$.ajax({
		url: "http://rapi.wtfast.com/user/login",
		jsonp: "callback",
		dataType: "jsonp",
		async: true,
		data: {
			username: saved_username,
			password: saved_passwd
		},
		success: function( response ) {
			wtfast_status = response;
			checkLoginStatus();
		},
		error: function(XMLHttpRequest, textStatus){
			if(textStatus == "timeout"){
				alert("Server does not response.");
				show_login_page(1);
				show_management_page(0);	
			}
		}
	});

}

function wtf_login(){
	if($("#wtf_username").val() != "" && $("#wtf_passwd").val() != ""){
		document.getElementById("loadingIcon_login").style.display = "";
		document.getElementById("login_button").style.display = "none";
		$.ajax({
			url: "http://rapi.wtfast.com/user/login",
			jsonp: "callback",
			dataType: "jsonp",
			async: true,
			data: {
				username: $("#wtf_username").val(),
				password: $("#wtf_passwd").val()
			},
			success: function( response ) {
				wtfast_status = response;
				checkLoginStatus();	
		
			}, 
			error: function(XMLHttpRequest, textStatus){
				if(textStatus == "timeout"){
					document.getElementById("error_msg").style.display = "";
					document.getElementById("error_msg").innerHTML = "* " + "Server does not response.";
				}
			},
			complete: function(){
				document.getElementById("loadingIcon_login").style.display = "none";
				document.getElementById("login_button").style.display = "";
			}
		});
	}
	else if($("#wtf_username").val() == ""){
		document.getElementById("error_msg").style.display = "";
		document.getElementById("error_msg").innerHTML = "* " + "E-Mail cannot be blank.";
		$("#wtf_username").focus();
	}
	else if($("#wtf_passwd").val() == ""){
		document.getElementById("error_msg").style.display = "";
		document.getElementById("error_msg").innerHTML = "* " + "<#File_Pop_content_alert_desc6#>";
		$("#wtf_passwd").focus();
	}
}

function wtf_logout(){

	if (wtfast_status.Session_Hash) {
		document.getElementById("logout_button").style.display = "none";
		document.getElementById("loadingIcon_logout").style.display = "";

		$.ajax({
			url: "http://rapi.wtfast.com/user/logout",
			jsonp: "callback",
			dataType: "jsonp",
			async: true,
			data: {
				session_hash: wtfast_status.Session_Hash
			},
			success: function( response ) {
		
				wtfast_status = response;
			
				Object.keys(wtfast_rulelist_array).forEach(function(key) {
					wtfast_rulelist_array[key][0] = "0";
				});
				update_rulelist();
				document.form.wtf_rulelist.value = wtfast_rulelist;
				document.wtfast_form.action_mode.value = "wtfast_logout";
				document.wtfast_form.submit();
				show_management_page(0);
				show_login_page(1);
				document.getElementById("loadingIcon_logout").style.display = "none";
				document.getElementById("logout_button").style.display = "";
			}
		});
	}

}

function open_link(page){
	if(page == "subscribe"){
		tourl = "https://www.wtfast.com/Subscription/Index/new";
		link = window.open(tourl, "WTFast","toolbar=yes,location=yes,directories=no,status=yes,menubar=yes,scrollbars=yes,resizable=yes,copyhistory=no,width=640,height=480");	
	}
	else if(page == "newAccoubt"){
		tourl = "https://www.wtfast.com/Subscription/Index/new";
		link = window.open(tourl, "WTFast","toolbar=yes,location=yes,directories=no,status=yes,menubar=yes,scrollbars=yes,resizable=yes,copyhistory=no,width=640,height=480");	
	}
	else if(page == "supportGames"){
		tourl = "https://www.wtfast.com/pages/supported_games/";
		link = window.open(tourl, "WTFast","toolbar=yes,location=yes,directories=no,status=yes,menubar=yes,scrollbars=yes,resizable=yes,copyhistory=no,width=640,height=480");	
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

<input type="hidden" name="current_page" value="Advanced_WTFast_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WTFast_Content.asp">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="action_script" value="restart_wtfd">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wtf_rulelist" value="">
<input type="hidden" name="wtf_server_list" value="">

<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td align="left" valign="top" >
		<div id="WTFast_login_div" class="Background" style="display:none;">
		<table width="760px" height="600px" border="0" cellpadding="0" cellspacing="0" id="WTFast_Login">
		<tr>
		<td colspan="2" style="height:300px; padding-bottom:0px;">
		<div class="LogoCircle"><img src="/images/wtfast_logo.png" style="position:relative; top:74px;"></div>
		<div style="float:left; color:#E3E3E3; margin-top:45px;margin-left:46px;margin-right:46px; font-size:14px;">
		WTFast untangles the web.  Our goal is to make the fastest and most secure network on the Internet. With WTFast you can dramatically speed up and smooth out connections for your favorite MMO/MOBA/FPS games.  WTFast often makes the difference between a winning and losing move in online games!
		</div>

		<div style="float:left; color:#A99877; margin-top:18px;margin-left:46px;margin-right:46px; font-size:12px;">
		<span style="font-weight:bolder">Supported games: </span>League of Legends, Counter Strike, Global Offensive, Dota 2, Grand Theft Auto V, World of Warcraft, World of Tank, SMITE, Diablo III, Minecraft, Heroes of the storm...<a id="see_all" href="javascript:open_link('supportGames')">(See All)</a><!--untranslated-->
		</div>
		</td>
		</tr>
		<tr><td colspan="2" style="color:#FFFFFF; height:40px; font-size:20px; padding-top:50px; font-weight:bold; text-align:center;">LOGIN TO WTFast</td></tr>
		<tr>
			<th style="width:254px; height:40px; color:#FFFFFF; font-size:14px; text-align:right; padding-right:15px;">E-Mail</th><!--untranslated-->
			<td style="color:#FFFFFF; font-size:14px; text-align:left;">
				<input type="text" maxlength="32" class="login_input" id="wtf_username" name="wtf_username" value="" onkeypress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off" >
				<span ><a id="link" href="javascript:open_link('newAccoubt')" style="margin-left:5px;text-decoration:underline;color:#">Create A Free Account</a></span>
			</td>
		</tr>
		<tr>
			<th style="height:35px; color:#FFFFFF; font-size:14px; text-align:right; padding-right:15px;"><#PPPConnection_Password_itemname#></th>
			<td style="color:#FFFFFF; font-size:14px; text-align:left;">
				<input type="password" maxlength="32" class="login_input" id="wtf_passwd" name="wtf_passwd" value="" autocorrect="off" autocapitalize="off">
			</td>
		</tr>
		<tr>
			<td id="error_msg" colspan="2" style="height:30px; text-align:left; padding-left:268px; color:#EFF21B; font-size:14px;"></td>
		</tr>
		<tr>
			<td colspan="2" style="height:35px;text-align:center;">
				<input id="login_button" class="button_gen" style="backgroud-color:#000000;" onclick="wtf_login();" type="button" value="Login"/>
				<img id="loadingIcon_login" style="display:none;" src="images/InternetScan.gif">
			</td>
		</tr>
		</table>
		<div style="position:relative; top:10px ;color:#A99877; font-size:12px; text-align:center;">* Note: New patch will be updated regularly, please keep your firmware stay up to date.</div>
		<div>
			<canvas id="fire" style="width:760px;height:430px; display:block;position:absolute; top:491px; z-index:-1;"></canvas>
			<img id="fire_pic" style="position: absolute; top: 499px; z-index: -1; margin-left: 3px;display:none;" src="images/fire.jpg">
		</div>
		</div>
	
		<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle" style="display:none;border-radius:3px;">
		<tbody>
			<tr>
				<td bgcolor="#4D595D" valign="top">
					<div>&nbsp;</div>
					<div class="formfonttitle">WTFast Management</div>
					<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
					<table id="MainTable1" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
						<thead>
						  <tr>
							<td colspan="2">Login Information</td>
						  </tr>
						</thead>

					<tr>
						<th>Contact E-Mail</th>
						<td>
					  		<span id="contact_email" style="color:#FFFFFF;"></span>
					  		<span><input id="logout_button" class="button_gen" style="margin-left:20px;" onclick="wtf_logout();" type="button" value="<#t1Logout#>"/>
					  		<img id="loadingIcon_logout" style="margin-left:10px; display:none;" src="/images/InternetScan.gif"></span>
						</td>
					</tr>

					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,2);">Account Type</a></th><!--untranslated-->
						<td>
					  		<div id="account_type"></div>
						</td>
					</tr>

					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,2);">Maximum Computers</a></th><!--untranslated-->
						<td>
					  		<span id="max_computers" style="color:#FFFFFF;"></span>
							<span><input class="button_gen" style="margin-left:20px;" onclick="open_link('subscribe');" type="button" value="Upgrade"/></span>
						</td>
					</tr>

					<tr id="ended_date_tr">
						<th>Ended Date</th>
						<td>
					  		<span id="ended_date" style="color:#FFFFFF;"></span><span style="margin-left:10px;">( Days Left: <span id="days_left"></span> )</span> <!--untranslated-->
						</td>
					</tr>
					
					</table>
					<table id="MainTable2" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">
						<thead>
							<tr>
								<td colspan="4">WTFast Rules List</td><!--untranslated-->
							</tr>
						</thead>
						<tr>
							<th width="15%"><#WLANConfig11b_WirelessCtrl_button1name#>/<#WLANConfig11b_WirelessCtrl_buttonname#></th>
							<th width="40%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,10);">Client's MAC address</th><!--untranslated-->
							<th width="30%"><#LANHostConfig_x_DDNSServer_itemname#></th>
							<th width="15%"><#list_add_delete#></th>
						</tr>
						<tr>
							<td>
								<div class="left" style="width:94px; float:left; cursor:pointer; margin-left:10px;" id="wtfast_rule_enable"></div>
								<div class="clear"></div>
									<script type="text/javascript">
									$('#wtfast_rule_enable').iphoneSwitch( rule_default_state,
										function() {
											cur_rule_enable = "1";
										},
										function() {
											cur_rule_enable = "0";
										}
									);
									</script>
							</td>
							<td>
								<input type="text" class="input_20_table" maxlength="17" name="clientmac_x_0" style="margin-left:-20px;width:220px;" onKeyPress="return validator.isHWAddr(this,event)" onClick="hideClients_Block();" autocorrect="off" autocapitalize="off" placeholder="ex: <% nvram_get("lan_hwaddr"); %>">
									<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;*margin-left:-3px;*margin-top:1px;" onclick="pullLANList(this);" title="<#select_MAC#>" onmouseover="over_var=1;" onmouseout="over_var=0;">
									<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>
							</td>
							<td>
								<select id="server_list" class="input_option" name="server_list" onchange="">
								</select>
							</td>
							<td>
								<input type="button" class="add_btn" onClick="addRule();" value="">
							</td>
						</tr>
					</table>
					<div id="wtfast_rulelist_Block"></div>

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
	</td>
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
<form method="post" name="wtfast_form" action="/apply.cgi" target="hidden_frame">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="wtf_username" value="">
<input type="hidden" name="wtf_passwd" value="">
<input type="hidden" name="wtf_rulelist" value="">
</form>
</body>
</html>