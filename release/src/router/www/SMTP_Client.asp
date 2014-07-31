<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - SMTP Client</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>

<script>
function initial(){
	show_menu();
}

function applyRule(){
	if(link_status == "2" && link_auxstatus == "0"){

		if(document.form.smtp_auth_x[1].checked)
				document.form.smtp_auth.value = "0";
		else
				document.form.smtp_auth.value = "1";
	
		document.form.submit();
		alert("The message has been sent out.");
	}
	else{
		alert("<#USB_Application_No_Internet#>");
		return false;
	}
}

function isEmail(strE) {
	if (strE.search(/^[A-Za-z0-9]+((-[A-Za-z0-9]+)|(\.[A-Za-z0-9]+)|(_[A-Za-z0-9]+))*\@[A-Za-z0-9]+((\.|-)[A-Za-z0-9]+)*\.[A-Za-z]+$/) != -1)
		return true;
	else
		return false;
}
</script>
</head>
<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="hiddenMask" class="popup_bg">
<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
<tr>
<td>
<div class="drword" id="drword" style="height:110px;"><#Main_alert_proceeding_desc4#> <#Main_alert_proceeding_desc1#>...
<br/>
<br/>
</div>
<div class="drImg"><img src="/images/alertImg.png"></div>
<div style="height:70px;"></div>
</td>
</tr>
</table>
</div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="current_page" value="SMTP_Client.asp">
<input type="hidden" name="action_mode" value="sendm">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="smtp_auth" value="">
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
<td bgcolor="#4D595D" valign="top" >
<div>&nbsp;</div>
<div class="formfonttitle">SMTP Client</div>
<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
<div class="formfontdesc"><!--## --></div>
<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
<thead>
	<tr>
		<td colspan="2">Server Information</td>	
	</tr>	
</thead>	
<tr>
	<th width="30%">SMTP Server</th>
	<td>
		<input type="text" name="smtp_gw" maxlength="30" class="input_25_table" value="">
	</td>
</tr>
<tr>
	<th width="30%">SMTP Server Port</th>
	<td>
		<input type="text" name="smtp_port" maxlength="5" class="input_6_table" value="">
	</td>
</tr>

<!--tr align="center">
	<td colspan="2">	
		<input class="button_gen" onclick="applyRule()" type="button" value="Send"/>
	</td>	
</tr-->
</table>

		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" style="margin-top:10px;">
		<thead>
		<tr>
			<td colspan="2">User Information</td>	
		</tr>	
		</thead>	
		<tr>
			<th width="30%">Name</th>
			<td>
				<input type="text" name="myname" maxlength="30" class="input_25_table" value="">
			</td>
		</tr>
		<tr>
			<th width="30%">Email Adress</th>
			<td>
				<input type="text" name="mymail" maxlength="50" class="input_25_table" value="">
			</td>
		</tr>

		<!--tr align="center">
			<td colspan="2">	
				<input class="button_gen" onclick="applyRule()" type="button" value="Send"/>
			</td>	
		</tr-->
		</table>
		
		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" style="margin-top:10px;">
		<thead>
		<tr>
			<td colspan="2">Login Information</td>	
		</tr>	
		</thead>	
		<tr>
			<th width="30%">Authenrization protocol</th>
			<td>
				<input type="radio" name="smtp_auth_x" value="1" <% nvram_match("smtp_auth", "1", "checked"); %>>Login
				<input type="radio" name="smtp_auth_x" value="0" <% nvram_match("smtp_auth", "0", "checked"); %>>Plain
			</td>
		</tr>
		<tr>
			<th width="30%">Username</th>
			<td>
				<input type="text" name="auth_user" maxlength="30" class="input_25_table" value="">
			</td>
		</tr>
		<tr>
			<th width="30%">Password</th>
			<td>
				<input type="text" name="auth_pass" maxlength="30" class="input_25_table" value="">
			</td>
		</tr>
		<!--tr align="center">
			<td colspan="2">	
				<input class="button_gen" onclick="applyRule()" type="button" value="Send"/>
			</td>	
		</tr-->
		</table>
		
		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" style="margin-top:10px;">
		<thead>
		<tr>
			<td colspan="2">Compose</td>	
		</tr>	
		</thead>	
		<tr>
			<th width="30%">To</th>
			<td>
				<input type="text" name="to_mail" maxlength="30" class="input_25_table" value="">
			</td>
		</tr>
		<tr>
			<th width="30%">Subject</th>
			<td>
				<input type="text" name="sendsub" maxlength="50" class="input_25_table" value="">
			</td>
		</tr>
		<tr>
			<th width="30%">Content</th>
			<td>
				<textarea name="sendmsg" maxlength="2000" cols="55" rows="8" style="font-family:'Courier New', Courier, mono; font-size:13px;background:#475A5F;color:#FFFFFF;"></textarea>
			</td>
		</tr>		
		</table>		
		
			<div id="submitBtn" class="apply_gen">
				<input class="button_gen" onclick="applyRule()" type="button" value="Send"/>
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

<!--Advanced_Feedback.asp-->
</html>


<td width="10" align="center" valign="top">&nbsp;</td>
</tr>
</table>
<div id="footer"></div>
</body>

<!--Advanced_Feedback.asp-->
</html>

