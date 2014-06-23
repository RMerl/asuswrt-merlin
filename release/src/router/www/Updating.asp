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
	var reboot_needed_time = eval("<% get_default_reboot_time(); %>");
	parent.$("hiddenMask").style.visibility = "hidden";
	if(parent.based_modelid == "RT-AC56S"
			|| parent.based_modelid == "RT-AC56U"
			|| parent.based_modelid == "RT-AC66U"
			|| parent.based_modelid == "RT-AC68U"
			|| parent.based_modelid == "RT-AC68U_V2"
			|| parent.based_modelid == "DSL-AC68U"
			|| parent.based_modelid == "RT-AC69U"
			|| parent.based_modelid == "RT-AC3200"){	//MODELDEP: RT-AC56S, RT-AC56U, RT-AC66U, RT-AC68U, RT-AC68U_V2, RT-AC69U, RT-AC3200 2014.06
			reboot_needed_time += 30;
			parent.showLoadingBar(reboot_needed_time);
			reboot_needed_time += 2;
			setTimeout("parent.detect_httpd();", reboot_needed_time*1000);
	}else if(parent.based_modelid == "RT-AC52U"){			//MODELDEP: RT-AC52U
			reboot_needed_time += 40;
			parent.showLoadingBar(reboot_needed_time);
			reboot_needed_time += 2;
            setTimeout("parent.detect_httpd();", reboot_needed_time*1000);
	}else{
			parent.showLoadingBar(270);
			setTimeout("parent.detect_httpd();", 272000);		
	}	
</script>
</body>
</html>
