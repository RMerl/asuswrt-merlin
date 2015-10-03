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
<title><#Web_Title#> - <#EZQoS#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/calendar/jquery-ui.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="/form.js"></script>
<style>
.QISform_wireless{
	width:600px;
	font-size:12px;
	color:#FFFFFF;
	margin-top:10px;
	*margin-left:10px;
}

.QISform_wireless th{
	padding-left:10px;
	*padding-left:30px;
	font-size:12px;
	font-weight:bolder;
	color: #FFFFFF;
	text-align:left; 
}	
	
#priority_panel{
	width:740px;	
	margin-top:55px; 
	margin-left:5px;
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:200;
	background-color:#2B373B;
	box-shadow: 3px 3px 10px #000;
	display:none;
	/*behavior: url(/PIE.htc);*/
}

.description_down{
	margin-top:10px;
	margin-left:10px;
	padding-left:5px;
	font-weight:bold;
	line-height:140%;
	color:#ffffff;	
}
	
#category_list {
	width:99%;
	height:520px;
}

#category_list div{
	width:85%;
	height:70px;
	background-color:#7B7B7B;
	color:white;
	margin:15px auto;
	text-align:center;
	border-radius:20px;
	line-height:75px;
	font-family: Arial, Helvetica, sans-serif;
	font-size:16px;
	font-weight:bold;
	box-shadow: 5px 5px 10px 0px black
}

#category_list div:hover{
	background-color:#66B3FF;
	color:#000;
	cursor:pointer;
	cursor:hand; 
}

.priority{
	text-align:center;
	font-size:16px;
	font-weight:bold;
	color:#FC0;
}

.priority_highest{
	margin:10px 0px -10px 0px;
}

.priority_lowest{
	margin:-10px 0px 20px 0px;
}	

.Quick_Setup_title{
	font-family: Arial, Helvetica, sans-serif;
	font-size:16px;
	font-weight:bold;	
}

.quick_setup{
	width:96px;
	height:96px;
	margin-left:2px;
	cursor:pointer;
}

#Game{	
	background-image:url('images/New_ui/QoS_quick/game.svg');
}

#Game:hover{
	width:118px;
	height:118px;
	background-image:url('images/New_ui/QoS_quick/game_act.svg');
}

#Game_act{
	width:118px;
	height:118px;
	background-image:url('images/New_ui/QoS_quick/game_act.svg');
}

#Media{
	background-image:url('images/New_ui/QoS_quick/media.svg');
}

#Media:hover{
	width:118px;
	height:118px;
	background-image:url('images/New_ui/QoS_quick/media_act.svg');
}

#Media_act{
	width:118px;
	height:118px;
	background-image:url('images/New_ui/QoS_quick/media_act.svg');
}

#Web{
	background-image:url('images/New_ui/QoS_quick/web.svg');
}

#Web:hover{
	width:118px;
	height:118px;
	background-image:url('images/New_ui/QoS_quick/web_act.svg');
}

#Web_act{
	width:118px;
	height:118px;
	background-image:url('images/New_ui/QoS_quick/web_act.svg');
}

#Customize{
	background-image:url('images/New_ui/QoS_quick/customize.svg');
}

#Customize:hover{
	width:118px;
	height:118px;
	background-image:url('images/New_ui/QoS_quick/customize_act.svg');
}

#Customize_act{
	width:118px;
	height:118px;
	background-image:url('images/New_ui/QoS_quick/customize_act.svg');
}
</style>
<script>
// WAN
<% wanlink(); %>
<% first_wanlink(); %>
var wans_dualwan_orig = '<% nvram_get("wans_dualwan"); %>';
var wans_flag = (wans_dualwan_orig.search("none") == -1) ? 1:0;
var dsllink_statusstr = "";
if(wans_flag == 1)	//dual_wan enabled
	dsllink_statusstr = first_wanlink_statusstr();
else
	dsllink_statusstr = wanlink_statusstr();
var dsl_DataRateDown = parseInt("<% nvram_get("dsllog_dataratedown"); %>");
var dsl_DataRateUp = parseInt("<% nvram_get("dsllog_datarateup"); %>");

var bwdpi_app_rulelist = "<% nvram_get("bwdpi_app_rulelist"); %>".replace(/&#60/g, "<");
var category_title = ["", "<#Adaptive_Game#>", "<#Adaptive_Stream#>","<#Adaptive_Message#>", "<#Adaptive_WebSurf#>","<#Adaptive_FileTransfer#>", "<#Adaptive_Others#>"];
var cat_id_array = [[9,20], [8], [4], [0,5,6,15,17], [13,24], [1,3,14], [7,10,11,21,23]];
var ctf_disable = '<% nvram_get("ctf_disable"); %>';
var ctf_fa_mode = '<% nvram_get("ctf_fa_mode"); %>';

function initial(){
	show_menu();
	if(downsize_4m_support || downsize_8m_support)
		document.getElementById("guest_image").parentNode.style.display = "none";
	
	if(document.form.qos_enable_orig.value == 1){
		if(document.form.qos_type.value != 2){
			document.getElementById('upload_tr').style.display = "";
			document.getElementById('download_tr').style.display = "";
		}
		else{
			document.getElementById('upload_tr').style.display = "none";
			document.getElementById('download_tr').style.display = "none";
		}

		document.getElementById('qos_type_tr').style.display = "";
		if(bwdpi_support){
			document.getElementById('int_type').style.display = "";
			document.getElementById('int_type_link').style.display = "";
			change_qos_type(document.form.qos_type_orig.value);
		}
		else
			show_settings("NonAdaptive");
	}
	else{	//qos disabled
		document.getElementById('settingSelection').style.display = "none";		
		document.getElementById('upload_tr').style.display = "none";
		document.getElementById('download_tr').style.display = "none";
		document.getElementById('qos_type_tr').style.display = "none";
		if(bwdpi_support){			
			show_settings("NonAdaptive");
		}	
			
	}

	if(bwdpi_support){
		document.getElementById('content_title').innerHTML = "<#menu5_3_2#> - <#Adaptive_QoS_Conf#>";
		if(document.form.qos_enable.value == 1){
			if(document.form.qos_type.value == 0){		//Traditional Type				
				add_option(document.getElementById("settingSelection"), '<#qos_user_rules#>', 3, 0);
				add_option(document.getElementById("settingSelection"), '<#qos_user_prio#>', 4, 0);
			}
			else if(document.form.qos_type.value == 2){		//Bandwidth Limiter
				add_option(document.getElementById("settingSelection"), "Bandwidth Limiter", 5, 0);
			}
			else{		//Adaptive Type or else
				document.getElementById('settingSelection').style.display = "none";	
			}
		}
		else{		// hide select option if qos disable
			document.getElementById('settingSelection').style.display = "none";	
		}
	}
	else{
		if(document.form.qos_type.value == 2){		//Bandwidth Limiter
			add_option(document.getElementById("settingSelection"), "Bandwidth Limiter", 5, 0);
		}
		else{		//Traditional Type			
			add_option(document.getElementById("settingSelection"), '<#qos_user_rules#>', 3, 0);
			add_option(document.getElementById("settingSelection"), '<#qos_user_prio#>', 4, 0);
		}
		document.getElementById('content_title').innerHTML = "<#Menu_TrafficManager#> - <#menu5_3_2#>";		
		document.getElementById('function_int_desc').style.display = "none";				
	}
	
	init_changeScale();
	//addOnlineHelp(document.getElementById("faq"), ["ASUSWRT", "QoS"]);	
}

function init_changeScale(){
	var upload = document.form.qos_obw.value;
	var download = document.form.qos_ibw.value;
	
	if(based_modelid == "DSL-AC68U"		//MODELDEP: DSL-AC68U
	&& wans_dualwan_orig.search("dsl") >= 0 && dsllink_statusstr == "Connected"
	&& ((upload == "" || upload == "0") && (download == "" || download == "0"))){

		document.form.obw.value = dsl_DataRateUp/1024;
		document.form.ibw.value = dsl_DataRateDown/1024;
	}
	else{

		document.form.obw.value = upload/1024;
		document.form.ibw.value = download/1024;
	}
}

function switchPage(page){
	if(page == "1")	
		location.href = "/QoS_EZQoS.asp";
	else if(page == "2")	
		location.href = "/AdaptiveQoS_Adaptive.asp";	//remove 2015.07
	else if(page == "3")	
		location.href = "/Advanced_QOSUserRules_Content.asp";
	else if(page == "4")	
		location.href = "/Advanced_QOSUserPrio_Content.asp";
	else if(page == "5")	
		location.href = "/Bandwidth_Limiter.asp";
	else
		return false;
}

function submitQoS(){
	if(document.form.qos_enable.value == 0 && document.form.qos_enable_orig.value == 0){
		return false;
	}

	if(document.form.qos_enable.value == 1){
		if(document.form.qos_type.value != 2){
			if(document.form.obw.value.length == 0){	//To check field is empty
				alert("<#JS_fieldblank#>");
				document.form.obw.focus();
				document.form.obw.select();
				return;
			}
			else if(document.form.qos_type.value == 0 && document.form.obw.value == 0){		// To check field is 0 && Traditional QoS
				alert("Upload Bandwidth can not be 0");
				document.form.obw.focus();
				document.form.obw.select();
				return;
			
			}
			else if(document.form.obw.value.split(".").length > 2){		//To check more than two point symbol
				alert("The format of field of upload bandwidth is invalid");
				document.form.obw.focus();
				document.form.obw.select();
				return;	
			}
			
			if(document.form.ibw.value.length == 0){
				alert("<#JS_fieldblank#>");
				document.form.ibw.focus();
				document.form.ibw.select();
				return;
			}
			else if(document.form.qos_type.value == 0 && document.form.ibw.value == 0){		// To check field is 0 && Traditional QoS
				alert("Download Bandwidth can not be 0");
				document.form.ibw.focus();
				document.form.ibw.select();
				return;
			}
			else if(document.form.ibw.value.split(".").length > 2){
				alert("The format of field of download bandwidth is invalid");
				document.form.ibw.focus();
				document.form.ibw.select();
				return;	
			}
			
			document.form.qos_obw.disabled = false;
			document.form.qos_ibw.disabled = false;
			document.form.qos_obw.value = document.form.obw.value*1024;
			document.form.qos_ibw.value = document.form.ibw.value*1024;
			
			if(document.form.qos_type.value == 1){	//Adaptive QoS
				if(document.getElementById("Game_act") || document.getElementById("Media_act") || document.getElementById("Web_act") || document.getElementById("Customize_act")){
					document.form.bwdpi_app_rulelist.disabled = "";
					if(document.getElementById("Game_act")) 
						document.form.bwdpi_app_rulelist.value = "9,20<8<4<0,5,6,15,17<13,24<1,3,14<7,10,11,21,23<<game";
					else if(document.getElementById("Media_act")) 
						document.form.bwdpi_app_rulelist.value = "9,20<4<0,5,6,15,17<8<13,24<1,3,14<7,10,11,21,23<<media";
					else if(document.getElementById("Web_act")) 
						document.form.bwdpi_app_rulelist.value = "9,20<13,24<4<0,5,6,15,17<8<1,3,14<7,10,11,21,23<<web";
					else	
						document.form.bwdpi_app_rulelist.value = document.PriorityForm.bwdpi_app_rulelist_edit.value;	
				}
				else{
					alert("You have not selected QoS priority mode.");	
					return false;
				}							
			}
		}
	}	
	
	if(ctf_disable == 1){
		document.form.action_script.value = "restart_qos;restart_firewall";
	}
	else{
		if(ctf_fa_mode == "2"){
			FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
		}
		else{
			if(document.form.qos_type.value == 0)
				FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
			else{				
				document.form.action_script.value = "restart_qos;restart_firewall";
			}					
		}
	}
	
	if(document.form.qos_type.value == 2){
		if((document.form.qos_enable.value != document.form.qos_enable_orig.value) && document.form.qos_enable.value == 0){
			document.form.action_script.value = "restart_qos;restart_firewall";
			showLoading();
			document.form.submit();		
		}
		else{
			location.href = "Bandwidth_Limiter.asp";
		}
	}
	else{
		showLoading();
		document.form.submit();		
	}
}

function change_qos_type(value){
	if(value == 0){		//Traditional QoS
		document.getElementById('int_type').checked = false;
		document.getElementById('trad_type').checked = true;
		document.getElementById('bw_limit_type').checked = false;
		document.getElementById('upload_tr').style.display = "";
		document.getElementById('download_tr').style.display = "";
		if(document.form.qos_type_orig.value == 0 && document.form.qos_enable_orig.value != 0){
			document.form.action_script.value = "restart_qos;restart_firewall";
		}	
		else{
			document.form.action_script.value = "reboot";
			document.form.next_page.value = "Advanced_QOSUserRules_Content.asp";
		}
		show_settings("NonAdaptive");
		$("#hint_zero").hide();
	}	
	else if(value == 1){		//Adaptive QoS
		document.getElementById('int_type').checked = true;
		document.getElementById('trad_type').checked = false;
		document.getElementById('bw_limit_type').checked = false;
		document.getElementById('upload_tr').style.display = "";
		document.getElementById('download_tr').style.display = "";
		if(document.form.qos_type_orig.value == 1 && document.form.qos_enable_orig.value != 0)
			document.form.action_script.value = "restart_qos;restart_firewall";
		else{
			document.form.action_script.value = "reboot";
			document.form.next_page.value = "QoS_EZQoS.asp";
		}
		
		show_settings("Adaptive_quick");
		$("#hint_zero").show();
	}
	else{		// Bandwidth Limiter
		document.getElementById('int_type').checked = false;
		document.getElementById('trad_type').checked = false;
		document.getElementById('bw_limit_type').checked = true;
		document.getElementById('upload_tr').style.display = "none";
		document.getElementById('download_tr').style.display = "none";
		if(document.form.qos_type_orig.value == 2 && document.form.qos_enable_orig.value != 0)
			document.form.action_script.value = "restart_qos;restart_firewall";
		else{
			document.form.action_script.value = "reboot";
			document.form.next_page.value = "Bandwidth_Limiter.asp";
		}
		show_settings("NonAdaptive");
	}

	document.form.qos_type.value = value;
}

function show_settings(flag){
	
	if(!bwdpi_support){
		document.getElementById("quick_setup_desc").style.display = "none";
		document.getElementById("quick_setup_table").style.display = "none";
	}
	else{
		if(flag == "NonAdaptive"){
			document.getElementById("quick_setup_desc").style.display = "none";
			document.getElementById("quick_setup_table").style.display = "none";		
		}
		else if(flag == "Adaptive_category"){
			document.getElementById("quick_setup_desc").style.display = "none";
			document.getElementById("quick_setup_table").style.display = "none";	
		}
		else{	//Adaptive_quick
			document.getElementById("quick_setup_desc").style.display = "";
			document.getElementById("quick_setup_table").style.display = "";	
			check_actived();
		}
	}
}

function check_actived(){
	
	if(document.getElementById("Game_act")) document.getElementById("Game_act").id = "Game";
	if(document.getElementById("Media_act")) document.getElementById("Media_act").id = "Media";
	if(document.getElementById("Web_act")) document.getElementById("Web_act").id = "Web";
	if(document.getElementById("Customize_act")) document.getElementById("Customize_act").id = "Customize";	
	
	if(bwdpi_app_rulelist == "9,20<8<4<0,5,6,15,17<13,24<1,3,14<7,10,11,21,23<<"){	//default APP priority of QoS
		return;
	}
	
	if(bwdpi_app_rulelist == "9,20<8<4<0,5,6,15,17<13,24<1,3,14<7,10,11,21,23<<game"){
		if(document.getElementById("Game"))		document.getElementById("Game").id = "Game_act";		
	}	
	else if(bwdpi_app_rulelist == "9,20<4<0,5,6,15,17<8<13,24<1,3,14<7,10,11,21,23<<media"){
		if(document.getElementById("Media"))	document.getElementById("Media").id = "Media_act";
	}	
	else if(bwdpi_app_rulelist == "9,20<13,24<4<0,5,6,15,17<8<1,3,14<7,10,11,21,23<<web"){
		if(document.getElementById("Web"))		document.getElementById("Web").id = "Web_act";
	}	
	else{		
		if(document.getElementById("Customize"))	document.getElementById("Customize").id = "Customize_act";		
	}	
}

function clickEvent(obj){
	var stitle;	
	
	if(document.getElementById("Game_act")) document.getElementById("Game_act").id = "Game";
	if(document.getElementById("Media_act")) document.getElementById("Media_act").id = "Media";
	if(document.getElementById("Web_act")) document.getElementById("Web_act").id = "Web";
	if(document.getElementById("Customize_act")) document.getElementById("Customize_act").id = "Customize";
	if(obj.id.indexOf("Game") >= 0){
		document.getElementById("Game").id = "Game_act";
		stitle = "Game";
	}
	else if(obj.id.indexOf("Media") >= 0){
		obj.id = "Media_act";		
		stitle = "Media";
	}
	else if(obj.id.indexOf("Web") >= 0){		
		obj.id = "Web_act";
		stitle = "Web";
	}
	else if(obj.id.indexOf("Customize") >= 0){
		obj.id = "Customize_act";
		show_settings("NonAdaptive");
		stitle = "Customize";
	}
	else
		alert("mouse over on wrong place!");			
}

function set_priority(flag){
	if(flag == 'on'){
		$("#priority_panel").fadeIn(300);		
		gen_category_block();
		register_event();		
	}
}

function regen_priority(obj){
	var priority_array = obj.children;
	var rule_temp = "";
	rule_temp += "9,20<";		//always at first priority
	for(i=0;i<priority_array.length;i++){
		rule_temp += cat_id_array[priority_array[i].id] + "<";
	}
	
	rule_temp += "<";
	bwdpi_app_rulelist = rule_temp+"customize";
}

function gen_category_block(){
	bwdpi_app_rulelist = bwdpi_app_rulelist.replace(/&#60/g, "<");	
	if(bwdpi_app_rulelist == ""){
		//customize default 
		bwdpi_app_rulelist = "9,20<8<4<0,5,6,15,17<13,24<1,3,14<7,10,11,21,23<<";
	}	
	var bwdpi_app_rulelist_row = bwdpi_app_rulelist.split("<");	
	var index = 0;
	var code = "";

	for(i=1;i<bwdpi_app_rulelist_row.length-2;i++){
		for(j=1;j<cat_id_array.length;j++){
			if(cat_id_array[j] == bwdpi_app_rulelist_row[i]){
				index = j;
				break;			
			}
		}
		
		code += '<div id='+ index +'>'+ category_title[index] +'</div>';		
	}
	
	document.getElementById('category_list').innerHTML = code;
	register_overHint();
}

function cancel_priority_panel() {	
	bwdpi_app_rulelist = "<% nvram_get("bwdpi_app_rulelist"); %>".replace(/&#60/g, "<");
	$("#priority_panel").fadeOut(300);	
	setTimeout("change_qos_type(document.form.qos_type.value);", 300);
}

function save_priority(){
	regen_priority(document.getElementById("category_list"));
	document.PriorityForm.bwdpi_app_rulelist_edit.value = bwdpi_app_rulelist;	
	$("#priority_panel").fadeOut(300);	
	setTimeout("change_qos_type(document.form.qos_type.value);", 300);
}

function register_event(){
	$(function() {
		$("#category_list").sortable({
			stop: function(event, ui){
				regen_priority(this);		
			}
		});
		$("#category_list").disableSelection();		
	});
}

function register_overHint(){
	document.getElementById('1').onmouseover = function(){overHint(91);}
	document.getElementById('1').onmouseout = function(){nd();}
	document.getElementById('2').onmouseover = function(){overHint(92);}
	document.getElementById('2').onmouseout = function(){nd();}
	document.getElementById('3').onmouseover = function(){overHint(93);}
	document.getElementById('3').onmouseout = function(){nd();}
	document.getElementById('4').onmouseover = function(){overHint(94);}
	document.getElementById('4').onmouseout = function(){nd();}
	document.getElementById('5').onmouseover = function(){overHint(95);}
	document.getElementById('5').onmouseout = function(){nd();}
	document.getElementById('6').onmouseover = function(){overHint(96);}
	document.getElementById('6').onmouseout = function(){nd();}
} 
</script>
</head>	
<body onload="initial();" id="body_id" onunload="unload_body();" onClick="">	
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<table id="main_table" class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="17">&nbsp;</td>	
		<!--=====Beginning of Main Menu=====-->
		<td valign="top" width="202">
		  <div id="mainMenu"></div>
		  <div id="subMenu"></div>
		</td>
	
		<td valign="top">
			<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
			<div id="priority_panel">
				<table class="QISform_wireless" border=0 align="center" cellpadding="5" cellspacing="0">
				<form method="post" name="PriorityForm" action="/start_apply.htm" target="hidden_frame">
					<input type="hidden" name="current_page" value="QoS_EZQoS.asp">
					<input type="hidden" name="next_page" value="QoS_EZQoS.asp">
					<input type="hidden" name="modified" value="0">
					<input type="hidden" name="flag" value="background">
					<input type="hidden" name="action_mode" value="apply">
					<input type="hidden" name="action_script" value="saveNvram">
					<input type="hidden" name="action_wait" value="1">
					<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
					<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
					<input type="hidden" name="bwdpi_app_rulelist_edit" value="<% nvram_get("bwdpi_app_rulelist"); %>">
					<tr>
						<div class="description_down"><#Adaptive_QoS#> - <#Adaptive_QoS#></div>
					</tr>
					<tr>
						<div style="margin-left:30px; margin-top:10px;">
							<div class="formfontdesc" style="line-height:20px;font-size:14px;"><#Adaptive_QoS_desc#></div>
						</div>
						<div style="margin:5px;*margin-left:-5px;"><img style="width: 730px; height: 2px;" src="/images/New_ui/export/line_export.png"></div>
					</tr>				
					<tr>
						<td valign="top">
							<table width="700px" border="0" cellpadding="4" cellspacing="0">
								<tbody>
								<tr>
									<td valign="top">
										<table id="category_table" width="100%">							
										<tr>
											<td colspan="2">
												<div class="priority priority_highest"><#Highest#></div>
											</td>
										</tr>
										<tr>
											<td colspan="2">	
												<div id="category_list"></div>
											</td>
										</tr>
										<tr>
											<td colspan="2">
												<div class="priority priority_lowest"><#Lowest#></div>	
											</td>
										</tr>
									</table>
									</td>
								</tr>						
								</tbody>						
							</table>
							<div style="margin-top:5px;width:100%;text-align:center;">
								<input class="button_gen" id="btn_cancel_priority" type="button" onclick="cancel_priority_panel();" value="<#CTL_Cancel#>">
								<input class="button_gen" type="button" onclick="save_priority();" value="<#CTL_onlysave#>">	
							</div>					
						</td>
					</tr>
				</form>
				</table>
			</div>

			<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
			<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
			<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
			<input type="hidden" name="current_page" value="/QoS_EZQoS.asp">
			<input type="hidden" name="next_page" value="/QoS_EZQoS.asp">
			<input type="hidden" name="group_id" value="">
			<input type="hidden" name="action_mode" value="apply">
			<input type="hidden" name="action_script" value="">
			<input type="hidden" name="action_wait" value="5">
			<input type="hidden" name="flag" value="">
			<input type="hidden" name="qos_enable" value="<% nvram_get("qos_enable"); %>">
			<input type="hidden" name="qos_enable_orig" value="<% nvram_get("qos_enable"); %>">
			<input type="hidden" name="qos_type" value="<% nvram_get("qos_type"); %>">
			<input type="hidden" name="qos_type_orig" value="<% nvram_get("qos_type"); %>">
			<input type="hidden" name="qos_obw" value="<% nvram_get("qos_obw"); %>" disabled>
			<input type="hidden" name="qos_ibw" value="<% nvram_get("qos_ibw"); %>" disabled>
			<input type="hidden" name="bwdpi_app_rulelist" value="<% nvram_get("bwdpi_app_rulelist"); %>" disabled>

			<table width="95%" border="0" align="left" cellpadding="0" cellspacing="0" class="FormTitle" id="FormTitle" style="height:820px;">
				<tr>
					<td bgcolor="#4D595D" valign="top">
						<table width="760px" border="0" cellpadding="4" cellspacing="0">
							<tr>
								<td bgcolor="#4D595D" valign="top">
									<table width="100%">
										<tr style="height:30px;">
											<td  class="formfonttitle" align="left">								
												<div id="content_title"></div>
											</td>
											<td align="right" >
												<div>
													<select id="settingSelection" onchange="switchPage(this.options[this.selectedIndex].value)" class="input_option">
														<option value="1"><#Adaptive_QoS_Conf#></option>										
													</select>	    
												</div>
											</td>	
										</tr>
									</table>	
								</td>
							</tr>						
							<tr>
								<td height="5" bgcolor="#4D595D" valign="top"><img src="images/New_ui/export/line_export.png" /></td>
							</tr>
							<tr>
								<td height="30" align="left" valign="top" bgcolor="#4D595D">
									<div>
										<table style="width:700px;margin-left:25px;">
											<tr>
												<td style="width:130px">
													<div id="guest_image" style="background: url(images/New_ui/QoS.png);width: 143px;height: 87px;"></div>
												</td>
												<td>&nbsp&nbsp</td>
												<td style="font-style: italic;font-size: 14px;">
													<div id="function_desc" class="formfontdesc" style="line-height:20px;">
														<#EzQoS_desc#>
														<ul>
															<li id="function_int_desc"><#EzQoS_desc_Adaptive#></li>
															<li><#EzQoS_desc_Traditional#></li>
															<li><span style="font-size:14px;font-weight:bolder">Bandwidth Limiter</span> helps you to control download and upload max speed of your cleint devices.</li><!--untranslated string--> 
														</ul>
														<!--#EzQoS_desc_note#-->
														To enable QoS function, click the QoS slide switch and fill in the upload and download.<!--unstranlated string-->
													</div>
													<div class="formfontdesc">
														<a id="faq" href="http://www.asus.com/us/support/FAQ/1008718/" target="_blank" style="text-decoration:underline;">QoS FAQ</a>
													</div>
												</td>
											</tr>
										</table>
									</div>
								</td>
							</tr>							
							<tr>
								<td valign="top">
									<table style="margin-left:3px;" width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
										<tr>
											<th><#Enable_QoS#></th> <!--untranslated string-->
											<td colspan="2">
												<div class="left" style="width:94px; float:left; cursor:pointer;" id="radio_qos_enable"></div>
													<script type="text/javascript">
														$('#radio_qos_enable').iphoneSwitch('<% nvram_get("qos_enable"); %>', 
															 function() {
																document.form.qos_enable.value = 1;
																if(document.form.qos_enable_orig.value != 1){
																	if(document.getElementById('int_type').checked == true && bwdpi_support)
																		document.form.next_page.value = "QoS_EZQoS.asp";
																	else if(document.getElementById('bw_limit_type').checked)		//Bandwidth Limiter
																		document.form.next_page.value = "Bandwidth_Limiter.asp";
																	else
																		document.form.next_page.value = "Advanced_QOSUserRules_Content.asp";
																}																
																
																if(document.form.qos_type.value != 2){
																	document.getElementById('upload_tr').style.display = "";
																	document.getElementById('download_tr').style.display = "";
																}
																else{
																	document.getElementById('upload_tr').style.display = "none";
																	document.getElementById('download_tr').style.display = "none";
																}

																document.getElementById('qos_type_tr').style.display = "";
																if(bwdpi_support){
																	document.getElementById('int_type').style.display = "";
																	document.getElementById('int_type_link').style.display = "";
																	document.getElementById('qos_enable_hint').style.display = "";
																	change_qos_type(document.form.qos_type_orig.value);
																}	
															 },
															 function() {
																document.form.qos_enable.value = 0;																
																document.getElementById('upload_tr').style.display = "none";
																document.getElementById('download_tr').style.display = "none";
																document.getElementById('qos_type_tr').style.display = "none";
	
																if(bwdpi_support){																	
																	
																	document.getElementById('qos_enable_hint').style.display = "none";
																	show_settings("NonAdaptive");
																}	
															 }
														);
													</script>			
												<div id="qos_enable_hint" style="color:#FC0;margin:5px 0px 0px 100px;display:none">Enabling QoS may take several minutes.<!--#Adaptive_note#--></div><!--untranslated string-->
											</td>
										</tr>
										<tr id="qos_type_tr" style="display:none">
											<th><#QoS_Type#></th>
											<td colspan="2">
												<input id="int_type" value="1" onClick="change_qos_type(this.value);" style="display:none;" type="radio" <% nvram_match("qos_type", "1","checked"); %>><a id="int_type_link" class="hintstyle" style="display:none;" href="javascript:void(0);" onClick="openHint(20, 6);"><label for="int_type"><#Adaptive_QoS#></label></a>
												<input id="trad_type" value="0" onClick="change_qos_type(this.value);" type="radio" <% nvram_match("qos_type", "0","checked"); %>><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 7);"><label for="trad_type"><#EzQoS_type_traditional#></label></a>
												<input id="bw_limit_type" value="2" onClick="change_qos_type(this.value);" type="radio" <% nvram_match("qos_type", "2","checked"); %>><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 8)"><label for="bw_limit_type"><#Bandwidth_Limiter#></label></a>
											</td>
										</tr>										
										<tr id="upload_tr">
											<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 2);"><#upload_bandwidth#></a></th>
											<td>
												<input type="text" maxlength="10" id="obw" name="obw" onKeyPress="return validator.isNumberFloat(this,event);" class="input_15_table" value="" autocorrect="off" autocapitalize="off">
												<label style="margin-left:5px;">Mb/s</label>
											</td>
											<td rowspan="2" style="width:250px;">
												<div>
													<ul style="padding:0 10px;margin:5px 0;">
														<li>Get the bandwidth information from ISP or go to <a href="http://speedtest.net" target="_blank" style="text-decoration:underline;color:#FC0">Speedtest</a> to check bandwidth</li>
														<li id="hint_zero">The default is 0, which means unlimited bandwidth</li>
													</ul>
												</div>
												
											</td>
										</tr>											
										<tr id="download_tr">
											<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 2);"><#download_bandwidth#></a></th>
											<td>
												<input type="text" maxlength="10" id="ibw" name="ibw" onKeyPress="return validator.isNumberFloat(this,event);" class="input_15_table" value="" autocorrect="off" autocapitalize="off">
												<label style="margin-left:5px;">Mb/s</label>
											</td>
										</tr>																
									</table>
								</td>
							</tr>
						</table>
						
						<table id="quick_setup_desc" width="98%" border="0" style="margin-top:5px;margin-left:5px;display:none;">
							<tr>
								<td height="30" align="left" valign="top" bgcolor="#4D595D">																		
									<div class="formfontdesc" style="line-height:20px;font-size:14px;">Please select priority mode depending on your networking environment. You can also choice customize mode to prioritize app category.</div>	<!-- untranslated -->
								</td>								
							</tr>
						</table>	
						
						<table id="quick_setup_table" width="100%" border="0" align="center" style="display:none;">
							<tr height="130px">
								<td width="10px"></td>
								<td width="130px" align="center">
									<div id="Game" class="quick_setup" onclick="clickEvent(this);" title="This mode is suitable for playing internet game and boost your gaming bandwidth."><a href=""></a></div>	<!-- untranslated -->								
								</td>
								<td width="50px"></td>
								<td width="130px" align="center">
									<div id="Media" class="quick_setup" onclick="clickEvent(this);" title="This mode is suitable for playing video streaming and make sure your viewing experience."><a href=""></a></div>	<!-- untranslated -->
								</td>
								<td width="50px"></td>
								<td width="130px" align="center">
									<div id="Web" class="quick_setup" onclick="clickEvent(this);" title="This mode is suitable for general web browsing and avoid to networking latency whileﬁfile transferring."><a href=""></a></div>	<!-- untranslated -->
								</td>	
								<td width="50px"></td>
								<td width="130px" align="center">
									<div id="Customize" class="quick_setup" onclick="clickEvent(this);set_priority('on');" title="Manually prioritize apps category depending on your preference."><a href=""></a></div>	<!-- untranslated -->
								</td>
								<td width="20px"></td>
							</tr>
							<tr height="40px" align="center">
								<td width="10px"></td>
								<td class="Quick_Setup_title" align="center">Game</td>	<!-- untranslated -->
								<td width="50px"></td>
								<td class="Quick_Setup_title" align="center">Media Streaming</td>	<!-- untranslated -->
								<td width="50px"></td>
								<td class="Quick_Setup_title" align="center">Web Surfing</td>	<!-- untranslated -->
								<td width="50px"></td>
								<td class="Quick_Setup_title" align="center">Customize</td>	<!-- untranslated -->
								<td width="20px"></td>
							</tr>						
							<tr height="40">
							</tr>
						</table>												
						
						<table width="100%">
							<tr>
								<td height="50" >
									<div style=" *width:136px;margin-left:300px;" class="titlebtn" align="center" onClick="submitQoS()"><span><#CTL_apply#></span></div>
								</td>
							</tr>					
						</table>
					</td>  
				</tr>
			</table>
		<!--===================================End of Main Content===========================================-->
		</td>	
	</tr>
</table>
</form>
<div id="footer"></div>
</body>
</html>
