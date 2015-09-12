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
<title><#Web_Title#> - <#TimeMach#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<link rel="stylesheet" type="text/css" href="app_installation.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="/form.js"></script>
<script>

window.onresize = function() {
	if(document.getElementById("folderTree_panel").style.display == "block") {
		cal_panel_block("folderTree_panel", 0.25);
	}
} 

function initial(){
	show_menu();
	document.getElementById("_APP_Installation").innerHTML = '<table><tbody><tr><td><div class="_APP_Installation"></div></td><td><div style="width:120px;"><#menu5_4#></div></td></tr></tbody></table>';
	document.getElementById("_APP_Installation").className = "menu_clicked";

	if('<% nvram_get("tm_device_name"); %>' != '')
		document.getElementById("tmPath").innerHTML = '/mnt/<% nvram_get("tm_device_name"); %>';
	else
		document.getElementById("tmPath").innerHTML = '<div style="margin-left:5px;color:#FC0"><#DM_Install_partition#></div>';
	
	if(document.form.timemachine_enable.value == "0"){
		document.getElementById("backupPath_tr").style.display = "none";
		document.getElementById("volSize_tr").style.display = "none";
	}
	else{
		document.getElementById("backupPath_tr").style.display = "";
		document.getElementById("volSize_tr").style.display = "";
	}

	if(document.form.tm_vol_size.value != "")	
		document.form.tm_vol_size.value = document.form.tm_vol_size.value/1024;

	detectUSBStatusApp();
}

function selPartition(){
	show_partition();
	cal_panel_block("folderTree_panel", 0.25);
	$("#folderTree_panel").fadeIn(300);
}

function cancel_folderTree(){
	$("#folderTree_panel").fadeOut(300);
}

function show_partition(){
 	require(['/require/modules/diskList.js'], function(diskList){
		var htmlcode = "";
		var mounted_partition = 0;
		
		htmlcode += '<table align="center" style="margin:auto;border-collapse:collapse;">';

 		var usbDevicesList = diskList.list();
		for(var i=0; i < usbDevicesList.length; i++){
			for(var j=0; j < usbDevicesList[i].partition.length; j++){
				var all_accessable_size = simpleNum(usbDevicesList[i].partition[j].size-usbDevicesList[i].partition[j].used);
				var all_total_size = simpleNum(usbDevicesList[i].partition[j].size);

				if(usbDevicesList[i].partition[j].status== "unmounted")
					continue;

				if(usbDevicesList[i].partition[j].isAppDev){
					if(all_accessable_size > 1)
						htmlcode += '<tr style="cursor:pointer;" onclick="setPart(\''+ usbDevicesList[i].partition[j].partName +'\', \''+ all_accessable_size +'\', \''+ all_total_size +'\');"><td class="app_table_radius_left"><div class="iconUSBdisk"></div></td><td class="app_table_radius_right" style="width:250px;">\n';
					else
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk_noquota"></div></td><td class="app_table_radius_right" style="width:250px;">\n';
					htmlcode += '<div class="app_desc"><b>'+ usbDevicesList[i].partition[j].partName + ' <span style="color:#FC0;">(active)</span></b></div>';
				}
				else{
					if(all_accessable_size > 1)
						htmlcode += '<tr style="cursor:pointer;" onclick="setPart(\''+ usbDevicesList[i].partition[j].partName +'\', \''+ all_accessable_size +'\', \''+ all_total_size +'\');"><td class="app_table_radius_left"><div class="iconUSBdisk"></div></td><td class="app_table_radius_right" style="width:250px;">\n';
					else
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk_noquota"></div></td><td class="app_table_radius_right" style="width:250px;">\n';
					htmlcode += '<div class="app_desc"><b>'+ usbDevicesList[i].partition[j].partName + '</b></div>'; 
				}

				if(all_accessable_size > 1)
					htmlcode += '<div class="app_desc"><#Availablespace#>: <b>'+ all_accessable_size+" GB" + '</b></div>'; 
				else
					htmlcode += '<div class="app_desc"><#Availablespace#>: <b>'+ all_accessable_size+" GB <span style=\'color:#FFCC00\'>(Disk quota can not less than 1GB)" + '</span></b></div>'; 

				htmlcode += '<div class="app_desc"><#Totalspace#>: <b>'+ all_total_size+" GB" + '</b></div>'; 
				htmlcode += '</div><br/><br/></td></tr>\n';
				mounted_partition++;
			}
		}

		if(mounted_partition == 0)
			htmlcode += '<tr height="300px"><td colspan="2"><span class="app_name" style="line-height:100%"><#no_usb_found#></span></td></tr>\n';

		document.getElementById("partition_div").innerHTML = htmlcode;
	});	
}

var totalSpace;
var availSpace;
function setPart(_part, _avail, _total){
	document.getElementById("tmPath").innerHTML = "/mnt/" + _part;
	document.form.tm_device_name.value = _part;
	cancel_folderTree();
	totalSpace = _total;
	availSpace = _avail;
	document.getElementById("maxVolSize").innerHTML = "<#Availablespace#>: " + _avail + " GB";
	document.form.tm_vol_size.value = "";
}

function detectUSBStatusApp(){
 	require(['/require/modules/diskList.js'], function(diskList){
		setInterval(function(){
			diskList.update(show_partition); 
		}, 2000);
	});
}

function applyRule(){
	if(document.form.tm_device_name.value == "" && document.form.timemachine_enable.value == "1"){
		alert("Change the Backup Path button to \"Select\" and the text \"Select the USB storage device that you want to access.\"");
		return false;
	}

	if(!validator.rangeAllowZero(document.form.tm_vol_size, 0, parseInt(availSpace), document.form.tm_vol_size.value))
		return false;
	else
		document.form.tm_vol_size.value = document.form.tm_vol_size.value*1024;

	document.form.tm_ui_setting.value = "1";
	showLoading(); 
	document.form.submit();
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<!-- floder tree-->
<div id="folderTree_panel" class="panel_folder">
	<table>
		<tr>
			<td>
				<div style="width:450px;font-family:Arial;font-size:13px;font-weight:bolder; margin-top:23px;margin-left:30px;"><#DM_Install_partition#> :</div>
			</td>
		</tr>
	</table>
	<div id="partition_div" class="folder_tree" style="margin-top:15px;height:335px;">
		<#no_usb_found#>
	</div>
	<div style="background-image:url(images/Tree/bg_02.png);background-repeat:no-repeat;height:90px;margin-top:5px;">		
		<input class="button_gen" type="button" style="margin-left:40%;margin-top:18px;" onclick="cancel_folderTree();" value="<#CTL_Cancel#>">
	</div>
</div>

<div id="hiddenMask" class="popup_bg">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
		<tr>
		<td>
			<div class="drword" id="drword" style="height:110px;"><#Main_alert_proceeding_desc4#> <#Main_alert_proceeding_desc1#>...
				<br/>
				<br/>
	    </div>
		  <div class="drImg"><img src="images/alertImg.png"></div>
			<div style="height:70px;"></div>
		</td>
		</tr>
	</table>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame" autocomplete="off">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_TimeMachine.asp">
<input type="hidden" name="next_page" value="Advanced_TimeMachine.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_timemachine">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="tm_ui_setting" value="">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="timemachine_enable" value="<% nvram_get("timemachine_enable"); %>">
<input type="hidden" name="tm_device_name" value="<% nvram_get("tm_device_name"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td width="17">&nbsp;</td>
	
	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>
	
	<td valign="top">
	<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
	<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td align="left" valign="top">
	  <table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle" style="-webkit-border-radius: 3px;-moz-border-radius: 3px;border-radius:3px;">
		<tbody>
		<tr>
			<td bgcolor="#4D595D" valign="top" height="680px">
				<div>&nbsp;</div>
				<div style="width:730px">
					<table width="730px">
						<tr>
							<td align="left">
								<span class="formfonttitle"><#TimeMach#></span>
							</td>
							<td align="right">
								<img onclick="go_setting('/APP_Installation.asp')" align="right" style="cursor:pointer;position:absolute;margin-left:-20px;margin-top:-30px;" title="Back to USB Extension" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
							</td>
						</tr>
					</table>
				</div>
				<div style="margin:5px;"><img src="/images/New_ui/export/line_export.png"></div>
				<div class="formfontdesc" style="line-height:20px;font-style:italic;font-size: 14px;">
					<table>
						<tr>
							<td>
								<img src="/images/New_ui/USBExt/time_machine_banner.png">
							</td>
							<td>
								1. <#TimeMach_enable#><br>
								2. <#TimeMach_target#> <br>
								3. <#TimeMach_usage_limit#><br>
								4. <#TimeMach_backup#> ( <a href="http://www.asus.com/support/Knowledge-Detail/11/2/RTAC68U/3FEED048-5AC2-4B97-ABAE-DE609DDBC151/" target="_blank" style="text-decoration:underline;"><#TimeMach_AppleURL#></a> )<br>
								5. <a href="https://www.youtube.com/watch?v=Bc3oYW1cmcQ" target="_blank" style="text-decoration:underline;"><#TimeMach_tutol#></a><br>
								6. <a href="http://www.asus.com/support/Knowledge-Detail/11/2/RTAC68U/25DFAE22-873C-4796-91C4-5CF1F08A2064/" target="_blank" style="text-decoration:underline;"><#TimeMach_FAQ#></a><br>
								<span style="color:#FC0">
									* <#TimeMach_recommand1#> <br>
									* <#TimeMach_recommand2#> <br>
									* <#TimeMach_recommand3#>
								</span>
							</td>
						</tr>
					</table>
				</div>			  

			  <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" style="margin-top:8px">
					<thead>
					<tr>
						<td colspan="2"><#t2BC#></td>
					</tr>
					</thead>							

					<tr>
						<th><#TimeMach_enable#></th>
						<td>
							<div class="left" style="width:94px; float:left; cursor:pointer;" id="radio_timemachine_enable"></div>
							<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
							<script type="text/javascript">
								$('#radio_timemachine_enable').iphoneSwitch('<% nvram_get("timemachine_enable"); %>', 
									 function() {
										document.form.timemachine_enable.value = "1";
										$("#backupPath_tr").fadeIn(500);
										$("#volSize_tr").fadeIn(500);
									 },
									 function() {
										document.form.timemachine_enable.value = "0";
										$("#backupPath_tr").fadeOut(300);
										$("#volSize_tr").fadeOut(300);
									 }
								);
							</script>			
							</div>	

						</td>
					</tr>

					<tr id="backupPath_tr">
						<th><#TimeMach_backuppath#></a></th>
						<td>
							<input class="button_gen" onclick="selPartition()" type="button" value="<#Select_btn#>"/>
							<span id="tmPath" style="font-family: Lucida Console;"></span>
		   			</td>
					</tr>
					<tr id="volSize_tr">
						<th><#TimeMach_vol_size#></a></th>
						<td>
							<input id="tm_vol_size" name="tm_vol_size" maxlength="5" class="input_6_table" type="text" maxLength="8" value="<% nvram_get("tm_vol_size"); %>" onKeyPress="return validator.isNumber(this,event);" placeholder="0" autocorrect="off" autocapitalize="off"/> GB (0: <#Limitless#>)
							&nbsp;<span id="maxVolSize"></span>
						</td>
					</tr>
				</table>	
	
				<div class="apply_gen">
					<input class="button_gen" onclick="applyRule()" type="button" value="<#CTL_apply#>"/>
				</div>
			</td>
		</tr>
		</tbody>	
	  </table>
		</td>
	</tr>
	</table>				
			<!--===================================End of Main Content===========================================-->
	</td>
  <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>
</form>					

<div id="footer"></div>
</body>
</html>
