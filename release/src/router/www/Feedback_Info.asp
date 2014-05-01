<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>

<!--Feedback_Info.asp-->
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="/images/favicon.png">
<link rel="icon" href="/images/favicon.png">
<title><#Web_Title#> - <#menu_feedback#></title>
<link rel="stylesheet" type="text/css" href="/index_style.css">
<link rel="stylesheet" type="text/css" href="/form_style.css">
<link rel="stylesheet" type="text/css" href="/other.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script>
var fb_response = "<% nvram_get("feedbackresponse"); %>";
	
function initial(){
	show_menu();
	check_info();
}

function check_info(){
	//Viz if(fb_response == "1"){
		document.getElementById("fb_success").style.display = "";
		
	//Viz }else{	// 0:default 2:Fail 3:limit reached
		//Viz document.getElementById("fb_fail").style.display = "";		
	//Viz }
}

function redirect(){
	document.location.href = "Advanced_Feedback.asp";
}

</script>
<style>
.feedback_info_0{
	font-size:20px;
	margin-left:25px;
	text-shadow: 1px 1px 0px black;
	font-family: Arial, Helvetica, sans-serif;
	font-weight: bolder;
}

.feedback_info_1{
	font-size:13px;
	margin-left:30px;
	font-family: Arial, Helvetica, sans-serif;
}
</style>	
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value=""></form>
<FORM METHOD="POST" ACTION="" name="uiForm" target="hidden_frame">
<INPUT TYPE="HIDDEN" NAME="dhcppoolFlag" VALUE="0">
<table class="content" align="center" cellpadding="0" cellspacing="0">
<tr>
<td width="17">&nbsp;</td>
<td valign="top" width="202">
<div id="mainMenu"></div>
<div id="subMenu"></div>
</td>
<td valign="top">
<div id="tabMenu" class="submenuBlock"></div>
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
<tr>
<td align="left" valign="top">
<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
<tbody>
<tr>
<td bgcolor="#4D595D" valign="top">
<div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_6#> - <#menu_feedback#></div>
<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

<div id="fb_success" style="display:none;">
	<br>
	<br>
	<div class="feedback_info_0">Thanks for taking the time to submit your feedback.</div>
	<br>
	<br>
	<div class="feedback_info_1">We are working hard to improve the firmware of <#Web_Title2#> and your feedback is very important to us. However due to the volume of feedback, please expect a slight delay in email responses.</div>
	<br>
	<br>
	<div class="feedback_info_1">To get help from other users, you could post your question in the <a href="http://vip.asus.com/forum/topic.aspx?board_id=11&SLanguage=en-us" style="color:#FFCC00;" target="_blank">ASUS VIP Forum</a>.</div>
	<br>
	<br>	
</div>

<div id="fb_fail" style="display:none;">
</div>

<div id="fb_deny" style="display:none;">
</div>	



<div class="apply_gen">
			<input class="button_gen" onclick="redirect();" type="button" value="<#Main_alert_proceeding_desc3#>"/>
</div>
</td>
</tr>
</tbody>
</table>
</td>
</form>
</tr>
</table>
</td>
<td width="10" align="center" valign="top">&nbsp;</td>
</tr>
</table>
<div id="footer"></div>
</body>
</html>

