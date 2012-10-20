<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>ASUS Wireless Router Web Manager</title>
<link rel="stylesheet" type="text/css" href="other.css">

<script>
var reboot_needed_time = <% get_default_reboot_time(); %>;
var action_mode = '<% get_parameter("action_mode"); %>';
function redirect(){
	parent.stopFlag = 1;
	setTimeout("redirect1();", reboot_needed_time*1000);
}

function redirect1(){
	if(action_mode == "Restore"){
		parent.$('drword').innerHTML = "<#Setting_factorydefault_iphint#><br/>";
		setTimeout("parent.hideLoading()",1000);
		setTimeout("parent.dr_advise();",1000);
		if(parent.location.href.indexOf("Advanced_SettingBackup_Content") >= 0)
				parent.location.href = "/QIS_wizard.htm?flag=welcome";
		else		
		parent.location.href = 'http://<% nvram_default_get("lan_ipaddr"); %>/QIS_wizard.htm?flag=welcome';
	}
	else{
		parent.location.href = "/";
		return false;
	}
}
</script>
</head>

<body onLoad="redirect();">
<script>
parent.hideLoading();
parent.showLoading(reboot_needed_time, "waiting");
</script>
</body>
</html>
