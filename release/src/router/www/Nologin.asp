<html>
<head>
<title>ASUS Wireless Router Web Manager</title>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<style type="text/css">
.Desc{
  text-shadow: 0px 1px 0px white;
  font-weight: bolder;
  font-size: 22px;
	font-family:Segoe UI, Arial, sans-serif;
  color: #000;				
	line-height: 40px;
	text-align: left;
}
</style>

<script language="javascript">
<% login_state_hook(); %>
var dhcpLeaseInfo = <% IP_dhcpLeaseInfo(); %>;

function initial(){
	var thehostName = "";
	for(var i=0; i<dhcpLeaseInfo.length; i++){
		if(dhcpLeaseInfo[i][0] == login_ip_str())
			thehostName = " (" + dhcpLeaseInfo[i][1] + ")";
	}

	document.getElementById("logined_ip_str").innerHTML = login_ip_str() + thehostName;
}
</script>
</head>

<body onload="initial()" style="text-align:center;background: #DDD">
	<div style="margin-top:100px;width:50%;margin-left:25%">
		<div class="Desc"><#login_hint1#> <span id="logined_ip_str"></span></div>
		<div class="Desc"><#login_hint2#></div>
	</div>
</body>
</html>
