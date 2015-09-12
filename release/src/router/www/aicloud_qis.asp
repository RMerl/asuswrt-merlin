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
<title>Check AiCloud 2.0</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<!--<script type="text/javascript" src="/popup.js"></script>-->
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script language="JavaScript" type="text/javascript" src="/md5.js"></script>
<!--<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>-->
<script>
var enable_webdav = '<% nvram_get("enable_webdav"); %>';
var ddns_enable_x = '<% nvram_get("ddns_enable_x"); %>';
var ddns_hostname_x = '<% nvram_get("ddns_hostname_x"); %>';
var webdav_https_port = '<% nvram_get("webdav_https_port"); %>';
var macAddr = '<% nvram_get("lan_hwaddr"); %>'.toUpperCase().replace(/:/g, "");
var MD5DDNSName = "A"+hexMD5(macAddr).toUpperCase()+".asuscomm.com";
var restart_time = 0;



<% wanlink(); %>

var process_times = 0;
var current_process_times = 0;

function initial(){
	
	var ip_type = valid_is_wan_ip(wanlink_ipaddr());
	
	document.getElementById("process_status").style.display = "none";
	
	if( enable_webdav == "1" ){
		
		//- public ip
		if( ip_type == 1 ){
			if( ddns_hostname_x != "" && ddns_enable_x == "1" ){
				document.getElementById("process_status").style.display = "block";
				document.getElementById("proceeding_main_txt").innerHTML = 'Complete...';
				
				setTimeout("open_aicloud()", restart_time*1000);
			}
			else if( ddns_hostname_x != "" && ddns_enable_x == "0" ){
				document.getElementById("process_status").style.display = "block";
				document.getElementById("proceeding_main_txt").innerHTML = 'Enable DDNS...';
				
				enable_ddns();
			}
			else{
				document.getElementById("process_status").style.display = "block";
				document.getElementById("proceeding_main_txt").innerHTML = 'Register DDNS...';
				
				register_ddns();
			}
		}
		else{
			document.getElementById("process_status").style.display = "none";
			document.getElementById("aicloud_learn_more").style.display = "none";
			if(tmo_support)
				var theUrl = "cellspot.router"; 
			else
				var theUrl = "router.asus.com";
			document.getElementById("aicloud_main_text").innerHTML = "<#AiCloud_maintext_note0#>"+ theUrl +"<#AiCloud_maintext_note1#>";
		}
	}
	else{
		process_times = 3;
		current_process_times = 0;
		
		document.getElementById("proceeding_main_txt").innerHTML = 'Enable aicloud...';
		document.getElementById("process_status").style.display = "block";
		
		enable_aicloud();
		
		setTimeout("onProcessTimer();", 1000);
	}
}

function onProcessTimer(){
	
	current_process_times++;
		
	if( current_process_times <= process_times ){
		document.getElementById("proceeding_txt").innerHTML = ", " + Math.floor((current_process_times/process_times)*100) + "%";
		setTimeout("onProcessTimer();", 1000);
	}
	else
		window.location.reload();
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
        	window.location.reload();
      }
   });
}

function valid_is_wan_ip(ip_obj){
  // test if WAN IP is a private IP.
  var A_class_start = inet_network("10.0.0.0");
  var A_class_end = inet_network("10.255.255.255");
  var B_class_start = inet_network("172.16.0.0");
  var B_class_end = inet_network("172.31.255.255");
  var C_class_start = inet_network("192.168.0.0");
  var C_class_end = inet_network("192.168.255.255");
  
  var ip_num = inet_network(ip_obj);
  if(ip_num > A_class_start && ip_num < A_class_end)
		return 0;
  else if(ip_num > B_class_start && ip_num < B_class_end)
		return 0;
  else if(ip_num > C_class_start && ip_num < C_class_end)
		return 0;
	else if(ip_num == 0)
		return 0;
  else
		return 1;
}

function inet_network(ip_str){
	if(!ip_str)
		return -1;
	
	var re = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/;
	if(re.test(ip_str)){
		var v1 = parseInt(RegExp.$1);
		var v2 = parseInt(RegExp.$2);
		var v3 = parseInt(RegExp.$3);
		var v4 = parseInt(RegExp.$4);
		
		if(v1 < 256 && v2 < 256 && v3 < 256 && v4 < 256)
			return v1*256*256*256+v2*256*256+v3*256+v4;
	}
	return -2;
}

function enable_aicloud(){
	document.form.webdav_aidisk.value = 1;
	document.form.enable_webdav.value = 1;
	document.form.webdav_proxy.value = 1;
	FormActions("", "apply", "restart_webdav", "3");
	document.form.submit();
}

function enable_ddns(){
	document.form.ddns_enable_x.value = 1;
	document.form.ddns_return_code_chk.value = "";	
	FormActions("", "apply", "adm_asusddns_register", "3");
	document.form.submit();
}

function register_ddns(){
	document.form.ddns_enable_x.value = 1;
	document.form.ddns_server_x.value = "WWW.ASUS.COM";
	document.form.ddns_hostname_x.value = MD5DDNSName;	
	document.form.ddns_return_code_chk.value = "";	
	FormActions("", "apply", "adm_asusddns_register", "3");
	document.form.submit();
}

function open_aicloud(){
	window.location.href = "http://www.asusrouter.com/aicloud.html?v=https://" + ddns_hostname_x + ":" + webdav_https_port;
}

function restart_needed_time(second){
	restart_time = second;
}
</script>
</head>
<body onload="initial();" onunload="return unload_body();">

<div id="hiddenMask" class="popup_bg" style="z-index:999;">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
	</table>
	<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>
	
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
	<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
	<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
	<input type="hidden" name="current_page" value="aicloud_enable_check.asp">
	<input type="hidden" name="next_page" value="cloud_main.asp">
	<input type="hidden" name="action_mode" value="">
	<input type="hidden" name="action_script" value="">
	<input type="hidden" name="action_wait" value="">
	<input type="hidden" name="enable_cloudsync" value="<% nvram_get("enable_cloudsync"); %>">
	<input type="hidden" name="enable_webdav" value="<% nvram_get("enable_webdav"); %>">
	<input type="hidden" name="webdav_aidisk" value="<% nvram_get("webdav_aidisk"); %>">
	<input type="hidden" name="webdav_proxy" value="<% nvram_get("webdav_proxy"); %>">
	<input type="hidden" name="ddns_enable_x" value="<% nvram_get("ddns_enable_x"); %>">
	<input type="hidden" name="ddns_server_x" value="<% nvram_get("ddns_server_x"); %>">
	<input type="hidden" name="ddns_hostname_x" value="<% nvram_get("ddns_hostname_x"); %>">
	<input type="hidden" name="ddns_return_code_chk" value="<% nvram_get("ddns_return_code_chk"); %>">
	
	<div style="width:95%;height:500px;position:relative;padding:10px;font-style:italic;font-size:14px;background-color:#21333E;color:#fff">
		<div style="background-image:url('/images/aicloud_logo.png');width:250px;height:50px;left:110px;top:80px;position:absolute;"></div>
		<div id="aicloud_main_text" style="width:320px;position:absolute;top:150px;left:120px;font-size:18px;">
			<#AiCloud_maintext_note#>
		</div>
		<div id="aicloud_learn_more" style="position:absolute;top:400px;left:370px">
			<a href="#" style="font-weight: bolder;text-decoration: underline;" target="_blank"><#Learn_more#></a>
		</div>
		
		<div id="process_status" style="position:absolute;top:250px;left:570px;font-size:24px;display:none">		
			<table cellpadding="5" cellspacing="0" id="loadingBlock" class="loadingBlock" align="center">
				<tr>
					<td width="20%" height="80" align="center"><img id="process_status_image" src="/images/loading.gif"></td>
					<td><span id="proceeding_main_txt"></span><span id="proceeding_txt" style="color:#FFFFCC;"></span></td>
				</tr>
			</table>
		</div>
		
	</div>
		
</form>
<% update_variables(); %>
</body>
</html>
