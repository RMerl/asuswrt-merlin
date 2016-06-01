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
	var reboottime = eval("<% get_default_reboot_time(); %>");
	var upgrade_fw_status = '<% nvram_get("upgrade_fw_status"); %>';
	
	if(upgrade_fw_status == 2 || upgrade_fw_status == 4){
		parent.document.getElementById("hiddenMask").style.visibility = "hidden";
		parent.document.getElementById('loading_block2').innerHTML = "<#FIRM_fail_desc#>";
		parent.document.getElementById('loading_block3').style.display = "none";
		parent.showLoadingBar(reboottime);
		setTimeout("parent.detect_httpd();", reboottime*1000);	
	}
	else if(upgrade_fw_status == 6){
		parent.document.getElementById("hiddenMask").style.visibility = "hidden";
		
                parent.document.getElementById('loading_block2').innerHTML = "<#FIRM_trx_valid_fail_desc#>";
                parent.document.getElementById('loading_block3').style.display = "none";
                parent.showLoadingBar(reboottime);
                setTimeout("parent.detect_httpd();", reboottime*1000);
	}
	else{
		aler("<#FIRM_fail_desc#>");
		parent.location.href=parent.location.href;
	}	
</script>
</body>
</html>
