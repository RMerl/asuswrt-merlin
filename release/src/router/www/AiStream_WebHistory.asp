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
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/client_function.js"></script>
<script>
var $j = jQuery.noConflict();
function initial(){
	show_menu();
	getWebHistory();
	genClientListOption()
}

function parsingAjaxResult(rawData){
	var retArea = document.getElementById("textarea");
	var retArray = eval(rawData);

	retArea.value = "";
	retArea.value += "Access time";
	retArea.value += "                         ";
	retArea.value += "MAC address";
	retArea.value += "               ";
	retArea.value += "Domain Name";
	retArea.value += "\n";
	retArea.value += "-------------------------------------------------------------------------------------------------------";
	retArea.value += "\n";
	for(var i=0; i<retArray.length; i++){
		retArea.value += convertTime(retArray[i][1]);
		retArea.value += "         ";
		retArea.value += retArray[i][0];
		retArea.value += "         ";
		retArea.value += retArray[i][2];
		retArea.value += "\n";
	}
}

function convertTime(t){
	JS_timeObj.setTime(t*1000);
	JS_timeObj2 = JS_timeObj.toString();	
	JS_timeObj2 = JS_timeObj2.substring(0,3) + ", " +
	              JS_timeObj2.substring(4,10) + "  " +
				  parent.checkTime(JS_timeObj.getHours()) + ":" +
				  parent.checkTime(JS_timeObj.getMinutes()) + ":" +
				  parent.checkTime(JS_timeObj.getSeconds()) + "  " +
				  JS_timeObj.getFullYear();
	
	return JS_timeObj2;
}

function getWebHistory(mac){
	var client = mac ? "?client=" + mac : ""; 

	$j.ajax({
		url: '/getWebHistory.asp' + client,
		dataType: 'html',
		
		error: function(xhr){
			setTimeout("getWebHistory();", 1000);
		},
		success: function(response){
			parsingAjaxResult(response);
		}
	});
}

function genClientListOption(){
	if(clientList.length == 0){
		setTimeout("genClientListOption();", 500);
		return false;
	}

	document.getElementById("clientListOption").options.length = 1;
	for(var i=0; i<clientList.length; i++){
		var clientObj = clientList[clientList[i]];

		if(clientObj.isGateway || !clientObj.isOnline)
			continue;

		var newItem = new Option(clientObj.name, clientObj.mac);
		document.getElementById("clientListOption").options.add(newItem); 
	}
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
<input type="hidden" name="current_page" value="/AiStream_WebHistory.asp">
<input type="hidden" name="next_page" value="/AiStream_WebHistory.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_qos">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="flag" value="">
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
									<div class="formfonttitle">Adaptive QoS - Web Histroy</div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div class="formfontdesc">
										Web history allow you to save all web visited record and manage visiting authority for the device. 
									</div>
									<div style="margin-bottom:5px">
										<select id="clientListOption" class="input_option" name="clientList" onchange="getWebHistory(this.value);">
											<option value="" selected>All client</option>
										</select>	
									</div>
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
										<textarea cols="63" rows="40" wrap="off" readonly="readonly" id="textarea" style="width:99%;font-family:Courier New, Courier, mono; font-size:11px;background:#475A5F;color:#FFFFFF;">
										</textarea>	
									</table>
									<div class="apply_gen">
										<input class="button_gen_long" onClick="getWebHistory(document.form.clientList.value)" type="button" value="<#CTL_refresh#>">
									</div>
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