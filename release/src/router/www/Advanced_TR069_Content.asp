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
<title><#Web_Title#> - TR-069 Client</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" language="JavaScript" src="/help.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>

<style type="text/css">
.contentM_qis{
	width:740px;	
	margin-top:110px;
	margin-left:380px;
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:200;
	background-color:#2B373B;
	display:none;
	/*behavior: url(/PIE.htc);*/
}
</style>

<script>


window.onresize = cal_panel_block;
var tr_enable = '<% nvram_get("tr_enable"); %>';
var tr_acs_url = '<% nvram_get("tr_acs_url"); %>';

var jffs2_support = isSupport("jffs2");

<% login_state_hook(); %>

function initial(){
	show_menu();
	//load_body();

	//if(based_modelid != "RT-AC68U" && based_modelid != "RT-AC66U" && based_modelid != "RT-N66U"
	// && based_modelid != "RT-N18U")
	if(!jffs2_support)
		showhide("cert_text", false);
}

function applyRule(){
	if (document.form.tr_enable[0].checked) {
		if (document.form.tr_acs_url.value == "")
			document.form.tr_discovery.value = "1";
		else if (document.form.tr_acs_url.value != tr_acs_url)
			document.form.tr_discovery.value = "0";
		document.form.action_script.value = "restart_tr";
		if (document.form.tr_discovery.value != "0")
			document.form.action_script.value += ";restart_wan_if";
	}
	showLoading();
	document.form.submit();	
}

function done_validating(action){
	refreshpage();
}

function set_cert(){
	cal_panel_block();
	$("#cert_panel").fadeIn(300);	
}

function cancel_cert_panel(){
	this.FromObject ="0";
	$("#cert_panel").fadeOut(300);	
	//setTimeout("document.getElementById('edit_tr_ca_cert').value = '<% nvram_clean_get("tr_ca_cert"); %>';", 300);
	//setTimeout("document.getElementById('edit_tr_client_cert').value = '<% nvram_clean_get("tr_client_cert"); %>';", 300);
	//setTimeout("document.getElementById('edit_tr_client_key').value = '<% nvram_clean_get("tr_client_key"); %>';", 300);
	setTimeout("document.getElementById('edit_tr_ca_cert').value = decodeURIComponent('<% nvram_char_to_ascii("", "tr_ca_cert"); %>');", 300);
	setTimeout("document.getElementById('edit_tr_client_cert').value = decodeURIComponent('<% nvram_char_to_ascii("", "tr_client_cert"); %>');", 300);
	setTimeout("document.getElementById('edit_tr_client_key').value = decodeURIComponent('<% nvram_char_to_ascii("", "tr_client_key"); %>');", 300);
}

function save_cert(){
	document.form.tr_ca_cert.value = document.getElementById('edit_tr_ca_cert').value;
	document.form.tr_client_cert.value = document.getElementById('edit_tr_client_cert').value;
	document.form.tr_client_key.value = document.getElementById('edit_tr_client_key').value;
	cancel_cert_panel();
}

function cal_panel_block(){
	var blockmarginLeft;
	if (window.innerWidth)
		winWidth = window.innerWidth;
	else if ((document.body) && (document.body.clientWidth))
		winWidth = document.body.clientWidth;
		
	if (document.documentElement  && document.documentElement.clientHeight && document.documentElement.clientWidth){
		winWidth = document.documentElement.clientWidth;
	}

	if(winWidth >1050){	
		winPadding = (winWidth-1050)/2;	
		winWidth = 1105;
		blockmarginLeft= (winWidth*0.15)+winPadding;
	}
	else if(winWidth <=1050){
		blockmarginLeft= (winWidth)*0.15+document.body.scrollLeft;	

	}

	document.getElementById("cert_panel").style.marginLeft = blockmarginLeft+"px";
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">

<div id="cert_panel"  class="contentM_qis" style="box-shadow: 3px 3px 10px #000;">
		<table class="QISform_wireless" border=0 align="center" cellpadding="5" cellspacing="0">
			<tr>
				<div class="description_down">Keys and Certification</div>
			</tr>
			<tr>
				<div style="margin-left:30px; margin-top:10px;">
					<p>Only paste the content of the <span style="color:#FFCC00;">----- BEGIN xxx ----- </span>/<span style="color:#FFCC00;"> ----- END xxx -----</span> block (including those two lines).
					<p>Limit: 2999 characters per field
				</div>
				<div style="margin:5px;*margin-left:-5px;"><img style="width: 730px; height: 2px;" src="/images/New_ui/export/line_export.png"></div>
			</tr>			
			<!--===================================Beginning of tls Content===========================================-->
        	<tr>
          		<td valign="top">
            		<table width="700px" border="0" cellpadding="4" cellspacing="0">
                	<tbody>
                	<tr>
                		<td valign="top">
							<table width="100%" id="page_cert" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable">
								<tr>
									<th>Certificate Authority</th>
									<td>
										<textarea rows="8" class="textarea_ssh_table" id="edit_tr_ca_cert" name="edit_tr_ca_cert" cols="65" maxlength="2999"><% nvram_clean_get("tr_ca_cert"); %></textarea>
									</td>
								</tr>
								<tr>
									<th>Client Certificate</th>
									<td>
										<textarea rows="8" class="textarea_ssh_table" id="edit_tr_client_cert" name="edit_tr_client_cert" cols="65" maxlength="2999"><% nvram_clean_get("tr_client_cert"); %></textarea>
									</td>
								</tr>
								<tr>
									<th>Client Key</th>
									<td>
										<textarea rows="8" class="textarea_ssh_table" id="edit_tr_client_key" name="edit_tr_client_key" cols="65" maxlength="2999"><% nvram_clean_get("tr_client_key"); %></textarea>
									</td>
								</tr>
							</table>
			  			</td>
			  		</tr>						
	      			</tbody>						
      				</table>
						<div style="margin-top:5px;width:100%;text-align:center;">
							<input class="button_gen" type="button" onclick="cancel_cert_panel();" value="<#CTL_Cancel#>">
							<input class="button_gen" type="button" onclick="save_cert();" value="<#CTL_onlysave#>">	
						</div>					
          		</td>
      		</tr>
      </table>		
      <!--===================================Ending of Certification & Key Content===========================================-->			
</div>


<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">

<input type="hidden" name="current_page" value="Advanced_TR069_Content.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_tr">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="tr_discovery" value="<% nvram_get("tr_discovery"); %>">
<input type="hidden" name="tr_ca_cert" value="<% nvram_clean_get("tr_ca_cert"); %>">
<input type="hidden" name="tr_client_cert" value="<% nvram_clean_get("tr_client_cert"); %>">
<input type="hidden" name="tr_client_key" value="<% nvram_clean_get("tr_client_key"); %>">

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
		<td valign="top" >
		
<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
<tbody>
	<tr>
		  <td bgcolor="#4D595D" valign="top"  >
		  <div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_6#> - TR069</div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  <!--<div class="formfontdesc"><#FirewallConfig_display2_sectiondesc#></div>-->

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
				<thead>
					<tr>
						<td colspan="2">ACS Login Information</td>
					</tr>
				</thead>	

				<tr>
					<th>Enable TR069</th>
					<td>
						<input type="radio" name="tr_enable" class="input" value="1" <% nvram_match_x("", "tr_enable", "1", "checked"); %>>Enable
						<input type="radio" name="tr_enable" class="input" value="0" <% nvram_match_x("", "tr_enable", "0", "checked"); %>>Disable
					</td>
				</tr>

				<tr>
					<th>URL</th>
					<td>
						<input type="text" maxlength="64" name="tr_acs_url" class="input_32_table" value="<% nvram_get("tr_acs_url"); %>" onKeyPress="return is_string(this,event);" autocorrect="off" autocapitalize="off"/>
						<span id="cert_text" onclick="set_cert();" style="text-decoration:underline;cursor:pointer;">Import Certificate</span>
					</td>
				</tr>

				<tr>
					<th>User Name</th>
					<td>
						<input type="text" maxlength="32" name="tr_username" class="input_15_table" value="<% nvram_get("tr_username"); %>" onKeyPress="return is_string(this,event);" autocorrect="off" autocapitalize="off"/>
					</td>
				</tr>

				<tr>
					<th>Password</th>
					<td>
						<input type="password" maxlength="32" name="tr_passwd" class="input_15_table" value="<% nvram_get("tr_passwd"); %>" onKeyPress="return is_string(this,event);" autocorrect="off" autocapitalize="off"/>
					</td>
				</tr>
			</table>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
				<thead>
					<tr>
						<td colspan="2">Connection Request Information</td>
					</tr>
				</thead>

				<tr>
					<th>User Name</th>
					<td>
						<input type="text" maxlength="32" name="tr_conn_username" class="input_15_table" value="<% nvram_get("tr_conn_username"); %>" onKeyPress="return is_string(this,event);" autocorrect="off" autocapitalize="off"/>
					</td>
				</tr>

				<tr>
					<th>Password</th>
					<td>
						<input type="password" maxlength="32" name="tr_conn_passwd" class="input_15_table" value="<% nvram_get("tr_conn_passwd"); %>" onKeyPress="return is_string(this,event);" autocorrect="off" autocapitalize="off"/>
					</td>
				</tr>
			</table>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
				<thead>
					<tr>
						<td colspan="2">Periodic Inform Config</td>
					</tr>
				</thead>

				<tr>
					<th>Enable Periodic Inform</th>
					<td>
						<input type="radio" name="tr_inform_enable" class="input" value="1" <% nvram_match_x("", "tr_inform_enable", "1", "checked"); %>>Enable
						<input type="radio" name="tr_inform_enable" class="input" value="0" <% nvram_match_x("", "tr_inform_enable", "0", "checked"); %>>Disable
					</td>
				</tr>

				<tr>
					<th>Interval</th>
					<td>
						<input type="text" maxlength="10" name="tr_inform_interval" class="input_15_table" value="<% nvram_get("tr_inform_interval"); %>" onKeyPress="return is_number(this,event);" autocorrect="off" autocapitalize="off"/>
					</td>
				</tr>	
        	</table>

            <div class="apply_gen">
            	<input name="button" type="button" class="button_gen" onclick="applyRule();" value="<#CTL_apply#>"/>
            </div>        
        
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
