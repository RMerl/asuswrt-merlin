<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>

<!--Advanced_Feedback.asp-->

<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#menu_feedback#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>

<script>
var orig_page = '<% get_parameter("origPage"); %>';
function initial(){
	show_menu();
	if(dsl_support){
		change_dsl_diag_enable(0);
		document.getElementById("fb_desc1").style.display = "";
		inputCtrl(document.form.fb_ptype, 0);
		inputCtrl(document.form.fb_pdesc, 0);
		
	}
	else{
		document.getElementById("fb_desc0").style.display = "";
		inputCtrl(document.form.fb_ISP, 0);
		inputCtrl(document.form.fb_Subscribed_Info, 0);
		document.form.attach_syslog_id.checked = true;
		document.form.attach_cfgfile_id.checked = true;
		document.form.attach_iptables.checked = false;
		document.form.attach_modemlog.checked = true;
		document.getElementById("attach_iptables_span").style.display = "none";		
		inputCtrl(document.form.dslx_diag_enable[0], 0);
		inputCtrl(document.form.dslx_diag_enable[1], 0);
		inputCtrl(document.form.dslx_diag_duration, 0);
		inputCtrl(document.form.fb_availability, 0);
		
		gen_ptype_list(orig_page);
		Reload_pdesc(document.form.fb_ptype,orig_page);
	}		

	if(modem_support == -1){
		document.form.attach_modemlog.checked = false;
		document.getElementById("attach_modem_span").style.display = "none";
	}

	setTimeout("check_wan_state();", 300);
}

function check_wan_state(){
	
	if(sw_mode != 3 && document.getElementById("connect_status").className == "connectstatusoff"){
		document.getElementById("fb_desc_disconnect").style.display = "";
		document.form.fb_country.disabled = "true";
		document.form.fb_email.disabled = "true";
		document.form.attach_syslog.disabled = "true";
		document.form.attach_cfgfile.disabled = "true";
		document.form.attach_modemlog.disabled = "true";
		document.form.fb_comment.disabled = "true";
		document.form.btn_send.disabled = "true";
		if(dsl_support){
			document.form.fb_ISP.disabled = "true";
			document.form.fb_Subscribed_Info.disabled = "true";
			document.form.attach_iptables.disabled = "true";
			document.form.dslx_diag_enable[0].disabled = "true";
			document.form.dslx_diag_enable[1].disabled = "true";
			document.form.dslx_diag_duration.disabled = "true";
			document.form.fb_availability.disabled = "true";
			
		}
		else{
			document.form.fb_ptype.disabled = "true";
			document.form.fb_pdesc.disabled = "true";
		}		
	}
	else{
		document.getElementById("fb_desc_disconnect").style.display = "none";
		document.form.fb_country.disabled = "";
		document.form.fb_email.disabled = "";
		document.form.attach_syslog.disabled = "";
		document.form.attach_modemlog.disabled = "";
		document.form.attach_cfgfile.disabled = "";
		document.form.fb_comment.disabled = "";
		document.form.btn_send.disabled = "";
		if(dsl_support){
			document.form.fb_ISP.disabled = "";
			document.form.fb_Subscribed_Info.disabled = "";
			document.form.attach_iptables.disabled = "";
			document.form.dslx_diag_enable[0].disabled = "";
			document.form.dslx_diag_enable[1].disabled = "";
			document.form.dslx_diag_duration.disabled = "";
			document.form.fb_availability.disabled = "";
			
		}
		else{
			document.form.fb_ptype.disabled = "";
			document.form.fb_pdesc.disabled = "";
		}
	}		
		
	setTimeout("check_wan_state();", 3000);
}

function gen_ptype_list(url){
	ptypelist = new Array();
	ptypelist.push(["<#Select_menu_default#> ...", "No_selected"]);
	ptypelist.push(["Setting Problem", "Setting_Problem"]);	
	ptypelist.push(["Connection/Speed Problem", "Connection_or_Speed_Problem"]);
	ptypelist.push(["Compatibility Problem", "Compatibility_Problem"]);
	ptypelist.push(["Translation of Suggestion", "Translation_of_Suggestion"]);
	ptypelist.push(["<#Adaptive_Others#>", "Other_Problem"]);
	free_options(document.form.fb_ptype);
	document.form.fb_ptype.options.length = ptypelist.length;
	for(var i = 0; i < ptypelist.length; i++){
		document.form.fb_ptype.options[i] = new Option(ptypelist[i][0], ptypelist[i][1]);
	}	
	if(url)		//with url : Setting Problem
		document.form.fb_ptype.options[1].selected = "1";
}

function Reload_pdesc(obj, url){
	//alert(obj.value+" ; "+url);
	free_options(document.form.fb_pdesc);
	var ptype = obj.value;
	desclist = new Array();
	url_group = new Array();
	desclist.push(["<#Select_menu_default#> ...","No_selected"]);
	url_group.push(["select"]);//false value
	if(ptype == "Setting_Problem"){
		desclist.push(["<#QIS#>","QIS"]);
		url_group.push(["QIS"]);

		desclist.push(["<#menu1#>","Network Map"]);
		url_group.push(["index"]);

		desclist.push(["<#Guest_Network#>","Guest Network"]);
		url_group.push(["Guest_network"]);

		desclist.push(["<#AiProtection_title#>","AiProtection"]);
		url_group.push(["AiProtection"]);

		desclist.push(["<#Adaptive_QoS#>","Adaptive QoS"]);	//5
		url_group.push(["AdaptiveQoS"]);

		desclist.push(["<#EzQoS_type_traditional#>","Traditional QoS"]);
		url_group.push(["AiProtection"]);

		desclist.push(["<#Menu_TrafficManager#>","<#Traffic_Analyzer#>"]);	/* untranslated */
		url_group.push(["TrafficMonitor"]);

		desclist.push(["<#Parental_Control#>","Parental Ctrl"]);
		url_group.push(["ParentalControl"]);

		desclist.push(["<#Menu_usb_application#>","USB Application"]);
		url_group.push(["APP_", "AiDisk", "aidisk", "mediaserver", "PrinterServer", "TimeMachine"]);

		desclist.push(["AiCloud","AiCloud"]);	//10
		url_group.push(["cloud"]);

		desclist.push(["<#menu5_1#>","Wireless"]);
		url_group.push(["ACL", "WAdvanced", "Wireless", "WMode", "WSecurity", "WWPS"]);

		desclist.push(["<#menu5_3#>","WAN"]);
		url_group.push(["WAN_", "PortTrigger", "VirtualServer", "Exposed", "NATPassThrough"]);

		desclist.push(["<#dualwan#>","Dual WAN"]);
		url_group.push(["WANPort"]);

		desclist.push(["<#menu5_2#>","LAN"]);
		url_group.push(["LAN", "DHCP", "GWStaticRoute", "IPTV", "SwitchCtrl"]);

		desclist.push(["<#menu5_4_4#>","USB dongle"]);	//15
		url_group.push(["Modem"]);

		desclist.push(["Download Master","DM"]);
		url_group.push(["DownloadMaster"]);//false value

		desclist.push(["<#menu5_3_6#>","DDNS"]);
		url_group.push(["DDNS"]);

		desclist.push(["IPv6","IPv6"]);
		url_group.push(["IPv6"]);

		desclist.push(["VPN","VPN"]);
		url_group.push(["VPN"]);

		desclist.push(["<#menu5_5#>","Firewall"]);	//20
		url_group.push(["Firewall", "KeywordFilter", "URLFilter"]);

		desclist.push(["<#menu5_6#>","Administration"]);
		url_group.push(["OperationMode", "System", "SettingBackup"]);

		desclist.push(["<#System_Log#>","System Log"]);
		url_group.push(["VPN"]);

		desclist.push(["<#Network_Tools#>","Network Tools"]);
		url_group.push(["Status_"]);

		desclist.push(["Rescue Mode","Rescue"]);
		url_group.push(["Rescue"]);//false value

		desclist.push(["With other network devices","Other Devices"]);	//25
		url_group.push(["Other_Device"]);//false value

		desclist.push(["Cannot access firmware page","Fail to access"]);
		url_group.push(["GUI"]);//false value

		desclist.push(["<#menu5_6_3#>","FW update"]);
		url_group.push(["FirmwareUpgrade"]);

	}
	else if(ptype == "Connection_or_Speed_Problem"){
		
		desclist.push(["Slow wireless speed","Wireless speed"]);
		desclist.push(["Slow wired speed","Wired speed"]);
		desclist.push(["Unstable connection problem","Unstable connection"]);
		desclist.push(["Router reboot automatically","Router reboot"]);
		
	}
	else if(ptype == "Compatibility_Problem"){
		
		desclist.push(["with modem","Compatible Problem"]);
		desclist.push(["with other router","Compatible Problem"]);
		desclist.push(["On OS or Application","Compatible Problem"]);
		desclist.push(["with printer","Compatible Problem"]);
		desclist.push(["with USB modem","Compatible Problem"]);
		desclist.push(["with external hardware disk","Compatible Problem"]);
		desclist.push(["with other network devices","Compatible Problem"]);

	}
	else if(ptype == "Translation_of_Suggestion"){
		
		desclist.splice(0,1);
		desclist.push(["Translation of Suggestion","Translation"]);		
	}
	else{	//Other_Problem
		
		desclist.splice(0,1);		
		desclist.push(["<#Adaptive_Others#>","others"]);
	}

	document.form.fb_pdesc.options.length = desclist.length;
	if(obj.value == "Setting_Problem" && url){
		for(var i = 0; i < url_group.length; i++){	
			document.form.fb_pdesc.options[i] = new Option(desclist[i][0], desclist[i][1]);
			//with url : Find pdesc in Setting Problem
			for(var j = 0; j < url_group[i].length; j++){
				if(url.search(url_group[i][j]) >= 0){					
					document.form.fb_pdesc.options[i].selected = "1";
				}
			}		
		}
	}
	else{
		for(var i = 0; i < desclist.length; i++){
			document.form.fb_pdesc.options[i] = new Option(desclist[i][0], desclist[i][1]);
		}	
	}
}

function updateUSBStatus(){	
	if(allUsbStatus.search("storage") == "-1"){			
		document.getElementById("storage_ready").style.display = "none";
		document.getElementById("be_lack_storage").style.display = "";
	}
	else{		
		document.getElementById("storage_ready").style.display = "";
		document.getElementById("be_lack_storage").style.display = "none";
	}					
}

function redirect(){
	document.location.href = "Feedback_Info.asp";
}

function applyRule(){
	//WAN connected check
	if(sw_mode != 3 && document.getElementById("connect_status").className == "connectstatusoff"){
                alert("<#USB_Application_No_Internet#>");
                return false;
        }
	else{

		/*if(document.form.feedbackresponse.value == "3"){
				alert("Feedback report daily maximum(10) send limit reached.");
				return false;
		}*/
		if(document.form.attach_syslog.checked == true)
			document.form.PM_attach_syslog.value = 1;
		else
			document.form.PM_attach_syslog.value = 0;
		if(document.form.attach_cfgfile.checked == true)
			document.form.PM_attach_cfgfile.value = 1;
		else
			document.form.PM_attach_cfgfile.value = 0;
		if(document.form.attach_modemlog.checked == true)
			document.form.PM_attach_modemlog.value = 1;
		else
			document.form.PM_attach_modemlog.value = 0;
		if(dsl_support){
			if(document.form.attach_iptables.checked == true)
				document.form.PM_attach_iptables.value = 1;
			else
				document.form.PM_attach_iptables.value = 0;
		}	
                
		if(document.form.fb_email.value == ""){
			if(!confirm("<#feedback_email_confirm#>")){
				document.form.fb_email.focus();
				return false;
			}
		}
		else{	//validate email
			if(!isEmail(document.form.fb_email.value)){
				alert("<#feedback_email_alert#>");    					
				document.form.fb_email.focus();
				return false;
			}
		}

		document.form.fb_browserInfo.value = navigator.userAgent;
		if(dsl_support){
			if(document.form.dslx_diag_enable[0].checked == true){
				document.form.action_wait.value="120";
				showLoading(120);
			}else	
				showLoading(60);
		}
		else
			showLoading(60);
		document.form.submit();
	}
}

function isEmail(strE) {
	if (strE.search(/^[A-Za-z0-9]+((-[A-Za-z0-9]+)|(\.[A-Za-z0-9]+)|(_[A-Za-z0-9]+))*\@[A-Za-z0-9]+((\.|-)[A-Za-z0-9]+)*\.[A-Za-z]+$/) != -1)
		return true;
	else
		return false;
}

function textCounter(field, cnt, upper) {
        if (field.value.length > upper)
                field.value = field.value.substring(0, upper);
        else
                cnt.value = upper - field.value.length;
}

function change_dsl_diag_enable(value) {
	if(value) {
		if(allUsbStatus.search("storage") == "-1"){
			alert("USB disk required in order to store the debug log, please plug-in a USB disk to <#Web_Title2#> and Enable DSL Line Diagnostic again.");
			document.form.dslx_diag_enable[1].checked = true;
			return;
		}
		else{
			alert("While debug log capture in progress, please do not unplug the USB disk as the debug log would be stored in the disk. UI top right globe icon flashing in yellow indicating that debug log capture in progress. Click on the yellow globe icon could cancel the debug log capture. Please note that xDSL line would resync in one minute after Feedback form submitted.");
		}
		showhide("dslx_diag_duration",1);
	}
	else {
		showhide("dslx_diag_duration",0);
	}
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
<div class="drImg"><img src="/images/alertImg.png"></div>
<div style="height:70px;"></div>
</td>
</tr>
</table>
</div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="current_page" value="Advanced_Feedback.asp">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_sendmail">
<input type="hidden" name="action_wait" value="60">
<input type="hidden" name="PM_attach_syslog" value="">
<input type="hidden" name="PM_attach_cfgfile" value="">
<input type="hidden" name="PM_attach_iptables" value="">	
<input type="hidden" name="PM_attach_modemlog" value="">
<input type="hidden" name="feedbackresponse" value="<% nvram_get("feedbackresponse"); %>">
<input type="hidden" name="fb_experience" value="<% nvram_get("fb_experience"); %>">
<input type="hidden" name="fb_browserInfo" value="">
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

<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
<tbody>
<tr>
<td bgcolor="#4D595D" valign="top" >
<div>&nbsp;</div>
<div class="formfonttitle"><#menu5_6#> - <#menu_feedback#></div>
<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
<div id="fb_desc0" class="formfontdesc" style="display:none;"><#Feedback_desc0#></div>
<div id="fb_desc1" class="formfontdesc" style="display:none;"><#Feedback_desc1#></div>
<div id="fb_desc_disconnect" class="formfontdesc" style="display:none;color:#FC0;">Now this function can't work, because your ASUS Router isn't connected to the Internet. Please send your Feedback to this email address : <a href="mailto:router_feedback@asus.com?Subject=<%nvram_get("productid");%>" target="_top" style="color:#FFCC00;">router_feedback@asus.com </a></div><!-- untranslated -->
<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
<tr>
<th width="30%"><#feedback_country#> *</th>
<td>
	<input type="text" name="fb_country" maxlength="30" class="input_25_table" value="" autocorrect="off" autocapitalize="off">
</td>
</tr>
<tr>
<th><#feedback_isp#> *</th>
<td>
	<input type="text" name="fb_ISP" maxlength="40" class="input_25_table" value="" autocorrect="off" autocapitalize="off">
</td>
</tr>
<tr>
<th>Name of the Subscribed Plan/Service/Package *</th>
<td>
	<input type="text" name="fb_Subscribed_Info" maxlength="50" class="input_25_table" value="" autocorrect="off" autocapitalize="off">
</td>
</tr>
<tr>
<th><#feedback_email#> *</th>
<td>
	<input type="text" name="fb_email" maxlength="50" class="input_25_table" value="" autocorrect="off" autocapitalize="off">
</td>
</tr>

<tr>
<th>Extra information for debugging *</th>
<td>
	<input type="checkbox" class="input" name="attach_syslog" id="attach_syslog_id"><label for="attach_syslog_id"><#System_Log#></label>&nbsp;&nbsp;&nbsp;
	<input type="checkbox" class="input" name="attach_cfgfile" id="attach_cfgfile_id"><label for="attach_cfgfile_id">Setting file</label>&nbsp;&nbsp;&nbsp;
	<span id="attach_iptables_span" style="color:#FFFFFF;"><input type="checkbox" class="input" name="attach_iptables" id="attach_iptables_id"><label for="attach_iptables_id">Iptable setting</label></span>
	<span id="attach_modem_span" style="color:#FFFFFF;"><input type="checkbox" class="input" name="attach_modemlog" id="attach_modemlog_id"><label for="attach_modemlog_id">3G/4G log</label></span>
</td>
</tr>

<tr>
	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(25,11);">Enable DSL Line Diagnostic *</a></th>
	<td>
		<input type="radio" name="dslx_diag_enable" class="input" value="1" onclick="change_dsl_diag_enable(1);"><#checkbox_Yes#>
		<input type="radio" name="dslx_diag_enable" class="input" value="0" onclick="change_dsl_diag_enable(0);" checked><#checkbox_No#>
		<br>	
		<span id="storage_ready" style="display:none;color:#FC0">* USB disk is ready.</span>
		<span id="be_lack_storage" style="display:none;color:#FC0">* No USB disk plug-in.</span>
	</td>
</tr>

<tr id="dslx_diag_duration">
	<th>Diagnostic debug log capture duration *</th>
	<td>
		<select id="" class="input_option" name="dslx_diag_duration">
			<option value="0" selected><#Auto#></option>
			<option value="3600">1 <#Hour#></option>
			<option value="18000">5 <#Hour#></option>
			<option value="43200">12 <#Hour#></option>
			<option value="86400">24 <#Hour#></option>
		</select>
	</td>
</tr>

<tr>
<th><#feedback_connection_type#></th>
<td>
	<select class="input_option" name="fb_availability">
		<option value="Not_selected"><#Select_menu_default#> ...</option>
		<option value="Stable_connection"><#feedback_stable#></option>
		<option value="Occasional_interruptions"><#feedback_Occasion_interrupt#></option>
		<option value="Frequent_interruptions"><#feedback_Frequent_interrupt#></option>
		<option value="Unavailable"><#feedback_usaul_unavailable#></option>
	</select>
</td>
</tr>

<tr>
<th><#feedback_problem_type#></th>
<td>
	<select class="input_option" name="fb_ptype" onChange="Reload_pdesc(this);">
		
	</select>
</td>
</tr>

<tr>
<th><#feedback_problem_desc#></th>
<td>
	<select class="input_option" name="fb_pdesc">
		
	</select>
</td>
</tr>

<tr>
	<th>
		<#feedback_comments#> *
	</th>
	<td>
		<textarea name="fb_comment" maxlength="2000" cols="55" rows="8" style="font-family:'Courier New', Courier, mono; font-size:13px;background:#475A5F;color:#FFFFFF;" onKeyDown="textCounter(this,document.form.msglength,2000);" onKeyUp="textCounter(this,document.form.msglength,2000)"></textarea>
		<span style="color:#FC0">Maximum of 2000 characters - characters left : </span>
		<input type="text" class="input_6_table" name="msglength" id="msglength" maxlength="4" value="2000" autocorrect="off" autocapitalize="off" readonly>
	</td>
</tr>

<tr align="center">
	<td colspan="2">
		<div style="margin-left:-680px;"><#feedback_optional#></div>
		<input class="button_gen" name="btn_send" onclick="applyRule()" type="button" value="Send"/>
	</td>	
</tr>

<tr>
	<td colspan="2">
		<strong><#FW_note#></strong>
		<ul>
			<li><#feedback_note1#></li>
			<li><#feedback_note2#></li>
			<li><#feedback_note3#></li>
		</ul>
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
</td>
<td width="10" align="center" valign="top">&nbsp;</td>
</tr>
</table>
<div id="footer"></div>
</body>

<!--Advanced_Feedback.asp-->
</html>


<td width="10" align="center" valign="top">&nbsp;</td>
</tr>
</table>
<div id="footer"></div>
</body>

<!--Advanced_Feedback.asp-->
</html>

