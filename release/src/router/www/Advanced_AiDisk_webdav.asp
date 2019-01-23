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
<title><#Web_Title#> - <#menu5_4_3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script>
function initial(){
	show_menu();
	hideAll(document.form.enable_webdav.value);
}

function hideAll(_value){
	document.getElementById("st_webdav_mode_tr").style.display = (_value == 0) ? "none" : "";
	document.getElementById("webdav_http_port_tr").style.display = (_value == 0) ? "none" : "";
	document.getElementById("webdav_https_port_tr").style.display = (_value == 0) ? "none" : "";

	if(_value == 1)
		showPortItem(document.form.st_webdav_mode.value);
}

function showPortItem(_value){
	document.getElementById("webdav_http_port_tr").style.display = (_value == 0) ? "" : "none";
	document.form.webdav_http_port.disabled = (_value == 0) ? false : true;

	document.getElementById("webdav_https_port_tr").style.display =  (_value == 0) ? "none" : "";
	document.form.webdav_https_port.disabled = (_value == 0) ? true : false;
}

function applyRule(){
	if(document.form.st_webdav_mode.value == 0){
		if(!validator.numberRange(document.form.webdav_http_port, 1, 65535)){
			return false;	
		}
	}
	else{	
		if(!validator.numberRange(document.form.webdav_https_port, 1, 65535)){
			return false;	
		}
	}

	showLoading();
	document.form.next_page.value = "";                
	document.form.submit();
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_AiDisk_webdav.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_webdav">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

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
                <td align="left" valign="top" >
                
<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
 	<tbody>
	<tr>
		  <td bgcolor="#4D595D" valign="top"  >
		  <div>&nbsp;</div>					
			<div style="width:730px">
				<table width="730px">
					<tr>
						<td align="left">
							<span class="formfonttitle"><#menu5_4#> - <#menu5_4_3#></span>
						</td>
						<td align="right">
							<img onclick="go_setting('/APP_Installation.asp')" align="right" style="cursor:pointer;position:absolute;margin-left:-20px;margin-top:-30px;" title="<#Menu_usb_application#>" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
						</td>
					</tr>
				</table>
			</div>
			<div style="margin:5px;"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc"><#USB_Application_disk_miscellaneous_desc#></div>
				<table width="98%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
        	<tr>
          	<th width="40%">WebDav to Samba</th>
						<td>
							<select name="enable_webdav" class="input_option" onChange="hideAll(this.value);">
								<option value="0" <% nvram_match("enable_webdav", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								<option value="1" <% nvram_match("enable_webdav", "1","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
							</select>
						</td>
					</tr>
 
        	<tr id="st_webdav_mode_tr" style="display:none;">
          	<th width="40%">WebDav to Samba Configure</th>
						<td>
							<select name="st_webdav_mode" class="input_option" onChange="showPortItem(this.value);">
								<option value="0" <% nvram_match("st_webdav_mode", "0", "selected"); %>>HTTP</option>
								<option value="1" <% nvram_match("st_webdav_mode", "1", "selected"); %>>HTTPS</option>
								<option value="2" <% nvram_match("st_webdav_mode", "2", "selected"); %>>BOTH</option>
							</select>
						</td>
					</tr>  

        	<tr id="webdav_http_port_tr" style="display:none;">
          	<th width="40%">WebDav to Samba Port</th>
						<td>
							<input type="text" name="webdav_http_port" class="input_6_table" maxlength="5" value="<% nvram_get("webdav_http_port"); %>" onKeyPress="return validator.isNumber(this, event);" autocorrect="off" autocapitalize="off">
						</td>
					</tr>

        	<tr id="webdav_https_port_tr" style="display:none;">
          	<th width="40%">WebDav to Samba Port</th>
						<td>
							<input type="text" name="webdav_https_port" class="input_6_table" maxlength="5" value="<% nvram_get("webdav_https_port"); %>" onKeyPress="return validator.isNumber(this, event);" autocorrect="off" autocapitalize="off">
						</td>
					</tr>

				</table>

            <div class="apply_gen">
            	<input type="button" class="button_gen" value="<#CTL_apply#>" onclick="applyRule();">
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
