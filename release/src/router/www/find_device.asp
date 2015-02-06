<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>Find ASUS Device</title>
<link rel="stylesheet" type="text/css" href="/other.css">
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<style>
.Levelfind{  
	margin-left: 0px;
	margin-top: 5px;
	margin-top: -15px \9;
	background-color:#21333e; /*#AAD1E0*/
}

.border1{
	border: 0px solid #53585f;
	border-radius:3px;
}

.border2{
	border-right: 1px solid #53585f;
	padding: 0px 5px 0px 5px;
}

.border3{
	border: 1px solid #53585f;
	border-radius:3px;
	padding: 5px 0px 5px 0px;
}

.border4{
	padding: 0px 0px 0px 10px;
}

.findname{
	font-size:12px;
	font-weight:bolder;
	color:#ffffff;
}
	
.ClientName{
	font:Impact;
	font-size: 14px;
	font-weight: BOLD;
	font-family: Arial, Helvetica, sans-serif;
	color: #76d6ff;
}

.macName{
	font-size: 10px;
	font-weight: BOLD;
	font-family: Arial, Helvetica, sans-serif;
	color: #a9a9a9;
}

.banner_t{
	width:998px;
	height:54px;
	background:url(images/New_ui/title_bg.png) 0 0 no-repeat;
	margin:0px auto;
	margin-bottom:-1px \9;
}

.bgd{
	background:url(images/New_ui/midup_bg.png) 0 0 no-repeat;
}

.bgd1{
	background:url(images/New_ui/bottom_bg1.png) 0 0 repeat;
}

.device_img3{
  background: url(../images/wl_device/wl_devices.png) no-repeat;
  background-position: 5px -57px; width: 30px; height: 32px;
}

.wifi_logo{
	background: url(../images/New_ui/wifi-logo.png) no-repeat;
	background-position: 1px 7px;width: 30px; height: 32px;
}

.go_logo{
	background: url(../images/arrow-right.png) no-repeat;
	background-size: 105% 105%;
	background-position: -2px -2px;width: 30px; height: 32px;
}

.loadingBarBlock1{
	position:absolute;	
	top: 0;
	left: 0;
	margin:80px auto;
	width:99%;
	z-index:100;
	font-size:11px;	
	line-height:16px;
	font-family:Verdana, Arial, Helvetica, sans-serif;
	color:#000;	
}

.Bar_container1{ /* for loadingBar */
	align:left;
	width:70%;
	height:14px;
	margin:0px 48px auto;
	background-color:#222325;
	z-index:100;
}

#proceeding_img_text1{
	position:absolute; z-index:101; font-size:11px; color:#000000; margin-left:175px; line-height:15px;
}
#proceeding_img1{
 	height:14px;
	background:#011846 url(/images/quotabar.gif);
}

.button_gen_dis{
	font-weight: bolder;
	text-shadow: 1px 1px 0px black;
  background: transparent url(/images/New_ui/contentbt_normal.png) no-repeat scroll center top;
  _background: transparent url(/images/New_ui/contentbt_normal_ie6.png) no-repeat scroll center top;
  border:0;
  color: #333333;
	height:33px;
	font-family:Verdana,Bold;
	font-size:12px;
  padding:0 .70em 0 .70em;  
 	width:122px;
  overflow:visible;  
	/*cursor:pointer;*/
	outline: none; /* for Firefox */
 	hlbr:expression(this.onFocus=this.blur()); /* for IE */
}

</style>
<script>	
function $(){
	var elements = new Array();
	
	for(var i = 0; i < arguments.length; ++i){
		var element = arguments[i];
		if(typeof element == 'string')
			element = document.getElementById(element);
		
		if(arguments.length == 1)
			return element;
		
		elements.push(element);
	}
	
	return elements;
}
</script>
<script language="JavaScript" type="text/javascript" src="/jquery.js"></script>
<script>
var rescan = 0;
var $j = jQuery.noConflict();

var DEVICE_TYPE = ["", "<#Device_type_01_PC#>", "<#Device_type_02_RT#>", "<#Device_type_03_AP#>", "<#Device_type_04_NS#>", "<#Device_type_05_IC#>", "<#Device_type_06_OD#>", "Printer", "TV Game Console"];

var sw_mode = '<% nvram_get("sw_mode"); %>';
var thisDevice;
(function(){
	var thisDeviceObj = {
		type: '<3',
		name: '<% nvram_get("productid"); %>',
		ipaddr: '<% nvram_get("lan_ipaddr"); %>',
		mac: '<% nvram_get("et0macaddr"); %>',
		other: "1>2>0",
		ssid:'<% nvram_get("wl_ssid"); %>'
	}

	thisDevice = [thisDeviceObj.type, thisDeviceObj.name, thisDeviceObj.ipaddr, thisDeviceObj.mac, thisDeviceObj.other,thisDeviceObj.ssid].join(">");
})()

var direct_dut=0;
var flag = '<% get_parameter("flag"); %>';
var productid = '<% nvram_get("productid"); %>'.toLowerCase();
var hostname = location.hostname;
var asus_device_list_buf = '<% nvram_get("asus_device_list"); %>';
var asus_device_list = asus_device_list_buf.replace(/&#62/g, ">").replace(/&#60/g, "<");
var client_list_array;

if(asus_device_list == "")
	client_list_array = thisDevice;
else
	client_list_array = asus_device_list;

var client_list_row;
var networkmap_scanning;

var winH,winW;
		
function winW_H(){
	if(parseInt(navigator.appVersion) > 3){
		winW = document.documentElement.scrollWidth;
		if(document.documentElement.clientHeight > document.documentElement.scrollHeight)
			winH = document.documentElement.clientHeight;
		else
			winH = document.documentElement.scrollHeight;
	}
} 

function FormActions(_Action, _ActionMode, _ActionScript, _ActionWait){
	if(_Action != "")
		document.form.action = _Action;
	if(_ActionMode != "")
		document.form.action_mode.value = _ActionMode;
	if(_ActionScript != "")
		document.form.action_script.value = _ActionScript;
	if(_ActionWait != "")
		document.form.action_wait.value = _ActionWait;
}

function update_clients(e) {
  $j.ajax({
    url: '/update_clients.asp',
    dataType: 'script', 
	
    error: function(xhr) {
      setTimeout("update_clients();", 1000);
    },
    success: function(response) {
			asus_device_list = asus_device_list_buf.replace(/&#62/g, ">").replace(/&#60/g, "<");

			if(asus_device_list == "")
				client_list_array = thisDevice;
			else
				client_list_array = asus_device_list;

			client_list_row = client_list_array.split('<');
			
			if(rescan == 1) return;
			
			showclient_list(0);
			_showNextItem(listFlag);
			// if(networkmap_scanning == 1 || client_list_array == "")
				if (retry < 10){
					setTimeout("update_clients();", 2000);
					retry++;
				}
		}    
  });
}

function load_body(){
	winW_H();
   if(flag == 'scan_finish'){	
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
	retry = 0;
	setTimeout("update_clients();", 2000);
   }else
	networkmap_update();
}

var listFlag = 0;
var itemperpage = 14;
function _showNextItem(num){
	$("client_list_Block").style.display = "";
	return false;

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
	if(hostname.search(productid) == 0){
		if(hostname.search(devName.toLowerCase()) < 0){
			return false;
		}else
			direct_dut++;
	}
	return true;
}

var overlib_str_tmp = "";

function showclient_list(list){
	var code = "";
	networkmap_scanning = 0;

	if(list)
		$("isblockdesc").innerHTML = "<#btn_remove#>";	

	code +='<table width="100%" align="center" id="client_list_table"><tr><td>';

	for(var i = 1; i < client_list_row.length; i++){
		var client_list_col = client_list_row[i].split('>');

		if(!isASUSDevice(client_list_col[1]))
			continue;

		code +='<table width="800" class="border3">';

		code +='<tr id="row'+i+'">';

		for(var j = 0; j < client_list_col.length; j++){
			if(j == 0){
				code +='<td width="6%" class="border2" height="30px;" title="'+DEVICE_TYPE[client_list_col[0]]+'">';
				code +='<div class="device_img3"></div></td>';
			}
			else if(j == 1){
				if(client_list_col[1] != "")
					code += '<td width="23%" class="border2">'+ '<div class="ClientName">' + client_list_col[1] + '</div>' + '<div class="macName">' + client_list_col[3] + '</div>' + '</td>';	// Show Device-name
			}
			else if(j == 2){
				if(client_list_col[1] != "")	
					//code += '<td width="15%" class="ClientName">'+ client_list_col[7] +'</td>';	// Show SSID
					code += '<td width="27%" class="border2">'+ '<div class="ClientName">' + client_list_col[2] + '</div>' + '<div class="macName">' + client_list_col[8] + '</div>' + '</td>';	// Show SSID
			}
			else if(j == 3){
					code += '<td width="5%" class="border4" height="30px;"><div class="wifi_logo"></div></td>';	
			}
			else if(j == 4){
					code += '<td width="30%" class="border2"><div class="findname">' + client_list_col[7] +'</div></td>';	
			}
			else if(j == 5){
					code += '<td width="6%" class="border4"><div border="0px"><a title="<#select_IP#>" target="_blank" href="http://'+ client_list_col[2] +'"><img src="../images/arrow-right.png" width="32px" hight="20px" style="border:0px"></a></div></td>';	
			}
			else
				code += '';
		}
		
		code += '</tr></table>';
	}
	code +='</tr></td></table>';
	$("client_list_Block").innerHTML = code;
	$("client_list_Block").style.display = "none";

	if(direct_dut == 1)
  		location.href = "index.asp";

	if($('client_list_table').innerHTML == "<tbody></tbody>"){
		code ='<tr><td style="color:#FFCC00;" colspan="4"><#IPConnection_VSList_Norule#></td></tr>'
		$("client_list_Block").innerHTML = code;
	}
}

function LoadingProgress1(seconds){
	$("LoadingBar").style.visibility = "visible";
	
	y = y + progress;
	if(typeof(seconds) == "number" && seconds >= 0){
		if(seconds != 0){
			$("proceeding_img1").style.width = Math.round(y) + "%";
			$("proceeding_img_text1").innerHTML = Math.round(y) + "%";
	
			if($("loading_block1")){
				$("proceeding_img_text1").style.width = document.getElementById("loading_block1").clientWidth;
				$("proceeding_img_text1").style.marginLeft = "100px";
			}	
			--seconds;
			setTimeout("LoadingProgress1("+seconds+");", 1000);
		}
		else{
			$("proceeding_img_text1").innerHTML = "<#Main_alert_proceeding_desc3#>";
			y = 0;
			location.href = "find_device.asp?flag=scan_finish";
		}
	}
}

function showLoadingBar1(seconds){
	loadingSeconds = seconds;
	progress = 100/loadingSeconds;
	y = 0;
	LoadingProgress1(seconds);
}

function _showloadingbar(){
	rescan = 1;
	$("LoadingBar").style.display ="";
	$("client_list_Block").style.display ="none";
	$("refresh_list").className = "button_gen_dis"
	showLoadingBar1(5);
	
}

function networkmap_update(){
	setTimeout("_showloadingbar();", 1000);
	FormActions("/findasus.cgi", "refresh_networkmap", "", "");
	document.form.target = "hidden_frame";
	document.form.current_page.value = "find_device.asp?flag=scan_finish";
	document.form.submit();
}

</script>
</head>

<body class="Levelfind" onload="load_body();">
<noscript>
	<div class="popup_bg" style="visibility:visible; z-index:999;">
		<div style="margin:200px auto; width:300px; background-color:#006699; color:#FFFFFF; line-height:150%; border:3px solid #FFF; padding:5px;"><#not_support_script#></p></div>
	</div>
</noscript>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="current_page" value="find_device.asp">
<input type="hidden" name="next_page" value="find_device.asp">
<input type="hidden" name="flag" value="">
</form>

<div id="hiddenMask" class="popup_bg">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
		<tr>
		<td>
			<div class="drword" id="drword"><#Main_alert_proceeding_desc4#> <#Main_alert_proceeding_desc1#>...
				<br/>
				<br/>
	    </div>
			<div id="wireless_client_detect" style="margin-left:10px;position:absolute;display:none">
				<img src="images/loading.gif">
				<div style="margin:-45px 0 0 75px;"><#QKSet_Internet_Setup_fail_method1#></div>
			</div>   
			<div class="drImg"><img src="images/alertImg.png"></div>
			<div style="height:100px; "></div>
		</td>
		</tr>
	</table>
</div>

<!--For content in ifame-->	
<!--[if !lte IE 6]>-->
<div class="banner_t" align="center">
	<img src="images/New_ui/asustitle.png" width="218" height="54" align="left">
	<span class="modelName_top" style="margin-top:20px;">Device Discovery Web Assistant</span>
</div>


<table width="998px" border="0" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td height="0px" width="998px" valign="top"></td>
	</tr>
  <tr>
    <td class="bgd1">
    <div class="bgd">			
						<table width="85%" style="padding:10px 10px 10px 10px;" align="center" cellpadding="4" cellspacing="0">
	  					<tr>
	    					<td style="padding:10px 10px 10px 10px;" class="border1" height="300px" valign="top">
									<div id="client_list_Block"></div>
									<div id="LoadingBar" style="display:none">
										<table width="80%" style="padding:70px 10px 10px 10px;" align="center" cellpadding="4" cellspacing="0">
											<tr>
												<td height="80">
													<div id="connHint" style="margin:8px auto; width:85%;"><span style="font-weight:bolder;" id="stassid">Web Assistant is now searching for ASUS networking devices, please wait... </span></div>
													<div class="Bar_container1">
														<span id="proceeding_img_text1" style="display:none;"></span>
														<div id="proceeding_img1"></div>
													</div>
												</td>
											</tr>
										</table>
									</div>									
	    					</td>
	  					</tr>
	 					</table>
    </div>	
    </td>
  </tr>
</table>
<table width="998px"  border="0" align="center" cellpadding="0" cellspacing="0" background="/images/New_ui/middown_bg.png">
	<tr>
	<tr><td>
<div style="text-align: center;">
	<img height="25px" id="leftBtn" onclick="showNextItem(0);" style="visibility:hidden;cursor:pointer;" src="/images/arrow-left.png">
	<input type="button" id="refresh_list" class="button_gen" onclick="networkmap_update();" value="<#CTL_refresh#>">
	<img height="25px" id="rightBtn" onclick="showNextItem(1);" style="visibility:hidden;cursor:pointer;" src="/images/arrow-right.png">
</div>	
	</td></tr>
		<td height="27" style="background: url('/images/New_ui/bottom_bg.png') no-repeat">&nbsp;</td>
	</tr>
</table>

</body>
</html>


