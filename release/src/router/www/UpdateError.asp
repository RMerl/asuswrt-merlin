<html>
<head>
<title>ASUS Wireless Router Web Manager</title>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
</head>
<body>
<script>
	var upgrade_fw_status = '<% nvram_get("upgrade_fw_status"); %>';
	if(upgrade_fw_status == 6){
		alert("<#FIRM_trx_valid_fail_desc#>");
	}
	else{
		alert("<#FIRM_fail_desc#>");
	}

	parent.location.href=parent.location.href;
</script>
</body>
</html>

