<html>
<head>
<title>ASUS Wireless Router Web Manager</title>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<link href="other.css"  rel="stylesheet" type="text/css">
<style type="text/css">
body {
	background-image: url(images/bg.gif);
	margin:50px auto;
}
</style>

<script language="javascript">
<% login_state_hook(); %>

function initial(){
	document.getElementById("logined_ip_str").innerHTML = login_ip_str();
}
</script>
</head>

<body onload="initial()">
<form name="formname" method="POST">
<table width="500" border="0" align="center" cellpadding="10" cellspacing="0" class="erTable">
  <tr>
    <th align="left" valign="top" background="images/er_bg.gif">
		<div class="drword_Nologin">
	  	  	<p><#login_hint1#> <span id="logined_ip_str"></span></p>
	  	  	<p><#login_hint2#></p>
		</div>
		<!--div class="drImg"><img src="images/alertImg.png"></div-->		
		<div style="height:70px; "></div>
	  	</th>
  </tr>
</table>
</form>
</body>
</html>
