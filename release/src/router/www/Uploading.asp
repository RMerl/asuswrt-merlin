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
var reboot_needed_time = eval("<% get_default_reboot_time(); %> + 5");
	parent.showLoadingBar(reboot_needed_time);
	setTimeout("parent.detect_httpd();", reboot_needed_time*1000);
</script>
</body>
</html>
