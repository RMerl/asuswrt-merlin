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
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/calendar/jquery-ui.js"></script> 
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<style>
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

.quick_setup{
	width:120px;
	height:120px;
	margin-left:67px;
	cursor:pointer;
}

#Game{	
	background-image:url('/images/New_ui/QoS_quick/game.svg');
}

#Game:hover{	
	background-image:url('/images/New_ui/QoS_quick/game_act.svg');
}

#Game_act{
	background-image:url('/images/New_ui/QoS_quick/game_act.svg');
}

#Media{
	background-image:url('/images/New_ui/QoS_quick/media.svg');
}

#Media:hover{
	background-image:url('/images/New_ui/QoS_quick/media_act.svg');
}

#Media_act{
	background-image:url('/images/New_ui/QoS_quick/media_act.svg');
}

#Web{
	background-image:url('/images/New_ui/QoS_quick/web.svg');
}

#Web:hover{
	background-image:url('/images/New_ui/QoS_quick/web_act.svg');
}

#Web_act{
	background-image:url('/images/New_ui/QoS_quick/web_act.svg');
}

#Customize{
	background-image:url('/images/New_ui/QoS_quick/customize.svg');
}

#Customize:hover{
	background-image:url('/images/New_ui/QoS_quick/customize_act.svg');
}

#Customize_act{
	background-image:url('/images/New_ui/QoS_quick/customize_act.svg');
}

.actived_check{
	width:40px;
	height:40px;
	margin-left:150px;
	margin-top:-35px;
	background-image:url('/images/New_ui/QoS_quick/actived.svg');
}

.Quick_Setup_tile{
	font-family: Arial, Helvetica, sans-serif;
	font-size:18px;
	font-weight:bold;
}
</style>
<script>
var bwdpi_app_rulelist = "<% nvram_get("bwdpi_app_rulelist"); %>".replace(/&#60/g, "<");
var category_title = ["", "<#Adaptive_Game#>", "<#Adaptive_Stream#>","<#Adaptive_Message#>", "<#Adaptive_WebSurf#>","<#Adaptive_FileTransfer#>", "<#Adaptive_Others#>"];
var cat_id_array = [[9,20], [8], [4], [0,5,6,15,17], [13,24], [1,3,14], [7,10,11,21,23]];

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

function initial(){
	show_menu();
	show_settings(0);	
}

function show_settings(flag){
	
	if(flag != 0){
		document.getElementById("quick_setup_desc").style.display = "none";
		document.getElementById("quick_setup_table").style.display = "none";
		document.getElementById("category_table").style.display = "";
		gen_category_block();
		register_event();
	}
	else{
		document.getElementById("quick_setup_desc").style.display = "";
		document.getElementById("quick_setup_table").style.display = "";
		document.getElementById("category_table").style.display = "none";
		check_actived();
	}	
}

function check_actived(){

	if(bwdpi_app_rulelist == "9,20<8<4<0,5,6,15,17<13,24<1,3,14<7,10,11,21,23<<"){
		document.getElementById("Game").id = "Game_act";
		document.getElementById("checked_Game").style.display = "";
	}	
	else if(bwdpi_app_rulelist == "9,20<4<0,5,6,15,17<8<13,24<1,3,14<7,10,11,21,23<<"){
		document.getElementById("Media").id = "Media_act";
		document.getElementById("checked_Media").style.display = "";
	}	
	else if(bwdpi_app_rulelist == "9,20<13,24<4<0,5,6,15,17<8<1,3,14<7,10,11,21,23<<"){
		document.getElementById("Web").id = "Web_act";
		document.getElementById("checked_Web").style.display = "";
	}	
	else{
		document.getElementById("Customize").id = "Customize_act";
		document.getElementById("checked_Customize").style.display = "";
	}	
}

function switchPage(page){
	if(page == "1")	
		location.href = "/QoS_EZQoS.asp";
	else if(page == "2")	
		location.href = "/AdaptiveQoS_Adaptive.asp";
	else
		return false;
}

function gen_category_block(){
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

function regen_priority(obj){
	var priority_array = obj.children;
	var rule_temp = "";
	rule_temp += "9,20<";		//always at first priority
	for(i=0;i<priority_array.length;i++){
		rule_temp += cat_id_array[priority_array[i].id] + "<";
	}
	
	rule_temp += "<";
	bwdpi_app_rulelist = rule_temp;
}

function applyRule(){
	if(document.getElementById("Game_act")) 
		document.form.bwdpi_app_rulelist.value = "9,20<8<4<0,5,6,15,17<13,24<1,3,14<7,10,11,21,23<<";
	else if(document.getElementById("Media_act")) 
		document.form.bwdpi_app_rulelist.value = "9,20<4<0,5,6,15,17<8<13,24<1,3,14<7,10,11,21,23<<";
	else if(document.getElementById("Web_act")) 
		document.form.bwdpi_app_rulelist.value = "9,20<13,24<4<0,5,6,15,17<8<1,3,14<7,10,11,21,23<<";
	else	
		document.form.bwdpi_app_rulelist.value = bwdpi_app_rulelist;
	document.form.submit();
}

var avoidkey;
var lastName;
function clickEvent(obj){
	var icon;
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
		//obj.id = "Customize_act";
		show_settings(1);
		stitle = "Customize";
	}
	else
		alert("mouse over on wrong place!");
	
	avoidkey = icon;	
	lastName = icon;
}
</script>
</head>

<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="/AdaptiveQoS_Adaptive.asp">
<input type="hidden" name="next_page" value="/AdaptiveQoS_Adaptive.asp">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_qos;restart_firewall">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="flag" value="">
<input type="hidden" name="qos_enable" value="<% nvram_get("qos_enable"); %>">
<input type="hidden" name="qos_enable_orig" value="<% nvram_get("qos_enable"); %>">
<input type="hidden" name="bwdpi_app_rulelist" value="">
<table class="content" align="center" cellpadding="0" cellspacing="0">
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
			<table width="95%" border="0" align="left" cellpadding="0" cellspacing="0" class="FormTitle" id="FormTitle">
				<tr>
					<td bgcolor="#4D595D" valign="top">
						<table width="760px" border="0" cellpadding="4" cellspacing="0">
							<tr>
								<td bgcolor="#4D595D" valign="top">
									<table width="100%">
										<tr>
											<td class="formfonttitle" align="left">								
												<div><#Adaptive_QoS#> - <#Adaptive_QoS#></div>
											</td>
											<td align="right">
												<div>
													<select onchange="switchPage(this.options[this.selectedIndex].value)" class="input_option">
														<option value="1" ><#Adaptive_QoS_Conf#></option>
														<option value="2" selected><#Adaptive_QoS#></option>														
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
						</table>
						
						<table id="quick_setup_desc" width="100%" border="0">
							<tr>
								<td height="30" align="left" valign="top" bgcolor="#4D595D">																		
									<div class="formfontdesc" style="line-height:20px;font-size:14px;">Please select priority mode depending on your networking environment. You can also choice customize mode to prioritize app category.</div>
								</td>								
							</tr>
						</table>	
						
						<table id="quick_setup_table" width="70%" border="0" align="center">							
							<tr  height="40">
							</tr>	
							<tr>
								<td>
									<div id="Game" class="quick_setup" onclick="clickEvent(this);" title="This mode is suitable for playing internet game and boost your gaming bandwidth."><a href=""></a></div>
									<div id="checked_Game" class="actived_check" style="display:none;"></div>
								</td>	
								<td>
									<div id="Media" class="quick_setup" onclick="clickEvent(this);" title="This mode is suitable for playing video streaming and make sure your viewing experience."><a href=""></a></div>
									<div id="checked_Media" class="actived_check" style="display:none;"></div>
								</td>
							</tr>
							<tr height="40px" align="center">
								<td class="Quick_Setup_tile">GAME</td>	
								<td class="Quick_Setup_tile">Media Streaming</td>
							</tr>
							<tr height="40px">
							</tr>	
							<tr>
								<td>
									<div id="Web" class="quick_setup" onclick="clickEvent(this);" title="This mode is suitable for general web browsing and avoid to networking latency whileﬁfile transferring."><a href=""></a></div>
									<div id="checked_Web" class="actived_check" style="display:none;"></div>
								</td>	
								<td>
									<div id="Customize" class="quick_setup" onclick="clickEvent(this);" title="Manually prioritize apps category depending on your preference."><a href=""></a></div>
									<div id="checked_Customize" class="actived_check" style="display:none;"></div>
								</td>	
							</tr>	
							<tr height="40px" align="center">
								<td class="Quick_Setup_tile">Web Surfing</td>
								<td class="Quick_Setup_tile">Customize</td>
							</tr>
							<tr  height="40">
							</tr>
						</table>	
						
						<table id="category_table" width="100%">
							<tr>
								<td width="700px" height="30" align="left" valign="top" bgcolor="#4D595D">																		
									<div class="formfontdesc" style="line-height:20px;font-size:14px;"><#Adaptive_QoS_desc#></div>
								</td>
								<td align="right">
									<img onclick="show_settings(0);" align="right" style="cursor:pointer;marigin-left:-10px;" title="<#Menu_usb_application#>" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
								</td>
							</tr>	
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
						
						<table width="100%">
							<tr>
								<td height="50" >
									<div style=" *width:136px;margin-left:300px;" class="titlebtn" align="center" onClick="applyRule()"><span><#CTL_apply#></span></div>
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
<div id="footer"></div>
</body>
</html>
