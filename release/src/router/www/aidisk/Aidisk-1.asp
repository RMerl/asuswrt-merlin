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

function initial(){
	parent.hideLoading();

	if(FTP_mode == 1)
		document.getElementById("noFTP_Hint").style.display = "";

 	require(['/require/modules/diskList.js'], function(diskList){
		var mount_num = 0;
 		var usbDevicesList = diskList.list();

		for(var i=0; i < usbDevicesList.length; ++i)
			mount_num += usbDevicesList[i].mountNumber;

		if(mount_num == 0){ // No USB disk plug.
			document.getElementById("AiDiskWelcome_desp").style.display = 'none';
			document.getElementById("linkdiskbox").style.display = 'none';
			document.getElementById("Nodisk_hint").style.display = 'block';
			document.getElementById("gotonext").style.display = 'none';
			return;
		}
		else if(dummyShareway != ""){  // Ever config aidisk wizard
			document.getElementById("AiDiskWelcome_desp").style.display = 'none';
			document.getElementById("linkdiskbox").style.display = 'block';
			document.getElementById("settingBtn").innerHTML = "<#CTL_Reset_OOB#>";

			show_share_link();
		}
		else{
			document.getElementById("linkdiskbox").style.display = 'none';
		}	
	});

	if(document.getElementById("tosLink").style.display == "")
		update_tosLink_url();
	if(document.getElementById("tosLink2").style.display == "")
		update_tosLink2_url();
}
var preferLang = parent.document.form.preferred_lang.value.toLowerCase();

function update_tosLink_url(){														
	if(preferLang == "cn")
		document.getElementById("tosLink").href = "http://www.asus.com.cn";
	else if(preferLang == "ms")
		document.getElementById("tosLink").href = "http://www.asus.com/my";
	else if(preferLang == "da")
		document.getElementById("tosLink").href = "http://www.asus.com/dk";
	else if(preferLang == "sv")
		document.getElementById("tosLink").href = "http://www.asus.com/se";
	else if(preferLang == "uk")
		document.getElementById("tosLink").href = "http://www.asus.com/ua";									
	else if(preferLang == "tw" || preferLang == "cz" || preferLang == "pl" || preferLang == "ro" ||
		preferLang == "ru" || preferLang == "de" || preferLang == "fr" || preferLang == "hu" ||
		preferLang == "tr" || preferLang == "th" || preferLang == "no" || preferLang == "it" ||
		preferLang == "fi" || preferLang == "br" || preferLang == "jp" || preferLang == "es"
	){
		document.getElementById("tosLink").href = "http://www.asus.com/" + preferLang;
	}
	else
		document.getElementById("tosLink").href = "http://www.asus.com/us";

	document.getElementById("tosLink").href += "/Terms_of_Use_Notice_Privacy_Policy/Official-Site";
}

function update_tosLink2_url(){
	if(preferLang == "cn")
		document.getElementById("tosLink2").href = "http://www.asus.com.cn";
	else if(preferLang == "ms")
		document.getElementById("tosLink2").href = "http://www.asus.com/my";
	else if(preferLang == "da")
		document.getElementById("tosLink2").href = "http://www.asus.com/dk";
	else if(preferLang == "sv")
		document.getElementById("tosLink2").href = "http://www.asus.com/se";
	else if(preferLang == "uk")
		document.getElementById("tosLink2").href = "http://www.asus.com/ua";
	else if(preferLang == "tw" || preferLang == "cz" || preferLang == "pl" || preferLang == "ro" ||
		preferLang == "ru" || preferLang == "de" || preferLang == "fr" || preferLang == "hu" ||
		preferLang == "tr" || preferLang == "th" || preferLang == "no" || preferLang == "it" ||
		preferLang == "fi" || preferLang == "br" || preferLang == "jp" || preferLang == "es"
	){
		document.getElementById("tosLink2").href = "http://www.asus.com/" + preferLang;
	}
	else
		document.getElementById("tosLink2").href = "http://www.asus.com/us";

	document.getElementById("tosLink2").href += "/Terms_of_Use_Notice_Privacy_Policy/Official-Site";
}

function show_share_link(){
	//alert("FTP"+FTP_status);
	// access the disk from WAN
	if(FTP_status == 1 && ddns_enable == 1 && ddns_server.length > 0 && ddns_hostname.length > 0){
		if(FTP_mode == 1 || dummyShareway == 0){
			document.getElementById("ddnslink1").style.display = ""; 
			showtext(document.getElementById("ddnslink1"), 'Internet FTP address: <a id="ddnslink1_link" target="_blank" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("ddns_hostname_x"); %></a>');
						
			document.getElementById("desc_2").style.display = ""; 
			document.getElementById("ddnslink1_LAN").style.display = "";
			showtext(document.getElementById("ddnslink1_LAN"), 'LAN FTP address: <a id="ddnslink1_LAN_link" target="_blank" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("lan_ipaddr"); %></a>');
			
		}
		else if(FTP_mode == 2){
			document.getElementById("ddnslink2").style.display = "";
			showtext(document.getElementById("ddnslink2"), 'Internet FTP address: <a id="ddnslink2_link" target="_blank" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("ddns_hostname_x"); %></a>');
						
			document.getElementById("desc_2").style.display = ""; 
			document.getElementById("ddnslink2_LAN").style.display = ""; 			
			showtext(document.getElementById("ddnslink2_LAN"), 'LAN FTP address: <a id="ddnslink2_LAN_link" target="_blank" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("lan_ipaddr"); %></a>');
			
		}
	}
	else{
		document.getElementById("noWAN_link").style.display = "";		
		if(FTP_status != 1)
			showtext(document.getElementById("noWAN_link"), '<#linktoFTP_no_1#>');
		else if(ddns_enable != 1){
			showtext(document.getElementById("noWAN_link"), "<#linktoFTP_no_2#>");
			document.getElementById("desc_2").style.display = ""; 
			document.getElementById("ddnslink1_LAN").style.display = "";			
			if(FTP_mode == 1){
					showtext(document.getElementById("ddnslink1_LAN"), 'LAN FTP address: <a id="ddnslink1_LAN_link" target="_blank" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("lan_ipaddr"); %></a>');
					
			}else{
					showtext(document.getElementById("ddnslink1_LAN"), 'LAN FTP address: <a id="ddnslink1_LAN_link" target="_blank" style="text-decoration: underline; font-family:Lucida Console;">ftp://<% nvram_get("lan_ipaddr"); %></a>');
						
			}		
		}else if(ddns_hostname.length <= 0){
			showtext(document.getElementById("noWAN_link"), "<#linktoFTP_no_3#>");
		}else
			alert("FTP and ddns exception");
	}
}

function detect_mount_status(){
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
			<img onclick="go_setting_parent('/APP_Installation.asp')" align="right" style="cursor:pointer;margin-right:20px;margin-top:-20px;" title="<#Menu_usb_application#>" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
  	</td>
  </tr>
   
  <tr>
  	<td height="20">
  		<img src="/images/New_ui/export/line_export.png" />
  	</td>
  </tr>

	<tr>
		<td>
			<div style="width:660px;line-height:180%;font-size:16px;margin-left:30px;">
	  			<div id="Nodisk_hint" class="alert_string" style="display:none;"><#no_usb_found#></div>

				<div id="AiDiskWelcome_desp">
			  	<#AiDiskWelcome_desp#>
			  	<ul>
			  		<li><#AiDiskWelcome_desp1#></li>
			  		<li><#AiDiskWelcome_desp2#></li>
						<li><#AiDisk_moreconfig#></li>
						<li><#NSlookup_help#></li>
						<li>
							<a id="tosLink" style="cursor:pointer;font-family:Lucida Console;text-decoration:underline;" target="_blank" href="">
								<#DDNS_termofservice_Title#>
							</a>
						</li>
		  		</ul>
		  	</div>
		  	
		  	<div id="linkdiskbox" >
		  			<span style="margin-left:5px;"><#AiDisk_wizard_text_box_title3#></span><br/>
		  			<ul>
		  				<li id="noFTP_Hint" style="display:none;">
		  					<span><#AiDisk_shareHint#></span>
		  				</li>
		  				<li> 
		  					<span id="noWAN_link" style="display:none;"></span>
		  					<span id="ddnslink1" style="display:none;"></span>
		  					<span id="ddnslink2" style="display:none;"></span>
		  				</li>
		  				<li id="desc_2" style="display:none;margin-top:8px;">
		  					<span id="ddnslink1_LAN" style="display:none;"></span>
		  					<span id="ddnslink2_LAN" style="display:none;"></span>
		  				</li>
							<li><#AiDisk_moreconfig#></li>
							<li><#Aidisk_authority_hint#></li>
							<li><#NSlookup_help#></li>	
							<li>
								<a id="tosLink2" style="cursor:pointer;font-family:Lucida Console;text-decoration:underline;" target="_blank" href="">
									<#DDNS_termofservice_Title#>
								</a>
							</li>	
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
