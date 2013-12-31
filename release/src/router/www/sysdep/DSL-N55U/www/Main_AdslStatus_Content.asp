<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7">
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<meta HTTP-EQUIV="refresh" CONTENT="10">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#menu_dsl_log#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
var sync_status = "<% nvram_get("dsltmp_adslsyncsts"); %>";
var adsl_timestamp = "<% nvram_get("adsl_timestamp"); %>";
var adsl_boottime = boottime - parseInt(adsl_timestamp);

function initial(){
	replace_Content("Modulation : 0", "Modulation : T1.413");
	replace_Content("Modulation : 1", "Modulation : G.lite");
	replace_Content("Modulation : 2", "Modulation : G.Dmt");
	replace_Content("Modulation : 3", "Modulation : ADSL2");
	replace_Content("Modulation : 4", "Modulation : ADSL2+");
	replace_Content("Modulation : 5", "Modulation : Multiple Mode");

	replace_Content("Line State : 0", "Line State : down");
	replace_Content("Line State : 1", "Line State : wait for init");
	replace_Content("Line State : 2", "Line State : init");
	replace_Content("Line State : 3", "Line State : up");

	replace_ContentAnnex("Annex Mode : Annex A ", "Annex Mode : Annex A/L");
}

function showadslbootTime(){
	if((adsl_timestamp != "") && (sync_status == "up"))
	{
		Days = Math.floor(adsl_boottime / (60*60*24));
		Hours = Math.floor((adsl_boottime / 3600) % 24);
		Minutes = Math.floor(adsl_boottime % 3600 / 60);
		Seconds = Math.floor(adsl_boottime % 60);

		$("boot_days").innerHTML = Days;
		$("boot_hours").innerHTML = Hours;
		$("boot_minutes").innerHTML = Minutes;
		$("boot_seconds").innerHTML = Seconds;
		adsl_boottime += 1;
		setTimeout("showadslbootTime()", 1000);
	}
	else
	{
		$("boot_days").innerHTML = "0";
		$("boot_hours").innerHTML = "0";
		$("boot_minutes").innerHTML = "0";
		$("boot_seconds").innerHTML = "0";
	}
}

function replace_Content(oldMod, newMod){
	var tb = document.form2.textarea1;
	if(tb.value.indexOf(oldMod)!= -1)
	{
		var startString = tb.value.substr(0, tb.value.indexOf(oldMod));
		var endString = tb.value.substring(tb.value.indexOf(oldMod)+14);
		tb.value = startString+newMod+endString;
	}
}

function replace_ContentAnnex(oldMod, newMod){
	var tb = document.form2.textarea1;
	if(tb.value.indexOf(oldMod)!= -1)
	{
		var startString = tb.value.substr(0, tb.value.indexOf(oldMod));
		var endString = tb.value.substring(tb.value.indexOf(oldMod)+21);
		tb.value = startString+newMod+endString;
	}
}
</script>
</head>

<body onload="show_menu();showadslbootTime();initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="apply.cgi" target="hidden_frame">
<input type="hidden" name="current_page" value="Main_AdslStatus_Content.asp">
<input type="hidden" name="next_page" value="Main_AdslStatus_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
</form>
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
		  			<div class="formfonttitle"><#System_Log#> - <#menu_dsl_log#></div>
		  			<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  			<div class="formfontdesc"><#GeneralLog_title#></div>
						<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
							<tr>
								<th width="20%"><#adsl_fw_ver_itemname#></th>
								<td>
									<% nvram_dump("adsl/tc_fw_ver.txt",""); %>
								</td>
							</tr>
							<tr>
								<th width="20%">RAS</th>
								<td>
									<% nvram_dump("adsl/tc_ras_ver.txt",""); %>
								</td>
							</tr>
							<tr>
								<th width="20%"><#adsl_link_sts_itemname#></th>
								<td>
									<% nvram_get("dsltmp_adslsyncsts"); %>
								</td>
							</tr>
							<tr>
								<th width="20%">ADSL <#General_x_SystemUpTime_itemname#></th>
								<td>
									<span id="boot_days"></span> <#Day#> <span id="boot_hours"></span> <#Hour#> <span id="boot_minutes"></span> <#Minute#> <span id="boot_seconds"></span> <#Second#>
								</td>
							</tr>
						</table>
					<form method="post" name="form2" action="dsllog.cgi">
						<div style="margin-top:8px">
							<textarea name="textarea1" cols="63" rows="27" wrap="off" readonly="readonly" id="textarea" style="width:99%; font-family:'Courier New', Courier, mono; font-size:11px;background:#475A5F;color:#FFFFFF;"><% nvram_dump("adsl/adsllog.log",""); %></textarea>
						</div>
					</td>
			</tr>

			<tr class="apply_gen" valign="top">
				<td width="20%" align="center">
						<input type="submit" onClick="onSubmitCtrl(this, ' Save ')" value="<#CTL_onlysave#>" class="button_gen">
					</form>
				</td>
				<td width="40%" align="left" >
					<form method="post" name="form3" action="apply.cgi">
						<input type="hidden" name="current_page" value="Main_AdslStatus_Content.asp">
						<input type="hidden" name="action_mode" value=" Refresh ">
						<input type="button" onClick="location.href=location.href" value="<#CTL_refresh#>" class="button_gen">						
					</form>
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
<div id="footer"></div>
		</form>
</body>
</html>
