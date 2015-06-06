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
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script>
<% get_AiDisk_status(); %>
var account_num;
//var accounts;
var pools = [];
var folderlist = [];
var page = parseInt('<% get_parameter("page"); %>'-'0');

function initial(){
	show_menu();
	document.getElementById("_APP_Installation").innerHTML = '<table><tbody><tr><td><div class="_APP_Installation""></div></td><td><div style="width:120px;"><#Menu_usb_application#></div></td></tr></tbody></table>';
	document.getElementById("_APP_Installation").className = "menu_clicked";

	document.getElementById("statusframe").style.display = "block";
	
	if(page == 2)
		show_iframe("aidisk/Aidisk-2.asp");
	else if(page == 3)
		show_iframe("aidisk/Aidisk-3.asp");
	else if(page == 4)
		show_iframe("aidisk/Aidisk-4.asp");
	else
		show_iframe("aidisk/Aidisk-1.asp");
	
	if(document.getElementById("dummyShareway").value == "")
		document.getElementById("dummyShareway").value = 0;
}

function show_iframe(src){
	document.getElementById("sub_frame").src = "";
	
	setTimeout('document.getElementById("sub_frame").src = \"'+src+'\";', 1);
}

function show_iframe_page(iframe_id){
	if(iframe_id)
		if(document.getElementById(iframe_id))
			return document.getElementById(iframe_id).src;
	
	return "";
}

function show_help_iframe(page_num){
}

function big_help_td(){
	document.getElementById("help_td").style.width = "300px";
	document.getElementById("statusframe").style.width = "300px";
}

function restore_help_td(){
	document.getElementById("hint_body").style.display = "block"; //avoid this block to be hide when open ASUS TOS in step2
	document.getElementById("help_td").style.width = "170px";
	document.getElementById("statusframe").style.width = "200px";
}

/*function get_account_parameter(){
	account_num = document.getElementById("accountNum").value;
	
	accounts = new Array(account_num);
	
	for(var i = 0; i < account_num; ++i){
		accounts[i] = new Array(3);
		
		accounts[i][0] = document.getElementById("account"+i).value;
		accounts[i][1] = document.getElementById("passwd"+i).value;
		accounts[i][2] = document.getElementById("permission"+i).value;
	}
}//*/

function initialAccount(){
	document.applyForm.action = "/aidisk/initial_account.asp";
	
	document.applyForm.submit();
}

function resultOfInitialAccount(){
	if(document.getElementById("dummyShareway").value == "1")
		createAccount();
	else{
		submitChangeAllFolderPermission('<% nvram_char_to_ascii("", "http_username"); %>', "3", "ftp");
	}
}

function createAccount(){
	//if(accounts[0]){
		document.applyForm.protocol.value = "";
		document.applyForm.pool.value = "";
		document.applyForm.folder.value = "";
		document.applyForm.permission.value = "";
		
		document.applyForm.action = "/aidisk/create_account.asp";
		document.applyForm.account.value = document.getElementById("account1").value;
		document.applyForm.password.value = document.getElementById("passwd1").value;
		
		document.applyForm.submit();
}

function resultOfCreateAccount(){
	submitChangeAllFolderPermission(document.getElementById("account1").value, document.getElementById("permission1").value, "ftp");
}

function submitChangePermission(account, permission, protocol){
	if(pools && pools.length > 0){
		if(folderlist && folderlist.length > 0){
			document.applyForm.password.value = "";
			
			document.applyForm.action = "/aidisk/set_account_permission.asp";
			document.applyForm.account.value = account;
			document.applyForm.pool.value = pools[0];
			if(folderlist[0] == "")
				document.applyForm.folder.disabled = 1;
			else{
				document.applyForm.folder.disabled = 0;
				document.applyForm.folder.value = folderlist[0];
			}
			document.applyForm.protocol.value = protocol;
			document.applyForm.permission.value = permission;
			document.applyForm.flag.value = "aidisk_wizard";
			
			folderlist.shift();
			
			document.applyForm.submit();
			return;
		}
		else{
			pools.shift();
			
			if(pools && pools.length > 0){
				folderlist = get_sharedfolder_in_pool(pools[0]);
			
				submitChangePermission(account, permission, protocol);
				return;
			}
		}
	}
	
	if(document.getElementById("dummyShareway").value == "1"){
		document.getElementById("dummyShareway").value = "";

	 	require(['/require/modules/diskList.js'], function(diskList){
			pools = [];
			for(var i=0; i < usbDevicesList.length; i++){
				for(var j=0; j < usbDevicesList[i].partition.length; j++){
					pools.push(usbDevicesList[i].partition[j].mountPoint);
				}
			}
			if(pools && pools.length > 0)
				folderlist = get_sharedfolder_in_pool(pools[0]);
			
			submitChangePermission('<% nvram_char_to_ascii("", "http_username"); %>', "3", "ftp");
	 	});
	}
	else
		switchShareMode("ftp", "account");
}

function submitChangeAllFolderPermission(account, permission, protocol){
	
	if(document.getElementById("dummyShareway").value == "1"){
		document.getElementById("dummyShareway").value = "";
		document.applyForm.password.value = "";
		document.applyForm.action = "/aidisk/set_account_all_folder_permission.asp";
		document.applyForm.account.value = account;
		document.applyForm.folder.disabled = 0;
		document.applyForm.protocol.value = protocol;
		document.applyForm.permission.value = permission;
		document.applyForm.flag.value = "aidisk_wizard";
		document.applyForm.submit();
	}
	else {
		switchShareMode("ftp", "account");
	}
}

function switchShareMode(protocol, mode){
	document.applyForm.account.value = "";
	document.applyForm.pool.value = "";
	document.applyForm.folder.value = "";
	document.applyForm.permission.value = "";
	
	document.applyForm.action = "/aidisk/switch_share_mode.asp";
	document.applyForm.protocol.value = protocol;
	document.applyForm.mode.value = mode;
	
	document.applyForm.submit();
}

function resultOfSwitchShareMode(){
	switchAppStatus("ftp", "on");
}

function switchAppStatus(protocol, flag){
	document.applyForm.mode.value = "";
	
	document.applyForm.action = "/aidisk/switch_AiDisk_app.asp";
	document.applyForm.protocol.value = protocol;
	document.applyForm.flag.value = flag;
	
	document.applyForm.submit();
}

function resultOfSwitchAppStatus(error_msg){
	finish_dummyway_setting();
}

function finish_dummyway_setting(){
	switchDDNS();
}

function switchDDNS(){
	document.ddnsForm.ddns_enable_x.value = getASUSDDNS_enable();
	document.ddnsForm.current_page.value = document.parameterForm.next_page.value;
	
	document.ddnsForm.submit();
}

var ddns_enable_x = '<% nvram_get("ddns_enable_x"); %>';

function setASUSDDNS_enable(flag){
		this.ddns_enable_x = flag;
}

function getASUSDDNS_enable(){
	return this.ddns_enable_x;
}
</script>
</head>

<body onload="initial();" onunload="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>

<form method="post" name="applyForm" id="applyForm" action="" target="hidden_frame">
<input type="hidden" name="dummyShareway" id="dummyShareway" value="<% nvram_get("dummyShareway"); %>">
<input type="hidden" name="account" id="account" value="">
<input type="hidden" name="password" id="password" value="">
<input type="hidden" name="protocol" id="protocol" value="">
<input type="hidden" name="mode" id="mode" value="">
<input type="hidden" name="pool" id="pool" value="">
<input type="hidden" name="folder" id="folder" value="">
<input type="hidden" name="permission" id="permission" value="">
<input type="hidden" name="flag" id="flag" value="on">
</form>

<form method="post" name="ddnsForm" id="ddnsForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="ddns_enable_x" value="">
</form>

<form method="post" name="parameterForm" id="parameterForm" action="" target="">
<input type="hidden" name="next_page" id="next_page" value="">
<input type="hidden" name="accountNum" id="accountNum" value="0">
<!--input type="hidden" name="account0" id="account0" value="">
<input type="hidden" name="passwd0" id="passwd0" value="">                                
<input type="hidden" name="permission0" id="permission0" value=""-->
<input type="hidden" name="account1" id="account1" value="">
<input type="hidden" name="passwd1" id="passwd1" value="">
<input type="hidden" name="permission1" id="permission1" value="">
</form>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
</form>

<table border="0" align="center" cellpadding="0" cellspacing="0" class="content">
	<tr>
		<td valign="top" width="17">&nbsp;</td>

		<!--=====Beginning of Main Menu=====-->
		<td valign="top" width="202">
			<div id="mainMenu"></div>
			<div id="subMenu"></div>
		</td>

		<td height="430" align="center" valign="top">
			<div id="tabMenu"></div>

		<!--==============Beginning of hint content=============-->
<table id="AiDiskFormTitle" width="100%" height="100%" border="0" bgcolor="#4d595d" cellpadding="0"  cellspacing="0" style="margin-top:-140px;-webkit-border-radius:3px;-moz-border-radius:3px;border-radius:3px;">
  <tr>
		<td align="left" valign="top">
			<iframe id="sub_frame" src="" width="760px" height="710px" frameborder="0" scrolling="no" style="position:relative;-webkit-border-radius:3px;-moz-border-radius:3px;border-radius:3px;"></iframe>
		</td>
		<td id="help_td" style="width:15px;" align="center" valign="top">
			<div id="helpicon"></div>
			<div id="hintofPM" style="display:none;">
				<table width="100%" cellpadding="0" cellspacing="1" class="Help" bgcolor="#999999">
					<thead>
					<tr>
						<td>
							<div id="helpname" class="AiHintTitle"></div>
						</td>
					</tr>
					</thead>

					<tr>
						<td valign="top">
							<div class="hint_body2" id="hint_body"></div>
							<iframe id="statusframe" name="statusframe" class="statusframe" src="/aidisk/Aidisk-1_help.asp" frameborder="0" /></iframe>
						</td>
					</tr>
				</table>
			</div>
		</td>
				</tr>
			</table>
		</td>
		
		<td width="20"></td>
	</tr>
</table>
		<!--==============Ending of hint content=============-->
		
<div id="footer"></div>
</body>
</html>
