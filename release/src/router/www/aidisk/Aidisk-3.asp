<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>AiDisk Wizard</title>
<link rel="stylesheet" type="text/css" href="aidisk.css">
<link rel="stylesheet" type="text/css" href="/form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javaScript" src="/general.js"></script>
<script type="text/javaScript" src="/validator.js"></script>
<script type="text/javaScript" src="/js/jquery.js"></script>
<script>
var ddns_server_x = '<% nvram_get("ddns_server_x"); %>';
var ddns_hostname_x = '<% nvram_get("ddns_hostname_x"); %>';
var ddns_old_name = '<% nvram_get("ddns_old_name"); %>';
var ddns_return_code = '<% nvram_get_ddns("LANHostConfig", "ddns_return_code"); %>';
var ddns_hostname_title;


function initial(){
	parent.hideLoading();
	
	if(this.ddns_server_x == "WWW.ASUS.COM" && this.ddns_hostname_x != ''){
		this.ddns_hostname_title = this.ddns_hostname_x.substring(0, this.ddns_hostname_x.indexOf('.asuscomm.com'));
		if(this.ddns_hostname_x != '' && this.ddns_hostname_title)
			document.getElementById("DDNSName").value = this.ddns_hostname_title;
		else
			document.getElementById("DDNSName").value = "<#asusddns_inputhint#>";
	}
	else{
		document.getElementById("DDNSName").value = "<#asusddns_inputhint#>";
	}
	switch_ddns();
}

function switch_ddns(){
	if(document.form.check_asus_ddns[0].checked){
		parent.setASUSDDNS_enable("1");
		document.form.ddns_server_x.value = "WWW.ASUS.COM";
	}
	else{
		parent.setASUSDDNS_enable("0");
		document.form.check_asus_ddns[1].focus();
		document.form.ddns_server_x.value = this.ddns_server_x;
		
		if(parent.show_iframe_page("statusframe").indexOf("ASUS_DDNS_TOS.asp") > 0)
			parent.show_help_iframe(3);
	}
	
	show_TOS_checkbox();
	parent.restore_help_td();
}

function show_TOS_checkbox(){
	if(document.form.check_asus_ddns[0].checked){
		document.form.asusddns_tos_agreement.value = "1";
		document.getElementById("ddnsname_input").style.display = "block";
		document.getElementById("DDNSName").focus();
		document.getElementById("DDNSName").select();
	}
	else{
		document.form.asusddns_tos_agreement.value = "0";		
		document.getElementById("ddnsname_input").style.display = "none";	
	}

	if(this.ddns_server_x == "WWW.ASUS.COM")
		check_return_code();
	}

function show_alert_block(alert_str){
	document.getElementById("alert_block").style.display = "block";
	
	showtext(document.getElementById("alert_str"), alert_str);
}

function hide_alert_block(){
	document.getElementById("alert_block").style.display = "none";
}

function check_return_code(){

	if(this.ddns_return_code != 'no_change'){
		var ddnsHint = getDDNSState(this.ddns_return_code, this.ddns_hostname_x, this.ddns_old_name);
		if(ddnsHint != ""){
			if(this.ddns_return_code == 'Time_out')
				show_alert_block("<#LANHostConfig_x_DDNS_alarm_12#>");
			else
				show_alert_block(ddnsHint);
		}
	}

	document.getElementById("loadingIcon").style.display = "none";	
	this.ddns_return_code = "";
}

function verify_ddns_name(){
	
	if(!validate_ddns_hostname(document.getElementById("DDNSName"))){
		document.form.DDNSName.focus();
		document.form.DDNSName.select();
		return false;		
		
	}else{
		document.getElementById("loadingIcon").style.display = "";
		
		document.form.current_page.value = "/aidisk/Aidisk-3.asp";
		document.form.ddns_hostname_x.value = document.getElementById("DDNSName").value+".asuscomm.com";
		
		document.form.flag.value = "nodetect";
		FormActions("", "apply", "aidisk_asusddns_register", "1");
		document.form.target = "hidden_frame";
		document.form.submit();
		
		return true;
	}
}

function go_next_page(){
	if(document.form.check_asus_ddns[0].checked){
		if(!verify_ddns_name())
			return;
	}
	else {
		document.form.action = "/aidisk/Aidisk-4.asp";
		document.form.target = "";
		document.form.submit();
	}
}

function go_pre_page(){
	document.form.action = "/aidisk/Aidisk-2.asp";
	document.form.target = "";
	document.form.submit();
}

function validate_ddns_hostname(o){
	dot = 0;
	s = o.value;
	if(s == ""){
		show_alert_block("<#QKSet_account_nameblank#>");
		return false;
	}
	
	var unvalid_start=new RegExp("^[0-9].*", "gi");
	if(unvalid_start.test(s) )
	{
		show_alert_block("<#LANHostConfig_x_DDNS_alarm_7#>");
		return false;
	}	
	if(!validator.string(o)){
		return false;
	}
	
	for(var i = 0; i < s.length; ++i){
		c = s.charCodeAt(i);
		if(c == 46){
			++dot;
			if(dot > 0){
				show_alert_block("<#LANHostConfig_x_DDNS_alarm_7#>");
				return false;
			}
		}
		
		if(!validator.hostNameChar(c)){
			show_alert_block("<#LANHostConfig_x_DDNS_alarm_13#> '"+s.charAt(i)+"' !");
			return false;
		}
	}
	
	return true;
}

function done_validating(action){
	document.form.action = "/aidisk/Aidisk-3.asp";
	document.form.target = "";
	document.form.submit();
}

function showLoading(seconds, flag){
	parent.showLoading(seconds, flag);
}

function hideLoading(flag){
	parent.hideLoading(flag);
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
                else {
			if( (ddns_return_code == 'register,200')
			||  (ddns_return_code == 'register,220')
			||  (ddns_return_code == 'register,230')
			||  (ddns_return_code == 'no_change') ) {
		        	parent.setASUSDDNS_enable("1");
                		document.form.action = "/aidisk/Aidisk-4.asp";
		                document.form.target = "";
                		document.form.submit();
			}
			else
				check_return_code();
		}
        }
   });
}

function cleandef(){
		if(document.form.DDNSName.value == "<#asusddns_inputhint#>")
				document.form.DDNSName.value = "";	
}
</script>
</head>

<body onload="initial();">
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame" onsubmit="return go_next_page();">
<input type="hidden" name="current_page" value="">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="aidisk_asusddns_register">
<input type="hidden" name="action_wait" value="15">
<input type="hidden" name="asusddns_tos_agreement" value="<% nvram_get("asusddns_tos_agreement"); %>">
<input type="hidden" name="ddns_server_x" value="<% nvram_get("ddns_server_x"); %>">
<input type="hidden" name="ddns_hostname_x" value="<% nvram_get("ddns_hostname_x"); %>">
<input type="hidden" name="flag" value="">

	<div class="aidisk1_table">
<table>
	<tr>
  <td height="780" align="left" valign="top" bgcolor="#4d595d" style="padding-top:25px;">	<!--height="780"   align="center" //Viz -->
  <table width="740" height="225" border="0">
    <!-- start Step 2 -->  
  	<tr>
    	<td>
    	<table width="740" border="0">
    		<tr>
    			<td width="15%" height="90px" style="background:url(/images/New_ui/aidisk/step1.png) 0% 0% no-repeat;"></td>
    			<td width="15%"><img src="/images/New_ui/aidisk/steparrow.png" /></td>
    			<td width="15%" height="90px" style="background:url(/images/New_ui/aidisk/step2.png) 0% 95% no-repeat;"></td>
    			<td width="15%"><img src="/images/New_ui/aidisk/steparrow.png" /></td>
    			<td width="15%" height="90px" style="background:url(/images/New_ui/aidisk/step3.png) 0% 0% no-repeat;"></td>
    			<td width="25%">&nbsp;</td>
    		</tr>
    	</table>
    	</td>
    </tr>
    
    <tr>
    	<td align="left" class="formfonttitle" style="padding-left:20px;" height="72"><#Step2_desp#></td>
    </tr>

    <tr>
			<td>
			<div style="margin-left:20px;">	
				<p><input type="radio" name="check_asus_ddns" id="c1" onClick="switch_ddns();" checked>
					<label for="c1"><#DDNSterm_agreeword#></label>
				</p>			
				<br/>
						<div id="ddnsname_input" class="aidiskdesc" style="display:none;">
							<input type="text" name="DDNSName" id="DDNSName" class="input_25_table" onClick="cleandef()" autocorrect="off" autocapitalize="off">.asuscomm.com
							<div id="alert_block" style="color:#FFCC00; margin-left:5px; font-size:11px;display:none;">
								<span id="alert_str"></span>
							</div>
						</div>
				<br/>
				<p><input type="radio" name="check_asus_ddns" id="c2" onClick="switch_ddns();" >
						<label for="c2"><#neednotDDNS#></label>
				</p>
			</div>							
			</td>
		</tr>
	
    <tr>
    	<td>
    		<div style="margin-top:30px;"><img src="/images/New_ui/export/line_export.png" /></div>
    	</td>
    </tr>     

		<tr valign="bottom">
  		<td width="20%">
			<div class="apply_gen" style="margin-top:30px">	
				<input type="button" id="prevButton" value="<#CTL_prev#>" onclick="go_pre_page();" class="button_gen">
				<input type="submit" id="nextButton" value="<#CTL_next#>" class="button_gen">
				<img id="loadingIcon" style="display:none;margin-top:7px" src="/images/InternetScan.gif"></span>
			</div>
		</td>
	  </tr>
	  <!-- end -->
  </table>
  </td>
</tr>  
</table>
</div>
</form>
</body>
</html>
