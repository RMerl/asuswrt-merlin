<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>ASUS Wireless Router <#Web_Title#> - Run Command</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script type="text/javascript" language="JavaScript" src="/detect.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

function onSubmitCtrl(o, s) {
	document.form.action_mode.value = s;
	document.getElementById("loadingIcon").style.display = "";
	setTimeout("checkCmdRet();", 500);
}

var $j = jQuery.noConflict();
var _responseLen;
var noChange = 0;
function checkCmdRet(){
	$j.ajax({
		url: '/cmdRet_check.htm',
		dataType: 'html',
		
		error: function(xhr){
			setTimeout("checkCmdRet();", 1000);
		},
		success: function(response){
			if(response.search("XU6J03M6") != -1){
				document.getElementById("loadingIcon").style.display = "none";
				document.getElementById("cmdBtn").disabled = false;
				document.getElementById("cmdBtn").style.color = "#FFF";
				document.getElementById("textarea").value = response.replace("XU6J03M6", " ");
				document.form.SystemCmd.value = "";
				return false;
			}

			if(_responseLen == response.length)
				noChange++;
			else
				noChange = 0;

			if(noChange > 30){
				document.getElementById("loadingIcon").style.display = "none";
				document.getElementById("cmdBtn").disabled = false;
				document.getElementById("cmdBtn").style.color = "#FFF";
				setTimeout("checkCmdRet();", 1000);
			}
			else{
				document.getElementById("cmdBtn").disabled = true;
				document.getElementById("cmdBtn").style.color = "#666";
				document.getElementById("loadingIcon").style.display = "";
				setTimeout("checkCmdRet();", 1000);
			}

			document.getElementById("textarea").value = response;
			_responseLen = response.length;
		}
	});
}
</script>
</head>

<body onload="show_menu();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="GET" name="form" id="ruleForm" action="/apply.cgi" target="hidden_frame">
<input type="hidden" name="current_page" value="Tools_RunCmd.asp">
<input type="hidden" name="next_page" value="Tools_RunCmd.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="SystemCmd" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
    <td width="17">&nbsp;</td>
    <td valign="top" width="202">
      <div id="mainMenu"></div>
      <div id="subMenu"></div></td>
    <td valign="top">
      <div id="tabMenu" class="submenuBlock"></div>

      <!--===================================Beginning of Main Content===========================================-->
      <table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
        <tr>
          <td valign="top">
            <table width="760px" border="0" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTitle" id="FormTitle">
              <tbody>
              <tr bgcolor="#4D595D">
                <td valign="top">
                  <div>&nbsp;</div>
                  <div class="formfonttitle">Tools - Run System Command</div>
                  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
				  <div class="formfontdesc">This page allows you to run Linux shell commands.<br>Avoid running any command which never return (such as 'top' or 'ping').</div>
				    <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					  <tbody>
                        <tr>
                          <td>
                            <input class="input_option" type="text" maxlength="255" size="60%" name="SystemCmd" value="">
                            <input class="button_gen" id="cmdBtn" onClick="onSubmitCtrl(this, ' Refresh ')" type="submit" value="<#CTL_refresh#>" name="action">
                            <img id="loadingIcon" style="display:none;" src="/images/InternetScan.gif"></span>
                          </td>
                        </tr>
                        <tr>
                          <td>
                            <textarea cols="80" rows="27" wrap="off" readonly="readonly" id="textarea" style="width:99%;font-family:Courier New, Courier, mono; font-size:11px;background:#475A5F;color:#FFFFFF;"><% nvram_dump("syscmd.log","syscmd.sh"); %></textarea>
                          </td>
                        </tr>
                      </tbody>
				   </table>
                 </td>
              </tr>
            </table>
          </td>
        </tr>
      </table>
     <!--===================================Ending of Main Content===========================================-->
    </td>
    <td width="10" align="center" valign="top">&nbsp;</td>
  </tr>
</table>

</form>
<div id="footer"></div>
</body>
</html>

