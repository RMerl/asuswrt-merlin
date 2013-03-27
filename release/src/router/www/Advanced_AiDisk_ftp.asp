<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#menu5_4_2#></title>
<link rel="stylesheet" type="text/css" href="/index_style.css">
<link rel="stylesheet" type="text/css" href="/form_style.css">
<link rel="stylesheet" type="text/css" href="/aidisk/AiDisk_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/disk_functions.js"></script>
<script type="text/javascript" src="/aidisk/AiDisk_folder_tree.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript">
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

<% disk_pool_mapping_info(); %>
<% available_disk_names_and_sizes(); %>

<% get_AiDisk_status(); %>
<% initial_folder_var_file(); %>
<% get_permissions_of_account(); %>

var PROTOCOL = "ftp";

var NN_status = get_cifs_status();  // Network-Neighborhood
var FTP_status = get_ftp_status(); // FTP
var AM_to_cifs = get_share_management_status("cifs");  // Account Management for Network-Neighborhood
var AM_to_ftp = get_share_management_status("ftp");  // Account Management for FTP

var accounts = [<% get_all_accounts(); %>];

var lastClickedAccount = 0;
var selectedAccount = null;

// changedPermissions[accountName][poolName][folderName] = permission
var changedPermissions = new Array();

var folderlist = new Array();

var ddns_enable = '<% nvram_get("ddns_enable_x"); %>';

function initial(){
	show_menu();
	$("option5").innerHTML = '<table><tbody><tr><td><div id="index_img5"></div></td><td><div style="width:120px;"><#Menu_usb_application#></div></td></tr></tbody></table>';
	$("option5").className = "m5_r";
	
	document.aidiskForm.protocol.value = PROTOCOL;
	
	// show page's control
	showShareStatusControl(PROTOCOL);
	showAccountControl(PROTOCOL);
	
	// show accounts
	showAccountMenu();
	
	// show the kinds of permission
	showPermissionTitle();
	
	// show folder's tree
	setTimeout('get_disk_tree();', 1000);
	
	// the click event of the buttons
	onEvent();
	chech_usb();
	
	$("sharebtn").disabled = true;
	$("accountbtn").disabled = true;
	$("refreshbtn").disabled = true;
	setTimeout("enable_display();", 2000);
}

function enable_display(){
	$("sharebtn").disabled = false;
	$("accountbtn").disabled = false;
	$("refreshbtn").disabled = false;
}

function chech_usb()
{
	var usb_path1 = '<% nvram_get("usb_path1"); %>';
	var usb_path2 = '<% nvram_get("usb_path2"); %>';
	
	if (usb_path1 != "storage" && usb_path2 != "storage"){
		$("accountbtn").disabled = true;
		$("sharebtn").disabled = true;	
	}
}

function get_disk_tree(){
	if(this.isLoading == 0){
		get_layer_items("0", "gettree");
		setTimeout('get_disk_tree();', 1000);
	}
	else
		;
}

function get_accounts(){
	return this.accounts;
}

function switchAppStatus(protocol){  // turn on/off the share
	var status;
	var confirm_str_on, confirm_str_off;
	
	if(protocol == "cifs"){
		status = this.NN_status;
		
		confirm_str_off= "<#confirm_disablecifs#>";  //"<#confirm_disableftp_dm#>"+ By Viz 2011.09
		confirm_str_on = "<#confirm_enablecifs#>";
	}
	else if(protocol == "ftp"){
		status = this.FTP_status;
		
		confirm_str_off = "<#confirm_disableftp#>";
		confirm_str_on = "<#confirm_enableftp#>";
	}
	
	switch(status){
		case 1:
			if(confirm(confirm_str_off)){
				showLoading();
				
				document.aidiskForm.action = "/aidisk/switch_AiDisk_app.asp";
				document.aidiskForm.protocol.value = protocol;
				document.aidiskForm.flag.value = "off";
				
				document.aidiskForm.submit();
			}
			break;
		case 0:
			if(confirm(confirm_str_on)){
				showLoading();
				
				document.aidiskForm.action = "/aidisk/switch_AiDisk_app.asp";
				document.aidiskForm.protocol.value = protocol;
				document.aidiskForm.flag.value = "on";
				
				document.aidiskForm.submit();
			}
			break;
	}
}

function resultOfSwitchAppStatus(){
	refreshpage(1);
}

function showShareStatusControl(protocol){
	var status;
	var str_on, str_off;
	
	if(protocol == "cifs"){
		status = this.NN_status;
		
		str_on = "<#enableCIFS#>";
		str_off = "<#disableCIFS#>";
	}
	else if(protocol == "ftp"){
		status = this.FTP_status;
		
		str_on = "<#enableFTP#>";
		str_off = "<#disableFTP#>";
	}
	else
		return;
	
	switch(status){
		case 1:
			//$("sharebtn").value = str_off;
			$("sharebtn").innerHTML = str_off;
			$("tableMask").style.width = "0px";
			$("accountbtn").disabled = false;
			
			showDDNS();
			break;
		case 0:
			//$("sharebtn").value = str_on;
			$("sharebtn").innerHTML = str_on;
			$("tableMask").style.width = "600px";
			$("accountbtn").disabled = true;
			
			showDDNS();
			break;
	}
}

function showDDNS(){
}

function switchAccount(protocol){
	var status;
	var confirm_str_on, confirm_str_off;
	
	if(protocol != "cifs" && protocol != "ftp" && protocol != "webdav")
		return;
	
	status = get_manage_type(protocol);
	
	confirm_str_on = "<#confirm_enableAccount#>";
	confirm_str_off = "<#confirm_disableAccount#>";
	
	switch(status){
		case 1:
			if(confirm(confirm_str_off)){
				document.aidiskForm.action = "/aidisk/switch_share_mode.asp";
				$("protocol").value = protocol;
				$("mode").value = "share";
				
				showLoading();
				document.aidiskForm.submit();
			}
			break;
		case 0:
			if(confirm(confirm_str_on)){
				document.aidiskForm.action = "/aidisk/switch_share_mode.asp";
				$("protocol").value = protocol;
				$("mode").value = "account";
				
				showLoading();
				document.aidiskForm.submit();
			}
			break;
	}
}

function resultOfSwitchShareMode(){
	refreshpage();
}

function showAccountMenu(){
	var account_menu_code = "";
	
	if(this.accounts.length <= 0)
		account_menu_code += '<div class="noAccount" id="noAccount"><#Noaccount#></div>\n'
	else
		for(var i = 0; i < this.accounts.length; ++i){
			account_menu_code += '<div class="accountName" id="';
			account_menu_code += "account"+i;
			account_menu_code += '" onClick="setSelectAccount('+i+');">'
			account_menu_code += decodeURIComponent(this.accounts[i]);
			account_menu_code += '</div>\n';
		}
	
	$("account_menu").innerHTML = account_menu_code;
	
	if(this.accounts.length > 0){
		if(get_manage_type(PROTOCOL) == 1)
			setSelectAccount(0);
	}
}

function showAccountControl(protocol){
	var status;
	var str_on, str_off;
	
	if(protocol != "cifs" && protocol != "ftp" && protocol != "webdav")
		return;
	
	status = get_manage_type(protocol);
	
	str_on = "<#enableAccountManage#>";
	str_off = "<#disableAccountManage#>";
	
	switch(status){
		case 1:
			$("accountMask").style.display = "none";
			//$("accountbtn").value = str_off;
			$("accountbtn").innerHTML = str_off;
			break;
		case 0:
			$("accountMask").style.display = "block";
			//$("accountbtn").value = str_on;
			$("accountbtn").innerHTML = str_on;
			break;
	}
}

function showPermissionTitle(){
	var code = "";
	
	code += '<table width="190"><tr>';
	
	if(PROTOCOL == "cifs"){
		code += '<td width="34%" align="center">R/W</td>';
		code += '<td width="28%" align="center">R</td>';
		code += '<td width="38%" align="center">No</td>';
	}else if(PROTOCOL == "ftp"){
		code += '<td width="28%" align="center">R/W</td>';
		code += '<td width="22%" align="center">W</td>';
		code += '<td width="22%" align="center">R</td>';
		code += '<td width="28%" align="center">No</td>';
	}
	
	code += '</tr></table>';
	
	$("permissionTitle").innerHTML = code;
}

var controlApplyBtn = 0;
function showApplyBtn(){
	if(this.controlApplyBtn == 1){
		$("changePermissionBtn").className = "button_gen";
		$("changePermissionBtn").disabled = false;
	}else{
		$("changePermissionBtn").className = "button_gen_dis";
		$("changePermissionBtn").disabled = true;
	}	
}

function setSelectAccount(account_order){
	this.selectedAccount = accounts[account_order];
	
	onEvent();
	
	show_permissions_of_account(account_order, PROTOCOL);
	contrastSelectAccount(account_order);
}

function getSelectedAccount(){
	return this.selectedAccount;
}

function show_permissions_of_account(account_order, protocol){
	var accountName = accounts[account_order];
	var poolName;
	var permissions;

	for(var i = 0; i < pool_devices().length; ++i){
		poolName = pool_devices()[i];
		if(!this.clickedFolderBarCode[poolName])
			continue;

		permissions = get_account_permissions_in_pool(accountName, poolName);
		for(var j = 1; j < permissions.length; ++j){
			var folderBarCode = get_folderBarCode_in_pool(poolName, permissions[j][0]);
			if(protocol == "cifs")
				showPermissionRadio(folderBarCode, permissions[j][1]);
			else if(protocol == "ftp")
				showPermissionRadio(folderBarCode, permissions[j][2]);
			else{
				alert("Wrong protocol when get permission!");	// system error msg. must not be translate
				return;
			}
		}
	}
}

function get_permission_of_folder(accountName, poolName, folderName, protocol){
	var permissions = get_account_permissions_in_pool(accountName, poolName);
	
	for(var i = 1; i < permissions.length; ++i)
		if(permissions[i][0] == folderName){
			if(protocol == "cifs")
				return permissions[i][1];
			else if(protocol == "ftp")
				return permissions[i][2];
			else{
				alert("Wrong protocol when get permission!");	// system error msg. must not be translate
				return;
			}
		}
	
	alert("Wrong folderName when get permission!");	// system error msg. must not be translate
}

function contrastSelectAccount(account_order){
	if(this.lastClickedAccount != 0){
		this.lastClickedAccount.style.marginRight = "0px";
		this.lastClickedAccount.style.background = "url(/images/New_ui/advancesetting/user_icon0.png)left no-repeat";
		this.lastClickedAccount.style.cursor = "pointer";
		this.lastClickedAccount.style.fontWeight ="normal";
	}
	
	var selectedObj = $("account"+account_order);
	
	selectedObj.style.marginRight = "-1px";
	selectedObj.style.background = "url(/images/New_ui/advancesetting/user_icon.png) left no-repeat";
	selectedObj.style.cursor = "default";
	selectedObj.style.fontWeight ="bolder";
	
	this.lastClickedAccount = selectedObj;
}

function submitChangePermission(protocol){
	var orig_permission;
	var target_account = null;
	var target_folder = null;
	
	for(var i = -1; i < accounts.length; ++i){
		if(i == -1)
			target_account = "guest";
		else
			target_account = accounts[i];
		
		if(!this.changedPermissions[target_account])
			continue;

		for(var j = 0; j < pool_devices().length; ++j){
			if(!this.changedPermissions[target_account][pool_devices()[j]])
				continue;
			
			folderlist = get_sharedfolder_in_pool(pool_devices()[j]);
			
			for(var k = 0; k < folderlist.length; ++k){
				target_folder = folderlist[k];
				
				if(!this.changedPermissions[target_account][pool_devices()[j]][target_folder])
					continue;
				
				if(target_account == "guest")
					orig_permission = get_permission_of_folder(null, pool_devices()[j], target_folder, PROTOCOL);
				else
					orig_permission = get_permission_of_folder(target_account, pool_devices()[j], target_folder, PROTOCOL);
				if(this.changedPermissions[target_account][pool_devices()[j]][target_folder] == orig_permission)
					continue;
				
				// the item which was set already
				if(this.changedPermissions[target_account][pool_devices()[j]][target_folder] == -1)
					continue;
				
				document.aidiskForm.action = "/aidisk/set_account_permission.asp";
				if(target_account == "guest")
					$("account").disabled = 1;
				else{
					$("account").disabled = 0;
					$("account").value = target_account;
				}
				$("pool").value = pool_devices()[j];
				if(target_folder == "")
					$("folder").disabled = 1;
				else{
					$("folder").disabled = 0;
					$("folder").value = target_folder;
				}
				$("protocol").value = protocol;
				$("permission").value = this.changedPermissions[target_account][pool_devices()[j]][target_folder];
				
				// mark this item which is set
				this.changedPermissions[target_account][pool_devices()[j]][target_folder] = -1;
				/*alert("account = "+$("account").value+"\n"+
					  "pool = "+$("pool").value+"\n"+
					  "folder = "+$("folder").value+"\n"+
					  "protocol = "+$("protocol").value+"\n"+
					  "permission = "+$("permission").value);//*/
				showLoading();
				document.aidiskForm.submit();
				return;
			}
		}
	}
	
	refreshpage();
}

function changeActionButton(selectedObj, type, action, flag){
	if(type == "User")
		if(this.accounts.length <= 0)
			if(action == "Del" || action == "Mod")
				return;
	
	if(typeof(flag) == "number")
		selectedObj.src = '/images/New_ui/advancesetting/'+type+action+'_'+flag+'.png';
	else
		selectedObj.src = '/images/New_ui/advancesetting/'+type+action+'.png';
}

function resultOfCreateAccount(){
	refreshpage();
}

function onEvent(){
	// account action buttons
	if(get_manage_type(PROTOCOL) == 1 && accounts.length < 6){
		changeActionButton($("createAccountBtn"), 'User', 'Add', 0);
		
		$("createAccountBtn").onclick = function(){
				popupWindow('OverlayMask','/aidisk/popCreateAccount.asp');
			};
		$("createAccountBtn").onmouseover = function(){
				changeActionButton(this, 'User', 'Add', 1);
			};
		$("createAccountBtn").onmouseout = function(){
				changeActionButton(this, 'User', 'Add', 0);
			};
	}
	else{
		changeActionButton($("createAccountBtn"), 'User', 'Add');
		
		$("createAccountBtn").onclick = function(){};
		$("createAccountBtn").onmouseover = function(){};
		$("createAccountBtn").onmouseout = function(){};
		$("createAccountBtn").title = (accounts.length < 6)?"<#AddAccountTitle#>":"<#account_overflow#>";
	}
	
	if(this.accounts.length > 0 && this.selectedAccount != null && this.selectedAccount.length > 0 && this.accounts[0] != this.selectedAccount){
		changeActionButton($("modifyAccountBtn"), 'User', 'Mod', 0);
		
		$("modifyAccountBtn").onclick = function(){
				if(!selectedAccount){
					alert("<#AiDisk_unselected_account#>");
					return;
				}
				
				popupWindow('OverlayMask','/aidisk/popModifyAccount.asp');
			};
		$("modifyAccountBtn").onmouseover = function(){
				changeActionButton(this, 'User', 'Mod', 1);
			};
		$("modifyAccountBtn").onmouseout = function(){
				changeActionButton(this, 'User', 'Mod', 0);
			};
	}
	else{
		changeActionButton($("modifyAccountBtn"), 'User', 'Mod');
		
		$("modifyAccountBtn").onclick = function(){};
		$("modifyAccountBtn").onmouseover = function(){};
		$("modifyAccountBtn").onmouseout = function(){};
	}
	
	if(this.accounts.length > 1 && this.selectedAccount != null && this.selectedAccount.length > 0 && this.accounts[0] != this.selectedAccount){
		changeActionButton($("deleteAccountBtn"), 'User', 'Del', 0);
		
		$("deleteAccountBtn").onclick = function(){
				if(!selectedAccount){
					alert("<#AiDisk_unselected_account#>");
					return;
				}
				
				popupWindow('OverlayMask','/aidisk/popDeleteAccount.asp');
			};
		$("deleteAccountBtn").onmouseover = function(){
				changeActionButton(this, 'User', 'Del', 1);
			};
		$("deleteAccountBtn").onmouseout = function(){
				changeActionButton(this, 'User', 'Del', 0);
			};
	}
	else{
		changeActionButton($("deleteAccountBtn"), 'User', 'Del');
		
		$("deleteAccountBtn").onclick = function(){};
		$("deleteAccountBtn").onmouseover = function(){};
		$("deleteAccountBtn").onmouseout = function(){};
	}
	
	// folder action buttons
	if(this.selectedPoolOrder >= 0 && this.selectedFolderOrder < 0){
		changeActionButton($("createFolderBtn"), 'Folder', 'Add', 0);
		
		$("createFolderBtn").onclick = function(){
				if(selectedDiskOrder < 0){
					alert("<#AiDisk_unselected_disk#>");
					return;
				}
				if(selectedPoolOrder < 0){
					alert("<#AiDisk_unselected_partition#>");
					return;
				}
				
				popupWindow('OverlayMask','/aidisk/popCreateFolder.asp');
			};
		$("createFolderBtn").onmouseover = function(){
				changeActionButton(this, 'Folder', 'Add', 1);
			};
		$("createFolderBtn").onmouseout = function(){
				changeActionButton(this, 'Folder', 'Add', 0);
			};
	}
	else{
		changeActionButton($("createFolderBtn"), 'Folder', 'Add');
		
		$("createFolderBtn").onclick = function(){};
		$("createFolderBtn").onmouseover = function(){};
		$("createFolderBtn").onmouseout = function(){};
	}
	
	if(this.selectedFolderOrder >= 0){
		changeActionButton($("deleteFolderBtn"), 'Folder', 'Del', 0);
		changeActionButton($("modifyFolderBtn"), 'Folder', 'Mod', 0);
		
		$("deleteFolderBtn").onclick = function(){
				if(selectedFolderOrder < 0){
					alert("<#AiDisk_unselected_folder#>");
					return;
				}
				
				popupWindow('OverlayMask','/aidisk/popDeleteFolder.asp');
			};
		$("deleteFolderBtn").onmouseover = function(){
				changeActionButton(this, 'Folder', 'Del', 1);
			};
		$("deleteFolderBtn").onmouseout = function(){
				changeActionButton(this, 'Folder', 'Del', 0);
			};
		
		$("modifyFolderBtn").onclick = function(){
				if(selectedFolderOrder < 0){
					alert("<#AiDisk_unselected_folder#>");
					return;
				}
				
				popupWindow('OverlayMask','/aidisk/popModifyFolder.asp');
			};
		$("modifyFolderBtn").onmouseover = function(){
				changeActionButton(this, 'Folder', 'Mod', 1);
			};
		$("modifyFolderBtn").onmouseout = function(){
				changeActionButton(this, 'Folder', 'Mod', 0);
			};
	}
	else{
		changeActionButton($("deleteFolderBtn"), 'Folder', 'Del');
		changeActionButton($("modifyFolderBtn"), 'Folder', 'Mod');
		
		$("deleteFolderBtn").onclick = function(){};
		$("deleteFolderBtn").onmouseover = function(){};
		$("deleteFolderBtn").onmouseout = function(){};
		
		$("modifyFolderBtn").onclick = function(){};
		$("modifyFolderBtn").onmouseover = function(){};
		$("modifyFolderBtn").onmouseout = function(){};
	}
	
	$("changePermissionBtn").onclick = function(){
			submitChangePermission(PROTOCOL);
		};
}

function unload_body(){
	$("createAccountBtn").onclick = function(){};
	$("createAccountBtn").onmouseover = function(){};
	$("createAccountBtn").onmouseout = function(){};
	
	$("deleteAccountBtn").onclick = function(){};
	$("deleteAccountBtn").onmouseover = function(){};
	$("deleteAccountBtn").onmouseout = function(){};
	
	$("modifyAccountBtn").onclick = function(){};
	$("modifyAccountBtn").onmouseover = function(){};
	$("modifyAccountBtn").onmouseout = function(){};
	
	$("createFolderBtn").onclick = function(){};
	$("createFolderBtn").onmouseover = function(){};
	$("createFolderBtn").onmouseout = function(){};
	
	$("deleteFolderBtn").onclick = function(){};
	$("deleteFolderBtn").onmouseover = function(){};
	$("deleteFolderBtn").onmouseout = function(){};
	
	$("modifyFolderBtn").onclick = function(){};
	$("modifyFolderBtn").onmouseover = function(){};
	$("modifyFolderBtn").onmouseout = function(){};
}
</script>
</head>

<body onLoad="initial();" onunload="unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>

<form method="post" name="aidiskForm" action="" target="hidden_frame">
<input type="hidden" name="motion" id="motion" value="">
<input type="hidden" name="layer_order" id="layer_order" value="">
<input type="hidden" name="protocol" id="protocol" value="">
<input type="hidden" name="mode" id="mode" value="">
<input type="hidden" name="flag" id="flag" value="">
<input type="hidden" name="account" id="account" value="">
<input type="hidden" name="pool" id="pool" value="">
<input type="hidden" name="folder" id="folder" value="">
<input type="hidden" name="permission" id="permission" value="">
</form>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="current_page" value="Advanced_AiDisk_ftp.asp">
</form>

<table width="983" border="0" align="center" cellpadding="0" cellspacing="0" class="content">
  <tr>
	<td width="17">&nbsp;</td>				
	
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>
	
	<td valign="top">
	  <div id="tabMenu" class="submenuBlock"></div>
	  <!--=====Beginning of Main Content=====-->
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top">

	  <table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">

<tbody>
	<tr>
		  <td bgcolor="#4D595D">
		  <div>&nbsp;</div>
			<div style="width:730px">
				<table width="730px">
					<tr>
						<td align="left">
							<span class="formfonttitle"><#menu5_4#> - <#menu5_4_2#></span>
						</td>
						<td align="right">
							<img onclick="go_setting('/APP_Installation.asp')" align="right" style="cursor:pointer;position:absolute;margin-left:-20px;margin-top:-30px;" title="Back to USB Extension" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
						</td>
					</tr>
				</table>
			</div>
			<div style="margin:5px;"><img src="/images/New_ui/export/line_export.png"></div>

		  <div class="formfontdesc"><#FTP_desc#></div>
			<!--input id="sharebtn" type="button" value="" class="button_gen" onClick="switchAppStatus(PROTOCOL);"-->
			<a href="javascript:switchAppStatus(PROTOCOL);"><div class="titlebtn" align="center"><span id="sharebtn" style="*width:196px;"></span></div></a>
			<!--input id="accountbtn" type="button" value="" class="button_gen_long" onClick="switchAccount(PROTOCOL);"-->
			<a href="javascript:switchAccount(PROTOCOL);"><div class="titlebtn" align="center"><span id="accountbtn" style="*width:266px;"></span></div></a>
			<!--input id="refreshbtn" type="button" value="<#DrSurf_refresh_page#>" class="button_gen" onClick="refreshpage();"-->
			<a href="javascript:refreshpage();"><div class="titlebtn" align="center"><span id="refreshbtn" style="*width:136px;"><#DrSurf_refresh_page#></span></div></a>	
			<br/><br/>
			
			<!-- The table of share. -->
			<div id="shareStatus">
				<!-- The mask of all share table. -->
				<div id="tableMask"></div>
				<!-- The mask of accounts. -->
				<div id="accountMask"></div>
		  
		<!--99 The action buttons of accounts and folders.  start  The action buttons of accounts and folders.-->
		<table width="740px"  height="35" cellpadding="2" cellspacing="0" class="accountBar">
			<tr>
				<!-- The action buttons of accounts. -->
    	    <td width="25%" style="border: 1px solid #222;">	
		      	<img id="createAccountBtn" src="/images/New_ui/advancesetting/UserAdd.png" hspace="1" title="<#AddAccountTitle#>">
						<img id="deleteAccountBtn" src="/images/New_ui/advancesetting/UserDel.png" hspace="1" title="<#DelAccountTitle#>">		        		
						<img id="modifyAccountBtn" src="/images/New_ui/advancesetting/UserMod.png" hspace="1" title="<#ModAccountTitle#>">	
		  		</td>
				<!-- The action buttons of folders. -->
    	  	<td width="75%">
						<img id="createFolderBtn" hspace="1" title="<#AddFolderTitle#>">
						<img id="deleteFolderBtn" hspace="1" title="<#DelFolderTitle#>">
						<img id="modifyFolderBtn" hspace="1" title="<#ModFolderTitle#>">						
		  		</td>
  			</tr>
	  	</table>
	  	<!-- The action buttons of accounts and folders.  END  The action buttons of accounts and folders.-->
	  	
	</div>
	<!-- The table of share. END-------------------------------------------------------------------------------->
	
		 <!--The table of accounts and folders.       start    The table of accounts and folders. -->
	      <table width="740px" height="200" align="center"  border="1" cellpadding="4" cellspacing="0" class="AiDiskTable"> 
  		 	<tr>
		    		<!-- The left side table of accounts. -->
    	    			<th align="left" valign="top">
			  		<div id="account_menu"></div>
		    		</th>
		    
				<!-- The right side table of folders.    Start  -->
    	    			<td valign="top">
			  		<table width="480"  border="0" cellspacing="0" cellpadding="0" class="FileStatusTitle">
		  	    		<tr>
		    	  			<td width="290" height="20" align="left">
				    			<div class="machineName"><#Web_Title2#></div>
				    			<!--<img src="/images/500icon.gif" width="20" height="20" hspace="2" align="absmiddle">-->
				    		</td>
				  		<td>
				    			<div id="permissionTitle"></div>
				  		</td>
		    			</tr>
			  		</table>
			 	 <!-- the tree of folders -->
  		      	<div id="e0" style="font-size:10pt; margin-top:2px;"></div>
			  	<div style="text-align:center; margin:10px auto; border-top:1px dotted #CCC; width:95%; padding:2px;">
			    		<input name="changePermissionBtn" id="changePermissionBtn" type="button" value="<#CTL_apply#>" class="button_gen_dis" disabled="disabled">
			  	</div>
		    		</td>
		    		<!-- The right side table of folders.    End -->
          		</tr>
	    	</table>
	    	<!-- The table of accounts and folders.       END    The table of accounts and folders.  -->
	    	
	</td>


</tr>
</tbody>

</table>
	  <!-- The table of DDNS. -->
    </td>
  <td width="10"></td>
  </tr>
</table>

			</td>
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>




<div id="footer"></div>

<!-- mask for disabling AiDisk -->
<div id="OverlayMask" class="popup_bg">
	<div align="center">
	<iframe src="" frameborder="0" scrolling="no" id="popupframe" width="400" height="400" allowtransparency="true" style="margin-top:150px;"></iframe>
	</div>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>

</body>
</html>
