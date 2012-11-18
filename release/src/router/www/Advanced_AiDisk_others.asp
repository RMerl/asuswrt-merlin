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
<title><#Web_Title#> - <#menu5_4_3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>]; // [[MAC, associated, authorized], ...]

var ddns_enable = '<% nvram_get("ddns_enable_x"); %>';
var ddns_server = '<% nvram_get("ddns_server_x"); %>';
var ddns_hostname = '<% nvram_get("ddns_hostname_x"); %>';;

function initial(){
	show_menu();
	$("option5").innerHTML = '<table><tbody><tr><td><div id="index_img5"></div></td><td><div style="width:120px;"><#Menu_usb_application#></div></td></tr></tbody></table>';
	$("option5").className = "m5_r";

	xfr();
  /*	Viz 2011.09
  if(ddns_enable == '1' && ddns_server == 'WWW.ASUS.COM' && ddns_hostname.length > '.asuscomm.com'.length){
  	$("computer_name").readOnly = true;
    $("computer_name").className = "devicepin";
  }*/
}

function xfr(){
        if(document.form.computer_name2.value != ""){
                document.form.computer_name.value = decodeURIComponent(document.form.computer_name2.value);
                //document.form.computer_nameb.value = encodeURIComponent(document.form.computer_name.value);
        }
        else{
                document.form.computer_name.value = document.form.computer_name3.value;
                //document.form.computer_nameb.value = encodeURIComponent(document.form.computer_name.value);
        }
}

function blanktest(obj, flag){
        var value2 = eval("document.form."+flag+"2.value");
        var value3 = eval("document.form."+flag+"3.value");
        
        if(obj.value == ""){
                if(value2 != "")
                        obj.value = decodeURIComponent(value2);
                else
                        obj.value = value3;
                
                alert("<#JS_Shareblanktest#>");
                
                return false;
        }
        
        return true;
}

function copytob(){
       // document.form.computer_nameb.value = encodeURIComponent(document.form.computer_name.value);
}

function copytob2(){
        document.form.st_samba_workgroupb.value = encodeURIComponent(document.form.st_samba_workgroup.value);
}

function applyRule(){
    if(validForm()){
        showLoading();
        document.form.current_page.value = "/Advanced_AiDisk_others.asp";
        document.form.next_page.value = "";                
		document.form.submit();
     }
}

function trim(str){
      	return str.replace(/(^s*)|(s*$)/g, "");
}

function validForm(){
    if(!validate_range(document.form.st_max_user, 1, 10)){
		document.form.st_max_user.focus();
		document.form.st_max_user.select();
		return false;
    }
        
    var computer_name_check = new RegExp('^[a-zA-Z0-9\-\_\.\][a-zA-Z0-9\-\_\.\ ]*[a-zA-Z0-9\-\_]$','gi');      	
    if(!computer_name_check.test(document.form.computer_name.value)){
        alert("<#JS_validchar#>");               
        document.form.computer_name.focus();
        document.form.computer_name.select();
        return false;
    }
		
	var workgroup_check = new RegExp('^[a-zA-Z0-9\-\_\.\][a-zA-Z0-9\-\_\.\ ]*[a-zA-Z0-9\-\_]$','gi'); 
	if(!workgroup_check.test(document.form.st_samba_workgroup.value)){
        alert("<#JS_validchar#>");                
        document.form.st_samba_workgroup.focus();
        document.form.st_samba_workgroup.select();
        return false;
    }

    return true;
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
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_AiDisk_others.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_ftpsamba;restart_dnsmasq">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

<!--input type="hidden" name="computer_nameb" value=""-->
<input type="hidden" name="computer_name2" value="<% nvram_get("computer_name"); %>">
<input type="hidden" name="computer_name3" value="<% nvram_get("computer_name"); %>">
<!--input type="hidden" name="st_samba_workgroupb" value="">
<input type="hidden" name="samba_workgroup2" value="<% nvram_get("st_samba_workgroupb"); %>">
<input type="hidden" name="samba_workgroup3" value="<% nvram_get("st_samba_workgroup"); %>"-->


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
							<img onclick="go_setting('/APP_Installation.asp')" align="right" style="cursor:pointer;position:absolute;margin-left:-20px;margin-top:-30px;" title="Back to USB Extension" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
						</td>
					</tr>
				</table>
			</div>
			<div style="margin:5px;"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc"><#USB_Application_disk_miscellaneous_desc#></div>
                        <table width="98%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
                                <tr>
                                        <th width="40%">
                                                <a class="hintstyle" href="javascript:void(0);" onClick="openHint(17,1);"><#ShareNode_MaximumLoginUser_itemname#></a>
                                        </th>
                                        <td>
                                                <input type="text" name="st_max_user" class="input_3_table" maxlength="1" value="<% nvram_get("st_max_user"); %>" onKeyPress="return is_number(this, event);">
                                        </td>
                                </tr>
                                
                                <tr>
                                        <th>
                                            <a class="hintstyle" href="javascript:void(0);" onClick="openHint(17,2);"><#ShareNode_DeviceName_itemname#></a>
                                        </th>
                                        <td>
                                                <input type="text" name="computer_name" id="computer_name" class="input_15_table" maxlength="15" value="<% nvram_get("computer_name"); %>">
                                        </td>
                                </tr>
                        
                        				<tr>
                                        <th>
                                            <a class="hintstyle" href="javascript:void(0);" onClick="openHint(17,3);"><#ShareNode_WorkGroup_itemname#></a>
                                        </th>
                                        <td>
                                                <input type="text" name="st_samba_workgroup" class="input_32_table" maxlength="32" value="<% nvram_get("st_samba_workgroup"); %>">
                                        </td>
                                </tr>

				<tr>
					<th>Simpler share naming<br><i>(without the disk name)</i></th>
					<td>
						<input type="radio" name="smbd_simpler_naming" class="input" value="1" <% nvram_match_x("", "smbd_simpler_naming", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="smbd_simpler_naming" class="input" value="0" <% nvram_match_x("", "smbd_simpler_naming", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>

				<tr>
					<th>Force as Master Browser</i></th>
					<td>
						<input type="radio" name="smbd_master" class="input" value="1" <% nvram_match_x("", "smbd_master", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="smbd_master" class="input" value="0" <% nvram_match_x("", "smbd_master", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>
					<th>Set as WINS server</i></th>
					<td>
						<input type="radio" name="smbd_wins" class="input" value="1" <% nvram_match_x("", "smbd_wins", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="smbd_wins" class="input" value="0" <% nvram_match_x("", "smbd_wins", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>

                                <tr>
                                        <th>
                                        		<a class="hintstyle" href="javascript:void(0);" onClick="openHint(17,9);"><#ShareNode_FTPLANG_itemname#></a>
                                        </th>
                                        <td>
                                                <select name="ftp_lang" class="input_option" onChange="return change_common(this, 'Storage', 'ftp_lang');">
                                                        <option value="CN" <% nvram_match("ftp_lang", "CN", "selected"); %>><#ShareNode_FTPLANG_optionname3#></option>
                                                        <option value="TW" <% nvram_match("ftp_lang", "TW", "selected"); %>><#ShareNode_FTPLANG_optionname2#></option>
                                                        <option value="EN" <% nvram_match("ftp_lang", "EN", "selected"); %>>UTF-8</option><!--<#ShareNode_FTPLANG_optionname1#>-->
                                                        <!-- Viz for Common N16 : RU CZ  -->	
														<option value="RU" <% nvram_match("ftp_lang", "RU", "selected"); %>><#ShareNode_FTPLANG_optionname4#></option>
														<option value="CZ" <% nvram_match("ftp_lang", "CZ", "selected"); %>><#ShareNode_FTPLANG_optionname5#></option>
                                                </select>
                                        </td>
                                </tr>
                                
                               <!-- Viz 2011.04 tr>
                                        <th>
                                                <a class="hintstyle" href="javascript:openHint(17, 4);"><#BasicConfig_EnableDownloadMachine_itemname#></a>
                                        </th>
                                        <td>
                                                <input type="radio" name="apps_dl" value="1" <% nvram_match("apps_dl", "1", "checked"); %>><#checkbox_Yes#>
                                                <input type="radio" name="apps_dl" value="0" <% nvram_match("apps_dl", "0", "checked"); %>><#checkbox_No#>
                                        </td>
                                </tr-->                                
                                <!--tr>
                                        <th>
                                                <a class="hintstyle" href="javascript:openHint(17, 5);"><#BasicConfig_EnableDownloadShare_itemname#></a>
                                        </th>
                                        <td>
                                                <input type="radio" name="apps_dl_share" value="1" <% nvram_match("apps_dl_share", "1", "checked"); %>>Yes
                                                <input type="radio" name="apps_dl_share" value="0" <% nvram_match("apps_dl_share", "0", "checked"); %>>No
                                        </td>
                                </tr-->
                                <!--
                                <tr>
                                        <th>Seeding</th>
                                        <td>
                                                <select name="apps_seed" class="input">
                                                  <option value="1" <% nvram_match("apps_seed", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
                                                  <option value="0" <% nvram_match("apps_seed", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
                                                </select>
                                        </td>
                                </tr>
                                <tr>
                                        <th>Maximum Upload Rate</th>
                                        <td>
            <input type="text" name="apps_upload_max" class="input" maxlength="3" size="3" value="<% nvram_get("apps_upload_max"); %>" /> Kb/s
                                        </td>
                                </tr>
                                -->

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
