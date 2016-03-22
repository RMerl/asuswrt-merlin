
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
	border:1px outset #FFF;
	background-color:#000;
	position:absolute;
	margin-top:1px;
	*margin-top:22px;	
	margin-left:-4px;
	*margin-left:-263px;
	width:220px;
	*width:192px;
	text-align:left;	
	height:auto;
	overflow-y:auto;
	z-index:200;
	padding: 1px;
	display:none;
	left: 151px;
}

#ClientList_Block_PC div{
	background-color:#000;
	height:auto;
	*height:20px;
	line-height:20px;
	text-decoration:none;
	font-family: calibri;
	font-size:14px;
}

#ClientList_Block_PC a{
	background-color:#EFEFEF;
	color:#EAE9E9;
	font-size:12px;
	font-family:Arial, Helvetica, sans-serif;
	text-decoration:none;	
}
#ClientList_Block_PC strong {
	padding-left: 3px;
}
#ClientList_Block_PC div:hover, #ClientList_Block a:hover{
	background-color:#3366FF;
	color:#FFFFFF;
}

.Background{
	background-image:
		url(images/wtfast_point.gif),
		url(images/wtfast_bg.png);
	background-repeat: no-repeat;
	border-radius: 3px;
	position:absolute;
	top:116px;
	z-index:1;
	width:760px; 
	height:800px; 
}

.Background_Management{
	border-radius: 3px;
	position:absolute;
	top:116px;
	z-index:1;
	width:760px;
	background: #4e5659; /* Old browsers */
	/* IE9 SVG, needs conditional override of 'filter' to 'none' */
	background: url(data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiA/Pgo8c3ZnIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgd2lkdGg9IjEwMCUiIGhlaWdodD0iMTAwJSIgdmlld0JveD0iMCAwIDEgMSIgcHJlc2VydmVBc3BlY3RSYXRpbz0ibm9uZSI+CiAgPGxpbmVhckdyYWRpZW50IGlkPSJncmFkLXVjZ2ctZ2VuZXJhdGVkIiBncmFkaWVudFVuaXRzPSJ1c2VyU3BhY2VPblVzZSIgeDE9IjAlIiB5MT0iMCUiIHgyPSIwJSIgeTI9IjEwMCUiPgogICAgPHN0b3Agb2Zmc2V0PSIwJSIgc3RvcC1jb2xvcj0iIzRlNTY1OSIgc3RvcC1vcGFjaXR5PSIxIi8+CiAgICA8c3RvcCBvZmZzZXQ9IjMzJSIgc3RvcC1jb2xvcj0iIzAwMDAwMCIgc3RvcC1vcGFjaXR5PSIxIi8+CiAgICA8c3RvcCBvZmZzZXQ9Ijc5JSIgc3RvcC1jb2xvcj0iIzAwMDAwMCIgc3RvcC1vcGFjaXR5PSIxIi8+CiAgICA8c3RvcCBvZmZzZXQ9IjEwMCUiIHN0b3AtY29sb3I9IiM1YjAwMDAiIHN0b3Atb3BhY2l0eT0iMSIvPgogIDwvbGluZWFyR3JhZGllbnQ+CiAgPHJlY3QgeD0iMCIgeT0iMCIgd2lkdGg9IjEiIGhlaWdodD0iMSIgZmlsbD0idXJsKCNncmFkLXVjZ2ctZ2VuZXJhdGVkKSIgLz4KPC9zdmc+);
	background: -moz-linear-gradient(top,  #4e5659 0%, #000000 33%, #000000 79%, #5b0000 100%); /* FF3.6+ */
	background: -webkit-gradient(linear, left top, left bottom, color-stop(0%,#4e5659), color-stop(33%,#000000), color-stop(79%,#000000), color-stop(100%,#5b0000)); /* Chrome,Safari4+ */
	background: -webkit-linear-gradient(top,  #4e5659 0%,#000000 33%,#000000 79%,#5b0000 100%); /* Chrome10+,Safari5.1+ */
	background: -o-linear-gradient(top,  #4e5659 0%,#000000 33%,#000000 79%,#5b0000 100%); /* Opera 11.10+ */
	background: -ms-linear-gradient(top,  #4e5659 0%,#000000 33%,#000000 79%,#5b0000 100%); /* IE10+ */
	background: linear-gradient(to bottom,  #4e5659 0%,#000000 33%,#000000 79%,#5b0000 100%); /* W3C */
	filter: progid:DXImageTransform.Microsoft.gradient( startColorstr='#4e5659', endColorstr='#5b0000',GradientType=0 ); /* IE6-8 */
}

select{
	border: solid 1px #EAE9E9;
	appearance:none;
	-moz-appearance:none;
	-webkit-appearance:none;
	background: url("images/arrow-down.gif") no-repeat scroll 90% center transparent;
	padding-right: 14px;
	height:25px;
	background-color:#000;
	color:#EAE9E9;
	font-family: calibri;
	font-size:14px;  
}

select::-ms-expand { display: none; }

#pull_arrow{
	border: 1px solid #FFF;
	border-left: 0px;
	background-color: transparent;
	padding:7px 4px 6px 0px;
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
	color:#EBE8E8; 
	width:220px; 
	border: solid 1px #949393;
}

.wtfast_button{
	font-weight: bolder;
	font-family: calibri;
	text-shadow: 1px 1px 0px black;
	border: solid 1px #B1ADAD;
	border-radius: 5px;
	color: #B1ADAD;
	height:33px;
	font-family:Verdana;
	font-size:12px;
	padding:0 .70em 0 .70em;  
	width:122px;
	overflow:visible;
	cursor:pointer;
	outline: none; /* for Firefox */
	hlbr:expression(this.onFocus=this.blur()); /* for IE */
	white-space:normal;
	background: transparent;
}

.wtfast_button:hover{
	border: solid 1px #FFFFFF;
	color: #FFFFFF;
}

.addCircle{
    border-radius: 50%;
    behavior: url(PIE.htc);
    width: 24px;
    height: 24px;
    background: transparent;
    border: 0px;
}

.addCircle_shine{
    border-radius: 50%;
    behavior: url(PIE.htc);
    width: 30px;
    height: 30px;
    background: transparent;
    border: 0px solid #FFFFFF;
    box-shadow: 0px 0 7px #FFFFFF,inset 0px 0 10px #FFFFFF;
    text-align: center;
}

.wtfast_input_option{
	height:21px;
	background-color:#000000;
	border: 1px solid #EAE9E9;	
	color:#EAE9E9;
	font-family: calibri;
	font-size:14px;
}

.wtfast_remove_btn{
 	background: transparent url(images/New_ui/delete.svg) no-repeat scroll center top;
	border:0;
	height:25px;
 	width:25px;
 	cursor:pointer;
}
.wtfast_remove_btn:hover, .wtfast_remove_btn:active{
	border:0;
 	cursor:pointer;
}
</style>
<script>

var rule_default_state = "1";
var cur_rule_enable = rule_default_state;
var rule_enable_array = [];
var MAX_RULE_NUM = 5; //Original Value: 1
var enable_num = 0;
var orig_wtfast_rulelist = decodeURIComponent('<% nvram_char_to_ascii("", "wtf_rulelist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<");
var wtfast_rulelist = orig_wtfast_rulelist;
var wtfast_rulelist_array = new Array();
var wtfast_rulelist_row = wtfast_rulelist.split('<');
for(var i = 1; i < wtfast_rulelist_row.length; i ++) {
	var  wtfast_rulelist_col = wtfast_rulelist_row[i].split('>');
	if(wtfast_rulelist_col.length == 4){
		wtfast_rulelist_array[i-1] = [wtfast_rulelist_col[0], wtfast_rulelist_col[1], wtfast_rulelist_col[2], "none", wtfast_rulelist_col[3]];
	}
	else	
		wtfast_rulelist_array[i-1] = [wtfast_rulelist_col[0], wtfast_rulelist_col[1], wtfast_rulelist_col[2], wtfast_rulelist_col[3], wtfast_rulelist_col[4]];
}

var saved_username = decodeURIComponent('<% nvram_char_to_ascii("", "wtf_username"); %>');
var saved_passwd = decodeURIComponent('<% nvram_char_to_ascii("", "wtf_passwd"); %>');
var saved_game_list = decodeURIComponent('<% nvram_char_to_ascii("", "wtf_game_list"); %>');
var saved_server_list = decodeURIComponent('<% nvram_char_to_ascii("", "wtf_server_list"); %>');
var wtf_enable_games = "";
//	[ToDo] This JSON should be stored in the router or get form wtfd.
var wtfast_status = [<% wtfast_status();%>][0];

if(saved_game_list != ""){
	var saved_game_row = saved_game_list.split('<');
		for(var i = 1; i < saved_game_row.length; i++) {
			var saved_game_col = saved_game_row[i].split('>');		
			wtfast_status.Game_List.push({"name": saved_game_col[0], "id":saved_game_col[1]});
		}
}

if(saved_server_list != ""){
	var saved_server_row = saved_server_list.split('<');
		for(var i = 1; i < saved_server_row.length; i++) {
			wtfast_status.Server_List[i-1] = saved_server_row[i];
		}
}
/*
	var wtfast_status = [{ 
		"eMail": "", 
		"Account_Type": "",
		"Max_Computers": 0,
		"Server_List": [],
		"Game_List": [{"name":"", "id":}],
		"Days_Left": 0,
		"Login_status": 0,
		"Session_Hash": ""
	}][0];
*/

$.ajaxSetup({
    timeout: 5000
});


function initial(){
	var sVer = navigator.userAgent.indexOf("MSIE") ;
	var ua = navigator.userAgent;
	var rv;

	if (new RegExp("Trident/.*rv:([0-9]{1,}[\.0-9]{0,})").exec(ua) != null) {
	  rv = parseFloat(RegExp.$1);
	} else {
	  rv = -1;
	}
	show_menu();
	document.getElementById("_GameBoost").innerHTML = '<table><tbody><tr><td><div class="_GameBoost"></div></td><td><div style="width:120px;"><#Game_Boost#></div></td></tr></tbody></table>';
	document.getElementById("_GameBoost").className = "menu_clicked";

	var GB_login_str = "<#Game_Boost_login#>";
	GB_login_str = GB_login_str.replace(/WTFast/gi,"<span><img src=\"/images/wtfast_logo.png\" style=\"margin-bottom:-5px; margin-left:10px;\"></span>");
	document.getElementById("Game_Boost_login_div").innerHTML = GB_login_str;

	if(saved_username != "" &&  saved_passwd != ""){
		document.getElementById("wtf_username").value = saved_username;
		document.getElementById("wtf_passwd").value = saved_passwd;
	}
	else{
		document.getElementById("wtf_username").value = "";
		document.getElementById("wtf_passwd").value = "";
	}
	checkLoginStatus();

	if( sVer!= -1 || rv == 11)
  		document.getElementById("pull_arrow").style.marginLeft = "-4px";

}

function _create_server_list(name, index, showauto){
	var id = name + index;
	var select = document.getElementById(id);

	select.length = 0;
	if (showauto){
		select.options[select.length] = new Option( "Auto", "auto");
	} else { 
		select.options[select.length] = new Option( "-", "none");
	}

	Object.keys(wtfast_status.Server_List).forEach(function(key) {
		select.options[select.length] = new Option( wtfast_status.Server_List[key], wtfast_status.Server_List[key]);
	});
}

function create_server_list(index){
	_create_server_list("server_1_list", index, true);
	_create_server_list("server_2_list", index, false);
}

function allow_multiple_servers() {
	return wtfast_status.Account_Type == "Advanced";
}

function create_game_list(index){
	var id = "game_list"+index;
	var select = document.getElementById(id);

	select.length = 0;
	var j = 0;
	Object.keys(wtfast_status.Game_List).forEach(function(key) {
		select.options[j] = new Option( wtfast_status.Game_List[key].name, wtfast_status.Game_List[key].id );
		j++;
	});
}

function show_info(){
	var email = wtfast_status.eMail;
	var date = new Date();

	if(email.length > 45){
		document.getElementById("contact_email").title = email;
		email = email.substr(0, 45);
		email = email + "...";
	}
	document.getElementById("contact_email").innerHTML = email;
	if(wtfast_status.Days_Left == 0){
		document.getElementById("account_type").innerHTML = "<#GB_account_type0#>";
		document.getElementById("ended_date").innerHTML = "<#GB_ended_date_type0#>";
		document.getElementById("days_left").style.display = "none";
		document.getElementById("days_title").style.display = "none";
	}
	else{
		if(wtfast_status.Account_Type == "Basic")
			document.getElementById("account_type").innerHTML = "<#GB_account_type1#>";
		else if(wtfast_status.Account_Type == "Advanced")
			document.getElementById("account_type").innerHTML = "<#GB_account_type2#>";
		date.setDate(date.getDate() + wtfast_status.Days_Left);
		var date_str = date.getFullYear() + "/" + (date.getMonth() + 1) + "/" + date.getDate();
		document.getElementById("ended_date").innerHTML = date_str;
		document.getElementById("days_left").style.display = "";
		document.getElementById("days_title").style.display = "";
		document.getElementById("days_left").innerHTML = wtfast_status.Days_Left + "&nbsp" + "Days";
	}
	document.getElementById("max_computers").innerHTML = wtfast_status.Max_Computers;
}

function applyRule(){
	update_rulelist(0);
	var WTF_Warning_MSG = "There is no rule enabled. Are you sure to apply the settings?";//Untranslated
	if(enable_num == 0){
		if(!confirm(WTF_Warning_MSG)){
			return false;
		}
	}

	document.form.wtf_rulelist.value = wtfast_rulelist;
	document.form.wtf_enable_game.value = wtf_enable_games;
	showLoading();
	document.form.submit();
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
	var element = document.getElementById('ClientList_Block_PC');
	var isMenuopen = element.offsetWidth > 0 || element.offsetHeight > 0;
	if(isMenuopen == 0){		
		obj.src = "images/arrow-top.gif"
		element.style.display = 'block';		
		document.form.clientmac_x_0.focus();
	}
	else
		hideClients_Block();
}

function EnableMouseOver(obj, enable){
	if(enable == ""){
		if(cur_rule_enable == "1")
			obj.src = "images/New_ui/enable_hover.svg";
		else
			obj.src = "images/New_ui/disable_hover.svg";
	}
	else{
		if(enable == "1")
			obj.src = "images/New_ui/enable_hover.svg";
		else
			obj.src = "images/New_ui/disable_hover.svg";
	}
}

function EnableMouseOut(obj, enable){
	if(enable == ""){
		if(cur_rule_enable == "1")
			obj.src = "images/New_ui/enable.svg";
		else
			obj.src = "images/New_ui/disable.svg";
	}
	else{
		if(enable == "1")
			obj.src = "images/New_ui/enable.svg";
		else
			obj.src = "images/New_ui/disable.svg";
	}
}

function change_enable(obj){
	if(cur_rule_enable == "1"){
		cur_rule_enable = "0";
		obj.src = "images/New_ui/disable.svg";
	}
	else{
		cur_rule_enable = "1";
		obj.src = "images/New_ui/enable.svg";
	}
}

function addRule(){
	var rule_num = Object.keys(wtfast_rulelist_array).length;
	var mac = document.form.clientmac_x_0.value;
	var rule_enable = cur_rule_enable;
	var addRule = 1;

	/*
		verify the total number of enabled clients.
	*/
	if(rule_num == MAX_RULE_NUM){
		alert("<#JS_itemlimit1#> " + MAX_RULE_NUM + " <#JS_itemlimit2#>");
		change_add_btn(0);
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
		change_add_btn(0);
		return false;
	}
	else if(check_macaddr(document.form.clientmac_x_0, check_hwaddr_flag(document.form.clientmac_x_0)) == true){
		Object.keys(wtfast_rulelist_array).forEach(function(key){
			if(wtfast_rulelist_array[key][1] == mac){
				alert("<#JS_duplicate#>");
				document.form.clientmac_x_0.focus();
				document.form.clientmac_x_0.select();
				addRule = 0;
			}
		});

		if(addRule){
			wtfast_rulelist_array.push([rule_enable, document.form.clientmac_x_0.value, document.form.server_1_list.value, document.form.server_2_list.value, document.form.game_list.value]);
			if(rule_enable == "1" && (wtf_enable_games.indexOf(document.form.game_list.value) == -1)){
				wtf_enable_games += "<" + document.form.game_list.value;
			}
			update_rulelist(1);
			show_rulelist();
		}
		change_add_btn(0);
		document.form.clientmac_x_0.value = "";
	}

}

function update_rulelist(ApplyCheck){
	var wtfast_rulelist_value = "";

	Object.keys(wtfast_rulelist_array).forEach(function(key) {
			wtfast_rulelist_value += "<" + wtfast_rulelist_array[key][0] + ">" + wtfast_rulelist_array[key][1] + ">" + wtfast_rulelist_array[key][2]+ ">" + wtfast_rulelist_array[key][3] + ">" + wtfast_rulelist_array[key][4];
		});
	wtfast_rulelist = wtfast_rulelist_value;

	if(ApplyCheck){
		if(orig_wtfast_rulelist != wtfast_rulelist)
			show_apply(1);
		else
			show_apply(0);
	}
}

 
function delRule(r){
	var wtfast_rulelist_table = document.getElementById('wtfast_rulelist_table');
	var i = r.parentNode.parentNode.parentNode.rowIndex;
	var array_index = i/2;
	wtfast_rulelist_array.splice(array_index, 1);
	wtfast_rulelist_table.deleteRow(i);
	update_rulelist(1);
	show_rulelist();
}


function show_rulelist(){
	var wtfast_rulelist_col = "";
	var code = "";
	enable_num = 0;

	code +='<table width="710px" align="center" cellpadding="4" cellspacing="0" style="text-align:center; margin-left:20px;" id="wtfast_rulelist_table">';
	
	if(Object.keys(wtfast_rulelist_array).length == 0)
		code +='<tr><td style="color:#EAE9E9;" colspan="5"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		rule_enable_array = [];
		Object.keys(wtfast_rulelist_array).forEach(function(key) {
			code +='<tr id="row'+key+'">';
			rule_enable_array.push(wtfast_rulelist_array[key][0]);
			code += '<td style="width:15%;" align="center">';
			if(wtfast_rulelist_array[key][0] == "1"){
				enable_num++;
				code += '<div><img id="wtfast_rule_enable'+ key + '" src = "images/New_ui/enable.svg" onMouseOver="EnableMouseOver(this, \'1\');" onMouseOut="EnableMouseOut(this, \'1\');" style="width:25px; height:25px; cursor:pointer;" onclick=""></div>';
			}
			else
				code += '<div><img id="wtfast_rule_enable'+ key + '" src = "images/New_ui/disable.svg" onMouseOver="EnableMouseOver(this, \'0\');" onMouseOut="EnableMouseOut(this, \'0\');" style="width:25px; height:25px; cursor:pointer;" onclick=""></div>';

			code += '</td>';
			code += '<td width="30%" align="center">';
			code += '<table style="width:100%;"><tr><td style="width:40%;height:52px;border:0px;">';
			var userIconBase64 = "NoIcon";
			var clientName, deviceType, deviceVender, clientIP;
			var clientMac = wtfast_rulelist_array[key][1];
			if(clientList[clientMac]) {
				clientName = (clientList[clientMac].nickName == "") ? clientList[clientMac].name : clientList[clientMac].nickName;
				deviceType = clientList[clientMac].type;
				deviceVender = clientList[clientMac].vendor;
				clientIP = clientList[clientMac].ip;
			}
			else {
				clientName = "New device";
				deviceType = 0;
				deviceVender = "";
				clientIP = "";
			}
			if(typeof(clientList[clientMac]) == "undefined") {
				code += '<div class="clientIcon type0" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'' + clientIP + '\', \'WTFast\')"></div>';
			}
			else {
				if(usericon_support) {
					userIconBase64 = getUploadIcon(clientMac.replace(/\:/g, ""));
				}
				if(userIconBase64 != "NoIcon") {
					code += '<div style="width:80px;text-align:center;" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'' + clientIP + '\', \'WTFast\')"><img class="imgUserIcon_card" src="' + userIconBase64 + '"></div>';
				}
				else if(deviceType != "0" || deviceVender == "") {
					code += '<div class="clientIcon type' + deviceType + '" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'' + clientIP + '\', \'WTFast\')"></div>';
				}
				else if(deviceVender != "" ) {
					var venderIconClassName = getVenderIconClassName(deviceVender.toLowerCase());
					if(venderIconClassName != "" && !downsize_4m_support) {
						code += '<div class="venderIcon ' + venderIconClassName + '" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'' + clientIP + '\', \'WTFast\')"></div>';
					}
					else {
						code += '<div class="clientIcon type' + deviceType + '" onClick="popClientListEditTable(\'' + clientMac + '\', this, \'' + clientName + '\', \'' + clientIP + '\', \'WTFast\')"></div>';
					}
				}
			}
			if(wtfast_rulelist_array[key][0] == "0")
				code += '</td><td style="width:60%;border:0px; color:#949393;">';
			else			
				code += '</td><td style="width:60%;border:0px;">';
			code += '<div>' + clientName + '</div>';
			code += '<div>' + clientMac + '</div>';
			code += '</td></tr></table>';
			code += '</td>';

			if(wtfast_rulelist_array[key][0] == "0")
				code += '<td style="width:26%;"><select id="game_list' + key + '"style="width:160px; background-position:143px; color:#949393; border: 1px solid #949393;" name="game_list'+ key + '" onchange="change_game(this);">';
			else
				code += '<td style="width:26%;"><select id="game_list' + key + '"style="width:160px; background-position:143px;" name="game_list'+ key + '" onchange="change_game(this);">';
			code += '</select></td>';

			if(wtfast_rulelist_array[key][0] == "0"){
				code += '<td style="width:14%;">';
				code += '<select data-server_number="1" data-row="' + key + '" id="server_1_list' + key + '" class="wtfast_input_option" style="color:#949393; border: 1px solid #949393;" name="server_1_list'+ key + '" onchange="change_server($(this));"></select>';	
				code += '<div class="server_2_list_div"><select data-server_number="2" data-row="' + key + '" id="server_2_list' + key + '" class="wtfast_input_option" style="color:#949393; border: 1px solid #949393; margin-top: 10px;" name="server_2_list' + key + '" onchange="change_server($(this));"></select></div>';
				code += '</td>';
			}
			else{
				code += '<td style="width:14%;">';
				code += '<select data-server_number="1" data-row="' + key + '" id="server_1_list' + key + '" class="wtfast_input_option" name="server_1_list'+ key + '" onchange="change_server($(this));"></select>';
				code += '<div class="server_2_list_div"><select data-server_number="2" data-row="' + key + '" id="server_2_list' + key + '" class="wtfast_input_option" style="margin-top: 10px;" name="server_2_list' + key + '" onchange="change_server($(this));"></select></div>';			
				code += '</td>';
			}
	
			code += '<td style="width:15%"><div><img src = "images/New_ui/delete.svg" onMouseOver="this.src=\'images/New_ui/delete_hover.svg\'" onMouseOut="this.src=\'images/New_ui/delete.svg\'"style="width:25px; height:25px; cursor:pointer;" onclick="delRule(this);"></div></td>';
			code += '</tr>';
			code += '<tr><td colspan="5"><div style="width:100%;height:1px;background-color:#660000"></div></td></tr>'
		});
	}
	code +='</table>';
	document.getElementById('wtfast_rulelist_Block').innerHTML = code;

	if(Object.keys(wtfast_rulelist_array).length > 0){
		Object.keys(wtfast_rulelist_array).forEach(function(key) {
			var obj_id = "wtfast_rule_enable" + key;
			var obj = document.getElementById(obj_id);

			obj.onclick = function(){
				if(wtfast_rulelist_array[key][0] == "1"){
					enable_wtfast_rule(key, "0");
				}
				else{
					enable_wtfast_rule(key, "1");
				}
			}

			create_game_list(key);
			var game_list_id = "game_list" + key;
			var select = document.getElementById(game_list_id);

			for(var j = 0; j < select.length; j++){
				if(select[j].value == wtfast_rulelist_array[key][4]){
					select.selectedIndex = j;
					break;
				}
			}			

			create_server_list(key);
			var server_1_list_id = "server_1_list" + key;
			var server_1_list_select = document.getElementById(server_1_list_id);

			for(var j = 0; j < server_1_list_select.length; j++){
				if(server_1_list_select[j].value == wtfast_rulelist_array[key][2]){
					server_1_list_select.selectedIndex = j;
					break;
				}
			}

			var server_2_list_id = "server_2_list" + key;
			var server_2_list_select = document.getElementById(server_2_list_id);

			for(var j = 0; j < server_2_list_select.length; j++){
				if(server_2_list_select[j].value == wtfast_rulelist_array[key][3]){
					server_2_list_select.selectedIndex = j;
					break;
				}
			}

			update_server_list_visibilities($(server_1_list_select));

		});
	}

	var enable_obj = document.getElementById("enable_fig");
	if(enable_num == wtfast_status.Max_Computers){
		cur_rule_enable = "0";
		enable_obj.src = "images/New_ui/disable.svg";
	}
	else{
		cur_rule_enable = "1";
		enable_obj.src = "images/New_ui/enable.svg";
	}
}

function enable_wtfast_rule(index, enable){
	var index_int = parseInt(index);
	var obj_id = "wtfast_rule_enable" + index;
	var obj = document.getElementById(obj_id);

	if(enable == "1"){
		if(enable_num == wtfast_status.Max_Computers){
			alert("The number of enabled rules reaches the limitiation of the account. You can't enable rule unless you disable one existed enabled rule first.");
			return;
		}
	}

	rule_enable_array.splice(index_int, 1, enable);
	wtfast_rulelist_array[index_int][0] = enable;
	update_rulelist(1);
	show_rulelist();
}

function change_game(r){
	var key = r.id.substr(9);
	var select = document.getElementById(r.id);
	wtfast_rulelist_array[key][4] = select.value;
	update_rulelist(1);
}

function update_server_list_visibilities(target) {
	var server_number = target.data("server_number");
	var row = target.data("row");
	var server = target.val();
	var container = target.closest("td");

	if(server == "auto") {
		container.children(".server_2_list_div").hide();
	} 
	else if(allow_multiple_servers()) {
		container.children(".server_2_list_div").show();
	}
	else{
		container.children(".server_2_list_div").hide();
	}
}

function change_server(target){
	var server_number = target.data("server_number");
	var row = target.data("row");
	var server = target.val();
	var container = target.closest("td");

	wtfast_rulelist_array[row][server_number + 1] = server;
	update_server_list_visibilities(target);
	update_rulelist(1);
}

function hideClients_Block(){
	document.getElementById("pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById("ClientList_Block_PC").style.display="none";
}

function setClientmac(macaddr){
	document.form.clientmac_x_0.value = macaddr;
	hideClients_Block();
	change_add_btn(1);
}

function reset_rule_state(){
	enable_num = 0;
	Object.keys(wtfast_rulelist_array).forEach(function(key) {
		wtfast_rulelist_array[key][0] = "0";
	});
	update_rulelist(0);
	orig_wtfast_rulelist = wtfast_rulelist;
}

function reset_proxy2(){
	Object.keys(wtfast_rulelist_array).forEach(function(key) {
		wtfast_rulelist_array[key][3] = "none";
	});
	update_rulelist(0);
	orig_wtfast_rulelist = wtfast_rulelist;
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
	if(show){
		document.getElementById("error_msg").innerHTML = "";
		document.getElementById("ManagementPage").style.display = "";
	}
	else
		document.getElementById("ManagementPage").style.display = "none";
}
/* 
	[ToDo] callback function to handle the response from wtfast web server.
*/
function checkLoginStatus(){
	if(wtfast_status.Login_status == 1 && (typeof(wtfast_status.eMail) != "undefined" && wtfast_status.eMail != "")){
		show_info();
		show_login_page(0);
		show_management_page(1);
		showDropdownClientList('setClientmac', 'mac', 'all', 'ClientList_Block_PC', 'pull_arrow', 'all');
		show_rulelist();
		update_server_list_visibilities($("server_1_list"));
		create_server_list("");
		create_game_list("");
		stopFire();
	}
	else{
		if(typeof(wtfast_status.Error) != "undefined"){
			document.getElementById("error_msg").style.display = "";
			document.getElementById("error_msg").innerHTML = "* " + wtfast_status.Error;
		}

		reset_rule_state();
		show_login_page(1);
		show_management_page(0);
	}
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
				var game_list = "";
				var server_list = "";

				if(wtfast_status.Login_status == 1){
					Object.keys(wtfast_status.Game_List).forEach(function(key) {
						game_list += "<"+ wtfast_status.Game_List[key].name + ">" + wtfast_status.Game_List[key].id;
					});
				
					Object.keys(wtfast_status.Server_List).forEach(function(key) {
						server_list += "<"+ wtfast_status.Server_List[key];
					});

					if(wtfast_status.Account_Type != "Advanced")
						reset_proxy2();
				}

				$.ajax({
					url: "/apply.cgi",
					type: "POST",
					data: {
						action_mode: "wtfast_login",
						wtf_username: $("#wtf_username").val(),
						wtf_passwd: $("#wtf_passwd").val(),
						wtf_login: response.Login_status,
						wtf_account_type: response.Account_Type,
						wtf_max_clients: response.Max_Computers,
						wtf_days_left: wtfast_status.Days_Left,
						wtf_server_list: server_list,
						wtf_game_list: game_list,
						wtf_session_hash: wtfast_status.Session_Hash
					},
					success: function( response ) {
					
					}
				});

				setTimeout("checkLoginStatus();", 5000);
		
			},
			error: function(XMLHttpRequest, textStatus){
				if(textStatus == "timeout"){
					document.getElementById("error_msg").style.display = "";
					document.getElementById("error_msg").innerHTML = "* " + "Server does not response.";
				}
			},
			complete: function(){
				setTimeout(function(){
					document.getElementById("loadingIcon_login").style.display = "none";
					document.getElementById("login_button").style.display = "";
				}, 6000);
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
				reset_rule_state();
				update_rulelist(0);
				document.wtfast_form.wtf_rulelist.value = wtfast_rulelist;
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

function return_to_login(){
	show_management_page(0);
	show_login_page(1);
	document.getElementById("loadingIcon_logout").style.display = "none";
	document.getElementById("logout_button").style.display = "";	
}

function open_link(page){
	var tourl;

	if(page == "terms")
		tourl = "https://www.wtfast.com/pages/terms/";
	else if(page == "privacy")
		tourl = "https://www.wtfast.com/pages/privacy/";
	else
		tourl = "https://www.wtfast.com/pages/asus_router/";
	
	link = window.open(tourl, "WTFast","toolbar=yes,location=yes,directories=no,status=yes,menubar=yes,scrollbars=yes,resizable=yes,copyhistory=no,width=640,height=480");
}

function check_value(obj){
	if(obj.value.length == 17){
		change_add_btn(1);
	}
	else
		change_add_btn(0);
}


function change_add_btn(big){//1: big   0: normal
	if(big){
		document.getElementById("addCircle").className = "addCircle_shine";
		document.getElementById("add_btn").style.width = "30px";
		document.getElementById("add_btn").style.height = "30px";
	}
	else{
		document.getElementById("addCircle").className = "addCircle";
		document.getElementById("add_btn").style.width = "24px";
		document.getElementById("add_btn").style.height = "24px";
	}
}

function show_apply(show){//1: show   0: hide
	if(show)
		document.getElementById("applyBtn").style.display = "";
	else
		document.getElementById("applyBtn").style.display = "none";
}

function clean_macerr(){
	document.getElementById("check_mac") ? document.getElementById("check_mac").style.display="none" : true;	
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
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_script" value="restart_wtfast_rule">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wtf_rulelist" value="">
<input type="hidden" name="wtf_enable_game" value="">

<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td align="left" valign="top" >
		<div id="WTFast_login_div" class="Background" style="display:none;">
		<div ><img onclick="go_setting('GameBoost.asp')" align="right" style="width:40px; cursor:pointer;position:absolute;top:10px;right:10px;" title="Back to Game Boost" src="images/wtfast_back.svg" onMouseOver="this.src='images/wtfast_back_hover.svg'" onMouseOut="this.src='images/wtfast_back.svg'" ></div>
		<table width="760px" height="200px" border="0" cellpadding="0" cellspacing="0" id="WTFast_Login" style="margin-top:350px;">
		<tr>
			<td rowspan="2" style="width:40%; vertical-align: top;">
				<div ><img src="/images/wtfast_device.svg" style="width: 310px; margin-left:40px; margin-top:0px;"></div>
			</td>
			<td style="height:75px; vertical-align: top;">
				<div style="margin-left:15px; margin-right:10px; color:#EAE9E9; font-size:13px; font-weight:bolder;">
					<#Game_Boost_desc1#>
				</div>
			</td>
		</tr>
		<tr>
			<td style="vertical-align: top;">
				<div style="font-size:12px; margin-left:15px; margin-right:10px;">
				<p style="color:#EBE8E8; font-size:13px;font-weight:bolder;"><#Game_Boost_Benefit#> :</p>
				<ul type="disc">
					<li style="line-height:15px; margin-left:-25px; margin-top:-10px; color:#949393;"><#Game_Boost_Benefit1#></li>
					<li style="line-height:15px; margin-left:-25px; color:#949393;"><#Game_Boost_Benefit2#></li>
					<li style="line-height:15px; margin-left:-25px; color:#949393;"><#Game_Boost_Benefit3#><br>(Windows, Macintosh, Linux, Mobile, Console game).</li>
					<li style="line-height:15px; margin-left:-25px; color:#949393;"><#Game_Boost_Benefit4#></li>
					<li style="line-height:15px; margin-left:-25px; color:#949393;"><#Game_Boost_Benefit5#></li>
				</ul>
				</div>
			</td>
		</tr>
		</table>
		<table style="margin-top:10px;  width:760px;">
			<tr><td colspan="2" style="color:#EBE8E8; font-size:20px; font-weight:bold; text-align:center;"><div id="Game_Boost_login_div"></div></td></tr>
			<tr>
				<th style="width:254px; height:35px; color:#949393; font-size:14px; text-align:right; padding-right:15px;">E-Mail</th><!--untranslated-->
				<td style="color:#949393; font-size:14px; text-align:left;">
				<input type="text" maxlength="32" class="login_input" id="wtf_username" name="wtf_username" value="" onkeypress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off" >
				<span ><a id="link" href="javascript:open_link('newAccount')" style="margin-left:5px;text-decoration:underline;color:#949393;"><#create_free_acc#></a></span>
				</td>
			</tr>
			<tr>
				<th style="height:35px; color:#949393; font-size:14px; text-align:right; padding-right:15px;"><#PPPConnection_Password_itemname#></th>
				<td style="color:#949393; font-size:14px; text-align:left;">
					<input type="password" maxlength="32" class="login_input" id="wtf_passwd" name="wtf_passwd" value="" autocorrect="off" autocapitalize="off">
				</td>
			</tr>
			<tr>
				<td id="error_msg" colspan="2" style="height:30px; text-align:left; padding-left:268px; color:#EFF21B; font-size:14px;"></td>
			</tr>
			<tr>
				<td colspan="2" style="height:35px;text-align:center;">
					<input id="login_button" class="wtfast_button" style="background-color:#A80000; opacity:0.9;" onclick="wtf_login();" type="button" value="<#CTL_login#>"/>
					<img id="loadingIcon_login" style="display:none;" src="images/InternetScan.gif">
				</td>
			</tr>
		</table>
		<div style="color:#949393; font-size:12px; text-align:center;">* <#note_up_to_date#></div>
		<div>
			<canvas id="fire" style="width:760px;height:430px; display:block;position:absolute; top:491px; z-index:-1;"></canvas>
			<img id="fire_pic" style="position: absolute; top: 499px; z-index: -1; margin-left: 3px;display:none;" src="images/fire.jpg">
		</div>
		<div style="color:#949393; font-size:12px; text-align:right; margin-top: 146px; margin-right: 20px;">Ver. 1.0.0.4_16.0105</div>
		</div><!--WTFast_login_div-->

		<div id="ManagementPage" style="display:none;">
			<table id="Background_Management" class="Background_Management" border="0" cellpadding="4" cellspacing="0">
			<tbody>
				<tr>
					<td valign="top">
						<div>&nbsp;</div>
						<div style="width:730px">
							<table width="710px" border="0">
								<tr>
									<td>
										<div style="float:left; margin-left:20px; margin-top:-10px;"><img src="images/New_ui/game.svg" style="width:77px; height:77px;"></div>
										<div style="color:#EBE8E8; font-size:26px; font-weight:bold; font-family:calibri; float:left; margin-top:12px; margin-left:6px;"><#Game_Boost_management#></div>										
									</td>
									<td align="right">
										<img onclick="go_setting('GameBoost.asp')" style="width:40px; cursor:pointer;position:absolute;margin-left:-20px;margin-top:-30px;" title="Back to Game Boost" src="images/wtfast_back.svg" onMouseOver="this.src='images/wtfast_back_hover.svg'" onMouseOut="this.src='images/wtfast_back.svg'">
									</td>
								</tr>
							</table>
						</div>
						<table id="MainTable1" width="710px" height="160px" border="0" align="left" cellpadding="4" cellspacing="0" style="margin-left:20px;">
							<thead>
								<tr>
									<td colspan="2" align="left" style="color:#FFFFFF; font-size:18px; font-family:calibri;"><#Game_Boost_lnformation#></td>
							  	</tr>
							  	<tr>
									<td colspan="2"><div style="width:100%; height:2px; background-color:#B80000"></div></td>
								</tr>
							</thead>
							<tr style="vertical-align:bottom; color:#949393; font-family:calibri; font-size:16px;">
								<th  align="left" style="width:50%;">
									<div style="float:left;"><#AiDisk_Account#></div>
									<div style="float:left; margin-left:10px;"><img id="logout_button" title="<#t1Logout#>" src="images/New_ui/logout.svg" onMouseOver="this.src='images/New_ui/logout_hover.svg'" onMouseOut="this.src='images/New_ui/logout.svg'"style="width:17px; height:17px; cursor:pointer;" onclick="wtf_logout();"><img id="loadingIcon_logout" style="margin-left:10px; display:none;" src="/images/InternetScan.gif"></div>
								</th>
								<th  align="left"><#account_type#></th>
							</tr>
							<tr style="vertical-align:top; color:#EAE9E9; font-family:calibri; font-size:15px;">
								<td>
							  		<span id="contact_email"></span>
								</td>
								
								<td>
							  		<div id="account_type"></div>
								</td>							
							</tr>
							<tr style="vertical-align:bottom; color:#949393; font-family:calibri; font-size:16px;">
								<th align="left">
									<div style="float:left;"><#max_computers#></div>
									<div style="float:left; margin-left:10px;"><img id="upgrade_button" title="Upgrade" src="images/New_ui/upgrade.svg" onMouseOver="this.src='images/New_ui/upgrade_hover.svg'" onMouseOut="this.src='images/New_ui/upgrade.svg'"style="width:20px; height:20px; cursor:pointer;" onclick="open_link('subscribe');"></div>
								</th>
								<th id="ended_date_th" align="left"><#GB_ended_date#></th>
							</tr>
							<tr style="vertical-align:top; color:#EAE9E9; font-family:calibri; font-size:15px;">
								<td>
							  		<span id="max_computers"></span>
								</td>
								<td id="ended_date_td">
							  		<span id="ended_date"></span><span id="days_title" style="margin-left:10px;">( Days Left: <span id="days_left"></span> )</span> <!--untranslated-->
								</td>
							</tr>			
						</table>

						<table id="MainTable2" width="710px" border="0" align="left" cellpadding="4" cellspacing="0" style="text-align:center; margin-left:20px; margin-top:40px;">
							<thead>
								<tr>
									<td colspan="5" align="left" style="color:#FFFFFF; font-size:18px; font-family:calibri;"><#GB_rulelist#></td>
								</tr>
							</thead>
							<tr style="vertical-align:bottom; color:#949393; font-family:calibri; font-size:16px;">
								<th width="15%"><#WLANConfig11b_WirelessCtrl_button1name#>/<#WLANConfig11b_WirelessCtrl_buttonname#></th>
								<th width="30%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,10);" style="vertical-align:bottom; color:#949393; font-family:calibri; font-size:16px;"><#ParentalCtrl_hwaddr#></th>
								<th width="26%"><#AiProtection_filter_stream1#></th>
								<th width="14%"><#LANHostConfig_x_DDNSServer_itemname#></th>
								<th width="15%"><#list_add_delete#></th>
							</tr>
							<tr>
								<td colspan="5"><div style="width:100%;height:2px;background-color:#B80000"></div></td>
							</tr>
							<tr height="38px">
								<td>
									<div><img id="enable_fig" src="" onMouseOver="EnableMouseOver(this, '');" onMouseOut="EnableMouseOut(this, '');" style="width:25px; height:25px; cursor:pointer;" onclick="change_enable(this);"></div>
								</td>

								<td>
									<input type="text" class="wtfast_input_option" maxlength="17" name="clientmac_x_0" style="width:180px; border-right:0px;" onKeyPress="return validator.isHWAddr(this,event);" onKeyUp="check_value(this);" onKeyDown="clean_macerr();" onClick="hideClients_Block();" autocorrect="off" autocapitalize="off" placeholder="ex: <% nvram_get("lan_hwaddr"); %>">
										<img id="pull_arrow" src="images/arrow-down.gif" style="position:absolute;*margin-left:-3px;*margin-top:1px;" onclick="pullLANList(this);" title="<#select_MAC#>">
										<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div> 
								</td>
								<td>
									<select id="game_list" name="game_list" style="width:160px; background-position:143px;" onchange=""></select>
								</td>
								<td>
									<select data-server_number="1" id="server_1_list" name="server_1_list" onchange="update_server_list_visibilities($(this));"></select>
									<div class="server_2_list_div" style="display: none; margin-top: 10px;">
										<select data-server_number="2" id="server_2_list" name="server_2_list" onchange="update_server_list_visibilities($(this));"></select>	
									</div>
								</td>
															
								<td align="center">
									<div data-server_number="2" id="addCircle" class="addCircle"><img id="add_btn" src="images/New_ui/add.svg" onMouseOver="this.src='images/New_ui/add_hover.svg'" onMouseOut="this.src='images/New_ui/add.svg'" style="width:25px; height:25px; cursor:pointer;" onClick="addRule();"></div>
								</td>
							</tr>
							<tr>
								<td colspan="5"><div style="width:100%;height:1px;background-color:#660000"></div></td>
							</tr>
						</table>
						<table cellspacing="0"><tr><td><div id="wtfast_rulelist_Block"></div></td></tr></table>
						<div style="color:#FFCC00; margin-left: 25px; font-family:calibri; font-size:11px;"><#GB_management_note1#></div>
						<div style="color:#FFCC00; margin-left: 25px; font-family:calibri; font-size:11px;"><#GB_management_note2#></div>
						<div style="color:#FFCC00; margin-left: 25px; font-family:calibri; font-size:11px;"><#GB_management_note3#></div>
						<div id="applyBtn" align="center" style="margin-top:20px; display:none;">
							<input class="wtfast_button" onclick="applyRule()" type="button" value="<#CTL_apply#>"/>
						</div>
					</td>
				</tr>
			</tbody>
			</table>
		</div><!--ManagementPage-->
		</td>
	</tr>
</table>
</td>
<td width="10" align="center" valign="top">&nbsp;</td>
</tr>
</table>
</form>

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
