﻿<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">

<script type="text/javascript" src="/disk_msg.js"></script>
<script type="text/javascript">
function safely_mount_disk_error(error_msg){
	parent.alert_error_msg(error_msg);
}

function safely_mount_disk_success(){
	parent.showLoading(5);
	parent.refreshpage(5);
}
</script>
</head>

<body>
<% safely_mount_disk(); %>
  
</body>
</html>
