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
<title>ASUS Wireless Router <#Web_Title#> - OpenVPN Keys</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script type="text/javascript" language="JavaScript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
server1pid = '<% sysinfo("pid.vpnserver1"); %>';
server2pid = '<% sysinfo("pid.vpnserver2"); %>';
client1pid = '<% sysinfo("pid.vpnclient1"); %>';
client2pid = '<% sysinfo("pid.vpnclient2"); %>';

function initial()
{
	show_menu();
	change_page(1);
}

function applyRule(){

	showLoading();

	if (client1pid > 0) document.form.action_script.value += "restart_vpnclient1;";
	if (client2pid > 0) document.form.action_script.value += "restart_vpnclient2;";
	if (server1pid > 0) document.form.action_script.value += "restart_vpnserver1;";
	if (server2pid > 0) document.form.action_script.value += "restart_vpnserver2;";

	if (document.form.action_script.value != "") document.form.action_wait.value = "10";

	document.form.submit();
}

function change_page(page){

	for (i=1; i<5; i++) {
		showhide("page"+i, (page == i ? 1 : 0));
	}
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_OpenVPN_Keys.asp">
<input type="hidden" name="next_page" value="Advanced_OpenVPN_Keys.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
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
                <div class="formfonttitle">OpenVPN- Keys</div>
                <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">

					<tr id="server_unit">
						<th>Select OpenVPN instance to edit:</th>
						<td>
			        		<select name="displayed_page" class="input_option" onChange="change_page(this.value);">
								<option value="1">Server 1</option>
								<option value="2">Server 2</option>
								<option value="3">Client 1</option>
								<option value="4">Client 2</option>
							</select>
			   			</td>
					</tr>
				</table>

				<br>

				<table width="100%" id="page1" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Keys and certificates</td>
						</tr>
					</thead>
					<tr>
						<th>Static Key</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_server1_static" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_server1_static"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Certificate Authority</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_server1_ca" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_server1_ca"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Server Certificate</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_server1_crt" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_server1_crt"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Server Key</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_server1_key" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_server1_key"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Diffie Hellman parameters</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_server1_dh" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_server1_dh"); %></textarea>
						</td>
					</tr>
				</table>

				<table width="100%" id="page2" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Keys and certificates</td>
						</tr>
					</thead>
					<tr>
						<th>Static Key</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_server2_static" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_server2_static"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Certificate Authority</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_server2_ca" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_server2_ca"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Server Certificate</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_server2_crt" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_server2_crt"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Server Key</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_server2_key" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_server2_key"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Diffie Hellman parameters</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_server2_dh" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_server2_dh"); %></textarea>
						</td>
					</tr>
				</table>

				<table width="100%" id="page3" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Keys and certificates</td>
						</tr>
					</thead>

					<tr>
						<th>Static Key</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_client1_static" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_client1_static"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Certificate Authority</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_client1_ca" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_client1_ca"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Client Certificate</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_client1_crt" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_client1_crt"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Client Key</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_client1_key" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_client1_key"); %></textarea>
						</td>
					</tr>
				</table>


				<table width="100%" id="page4" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Keys and certificates</td>
						</tr>
					</thead>

					<tr>
						<th>Static Key</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_client2_static" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_client2_static"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Certificate Authority</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_client2_ca" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_client2_ca"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Client Certificate</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_client2_crt" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_client2_crt"); %></textarea>
						</td>
					</tr>
					<tr>
						<th>Client Key</th>
						<td>
							<textarea rows="8" class="textarea_ssh_table" name="vpn_crt_client2_key" cols="65" maxlength="4096"><% nvram_clean_get("vpn_crt_client2_key"); %></textarea>
						</td>
					</tr>

				</table>

				<div class="apply_gen">
					<input name="button" type="button" class="button_gen" onclick="applyRule();" value="<#CTL_apply#>"/>
			    </div>
			  </td></tr>
	        </tbody>
            </table>
            </form>
            </td>

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

