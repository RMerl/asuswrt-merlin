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
<title>IFTTT - Get PinCode</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script>
var PinCode = "";

function initial(){
	show_menu();

}

function getPinCode() {
	$.ajax({
		url: '/get_IFTTTPincode.cgi',
		async: false,
		dataType: 'json',
		error: function(xhr){
			setTimeout("getPinCode();", 1000);
		},
		success: function(response){
			PinCode = response.ifttt_pincode;
			document.getElementById('gen_pincode').style.display = "none";
			document.getElementById('show_pincode').style.display = "";
			showtext(document.getElementById("ifttt_pincode_t"), PinCode);
		}
	});
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="POST" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="17">&nbsp;</td>		
		<td valign="top" width="202">
		<div  id="mainMenu"></div>
		<div  id="subMenu"></div>	
		</td>
		<td valign="top">
	<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
<input type="hidden" name="current_page" value="Advanced_WWPS_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WWPS_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="3">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top" >
		
<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">

	<tbody>
	<tr>
		  <td bgcolor="#4D595D" valign="top"  >
		  <div>&nbsp;</div>
		  <div class="formfonttitle">IFTTT - Get PinCode</div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0"  class="FormTable">



			<tr>
				<th>PinCode</th>
				<td>
					<div style="margin-left:-10px">
						<table ><tr>
							<td style="border:0px;h" >
								<div class="devicepin" style="color:#FFF;" id="wps_config_td"></div>
							</td>
							<td style="border:0px" id="gen_pincode">
								<input class="button_gen" type="button" onClick="getPinCode();" id="Reset_OOB" name="Reset_OOB" value="Get PinCode" style="padding:0 0.3em 0 0.3em;" >
							</td>
							<td style="border:0px;display:none" id="show_pincode">
								<p name="ifttt_pincode_t" id="ifttt_pincode_t" style="line-height:33px"></p>
							</td>							
						</tr></table>
					</div>
				</td>
			</tr>

		</table>


	  </td>
	</tr>
</tbody>	
</table>		
					
		</td>
</form>
		

        </tr>
      </table>	
		<!--===================================Ending of Main Content===========================================-->		
	</td>
		
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
