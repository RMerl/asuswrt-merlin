<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">

<script type="text/javascript" src="/disk_msg.js"></script>
<script>
function set_AiDisk_status_error(error_msg){
	alert(error_msg);
	parent.resultOfSwitchAppStatus(error_msg);
}

function set_AiDisk_status_success(){
	parent.resultOfSwitchAppStatus();
}
</script>
</head>

<body onload="set_AiDisk_status_success();">

<% set_AiDisk_status(); %>

</body>
</html>
