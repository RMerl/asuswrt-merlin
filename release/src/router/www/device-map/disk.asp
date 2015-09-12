<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title></title>
<link href="../form_style.css" rel="stylesheet" type="text/css" />
<link href="../NM_style.css" rel="stylesheet" type="text/css" />
<style>
a:link {
	text-decoration: underline;
	color: #FFFFFF;
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
</style>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
if(parent.location.pathname.search("index") === -1) top.location.href = "../index.asp";

var diskOrder = parent.getSelectedDiskOrder();

<% get_AiDisk_status(); %>

var apps_array = <% apps_info("asus"); %>;

function initial(){
	document.getElementById("t0").className = "tabclick_NW";
	document.getElementById("t1").className = "tab_NW";
	flash_button();

	if(!parent.media_support)
		document.getElementById("mediaserver_hyperlink").style.display = "none";
	
	// Hide disk utility temporarily.
	if(parent.diskUtility_support){
		document.getElementById("diskTab").style.display = "";
	}

	showDiskUsage(parent.usbPorts[diskOrder-1]);

	if(sw_mode == "2" || sw_mode == "3" || sw_mode == "4")
		document.getElementById("aidisk_hyperlink").style.display = "none";
		
	if(noaidisk_support)
		document.getElementById("aidisk_hyperlink").style.display = "none";
	
	if((based_modelid == "RT-AC87U" || based_modelid == "RT-AC5300" || based_modelid == "RT-AC88U" || based_modelid == "RT-AC3100") && parent.currentUsbPort == 0){
		document.getElementById('reduce_usb3_table').style.display = "";
	}		
}

var thisForeignDisksIndex;
function showDiskUsage(device){
	document.getElementById("disk_model_name").innerHTML = device.deviceName;

	if(device.mountNumber > 0){
		showtext(document.getElementById("disk_total_size"), simpleNum(device.totalSize) + " GB");		
		showtext(document.getElementById("disk_avail_size"), simpleNum(device.totalSize - device.totalUsed) +" GB");
		document.getElementById("mounted_item1").style.display = "";		
		document.getElementById("unmounted_refresh").style.display = "none";
	}
	else{
		document.getElementById("mounted_item1").style.display = "none";
		document.getElementById("unmounted_refresh").style.display = "";
	}

	for(var i = 0; i < apps_array.length; i++){
		if(apps_array[i][0] == "downloadmaster" && apps_array[i][4] == "yes" && apps_array[i][3] == "yes"){
			if(device.hasAppDev){
				document.getElementById("dmLink").style.display = "";
			}
			break;
		}
	}

	thisForeignDisksIndex = device.node;
}

function goUPnP(){
	parent.location.href = "/mediaserver.asp";
}

function gotoAidisk(){
	parent.location.href = "/aidisk.asp";
}

function gotoDM(){
	var dm_http_port = '<% nvram_get("dm_http_port"); %>';
	if(dm_http_port == "")
		dm_http_port = "8081";

	var dm_url = "";
	if(parent.location.host.split(":").length > 1)
		dm_url = "http://" + parent.location.host.split(":")[0] + ":" + dm_http_port;
	else
		dm_url = "http://" + parent.location.host + ":" + dm_http_port;

	window.open(dm_url);
}

function remove_disk(){
	var str = "<#Safelyremovedisk_confirm#>";
	if(confirm(str)){
		parent.showLoading();
		
		document.diskForm.action = "safely_remove_disk.asp";
		document.diskForm.disk.value = thisForeignDisksIndex;
		setTimeout("document.diskForm.submit();", 1);
	}
}
</script>
</head>

<body class="statusbody" onload="initial();">
<table>
	<tr>
	<td>		
		<table id="diskTab" width="100px" border="0" align="left" style="margin-left:5px;display:none;" cellpadding="0" cellspacing="0">
  		<td>
				<div id="t0" class="tabclick_NW" align="center" style="font-weight: bolder;margin-right:2px;" onclick="">
					<span id="span1" style="cursor:pointer;font-weight: bolder;"><#diskUtility_information#></span>
				</div>
			</td>
  		<td>
				<div id="t1" class="tab_NW" align="center" style="font-weight: bolder;margin-right:2px;" onclick="location.href='disk_utility.asp'">
					<span id="span1" style="cursor:pointer;font-weight: bolder;"><#diskUtility#></span>
				</div>
			</td>
		</table>
	</td>
</tr>
</table>
<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px" style="margin-top:-3px;">
	<tr>
    <td style="padding:5px 10px 0px 15px;">
    	<p class="formfonttitle_nwm"><#Modelname#>:</p>
			<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px; color:#FFFFFF;" id="disk_model_name"></p>
      <img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
  </tr>
</table>

<table id="mounted_item1" width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px">
  <tr>
    <td style="padding:5px 10px 0px 15px;">
    	<p class="formfonttitle_nwm"><#Availablespace#>:</p>
    	<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px; color:#FFFFFF;" id="disk_avail_size"></p>
      <img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
  </tr>

  <tr>
    <td style="padding:5px 10px 0px 15px;">
    	<p class="formfonttitle_nwm"><#Totalspace#>:</p>
    	<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px; color:#FFFFFF;" id="disk_total_size"></p>
      <img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
  </tr>

  <tr id="mediaserver_hyperlink">
    <td style="padding:10px 15px 0px 15px;;">
    	<p class="formfonttitle_nwm" style="float:left;width:138px;"><#UPnPMediaServer#>:</p>
      <input type="button" class="button_gen" onclick="goUPnP();" value="<#btn_go#>" >
    	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
  </tr>

  <tr id="aidisk_hyperlink">
    <td height="50" style="padding:10px 15px 0px 15px;">
    	<p class="formfonttitle_nwm" style="float:left;width:138px;"><#AiDiskWizard#>:</p>
    	<input type="button" class="button_gen" onclick="gotoAidisk();" value="<#btn_go#>" >
      <img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
  </tr>

  <tr id="dmLink" style="display:none;">
    <td height="50" style="padding:10px 15px 0px 15px;">
    	<p class="formfonttitle_nwm" style="float:left;width:138px;">Download Master</p>
    	<input type="button" class="button_gen" onclick="gotoDM();" value="<#btn_go#>" >
      <img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
  </tr>
</table>

<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px">
  <tr>
    <td height="50" style="padding:10px 15px 0px 15px;">
    	<p class="formfonttitle_nwm" style="float:left;width:138px; "><#Safelyremovedisk_title#>:</p>
    	<input id="show_remove_button" class="button_gen" type="button" class="button" onclick="remove_disk();" value="<#btn_remove#>">
    	<div id="show_removed_string" style="display:none;"><#Safelyremovedisk#></div>
		 <img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
  </tr>
</table>

<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px" id="reduce_usb3_table" style="display:none">
	<tr>
		<td height="50" style="padding:10px 15px 0px 15px;">
			<p class="formfonttitle_nwm" style="float:left;width:138px; " onmouseover="parent.overHint(24);" onmouseout="parent.nd();"><#WLANConfig11b_x_ReduceUSB3#></p>
			<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
				<input type="hidden" name="current_page" value="/index.asp">
				<input type="hidden" name="next_page" value="/index.asp">
				<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
				<input type="hidden" name="action_mode" value="apply">
				<input type="hidden" name="action_script" value="reboot">
				<input type="hidden" name="action_wait" value="160">
				<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
				<input type="hidden" name="usb_usb3" value="<% nvram_get("usb_usb3"); %>">
			
			<div align="center" class="left" style="width:120px; float:left; cursor:pointer;margin-top:-7px;" id="reduce_usb3_enable"></div>
			<script type="text/javascript">
				var flag = document.form.usb_usb3.value == 1 ?  0: 1;
				$('#reduce_usb3_enable').iphoneSwitch( flag,
					function(){		//ON:0
						document.form.usb_usb3.value = 0;
						document.form.submit();
					},
					function(){		//OFF:1
						document.form.usb_usb3.value = 1;
						document.form.submit();
					}
				);
			</script>
			</form>
		</td>
	</tr>
</table>

<div id="unmounted_refresh" style="padding:5px 0px 5px 25px; display:none">
<ul style="font-size:11px; font-family:Arial; padding:0px; margin:0px; list-style:outside; line-height:150%;">
	<li><#DiskStatus_refresh1#><a href="/" target="_parent"><#DiskStatus_refresh2#></a><#DiskStatus_refresh3#></li>
</ul>
</div>

<form method="post" name="diskForm" action="">
<input type="hidden" name="disk" value="">
</form>
</body>
</html>
