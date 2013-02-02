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

var all_accessable_size = parent.simpleNum2(parent.computeallpools(diskOrder, "size")-parent.computeallpools(diskOrder, "size_in_use"));
var all_total_size = parent.simpleNum2(parent.computeallpools(diskOrder, "size"));
var mountedNum = parent.getDiskMountedNum(diskOrder);
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
	flash_button();
	showtext($("disk_model_name"), parent.getDiskModelName(diskOrder));
	showtext($("disk_total_size"), parent.getDiskTotalSize(diskOrder));

	if(parent.media_support == -1)
		$("mediaserver_hyperlink").style.display = "none";
	
	if(WebDav_support == -1)
		$("clouddiskstr").style.display = "none";

	if(mountedNum > 0){
		showtext($("disk_total_size"), all_total_size+" GB");		
		showtext($("disk_avail_size"), all_accessable_size+" GB");		
		//$("show_remove_button").style.display = "";
		showdisklink();
		DMhint(); //Download Master Hint for user
	}
	else{
		$("mounted_item1").style.display = "none";
		$("mounted_item2").style.display = "none";
		
		//$("show_removed_string").style.display = "";
		$("unmounted_refresh").style.display = "";
	}
	if(sw_mode == "2" || sw_mode == "3")
		$("aidisk_hyperlink").style.display = "none";

	for(var i = 0; i < apps_array.length; i++){
		if(apps_array[i][0] == "downloadmaster" && apps_array[i][4] == "yes" && apps_array[i][3] == "yes"){
			if(_DMDiskNum == diskOrder || foreign_disks().length == 1) {
				$("dmLink").style.display = "";
				break;
			}
		}
	}
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
			$("selected_account_link").href = 'ftp://'+accounts[0]+'@<% nvram_get("ddns_hostname_x"); %>';
			showtext($("selected_account_str"), 'ftp://<% nvram_get("ddns_hostname_x"); %>');
			$("selected_account_link_LAN").href = 'ftp://'+accounts[0]+'@<% nvram_get("lan_ipaddr"); %>';
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
				$("ftp_account_link").href = 'ftp://'+accounts[0]+'@<% nvram_get("lan_ipaddr"); %>';
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
	parent.location.href = "http://" + location.host + ":8081";
}

function remove_disk(){
	var str = "<#Safelyremovedisk_confirm#>";
	if(confirm(str)){
		parent.showLoading();
		
		document.diskForm.action = "safely_remove_disk.asp";
		document.diskForm.disk.value = parent.getDiskPort(this.diskOrder);
		setTimeout("document.diskForm.submit();", 1);
	}
}

function DMhint(){
	var size_of_first_partition = 0;
	var format_of_first_partition = "";
	var	mnt_type = '<% nvram_get("mnt_type"); %>';	
	for(var i = 0; i < parent.pool_names().length; ++i){
		if(parent.per_pane_pool_usage_kilobytes(i, diskOrder)[0] && parent.per_pane_pool_usage_kilobytes(i, diskOrder)[0] > 0){
			size_of_first_partition = parent.simpleNum(parent.per_pane_pool_usage_kilobytes(i, diskOrder)[0]);
			format_of_first_partition = parent.pool_types()[i];
			break;
		}
	}
	//alert(format_of_first_partition);
	/*if(format_of_first_partition != 'vfat'
			&& format_of_first_partition != 'msdos'
			&& format_of_first_partition != 'ext2'
			&& format_of_first_partition != 'ext3'
			&& format_of_first_partition != 'fuseblk'){*/
	/*if(mnt_type == "ntfs"){
		$("DMhint").style.display = "block";
		$("DMFail_reason").innerHTML = "<#DM_reason1#>";
	}
	else if(size_of_first_partition <= 1){	// 0.5 = 512 Mb.
		$("DMhint").style.display = "block";
		$("DMFail_reason").innerHTML = "<#DM_reason2#>";
	}*/
}
</script>
</head>

<body class="statusbody" onload="initial();">
<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px">
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
    	<p class="formfonttitle_nwm"><#Totalspace#>:</p>
    	<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px; color:#FFFFFF;" id="disk_total_size"></p>
      <img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
  </tr>

  <tr>
    <td style="padding:5px 10px 0px 15px;">
    	<p class="formfonttitle_nwm"><#Availablespace#>:</p>
    	<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px; color:#FFFFFF;" id="disk_avail_size"></p>
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
			<span style="text-decoration:underline;font-family:Lucida Console;">file://<% nvram_get("lan_ipaddr"); %></span>
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
