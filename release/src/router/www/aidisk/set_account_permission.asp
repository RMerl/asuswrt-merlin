<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">

<script type="text/javascript" src="/disk_msg.js"></script>
<script type="text/javascript">
var account = '<% get_parameter("account"); %>';
var permission = '<% get_parameter("permission"); %>';
var protocol = '<% get_parameter("protocol"); %>';
var flag = '<% get_parameter("flag"); %>';

function set_account_permission_error(error_msg){
	if(flag == "aidisk_wizard")
		//parent.alert_error_msg("<#AiDisk_Wizard_failedreson1#>");
		parent.alert_error_msg(error_msg);
	else
		parent.alert_error_msg(error_msg);
}

function set_account_permission_success(){
	if(flag == "aidisk_wizard")
		parent.submitChangePermission(account, permission, protocol);
	else
		parent.submitChangePermission(protocol);
}
</script>
</head>

<body>

<% set_account_permission(); %>

</body>
</html>
