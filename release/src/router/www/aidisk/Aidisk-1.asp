<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>AiDisk Wizard</title>
<link rel="stylesheet" type="text/css" href="../NM_style.css">
<link rel="stylesheet" type="text/css" href="aidisk.css">
<link rel="stylesheet" type="text/css" href="/form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script>
var dummyShareway = '<% nvram_get("dummyShareway"); %>';
var FTP_status = parent.get_ftp_status();  // FTP  0=disable 1=enable
var FTP_mode = parent.get_share_management_status("ftp");  // if share by account. 1=no 2=yes
var accounts = [<% get_all_accounts(); %>];
var ddns_enable = '<% nvram_get("ddns_enable_x"); %>';
var ddns_server = '<% nvram_get("ddns_server_x"); %>';
var ddns_hostname = '<% nvram_get("ddns_hostname_x"); %>';
var format_of_first_partition = parent.pool_types()[0]; //"ntfs";

function initial(){
	parent.hideLoading();
	showdisklink();
}

function showdisklink(){
	if(detect_mount_status() == 0){ // No USB disk plug.
		$("AiDiskWelcome_desp").style.display = 'none';
		$("linkdiskbox").style.display = 'none';
		$("Nodisk_hint").style.display = 'block';
		$("gotonext").style.display = 'none';
		return;
	}
	else if(dummyShareway != ""){  // Ever config aidisk wizard
		$("AiDiskWelcome_desp").style.display = 'none';
		$("linkdiskbox").style.display = 'block';
		$("settingBtn").innerHTML = "<#CTL_Reset_OOB#>";

		show_share_link();
	}
	else{  // Never config aidisk wizard
		$("linkdiskbox").style.display = 'none';
//		$("AiDisk_scenerio").style.display = 'block';
	}	
	// access the disk from LAN
}

function show_share_link(){

	/*if(NN_status == 1 && nav == false)
		$("ie_link").style.display = "block";
	if(FTP_status == 1 && FTP_mode == 1)
		$("notie_link").style.display = "block";
	else{
		$("noLAN_link").style.display = "block";
		
		if(NN_status != 1)
			showtext($("noLAN_link"), "<#linktodisk_no_1#>");
		else
			showtext($("noLAN_link"), "<#linktodisk_no_2#>");
	}*/
	
	//alert("FTP"+FTP_status);
	// access the disk from WAN
	if(FTP_status == 1 && ddns_enable == 1 && ddns_server.length > 0 && ddns_hostname.length > 0){
		if(FTP_mode == 1 || dummyShareway == 0){
			$("ddnslink1").style.display = ""; 
			$("desc_2").style.display = ""; 
			$("ddnslink1_LAN").style.display = ""; 			
			showtext($("ddnslink1_LAN"), '<#t2LAN#>&nbsp;<#AiDisk_linktoFTP_fromInternet#>&nbsp;<a id="ddnslink1_LAN_link" target="_blank" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("lan_ipaddr"); %></a>');
			$("ddnslink1_LAN_link").href = 'ftp://<% nvram_get("lan_ipaddr"); %>';
		}
		else if(FTP_mode == 2){
			$("ddnslink2").style.display = "";
			$("desc_2").style.display = ""; 
			$("ddnslink2_LAN").style.display = ""; 			
			showtext($("ddnslink2_LAN"), '<#t2LAN#>&nbsp;<#AiDisk_linktoFTP_fromInternet#>&nbsp;<a id="ddnslink2_LAN_link" target="_blank" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("lan_ipaddr"); %></a>');
			$("ddnslink2_LAN_link").href = 'ftp://'+accounts[0]+'@<% nvram_get("lan_ipaddr"); %>';
			$("ddnslink2_LAN_link").innerHTML = 'ftp://<% nvram_get("lan_ipaddr"); %>';
		}
	}
	else{
		$("noWAN_link").style.display = "";		
		if(FTP_status != 1)
			showtext($("noWAN_link"), '<#linktoFTP_no_1#>');
		else if(ddns_enable != 1){
			showtext($("noWAN_link"), "<#linktoFTP_no_2#>");
			$("desc_2").style.display = ""; 
			$("ddnslink1_LAN").style.display = "";			
			if(FTP_mode == 1){
					showtext($("ddnslink1_LAN"), '<#t2LAN#>&nbsp;<#AiDisk_linktoFTP_fromInternet#>&nbsp;<a id="ddnslink1_LAN_link" target="_blank" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("lan_ipaddr"); %></a>');
					$("ddnslink1_LAN_link").href = 'ftp://<% nvram_get("lan_ipaddr"); %>';
			}else{
					showtext($("ddnslink1_LAN"), '<#t2LAN#>&nbsp;<#AiDisk_linktoFTP_fromInternet#>&nbsp;<a id="ddnslink1_LAN_link" target="_blank" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("lan_ipaddr"); %></a>');
					$("ddnslink1_LAN_link").href = 'ftp://'+accounts[0]+'@<% nvram_get("lan_ipaddr"); %>';
					$("ddnslink1_LAN_link").innerHTML = 'ftp://<% nvram_get("lan_ipaddr"); %>';
			}		
		}else if(ddns_hostname.length <= 0){
			showtext($("noWAN_link"), "<#linktoFTP_no_3#>");
		}else
			alert("FTP and ddns exception");
	}
}

function detect_mount_status(){
	var mount_num = 0;
	for(var i = 0; i < parent.foreign_disk_total_mounted_number().length; ++i)
		mount_num += parent.foreign_disk_total_mounted_number()[i];
	return mount_num;
}

function go_next_page(){
	document.redirectForm.action = "/aidisk/Aidisk-2.asp";
	//document.redirectForm.target = "_self";
	document.redirectForm.submit();
}
</script>
</head>
<body onload="initial();">
<form method="GET" name="redirectForm" action="">
<input type="hidden" name="flag" value="">
</form>

<div class="aidisk1_table">
<table>
<tr>
  <td valign="top" bgcolor="#4d595d" style="padding-top:25px;">
  <table width="740" height="125" border="0">
	<!-- start Step 0 -->  
	<tr>
		<td class="formfonttitle">
			<span style="margin-left:3px;"><#AiDiskWelcome_title#></span>
			<img onclick="go_setting_parent('/APP_Installation.asp')" align="right" style="cursor:pointer;margin-right:20px;margin-top:-20px;" title="Go to APP Gallery" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
  	</td>
  </tr>
   
  <tr>
  	<td height="20">
  		<img src="/images/New_ui/export/line_export.png" />
  	</td>
  </tr>

	<tr>
		<td>
			<div style="width:660px; line-height:180%;">
	  		<div id="Nodisk_hint" class="alert_string" style="display:none;"><#no_usb_found#></div>

				<div id="AiDiskWelcome_desp">
			  	<#AiDiskWelcome_desp#>
			  	<ul>
			  		<li><#AiDiskWelcome_desp1#></li>
			  		<li><#AiDiskWelcome_desp2#></li>
						<li><#AiDisk_moreconfig#></li>
		  		</ul>
		  	</div>
		  	
		  	<div id="linkdiskbox" >
		  			<span style="margin-left:5px;"><#AiDisk_wizard_text_box_title3#></span><br/>
		  			<ul>
		  				<li> 
		  					<span id="noWAN_link" style="display:none;"></span>
		  					<span id="ddnslink1" style="display:none;">
		  					<#Internet#>&nbsp;<#AiDisk_linktoFTP_fromInternet#>&nbsp;<a target="_blank" href="ftp://<% nvram_get("ddns_hostname_x"); %>" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("ddns_hostname_x"); %></a>
		  					</span>
		  					<span id="ddnslink2" style="display:none;">
		  					<#Internet#>&nbsp;<#AiDisk_linktoFTP_fromInternet#>&nbsp;<a target="_blank" href="ftp://<% nvram_get("ddns_hostname_x"); %>" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("ddns_hostname_x"); %></a>
		  					</span>
		  				</li>
		  				<li id="desc_2" style="display:none;margin-top:8px;">
		  					<span id="ddnslink1_LAN" style="display:none;">
		  					<!-- #t2LAN#>&nbsp;<#AiDisk_linktoFTP_fromInternet#>&nbsp;<a target="_blank" href="ftp://<% nvram_get("lan_ipaddr"); %>" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("lan_ipaddr"); %></a -->
		  					</span>
		  					<span id="ddnslink2_LAN" style="display:none;">
		  					<!-- #t2LAN#>&nbsp;<#AiDisk_linktoFTP_fromInternet#>&nbsp;<a target="_blank" href="ftp://<% nvram_get("lan_ipaddr"); %>" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("lan_ipaddr"); %></a -->
		  					</span>
		  				</li>
							<li><#AiDisk_moreconfig#></li>
							<li><#Aidisk_authority_hint#></li>
		  			</ul>
		  	</div>		
			</div>
	  </td>
  </tr>
      
  <tr>
  	<td align="center" width="740px" height="60px">
			<div id="gotonext">
  			<a href="javascript:go_next_page();"><div class="titlebtn" style="margin-left:300px;_margin-left:150px;" align="center"><span id="settingBtn" style="*width:190px;"><#btn_go#></span></div></a>
			</div>
  	</td>
  </tr>
<!-- end -->    
  </table>

  </td>
</tr>  
</table>
</div>  <!--  //Viz-->
</body>
</html>
