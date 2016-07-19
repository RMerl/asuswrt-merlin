<html>
<head>
<title>ASUS Wireless Router Web Manager</title>
</head>
<body>
</body>
<script>
(function(){
	<% login_state_hook(); %> 

	var loginUserIp = (function(){
		return (typeof login_ip_str === "function") ? login_ip_str().replace("0.0.0.0", "") : "";
	})();

	var loginUser = (function(){
		if(loginUserIp === "") return "";

		var dhcpLeaseInfo = [];
		var hostName = "";
		dhcpLeaseInfo.forEach(function(elem){
			if(elem[0] === loginUserIp){
				hostName = " (" + elem[1] + ")";
				return false;
			}
		})
		return '<#login_hint1#> ' + loginUserIp + hostName;
	})();

	top.document.body.style.background = "#DDD";
	top.document.body.style.textAlign = "center";
	top.document.body.innerHTML = ""
		+ '</style>'
		+ '<div class="Desc" style="'
			+ 'margin-top:100px;'
			+ 'line-height:60px;'
			+ 'text-shadow:0px 1px 0px white;'
			+ 'font-weight:bolder;'
			+ 'font-size:32px;'
			+ 'font-family:Segoe UI, Arial, sans-serif;'
			+ 'color:#000;'
			+ 'text-align:center;'
		+ '"><span><#login_hint2#><br>'
		+ loginUser
		+ '</span></div>';
})()
</script>
</html>