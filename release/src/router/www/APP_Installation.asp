<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png"><title><#Web_Title#> - <#Menu_usb_application#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link rel="stylesheet" type="text/css" href="app_installation.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<style>
#Aidisk_png{
  background: url(images/New_ui/USBExt/APP_list.png);
  background-position: 0px 0px;
  width: 66px;
  height: 66px;
}
#server_png{
  background: url(images/New_ui/USBExt/APP_list.png);
  background-position: 0px -76px;
  width: 66px;
  height: 66px;
}
#PrinterServer_png{
  background: url(images/New_ui/USBExt/APP_list.png);
  background-position: 0px -152px;
  width: 66px;
  height: 66px;
}
#modem_png{
  background: url(images/New_ui/USBExt/APP_list.png);
  background-position: 0px -228px;
  width: 66px;
  height: 66px;
}
#downloadmaster_png{
  background: url(images/New_ui/USBExt/APP_list.png);
  background-position: 0px -304px;
  width: 66px;
  height: 66px;
}
#mediaserver_png{
  background: url(images/New_ui/USBExt/APP_list.png);
  background-position: 0px -380px;
  width: 66px;
  height: 66px;
}
#mediaserver2_png{
  background: url(images/New_ui/USBExt/APP_list.png);
  background-position: 0px -380px;
  width: 66px;
  height: 66px;
}
#aicloud_png{
  background: url(images/New_ui/USBExt/APP_list.png);
  background-position: 0px -456px;
  width: 66px;
  height: 66px;
}
#TimeMachine_png{
  background: url(images/New_ui/USBExt/TimeMachine.png);
  width: 66px;
  height: 66px;
}
</style>
<script>


var apps_array = <% apps_info("asus"); %>;

<% apps_state_info(); %>

var apps_download_percent_done = 0;

<% apps_action(); %> //trigger apps_action.

var stoppullstate = 0;
var isinstall = 0;
var installPercent = 1;
var default_apps_array = new Array();
var appnum = 0;
var _appname = "";
var _dm_install;
var _dm_enable;
var dm_http_port = '<% nvram_get("dm_http_port"); %>';
if(dm_http_port == "")
	dm_http_port = "8081";

var _apps_action = '<% get_parameter("apps_action"); %>';
if(_apps_action == 'cancel')
	_apps_action = '';

var webs_state_update;
var webs_state_error;
var webs_state_info;
var wan_unit_orig = '<% nvram_get("wan_unit"); %>';

function initial(){
	show_menu();

	default_apps_array = [["AiDisk", "aidisk.asp", "<#AiDiskWelcome_desp1#>", "Aidisk_png", ""],
			["<#Servers_Center#>", tablink[4][1], "<#UPnPMediaServer_Help#>", "server_png", ""],
			["<#Network_Printer_Server#>", "PrinterServer.asp", "<#Network_Printer_desc#>", "PrinterServer_png", ""],
			["3G/4G", "Advanced_Modem_Content.asp", "<#HSDPAConfig_hsdpa_enable_hint1#>", "modem_png", ""],
			["<#TimeMach#>", "Advanced_TimeMachine.asp", "<#TimeMach_enable_hint#>", "TimeMachine_png", "1.0.0.1"]];
	
	if(!media_support){
		default_apps_array[1].splice(2,1,"<#MediaServer_Help#>");
	}
	
	if(sw_mode == 2 || sw_mode == 3 || sw_mode == 4 || noaidisk_support)
		default_apps_array = default_apps_array.del(default_apps_array.getIndexByValue2D("AiDisk")[0]);

	if(!printer_support)
		default_apps_array = default_apps_array.del(default_apps_array.getIndexByValue2D("<#Network_Printer_Server#>")[0]);

	if(sw_mode == 2 || sw_mode == 3 || sw_mode == 4 || !modem_support || based_modelid == "4G-AC55U")
		default_apps_array = default_apps_array.del(default_apps_array.getIndexByValue2D("3G/4G")[0]);

	if(!timemachine_support)
		default_apps_array = default_apps_array.del(default_apps_array.getIndexByValue2D("<#TimeMach#>")[0]);

	if(!nodm_support){              
		addOnlineHelp(document.getElementById("faq2"), ["ASUSWRT", "download","associated"]);
	}

	trNum = default_apps_array.length;
	calHeight(0);
	
	if(_apps_action == '' && 
		(apps_state_upgrade == 4 || apps_state_upgrade == "") && 
		(apps_state_enable == 2 || apps_state_enable == "") &&
		(apps_state_update == 2 || apps_state_update == "") && 
		(apps_state_remove == 2 || apps_state_remove == "") &&
		(apps_state_switch == 5 || apps_state_switch == "") && 
		(apps_state_autorun == 4 || apps_state_autorun == "") && 
		(apps_state_install == 5 || apps_state_install == "")){
		show_apps();
	}
	else{
		setTimeout("update_appstate();", 2000);
	}

}

function calHeight(_trNum){
	document.getElementById("applist_table").style.height = "auto";

	if(_trNum != 0)
		_trNum = document.getElementById("applist_table").clientHeight;

	var optionHeight = 52;
	var manualOffSet = 28;
	menu_height = Math.round(optionHeight*calculate_height - manualOffSet*calculate_height/14 - document.getElementById("tabMenu").clientHeight) - 18;
	if(menu_height > _trNum){
		if(menu_height < 580)
			document.getElementById("applist_table").style.height = "580px";
		else	
			document.getElementById("applist_table").style.height = menu_height + "px";
	}	
}

function update_appstate(e){
  $.ajax({
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
				calHeight(0);
			}
			else
      			setTimeout("update_applist();", 3000);
		}    
  });
}

function update_applist(e){
  $.ajax({
    url: '/update_applist.asp',
    dataType: 'script',
	
    error: function(xhr){
		update_applist();
    },
    success: function(response){
			if(isinstall > 0 && cookie.get("apps_last") == "downloadmaster"){
				for(var i = 0; i < apps_array.length; i++){
					if(apps_array[i][0] == "DM2_Utility")
						document.getElementById("DMUtilityLink").href = apps_array[i][5]+ "/" + apps_array[i][12];
						
					if(apps_array[i][0] == "downloadmaster"){			//set cookie for help.js	
						_dm_install = apps_array[i][3];
						_dm_enable = apps_array[i][4];
					}	
				}
				document.getElementById("isInstallDesc").style.display = "";
				setTimeout('divdisplayctrl("none", "none", "none", "");', 100);
				document.getElementById("return_btn").style.display = "";
			}
			else{
				// setTimeout('show_partition();', 100);
				setTimeout('show_apps();', 100);
			}
		}    
  });
}

function check_appstate(){
	if(_apps_action != "" && apps_state_upgrade == "" && apps_state_enable == "" && apps_state_update == "" && 
		 apps_state_remove == "" && apps_state_switch == "" && apps_state_autorun == "" && apps_state_install == ""){
		return false;
	}

	if((apps_state_upgrade == 4 || apps_state_upgrade == "") && (apps_state_enable == 2 || apps_state_enable == "") &&
		(apps_state_update == 2 || apps_state_update == "") && (apps_state_remove == 2 || apps_state_remove == "") &&
		(apps_state_switch == 5 || apps_state_switch == "") && (apps_state_autorun == 4 || apps_state_autorun == "") && 
		(apps_state_install == 5 || apps_state_install == "")){
		if(apps_state_install == 5 || apps_state_upgrade == 4){
			if(installPercent > 1 && installPercent < 95)
				installPercent = 95;
			else
				return true;
		}
		else
			return true;
	}

	var errorcode;
	var proceed = 0.6;

	if(apps_state_upgrade != 4 && apps_state_upgrade != ""){ // upgrade error handler
		errorcode = "apps_state_upgrade = " + apps_state_upgrade;
		if(apps_state_error == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_inputerror#>";
		else if(apps_state_error == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_error == 4)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_install#>";
		else if(apps_state_error == 6)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_remote_responding#>";
		else if(apps_state_error == 7)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_upgrade#>";
		else if(apps_state_error == 9)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_unmount#>";
		else if(apps_state_error == 10)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_dev_responding#>";
		else if(apps_state_upgrade == 0)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_initializing#>";
		else if(apps_state_upgrade == 1){
			if(apps_download_percent > 0 && apps_download_percent <= 100){
				document.getElementById("apps_state_desc").innerHTML = apps_download_file + " is downloading.. " + " <b>" + apps_download_percent + "</b> <span style='font-size: 16px;'>%</span>";
				apps_download_percent_done = 0;
			}
			else if(apps_download_percent_done > 5){
				if(installPercent > 99)
					installPercent = 99;
				document.getElementById("loadingicon").style.display = "none";
				document.getElementById("apps_state_desc").innerHTML = "[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
				installPercent = installPercent + proceed;//*/
			}
			else{
				document.getElementById("apps_state_desc").innerHTML = "&nbsp;<#usb_initializing#>...";
				apps_download_percent_done++;
			}
		}
		else if(apps_state_upgrade == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_uninstalling#>";
		else{
			if(apps_depend_action_target != "terminated" && apps_depend_action_target != "error"){
				if(apps_depend_action_target == "")
					document.getElementById("apps_state_desc").innerHTML = "<b>[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> </b>";
				else
					document.getElementById("apps_state_desc").innerHTML = "<b>[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> </b>"
							+"<br> <span style='font-size: 16px;'> <#Excute_processing#>："+apps_depend_do+"</span>"
							+"<br> <span style='font-size: 16px;'>"+apps_depend_action+"  "+apps_depend_action_target+"</span>"
							;
			}
			else{
				if(installPercent > 99)
					installPercent = 99;
				document.getElementById("loadingicon").style.display = "none";
				document.getElementById("apps_state_desc").innerHTML = "[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
				installPercent = installPercent + proceed;
			}
		}
	}
	else if(apps_state_enable != 2 && apps_state_enable != ""){
		errorcode = "apps_state_enable = " + apps_state_enable;
		if(apps_state_error == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_unknown#>";
		else if(apps_state_error == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_error == 3)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_create_swap#>";
        else if(apps_state_error == 8)
            document.getElementById("apps_state_desc").innerHTML = "Enable error!";
		else{
			document.getElementById("loadingicon").style.display = "";
			document.getElementById("apps_state_desc").innerHTML = "<#QIS_autoMAC_desc2#>";
		}
	}
	else if(apps_state_update != 2 && apps_state_update != ""){
		errorcode = "apps_state_update = " + apps_state_update;
		if(apps_state_error == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_Preparing#>";
		else if(apps_state_error == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_No_Internet#>";
		else
			document.getElementById("apps_state_desc").innerHTML = "<#upgrade_processing#>";
	}
	else if(apps_state_remove != 2 && apps_state_remove != ""){
		errorcode = "apps_state_remove = " + apps_state_remove;
		document.getElementById("apps_state_desc").innerHTML = "<#uninstall_processing#>";
	}
	else if(apps_state_switch != 4 && apps_state_switch != 5 && apps_state_switch != ""){
		errorcode = "apps_state_switch = " + apps_state_switch;
		if(apps_state_error == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_unknown#>";
		else if(apps_state_error == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_switch == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_Stopping#>";
		else if(apps_state_switch == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_Stopwapping#>";
		else if(apps_state_switch == 3)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_Partition_Check#>";
		else
			document.getElementById("apps_state_desc").innerHTML = "<#Excute_processing#>";
	}
	else if(apps_state_autorun != 4 && apps_state_autorun != ""){
		errorcode = "apps_state_autorun = " + apps_state_autorun;
		if(apps_state_error == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_unknown#>";
		else if(apps_state_error == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_autorun == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_disk_checking#>";
		else if(apps_state_install == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_Swap_creating#>";
		else
			document.getElementById("apps_state_desc").innerHTML = "<#Auto_Install_processing#>";
	}
	else if(apps_state_install != 5 && apps_state_error > 0){ // install error handler
		if(apps_state_error == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_inputerror#>";
		else if(apps_state_error == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_error == 3)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_create_swap#>";
		else if(apps_state_error == 4)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_install#>";
		else if(apps_state_error == 5)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_connect_internet#>";
		else if(apps_state_error == 6)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_remote_responding#>";
		else if(apps_state_error == 7)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_upgrade#>";
		else if(apps_state_error == 9)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_unmount#>";
		else if(apps_state_error == 10)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_dev_responding#>";

		isinstall = 0;
	}
	else if(apps_state_install != 5 && apps_state_install != ""){
		isinstall = 1;
		errorcode = "_apps_state_install = " + apps_state_install;

		if(apps_state_install == 0)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_partitioning#>";
		else if(apps_state_install == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_disk_checking#>";
		else if(apps_state_install == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_Swap_creating#>";
		else if(apps_state_install == 3){
			if(apps_download_percent > 0 && apps_download_percent <= 100){
				document.getElementById("apps_state_desc").innerHTML = apps_download_file + " is downloading.. " + " <b>" + apps_download_percent + "</b> <span style='font-size: 16px;'>%</span>";
				apps_download_percent_done = 0;
			}
			else if(apps_download_percent_done > 5){
				if(installPercent > 99)
					installPercent = 99;
				document.getElementById("loadingicon").style.display = "none";
				document.getElementById("apps_state_desc").innerHTML = "[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
				installPercent = installPercent + proceed;//*/
			}
			else{
				document.getElementById("apps_state_desc").innerHTML = "&nbsp;<#usb_initializing#>...";
				apps_download_percent_done++;
			}
		}
		else{ //apps_state_install == 4
			if(apps_depend_action_target != "terminated" && apps_depend_action_target != "error"){
				if(apps_depend_action_target == ""){
					if(installPercent > 99)
						installPercent = 99;
					document.getElementById("loadingicon").style.display = "none";
					document.getElementById("apps_state_desc").innerHTML = "[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
					installPercent = installPercent + proceed;
				}
				else{
					var _apps_depend_do = apps_depend_do.replace(apps_depend_action, "<span style='color:#FC0'>"+apps_depend_action+"</span>");

					document.getElementById("apps_state_desc").innerHTML = "<b>[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> </b>"
							+"<br> <span style='font-size: 16px;'> <#Excute_processing#>："+_apps_depend_do+"</span>"
							+"<br><br> <span style='font-size: 18px;'>"+apps_depend_action+"  "+apps_depend_action_target+"</span>"
					;
				}
			}
			else{
				if(installPercent > 99)
					installPercent = 99;
				document.getElementById("loadingicon").style.display = "none";
				document.getElementById("apps_state_desc").innerHTML = "[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
				installPercent = installPercent + proceed;
			}
		}
	}
	else{
		document.getElementById("loadingicon").style.display = "";
		document.getElementById("apps_state_desc").innerHTML = "<#QIS_autoMAC_desc2#>";
	}
	
	if(apps_state_error != 0){
		document.getElementById("return_btn").style.display = "";
		document.getElementById("loadingicon").style.display = "none";
		stoppullstate = 1;
	}
	else
		document.getElementById("return_btn").style.display = "none";

	document.getElementById("cancelBtn").style.display = "";
	return false;
}

var trNum;
function show_apps(){
	document.getElementById("usbHint").innerHTML = "<#remove_usb_hint#>";

	var counter = 0;
	appnum = 0;
	
	if(apps_array == "" && (appnet_support || appbase_support)){
		apps_array = [["downloadmaster", "", "", "no", "no", "", "", "<#DM_EnableHint#>", "downloadmaster_png", "", "", ""],
									["mediaserver", "", "", "no", "no", "", "", "", "mediaserver_png", "", "", ""]];
		if(nodm_support)
			apps_array[1][0] = "mediaserver2";

		if(aicloudipk_support)
			apps_array.push(["aicloud", "", "", "no", "no", "", "", "AiCloud 2.0 utilities", "aicloud_png", "", "", ""]);
	}

	if(!aicloudipk_support){
		var aicloud_idx = apps_array.getIndexByValue2D("aicloud");
		if(aicloud_idx[1] != -1 && aicloud_idx != -1)
			apps_array.splice(aicloud_idx[0], 1);
	}

	if(nodm_support){
		var dm_idx = apps_array.getIndexByValue2D("downloadmaster");
		if(dm_idx[1] != -1 && dm_idx != -1)
			apps_array.splice(dm_idx[0], 1);
	}

	if(media_support || nomedia_support){ // buildin or hidden 
		// remove mediaserver
		var media_idx = apps_array.getIndexByValue2D("mediaserver");
		if(media_idx[1] != -1 && media_idx != -1)
			apps_array.splice(media_idx[0], 1);

		// remove mediaserver2
		var media2_idx = apps_array.getIndexByValue2D("mediaserver2");
		if(media2_idx[1] != -1 && media2_idx != -1)
			apps_array.splice(media2_idx[0], 1);
	}
	else{
		if(nodm_support)
			var media_idx = apps_array.getIndexByValue2D("mediaserver");
		else
			var media_idx = apps_array.getIndexByValue2D("mediaserver2");

		if(media_idx[1] != -1 && media_idx != -1)
			apps_array.splice(media_idx[0], 1);

		var media_idx = apps_array.getIndexByValue2D("mediaserver");
		if(!nodm_support && (media_idx == -1 || media_idx[1] == -1)){
			var apps_len = apps_array.length;
			apps_array[apps_len] = ["mediaserver", "", "", "no", "no", "", "", "", "mediaserver_png", "", "", ""];
		}
  }

	// calculate div height
	htmlcode = '<table class="appsTable" align="center" style="margin:auto;border-collapse:collapse;">';
	
	//show default Apps
	for(var i = 0; i < default_apps_array.length; i++){
		htmlcode += '<tr><td align="center" class="app_table_radius_left" style="width:85px">';
		//Viz modified to CSS sprites : htmlcode += '<img style="margin-top:0px;" src="/images/New_ui/USBExt/'+ default_apps_array[i][3] +'" style="cursor:pointer" onclick="location.href=\''+ default_apps_array[i][1] +'\';">';
		if(i == 3 && wan_unit_orig != usb_index && usb_index != -1){
			htmlcode += '<div id="'+default_apps_array[i][3]+'" style="cursor:pointer" onclick="go_modem_page('+usb_index+');"></div>';
		}
		else
			htmlcode += '<div id="'+default_apps_array[i][3]+'" style="cursor:pointer" onclick="location.href=\''+ default_apps_array[i][1] +'\';"></div>';
		htmlcode += '</td><td class="app_table_radius_right" style="width:350px;">\n';
		if(i == 3 && wan_unit_orig != usb_index && usb_index != -1){
			console.log("2 need to change wan unit!");
			htmlcode += '<div class="app_name"><a style="text-decoration: underline; cursor:pointer;" onclick="go_modem_page('+usb_index+');">'+ default_apps_array[i][0] + '</a></div>\n';
		}
		else
			htmlcode += '<div class="app_name"><a style="text-decoration: underline;" href="' + default_apps_array[i][1] + '">' + default_apps_array[i][0] + '</a></div>\n';
		if(i ==3){
			htmlcode += '<div class="app_desc">' + default_apps_array[i][2] + ' <a href="http://www.asus.com/event/networks_3G4G_support/" target="_blank" style="text-decoration:underline;">Support</a></div>\n';
		}
		else{
			htmlcode += '<div class="app_desc">' + default_apps_array[i][2] + '</div>\n';
		}
		htmlcode += '<div style="margin-top:10px;"></div><br/><br/></td></tr>\n';
	}

	// show all apps
	for(var i = 0; i < apps_array.length; i++){
		if(apps_array[i][0] == "DM2_Utility")
			document.getElementById("DMUtilityLink").href = apps_array[i][5]+ "/" + apps_array[i][12];

		if(apps_array[i][0] != "downloadmaster" && apps_array[i][0] != "mediaserver" && apps_array[i][0] != "mediaserver2" && apps_array[i][0] != "aicloud") // discard unneeded apps
			continue;
		else if((apps_array[i][0] == "downloadmaster" || apps_array[i][0] == "mediaserver" || apps_array[i][0] == "mediaserver2" || apps_array[i][0] == "aicloud") && apps_array[i][3] == "yes" && apps_array[i][4] == "yes"){
			if(location.host.split(":").length > 1)
				apps_array[i][6] = "http://" + location.host.split(":")[0] + ":" + dm_http_port;
			else
				apps_array[i][6] = "http://" + location.host + ":" + dm_http_port;

			if(apps_array[i][0] == "aicloud") // append URL
				apps_array[i][6] = "/cloud_main.asp";
			else if(apps_array[i][0] == "mediaserver" || apps_array[i][0] == "mediaserver2")
				apps_array[i][6] += "/mediaserverui/mediaserver.asp";
		}
		appnum++; // cal the needed height of applist table 

		if(apps_array[i][4] == "no" && apps_array[i][3] == "yes")
			apps_array[i][6] = "";	
			
		// apps_icon
		htmlcode += '<tr style="height: 100px;"><td class="app_table_radius_left" align="center" style="width:85px">\n';
		if(apps_array[i][4] == "yes" && apps_array[i][3] == "yes"){
			if(apps_array[i][6] != ""){
				htmlcode += '<div id="'+apps_array[i][0]+'_png" style="cursor:pointer" onclick="location.href=\''+ apps_array[i][6] +'\';"></div>';
			}
			else{
				htmlcode += '<div id="'+apps_array[i][0]+'_png"></div>';	
			}
		}	
		else{
			htmlcode += '<div id="'+apps_array[i][0]+'_png"></div>';
		}
		htmlcode += '</td>\n';

		// apps_name
		htmlcode += '<td class="app_table_radius_right" style="width:350px;">\n';
		if(apps_array[i][0] == "downloadmaster")
			apps_array[i][0] = "Download Master";
		else if(apps_array[i][0] == "mediaserver" || apps_array[i][0] == "mediaserver2")
			apps_array[i][0] = "Media Server";
		else if(apps_array[i][0] == "aicloud")
			apps_array[i][0] = "AiCloud 2.0";

		if(apps_array[i][6] != ""){ // with hyper-link
			htmlcode += '<div class="app_name">';

			if(apps_array[i][1] == ""){
				if(apps_array[i][3] == "no") // uninstall
					htmlcode += apps_array[i][0] + '</div>\n';
				else if(apps_array[i][4] == "no" && apps_array[i][3] == "yes") // disable
					htmlcode += '<a href="' + apps_array[i][6] + '" style="color:gray;">' + apps_array[i][0] + '<span class="app_ver" style="color:gray">' + apps_array[i][1] + '</sapn></a></div>\n';
				else{ // enable
					if(apps_array[i][0] == "Download Master")
						htmlcode += '<a target="_blank" href="' + apps_array[i][6] + '" style="text-decoration: underline;">' + apps_array[i][0] + '</a><span class="app_ver">' + apps_array[i][1] + '</sapn></div>\n';		
					else
						htmlcode += '<a href="' + apps_array[i][6] + '" style="text-decoration: underline;">' + apps_array[i][0] + '</a><span class="app_ver">' + apps_array[i][1] + '</sapn></div>\n';		
				}
			}
			else{
				if(apps_array[i][4] == "no" && apps_array[i][3] == "yes") // disable
					htmlcode += '<a href="' + apps_array[i][6] + '" style="color:gray">' + apps_array[i][0] + '<span class="app_ver" style="color:gray">ver. ' + apps_array[i][1] + '</sapn></a></div>\n';
				else{ // enable
					if(apps_array[i][0] == "Download Master")
						htmlcode += '<a target="_blank" href="' + apps_array[i][6] + '" style="text-decoration: underline;">' + apps_array[i][0] + '</a><span class="app_ver">ver. ' + apps_array[i][1] + '</sapn></div>\n';
					else
						htmlcode += '<a href="' + apps_array[i][6] + '" style="text-decoration: underline;">' + apps_array[i][0] + '</a><span class="app_ver">ver. ' + apps_array[i][1] + '</sapn></div>\n';
				}
			}
		}
		else{ // without hyper-link
			if(apps_array[i][4] == "no" && apps_array[i][3] == "yes")
				htmlcode += '<div class="app_name" style="color:gray">';
			else
				htmlcode += '<div class="app_name">';
			if(apps_array[i][1] == "")
				htmlcode += apps_array[i][0] + '<span class="app_ver">' + apps_array[i][1] + '</sapn></div>\n';
			else
				htmlcode += apps_array[i][0] + '<span class="app_ver">ver. ' + apps_array[i][1] + '</sapn></div>\n';
		}

		if(apps_array[i][0] == "Download Master")
			apps_array[i][0] = "downloadmaster";
		else if(apps_array[i][0] == "Media Server"){
			if(!nodm_support)
				apps_array[i][0] = "mediaserver";
			else
				apps_array[i][0] = "mediaserver2";
		}
		else if(apps_array[i][0] == "AiCloud 2.0")
			apps_array[i][0] = "aicloud";

		// apps_desc
		if(apps_array[i][4] == "no" && apps_array[i][3] == "yes"){
			htmlcode += '<div class="app_desc" style="color:gray">' + apps_array[i][7] + '</div>\n';
			htmlcode += '<div style="margin-top:10px;">\n';		
		}
		else{
			htmlcode += '<div class="app_desc">' + apps_array[i][7] + '</div>\n';
			htmlcode += '<div style="margin-top:10px;">\n';		
		}

		// apps_action
		if(apps_array[i][3] == "yes"){ //installed
			htmlcode += '<span class="app_action" onclick="apps_form(\'remove\',\''+ apps_array[i][0] +'\',\'\');">Uninstall</span>\n';
			if(apps_array[i][4] == "yes")		//enable
				htmlcode += '<span class="app_action" onclick="apps_form(\'enable\',\''+ apps_array[i][0] +'\',\'no\');">Disable</span>\n';
			else
				htmlcode += '<span class="app_action" onclick="apps_form(\'enable\',\''+ apps_array[i][0] +'\',\'yes\');">Enable</span>\n';
		}
		else{
			if(apps_array[i][0] == "downloadmaster" || apps_array[i][0] == "mediaserver" || apps_array[i][0] == "aicloud" || apps_array[i][0] == "mediaserver2")
				htmlcode += '<span class="app_action" onclick="_appname=\''+apps_array[i][0]+'\';divdisplayctrl(\'none\', \'\', \'none\', \'none\');location.href=\'#\';">Install</span>\n';
			else
				htmlcode += '<span class="app_action" onclick="apps_form(\'install\',\''+ apps_array[i][0] +'\',\''+ partitions_array[i] +'\');">Install</span>\n';
		}

		if(apps_array[i][0] == "downloadmaster"){
			if(apps_array[i][3] == "yes"){
				htmlcode += '<span class="app_action" onclick="divdisplayctrl(\'none\', \'none\', \'none\', \'\');">Help</span>\n';
			}

			cookie.set("dm_install", apps_array[i][3], 1000);
			cookie.set("dm_enable", apps_array[i][4], 1000);
		}

		if(hasNewVer(apps_array[i]))
			htmlcode += '</div><div style="color:#FC0;margin-top:10px;"><span class="app_action" onclick="apps_form(\'upgrade\',\''+ apps_array[i][0] +'\',\'\');"><#update_available#></span>\n';
		
		htmlcode += '</div><br/><br/></td></tr>\n';
	
		// setCookie
		if(apps_array[i][0] == "downloadmaster"){ // set Cookie
			//set cookie for help.js		
			_dm_install = apps_array[i][3];
			_dm_enable = apps_array[i][4];
		}
	}

	htmlcode += '</table>\n';
	document.getElementById("app_table").innerHTML = htmlcode;
	divdisplayctrl("", "none", "none", "none");
	stoppullstate = 1;
	calHeight(1);
	cookie.set("hwaddr", '<% nvram_get("lan_hwaddr"); %>', 1000);
}

/* 
The first four digits only contain the APP version.
Extention number should be appended to the end of APP version.
*/
var hasNewVer = function(arr){
	if(arr[1])
		oldVer = arr[1].split(".");
	else
		return false;

	if(arr[2])
		newVer = arr[2].split(".");
	else
		return false;
	
	for(var i=0; i<4; i++){ 
		if(parseInt(newVer[i]) > parseInt(oldVer[i])){
			return true;
			break;
		}
		else if(parseInt(newVer[i]) == parseInt(oldVer[i]))
			continue;
		else
			return false;
	}

	if(oldVer.length < newVer.length)
		return false;
	else if(oldVer.length > newVer.length)
		return true;		
	else{
		if(oldVer[oldVer.length-1] != newVer[newVer.length-1])
			return true;
		else
			return false;
	}

	return false;
}

var partitions_array = [];
function show_partition(){
 	require(['/require/modules/diskList.js'], function(diskList){
		var htmlcode = "";
		var mounted_partition = 0;
		partitions_array = [];
		document.getElementById("app_table").style.display = "none";
		htmlcode += '<table align="center" style="margin:auto;border-collapse:collapse;">';

 		var usbDevicesList = diskList.list();

		for(var i=0; i < usbDevicesList.length; i++){
			for(var j=0; j < usbDevicesList[i].partition.length; j++){
				partitions_array.push(usbDevicesList[i].partition[j].mountPoint);
				var accessableSize = simpleNum(usbDevicesList[i].partition[j].size-usbDevicesList[i].partition[j].used);
				var totalSize = simpleNum(usbDevicesList[i].partition[j].size);

				if(usbDevicesList[i].partition[j].status == "unmounted")
					continue;
					
				if(usbDevicesList[i].partition[j].isAppDev){
					if(accessableSize > 1)
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk" onclick="apps_form(\'install\',\'' + _appname +'\',\'' + usbDevicesList[i].partition[j].mountPoint +'\');"></div></td><td class="app_table_radius_right" style="width:300px;">\n';
					else
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk_noquota"></div></td><td class="app_table_radius_right" style="width:300px;">\n';

					htmlcode += '<div class="app_desc"><b>'+ usbDevicesList[i].partition[j].partName + ' (active)</b></div>';
				}
				else{
					if(accessableSize > 1)
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk" onclick="apps_form(\'switch\',\''+_appname+'\',\''+usbDevicesList[i].partition[j].mountPoint+'\');"></div></td><td class="app_table_radius_right" style="width:300px;">\n';
					else
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk_noquota"></div></td><td class="app_table_radius_right" style="width:300px;">\n';
					htmlcode += '<div class="app_desc"><b>'+ usbDevicesList[i].partition[j].partName + '</b></div>'; 
				}

				if(accessableSize > 1)
					htmlcode += '<div class="app_desc"><#Availablespace#>: <b>'+ accessableSize+" GB" + '</b></div>'; 
				else
					htmlcode += '<div class="app_desc"><#Availablespace#>: <b>'+ accessableSize+" GB <span style=\'color:#FFCC00\'>(Disk quota can not less than 1GB)" + '</span></b></div>'; 

				htmlcode += '<div class="app_desc"><#Totalspace#>: <b>'+ totalSize+" GB" + '</b></div>'; 
				htmlcode += '<div class="app_desc"><b>' + usbDevicesList[i].deviceName + '</b></div>'; 
				htmlcode += '</div><br/><br/></td></tr>\n';
				mounted_partition++;	
			}
		}

		if(mounted_partition == 0)
			htmlcode += '<tr height="360px"><td colspan="2" class="nohover"><span class="app_name" style="line-height:100%"><#no_usb_found#></span></td></tr>\n';

		document.getElementById("partition_div").innerHTML = htmlcode;
		document.getElementById("usbHint").innerHTML = "<#DM_Install_partition#> :";
		calHeight(1);
	});
}

function detectUSBStatusApp(){
 	require(['/require/modules/diskList.js'], function(diskList){
		setInterval(function(){
			diskList.update(show_partition); 
		}, 2000);
	});
}

function apps_form(_act, _name, _flag){
	cookie.set("apps_last", _name, 1000);

	document.app_form.apps_action.value = _act;
	document.app_form.apps_name.value = _name;
	document.app_form.apps_flag.value = _flag;
	document.app_form.submit();
}

function divdisplayctrl(flag1, flag2, flag3, flag4){
	document.getElementById("app_table").style.display = flag1;
	document.getElementById("partition_div").style.display = flag2;
	document.getElementById("app_state").style.display = flag3;
	document.getElementById("DMDesc").style.display = flag4;

	if(flag1 != "none"){ // app list
		document.getElementById("return_btn").style.display = "none";
	}
	else if(flag2 != "none"){ // partition list
		detectUSBStatusApp();
		show_partition();
		document.getElementById("return_btn").style.display = "";
		calHeight(1);
	}
	else if(flag4 != "none"){ // help
		if(location.host.split(":").length > 1)
			var _quick_dmlink = "http://" + location.host.split(":")[0] + ":" + dm_http_port;
		else
			var _quick_dmlink = "http://" + location.host + ":" + dm_http_port;
		
		if(_dm_enable == "yes"){
			document.getElementById("realLink").href = _quick_dmlink;
		}
		else{
			document.getElementById("quick_dmlink").onclick = function(){alert("<#DM_DisableHint#>");return false;}
		}	
			
		document.getElementById("return_btn").style.display = "";
		calHeight(1);
	}
	else{ // status
		calHeight(0);
 	}

	if(flag4 == "none")
		document.getElementById("usbHint").style.display = "";
	else
		document.getElementById("usbHint").style.display = "none";
}

function reloadAPP(){
	document.app_form.apps_action.value = "";
	document.app_form.apps_name.value = "";
	document.app_form.apps_flag.value = "";
	location.href = "/APP_Installation.asp";
}

function go_modem_page(usb_unit_flag){
	document.act_form.wan_unit.value = usb_unit_flag;
	document.act_form.action_mode.value = "change_wan_unit";
	document.act_form.target = "";
	document.act_form.submit();
	location.herf = default_apps_array[3][1];
}
</script>
</head>

<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="app_form" action="/APP_Installation.asp">
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
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
</form>

<table class="content" align="center" cellspacing="0" style="margin:auto;">
  <tr>
	<td width="17">&nbsp;</td>
	
	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>
	
    <td valign="top">
		<div id="tabMenu" style="*margin-top: -160px;"></div>
		<br>
<!--=====Beginning of Main Content=====-->
<div class="app_table" id="applist_table">
<table>

  <tr>
  	<td class="formfonttitle">
				<div style="width:730px">
					<table width="730px">
						<tr>
							<td align="left">
								<span class="formfonttitle"><#Menu_usb_application#></span>
							</td>
							<td>
								<img id="return_btn" onclick="reloadAPP();" align="right" style="cursor:pointer;position:absolute;margin-left:-30px;margin-top:-25px;" title="<#Menu_usb_application#>" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
							</td>
						</tr>
					</table>
				</div>
		</td>
  </tr> 
  <tr>
  	<td class="line_export"><img src="images/New_ui/export/line_export.png" /></td>
  </tr>
  <tr>
		<td>
			<div class="formfontdesc" id="usbHint"><#remove_usb_hint#></div>
		</td> 
  </tr>

  <tr>
   	<td valign="top">
		<div id="partition_div"></div>
		<div id="app_state" class="app_state">
			<span style="margin-left:15px;" id="apps_state_desc"><#APP_list_loading#></span>
			<img id="loadingicon" style="margin-left:10px" src="/images/InternetScan.gif">
			<br>
			<br>
			<br>
			<div id="cancelBtn" style="display:none;">
				<input class="button_gen" onclick="apps_form('cancel','','');" type="button" value="<#CTL_Cancel#>"/>
			</div>
		</div>
		<div id="DMDesc" style="display:none;">
			<div style="margin-left:10px;" id="isInstallDesc">
				<h2><#DM_Install_success#></h2>
				<table>	
					<tr>
					<td><div class="top-heading" style="cursor:pointer;margin-top:10px;height:20px;" id="quick_dmlink"><a id="realLink" href="" target="_blank"><b style="text-decoration:underline;color:#FC0;font-size:16px;"><#DM_launch#></b></></a></div></td>
					<td><div style="margin-left:10px;"><img src="images/New_ui/aidisk/steparrow.png"></div></td>
					</tr>		
				</table>
			<br/>
			<div><img src="images/New_ui/export/line_export.png" /></div>
			<table class="" cellspacing="0" cellpadding="0">
				<tbody>
					<tr valign="top">
					<td>
						<ul style="margin-left:10px;">
							<br>
							<li>
								<a id="faq" href="http://www.asus.com/support/FAQ/1009773/" target="_blank" style="text-decoration:underline;font-size:14px;font-weight:bolder;color:#FFF">Download Master FAQ</a>
							</li>
							<li style="margin-top:10px;">
								<a id="faq2" href="" target="_blank" style="text-decoration:underline;font-size:14px;font-weight:bolder;color:#FFF">Download Master Tool FAQ</a>
							</li>
							<li style="margin-top:10px;">
								<a id="DMUtilityLink" href="http://dlcdnet.asus.com/pub/ASUS/wireless/ASUSWRT/DM2_2037.zip" style="text-decoration:underline;font-size:14px;font-weight:bolder;color:#FFF"><#DM_Download_Tool#></a>
							</li>
							<li style="margin-top:10px;">
								<a target="_blank" style="font-weight: bolder; cursor:pointer;text-decoration: underline;" href="http://www.youtube.com/v/Em6Hddyytlw"><#Video_Link1#></a>
							</li>
						</ul>
					</td>
					</tr>
				</tbody>
			</table>													
   	</td> 
  </tr>  
	   
  <tr>
   	<td valign="top" id="app_table_td" height="0px">
				<div id="app_table"></div>
   	</td> 
  </tr>  
</table>
</div>
<!--=====End of Main Content=====-->
		</td>

		<td width="20" align="center" valign="top"></td>
	</tr>
</table>
</div>

<div id="footer"></div>
<form method="post" name="act_form" action="/apply.cgi" target="hidden_frame">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="wan_unit" value="">
<input type="hidden" name="current_page" value="Advanced_Modem_Content.asp">
</form>
</body>
</html>

