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
<title><#Web_Title#> - Switch Control</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="other.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>

<script>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var level2CTF_supprot = ('<% nvram_get("ctf_fa_mode"); %>' == '') ? false : true;

function initial(){
	show_menu();

	if(level2CTF_supprot){
		document.form.ctf_level.length = 0;
		add_option(document.form.ctf_level, "<#WLANConfig11b_WirelessCtrl_buttonname#>", 0, getCtfLevel(0));
		add_option(document.form.ctf_level, "Level 1 CTF", 1, getCtfLevel(1));
		add_option(document.form.ctf_level, "Level 2 CTF", 2, getCtfLevel(2));
	}
}

/*
for RT-AC87U:
		 ctf_disable_force   ctf_fa_mode_close
Level 1          0                   1
Level 2          0                   0 
Disable          1                   1

for Others:
         ctf_disable_force   ctf_fa_mode
Level 1          0                0
Level 2          0                2 
Disable          1                0
*/

function getCtfLevel(val){
	var curVal;

	if(based_modelid == 'RT-AC87U'){ // MODELDEP: RT-AC87U
		if(document.form.ctf_disable_force.value == 0 && document.form.ctf_fa_mode_close.value == 1)
			curVal = 1;
		else if(document.form.ctf_disable_force.value == 0 && document.form.ctf_fa_mode_close.value == 0)
			curVal = 2;
		else
			curVal = 0;
	}
	else{
		if(document.form.ctf_disable_force.value == 0 && document.form.ctf_fa_mode.value == 0)
			curVal = 1;
		else if(document.form.ctf_disable_force.value == 0 && document.form.ctf_fa_mode.value == 2)
			curVal = 2;
		else
			curVal = 0;		
	}

	if(curVal == val)
		return true;
	else
		return false;
}

function applyRule(){
	if(based_modelid == 'RT-AC87U'){ // MODELDEP: RT-AC87U
		document.form.ctf_fa_mode.disabled = true;

		if(document.form.ctf_level.value == 1){
			document.form.ctf_disable_force.value = 0;
			document.form.ctf_fa_mode_close.value = 1;
		}
		else if(document.form.ctf_level.value == 2){
			document.form.ctf_disable_force.value = 0;
			document.form.ctf_fa_mode_close.value = 0;
		}
		else{
			document.form.ctf_disable_force.value = 1;
			document.form.ctf_fa_mode_close.value = 1;
		}
	}
	else{
		if(level2CTF_supprot){		// for RT-AC68U, RT-AC56U support Level 2 CTF
			if(document.form.wan_proto.value != "dhcp" && document.form.wan_proto.value != "static"){
				if(document.form.ctf_level.value == 2){
					alert("Can not use Level 2 CTF if WAN type is PPPoE、PPTP or L2TP");
					return false;
				}		
			}		
		}

		document.form.ctf_fa_mode_close.disabled = true;
		if(document.form.ctf_level.value == 1){
			document.form.ctf_disable_force.value = 0;
			document.form.ctf_fa_mode.value = 0;
		}
		else if(document.form.ctf_level.value == 2){
			document.form.ctf_disable_force.value = 0;
			document.form.ctf_fa_mode.value = 2;
		}
		else{
			document.form.ctf_disable_force.value = 1;
			document.form.ctf_fa_mode.value = 0;
		}
	}

	showLoading();
	document.form.submit();	
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
		  <div class="drImg"><img src="images/alertImg.png"></div>
			<div style="height:70px;"></div>
		</td>
		</tr>
	</table>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_SwitchCtrl_Content.asp">
<input type="hidden" name="next_page" value="Advanced_SwitchCtrl_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="reboot">
<input type="hidden" name="action_wait" value="60">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="ctf_fa_mode_close" value="<% nvram_get("ctf_fa_mode_close"); %>">
<input type="hidden" name="ctf_fa_mode" value="<% nvram_get("ctf_fa_mode"); %>">
<input type="hidden" name="ctf_disable_force" value="<% nvram_get("ctf_disable_force"); %>">
<input type="hidden" name="wan_proto" value="<% nvram_get("wan_proto"); %>" disabled>

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td width="17">&nbsp;</td>
	
	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>
	
    <td valign="top">
	<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td align="left" valign="top">
  <table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
	<tbody>
	<tr>
		  <td bgcolor="#4D595D" valign="top">
		  <div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_2#> - Switch Control</div>
      <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
      <div class="formfontdesc"><#SwitchCtrl_desc#></div>
		  
		  <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
      <tr>
      <th><#jumbo_frame#></th>
          <td>
						<select name="jumbo_frame_enable" class="input_option">
							<option class="content_input_fd" value="0" <% nvram_match("jumbo_frame_enable", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
							<option class="content_input_fd" value="1" <% nvram_match("jumbo_frame_enable", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
						</select>
          </td>
      </tr>
      <tr>
      <th><#NAT_Acceleration#></th>
          <td>
						<select name="ctf_level" class="input_option">
							<option class="content_input_fd" value="0" <% nvram_match("ctf_disable_force", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
							<option class="content_input_fd" value="1" <% nvram_match("ctf_disable_force", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
						</select>
          </td>
      </tr>     
	    <tr style="display:none">
	      <th>Enable GRO(Generic Receive Offload)</th>
 	          <td>
	              <input type="radio" name="gro_disable_force" value="0" <% nvram_match("gro_disable_force", "0", "checked"); %>><#checkbox_Yes#>
	              <input type="radio" name="gro_disable_force" value="1" <% nvram_match("gro_disable_force", "1", "checked"); %>><#checkbox_No#>
 	          </td>
	      </tr>
      <tr>
          <th>Spanning-Tree Protocol</th>
              <td>
				                <select name="lan_stp" class="input_option">
						        <option class="content_input_fd" value="0" <% nvram_match("lan_stp", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
						        <option class="content_input_fd" value="1" <% nvram_match("lan_stp", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
                                                </select>
              </td>
      </tr>

			</table>	

		<div class="apply_gen">
			<input class="button_gen" onclick="applyRule()" type="button" value="<#CTL_apply#>"/>
		</div>
		
	  </td>
	</tr>

	</tbody>	
  </table>		
					
		</td>
	</form>					
				</tr>
			</table>				
			<!--===================================End of Main Content===========================================-->
</td>

    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
