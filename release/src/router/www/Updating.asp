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
	parent.$("hiddenMask").style.visibility = "hidden";
	if(parent.based_modelid == "RT-AC56U"
			|| parent.based_modelid == "RT-AC66U"
			|| parent.based_modelid == "RT-AC67U"){		//MODELDEP: RT-AC56U, RT-AC66U, RT-AC67U 2013.02
			parent.showLoadingBar(90);
			setTimeout("parent.detect_httpd();", 92000);
	}else{
			parent.showLoadingBar(270);
			setTimeout("parent.detect_httpd();", 272000);		
	}	
</script>
</body>
</html>
