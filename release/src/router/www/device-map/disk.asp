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
<script>
var diskOrder = parent.getSelectedDiskOrder();
var _DMDiskNum = (pool_devices().getIndexByValue('<% nvram_get("apps_dev"); %>') < foreign_disk_total_mounted_number()[0])? 0 : 1;

var ddns_result = '<% nvram_get("ddns_return_code_chk");%>'; //Boyau
<% get_AiDisk_status(); %>

var FTP_status = get_ftp_status();  // FTP
var FTP_mode = get_share_management_status("ftp");
var accounts = [<% get_all_accounts(); %>];

var ddns_enable = '<% nvram_get("ddns_enable_x"); %>';
var ddns_server = '<% nvram_get("ddns_server_x"); %>';
var ddns_hostname = '<% nvram_get("ddns_hostname_x"); %>';
var apps_array = <% apps_info("asus"); %>;
var apps_dev = "<% nvram_get("apps_dev"); %>";
var dummyShareway = '<% nvram_get("dummyShareway"); %>';

function initial(){
	$("t0").className = "tabclick_NW";
	$("t1").className = "tab_NW";
	flash_button();

	if(!parent.media_support)
		$("mediaserver_hyperlink").style.display = "none";
	
	if(!WebDav_support)
		$("clouddiskstr").style.display = "none";

	// Hide disk utility temporarily.
	if(parent.diskUtility_support){
		$("diskTab").style.display = "";
	}

	showDiskUsage(parent.usbPorts[diskOrder-1]);

	if(sw_mode == "2" || sw_mode == "3" || sw_mode == "4")
		$("aidisk_hyperlink").style.display = "none";
}

var thisForeignDisksIndex;
function showDiskUsage(device){
	document.getElementById("disk_model_name").innerHTML = device.deviceName;

	if(device.mountNumber > 0){
		showtext($("disk_total_size"), simpleNum(device.totalSize) + " GB");		
		showtext($("disk_avail_size"), simpleNum(device.totalSize - device.totalUsed) +" GB");		
		showdisklink();
		$("mounted_item1").style.display = "";
		$("mounted_item2").style.display = "";
        $("remove_disk_item").style.display = "";
        $("mount_disk_item").style.display = "none";
		$("unmounted_refresh").style.display = "none";
	}
	else{
		$("mounted_item1").style.display = "none";
		$("mounted_item2").style.display = "none";
        $("remove_disk_item").style.display = "none";
        $("mount_disk_item").style.display = "";
		$("unmounted_refresh").style.display = "";
	}

	for(var i = 0; i < apps_array.length; i++){
		if(apps_array[i][0] == "downloadmaster" && apps_array[i][4] == "yes" && apps_array[i][3] == "yes"){
			if(device.hasAppDev){
				$("dmLink").style.display = "";
			}
			break;
		}
	}

	thisForeignDisksIndex = device.node;
}

function showdisklink(){
	// access the disk from WAN
	if(sw_mode != "3" && FTP_status == 1 && ddns_enable == 1 && ddns_server.length > 0 && ddns_hostname.length > 0){
		if(FTP_mode == 1){
			if((ddns_result.indexOf(",200") >= 0) || //Boyau
   			 (ddns_result.indexOf(",220") >= 0) || 
   			 (ddns_result.indexOf(",230") >= 0) )
   				$("ddnslink1").style.display = "";
			else
   				$("desc_1").style.display = "none";

			$("desc_2").style.display = "";
			$("ddnslink1_LAN").style.display = "";
		}
		else{
			$("ddnslink2").style.display = "";
			$("desc_2").style.display = "";			
			$("ddnslink2_LAN").style.display = "";
			$("selected_account_link").href = 'ftp://'+accounts[0]+':<% nvram_get("http_passwd"); %>@<% nvram_get("ddns_hostname_x"); %>';
			showtext($("selected_account_str"), 'ftp://<% nvram_get("ddns_hostname_x"); %>');
			$("selected_account_link_LAN").href = 'ftp://'+accounts[0]+':<% nvram_get("http_passwd"); %>@<% nvram_get("lan_ipaddr"); %>';
			showtext($("selected_account_str_LAN"), 'ftp://<% nvram_get("lan_ipaddr"); %>');
		}
		
		if('<% nvram_get("enable_samba"); %>' == '1'
				&& navigator.appName.indexOf("Microsoft") >= 0
				&& (( navigator.userAgent.indexOf("MSIE 7.0") >= 0 && navigator.userAgent.indexOf("Trident") < 0 )
							|| ( navigator.userAgent.indexOf("MSIE 7.0") >= 0 && navigator.userAgent.indexOf("Trident/4.0") >= 0 )
							|| ( navigator.userAgent.indexOf("MSIE 8.0") >= 0 && navigator.userAgent.indexOf("Trident/4.0") >= 0 ))
				&& FTP_mode == 1			
		){
			// IE 7, IE 8
			$("desc_3").style.display = "";
			$("ddnslink3_LAN").style.display = "";
		}else if('<% nvram_get("enable_samba"); %>' == '1'
				&& navigator.appName.indexOf("Microsoft") >= 0){
			// IE else
			$("desc_3").style.display = "";
			$("ddnslink_non_LAN").style.display = "";	
		}	
	}
	else{
		$("noWAN_link").style.display = "";
		$("ddnslink3").style.display = "";
		if(FTP_mode == 1){
				$("ftp_account_link").href = 'ftp://<% nvram_get("lan_ipaddr"); %>';
		}else{
				$("ftp_account_link").href = 'ftp://'+accounts[0]+':<% nvram_get("http_passwd"); %>@<% nvram_get("lan_ipaddr"); %>';
				$("ftp_account_link").innerHTML = 'ftp://<% nvram_get("lan_ipaddr"); %>';
		}		
						
		if('<% nvram_get("enable_samba"); %>' == '1'
				&& navigator.appName.indexOf("Microsoft") >= 0
				&& (( navigator.userAgent.indexOf("MSIE 7.0") >= 0 && navigator.userAgent.indexOf("Trident") < 0 )
							|| ( navigator.userAgent.indexOf("MSIE 7.0") >= 0 && navigator.userAgent.indexOf("Trident/4.0") >= 0 )
							|| ( navigator.userAgent.indexOf("MSIE 8.0") >= 0 && navigator.userAgent.indexOf("Trident/4.0") >= 0 ))
		){
			// IE 7, IE 8
			$("desc_3").style.display = "";
			$("ddnslink3_LAN").style.display = "";
		}else if('<% nvram_get("enable_samba"); %>' == '1'
				&& navigator.appName.indexOf("Microsoft") >= 0){
			// IE else
			$("desc_3").style.display = "";
			$("ddnslink_non_LAN").style.display = "";		
		}	
	
		if(FTP_status != 1){
			$("ddnslink3").style.display = "none";
			showtext($("noWAN_link"), '<#linktoFTP_no_1#><br>');
		}else if(ddns_enable != 1 && sw_mode != "2" && sw_mode != "3")			
			showtext($("noWAN_link"), "<br><#linktoFTP_no_2#>");
		else if(ddns_hostname.length <= 0 && sw_mode != "2" && sw_mode != "3")
			showtext($("noWAN_link"), "<br><#linktoFTP_no_3#>");
		else
			return false;
			//alert("System error!");
	}
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

	if(parent.location.host.split(":").length > 1)
		parent.location.href = "http://" + parent.location.host.split(":")[0] + ":" + dm_http_port;
	else
		parent.location.href = "http://" + parent.location.host + ":" + dm_http_port;
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

function mount_disk(){
	var str = "<#Safelymountdisk_confirm#>";
	if(confirm(str)){
		parent.showLoading();
		
		document.diskForm.action = "safely_mount_disk.asp";
		document.diskForm.disk.value = parent.getDiskPort(this.diskOrder);
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
				<div id="t0" class="tabclick_NW" align="center" style="font-weight: bolder;margin-right:2px; width:100px;" onclick="">
					<span id="span1" style="cursor:pointer;font-weight: bolder;"><#diskUtility_information#></span>
				</div>
			</td>
  		<td>
				<div id="t1" class="tab_NW" align="center" style="font-weight: bolder;margin-right:2px; width:100px;" onclick="location.href='disk_utility.asp'">
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

<table id="remove_disk_item" width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px">
  <tr>
    <td height="50" style="padding:10px 15px 0px 15px;">
    	<p class="formfonttitle_nwm" style="float:left;width:138px; "><#Safelyremovedisk_title#>:</p>
    	<input id="show_remove_button" class="button_gen" type="button" class="button" onclick="remove_disk();" value="<#btn_remove#>">
    	<div id="show_removed_string" style="display:none;"><#Safelyremovedisk#></div>
    </td>
  </tr>
</table>

<table id="mount_disk_item" width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px">
  <tr>
    <td height="50" style="padding:10px 15px 0px 15px;">
    	<p class="formfonttitle_nwm" style="float:left;width:138px; "><#Safelymountdisk_title#>:</p>
    	<input id="show_mount_button" class="button_gen" type="button" class="button" onclick="mount_disk();" value="<#btn_mount#>">
    	<div id="show_mounted_string" style="display:none;"><#Safelymountdisk#></div>
    </td>
  </tr>
</table>



<div id="unmounted_refresh" style="padding:5px 0px 5px 25px; display:none">
<ul style="font-size:11px; font-family:Arial; padding:0px; margin:0px; list-style:outside; line-height:150%;">
	<li><#DiskStatus_refresh1#><a href="/" target="_parent"><#DiskStatus_refresh2#></a><#DiskStatus_refresh3#></li>
</ul>
</div>
<div id="mounted_item2" style="padding:5px 0px 5px 25px;">
<ul style="font-size:11px; font-family:Arial; padding:0px; margin:0px; list-style:outside; line-height:150%;">
	<li id="desc_1">
	  <span id="ddnslink1" style="display:none;"><#Internet#>&nbsp;<#AiDisk_linktoFTP_fromInternet#><br><a target="_blank" href="ftp://<% nvram_get("ddns_hostname_x"); %>" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("ddns_hostname_x"); %></a></span>
	  <span id="ddnslink2" style="display:none;"><#Internet#>&nbsp;<#AiDisk_linktoFTP_fromInternet#><br><a id="selected_account_link" href="" onclick="alert('<#AiDiskWelcome_desp1#>');" target="_blank"><span id="selected_account_str"></span></a></span>
	  <span id="ddnslink3" style="display:none;"><#AiDisk_linktoFTP_fromInternet#><br><a id="ftp_account_link" target="_blank" href="" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("lan_ipaddr"); %></a></span>
		<span id="noWAN_link" style="display:none;"></span>
	</li>
	<li id="desc_2" style="display:none;">
		<span id="ddnslink1_LAN" style="display:none;"><#linktodisk#><br>
	  	<a target="_blank" href="ftp://<% nvram_get("lan_ipaddr"); %>" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("lan_ipaddr"); %></a>
	  </span>
	  <span id="ddnslink2_LAN" style="display:none;"><#linktodisk#><br>
	  	<a id="selected_account_link_LAN" href="" target="_blank" style="text-decoration: underline; font-family:Lucida Console;"><span id="selected_account_str_LAN"></span></a>
	  </span>
	</li>
	<li id="desc_3" style="display:none;">
		<span id="ddnslink3_LAN" style="display:none;"><#menu5_4_1#><span id="clouddiskstr"> / Cloud Disk :</span><br>
			<a target="_blank" href="\\<% nvram_get("lan_ipaddr"); %>" style="text-decoration:underline; font-family:Lucida Console;">\\<% nvram_get("lan_ipaddr"); %></a>
		</span>
		<span id="ddnslink_non_LAN" style="display:none;"><#menu5_4_1#><span id="clouddiskstr"> / Cloud Disk :</span><br>
			<span style="text-decoration:underline;font-family:Lucida Console;" title="<#samba_tips#>">file://<% nvram_get("lan_ipaddr"); %></span>
		</span>		
	</li>
</ul>
<div id="DMhint" class="DMhint">
	<#DM_hint1#> <span id="DMFail_reason"></span>
</div>
</div>

<form method="post" name="diskForm" action="">
<input type="hidden" name="disk" value="">
</form>
</body>
</html>
