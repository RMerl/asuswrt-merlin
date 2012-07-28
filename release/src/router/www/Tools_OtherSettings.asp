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
<title>ASUS Wireless Router <#Web_Title#> - Other Settings</title>
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

function initial()
{
	show_menu();
	initConntrackValues()
	set_rstats_location();
	hide_rstats_storage(document.form.rstats_location.value);
}

function set_rstats_location()
{
	rstats_loc = '<% nvram_get("rstats_path"); %>';

	if (rstats_loc === "")
	{
		document.form.rstats_location.value = "0";
	}
	else if (rstats_loc === "*nvram")
	{
		document.form.rstats_location.value = "2";
	}
	else
	{
		document.form.rstats_location.value = "1";
	}

}

function hide_rstats_storage(_value){

        $("rstats_new_tr").style.display = (_value == "1" || _value == "2") ? "" : "none";
        $("rstats_stime_tr").style.display = (_value == "1" || _value == "2") ? "" : "none";
        $("rstats_path_tr").style.display = (_value == "1") ? "" : "none";
}

function initConntrackValues(){

	tcp_array = document.form.ct_tcp_timeout.value.split(" ");

	document.form.tcp_established.value = tcp_array[1];
	document.form.tcp_syn_sent.value = tcp_array[2];
	document.form.tcp_syn_recv.value = tcp_array[3];
	document.form.tcp_fin_wait.value = tcp_array[4];
	document.form.tcp_time_wait.value = tcp_array[5];
	document.form.tcp_close.value = tcp_array[6];
	document.form.tcp_close_wait.value = tcp_array[7];
	document.form.tcp_last_ack.value = tcp_array[8];

	udp_array = document.form.ct_udp_timeout.value.split(" ");

	document.form.udp_unreplied.value = udp_array[0];
	document.form.udp_assured.value = udp_array[1];

}


function applyRule(){

	showLoading();

	if (document.form.rstats_location.value == "2")
	{
		document.form.rstats_path.value = "*nvram";
	} else if (document.form.rstats_location.value == "0")
	{
		document.form.rstats_path.value = "";
	}


	document.form.ct_tcp_timeout.value = "0 "+
		document.form.tcp_established.value +" " +
		document.form.tcp_syn_sent.value +" " +
		document.form.tcp_syn_recv.value +" " +
		document.form.tcp_fin_wait.value +" " +
		document.form.tcp_time_wait.value +" " +
		document.form.tcp_close.value +" " +
		document.form.tcp_close_wait.value +" " +
		document.form.tcp_last_ack.value +" 0";

	document.form.ct_udp_timeout.value = document.form.udp_unreplied.value + " "+document.form.udp_assured.value;

	if (document.form.usb_idle_timeout.value != <% nvram_get("usb_idle_timeout"); %>)
		document.form.action_script.value += ";restart_sdidle";

	document.form.submit();
}


function done_validating(action){
        refreshpage();
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Tools_OtherSettings.asp">
<input type="hidden" name="next_page" value="Tools_OtherSettings.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_rstats;restart_conntrack">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="SystemCmd" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="ct_tcp_timeout" value="<% nvram_get("ct_tcp_timeout"); %>">
<input type="hidden" name="ct_udp_timeout" value="<% nvram_get("ct_udp_timeout"); %>">



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
                <div class="formfonttitle">Tools - Other Settings</div>
                <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Traffic Monitoring</td>
						</tr>
					</thead>
					<tr>
						<th>Traffic history location</th>
			        	<td>
			       			<select name="rstats_location" class="input_option" onchange="hide_rstats_storage(this.value);">
								<option value="0">RAM (Default)</option>
								<option value="1">Custom location</option>
								<option value="2">NVRAM</option>
							</select>
			   			</td>
					</tr>

					<tr id="rstats_stime_tr">
						<th>Save frequency:</th>
						<td>
							<select name="rstats_stime" class="input_option" >
								<option value="1" <% nvram_match("rstats_stime", "1","selected"); %>>Every 1 hour</option>
			           				<option value="6" <% nvram_match("rstats_stime", "6","selected"); %>>Every 6 hours</option>
			           				<option value="12" <% nvram_match("rstats_stime", "12","selected"); %>>Every 12 hours</option>
			           				<option value="24" <% nvram_match("rstats_stime", "24","selected"); %>>Every 1 day</option>
			           				<option value="72" <% nvram_match("rstats_stime", "72","selected"); %>>Every 3 days</option>
			           				<option value="168" <% nvram_match("rstats_stime", "168","selected"); %>>Every 1 week</option>
							</select>
						</td>
					</tr>
					<tr id="rstats_path_tr">
						<th>Save history location:<br><i>Directory must end with a '/'.</i></th>
						<td><input type="text" id="rstats_path" size=32 maxlength=90 name="rstats_path" class="input_32_table" value="<% nvram_get("rstats_path"); %>"></td>
					</tr>
					<tr id="rstats_new_tr">
			        		<th>Create or reset data files:<br><i>Enable if using a new location</i></th>
				        	<td>
	        		       			<input type="radio" name="rstats_new" class="input" value="1" <% nvram_match_x("", "rstats_new", "1", "checked"); %>><#checkbox_Yes#>
			        		        <input type="radio" name="rstats_new" class="input" value="0" <% nvram_match_x("", "rstats_new", "0", "checked"); %>><#checkbox_No#>
			       	        	</td>
	        			</tr>
					<tr>
					        <th>Starting day of monthly cycle</th>
				        	<td><input type="text" maxlength="2" class="input_3_table" name="rstats_offset" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 31)" value="<% nvram_get("rstats_offset"); %>"></td>
				        </tr>
				</table>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
                                        <thead>
						<tr>
							<td colspan="2">Miscellaneous Options</td>
						</tr>
					</thead>

					<tr>
						<th>Resolve IPs on active connections list:<br><i>Can considerably slow down the display</i></th>
						<td>
							<input type="radio" name="webui_resolve_conn" class="input" value="1" <% nvram_match_x("", "webui_resolve_conn", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="webui_resolve_conn" class="input" value="0" <% nvram_match_x("", "webui_resolve_conn", "0", "checked"); %>><#checkbox_No#>
						</td>
	                                </tr>

					<tr>
						<th>Disk spindown idle time (in seconds)</th>
						<td>
							<input type="text" maxlength="6" class="input_12_table"name="usb_idle_timeout" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 0, 43200)"value="<% nvram_get("usb_idle_timeout"); %>">
						</td>
					</tr>
				</table>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">TCP/IP settings</td>
						</tr>
					</thead>
 					<tr>
						<th>TCP connections limit</th>
						<td>
							<input type="text" maxlength="6" class="input_12_table" name="ct_max" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 256, 300000)" value="<% nvram_get("ct_max"); %>">
						</td>
						</tr>

						<tr>
							<th>TCP Timeout: Established</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="tcp_established" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 1200</span>
							</td>
                        
						</tr>

 						<tr>
							<th>TCP Timeout: syn_sent</th>
							<td>
 								<input type="text" maxlength="5" class="input_6_table" name="tcp_syn_sent" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 120</span>
							</td>
						</tr>

						<tr>
							<th>TCP Timeout: syn_recv</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="tcp_syn_recv" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 60</span>
							</td>
						</tr>

						<tr>
							<th>TCP Timeout: fin_wait</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="tcp_fin_wait" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 120</span> 
							</td>
						</tr>

						<tr>
							<th>TCP Timeout: time_wait</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="tcp_time_wait" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 120</span> 
							</td>
						</tr>

						<tr>
							<th>TCP Timeout: close</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="tcp_close" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 10</span> 
							</td>
						</tr>

						<tr>
							<th>TCP Timeout: close_wait</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="tcp_close_wait" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 60</span> 
							</td>
						</tr>

						<tr>
							<th>TCP Timeout: last_ack</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="tcp_last_ack" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 30</span> 
							</td>
						</tr>

						<tr>
							<th>UDP Timeout: Assured</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="udp_assured" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 86400)" value="">
								<span>Default: 180</span>
							</td>
						</tr>

						<tr>
							<th>UDP Timeout: Unreplied</th>
							<td>
								<input type="text" maxlength="5" class="input_6_table" name="udp_unreplied" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1,86400)" value="">
								<span>Default: 30</span> 
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

