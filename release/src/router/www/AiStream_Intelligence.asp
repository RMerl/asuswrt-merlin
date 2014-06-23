<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#EZQoS#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<script type="text/javascript" src="/detect.js"></script>
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
	height:600px;
	padding:20px 0px;
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
</style>
<script>
var $j = jQuery.noConflict();
var bwdpi_app_rulelist = "<% nvram_get("bwdpi_app_rulelist"); %>".replace(/&#60/g, "<");;
var category_desc = ["", "Streaming Media","Online Chat & Communication","Games","Web Surfing","File Transfer","General"];
var cat_id_array = [[9,20], [4], [0,5,6,15,17], [8], [13,24], [1,3,14], [7,10,11,21,23]];

function register_event(){
	$j(function() {
		$j("#category_list").sortable({
			stop: function(event, ui){
				regen_priority(this);		
			}
		});
		$j("#category_list").disableSelection();
	});
} 

function initial(){
	show_menu();
	gen_category_block();
	register_event();
}

function switchPage(page){
	if(page == "1")	
		location.href = "/QoS_EZQoS.asp";
	else if(page == "2")	
		location.href = "/AiStream_Intelligence.asp";
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
		
		code += '<div id='+ index +'>'+ category_desc[index] +'</div>';
	}
	
	$('category_list').innerHTML = code;
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
	document.form.bwdpi_app_rulelist.value = bwdpi_app_rulelist;
	document.form.submit();
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
<input type="hidden" name="current_page" value="/AiStream_Intelligence.asp">
<input type="hidden" name="next_page" value="/AiStream_Intelligence.asp">
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
											<td  class="formfonttitle" align="left">								
												<div >Adaptive QoS - Intelligence</div>
											</td>
											<td align="right">
												<div>
													<select onchange="switchPage(this.options[this.selectedIndex].value)" class="input_option">
														<option value="1" >Configure</option>
														<option value="2" selected>Intelligence Type</option>														
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
									<div class="formfontdesc" style="line-height:20px;font-size:14px;">In the QoS Intelligence type, the APPs with higher priority will be get better bandwidth over another when the APPs are competing for limited bandwidth, Drag and drop APPs category to upper level for higher priority</div>
								</td>
							</tr>							
							<tr>
								<td>	
									<div id="category_list"></div>
								</td>
							</tr>
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
