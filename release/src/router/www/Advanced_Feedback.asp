﻿<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>

<!--Advanced_Feedback.asp-->

<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#menu_feedback#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<!--link rel="stylesheet" type="text/css" href="pwdmeter.css"-->
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>

<script>
function initial(){
	show_menu();
}

function redirect(){
	document.location.href = "Feedback_Info.asp";
}

function applyRule(){
	if(link_status == "2" && link_auxstatus == "0"){

		/*if(document.form.feedbackresponse.value == "3"){
				alert("Feedback report daily maximum(10) send limit reached.");
				return false;
		}*/
		if(document.form.attach_syslog.checked == true)
			document.form.PM_attach_syslog.value = 1;
		else
			document.form.PM_attach_syslog.value = 0;
		if(document.form.attach_cfgfile.checked == true)
			document.form.PM_attach_cfgfile.value = 1;
		else
			document.form.PM_attach_cfgfile.value = 0;
		if(document.form.attach_iptables.checked == true)
			document.form.PM_attach_iptables.value = 1;
		else
			document.form.PM_attach_iptables.value = 0;		
		showLoading(20);		
		document.form.submit();
	}
	else{
		alert("<#USB_Application_No_Internet#>");
		return false;
	}
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
<input type="hidden" name="current_page" value="Advanced_Feedback.asp">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_DSLsendmail">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="PM_attach_syslog" value="">
<input type="hidden" name="PM_attach_cfgfile" value="">
<input type="hidden" name="PM_attach_iptables" value="">	
<input type="hidden" name="feedbackresponse" value="<% nvram_get("feedbackresponse"); %>">
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
<div class="formfonttitle"><#menu5_6#> - <#menu_feedback#></div>
<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
<div class="formfontdesc"><#Feedback_desc#></div>
<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
<tr>
<th width="30%"><#feedback_country#> *</th>
<td>
	<input type="text" name="fb_country" maxlength="30" class="input_25_table" value="">
</td>
</tr>
<tr>
<th><#feedback_isp#> *</th>
<td>
	<input type="text" name="fb_ISP" maxlength="40" class="input_25_table" value="">
</td>
</tr>
<tr>
<th>Name of the Subscribed Plan/Service/Package *</th>
<td>
	<input type="text" name="fb_Subscribed_Info" maxlength="50" class="input_25_table" value="">
</td>
</tr>
<tr>
<th><#feedback_email#> *</th>
<td>
	<input type="text" name="fb_email" maxlength="50" class="input_25_table" value="">
</td>
</tr>

<tr>
<th>Extra information for debugging *</th>
<td>
	<input type="checkbox" class="input" name="attach_syslog">Syslog&nbsp;&nbsp;&nbsp;
	<input type="checkbox" class="input" name="attach_cfgfile">Setting file&nbsp;&nbsp;&nbsp;
	<input type="checkbox" class="input" name="attach_iptables">Iptable setting
</td>
</tr>

<tr>
<th><#feedback_connection_type#></th>
<td>
	<select id="" class="input_option" name="fb_availability">
		<option value="Not_selected"><#Select_menu_default#> ...</option>
		<option value="Stable_connection"><#feedback_stable#></option>
		<option value="Occasional_interruptions"><#feedback_Occasion_interrupt#></option>
		<option value="Frequent_interruptions"><#feedback_Frequent_interrupt#></option>
		<option value="Unavailable"><#feedback_usaul_unavailable#></option>
	</select>
</td>
</tr>
<tr>
	<th>
		<#feedback_comments#> *
	</th>
	<td>
		<textarea name="fb_comment" maxlength="2000" cols="35" rows="5" style="font-family:'Courier New', Courier, mono; font-size:13px;background:#475A5F;color:#FFFFFF;"></textarea>
		<br> <#feedback_optional#>
	</td>
</tr>
</table>
<div class="apply_gen">
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

