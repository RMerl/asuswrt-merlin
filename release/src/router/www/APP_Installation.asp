<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
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
<script type="text/javascript" src="/jquery.js"></script>
<script>
var $j = jQuery.noConflict();
</script>
<script>
<% login_state_hook(); %>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

var apps_array = <% apps_info("asus"); %>;
var apps_state_upgrade = "<% nvram_get("apps_state_upgrade"); %>";
var apps_state_update = "<% nvram_get("apps_state_update"); %>";
var apps_state_remove = "<% nvram_get("apps_state_remove"); %>";
var apps_state_enable = "<% nvram_get("apps_state_enable"); %>";
var apps_state_switch = "<% nvram_get("apps_state_switch"); %>";
var apps_state_autorun = "<% nvram_get("apps_state_autorun"); %>";
var apps_state_install = "<% nvram_get("apps_state_install"); %>";
var apps_state_error = "<% nvram_get("apps_state_error"); %>";
var apps_dev = "<% nvram_get("apps_dev"); %>";

<% apps_action(); %> //trigger apps_action.

var curr_pool_name = "";
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
var usb_path1_index;
var usb_path2_index;

<% disk_pool_mapping_info(); %>

function initial(){
	show_menu();

	default_apps_array = [["AiDisk", "aidisk.asp", "<#AiDiskWelcome_desp1#>", "Aidisk.png"],
												["Servers Center", tablink[3][1], "<#UPnPMediaServer_Help#>", "server.png"],
												["<#Network_Printer_Server#>", "PrinterServer.asp", "<#Network_Printer_desc#>", "PrinterServer.png"],
												["3G/4G", "Advanced_Modem_Content.asp", "<#HSDPAConfig_hsdpa_enable_hint1#>", "modem.png"]];
	
	if(media_support == -1){
			default_apps_array[1].splice(2,1,"<#MediaServer_Help#>");						
	}											

	if(dualWAN_support != -1){
		default_apps_array[3][2] += "<br><br>Make sure Dual WAN support is enabled first.";
	}

	if(sw_mode == 2 || sw_mode == 3){
		default_apps_array.splice(3, 1);
		default_apps_array.splice(0, 1);
	}
	if(modem_support == -1)
		default_apps_array.splice(3, 1);

	trNum = default_apps_array.length;
	calHeight(0);

	if(_apps_action == '' && 
		(apps_state_upgrade == 3 || apps_state_upgrade == "") && 
		(apps_state_enable == 2 || apps_state_enable == "") &&
		(apps_state_update == 2 || apps_state_update == "") && 
		(apps_state_remove == 2 || apps_state_remove == "") &&
		(apps_state_switch == 5 || apps_state_switch == "") && 
		(apps_state_autorun == 4 || apps_state_autorun == "") && 
		(apps_state_install == 4 || apps_state_install == "")){
		show_apps();
	}
	else{
		setTimeout("update_appstate();", 2000);
	}

	if(nodm_support == -1){
		addOnlineHelp($("faq"), ["ASUSWRT", "download","master"]);
		addOnlineHelp($("faq2"), ["ASUSWRT", "download","tool"]);
	}
}

function calHeight(_trNum){
	$("applist_table").style.height = "auto";

	if(_trNum != 0)
		_trNum = document.getElementById("applist_table").clientHeight;

	var optionHeight = 52;
	var manualOffSet = 28;
	menu_height = Math.round(optionHeight*calculate_height - manualOffSet*calculate_height/14 - $("tabMenu").clientHeight) - 18;

	if(menu_height > _trNum)
		$("applist_table").style.height = menu_height + "px";
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
				calHeight(0);
			}
			else
				update_applist();
		}    
  });
}

function update_applist(e){
  $j.ajax({
    url: '/update_applist.asp',
    dataType: 'script',
	
    error: function(xhr){
      update_applist();
    },
    success: function(response){
			if(isinstall > 0 && getCookie_help("apps_last") == "downloadmaster"){
				for(var i = 0; i < apps_array.length; i++){
					if(apps_array[i][0] == "DM2_Utility")
						$("DMUtilityLink").href = apps_array[i][5]+ "/" + apps_array[i][12];
				}
				$("isInstallDesc").style.display = "";
				setTimeout('divdisplayctrl("none", "none", "none", "");', 100);
				$("return_btn").style.display = "";
			}
			else{
				setTimeout('show_partition();', 100);
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

	if((apps_state_upgrade == 3 || apps_state_upgrade == "") && (apps_state_enable == 2 || apps_state_enable == "") &&
		(apps_state_update == 2 || apps_state_update == "") && (apps_state_remove == 2 || apps_state_remove == "") &&
		(apps_state_switch == 5 || apps_state_switch == "") && (apps_state_autorun == 4 || apps_state_autorun == "") && 
		(apps_state_install == 4 || apps_state_install == "")){
		return true;
	}

	var errorcode;

	if(apps_state_upgrade != 3 && apps_state_upgrade != ""){ // upgrade error handler
		errorcode = "apps_state_upgrade = " + apps_state_upgrade;
		if(apps_state_error == 1)
			$("apps_state_desc").innerHTML = "Input error!";
		else if(apps_state_error == 2)
			$("apps_state_desc").innerHTML = "Mount error!";
		else if(apps_state_error == 4)
			$("apps_state_desc").innerHTML = "Initiate upgrading fail!";
		else if(apps_state_error == 6)
			$("apps_state_desc").innerHTML = "No response from the remote server.";
		else if(apps_state_error == 7)
			$("apps_state_desc").innerHTML = "Application upgrade fail. It may result from incorrect file or error transmission.";
		else if(apps_state_error == 9)
			$("apps_state_desc").innerHTML = "Can't remove!";
		else if(apps_state_error == 10)
			$("apps_state_desc").innerHTML = "USB storage device abnormal!";
		else if(apps_state_upgrade == 0)
			$("apps_state_desc").innerHTML = "Initializing, it may take several minutes to download package...";
		else if(apps_state_upgrade == 1)
			$("apps_state_desc").innerHTML = "Removing...";
		else if(apps_state_upgrade == 2){
			if(installPercent > 99)
				installPercent = 99;
			$("loadingicon").style.display = "none";
			$("apps_state_desc").innerHTML = "Upgrading... <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
			installPercent = installPercent + 3.5;		
		}
		else
			$("apps_state_desc").innerHTML = "<#QIS_autoMAC_desc2#>...";
	}
	else if(apps_state_enable != 2 && apps_state_enable != ""){
		errorcode = "apps_state_enable = " + apps_state_enable;
		if(apps_state_error == 1)
			$("apps_state_desc").innerHTML = "An error occurred!";
		else if(apps_state_error == 2)
			$("apps_state_desc").innerHTML = "Mount error!";
		else if(apps_state_error == 3)
			$("apps_state_desc").innerHTML = "Create Swap error!";
        else if(apps_state_error == 8)
            $("apps_state_desc").innerHTML = "Enable error!";
		else{
			$("loadingicon").style.display = "";
			$("apps_state_desc").innerHTML = "<#QIS_autoMAC_desc2#>...";
		}
	}
	else if(apps_state_update != 2 && apps_state_update != ""){
		errorcode = "apps_state_update = " + apps_state_update;
		if(apps_state_error == 1)
			$("apps_state_desc").innerHTML = "BASEAPPS...";
		else if(apps_state_error == 2)
			$("apps_state_desc").innerHTML = "No interent!";
		else
			$("apps_state_desc").innerHTML = "Updating...";
	}
	else if(apps_state_remove != 2 && apps_state_remove != ""){
		errorcode = "apps_state_remove = " + apps_state_remove;
		$("apps_state_desc").innerHTML = "Uninstall...";
	}
	else if(apps_state_switch != 4 && apps_state_switch != 5 && apps_state_switch != ""){
		errorcode = "apps_state_switch = " + apps_state_switch;
		if(apps_state_error == 1)
			$("apps_state_desc").innerHTML = "An error occurred!";
		else if(apps_state_error == 2)
			$("apps_state_desc").innerHTML = "Mount error!";
		else if(apps_state_switch == 1)
			$("apps_state_desc").innerHTML = "Stop Apps...";
		else if(apps_state_switch == 2)
			$("apps_state_desc").innerHTML = "Stop Swap...";
		else if(apps_state_switch == 3)
			$("apps_state_desc").innerHTML = "Checking Choosed Part...";
		else
			$("apps_state_desc").innerHTML = "Executing...";
	}
	else if(apps_state_autorun != 4 && apps_state_autorun != ""){
		errorcode = "apps_state_autorun = " + apps_state_autorun;
		if(apps_state_error == 1)
			$("apps_state_desc").innerHTML = "An error occurred!";
		else if(apps_state_error == 2)
			$("apps_state_desc").innerHTML = "Mount error!";
		else if(apps_state_autorun == 1)
			$("apps_state_desc").innerHTML = "Checking Disk...";
		else if(apps_state_install == 2)
			$("apps_state_desc").innerHTML = "Creating Swap...";
		else
			$("apps_state_desc").innerHTML = "Auto Installing...";
	}
	else if(apps_state_install != 4 && apps_state_error > 0){ // install error handler
		if(apps_state_error == 1)
			$("apps_state_desc").innerHTML = "Input error!";
		else if(apps_state_error == 2)
			$("apps_state_desc").innerHTML = "Mount error!";
		else if(apps_state_error == 3)
			$("apps_state_desc").innerHTML = "Create Swap error!";
		else if(apps_state_error == 4)
			$("apps_state_desc").innerHTML = "Initiate upgrading fail!";
		else if(apps_state_error == 5)
			$("apps_state_desc").innerHTML = "Couldn't connect to Internet!";
		else if(apps_state_error == 6)
			$("apps_state_desc").innerHTML = "No response from the remote server.";
		else if(apps_state_error == 7)
			$("apps_state_desc").innerHTML = "Application upgrade fail. It may result from incorrect file or error transmission.";
		else if(apps_state_error == 9)
			$("apps_state_desc").innerHTML = "Can't remove!";
		else if(apps_state_error == 10)
			$("apps_state_desc").innerHTML = "USB storage device abnormal!";

		isinstall = 0;
	}
	else if(apps_state_install != 4 && apps_state_install != ""){
		isinstall = 1;
		errorcode = "_apps_state_install = " + apps_state_install;

		if(apps_state_install == 0)
			$("apps_state_desc").innerHTML = "Preparing Partition...";
		else if(apps_state_install == 1)
			$("apps_state_desc").innerHTML = "Checking Disk...";
		else if(apps_state_install == 2)
			$("apps_state_desc").innerHTML = "Creating Swap...";
		else{
			if(installPercent > 99)
				installPercent = 99;
			$("loadingicon").style.display = "none";
			$("apps_state_desc").innerHTML = "Installing... <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
			installPercent = installPercent + 1.5;
		}
	}
	else{
		$("loadingicon").style.display = "";
		$("apps_state_desc").innerHTML = "<#QIS_autoMAC_desc2#>...";
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

var trNum;
function show_apps(){
	$("usbHint").innerHTML = "<#remove_usb_hint#>";

	var counter = 0;
	appnum = 0;

	if(dm_http_port == "")
		dm_http_port = "8081";

	if(apps_array == "" && (appnet_support != -1 || appbase_support != -1)){
		apps_array = [["downloadmaster", "", "", "no", "no", "", "", "Download tools", "downloadmaster.png", "", "", ""],
									["mediaserver", "", "", "no", "no", "", "", "", "mediaserver.png", "", "", ""]];
		if(nodm_support != -1)
			apps_array[1][0] = "mediaserver2";
	}

	if(nodm_support != -1){
		var dm_idx = apps_array.getIndexByValue2D("downloadmaster");
		if(dm_idx[1] != -1 && dm_idx != -1)
			apps_array.splice(dm_idx[0], 1);
	}

	if(media_support == -1){
		if(nodm_support != -1)
			var media_idx = apps_array.getIndexByValue2D("mediaserver");
		else
			var media_idx = apps_array.getIndexByValue2D("mediaserver2");

		if(media_idx[1] != -1 && media_idx != -1)
			apps_array.splice(media_idx[0], 1);
	} 
	else{
		// remove mediaserver
		var media_idx = apps_array.getIndexByValue2D("mediaserver");
		if(media_idx[1] != -1 && media_idx != -1)
			apps_array.splice(media_idx[0], 1);

		// remove mediaserver2
		var media2_idx = apps_array.getIndexByValue2D("mediaserver2");
		if(media2_idx[1] != -1 && media2_idx != -1)
			apps_array.splice(media2_idx[0], 1);
  }

	// calculate div height
	htmlcode = '<table class="appsTable" align="center" style="margin:auto;border-collapse:collapse;">';
	
	//show default Apps
	for(var i = 0; i < default_apps_array.length; i++){
		htmlcode += '<tr><td align="center" class="app_table_radius_left" style="width:85px"><img style="margin-top:0px;" src="/images/New_ui/USBExt/'+ default_apps_array[i][3] +'" style="cursor:pointer" onclick="location.href=\''+ default_apps_array[i][1] +'\';"></td><td class="app_table_radius_right" style="width:350px;">\n';
		htmlcode += '<div class="app_name"><a style="text-decoration: underline;" href="' + default_apps_array[i][1] + '">' + default_apps_array[i][0] + '</a></div>\n';
		htmlcode += '<div class="app_desc">' + default_apps_array[i][2] + '</div>\n';
		htmlcode += '<div style="margin-top:10px;"></div><br/><br/></td></tr>\n';
	}

	// show all apps
	for(var i = 0; i < apps_array.length; i++){
		if(apps_array[i][0] == "DM2_Utility")
			$("DMUtilityLink").href = apps_array[i][5]+ "/" + apps_array[i][12];

		if(apps_array[i][0] != "downloadmaster" && apps_array[i][0] != "mediaserver" && apps_array[i][0] != "mediaserver2") // discard unneeded apps
			continue;
		else if((apps_array[i][0] == "downloadmaster" || apps_array[i][0] == "mediaserver" || apps_array[i][0] == "mediaserver2" || apps_array[i][0] == "cloudsync") && apps_array[i][3] == "yes" && apps_array[i][4] == "yes"){
			if(location.host.split(":").length > 1)
				apps_array[i][6] = "http://" + location.host.split(":")[0] + ":" + dm_http_port;
			else
				apps_array[i][6] = "http://" + location.host + ":" + dm_http_port;

			if(apps_array[i][0] == "cloudsync") // append URL
				apps_array[i][6] += "/cloudui/Setting.asp";
			else if(apps_array[i][0] == "mediaserver" || apps_array[i][0] == "mediaserver2")
				apps_array[i][6] += "/mediaserverui/mediaserver.asp";
		}
		appnum++; // cal the needed height of applist table 

		if(apps_array[i][4] == "no" && apps_array[i][3] == "yes")
			apps_array[i][6] = "";	
			
		// apps_icon
		htmlcode += '<tr><td class="app_table_radius_left" align="center" style="width:85px">\n';
		if(apps_array[i][4] == "yes" && apps_array[i][3] == "yes"){
			if(apps_array[i][6] != ""){
				if(apps_array[i][0] == "mediaserver" || apps_array[i][0] == "mediaserver2")
					htmlcode += '<img style="margin-top:0px;" src="/images/New_ui/USBExt/mediaserver.png" style="cursor:pointer" onclick="location.href=\''+ apps_array[i][6] +'\';"></td>\n';
				else
					htmlcode += '<img style="margin-top:0px;" src="/images/New_ui/USBExt/'+ apps_array[i][0] +'.png" style="cursor:pointer" onclick="location.href=\''+ apps_array[i][6] +'\';"></td>\n';
			}
			else
				htmlcode += '<img style="margin-top:0px;" src="/images/New_ui/USBExt/'+ apps_array[i][0] +'.png"></td>\n';				
		}	
		else{
			if(apps_array[i][0] == "mediaserver" || apps_array[i][0] == "mediaserver2")
				htmlcode += '<img style="margin-top:0px;filter:gray" src="/images/New_ui/USBExt/mediaserver.png"></td>\n';			
			else
				htmlcode += '<img style="margin-top:0px;filter:gray" src="/images/New_ui/USBExt/'+ apps_array[i][0] +'.png"></td>\n';			
		}

		// apps_name
		htmlcode += '<td class="app_table_radius_right" style="width:350px;">\n';
		if(apps_array[i][0] == "downloadmaster")
			apps_array[i][0] = "Download Master";
		else if(apps_array[i][0] == "mediaserver" || apps_array[i][0] == "mediaserver2")
			apps_array[i][0] = "Media Server";
		else if(apps_array[i][0] == "cloudsync")
			apps_array[i][0] = "Cloud Sync";

		if(apps_array[i][6] != ""){ // with hyper-link
			htmlcode += '<div class="app_name">';

			if(apps_array[i][1] == ""){
				if(apps_array[i][3] == "no")
					htmlcode += apps_array[i][0] + '</div>\n';
				else if(apps_array[i][4] == "no" && apps_array[i][3] == "yes") // disable
					htmlcode += '<a href="' + apps_array[i][6] + '" style="color:gray;">' + apps_array[i][0] + '<span class="app_ver" style="color:gray">' + apps_array[i][1] + '</sapn></a></div>\n';
				else // enable
					htmlcode += '<a href="' + apps_array[i][6] + '" style="text-decoration: underline;">' + apps_array[i][0] + '</a><span class="app_ver">' + apps_array[i][1] + '</sapn></div>\n';		
			}
			else{
				if(apps_array[i][4] == "no" && apps_array[i][3] == "yes") // disable
					htmlcode += '<a href="' + apps_array[i][6] + '" style="color:gray">' + apps_array[i][0] + '<span class="app_ver" style="color:gray">ver. ' + apps_array[i][1] + '</sapn></a></div>\n';
				else // enable
					htmlcode += '<a href="' + apps_array[i][6] + '" style="text-decoration: underline;">' + apps_array[i][0] + '</a><span class="app_ver">ver. ' + apps_array[i][1] + '</sapn></div>\n';
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
			if(nodm_support == -1)
				apps_array[i][0] = "mediaserver";
			else
				apps_array[i][0] = "mediaserver2";
		}
		else if(apps_array[i][0] == "Cloud Sync")
			apps_array[i][0] = "cloudsync";

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

			if(parseInt(apps_array[i][2].replace(/[.]/gi,"")) > parseInt(apps_array[i][1].replace(/[.]/gi,"")))
				htmlcode += '<span class="app_action" onclick="apps_form(\'upgrade\',\''+ apps_array[i][0] +'\',\'\');">Upgrade</span>\n';
		}
		else{
			if(apps_array[i][0] == "downloadmaster" || apps_array[i][0] == "mediaserver" || apps_array[i][0] == "cloudsync" || apps_array[i][0] == "mediaserver2")
				htmlcode += '<span class="app_action" onclick="_appname=\''+apps_array[i][0]+'\';divdisplayctrl(\'none\', \'\', \'none\', \'none\');location.href=\'#\';">Install</span>\n';
			else
				htmlcode += '<span class="app_action" onclick="apps_form(\'install\',\''+ apps_array[i][0] +'\',\''+ partitions_array[i] +'\');">Install</span>\n';
		}
		if(apps_array[i][0] == "downloadmaster" && apps_array[i][3] == "yes")
			htmlcode += '<span class="app_action" onclick="divdisplayctrl(\'none\', \'none\', \'none\', \'\');">Help</span>\n';
		
		htmlcode += '</div><br/><br/></td></tr>\n';
	
		// setCookie
		if(apps_array[i][0] == "downloadmaster"){ // set Cookie
			//set cookie for help.js		
			_dm_install = apps_array[i][3];
			_dm_enable = apps_array[i][4];
		}
	}

	htmlcode += '</table>\n';
	$("app_table").innerHTML = htmlcode;
	divdisplayctrl("", "none", "none", "none");
	stoppullstate = 1;
	calHeight(1);
}

var partitions_array = "";
function show_partition(){
	var htmlcode = "";
	var mounted_partition = 0;

	if(pool_names() != "") //  avoid no_disk error
		partitions_array = pool_devices(); 
	
	$("app_table").style.display = "none";
	htmlcode += '<table align="center" style="margin:auto;border-collapse:collapse;">';

	for(var i = 0; i < partitions_array.length; i++){
		var all_accessable_size = simpleNum(pool_kilobytes()[i]-pool_kilobytes_in_use()[i]);
		var all_total_size = simpleNum(pool_kilobytes()[i]);

		if(pool_status()[i] == "unmounted")
			continue;

		if(apps_dev == partitions_array[i]){
			curr_pool_name = pool_names()[i];
			if(all_accessable_size > 1)
				htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk" onclick="apps_form(\'install\',\''+ _appname +'\',\''+ partitions_array[i] +'\');"></div></td><td class="app_table_radius_right" style="width:200px;">\n';
			else
				htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk_noquota"></div></td><td class="app_table_radius_right" style="width:200px;">\n';
			htmlcode += '<div class="app_desc"><b>'+ pool_names()[i] + ' (active)</b></div>';
		}
		else{
			if(all_accessable_size > 1)
				htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk" onclick="apps_form(\'switch\',\''+_appname+'\',\''+partitions_array[i]+'\');"></div></td><td class="app_table_radius_right" style="width:200px;">\n';
			else
				htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk_noquota"></div></td><td class="app_table_radius_right" style="width:200px;">\n';
			htmlcode += '<div class="app_desc"><b>'+ pool_names()[i] + '</b></div>'; 
		}

		if(all_accessable_size > 1)
			htmlcode += '<div class="app_desc"><#Availablespace#>: <b>'+ all_accessable_size+" GB" + '</b></div>'; 
		else
			htmlcode += '<div class="app_desc"><#Availablespace#>: <b>'+ all_accessable_size+" GB <span style=\'color:#FFCC00\'>(Disk quota can not less than 1GB)" + '</span></b></div>'; 

		htmlcode += '<div class="app_desc"><#Totalspace#>: <b>'+ all_total_size+" GB" + '</b></div>'; 
		htmlcode += '</div><br/><br/></td></tr>\n';
		mounted_partition++;
	}

	if(mounted_partition == 0)
		htmlcode += '<tr height="360px"><td colspan="2"><span class="app_name" style="line-height:100%"><#no_usb_found#></span></td></tr>\n';

	$("partition_div").innerHTML = htmlcode;
	$("usbHint").innerHTML = "<#DM_Install_partition#>";
	calHeight(1);
}

var mounted_partition_old = 0;
function detectUSBStatusApp(){
	var mounted_partition = 0;
	$j.ajax({
    		url: '/update_diskinfo.asp',
    		dataType: 'script',
    		error: function(xhr){
    			detectUSBStatusApp();
    		},
    		success: function(){
					for(i=0; i<foreign_disk_interface_names().length; i++){
						if(foreign_disk_total_mounted_number()[i] != 0){
							mounted_partition += foreign_disk_total_mounted_number()[i];
						}
					}
					if(mounted_partition != mounted_partition_old){
						show_partition();
					}
					mounted_partition_old = mounted_partition;
					setTimeout("detectUSBStatusApp();", 1000);
  			}
  });
}

function apps_form(_act, _name, _flag){
	document.app_form.apps_action.value = _act;
	document.app_form.apps_name.value = _name;
	document.app_form.apps_flag.value = _flag;
	document.app_form.submit();
}

function divdisplayctrl(flag1, flag2, flag3, flag4){
	$("app_table").style.display = flag1;
	$("partition_div").style.display = flag2;
	$("app_state").style.display = flag3;
	$("DMDesc").style.display = flag4;

	if(flag1 != "none"){ // app list
		$("return_btn").style.display = "none";
	}
	else if(flag2 != "none"){ // partition list
		detectUSBStatusApp();
		show_partition()
		$("return_btn").style.display = "";
		calHeight(1);
	}
	else if(flag4 != "none"){ // help
		if(location.host.split(":").length > 1)
			var _quick_dmlink = "http://" + location.host.split(":")[0] + ":" + dm_http_port;
		else
			var _quick_dmlink = "http://" + location.host + ":" + dm_http_port;

		$("quick_dmlink").onclick = function(){location.href=_quick_dmlink;}
		$("return_btn").style.display = "";
		calHeight(1);
	}
	else{ // status
		calHeight(0);
 	}

	if(flag4 == "none")
		$("usbHint").style.display = "";
	else
		$("usbHint").style.display = "none";
}

window.onbeforeunload = function(){
	cookie_help.set("apps_last", _appname, 1000);
	cookie_help.set("dm_install", _dm_install, 1000);
	cookie_help.set("dm_enable", _dm_enable, 1000);
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
<input type="hidden" name="next_host" value="">
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
  	<td class="formfonttitle"><!--#Menu_usb_application#>
			<img id="return_btn" onclick="show_apps();" align="right" style="cursor:pointer;margin-right:10px;margin-top:-10px" title="Back to USB Extension" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'"-->

				<div style="width:730px">
					<table width="730px">
						<tr>
							<td align="left">
								<span class="formfonttitle"><#Menu_usb_application#></span>
							</td>
							<td>
								<img id="return_btn" onclick="show_apps();" align="right" style="cursor:pointer;position:absolute;margin-left:-30px;margin-top:-25px;" title="Back to USB Extension" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
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
		<div id="app_state" class="app_state"><span id="apps_state_desc">Loading APP list...</span><img id="loadingicon" style="margin-left:5px;" src="/images/InternetScan.gif"></div>
		<div id="DMDesc" style="display:none;">
			<div style="margin-left:10px;" id="isInstallDesc">
				<h2>Download Master install success!</h2>
				<div class="top-heading" style="cursor:pointer;" id="quick_dmlink">
					<table><tr><td><b style="text-decoration:underline;color:#FC0;font-size:16px;">Use Download Master right now</b></td><td><img style="margin-left:10px;" src="images/New_ui/aidisk/steparrow.png" /></td></tr></table>
				</div>
			</div>
			<br/>
			<div><img src="images/New_ui/export/line_export.png" /></div>
			<table class="" cellspacing="0" cellpadding="0">
				<tbody>
					<tr valign="top">
					<td>
						<ul style="margin-left:10px;">
							<br>
							<li>
								<a id="faq" href="" target="_blank" style="text-decoration:underline;font-size:14px;font-weight:bolder;color:#FFF">Download Master FAQ</a>
							</li>
							<li style="margin-top:10px;">
								<a id="faq2" href="" target="_blank" style="text-decoration:underline;font-size:14px;font-weight:bolder;color:#FFF">Download Master Tool FAQ</a>
							</li>
							<li style="margin-top:10px;">
								<a id="DMUtilityLink" href="http://dlcdnet.asus.com/pub/ASUS/wireless/ASUSWRT/DM2_2017.zip" style="text-decoration:underline;font-size:14px;font-weight:bolder;color:#FFF">Download "Download Master Tool" Now!</a>
							</li>
							<li style="margin-top:10px;">
								<a target="_blank" style="font-weight: bolder; cursor:pointer;text-decoration: underline;" href="http://www.youtube.com/v/Em6Hddyytlw">Click to open tutorial video.</a>
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
</body>
</html>

