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
<title><#Web_Title#> - <#Adaptive_History#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/client_function.js"></script>
<script>
function initial(){
	show_menu();	
	getWebHistory();
	genClientListOption()
}
var data_array = new Array();
function parsingAjaxResult(rawData){
	var match = 0;;
	for(i=0;i<rawData.length;i++){
		for(j=0;j<data_array.length;j++){
			if((data_array[j][0] == rawData[i][0])
			&& (data_array[j][1] == rawData[i][1])
			&& (data_array[j][2].toUpperCase() == rawData[i][2].toUpperCase())){
				match = 1;				
				break;
			}	
		}
				
		if(match == 0)
			data_array.push(rawData[i]);
		
		match = 0;
	}

	var code = "";
	code += "<tr>";
	code += "<th style='width:20%;text-align:left'>Access Time</th>";
	code += "<th style='width:30%;text-align:left'>MAC Address / Client's Name</th>";
	code += "<th style='width:50%;text-align:left'>Domain Name</th>";
	code += "</tr>";
	for(var i=0; i<data_array.length; i++){	
		code += "<tr style='line-height:15px;'>";
		code += "<td>" + convertTime(data_array[i][1]) + "</td>";
		if(clientList[data_array[i][0]] != undefined) {
			var clientName = (clientList[data_array[i][0]].nickName == "") ? clientList[data_array[i][0]].name : clientList[data_array[i][0]].nickName;
			code += "<td title="+ data_array[i][0] + ">" + clientName + "</td>";
		}
		else
			code += "<td>" + data_array[i][0] + "</td>";
		
		code += "<td>" + data_array[i][2] + "</td>";	
		code += "</tr>";
	}
	
	document.getElementById('log_table').innerHTML = code;
	data_array = [];
}

function convertTime(t){
	var time = new Date();
	var time_string = "";
	time.setTime(t*1000);

	time_string = time.getFullYear() + "-" + (time.getMonth() + 1) + "-";
	time_string += transform_time_format(time.getDate());	  
	time_string += "&nbsp&nbsp" + transform_time_format(time.getHours()) + ":" + transform_time_format(time.getMinutes()) + ":" + transform_time_format(time.getSeconds());
	
	return time_string;
}

function transform_time_format(time){
	var string = "";
	if(time < 10)
		string += "0" + time;
	else
		string += time;
		
	return string;
}

var history_array = new Array();
function getWebHistory(mac, page){
	var page_count = page ? page : "1";
	var client = mac ? ("?client=" + mac) : ("?page=" + page_count); 

	$.ajax({
		url: '/getWebHistory.asp' + client,
		dataType: 'script',	
		error: function(xhr){
			setTimeout("getWebHistory();", 1000);
		},
		success: function(response){
			history_array = array_temp;
			parsingAjaxResult(array_temp);
			if(page_count == "1" || document.getElementById('clientListOption').value  != ""){
				document.getElementById('previous_button').style.visibility = "hidden";
			}
			else{
				document.getElementById('previous_button').style.visibility = "visible";		
			}
			
			if(history_array.length < 50 || document.getElementById('clientListOption').value  != ""){
				document.getElementById('next_button').style.visibility = "hidden";
			}
			else{
				document.getElementById('next_button').style.visibility = "visible";			
			}
			
			if(document.getElementById('clientListOption').value  == "" && page_count == "1" && history_array.length < 50){
				document.getElementById('current_page').style.visibility = "hidden";			
			}
			else{
				document.getElementById('current_page').style.visibility = "visible";			
			}
					
			if(document.getElementById('clientListOption').value  != ""){			
				document.getElementById('current_page').style.display = "none";
			}
			else{
				document.getElementById('current_page').style.display = "";
			}
			
			document.getElementById('current_page').value = page_count;
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

		var clientName = (clientObj.nickName == "") ? clientObj.name : clientObj.nickName;
		var newItem = new Option(clientName, clientObj.mac);
		document.getElementById("clientListOption").options.add(newItem); 
	}
}

function change_page(flag){
	var current_page = document.getElementById('current_page').value;
	var page = 1;
	if(flag == "next"){
		page = parseInt(current_page) + 1;
		getWebHistory("", page);
	}
	else{
		page = parseInt(current_page) - 1;
		if(page < 1)
			page = 1;
	
		getWebHistory("", page);
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
<input type="hidden" name="current_page" value="/AdaptiveQoS_WebHistory.asp">
<input type="hidden" name="next_page" value="/AdaptiveQoS_WebHistory.asp">
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
									<div id="content_title" class="formfonttitle"><#menu5_3_2#> - <#Adaptive_History#></div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div class="formfontdesc">
										<#Adaptive_History_desc#>
									</div>
									<div style="margin-bottom:5px">
										<select id="clientListOption" class="input_option" name="clientList" onchange="getWebHistory(this.value);">
											<option value="" selected>All client</option>
										</select>
										<label style="margin: 0 5px 0 20px;visibility:hidden;cursor:pointer" id="previous_button" onclick="change_page('previous');">Previous</label>
										<input class="input_3_table" value="1" id="current_page"></input>
										<label style="margin-left:5px;cursor:pointer" id="next_button" onclick="change_page('next');">Next</label>
									</div>
									<div style="height:600px;border:1px solid #A9A9A9;overflow:auto;">
										<table style="width:100%" id="log_table"></table>
									</div>
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
