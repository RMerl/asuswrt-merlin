<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7">
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>Network Tools - Netstat</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.js"></script>
<script>
function onSubmitCtrl(o, s) {
	document.form.action_mode.value = s;
	updateOptions();
}

function updateOptions(){
	if (document.form.NetOption.value == "NAT")
		document.form.SystemCmd.value = "netstat-nat.sh";
	else
		document.form.SystemCmd.value = "netstat " + document.form.NetOption.value;
	
	document.form.submit();
	document.getElementById("cmdBtn").disabled = true;
	document.getElementById("cmdBtn").style.color = "#666";
	document.getElementById("loadingIcon").style.display = "";
	setTimeout("checkCmdRet();", 500);
}

var $j = jQuery.noConflict();
var _responseLen;
var noChange = 0;
function checkCmdRet(){
	if (document.form.NetOption.value == "NAT")
		waitLimit = 90;
	else
		waitLimit = 10;

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
				return false;
			}

			if(_responseLen == response.length)
				noChange++;
			else
				noChange = 0;

			if(noChange > waitLimit){
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
<body onload="show_menu();load_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="GET" name="form" action="/apply.cgi" target="hidden_frame"> 
<input type="hidden" name="current_page" value="Main_Netstat_Content.asp">
<input type="hidden" name="next_page" value="Main_Netstat_Content.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="SystemCmd" onkeydown="onSubmitCtrl(this, ' Refresh ')" value="">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

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
						<table width="760px" border="0" cellpadding="5" cellspacing="0" bordercolor="#6b8fa3"  class="FormTitle" id="FormTitle">		
							<tr>
								<td bgcolor="#4D595D" colspan="3" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle">Network Tools - Netstat</div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div class="formfontdesc">Display networking information</div>
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
										<tr>
											<th width="20%">Option</th>
											<td>
												<select id="NetOption" class="input_option" name="NetOption">
													<option value="-a">Display all sockets</option>
													<option value="-t">TCP sockets</option>
													<option value="-u">UDP sockets</option>
													<option value="-w">RAW sockets</option>
													<option value="-x">UNIX sockets</option>
													<option value="-l">Display listening server sockets</option>
													<option value="-r">Display routing table</option>
													<option value="NAT">NAT Connections</option>
 												</select>
											</td>										
										</tr>
									</table>

									<div class="apply_gen">
										<input class="button_gen" id="cmdBtn" onClick="onSubmitCtrl(this, ' Refresh ')" type="button" value="Netstat" name="action">
										<img id="loadingIcon" style="display:none;" src="/images/InternetScan.gif"></span>
									</div>

									<div style="margin-top:8px" id="logArea">
										<textarea cols="63" rows="27" wrap="off" readonly="readonly" id="textarea" style="width:99%;font-family:Courier New, Courier, mono; font-size:11px;background:#475A5F;color:#FFFFFF;"><% nvram_dump("syscmd.log","syscmd.sh"); %></textarea>
									</div>
								</td>
							</tr>
						</table>
					</td>
				</tr>
			</table>
			<!--===================================Ending of Main Content===========================================-->
		</td>
		<td width="10" align="center" valign="top"></td>
	</tr>
</table>
</form>

<div id="footer"></div>
</body>
<script type="text/javascript">
<!--[if !IE]>-->
	jQuery.noConflict();
	(function($){
		var textArea = document.getElementById('textarea');
		textArea.scrollTop = textArea.scrollHeight;
	})(jQuery);
<!--<![endif]-->
</script>
</html>
