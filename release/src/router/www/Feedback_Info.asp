<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>

<!--Feedback_Info.asp-->
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
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
<script>
var fb_state = "<% nvram_get("fb_state"); %>";
	
function initial(){
	show_menu();
	check_info();
}

function check_info(){
	//0:initial  1:Success  2.Failed  3.Limit?  4.dla
	if(wan_diag_state == "4"){	
		document.getElementById("fb_send_debug_log").style.display = "";
		document.getElementById("Email_subject").href = "mailto:xdsl_feedback@asus.com?Subject="+based_modelid;
		get_debug_log_info();
	}
	else{
		if(dsl_support){
			document.getElementById("fb_success_dsl_0").style.display = "";
			document.getElementById("fb_success_dsl_1").style.display = "";
		}
		else{
			document.getElementById("fb_success_router_0").style.display = "";
			document.getElementById("fb_success_router_1").style.display = "";
		}
	} 	

	if(dsl_support && fb_state == "2"){
		document.getElementById("fb_fail_dsl").style.display = "";
		document.getElementById("fb_fail_textarea").style.display = "";
	}
	else if(fb_state == "2"){
		document.getElementById("fb_fail_router").style.display = "";
		document.getElementById("fb_fail_textarea").style.display = "";
	}
}

function get_debug_log_info(){

	var desc = "DSL DIAGNOSTIC LOG\n";
	desc += "----------------------------------------------------------------------\n";

	desc += "Model: "+based_modelid+"\n";
	desc += "Firmware Version: <% nvram_get("firmver"); %>.<% nvram_get("buildno"); %>_<% nvram_get("extendno"); %>\n";
	desc += "Inner Version: <% nvram_get("innerver"); %>\n";
	desc += "DSL Firmware Version: <% nvram_get("dsllog_fwver"); %>\n";
	desc += "DSL Driver Version:  <% nvram_get("dsllog_drvver"); %>\n\n";

	desc += "PIN Code: <% nvram_get("secret_code"); %>\n";
	desc += "MAC Address: <% nvram_get("lan_hwaddr"); %>\n\n";

	desc += "Diagnostic debug log capture duration: <% nvram_get("dslx_diag_duration"); %>\n";
	desc += "DSL connection: <% nvram_get("fb_availability"); %>\n";

	document.uiForm.fb_send_debug_log_content.value = desc;
	
}

function redirect(){
	document.location.href = "Advanced_Feedback.asp";
}

function reset_diag_state(){	
	document.diagform.dslx_diag_state.value = 0;	
	document.diagform.submit();
	setTimeout("location.href='TCC.log.gz';", 300);
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
<form method="post" name="diagform" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="dslx_diag_state" value="<% nvram_get("dslx_diag_state"); %>">
</form>
<FORM METHOD="POST" ACTION="" name="uiForm" target="hidden_frame">
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

<div id="fb_success_dsl_0" style="display:none;">
	<br>
	<br>
	<div class="feedback_info_0"><#feedback_thanks#></div>
	<br>
</div>

<div id="fb_success_router_0" style="display:none;">
        <br>
        <br>
        <div class="feedback_info_0"><#feedback_thanks#></div>
        <br>
</div>

<div id="fb_fail_dsl" style="display:none;" class="feedback_info_1">
	<#feedback_fail0#>
	<br>
	<#feedback_fail1#> : ( <a href="mailto:xdsl_feedback@asus.com?Subject=<%nvram_get("productid");%>" target="_top" style="color:#FFCC00;">xdsl_feedback@asus.com </a>) <#feedback_fail2#>
	<br>
</div>

<div id="fb_fail_router" style="display:none;" class="feedback_info_1">
	<#feedback_fail0#>
	<br>
	<#feedback_fail1#> : ( <a href="mailto:router_feedback@asus.com?Subject=<%nvram_get("productid");%>" target="_top" style="color:#FFCC00;">router_feedback@asus.com </a>) <#feedback_fail2#>
	<br>
</div>

<div id="fb_fail_textarea" style="display:none;">
	<textarea name="fb_fail_content" cols="70" rows="10" style="width:99%; font-family:'Courier New', Courier, mono; font-size:13px;background:#475A5F;color:#FFFFFF;" readonly><% nvram_dump("fb_fail_content", ""); %></textarea>
	<br>
</div>

<div id="fb_success_dsl_1" style="display:none;">
	<br>
	<div class="feedback_info_1">We are working hard to improve the firmware of <#Web_Title2#> and your feedback is very important to us. We will use your feedbacks and comments to strive to improve your ASUS experience.</div>
	<br>	
</div>

<div id="fb_success_router_1" style="display:none;">	
	<br>
	<div class="feedback_info_1"> 
	<#feedback_success_rt#>
	</div>
	<br>
	<br>
</div>	

<div id="fb_send_debug_log" style="display:none;">
	<br>
	<br>
	<div class="feedback_info_0">Diagnostic DSL debug log capture completed.</div>
	<br>
	<br>
	<div class="feedback_info_1">Please send us an email directly ( <a id="Email_subject" href="" target="_top" style="color:#FFCC00;">xdsl_feedback@asus.com</a> ). Simply copy from following text area and paste as mail content. <br><div onClick="reset_diag_state();" style="text-decoration: underline; font-family:Lucida Console; cursor:pointer;">Click here to download the debug log and add as mail attachment.</div></div>
	<br>
	<textarea name="fb_send_debug_log_content" cols="70" rows="15" style="width:99%; font-family:'Courier New', Courier, mono; font-size:13px;background:#475A5F;color:#FFFFFF;" readonly></textarea>
	<br>	
</div>

<div id="fb_deny" style="display:none;">
</div>	<div class="apply_gen">
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

