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
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();
</script>
<style type="text/css">
.qos_table{
	width:740px;
	padding:10px; 
	margin-top:-17px;
	position:relative;
	background-color:#4d595d;
	height: 650px;
	-webkit-border-bottom-right-radius: 3px;
	-webkit-border-bottom-left-radius: 3px;
	-moz-border-radius-bottomright: 3px;
	-moz-border-radius-bottomleft: 3px;
	border-bottom-right-radius: 3px;
	border-bottom-left-radius: 3px;
}
</style>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

function initial(){
	show_menu();

	if(downsize_support != -1)
		$("guest_image").parentNode.style.display = "none";

	if(document.form.qos_enable.value==1){
		document.form.qos_obw.parentNode.parentNode.style.display = "";
		document.form.qos_ibw.parentNode.parentNode.style.display = "";		
	}else{
		document.form.qos_obw.parentNode.parentNode.style.display = "none";
		document.form.qos_ibw.parentNode.parentNode.style.display = "none";		
	}
	init_changeScale("qos_obw");
	init_changeScale("qos_ibw");	
	addOnlineHelp($("faq"), ["ASUSWRT", "QoS"]);
}

function init_changeScale(_obj_String){
	if($(_obj_String).value > 999){
		$(_obj_String+"_scale").value = "Mb/s";
		$(_obj_String).value = Math.round(($(_obj_String).value/1024)*100)/100;
	}
}

function changeScale(_obj_String){
	if($(_obj_String+"_scale").value == "Mb/s")
		$(_obj_String).value = Math.round(($(_obj_String).value/1024)*100)/100;
	else
		$(_obj_String).value = Math.round($(_obj_String).value*1024);
}

function switchPage(page){
	if(page == "1")
		return false;
	else
		location.href = "/Advanced_QOSUserSpec_Content.asp";
}

function submitQoS(){
	if(document.form.qos_enable.value == 1){
		// Jieming To Do: please add a hint here when error occurred, and qos_ibw & qos_obw should allow number only.
		if(document.form.qos_obw.value.length == 0 || document.form.qos_obw.value == 0){
				alert("<#JS_fieldblank#>");
				document.form.qos_obw.focus();
				return;
		}
		if(document.form.qos_ibw.value.length == 0 || document.form.qos_ibw.value == 0){
				alert("<#JS_fieldblank#>");
				document.form.qos_ibw.focus();
				return;
		}
		// end
  }	

	if($("qos_obw_scale").value == "Mb/s")
		document.form.qos_obw.value = Math.round(document.form.qos_obw.value*1024);
	if($("qos_ibw_scale").value == "Mb/s")
		document.form.qos_ibw.value = Math.round(document.form.qos_ibw.value*1024);
  
	if(document.form.qos_enable.value != document.form.qos_enable_orig.value)
    	FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
			
	if(wl6_support != -1)
		document.form.action_wait.value = parseInt(document.form.action_wait.value)+10;			// extend waiting time for BRCM new driver

	parent.showLoading();
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
<input type="hidden" name="current_page" value="QoS_EZQoS.asp">
<input type="hidden" name="next_page" value="QoS_EZQoS.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_qos">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="flag" value="">
<input type="hidden" name="qos_enable" value="<% nvram_get("qos_enable"); %>">
<input type="hidden" name="qos_enable_orig" value="<% nvram_get("qos_enable"); %>">
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
			<br>
		<!--===================================Beginning of Main Content===========================================-->
		<div class="qos_table" id="qos_table">
		<table>
  		<tr>
    			<td bgcolor="#4D595D" valign="top">
    				<table>
        			<tr>
          			<td>
     							<div align="right">
     		   					<select onchange="switchPage(this.options[this.selectedIndex].value)" class="input_option">
											<!--option><#switchpage#></option-->
											<option value="1" selected><#qos_automatic_mode#></option>
											<option value="2">User define QoS rules</option>
										</select>	    
									</div>
								</td>
        			</tr>
        			<tr>
          				<td height="20"><img src="images/New_ui/export/line_export.png" /></td>
        			</tr>
        			<tr>
          				<td height="30" align="left" valign="middle" >
										<div>
											<table width="650px">
												<tr>
													<td width="130px">
														<img id="guest_image" src="/images/New_ui/QoS.png">
													</td>
													<td>
														<div class="formfontdesc" style="line-height:20px;"><#ezqosDesw#></div>
														<div class="formfontdesc">
															<a id="faq" href="" target="_blank" style="text-decoration:underline;">QoS FAQ</a>
														</div>
													</td>
												</tr>
											</table>
										</div>
          				</td>
        			</tr>
							
							<tr>
								<td>
									<table style="margin-left:3px; margin-top:15px;" width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
										<tr>
										<th><#Enable_defaule_rule#></th>
											<td>
												<div class="left" style="width:94px; float:left; cursor:pointer;" id="radio_qos_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_qos_enable').iphoneSwitch('<% nvram_get("qos_enable"); %>', 
														 function() {
															document.form.qos_enable.value = "1";
															document.form.qos_obw.parentNode.parentNode.style.display = "";
															document.form.qos_ibw.parentNode.parentNode.style.display = "";
														 },
														 function() {
															document.form.qos_enable.value = "0";
															document.form.qos_obw.parentNode.parentNode.style.display = "none";
															document.form.qos_ibw.parentNode.parentNode.style.display = "none";
														 },
														 {
															switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														 }
													);
												</script>			
								</div>	
											</td>
										</tr>										
										<tr>
											<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 2);"><#upload_bandwidth#></a></th>
											<td>
													<input type="text" maxlength="10" id="qos_obw" name="qos_obw" onKeyPress="return is_number(this,event);" class="input_15_table" value="<% nvram_get("qos_obw"); %>">
														<select id="qos_obw_scale" class="input_option" style="width:87px;" onChange="changeScale('qos_obw');">
															<option value="Kb/s">Kb/s</option>
															<option value="Mb/s">Mb/s</option>
														</select>
											</td>
										</tr>
										
										<tr>
											<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(20, 2);"><#download_bandwidth#></a></th>
											<td>
													<input type="text" maxlength="10" id="qos_ibw" name="qos_ibw" onKeyPress="return is_number(this,event);" class="input_15_table" value="<% nvram_get("qos_ibw"); %>">
														<select id="qos_ibw_scale" class="input_option" style="width:87px;" onChange="changeScale('qos_ibw');">
															<option value="Kb/s">Kb/s</option>
															<option value="Mb/s">Mb/s</option>
														</select>
											</td>
										</tr>

									</table>
								</td>
				</tr>	

        			<tr>
          				<td height="50" >
          					<div style=" *width:136px;margin-left:300px;" class="titlebtn" align="center" onClick="submitQoS();"><span><#CTL_onlysave#></span></div>
          				</td>
        			</tr>
      			</table>
      		</td>  
      	</tr>
		</table>
	</div>
		<!--===================================End of Main Content===========================================-->
		</td>
		
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
