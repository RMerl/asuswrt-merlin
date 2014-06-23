<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN"
"http://www.w3.org/TR/html4/loose.dtd">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>Find ASUS Device</title>
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
<script>
var $j = jQuery.noConflict();
<% login_state_hook(); %>

var DEVICE_TYPE = ["", "<#Device_type_01_PC#>", "<#Device_type_02_RT#>", "<#Device_type_03_AP#>", "<#Device_type_04_NS#>", "<#Device_type_05_IC#>", "<#Device_type_06_OD#>", "Printer", "TV Game Console"];

var sw_mode = '<% nvram_get("sw_mode"); %>';
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

var client_list_array = thisDevice + '<% get_client_detail_info(); %>';
var client_list_row;
var networkmap_scanning;

/* get client info form dhcp lease log */
loadXMLDoc("/getdhcpLeaseInfo.asp");
var _xmlhttp;
function loadXMLDoc(url){
	if(sw_mode != 1) return false;
 
	var ie = window.ActiveXObject;
	if(ie)
		_loadXMLDoc_ie(url);
	else
		_loadXMLDoc(url);
}

var _xmlDoc_ie;
function _loadXMLDoc_ie(file){
	_xmlDoc_ie = new ActiveXObject("Microsoft.XMLDOM");
	_xmlDoc_ie.async = false;
	if (_xmlDoc_ie.readyState==4){
		_xmlDoc_ie.load(file);
		setTimeout("parsedhcpLease(_xmlDoc_ie);", 1000);
	}
}

function _loadXMLDoc(url) {
	_xmlhttp = new XMLHttpRequest();
	if (_xmlhttp && _xmlhttp.overrideMimeType)
		_xmlhttp.overrideMimeType('text/xml');
	else
		return false;

	_xmlhttp.onreadystatechange = state_Change;
	_xmlhttp.open('GET', url, true);
	_xmlhttp.send(null);
}

function state_Change(){
	if(_xmlhttp.readyState==4){// 4 = "loaded"
  	if(_xmlhttp.status==200){// 200 = OK
			parsedhcpLease(_xmlhttp.responseXML);    
		}
  	else{
			return false;
    }
  }
}

var leasehostname;
var leasemac;
function parsedhcpLease(xmldoc)
{
	var dhcpleaseXML = xmldoc.getElementsByTagName("dhcplease");
	leasehostname = dhcpleaseXML[0].getElementsByTagName("hostname");
	leasemac = dhcpleaseXML[0].getElementsByTagName("mac");
}

var retHostName = function(_mac){
	if(sw_mode != 1 || !leasemac) return false;

	for(var idx=0; idx<leasemac.length; idx++){
		if(!(leasehostname[idx].childNodes[0].nodeValue.split("value=")[1]) || !(leasemac[idx].childNodes[0].nodeValue.split("value=")[1]))
			continue;

		if( _mac.toLowerCase() == leasemac[idx].childNodes[0].nodeValue.split("value=")[1].toLowerCase()){
			if(leasehostname[idx].childNodes[0].nodeValue.split("value=")[1] != "*")
				return leasehostname[idx].childNodes[0].nodeValue.split("value=")[1];
			else
				return "";
		}
	}
	return "";
}
/* end */

function update_clients(e) {
  $j.ajax({
    url: '/update_clients.asp',
    dataType: 'script', 
	
    error: function(xhr) {
      setTimeout("update_clients();", 1000);
    },
    success: function(response) {
			client_list_array = thisDevice + client_list_array;

			client_list_row = client_list_array.split('<');
			showclient_list(0);
			_showNextItem(listFlag);
			if(networkmap_scanning == 1 || client_list_array == "")
				setTimeout("update_clients();", 2000);
		}    
  });
}

function initial(){
	if(client_list_array != ""){
		client_list_row = client_list_array.split('<');
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
}

var listFlag = 0;
var itemperpage = 14;
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

function isASUSDevice(devName){
	if(devName.search("RT-")<0 && devName.search("DSL-")<0 && devName.search("RP-")<0)
		return false;
	return true;
}

var overlib_str_tmp = "";
function showclient_list(list){
	var code = "";
	networkmap_scanning = 0;

	if(list)
		$("isblockdesc").innerHTML = "<#btn_remove#>";

	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table" id="client_list_table">';
	if(client_list_row.length == 1)
		code +='<tr><td style="color:#FFCC00;font-size:12px; border-collapse: collapse;border:1;" colspan="4"><span style="line-height:25px;"><#Device_Searching#></span>&nbsp;<img style="margin-top:10px;" src="/images/InternetScan.gif"></td></tr>';
	else{
		for(var i = 1; i < client_list_row.length; i++){
			var overlib_str = "";
			var client_list_col = client_list_row[i].split('>');

			if(!isASUSDevice(client_list_col[1]))
				continue;

			code +='<tr id="row'+i+'">';

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
				else				
					code += '<td width="36%" class="ClientName" onclick="oui_query(\'' + client_list_col[3] + '\');overlib_str_tmp=\''+ overlib_str +'\';return overlib(\''+ overlib_str +'\');" onmouseout="nd();">'+ client_list_col[j] +'</td>';
			}
			
			code += '</tr>';
		}
	}
	code +='</table>';
	$("client_list_Block").innerHTML = code;
	$("client_list_Block").style.display = "none";

	if($('client_list_table').innerHTML == "<tbody></tbody>"){
		code ='<tr><td style="color:#FFCC00;" colspan="4"><#IPConnection_VSList_Norule#></td></tr>'
		$("client_list_Block").innerHTML = code;
	}
}

overlib.isOut = true;
function oui_query(mac) {
	var tab = new Array();	
	tab = mac.split(mac.substr(2,1));

  $j.ajax({
    url: 'http://standards.ieee.org/cgi-bin/ouisearch?'+ tab[0] + '-' + tab[1] + '-' + tab[2],
		type: 'GET',
    error: function(xhr) {
			if(overlib.isOut)
				return true;
			else
				oui_query(mac);
    },
    success: function(response) {
			if(overlib.isOut)
				return nd();
			var retData = response.responseText.split("pre")[1].split("(base 16)")[1].replace("PROVINCE OF CHINA", "R.O.C").split("&lt;/");
			overlib_str_tmp += "<p><span>.....................................</span></p>";
			return overlib(overlib_str_tmp + "<p style='margin-top:5px'>Manufacturer:</p>" + retData[0]);
		}    
  });
}

function networkmap_update(){
	FormActions("/apply.cgi", "refresh_networkmap", "", "");
	document.form.current_page.value = "find_device.asp";
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
<input type="hidden" name="current_page" value="find_device.asp">
<input type="hidden" name="next_page" value="find_device.asp">
<input type="hidden" name="flag" value="">
</form>

<table width="450px" align="center" border="0" cellpadding="0" cellspacing="0">
	<tr>
		<td>				
			<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px">
	  		<tr>
	    			<td style="padding:3px 3px 5px 5px;">
							<div id="client_list_Block"></div>
	    			</td>
	  		</tr>
	 		</table>
		</td>
	</tr>
</table>

<br>
<div style="text-align: center;">
	<img height="25px" id="leftBtn" onclick="showNextItem(0);" style="visibility:hidden;cursor:pointer;" src="/images/arrow-left.png">
	<input type="button" id="refresh_list" class="button_gen" onclick="networkmap_update();" value="<#CTL_refresh#>">
	<img height="25px" id="rightBtn" onclick="showNextItem(1);" style="visibility:hidden;cursor:pointer;" src="/images/arrow-right.png">
</div>

</body>
</html>
