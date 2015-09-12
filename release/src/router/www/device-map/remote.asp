<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>Untitled Document</title>
<link rel="stylesheet" type="text/css" href="../NM_style.css">
<link rel="stylesheet" type="text/css" href="../form_style.css">
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script>
if(parent.location.pathname.search("index") === -1) top.location.href = "../index.asp";

var remoteIP = '<% nvram_get("lan_gateway_now"); %>';
remoteIP = (remoteIP == '')?'<% nvram_get("lan_gateway_now"); %>';
var re_status = parent.getConnectingStatus();

function initial(){
	if(re_status == -1){
		showtext(document.getElementById("Connstatus"), "<#OP_RE_item#>");
		document.getElementById("remoteIP_tr").style.display = "none";
		setTimeout("set_re_status();",6000);
	}
	else
		set_re_status();
		
	document.getElementById("remoteIP_span").innerHTML = (remoteIP == "")?"<#AP_fail_get_IPaddr#>":remoteIP;
}

function set_re_status(){
	re_status = parent.getConnectingStatus();
	//alert("re_status "+re_status);
	if(re_status == 2){
		showtext(document.getElementById("Connstatus"), "<#Connected#>");
		document.getElementById("remoteIP_tr").style.display = "";
	}
	else{
		showtext(document.getElementById("Connstatus"), "<#CTL_Disconnect#>");
		document.getElementById("remoteIP_tr").style.display = "none";
	}
}

function sbtnOver(o){
	o.style.color = "#FFFFFF";		
	o.style.background = "url(/images/sbtn.gif) #FFCC66";
	o.style.cursor = "pointer";
}

function sbtnOut(o){
	o.style.color = "#000000";
	o.style.background = "url(/images/sbtn0.gif) #FFCC66";
}
</script>
</head>

<body class="statusbody" onload="initial();">
<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px">
  <tr>
    <th width="120"><#PPPConnection_x_WANLink_itemname#></th>
    <td width="150"><span id="Connstatus"></span></td>
  </tr>
  <tr id="remoteIP_tr">
    <th><#AP_Remote_IP#></th>
    <td><span id="remoteIP_span"></span></td>
  </tr>
  <tr>
    <th><#AP_survey#></th>
    <td><input type="button" class="sbtn" value="<#btn_go#>" onclick="javascript:parent.location.href='../survey.htm';" onmouseover="sbtnOver(this);" onmouseout="sbtnOut(this);"></td>
  </tr>   
</table>
</body>
</html>
