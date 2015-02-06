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
<title><#Web_Title#> - <#menu3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="app_installation.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>

if('<% nvram_get("start_aicloud"); %>' == '0')
	location.href = "cloud__main.asp";

var $j = jQuery.noConflict();

<% wanlink(); %>

<% apps_action(); %> //trigger apps_action.

var cloud_status;
window.onresize = cal_agreement_block;
var curState = '<% nvram_get("webdav_aidisk"); %>';

var _apps_action = '<% get_parameter("apps_action"); %>';
if(_apps_action == 'cancel')
	_apps_action = '';
	
var apps_array = <% apps_info("asus"); %>;
if(apps_array == ""){
		apps_array =["aicloud", "", "", "no", "no", "", "", "","", "", "", "", ""];
}

<% apps_state_info(); %>

var apps_download_percent_done = 0;

var stoppullstate = 0;
var isinstall = 0;
var installPercent = 1;
var default_apps_array = new Array();
var appnum = 0;
	
var is_cloud_installed = false;
for(var x=0; x < apps_array.length; x++){	//check if AiCloud 2.0 has installed
	if(apps_array[x][0]=='aicloud' && apps_array[x][3]=='yes' && apps_array[x][4]=='yes'){
		is_cloud_installed = true;
	}
}
var ddns_hostname = '<% nvram_get("ddns_hostname_x"); %>';
var https_port = '<% nvram_get("webdav_https_port"); %>';

if(tmo_support)
	var theUrl = "cellspot.router"; 
else
	var theUrl = "router.asus.com";

function initial(){
	show_menu();
	addOnlineHelp($("faq0"), ["samba"]);
	addOnlineHelp($("faq1"), ["ASUSWRT", "port", "forwarding"]);
	addOnlineHelp($("faq2"), ["ASUSWRT", "DMZ"]);
	addOnlineHelp($("faq3"), ["WOL", "BIOS"]);

	$("app_state").style.display = "";
	
	if(cloudsync_support){		//aicloud builded in
			divdisplayctrl("none", "none", "none", "");
			$('btn_cloud_uninstall').style.display = "none";
			
	}else{		// aicloud ipk
		
		if(_apps_action == '' && !is_cloud_installed){	//setup not yet	
				divdisplayctrl("", "none", "none", "none");
		}		
		
  	if(_apps_action == '' && 
			(apps_state_upgrade == 4 || apps_state_upgrade == "") && 
			(apps_state_enable == 2 || apps_state_enable == "") &&
			(apps_state_update == 2 || apps_state_update == "") && 
			(apps_state_remove == 2 || apps_state_remove == "") &&
			(apps_state_switch == 5 || apps_state_switch == "") && 
			(apps_state_autorun == 4 || apps_state_autorun == "") && 
			(apps_state_install == 5 || apps_state_install == "") &&
			is_cloud_installed){	//setup install is done
					$('cloudsetup_movie').style.display = "none";
					divdisplayctrl("none", "none", "none", "");
		}
		else{	//setup status else 
					update_appstate();
		}
	}

	switch(valid_is_wan_ip(wanlink_ipaddr())){
		/* private */
		case 0:
			if(https_port == 443)
				$("accessMethod").innerHTML = "<#AiCloud_enter#> <a id=\"cloud_url\" style=\"font-weight: bolder;text-decoration: underline;\" href=\"https://router.asus.com\" target=\"_blank\">https://router.asus.com</a>";
			else{
				$("accessMethod").innerHTML = "<#AiCloud_enter#> <a id=\"cloud_url\" style=\"font-weight: bolder;text-decoration: underline;\" href=\"https://router.asus.com\" target=\"_blank\">https://router.asus.com</a>";
				$('cloud_url').href = "https://"+ theUrl +":" + https_port;
				$('cloud_url').innerHTML = "https://"+ theUrl +":" + https_port;
			}
			break;
		/* public */
		case 1:
			if('<% nvram_get("ddns_enable_x"); %>' == '1' && ddns_hostname != ''){
				if(https_port == 443) // if the port number of https is 443, hide it
					$("accessMethod").innerHTML = "<#AiCloud_enter#> <a style=\"font-weight: bolder;text-decoration: underline;word-break:break-all;\" href=\"https://"+ ddns_hostname + ":"+ https_port +"\" target=\"_blank\">https://"+ ddns_hostname +"</a><br />";
				else
					$("accessMethod").innerHTML = "<#AiCloud_enter#> <a style=\"font-weight: bolder;text-decoration: underline;word-break:break-all;\" href=\"https://"+ ddns_hostname + ":"+ https_port +"\" target=\"_blank\">https://"+ ddns_hostname +":"+ https_port +"</a><br />";
				
				$("accessMethod").innerHTML += "To modify the ddns name, please click <a style=\"font-weight: bolder;text-decoration: underline;\" href=\"/Advanced_ASUSDDNS_Content.asp?af=DDNSName\" target=\"_blank\">here</a>.";
			}
			else{
				$("accessMethod").innerHTML = "<#AiCloud_enter#> <a id=\"cloud_url\" style=\"font-weight: bolder;text-decoration: underline;\" href=\"https://router.asus.com\" target=\"_blank\">https://router.asus.com</a>";
				$("accessMethod").innerHTML += "<br/>Register for an ASUS DDNS <a style=\"font-weight: bolder;text-decoration: underline;\" href=\"/Advanced_ASUSDDNS_Content.asp\" target=\"_blank\">here</a> to access the cloud disk online.";
			}
			break;
	}

	cal_agreement_block();

	if(!rrsut_support)
		$("rrsLink").style.display = "none";
}

function valid_is_wan_ip(ip_obj){
  // test if WAN IP is a private IP.
  var A_class_start = inet_network("10.0.0.0");
  var A_class_end = inet_network("10.255.255.255");
  var B_class_start = inet_network("172.16.0.0");
  var B_class_end = inet_network("172.31.255.255");
  var C_class_start = inet_network("192.168.0.0");
  var C_class_end = inet_network("192.168.255.255");
  
  var ip_num = inet_network(ip_obj);
  if(ip_num > A_class_start && ip_num < A_class_end)
		return 0;
  else if(ip_num > B_class_start && ip_num < B_class_end)
		return 0;
  else if(ip_num > C_class_start && ip_num < C_class_end)
		return 0;
	else if(ip_num == 0)
		return 0;
  else
		return 1;
}

function inet_network(ip_str){
	if(!ip_str)
		return -1;
	
	var re = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/;
	if(re.test(ip_str)){
		var v1 = parseInt(RegExp.$1);
		var v2 = parseInt(RegExp.$2);
		var v3 = parseInt(RegExp.$3);
		var v4 = parseInt(RegExp.$4);
		
		if(v1 < 256 && v2 < 256 && v3 < 256 && v4 < 256)
			return v1*256*256*256+v2*256*256+v3*256+v4;
	}
	return -2;
}

function cancel(){
	this.FromObject ="";
	$j("#agreement_panel").fadeOut(300);
	$j('#iphone_switch').animate({backgroundPosition: -37}, "slow", function() {});
	curState = 0;
	$("hiddenMask").style.visibility = "hidden";
}

function _confirm(){
	document.form.webdav_aidisk.value = 1;
	document.form.enable_webdav.value = 1;
	FormActions("start_apply.htm", "apply", "restart_webdav", "3");
	showLoading();
	document.form.submit();
}

function cal_agreement_block(){
	var blockmarginLeft;
	if (window.innerWidth)
		winWidth = window.innerWidth;
	else if ((document.body) && (document.body.clientWidth))
		winWidth = document.body.clientWidth;
		
	if (document.documentElement  && document.documentElement.clientHeight && document.documentElement.clientWidth){
		winWidth = document.documentElement.clientWidth;
	}

	if(winWidth >1050){	
		winPadding = (winWidth-1050)/2;	
		winWidth = 1105;
		blockmarginLeft= (winWidth*0.25)+winPadding;
	}
	else if(winWidth <=1050){
		blockmarginLeft= (winWidth)*0.25+document.body.scrollLeft;	

	}

	$("agreement_panel").style.marginLeft = blockmarginLeft+"px";
}

function apps_form(_act, _name, _flag){
	cookie.set("apps_last", _name, 1000);
	document.app_form.apps_action.value = _act;
	document.app_form.apps_name.value = _name;
	document.app_form.apps_flag.value = _flag;
	document.app_form.submit();
}

function show_partition(){
 	require(['/require/modules/diskList.js'], function(diskList){
		var htmlcode = "";
		var mounted_partition = 0;
			
		htmlcode += '<div class="formfontdesc" id="usbHint3">Please select an disk partition to install ASUS AiCloud 2.0 , the one you choose will be the system disk.</div>';
		htmlcode += '<div class="formfontdesc" id="usbHint4" style="font-size:12px;">Note: Download Master and AiCloud 2.0 should be installed in the same system disk.</div>';			
		htmlcode += '<table align="center" style="margin:auto;border-collapse:collapse;">';

 		var usbDevicesList = diskList.list();
		for(var i=0; i < usbDevicesList.length; i++){
			for(var j=0; j < usbDevicesList[i].partition.length; j++){
				var all_accessable_size = simpleNum(usbDevicesList[i].partition[j].size-usbDevicesList[i].partition[j].used);
				var all_total_size = simpleNum(usbDevicesList[i].partition[j].size);

				if(usbDevicesList[i].partition[j].status == "unmounted")
					continue;
					
				if(usbDevicesList[i].partition[j].isAppDev){
					if(all_accessable_size > 1)
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk" onclick="divdisplayctrl(\'none\', \'none\', \'\', \'none\');apps_form(\'install\',\'aicloud\',\''+usbDevicesList[i].partition[j].mountPoint+'\');"></div></td><td class="app_table_radius_right" style="width:200px;">\n';
					else
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk_noquota"></div></td><td class="app_table_radius_right" style="width:200px;">\n';
					htmlcode += '<div class="app_desc"><b>'+ usbDevicesList[i].partition[j].partName + ' (active)</b></div>';
				}
				else{
					if(all_accessable_size > 1)
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk" onclick="divdisplayctrl(\'none\', \'none\', \'\', \'none\');apps_form(\'switch\',\'aicloud\',\''+usbDevicesList[i].partition[j].mountPoint+'\');"></div></td><td class="app_table_radius_right" style="width:200px;">\n';
					else
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk_noquota"></div></td><td class="app_table_radius_right" style="width:200px;">\n';
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
			htmlcode += '<tr height="360px"><td colspan="2"><span class="app_name" style="line-height:100%"><#no_usb_found#></span></td></tr>\n';

		$("partition_div").innerHTML = htmlcode;
	});
}


function divdisplayctrl(flag1, flag2, flag3, flag4){
	$("cloud_uninstall").style.display = flag1;
	$("partition_div").style.display = flag2;
	$("app_state").style.display = flag3;
	$("cloud_installed").style.display = flag4;

	if(flag1 != "none"){ // AiCloud 2.0 uninstall
		$("return_btn").style.display = "none";
		$("tab_smartsync").style.display="none";
		$("tab_routersync").style.display="none";
		$("tab_setting").style.display="none";
		$("tab_syslog").style.display="none";
	}
	else if(flag2 != "none"){ // partition list
		//detectUSBStatusApp();
		show_partition();
		$("return_btn").style.display = "";
		//calHeight(1);
		$("tab_smartsync").style.display="none";
		$("tab_routersync").style.display="none";
		$("tab_setting").style.display="none";
		$("tab_syslog").style.display="none";
	}
	else if(flag3 != "none"){ // AiCloud 2.0 installing
		$("return_btn").style.display = "none";
		//calHeight(1);
		$("tab_smartsync").style.display="none";
		$("tab_routersync").style.display="none";
		$("tab_setting").style.display="none";
		$("tab_syslog").style.display="none";
	}	
	else if(flag4 != "none"){ // Have AiCloud 2.0 installed
		$("return_btn").style.display = "none";
		//calHeight(1);
		$("tab_smartsync").style.display="";
		$("tab_routersync").style.display="";
		$("tab_setting").style.display="";
		$("tab_syslog").style.display="";
	}
	else{
		$("return_btn").style.display = "none";		
		//calHeight(0);		
 	}

 	//stoppullstate = 1;
}


function check_wan(){		//Don't need check WAN
	divdisplayctrl("none", "", "none", "none");
	//alert("Please make sure you have connected to internet and reinstall.");	
}

function update_appstate(e){
  $j.ajax({
    url: '/update_appstate.asp',
    dataType: 'script',
	
    error: function(xhr){
      update_appstate();
    },
    success: function(response){
			if(stoppullstate == 1)
				return false;
			else if(!check_appstate()){
      	setTimeout("update_appstate();", 1000);
				//calHeight(0);
			}
			else
				update_applist();
		}    
  });
}


function check_appstate(){
	if(_apps_action != "" 
		 && apps_state_upgrade == "" 
		 && apps_state_enable == "" 
		 && apps_state_update == "" 
		 && apps_state_remove == "" 
		 && apps_state_switch == "" 
		 && apps_state_autorun == "" 
		 && apps_state_install == ""){
		return false;
	}

	if((apps_state_upgrade == 4 || apps_state_upgrade == "") 
			&& (apps_state_enable == 2 || apps_state_enable == "") 
			&& (apps_state_update == 2 || apps_state_update == "") 
			&& (apps_state_remove == 2 || apps_state_remove == "") 
			&& (apps_state_switch == 5 || apps_state_switch == "") 
			&& (apps_state_autorun == 4 || apps_state_autorun == "") 
			&& (apps_state_install == 5 || apps_state_install == "")){
		$('cloudsetup_movie').style.display = "none";	
		//divdisplayctrl("none", "none", "none", "");
		return true;
	}

	var errorcode;
	var proceed = 0.6;

	divdisplayctrl("none", "none", "", "none");
	$('cloudsetup_movie').style.display = "";

	if(apps_state_upgrade != 4 && apps_state_upgrade != ""){ // upgrade error handler
		errorcode = "apps_state_upgrade = " + apps_state_upgrade;
		if(apps_state_error == 1)
			$("apps_state_desc").innerHTML = "<#usb_inputerror#>";
		else if(apps_state_error == 2)
			$("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_error == 4)
			$("apps_state_desc").innerHTML = "<#usb_failed_install#>";
		else if(apps_state_error == 6)
			$("apps_state_desc").innerHTML = "<#usb_failed_remote_responding#>";
		else if(apps_state_error == 7)
			$("apps_state_desc").innerHTML = "<#usb_failed_upgrade#>";
		else if(apps_state_error == 9)
			$("apps_state_desc").innerHTML = "<#usb_failed_unmount#>";
		else if(apps_state_error == 10)
			$("apps_state_desc").innerHTML = "<#usb_failed_dev_responding#>";
		else if(apps_state_upgrade == 0)
			$("apps_state_desc").innerHTML = "<#usb_initializing#>";
		else if(apps_state_upgrade == 1){
			if(apps_download_percent > 0 && apps_download_percent <= 100){
				$("apps_state_desc").innerHTML = apps_download_file + " is downloading.. " + " <b>" + apps_download_percent + "</b> <span style='font-size: 16px;'>%</span>";
				apps_download_percent_done = 0;
			}
			else if(apps_download_percent_done > 5){
				if(installPercent > 99)
					installPercent = 99;
				$("loadingicon").style.display = "none";
				$("apps_state_desc").innerHTML = "[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
				installPercent = installPercent + proceed;//*/
			}
			else{
				$("apps_state_desc").innerHTML = "<#usb_initializing#>...";
				apps_download_percent_done++;
			}
		}
		else if(apps_state_upgrade == 2)
			$("apps_state_desc").innerHTML = "<#usb_uninstalling#>";
		else{
			if(apps_depend_action_target != "terminated" && apps_depend_action_target != "error"){
				if(apps_depend_action_target == "")
					$("apps_state_desc").innerHTML = "<b>[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> </b>";
				else
					$("apps_state_desc").innerHTML = "<b>[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> </b>"
							+"<br> <span style='font-size: 16px;'> <#Excute_processing#>："+apps_depend_do+"</span>"
							+"<br> <span style='font-size: 16px;'>"+apps_depend_action+"  "+apps_depend_action_target+"</span>"
							;
			}
			else{
				if(installPercent > 99)
					installPercent = 99;
				$("loadingicon").style.display = "none";
				$("apps_state_desc").innerHTML = "[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
				installPercent = installPercent + proceed;
			}
		}
	}
	else if(apps_state_enable != 2 && apps_state_enable != ""){
		errorcode = "apps_state_enable = " + apps_state_enable;
		if(apps_state_error == 1)
			$("apps_state_desc").innerHTML = "<#usb_failed_unknown#>";
		else if(apps_state_error == 2)
			$("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_error == 3)
			$("apps_state_desc").innerHTML = "<#usb_failed_create_swap#>";
        else if(apps_state_error == 8)
            $("apps_state_desc").innerHTML = "Enable error!";
		else{
			$("loadingicon").style.display = "";
			$("apps_state_desc").innerHTML = "<#QIS_autoMAC_desc2#>";
		}
	}
	else if(apps_state_update != 2 && apps_state_update != ""){
		errorcode = "apps_state_update = " + apps_state_update;
		if(apps_state_error == 1)
			$("apps_state_desc").innerHTML = "<#USB_Application_Preparing#>";
		else if(apps_state_error == 2)
			$("apps_state_desc").innerHTML = "<#USB_Application_No_Internet#>";
		else
			$("apps_state_desc").innerHTML = "Updating...";
	}
	else if(apps_state_remove != 2 && apps_state_remove != ""){
		errorcode = "apps_state_remove = " + apps_state_remove;
		$("apps_state_desc").innerHTML = "<#uninstall_processing#>";
	}
	else if(apps_state_switch != 4 && apps_state_switch != 5 && apps_state_switch != ""){
		errorcode = "apps_state_switch = " + apps_state_switch;
		if(apps_state_error == 1)
			$("apps_state_desc").innerHTML = "<#usb_failed_unknown#>";
		else if(apps_state_error == 2)
			$("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_switch == 1)
			$("apps_state_desc").innerHTML = "<#USB_Application_Stopping#>";
		else if(apps_state_switch == 2)
			$("apps_state_desc").innerHTML = "<#USB_Application_Stopwapping#>";
		else if(apps_state_switch == 3)
			$("apps_state_desc").innerHTML = "<#USB_Application_Partition_Check#>";
		else
			$("apps_state_desc").innerHTML = "<#Excute_processing#>";
	}
	else if(apps_state_autorun != 4 && apps_state_autorun != ""){
		errorcode = "apps_state_autorun = " + apps_state_autorun;
		if(apps_state_error == 1)
			$("apps_state_desc").innerHTML = "<#usb_failed_unknown#>";
		else if(apps_state_error == 2)
			$("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_autorun == 1)
			$("apps_state_desc").innerHTML = "<#USB_Application_disk_checking#>";
		else if(apps_state_install == 2)
			$("apps_state_desc").innerHTML = "<#USB_Application_Swap_creating#>";
		else
			$("apps_state_desc").innerHTML = "<#Auto_Install_processing#>";
	}
	else if(apps_state_install != 5 && apps_state_error > 0){ // install error handler
		if(apps_state_error == 1)
			$("apps_state_desc").innerHTML = "<#usb_inputerror#>";
		else if(apps_state_error == 2)
			$("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_error == 3)
			$("apps_state_desc").innerHTML = "<#usb_failed_create_swap#>";
		else if(apps_state_error == 4)
			$("apps_state_desc").innerHTML = "<#usb_failed_install#>";
		else if(apps_state_error == 5)
			$("apps_state_desc").innerHTML = "<#usb_failed_connect_internet#>";
		else if(apps_state_error == 6)
			$("apps_state_desc").innerHTML = "<#usb_failed_remote_responding#>";
		else if(apps_state_error == 7)
			$("apps_state_desc").innerHTML = "<#usb_failed_upgrade#>";
		else if(apps_state_error == 9)
			$("apps_state_desc").innerHTML = "<#usb_failed_unmount#>";
		else if(apps_state_error == 10)
			$("apps_state_desc").innerHTML = "<#usb_failed_dev_responding#>";

		isinstall = 0;
	}
	else if(apps_state_install != 5 && apps_state_install != ""){
		isinstall = 1;
		errorcode = "_apps_state_install = " + apps_state_install;

		if(apps_state_install == 0)
			$("apps_state_desc").innerHTML = "<#usb_partitioning#>";
		else if(apps_state_install == 1)
			$("apps_state_desc").innerHTML = "<#USB_Application_disk_checking#>";
		else if(apps_state_install == 2)
			$("apps_state_desc").innerHTML = "<#USB_Application_Swap_creating#>";
		else if(apps_state_install == 3){
			if(apps_download_percent > 0 && apps_download_percent <= 100){
				$("apps_state_desc").innerHTML = apps_download_file + " is downloading.. " + " <b>" + apps_download_percent + "</b> <span style='font-size: 16px;'>%</span>";
				apps_download_percent_done = 0;
			}
			else if(apps_download_percent_done > 5){
				if(installPercent > 99)
					installPercent = 99;
				$("loadingicon").style.display = "none";
				$("apps_state_desc").innerHTML = "[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
				installPercent = installPercent + proceed;//*/
			}
			else{
				$("apps_state_desc").innerHTML = "<#usb_initializing#>...";
				apps_download_percent_done++;
			}
		}
		else{
			if(apps_depend_action_target != "terminated" && apps_depend_action_target != "error"){
				if(apps_depend_action_target == "")
					$("apps_state_desc").innerHTML = "<b>[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> </b>";
				else
					$("apps_state_desc").innerHTML = "<b>[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> </b>"
							+"<br> <span style='font-size: 16px;'> <#Excute_processing#>："+apps_depend_do+"</span>"
							+"<br> <span style='font-size: 16px;'>"+apps_depend_action+"  "+apps_depend_action_target+"</span>"
							;
			}
			else{
				if(installPercent > 99)
					installPercent = 99;
				$("loadingicon").style.display = "none";
				$("apps_state_desc").innerHTML = "[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
				installPercent = installPercent + proceed;
			}
		}
	}
	else{
		$("loadingicon").style.display = "";
		$("apps_state_desc").innerHTML = "<#QIS_autoMAC_desc2#>";
	}
	
	if(apps_state_error != 0){
		$("return_btn").style.display = "";
		$("loadingicon").style.display = "none";
		stoppullstate = 1;
	}
	else
		$("return_btn").style.display = "none";

	$("apps_state_desc").innerHTML += '<span class="app_action" onclick="apps_form(\'cancel\',\'\',\'\');">(<#CTL_Cancel#>)</span>';
	return false;
}


function update_applist(e){
  $j.ajax({
    url: '/update_applist.asp',
    dataType: 'script',
	
    error: function(xhr){
      update_applist();
    },
    success: function(response){
			if(isinstall > 0 && cookie.get("apps_last") == "aicloud"){
				$('cloudsetup_movie').style.display = "none";
				setTimeout('divdisplayctrl("none", "none", "none", "");', 100);
			}
			else{
				$('cloudsetup_movie').style.display = "none";
				setTimeout('show_partition();', 100);
				setTimeout('divdisplayctrl("", "none", "none", "none");', 100);
				//setTimeout('show_apps();', 100);
			}
		}    
  });
}

</script>
</head>
<body onload="initial();" onunload="return unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
	<div id="agreement_panel" class="panel_folder" style="margin-top: -100px;display:none;position:absolute;">
			<div class="machineName" style="font-family:Microsoft JhengHei;font-size:12pt;font-weight:bolder; margin-top:25px;margin-left:30px;height:35px;">Term of use</div>
			<div class="folder_tree">Thank you for using our AiCloud 2.0 firmware, AiCloud 2.0 mobile application and AiCloud 2.0 portal website(“AiCloud”). AiCloud 2.0 is provided by ASUSTeK Computer Inc. (“ASUS”). This notice constitutes a valid and binding agreement between ASUS. By using AiCloud 2.0 , YOU, AS A USER, EXPRESSLY ACKNOWLEGE THAT YOU HAVE READ AND UNDERSTAND AND AGREE TO BE BOUND BY THE TERMS OF USE NOTICE (“NOTICE”) AND ANY NEW VERSIONS HEREOF.
<br><br>
<b>1.LICENSE</b><br>
Subject to the terms of use notice, ASUS hereby grants you a limited, personal, non-commercial, non-exclusive license to use AiCloud 2.0 solely as embedded in the ASUS products. This license shall not be sublicensed, and is not transferable except to a person or entity to whom you transfer ownership of the complete ASUS products containing AiCloud 2.0, provided you permanently transfer all rights under this notice and do not retain any full or partial copies of AiCloud 2.0, and the recipient agrees to the NOTICE.
<br><br> 
2.SOFTWARE<br> 
AiCloud 2.0 means AiCloud 2.0 feature in firmware, AiCloud 2.0 mobile application or AiCloud 2.0 portal website ASUS provided in or with the applicable ASUS product including but not limited to any future programming fixes, updates, upgrades and modified versions.
<br><br> 
3.REVERSE RESTRICTION<br>
You shall not reverse engineer, decompile decrypt or disassemble the Software or permit or induce the foregoing except to the extent directly applicable law prohibits enforcement of the foregoing.
<br><br>  
4.CONFIDENTIAL ITY<br>
You shall have a duty to protect and not to disclose or cause to be disclosed in whole or in part of confidential information of AiCloud 2.0 including but not limited to trade secrets, copyrighted material in any form to any third party.
<br><br>  
5.INTELLECTUAL PROPERTY RIGHTS (“IP RIGHTS”)<br>
You acknowledge and agree that all IP Rights, title, and interest to or arising from AiCloud 2.0 are and shall remain the exclusive property of ASUS or its licensor. Nothing in this notice intends to transfer any such IP Rights to You. Any unauthorized use of the IP Rights is a violation of this notice as well as a violation of intellectual property laws, including but not limited to copyright laws and trademark laws. ASUS hereby grants you limited, non-exclusive and revocable right and license for you to (a) install and use AiCloud 2.0 and its related components and documents for personal non-commercial use in compliance with this agreement; (b) create AiCloud 2.0 mobile software ID and use in compliance with this agreement; (c) visit AiCloud 2.0 portal website in compliance with this agreement. AiCloud 2.0 is under protection of Republic of China(“Taiwan”) law and international conventions. The entire responsibility with regard to all data, including but not limited to images, texts, software, music, videos, graphs, and messages you upload, transmit or enter into AiCloud 2.0 is with you and does not reflect the views of ASUS. 
<br><br> 
6.PERMITTED USES<br>
You may: (a) install AiCloud 2.0 on devices that stores sound recordings, audiovisual works and pictures (generically “content”), that you either own, have a legal right to possess and use or have acquired usage rights under a valid license from the content owner; (b) use AiCloud 2.0 to transfer content (“sync”) between your storage and one or more ASUS-branded devices/serives; (c) use AiCloud 2.0 to designate content and other files on your computer or storage to make available for access via the Internet or wireless network to other devices that you own through a web browser, mobile device application or other service (“stream”); and (d) configure your ASUS-branded devices to automatically upload photos that you have taken with their cameras to your computer; and (e) download and install software updates to your ASUS-branded devices, all subject to local laws and regulations governing such activities. You may only sync and stream content with devices that you own or to which you have the right to reproduce or display under a valid license or an applicable law.
<br><br>  
7.PROHIBITED USES<br>
You may not use AiCloud 2.0 in any manner not expressly authorized by this agreement, including but not limited to: (a) decompiling, reverse engineering or otherwise attempting to derive the source code of AiCloud 2.0 ; (b) modifying, translating or otherwise altering AiCloud’s source or object code; (c) using AiCloud 2.0 for the benefit of a third party except as expressly permitted by law; (d) using AiCloud 2.0 to sync content that you do not own, possess under a valid license from the content owner or otherwise have a legal right to sync; (e) using AiCloud 2.0 in conjunction with any kind of automated software such as but not limited to bots, spiders, malware, viruses or adware; (f) using AiCloud 2.0 in a manner that would damage ASUS in any way; (g) using AiCloud 2.0 in any manner prohibited by law, including without limitation to infringe copyrighted materials; (h) using AiCloud 2.0 in any commercial endeavor without ASUS’s express written consent; or (i) transferring AiCloud 2.0 to a third party in any way. AiCloud 2.0 is not fault-tolerant and is not designed, manufactured or intended for use in hazardous environments requiring fail-safe performance in which the failure of AiCloud 2.0 could lead directly to death, personal injury or severe physical or environmental damage ("High Risk Activities"). You agree not to use AiCloud 2.0 in High Risk Activities. ASUS respects intellectual property rights (IPR). You shall not use AiCloud 2.0 in a way infringing other’s IPR. If you use AiCloud 2.0 in a way violating IPR, ASUS reserves respective rights to (a) remove or disable access of contents which ASUS believes to infringe on a third party’s right; (b) disable or close your AiCloud 2.0 account; (c) take legal actions. You shall not sell, lease, provide for download or transfer AiCloud 2.0, including all other ASUS software services, in any way and to any degree. ASUS, its suppliers and licensors reserve the right to take legal actions in the event of any infringement of IPR and business benefit. 
<br><br>  
8.ELIGIBILITY<br>
You acknowledge and agree if you are under the legal age of majority for your country of residence or if your use of AiCloud 2.0 has previously been suspended or prohibited, that any license grant provided you is automatically terminated retroactively. BY DOWNLOADING, INSTALLING OR OTHERWISE USING AiCloud 2.0, YOU REPRESENT THAT YOU ARE OF LEGAL AGE OF MAJORITY IN YOUR COUNTRY OF RESIDENCE AND HAVE NOT BEEN PREVIOUSLY SUSPENDED OR PROHIBITED FROM USING AiCloud 2.0.
<br><br>  
9.FEEDBACK<br> 
By sending any information, material, ideas, concepts or techniques except for personal information to ASUS through AiCloud 2.0 (“Feedbacks”), You acknowledge and agree that: (i) Your Feedbacks shall not contain confidential or proprietary information; (ii) ASUS is under no obligation of any kind of confidentiality, express or implied, regarding the Feedbacks; (iii)ASUS shall be entitled to use or disclose, at ASUS’s sole discretion, the Feedbacks for any purpose, in any way, worldwide; (iv) Your Feedbacks shall become the property of ASUS without any obligation of ASUS to You; and (v) You are not entitled to any compensation or reimbursement of any kind from.
<br><br>  
10.TERM AND TERMINATION<br>
This NOTICE will be effective as of the date you agree this NOTICE and enable the AiCloud 2.0 in firmware or install AiCloud 2.0 in your mobile devices. You may terminate this License at any time by ceasing all use of AiCloud 2.0 and removing AiCloud 2.0 mobile application. Your right will terminate without prior notice from ASUS if we think you fail to comply with any provision of this NOTICE.
<br><br>  
11.WARRANTY DISCLAIMER AND LIMITATION OF LIABILITY<br>
By using AiCloud 2.0, you represent and warranty that (a) you agree to this NOTICE; (b) your right to use AiCloud 2.0 has never been revoked by ASUS before; (c) you use AiCloud 2.0 obeying this NOTICE and laws concerned. ASUS warrants to you that ASUS protect information by de facto standard safety methods and procedures. NOTWITHSTANDING THE ABOVEMENTIONED WARRANTY FOR DE FACTO STANDARD SAFETY METHODS AND PROCEDURES, AICLOUD IS PROVIDED TO YOU “AS IS.” THE ENTIRE RISK AS TO THE QUALITY, DATA SAFETY AND PERFORMANCE OF AICLOUD 2.0 IS WITH YOU, INCLUDING BUT NOT LIMITED TO RELATED COST OF ALL NECESSARY SERVICING, DATA LOSS, INTERNET ATTACKS AND REPAIR UNLESS SERIOUS HUMAN NEGLIGENCE OR INTENDED DAMAGE CAUSED BY ASUS. TO THE EXTENT PERMITTED BY LAW, ASUS AND ITS SUPPLIERS AND LICENSORS MAKES NO WARRANTIES, EXPRESS OR IMPLIED, WITH RESPECT TO AICLOUD OR ANY OTHER INFORMATION OR DOCUMENTATION PROVIDED UNDER THIS EULA OR FOR AICLOUD, INCLUDING BUT NOT LIMITED TO ANY WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE OR AGAINST INFRINGEMENT, OR ANY WARRANTY ARISING OUT OF USAGE OR OUT OF COURSE OF PERFORMANCE. ASUS ALSO MAKES NO WARRANTY AS TO THE RESULTS THAT MAY BE OBTAINED FROM THE USE OF AICLOUD OR AS TO THE ACCURACY OR RELIABILITY OF ANY INFORMATION OBTAINED THROUGH AICLOUD. ASUS AND ITS SUPPLIERS AND LICENSORS ARE NOT RESPONSIBLE FOR THE LOSS RESULTING FROM YOUR USE AND NOT-USE OF AICLOUD 2.0, CONTENTS AND INFORMATION, INCLUDING BUT NOT LIMIT TO SPECIAL, DIRECT, INDIRECT OR NECESSARY LOSS, LOSS OF PUNISHMENT, RELIABILITY OR ANY FORM, EVEN IN THE EVENT THAT ASUS AND ITS SUPPLIERS AND LICENSORS ARE INFORMED IN PRIOR. SOME JURISDICTIONS DO NOT ALLOW THE EXCLUSION OF CERTAIN WARRANTIES OR THE LIMITATION OR EXCLUSION OF LIABILITY FOR CERTAIN DAMAGES. ACCORDINGLY, SOME OF THE ABOVE DISCLAIMERS AND LIMITATIONS OF LIABILITY MAY NOT APPLY TO YOU. As AiCloud 2.0 is a free licensed software, in any event that your use of AiCloud 2.0 causes loss and damage, no matter written in this NOTICE or not, ASUS, its contractors, suppliers, associated companies, employees, agents, third party cooperators, licensors are liable for indemnity for less than 1USD. ASUS sets forth amount and terms herein reflecting the fairness of risk taken by you and ASUS. By using AiCloud 2.0, you agree amounts, terms and disclaimers set forth in this NOTICE. If you do not agree, you may not be able to use AiCloud 2.0.
<br><br>  
12.INDEMNITY<br>
YOU agree to defend, indemnify and hold harmless ASUS and its affiliates, service providers, syndicators, suppliers, licensors, officers, directors and employees, from and against any and all losses, damages, liabilities, and expenses arising out of any claim or demand (including reasonable attorneys' fees and court costs), due to or in connection with YOUR violation of this NOTICE or any applicable law or regulation, or third-party right. In particular, AiCloud 2.0 app is available on Apple Store and Google Play!. You agree to defend, indemnify and hold harmless Apple Store and Google Play!. ASUS reserves the right to recover court costs and a reasonable attorney’s fee. You agree to cooperate with ASUS for related recoveries. ASUS will notify you of related recoveries, actions or lawsuits to a reasonable degree. 
<br><br>  
13.INFORMATION COLLECTION AND USE: PRIVACY POLICY<br>
To use AiCloud 2.0, You have to create an ASUS DDNS name or use an existing one. If you choose not to provide an ASUS DDNS name, You will not be able to use AiCloud 2.0. An ASUS DDNS name collects solely public IPs of your ASUS routers to enable remote access function. An ASUS DDNS name does not collect any information of your usage data, files or personal info. When you create an ASUS DDNS name account, ASUS saves all info collected via the ASUS DDNS name in ASUS servers. By using an ASUS DDNS, you understand and consent to these information collection and usage terms, including (where applicable) the transfer of data into the ASUS server. ASUS does not disclose your personal information to third parties, including but not limited to advertising or marketing agencies. ASUS discloses your personal information to third parties via AiCloud 2.0 only in following circumstances: a.When ASUS provides You technical support for AiCloud 2.0. ASUS requires third parties to sign related agreements to protect your personal information from being disclosed according to the terms in this agreement. b.When disclosing your personal information is to follow legal procedures or public policies to protect public or personal safety, or to protect ASUS property and benefit. If You have any question or further information regarding ASUS AiCloud 2.0 information collection and use, please contact ASUS via privacy@asus.com or address below: ASUS address
<br><br>  
14.INFORMATION COLLECTION AND USE: COOKIES<br>
When you visit AiCloud 2.0 portal website for the first time, ASUS server may save one small text file, a “Cookie”, to your desktop or mobile devices. The Cookie allows AiCloud 2.0 website to retrieve your browsing history. The Cookie helps ASUS provide customized service and better user experience to you. ASUS may use Cookie to measure website traffic volume, to learn areas on the website you have visited and the pattern of your visit.Information collected by Cookie is called “Clickstream”. ASUS uses Clickstream to understand how users are navigated by ASUS websites and traffic volume patterns such as which website users come from. Clickstream helps ASUS develop website guide, recommend products and re-design websites according to users’ browsing behaviors. Clickstream also helps ASUS customize contents, banners and promotion advertisements for each user to make best purchase decisions. ASUS retrieves Clickstream through a third party. You may change Cookie settings according to your needs. Changing Internet Explorer preference settings allows you to get notice to accept or reject Cookies or when Cookie settings changed. Please note that if you reject all Cookies, you will be not able to use services or join activities that require Cookie. 
<br><br>  
15.TRADEMARKS<br>
“ASUS”, and all other ASUS logos and brand names are properties of ASUS and its suppliers and licensors. By using AiCloud 2.0, you agree not to remove, alter or obscure any such marks or logos that appear as part of AiCloud 2.0 or any associated materials that you may receive. This agreement grants you no right to use such marks other than in conjunction with AiCloud 2.0.
<br><br>  
16.TRANSLATIONS<br>
The English version of this agreement is the controlling version. Any translations are provided for convenience only.
<br><br>  
17.MISCELLANEOUS<br>
This agreement constitutes the entire agreement between you and ASUS with respect to AiCloud 2.0 and supersedes all previous communications, representations, understandings and agreements, either oral or written, between you and ASUS regarding AiCloud 2.0. This agreement may not be modified or waived except in writing and signed by an officer or other authorized representative of each party. If any provision is held invalid, all other provisions shall remain valid, unless such invalidity would frustrate the purpose of this agreement. The failure of either party to enforce any rights granted hereunder or to take action against the other party in the event of any breach hereunder will not waive that party’s as to subsequent enforcement of rights or subsequent action in the event of future breaches. AiCloud 2.0 reserves the right to: (a) add or remove functions and features or to provide updates, upgrades or programming fixes to AiCloud 2.0 with or without prior notice to you; (b) require you to agree to a new agreement in order to use any new version of AiCloud 2.0 that it releases; or (c) require you to upgrade to a new version of AiCloud 2.0 by discontinuing service or support to any prior version of AiCloud 2.0 without notice.
</div>
			<div style="background-image:url(images/Tree/bg_02.png);background-repeat:no-repeat;height:90px;">
				<input class="button_gen" type="button" style="margin-left:27%;margin-top:18px;" onclick="cancel();" value="<#CTL_Disagree#>">
				<input class="button_gen" type="button"  onclick="_confirm();" value="<#CTL_Agree#>">
			</div>
	</div>
	<div id="hiddenMask" class="popup_bg" style="z-index:999;">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
	</table>
	<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
	</div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="app_form" action="/cloud_main.asp">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>" disabled>
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>" disabled>
<input type="hidden" name="apps_action" value="">
<input type="hidden" name="apps_path" value="">
<input type="hidden" name="apps_name" value="">
<input type="hidden" name="apps_flag" value="">
</form>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="cloud_main.asp">
<input type="hidden" name="next_page" value="cloud_main.asp">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="enable_cloudsync" value="<% nvram_get("enable_cloudsync"); %>">
<input type="hidden" name="enable_webdav" value="<% nvram_get("enable_webdav"); %>">
<input type="hidden" name="webdav_aidisk" value="<% nvram_get("webdav_aidisk"); %>">
<input type="hidden" name="webdav_proxy" value="<% nvram_get("webdav_proxy"); %>">

<table border="0" align="center" cellpadding="0" cellspacing="0" class="content">
	<tr>
		<td valign="top" width="17">&nbsp;</td>
		<!--=====Beginning of Main Menu=====-->
		<td valign="top" width="202">
			<div id="mainMenu"></div>
			<div id="subMenu"></div>
		</td>
		<td valign="top">
			<div id="tabMenu" class="submenuBlock">
				<table border="0" cellspacing="0" cellpadding="0">
					<tbody>
					<tr>
						<td>
							<div class="tabclick"><span>AiCloud 2.0</span></div>
						</td>
						<td>
							<a href="cloud_sync.asp"><div class="tab" id="tab_smartsync"><span><#smart_sync#></span></div></a>
						</td>
						<td>
							<a id="rrsLink" href="cloud_router_sync.asp"><div class="tab" id="tab_routersync"><span><#Server_Sync#></span></div></a>
						</td>
						<td>
							<a href="cloud_settings.asp"><div class="tab" id="tab_setting"><span><#Settings#></span></div></a>
						</td>
						<td>
							<a href="cloud_syslog.asp"><div class="tab" id="tab_syslog"><span><#Log#></span></div></a>
						</td>
					</tr>
					</tbody>
				</table>
			</div>
<!--==============Beginning of hint content=============-->
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
			  <tr>
					<td align="left" valign="top">
					  <table width="100%" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
							<tr>
							  <td bgcolor="#4D595D" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle">AiCloud 2.0</div>									
									<div><img id="return_btn" onclick="location.href='/cloud_main.asp'" align="right" style="cursor:pointer;position:absolute;margin-left:690px;margin-top:-45px;" title="Back to AiCloud 2.0" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'"></div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div id="cloud_uninstall" style="display:none;">
   										<table>	
  										<tr>
   												<td><div class="formfontdesc" id="usbHint"><#AiCloud_maintext_note#></div></td> 
  										</tr>
											<tr>
   												<td><div class="formfontdesc" id="usbHint2"><#Learn_more#> : <a href="http://aicloud-faq.asuscomm.com/aicloud-faq/" target="_blank" style="color:#FC0;text-decoration: underline; font-family:Lucida Console;">http://aicloud-faq.asuscomm.com/aicloud-faq/</a></div></td> 
  										</tr>  
  										<tr>
   												<td valign="top"><div id="cloud_movie" style="box-shadow: 2px 2px 15px #222;margin-top: 50px;width:400px;height:241px;margin-left:165px;background:url(images/movie.jpg) no-repeat center;cursor:pointer" onClick="window.open('http://www.youtube.com/watch?v=MgIAfG5ZhPs')"></div></td>
  										</tr>  	   
  										<tr>
   												<td align="center" width="740px" height="60px">
													<div id="gotonext">
  														<div class="titlebtn" style="margin-top: 50px;margin-left:300px;_margin-left:150px;cursor:pointer;" align="center"><span id="settingBtn" style="*width:140px;" onClick="check_wan();">Install</span></div>
													</div>
  												</td>
  										</tr>  
   										</table>										
									</div>
									<div id="partition_div" style="display:none;"></div>
									<div id="cloudsetup_movie" style="box-shadow: 2px 2px 15px #222;display:none;width:349px;height:193px;margin-left:165px;margin-top:40px;background:url(images/setup.jpg) no-repeat center;cursor:pointer" onClick="window.open('http://www.youtube.com/watch?v=1zIVzl1h8P4')"></div>
									<div id="app_state" class="app_state" style="display:none;"><span id="apps_state_desc">Loading APP list...</span><img id="loadingicon" style="margin-left:5px;" src="/images/InternetScan.gif"></div>
									<div id="cloud_installed" style="display:none;">
									<table width="100%" height="550px" style="border-collapse:collapse;">

									  <tr bgcolor="#444f53">
									    <td colspan="5" bgcolor="#444f53" class="cloud_main_radius">
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;">
												<#AiCloud_maintext_note#>
													<br/><br/>
													<table width="100%" >
														<tr>
															<td>
																<ul style="margin: 0px;padding-left:15px;" >
																	<li style="margin-top:-5px;">
																 	<div id="accessMethod"></div>
																 	</li>
																 	<li style="margin-top:-5px;">
																	 <#Video_Find#> <a style="font-weight: bolder;text-decoration: underline;" href="http://www.youtube.com/asusrouters" target="_blank">GO</a>
																	</li>
																	<li style="margin-top:-5px;">
																	 <#FAQ_Find#> <a style="font-weight: bolder;text-decoration: underline;" href="http://aicloud-faq.asuscomm.com/aicloud-faq/" target="_blank">GO</a>
																	</li>
																</ul>
															</td>
															<td align="right" valign="top"  nowrap="nowrap">
																<a href="https://play.google.com/store/apps/details?id=com.asustek.aicloud" target="_blank"><img border="0" src="/images/cloudsync/googleplay.png" width="100px"></a>
																<a href="https://itunes.apple.com/us/app/aicloud-lite/id527118674" target="_blank"><img src="/images/cloudsync/AppStore.png" border="0"  width="100px"></a>
															</td>
														</tr>
													</table>
												</div>
											</td>
									  </tr>

									  <tr height="10px">
											<td colspan="3">
											</td>
									  </tr>

									  <tr bgcolor="#444f53" width="235px">
									    <td bgcolor="#444f53" class="cloud_main_radius_left" width="20%" height="50px">
												<div style="padding:10px;" align="center"><img src="/images/cloudsync/001.png">
													<div align="center" style="margin-top:10px;font-size: 18px;text-shadow: 1px 1px 0px black;"><#Cloud_Disk#></div>
												</div>
											</td>

									    <td width="6px">
												<div align="center"><img src="/images/cloudsync/line.png"></div>
											</td>

									    <td width="1px"></td>

									    <td>
											<div style="padding:10px;width:95%;font-style:italic;font-size:14px;">
												<#aicloud_disk_desc#>												
											</div>
										</td>

									    <td bgcolor="#444f53" class="cloud_main_radius_right" width="100px">
												<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_clouddisk_enable"></div>
												<div id="iphone_switch_container" class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_clouddisk_enable').iphoneSwitch('<% nvram_get("webdav_aidisk"); %>', 
														 function(){
															if(window.scrollTo)
																window.scrollTo(0,0);
															curState = 1;
															$j("#agreement_panel").fadeIn(300);	
															dr_advise();
															htmlbodyforIE = document.getElementsByTagName("html");
															htmlbodyforIE[0].style.overflow = "auto";
														 },
														 function() {
															document.form.webdav_aidisk.value = 0;
															if(document.form.webdav_proxy.value == 0)
																document.form.enable_webdav.value = 0;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();	
															document.form.submit();
														 }
													);
												</script>
												</div>	
											</td>
									  </tr>

									  <tr height="10px">
											<td colspan="3">
											</td>
									  </tr>
										
									  <tr bgcolor="#444f53">
									    <td bgcolor="#444f53" class="cloud_main_radius_left" width="20%" height="50px">
												<div align="center"><img src="/images/cloudsync/002.png">
													<div align="center" style="margin-top:10px;font-size: 18px;text-shadow: 1px 1px 0px black;"><#Smart_Access#></div>
												</div>
											</td>
									    <td>
												<div align="center"><img src="/images/cloudsync/line.png"></div>
											</td>
									    <td>
												&nbsp;
											</td>
									    <td width="">
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;">
													<#smart_access_desc#>
												</div>
											</td>
									    <td bgcolor="#444f53" class="cloud_main_radius_right" width="100">
												<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_smartAccess_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_smartAccess_enable').iphoneSwitch('<% nvram_get("webdav_proxy"); %>', 
														 function() {
															curState = '<% nvram_get("webdav_proxy"); %>';
															document.form.webdav_proxy.value = 1;
															document.form.enable_webdav.value = 1;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();	
															document.form.submit();
														 },
														 function() {
															curState = '<% nvram_get("webdav_proxy"); %>';
															document.form.webdav_proxy.value = 0;
															if(document.form.webdav_aidisk.value == 0)
																document.form.enable_webdav.value = 0;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();	
															document.form.submit();
														 }
													);
												</script>			
												</div>	
											</td>
									  </tr>

									  <tr height="10px">
											<td colspan="3">
											</td>
									  </tr>

									  <tr bgcolor="#444f53">
									    <td bgcolor="#444f53" class="cloud_main_radius_left" width="20%" height="50px">
												<div align="center">
													<img src="/images/cloudsync/003.png">
													<div align="center" style="margin-top:10px;font-size: 18px;text-shadow: 1px 1px 0px black;"><#smart_sync#></div>
												</div>
											</td>
									    <td>
												<div align="center"><img src="/images/cloudsync/line.png"></div>
											</td>
									    <td>
												&nbsp;
											</td>
									    <td width="">
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;">
													<#smart_sync_desc#>
												</div>
											</td>
									    <td bgcolor="#444f53" class="cloud_main_radius_right" width="100">
												<!--div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_smartSync_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_smartSync_enable').iphoneSwitch('<% nvram_get("enable_cloudsync"); %>',
														function() {
															document.form.enable_cloudsync.value = 1;
															FormActions("start_apply.htm", "apply", "restart_cloudsync", "3");
															showLoading();	
															document.form.submit();
														},
														function() {
															document.form.enable_cloudsync.value = 0;
															FormActions("start_apply.htm", "apply", "restart_cloudsync", "3");
															showLoading();	
															document.form.submit();	
														}
													);
												</script>			
												</div-->

						  					<input name="button" type="button" class="button_gen_short" onclick="location.href='/cloud_sync.asp'" value="<#btn_go#>"/>
											</td>
									  </tr>
									  <tr>
									  	<td colspan="5" id="btn_cloud_uninstall">
												<div class="apply_gen" style="margin-top:20px;">
													<input class="button_gen" onclick="apps_form('remove','aicloud','');" type="button" value="Uninstall"/>
			            			</div>			            
			            		</td>
			            	</tr>		
									</table>
									</div>

							  </td>
							</tr>				
							</tbody>	
				  	</table>			
					</td>
				</tr>
			</table>
		</td>
		<td width="20"></td>
	</tr>
</table>
<div id="footer"></div>
</form>

</body>
</html>
