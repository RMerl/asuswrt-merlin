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
<title><#Web_Title#> - <#menu5_3_6#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" language="JavaScript" src="/help.js"></script>
<script type="text/javascript" language="JavaScript" src="/validator.js"></script>
<script type="text/javaScript" src="/js/jquery.js"></script>
<script>
<% wanlink(); %>
var ddns_hostname_x_t = '<% nvram_get("ddns_hostname_x"); %>';
var ddns_server_x_t = '<% nvram_get("ddns_server_x"); %>';
var ddns_updated_t = '<% nvram_get("ddns_updated"); %>';
var wans_mode ='<% nvram_get("wans_mode"); %>';
var no_phddns = isSupport("no_phddns");

function init(){
	show_menu();
	if(realip_support){
		if(!external_ip)
	    	showhide("wan_ip_hide2", 1);
	    else
	    	showhide("wan_ip_hide2", 0);

	    showhide("wan_ip_hide3", 0);		
	}
	else
    	valid_wan_ip();

    ddns_load_body();

	//if(dualWAN_support && wans_mode == "lb")
	//	document.getElementById("lb_note").style.display = "";

	if(no_phddns){
		for(var i = 0; i < document.form.ddns_server_x.length; i++){
			if(document.form.ddns_server_x.options[i].value == "WWW.ORAY.COM"){
				document.form.ddns_server_x.remove(i);
				break;
			}
		}
	}
}

function check_update(){
    var ddns_ipaddr_t = '<% nvram_get("ddns_ipaddr"); %>';
		ddns_ipaddr_t = ddns_ipaddr_t.replace(/&#10/g,"");

    if ((wanlink_ipaddr() == ddns_ipaddr_t) &&
        (ddns_server_x_t == document.form.ddns_server_x.value) &&
        (ddns_hostname_x_t == document.form.ddns_hostname_x.value) &&
			ddns_updated_t == '1'){
			force_update();
    }else{
			document.form.submit();
			showLoading();
    }
}

function force_update() {
    var r = confirm("<#LANHostConfig_x_DDNS_update_confirm#>");
	if(r == false)
		return false
		
	document.form.submit();
	showLoading();	
}

function valid_wan_ip() {
    // test if WAN IP is a private IP.
    var A_class_start = inet_network("10.0.0.0");
    var A_class_end = inet_network("10.255.255.255");
    var B_class_start = inet_network("172.16.0.0");
    var B_class_end = inet_network("172.31.255.255");
    var C_class_start = inet_network("192.168.0.0");
    var C_class_end = inet_network("192.168.255.255");     
    var ip_obj = wanlink_ipaddr();
    var ip_num = inet_network(ip_obj);
    var ip_class = "";

    if(ip_num > A_class_start && ip_num < A_class_end)
        ip_class = 'A';
    else if(ip_num > B_class_start && ip_num < B_class_end)
        ip_class = 'B';
    else if(ip_num > C_class_start && ip_num < C_class_end)
        ip_class = 'C';
    else if(ip_num != 0){
        showhide("wan_ip_hide2", 0);
        showhide("wan_ip_hide3", 0);
        return;
    }
		
    showhide("wan_ip_hide2", 1);
    showhide("wan_ip_hide3", 0);
    return;
}

function ddns_load_body(){
    var ddns_return_code = '<% nvram_get_ddns("LANHostConfig","ddns_return_code"); %>';
    var ddns_old_name = '<% nvram_get("ddns_old_name"); %>';
    var ddns_server_x = '<% nvram_get("ddns_server_x"); %>';
	var ddns_enable_x = '<% nvram_get("ddns_enable_x"); %>';
   
    if(ddns_enable_x == 1){
        inputCtrl(document.form.ddns_server_x, 1);
        document.getElementById('ddns_hostname_tr').style.display = "";
        if(ddns_server_x == "WWW.ASUS.COM" || ddns_server_x == ""){
            document.form.ddns_hostname_x.parentNode.style.display = "none";
            document.form.DDNSName.parentNode.style.display = "";
            var ddns_hostname_title = ddns_hostname_x_t.substring(0, ddns_hostname_x_t.indexOf('.asuscomm.com'));
            if(ddns_hostname_x_t != '' && ddns_hostname_title)
                document.getElementById("DDNSName").value = ddns_hostname_title;
            else
                document.getElementById("DDNSName").value = "<#asusddns_inputhint#>";
        }else{
            document.form.ddns_hostname_x.parentNode.style.display = "";
            document.form.DDNSName.parentNode.style.display = "none";
            inputCtrl(document.form.ddns_username_x, 1);
            inputCtrl(document.form.ddns_passwd_x, 1);                   
            if(ddns_hostname_x_t != '')
                document.getElementById("ddns_hostname_x").value = ddns_hostname_x_t;
            else
                document.getElementById("ddns_hostname_x").value = "<#asusddns_inputhint#>";
        }
		
        change_ddns_setting(document.form.ddns_server_x.value);
	    if(document.form.ddns_server_x.value == "WWW.ORAY.COM"){
		    if(ddns_updated_t == "1"){
				document.getElementById("ddns_hostname_info_tr").style.display = "";
				document.getElementById("ddns_hostname_x_value").innerHTML = ddns_hostname_x_t;
			}
		}
    }else{
        inputCtrl(document.form.ddns_server_x, 0);
        document.getElementById('ddns_hostname_tr').style.display = "none";
        document.getElementById("ddns_hostname_info_tr").style.display = "none";
        inputCtrl(document.form.ddns_username_x, 0);
        inputCtrl(document.form.ddns_passwd_x, 0);
        document.form.ddns_wildcard_x[0].disabled= 1;
        document.form.ddns_wildcard_x[1].disabled= 1;
        showhide("wildcard_field",0);
    }   
   
    hideLoading();
    var ddnsHint = getDDNSState(ddns_return_code, ddns_hostname_x_t, ddns_old_name);

    if(ddnsHint != "")
        alert(ddnsHint);
    if(ddns_return_code.indexOf('200')!=-1 || ddns_return_code.indexOf('220')!=-1 || ddns_return_code == 'register,230'){
        showhide("wan_ip_hide2", 0);
        if(ddns_server_x == "WWW.ASUS.COM")
            showhide("wan_ip_hide3", 1);       
    }
}

function applyRule(){
    if(validForm()){
        if(document.form.ddns_enable_x[0].checked == true && document.form.ddns_server_x.selectedIndex == 0){          
            document.form.ddns_hostname_x.value = document.form.DDNSName.value+".asuscomm.com";   
        }

        check_update();
    }
}

function validForm(){
	if(document.form.ddns_enable_x[0].checked){		//ddns enable
		if(document.form.ddns_server_x.selectedIndex == 0){		//WWW.ASUS.COM	
			if(document.form.DDNSName.value == ""){
				alert("<#LANHostConfig_x_DDNS_alarm_14#>");
				document.form.DDNSName.focus();
				document.form.DDNSName.select();
				return false;
			}else{
				if(!validate_ddns_hostname(document.form.DDNSName)){
					document.form.DDNSName.focus();
					document.form.DDNSName.select();
					return false;
				}

				return true;
			}
		}else{
			if(!validator.numberRange(document.form.ddns_refresh_x, 0, 365))
				return false;

			if(document.form.ddns_server_x.value != "WWW.ORAY.COM" && document.form.ddns_hostname_x.value == ""){
				alert("<#LANHostConfig_x_DDNS_alarm_14#>");
				document.form.ddns_hostname_x.focus();
				document.form.ddns_hostname_x.select();
				return false;
			}else if(!validator.string(document.form.ddns_hostname_x)){
				return false;
			}

			if(document.form.ddns_server_x.value != "CUSTOM"){             // Not CUSTOM
				if(document.form.ddns_username_x.value == ""){
					alert("<#QKSet_account_nameblank#>");
					document.form.ddns_username_x.focus();
					document.form.ddns_username_x.select();
					return false;
				}else if(!validator.string(document.form.ddns_username_x)){
					return false;
				}
			
				if(document.form.ddns_passwd_x.value == ""){
					alert("<#File_Pop_content_alert_desc6#>");
					document.form.ddns_passwd_x.focus();
					document.form.ddns_passwd_x.select();
					return false;
				}else if(!validator.string(document.form.ddns_passwd_x)){
					return false;
				}
			}

			if(document.form.ddns_regular_period.value < 30){	
				alert(Untranslated.period_time_validation + " : 30");
				document.form.ddns_regular_period.focus();
				document.form.ddns_regular_period.select();
				return false;
			}
		
			return true;
		}
	}
	else
		return true;
}

function checkDDNSReturnCode(){
    $.ajax({
    	url: '/ajax_ddnscode.asp',
    	dataType: 'script', 
    	error: function(xhr){
      		checkDDNSReturnCode();
    	},
    	success: function(response){
            if(ddns_return_code == 'ddns_query')
        	    setTimeout("checkDDNSReturnCode();", 500);
            else          	
                refreshpage(); 
       }
   });
}

function validate_ddns_hostname(o){
	dot=0;
	s=o.value;

	if(s == ""){
		show_alert_block("<#QKSet_account_nameblank#>");
		return false;
	}		

	var unvalid_start=new RegExp("^[0-9].*", "gi");
	if(unvalid_start.test(s) ){
		show_alert_block("<#LANHostConfig_x_DDNS_alarm_7#>");
		return false;
	}
	
	if (!validator.string(o)){
		return false;
	}
		
	for(i=0;i<s.length;i++){
		c = s.charCodeAt(i);
		if (c==46){
			dot++;
			if(dot>0){
				show_alert_block("<#LANHostConfig_x_DDNS_alarm_7#>");
				return false;
			}
		}
		
		if (!validator.hostNameChar(c)){
			show_alert_block("<#LANHostConfig_x_DDNS_alarm_13#> '" + s.charAt(i) +"' !");
			return false;
		}
	}
		
	return true;
}

function show_alert_block(alert_str){
	document.getElementById("alert_block").style.display = "block";
	showtext(document.getElementById("alert_str"), alert_str);
}

function cleandef(){
	if(document.form.DDNSName.value == "<#asusddns_inputhint#>")
		document.form.DDNSName.value = "";	
}

function onSubmitApply(s){
	if(s == "hostname_check"){
		showLoading();
		if(!validate_ddns_hostname(document.form.ddns_hostname_x)){
			hideLoading();
			return false;
		}
	}
	
	document.form.action_mode.value = "Update";
	document.form.action_script.value = s;
	return true;
}
	
</script>
</head>

<body onload="init();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_ASUSDDNS_Content.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_ddns">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

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
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td align="left" valign="top" >
          
		<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
		<tbody>
			<tr>
		  		<td bgcolor="#4D595D" valign="top"  >
		  		<div>&nbsp;</div>
		  		<div class="formfonttitle"><#menu5_3#> - <#menu5_3_6#></div>
		  		<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		 		<div class="formfontdesc"><#LANHostConfig_x_DDNSEnable_sectiondesc#></div>
		 		<div class="formfontdesc" style="margin-top:-8px;"><#NSlookup_help#></div>
				<div class="formfontdesc" id="wan_ip_hide2" style="color:#FFCC00;"><#LANHostConfig_x_DDNSEnable_sectiondesc2#></div>
				<div class="formfontdesc" id="wan_ip_hide3" style="color:#FFCC00;"><#LANHostConfig_x_DDNSEnable_sectiondesc3#></div>
				<div class="formfontdesc" id="lb_note" style="color:#FFCC00; display:none;"><#lb_note_ddns#></div>
				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
				<input type="hidden" name="wl_gmode_protection_x" value="<% nvram_get("wl_gmode_protection_x"); %>">
			<tr>
				<th><#LANHostConfig_x_DDNSEnable_itemname#></th>
				<td>
				<input type="radio" value="1" name="ddns_enable_x"onClick="return change_common_radio(this, 'LANHostConfig', 'ddns_enable_x', '1')" <% nvram_match("ddns_enable_x", "1", "checked"); %>><#checkbox_Yes#>
				<input type="radio" value="0" name="ddns_enable_x"onClick="return change_common_radio(this, 'LANHostConfig', 'ddns_enable_x', '0')" <% nvram_match("ddns_enable_x", "0", "checked"); %>><#checkbox_No#>
				</td>
			</tr>		
			<tr>
				<th><#LANHostConfig_x_DDNSServer_itemname#></th>
				<td>
                  		<select name="ddns_server_x"class="input_option" onchange="change_ddns_setting(this.value)">
                    			<option value="WWW.ASUS.COM" <% nvram_match("ddns_server_x", "WWW.ASUS.COM","selected"); %>>WWW.ASUS.COM</option>
                    			<option value="WWW.DYNDNS.ORG" <% nvram_match("ddns_server_x", "WWW.DYNDNS.ORG","selected"); %>>WWW.DYNDNS.ORG</option>
                    			<option value="WWW.DYNDNS.ORG(CUSTOM)" <% nvram_match("ddns_server_x", "WWW.DYNDNS.ORG(CUSTOM)","selected"); %>>WWW.DYNDNS.ORG(CUSTOM)</option>
                    			<option value="WWW.DYNDNS.ORG(STATIC)" <% nvram_match("ddns_server_x", "WWW.DYNDNS.ORG(STATIC)","selected"); %>>WWW.DYNDNS.ORG(STATIC)</option>
                    			<option value="WWW.SELFHOST.DE" <% nvram_match("ddns_server_x", "WWW.SELFHOST.DE","selected"); %>>WWW.SELFHOST.DE</option>
								<option value="WWW.ZONEEDIT.COM" <% nvram_match("ddns_server_x", "WWW.ZONEEDIT.COM","selected"); %>>WWW.ZONEEDIT.COM</option>
                    			<option value="WWW.DNSOMATIC.COM" <% nvram_match("ddns_server_x", "WWW.DNSOMATIC.COM","selected"); %>>WWW.DNSOMATIC.COM</option>
                    			<option value="WWW.TUNNELBROKER.NET" <% nvram_match("ddns_server_x", "WWW.TUNNELBROKER.NET","selected"); %>>WWW.TUNNELBROKER.NET</option>
								<option value="WWW.NO-IP.COM" <% nvram_match("ddns_server_x", "WWW.NO-IP.COM","selected"); %>>WWW.NO-IP.COM</option>
								<option value="WWW.ORAY.COM" <% nvram_match("ddns_server_x", "WWW.ORAY.COM","selected"); %>>WWW.ORAY.COM(花生壳)</option>                    			<option value="WWW.NAMECHEAP.COM" <% nvram_match("ddns_server_x", "WWW.NAMECHEAP.COM","selected"); %>>WWW.NAMECHEAP.COM</option>
					<option value="CUSTOM" <% nvram_match("ddns_server_x", "CUSTOM","selected");  %>>Custom</option>
                  		</select>
				<a id="link" href="javascript:openLink('x_DDNSServer')" style=" margin-left:5px; text-decoration: underline;"><#LANHostConfig_x_DDNSServer_linkname#></a>
				<a id="linkToHome" href="javascript:openLink('x_DDNSServer')" style=" margin-left:5px; text-decoration: underline;"><#ddns_home_link#></a>
				<div id="customnote" style="display:none;"><span>For the Custom DDNS you must manually create a ddns-start script that handles your custom notification.</span></div>
				<div id="need_custom_scripts" style="display:none;"><span>WARNING: you must enable both the JFFS2 partition and custom scripts support!<br>Click <a href="Advanced_System_Content.asp" style="text-decoration: underline;">HERE</a> to proceed.</span></div>
				</td>
			</tr>
			<tr id="ddns_hostname_tr">
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,13);"><#LANHostConfig_x_DDNSHostNames_itemname#></a></th>
				<td>
					<div id="ddnsname_input" style="display:none;">
						<input type="text" maxlength="64" class="input_25_table" name="ddns_hostname_x" id="ddns_hostname_x" value="<% nvram_get("ddns_hostname_x"); %>" onKeyPress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off">
					</div>
					<div id="asusddnsname_input" style="display:none;">
						<input type="text" maxlength="32" class="input_32_table" name="DDNSName" id="DDNSName" class="inputtext" onKeyPress="return validator.isString(this, event)" OnClick="cleandef();" autocorrect="off" autocapitalize="off">.asuscomm.com
						<div id="alert_block" style="color:#FFCC00; margin-left:5px; font-size:11px;display:none;">
								<span id="alert_str"></span>
						</div>
					</div>	
							
				</td>
			</tr>
			<tr id="ddns_hostname_info_tr" style="display:none;">
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,13);"><#LANHostConfig_x_DDNSHostNames_itemname#></a></th>
				<td id="ddns_hostname_x_value"><% nvram_get("ddns_hostname_x"); %></td>
			</tr>
			<tr>
				<th id="ddns_username_th"><#LANHostConfig_x_DDNSUserName_itemname#></th>
				<td><input type="text" maxlength="32" class="input_25_table" name="ddns_username_x" value="<% nvram_get("ddns_username_x"); %>" onKeyPress="return validator.isString(this, event)" autocomplete="off" autocorrect="off" autocapitalize="off"></td>
			</tr>
			<tr>
				<th><#LANHostConfig_x_DDNSPassword_itemname#></th>
				<td><input type="password" autocapitalization="off" maxlength="64" class="input_25_table" name="ddns_passwd_x" value="<% nvram_get("ddns_passwd_x"); %>" autocomplete="off" autocorrect="off" autocapitalize="off"></td>
			</tr>
			<tr id="wildcard_field">
				<th><#LANHostConfig_x_DDNSWildcard_itemname#></th>
				<td>
					<input type="radio" value="1" name="ddns_wildcard_x" onClick="return change_common_radio(this, 'LANHostConfig', 'ddns_wildcard_x', '1')" <% nvram_match("ddns_wildcard_x", "1", "checked"); %>><#checkbox_Yes#>
					<input type="radio" value="0" name="ddns_wildcard_x" onClick="return change_common_radio(this, 'LANHostConfig', 'ddns_wildcard_x', '0')" <% nvram_match("ddns_wildcard_x", "0", "checked"); %>><#checkbox_No#>
				</td>
			</tr>
			<tr>
				<th>Forced refresh interval (in days)</th>
				<td>
					<input type="text" maxlength="3" name="ddns_refresh_x" class="input_3_table" value="<% nvram_get("ddns_refresh_x"); %>" onKeyPress="return validator.isNumber(this,event)">
				</td>
			</tr>
			<tr id="check_ddns_field" style="display:none;">
				<th><#DDNS_verification_enable#></th>
				<td>
					<input type="radio" value="1" name="ddns_regular_check" onClick="change_ddns_setting(document.form.ddns_server_x.value);" <% nvram_match("ddns_regular_check", "1", "checked"); %>><#checkbox_Yes#>
					<input type="radio" value="0" name="ddns_regular_check" onClick="change_ddns_setting(document.form.ddns_server_x.value);" <% nvram_match("ddns_regular_check", "0", "checked"); %>><#checkbox_No#>
				</td>
			</tr>
			<tr style="display:none;">
				<th><#DDNS_verification_frequency#></th>
				<td>
					<input type="text"  class="input_3_table" name="ddns_regular_period" value="<% nvram_get("ddns_regular_period"); %>" autocorrect="off" autocapitalize="off"> <#Minute#>
				</td>
			</tr>
			<tr style="display:none;">
				<th><#LANHostConfig_x_DDNSStatus_itemname#></th>
				<td>
					<input type="hidden" maxlength="15" class="button_gen" size="12" name="" value="<% nvram_get("DDNSStatus"); %>">
				  	<input type="submit" maxlength="15" class="button_gen" onclick="showLoading();return onSubmitApply('ddnsclient');" size="12" name="LANHostConfig_x_DDNSStatus_button" value="<#LANHostConfig_x_DDNSStatus_buttonname#>" /></td>
			</tr>
		</table>
				<div class="apply_gen">
					<input class="button_gen" onclick="applyRule();" type="button" value="<#CTL_apply#>" />
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
