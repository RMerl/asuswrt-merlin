﻿<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN"
"http://www.w3.org/TR/html4/loose.dtd">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>device-map/clients.asp</title>
<style>
a:link {
	text-decoration: underline;
	color: #FFFFFF;
	font-family: Lucida Console;
}
a:visited {
	text-decoration: underline;
	color: #FFFFFF;
}
a:hover {
	text-decoration: underline;
	color: #FFFFFF;
}
a:active {
	text-decoration: none;
	color: #FFFFFF;
}
p{
	font-weight: bolder;
}
.ClientName{
	font-size: 12px;
	font-family: Lucida Console;
}
#device_img1{
  background: url(../images/wl_device/wl_devices.png) no-repeat;
  background-position: 4px 6px; width: 30px; height: 32px;
}
#device_img2{
  background: url(../images/wl_device/wl_devices.png) no-repeat;
  background-position: 4px -25px; width: 30px; height: 32px;
}
#device_img3{
  background: url(../images/wl_device/wl_devices.png) no-repeat;
  background-position: 4px -57px; width: 30px; height: 32px;
}
#device_img4{
  background: url(../images/wl_device/wl_devices.png) no-repeat;
  background-position: 4px -89px; width: 30px; height: 32px;
}
#device_img5{
  background: url(../images/wl_device/wl_devices.png) no-repeat;
  background-position: 7px -121px; width: 30px; height: 32px;
}
#device_img6{
  background: url(../images/wl_device/wl_devices.png) no-repeat;
  background-position: 7px -152px; width: 30px; height: 33px;
}
#device_img7{
  background: url(../images/wl_device/wl_devices.png) no-repeat;
  background-position: 5px -182px; width: 30px; height: 32px;
}
#device_img8{
  background: url(../images/wl_device/wl_devices.png) no-repeat;
  background-position: 3px -215px; width: 30px; height: 32px;
}
</style>
<link href="/form_style.css" rel="stylesheet" type="text/css" />
<link href="/NM_style.css" rel="stylesheet" type="text/css" />
<link rel="stylesheet" type="text/css" href="../form_style.css"> 
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/jquery.xdomainajax.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/tmmenu.js"></script>
<script type="text/javascript" src="/nameresolv.js"></script>
<script>
var $j = jQuery.noConflict();
<% login_state_hook(); %>

var DEVICE_TYPE = ["", "<#Device_type_01_PC#>", "<#Device_type_02_RT#>", "<#Device_type_03_AP#>", "<#Device_type_04_NS#>", "<#Device_type_05_IC#>", "<#Device_type_06_OD#>", "Printer", "TV Game Console"];
var asus_device_list_buf = "<% nvram_get("asus_device_list"); %>";
var asus_device_list = asus_device_list_buf.replace(/&#62/g, ">").replace(/&#60/g, ",<")
var client_list_array = '<% get_client_detail_info(); %>';
var client_list_row;
var asus_device_list_row;
var networkmap_scanning;
var macfilter_rulelist_array = '<% nvram_get("macfilter_rulelist"); %>';
var macfilter_rulelist_row = macfilter_rulelist_array.split('&#60'); 
var macfilter_enable =  '<% nvram_get("macfilter_enable_x"); %>';
var mapscanning = 0;
var thisDevice;
(function(){
	var thisDeviceObj = {
		type: '<3',
		name: '<% nvram_get("productid"); %>',
		ipaddr: '<% nvram_get("lan_ipaddr"); %>',
		mac: '<% nvram_get("et0macaddr"); %>',
		other: "1>2>0"
	}

	thisDevice = [thisDeviceObj.type, thisDeviceObj.name, thisDeviceObj.ipaddr, thisDeviceObj.mac, thisDeviceObj.other].join(">");
})()

function update_clients(e) {
  $j.ajax({
    url: '/update_clients.asp',
    dataType: 'script', 
	
    error: function(xhr) {
      setTimeout("update_clients();", 1000);
    },
    success: function(response) {
			asus_device_list = asus_device_list_buf.replace(/&#62/g, ">").replace(/&#60/g, "<");
			merge_client_list();
			showclient_list(0);
			_showNextItem(listFlag);
			if(networkmap_scanning == 1 || client_list_array == "")
				setTimeout("update_clients();", 2000);
			$("loadingIcon").style.display = (mapscanning == 1 ? "" : "none");
		}    
  });

}

function gotoMACFilter(){
	if(parent.ParentalCtrl2_support)
		parent.location.href = "/ParentalControl.asp";
	else
		parent.location.href = "/Advanced_MACFilter_Content.asp";
}

function check_subnet(){
	var thisDevice_array = thisDevice.split('<');
	var thisDevice_col = thisDevice_array[1].split('>');
	for(var i = asus_device_list_row.length - 1; i > 0; i--){
		var asus_device_list_col = asus_device_list_row[i].split('>');
		if(asus_device_list_col.indexOf(thisDevice_col[2]) > 0){
			asus_device_list_row.splice(i,1);
			break;
		}	
	}
}

function merge_client_list(){
	client_list_row = client_list_array.split('<');
	asus_device_list_row = asus_device_list.split('<');
	check_subnet();
	for(var i = 1; i < asus_device_list_row.length; i++){
		var asus_device_list_row_col = asus_device_list_row[i].split('>');
		for(var j = client_list_row.length - 1; j > 0; j--){
			find_idx = client_list_row[j].indexOf(asus_device_list_row_col[3]);
			if (find_idx > 0){
				client_list_row.splice(j,1);
				break;
			}
		}
	}
		
	client_list_row = asus_device_list_row.concat(client_list_row);
	client_list_row.splice(asus_device_list_row.length,1);
}

function initial(){
	if(client_list_array != ""){
		merge_client_list();
		showclient_list(0);
		setTimeout("_showNextItem(listFlag);", 1);
	}
	else{
		var HTMLCode = '<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table" id="client_list_table">';
		HTMLCode += '<tr><td style="color:#FFCC00;font-size:12px; border-collapse: collapse;border:1;" colspan="4"><span style="line-height:25px;"><#Device_Searching#></span>&nbsp;<img style="margin-top:10px;" src="/images/InternetScan.gif"></td></tr>';
		HTMLCode += '</table>';
		$("client_list_Block").innerHTML = HTMLCode;
	}
	setTimeout("update_clients();", 1000);
	if((macfilter_enable != 0 || ParentalCtrl_support) && sw_mode == 1)
			$("macFilterHint").style.display = "";
}

var listFlag = 0;
var itemperpage = 12;
function _showNextItem(num){
	$("client_list_Block").style.display = "";
	var _client_list_row_length = client_list_row.length-1;

	if(_client_list_row_length < parseInt(itemperpage)+1){
		$("leftBtn").style.visibility = "hidden";
		$("rightBtn").style.visibility = "hidden";
		return false;
	}

	for(var i=1; i<client_list_row.length; i++){
		$("row"+i).style.display = "none";
	}

	if(num == 0)
		var startNum = 1;
	else
		var startNum = parseInt(num)*itemperpage+1;

	if(num == Math.floor(_client_list_row_length/itemperpage))  // last page
		var endNum = client_list_row.length;
	else
		var endNum = startNum + itemperpage;

	for(i=startNum; i<endNum; i++){
		$("row"+i).style.display = "";
	}

	// start
	if(startNum == 1){
		$("leftBtn").style.visibility = "hidden";
	}else{
		$("leftBtn").style.visibility = "";
		$("leftBtn").title = "<#prev_page#>";
	}	

	// end
	if(endNum == client_list_row.length){
		$("rightBtn").style.visibility = "hidden";
	}else{
		$("rightBtn").style.visibility = "";
		$("rightBtn").title = "<#next_page#>";
	}	
}

function showNextItem(act){
	if(act == 1) // next page
		listFlag++;
	else  // previous page
		listFlag--;

	if(listFlag < 0)
		listFlag = 0;
	else if(listFlag > Math.floor(client_list_row.length/itemperpage))
		listFlag = Math.floor(client_list_row.length/itemperpage);

	_showNextItem(listFlag);
}

var overlib_str_tmp = "";
function showclient_list(list){
	var code = "";
	networkmap_scanning = 0;

	$("t0").className = list ? "tab_NW" : "tabclick_NW";
	$("t1").className = list ? "tabclick_NW" : "tab_NW";
	
	if(list)
		$("isblockdesc").innerHTML = "<#btn_remove#>";

	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table" id="client_list_table">';
	if(client_list_row.length == 1)
		code +='<tr><td style="color:#FFCC00;font-size:12px; border-collapse: collapse;border:1;" colspan="4"><span style="line-height:25px;"><#Device_Searching#></span>&nbsp;<img style="margin-top:10px;" src="/images/InternetScan.gif"></td></tr>';
	else{
		for(var i = 1; i < client_list_row.length; i++){
			code +='<tr id="row'+i+'">';
			var client_list_col = client_list_row[i].split('>');
			var overlib_str = "";

			if(client_list_col[1] == "")	
				client_list_col[1] = retHostName(client_list_col[3]);

			if(client_list_col[1].length > 16){
				overlib_str += "<p><#PPPConnection_UserName_itemname#></p>" + client_list_col[1];
				client_list_col[1] = client_list_col[1].substring(0, 13);
				client_list_col[1] += "...";
			}

			overlib_str += "<p><#MAC_Address#>:</p>" + client_list_col[3];
			if(login_ip_str() == client_list_col[2])
				overlib_str += "<p><#CTL_localdevice#>:</p>YES";
			if(client_list_col[5] == 1)
				overlib_str += "<p><#Device_service_Printer#></p>YES";
			if(client_list_col[6] == 1)
				overlib_str += "<p><#Device_service_iTune#></p>YES";

			for(var j = 0; j < client_list_col.length-3; j++){
				if(j == 0){
					if(client_list_col[0] == "0" || client_list_col[0] == ""){
						code +='<td width="12%" height="30px;" title="'+DEVICE_TYPE[client_list_col[0]]+'"><div id="device_img6"></div></td>';
						networkmap_scanning = 1;
					}
					else{
						code +='<td width="12%" height="30px;" title="'+DEVICE_TYPE[client_list_col[0]]+'">';
						code +='<div id="device_img'+client_list_col[0]+'"></div></td>';
					}	
				}
				else if(j == 1){
					if(client_list_col[1] != "")
						code += '<td width="40%" onclick="oui_query(\'' + client_list_col[3] + '\');overlib_str_tmp=\''+ overlib_str +'\';return overlib(\''+ overlib_str +'\');" onmouseout="nd();" class="ClientName" style="cursor:pointer;text-decoration:underline;">'+ client_list_col[1] +'</td>';	// Show Device-name
					else
						code += '<td width="40%" onclick="oui_query(\'' + client_list_col[3] + '\');overlib_str_tmp=\''+ overlib_str +'\';return overlib(\''+ overlib_str +'\');" onmouseout="nd();" class="ClientName" style="cursor:pointer;text-decoration:underline;">'+ client_list_col[3] +'</td>';	// Show MAC
				}
				else if(j == 2){
					if(client_list_col[4] == "1")			
						code += '<td width="36%"><a title="<#LAN_IP_client#>" class="ClientName" style="text-decoration:underline;" target="_blank" href="http://'+ client_list_col[2] +'">'+ client_list_col[2] +'</a></td>';
					else
						code += '<td width="36%"><span title="<#LAN_IP_client#>" class="ClientName">'+ client_list_col[2] +'</span></td>';	
				}
				else if(j == client_list_col.length-4)
					code += '';
			}
			
			if(parent.sw_mode == 1 && ParentalCtrl_support)
				code += '<td width="12%"><input class="remove_btn_NM" type="submit" title="<#Block#>" onclick="block_this_client(this);" value=""/></td></tr>';
			else
				code += '</tr>';
		}
	}
	code +='</table>';
	$("client_list_Block").innerHTML = code;
	$("client_list_Block").style.display = "none";

	for(var i=client_list_row.length-1; i>0; i--){
		var client_list_col = client_list_row[i].split('>');
		if(list == is_blocked_client(client_list_col[3])){
			$('client_list_table').deleteRow(i-1);
		}
	}

	if($('client_list_table').innerHTML == "<tbody></tbody>"){
		code ='<tr><td style="color:#FFCC00;" colspan="4"><#IPConnection_VSList_Norule#></td></tr>'
		$("client_list_Block").innerHTML = code;
	}

	parent.client_list_array = client_list_array;
	parent.show_client_status();
}

overlib.isOut = true;

function is_blocked_client(client_mac){
	var macfilter_rulelist_row = macfilter_rulelist_array.split('&#60');
	var is_blocked = 0;
	
	if(macfilter_enable == '0')
		return 1;
		
	for(var i = 1; i < macfilter_rulelist_row.length; i++){
		if(macfilter_rulelist_row[i] == client_mac)
			is_blocked = is_blocked+1;
	}
	
	if(macfilter_enable == "1")
		return is_blocked;
	else
		return !is_blocked;
}

function block_this_client(r){
	var macfilter_rulelist_row = macfilter_rulelist_array.split('&#60');
	var this_client = r.parentNode.parentNode.rowIndex;
	var client_list_col = client_list_row[this_client+1].split('>');

	if($("t0").className == "tabclick_NW")
		var list_type = "2";
	else
		var list_type = "1";
	
	if(document.form.macfilter_enable_x.value == "0"){
		if(macfilter_rulelist_row[0] != ""){
			if(confirm("Do you want to block all clients in original blacklist too?")){
				for(var i = 1; i < macfilter_rulelist_row.length+1; i++){
					if(macfilter_rulelist_row[i] != client_list_col[3]){
						macfilter_rulelist_array += "&#60"; 
						macfilter_rulelist_array += client_list_col[3];
					}
				}
			}
			else{
				macfilter_rulelist_array = "&#60"; 
				macfilter_rulelist_array += client_list_col[3];
			}
		}
		else{
			macfilter_rulelist_array = "&#60"; 
			macfilter_rulelist_array += client_list_col[3];
		}
		document.form.macfilter_enable_x.value = "2";					
	}
	else if(document.form.macfilter_enable_x.value == list_type){
		macfilter_rulelist_array += "&#60"; 
		macfilter_rulelist_array += client_list_col[3];
	}
	else{
		for(var i = 1; i < macfilter_rulelist_row.length+1; i++){
			if(macfilter_rulelist_row[i] == client_list_col[3]){
				macfilter_rulelist_array = macfilter_rulelist_array.replace("&#60"+macfilter_rulelist_row[i], "");
			}
			break;
		}
	}
			
	macfilter_rulelist_array = macfilter_rulelist_array.replace(/\&#60/g, '<');
	document.form.macfilter_rulelist.value = macfilter_rulelist_array;
	FormActions("", "apply", "restart_firewall", "");
	document.form.submit();
}

function networkmap_update(){
	FormActions("/apply.cgi", "refresh_networkmap", "", "");
	document.form.current_page.value = "device-map/clients.asp";
	document.form.target = "";
	document.form.submit();
}

</script>
</head>

<body class="statusbody" onload="initial();">

<iframe name="applyFrame" id="applyFrame" src="" width="0" height="0" frameborder="0" scrolling="no"></iframe>

<form method="post" name="form" id="refreshForm" action="/start_apply.htm" target="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="current_page" value="device-map/clients.asp">
<input type="hidden" name="next_page" value="device-map/clients.asp">
<input type="hidden" name="flag" value="">
<input type="hidden" name="macfilter_rulelist" value="<% nvram_get("macfilter_rulelist"); %>">
<input type="hidden" name="macfilter_enable_x" value="<% nvram_get("macfilter_enable_x"); %>">
</form>

<table width="320px" align border="0" cellpadding="0" cellspacing="0">
	<tr style="display:none;">
		<td>		
			<table width="100px" border="0" align="left" cellpadding="0" cellspacing="0">
  			<tr>
  				<td ><div id="t0" class="tabclick_NW" align="center" style="font-weight: bolder; margin-right:2px; width:70px;" onclick="showclient_list(0)"><span id="span1" ><a><#ConnectedClient#></a></span></div></td>
  				<td ><div id="t1" class="tab_NW" align="center" style="font-weight: bolder; margin-right:2px; width:70px;" onclick="showclient_list(1)"><span id="span1" ><a><#BlockedClient#></a></span></div></td>
				<td>&nbsp</td>
			</tr>
			</table>
		</td>
	</tr>
	
<tr>
	<td>				
		<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px">
  		<tr>
    			<td style="padding:3px 3px 5px 5px;">
						<div id="client_list_Block">
						</div>
    			</td>
  		</tr>
 		</table>
		
		<div id="macFilterHint" style="padding:5px 0px 5px 25px;display:none;">
			<ul style="font-size:11px; font-family:Arial; padding:0px; margin:0px; list-style:outside; line-height:150%;">
				<li><a onclick="gotoMACFilter();" style="font-weight:bolder;cursor:pointer;text-decoration:underline;"><#block_client#></a></li>
			</ul>
		</div>
 	</td>
</tr>
</table>

<br/>
<img height="25" id="leftBtn" onclick="showNextItem(0);" style="visibility:hidden;cursor:pointer;margin-left:10px;" src="/images/arrow-left.png">
<input type="button" id="refresh_list" class="button_gen" onclick="networkmap_update();" value="<#CTL_refresh#>" style="margin-left:70px;">
<img id="loadingIcon" src="/images/InternetScan.gif">
<img height="25" id="rightBtn" onclick="showNextItem(1);" style="visibility:hidden;cursor:pointer;float:right;" src="/images/arrow-right.png">
</body>
</html>
