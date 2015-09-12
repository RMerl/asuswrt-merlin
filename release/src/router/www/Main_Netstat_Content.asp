<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge">
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Network_Tools#> - Netstat</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<style>
#ClientList_Block_PC{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	*margin-top:26px;
	margin-left:3px;
	*margin-left:-129px;
	width:255px;
	text-align:left;	
	height:auto;
	overflow-y:auto;
	z-index:200;
	padding: 1px;
	display:none;
}
#ClientList_Block_PC div{
	background-color:#576D73;
	height:auto;
	*height:20px;
	line-height:20px;
	text-decoration:none;
	font-family: Lucida Console;
	padding-left:2px;
}

#ClientList_Block_PC a{
	background-color:#EFEFEF;
	color:#FFF;
	font-size:12px;
	font-family:Arial, Helvetica, sans-serif;
	text-decoration:none;	
}
#ClientList_Block_PC div:hover, #ClientList_Block a:hover{
	background-color:#3366FF;
	color:#FFFFFF;
	cursor:default;
}	
</style>	
<script>
option_netstat = new Array("<#sockets_all#>","<#sockets_TCP#>","<#sockets_UDP#>","<#sockets_RAW#>","<#sockets_UNIX#>","<#sockets_listening#>","<#Display_routingtable#>");
optval_netstat = new Array("-a","-t","-u","-w","-x","-l","-r");
option_netstat_nat = new Array("<#Netstatnat_option1#>", "<#Netstatnat_option2#>", "<#Netstatnat_option3#>");
optval_netstat_nat = new Array("-L","-s","-S");
	
function key_event(evt){
	if(evt.keyCode != 27 || isMenuopen == 0) 
		return false;
	pullLANIPList(document.getElementById("pull_arrow"));
}	
	
function onSubmitCtrl(o, s) {
	if(validForm()){
			document.form.action_mode.value = s;
			updateOptions();
	}		
}

function init(){
	show_menu();
	showLANIPList();

	if(downsize_4m_support){
		for (var i = 0; i < document.form.cmdMethod.options.length; i++) {
			if (document.form.cmdMethod.options[i].value == "netstat-nat") {
				document.form.cmdMethod.options.remove(i);
				break;
			}
		}	
	}	
}

function updateOptions(){
	if(document.form.cmdMethod.value == "netstat-nat" && document.form.NetOption.value == "-s"){
		document.form.SystemCmd.value = "netstat-nat " + document.form.NetOption.value + " "
																		+ document.form.targetip.value + " " + document.form.ExtOption.value +" -n";
	}else if(document.form.cmdMethod.value == "netstat-nat"){
		document.form.SystemCmd.value = "netstat-nat " + document.form.NetOption.value + " "
																		+document.form.ExtOption.value +" -n";
	}else{
		document.form.SystemCmd.value = "netstat " + document.form.NetOption.value;
		if (document.form.ResolveName.value != "1")
			document.form.SystemCmd.value += " -n";
	}		
	document.form.submit();
	document.getElementById("cmdBtn").disabled = true;
	document.getElementById("cmdBtn").style.color = "#666";
	document.getElementById("loadingIcon").style.display = "";
	setTimeout("checkCmdRet();", 500);
}


var _responseLen;
var noChange = 0;
function checkCmdRet(){
       
	if (document.form.cmdMethod.value == "netstat-nat")
		waitLimit = 90;
	else
		waitLimit = 10;

	$.ajax({
		url: '/cmdRet_check.htm',
		dataType: 'html',
		
		error: function(xhr){
			setTimeout("checkCmdRet();", 1000);
		},
		success: function(response){
			var retArea = document.getElementById("textarea");
			var _cmdBtn = document.getElementById("cmdBtn");

			if(response.search("XU6J03M6") != -1){
				document.getElementById("loadingIcon").style.display = "none";
				_cmdBtn.disabled = false;
				_cmdBtn.style.color = "#FFF";
				retArea.value = response.replace("XU6J03M6", " ");
				retArea.scrollTop = retArea.scrollHeight;
				return false;
			}

			if(_responseLen == response.length)
				noChange++;
			else
				noChange = 0;

			if(noChange > waitLimit){
				document.getElementById("loadingIcon").style.display = "none";
				_cmdBtn.disabled = false;
				_cmdBtn.style.color = "#FFF";
				setTimeout("checkCmdRet();", 1000);
			}
			else{
				_cmdBtn.disabled = true;
				_cmdBtn.style.color = "#666";
				document.getElementById("loadingIcon").style.display = "";
				setTimeout("checkCmdRet();", 1000);
			}

			retArea.value = response;
			retArea.scrollTop = retArea.scrollHeight;
			_responseLen = response.length;
		}
	});
}

function hideCNT(obj){
	if(obj.value == "netstat-nat"){		
		document.getElementById("cmdDesc").innerHTML = "Display NAT connections.";
		addNetOption(document.form.NetOption, option_netstat_nat, optval_netstat_nat);
		append_value(document.form.NetOption);
		document.getElementById('ExtOption_tr').style.display = "";
		document.getElementById('resolvename').style.display = "none";
	}
	else{
		document.getElementById("cmdDesc").innerHTML = "<#NetworkTools_Info#>";	
		addNetOption(document.form.NetOption, option_netstat, optval_netstat);
		document.getElementById('ExtOption_tr').style.display = "none";
		document.getElementById('resolvename').style.display = "";
	}
}

function addNetOption(obj, opt, val){
	free_options(obj);
	for(i=0; i<opt.length; i++){
		if(opt[i].length > 0){
			obj.options[i] = new Option(opt[i], val[i]);
		}	
	}
}

function append_value(obj){
	if(document.form.cmdMethod.value == "netstat-nat"
			&& obj.value == "-s"){
				document.getElementById('targetip_tr').style.display = "";		
				document.getElementById('targetip').style.display = "";
	}else{
				document.getElementById('targetip_tr').style.display = "none";
	}
}

var over_var = 0;
var isMenuopen = 0;
function hideClients_Block(evt){
	if(typeof(evt) != "undefined"){
		if(!evt.srcElement)
			evt.srcElement = evt.target; // for Firefox

		if(evt.srcElement.id == "pull_arrow" || evt.srcElement.id == "ClientList_Block"){
			return;
		}
	}	
	
	document.getElementById("pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById('ClientList_Block_PC').style.display='none';
	isMenuopen = 0;
}

function pullLANIPList(obj){
	
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		document.getElementById("ClientList_Block_PC").style.display = 'block';		
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

function showLANIPList(){
	if(clientList.length == 0){
		setTimeout("showLANIPList();", 500);
		return false;
	}

	var htmlCode = "";
	for(var i=0; i<clientList.length;i++){
		var clientObj = clientList[clientList[i]];

		if(clientObj.ip == "offline") clientObj.ip = "";
		if(clientObj.name.length > 20) clientObj.name = clientObj.name.substring(0, 16) + "..";

		htmlCode += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\'';
		htmlCode += clientObj.ip;
		htmlCode += '\');"><strong>';
		htmlCode += clientObj.name;
		htmlCode += '</strong></div></a><!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
	}

	document.getElementById("ClientList_Block_PC").innerHTML = htmlCode;
}

function setClientIP(ipaddr){
	document.form.targetip.value = ipaddr;
	hideClients_Block();
	over_var = 0;
}

function validForm(){
	if(document.form.NetOption.value == "-s" && document.form.targetip.value == ""){
			alert("<#JS_fieldblank#>");        
      document.form.targetip.focus();
      document.form.targetip.select();
			return false;	
	}else{
			return true;	
	}
	
}
</script>
</head>
<body onkeydown="key_event(event);" onclick="if(isMenuopen){hideClients_Block(event)}" onload="init();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="GET" name="form" action="/apply.cgi" target="hidden_frame"> 
<input type="hidden" name="current_page" value="Main_Netstat_Content.asp">
<input type="hidden" name="next_page" value="Main_Netstat_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="SystemCmd" onkeydown="onSubmitCtrl(this, ' Refresh ')" value="">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

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
						<table width="760px" border="0" cellpadding="5" cellspacing="0" bordercolor="#6b8fa3"  class="FormTitle" id="FormTitle">		
							<tr>
								<td bgcolor="#4D595D" colspan="3" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle"><#Network_Tools#> - Netstat</div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div class="formfontdesc" id="cmdDesc"><#NetworkTools_Info#></div>
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
										<tr>
											<th width="20%"><#NetworkTools_Method#></th>
											<td>
												<select id="cmdMethod" class="input_option" name="cmdMethod" onchange="hideCNT(this);">
													<option value="netstat" selected>Netstat</option>
													<option value="netstat-nat">Netstat-nat</option>
 												</select>
											</td>										
										</tr>										
										<tr>
													<!-- client info -->
											<th width="20%"><#NetworkTools_option#></th>
											<td>
												<select id="NetOption" class="input_option" name="NetOption" onChange="append_value(this);">
													<option value="-a"><#sockets_all#></option>
													<option value="-t"><#sockets_TCP#></option>
													<option value="-u"><#sockets_UDP#></option>
													<option value="-w"><#sockets_RAW#></option>
													<option value="-x"><#sockets_UNIX#></option>
													<option value="-l"><#sockets_listening#></option>
													<option value="-r"><#Display_routingtable#></option>
 												</select>	
											</td>			
										</tr>
										<tr id="targetip_tr" style="display:none;">
											<th width="20%"><#NetworkTools_target#> IP</th>
											<td>
													<input type="text" id="targetip" class="input_15_table" maxlength="15" name="targetip" onKeyPress="return validator.isIPAddr(this,event)" onClick="hideClients_Block();" autocorrect="off" autocapitalize="off">
												<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;*margin-left:-3px;*margin-top:1px;" onclick="pullLANIPList(this);" title="<#select_device_name#>" onmouseover="over_var=1;" onmouseout="over_var=0;">
												<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>
											</td>										
										</tr>										
										<tr id="ExtOption_tr" style="display:none;">
											<th width="20%"><#NetworkTools_extended_option#></th>
											<td>
												<select id="ExtOption" class="input_option" name="ExtOption">
													<option value="-r state" selected><#Netstatnat_sort_state#></option>
													<option value="-r src"><#Netstatnat_sort_src#></option>
													<option value="-r dst"><#Netstatnat_sort_dst#></option>
													<option value="-r src-port"><#Netstatnat_sort_src_port#></option>
													<option value="-r dst-port"><#Netstatnat_sort_dst_port#></option>
 												</select>
											</td>										
										</tr>						
										<tr id = "resolvename" style="">
											<th width="20%"><#NetworkTools_ResolveName#></th>
											<td>
												<select id="ResolveName" class="input_option" name="ResolveName">
													<option value="0">No</option>
													<option value="1">Yes</option>
												</select>
											</td>
										</tr>
									</table>

									<div class="apply_gen">
										<input class="button_gen" id="cmdBtn" onClick="onSubmitCtrl(this, ' Refresh ')" type="button" value="Netstat">
										<img id="loadingIcon" style="display:none;" src="/images/InternetScan.gif"></span>
									</div>

									<div style="margin-top:8px" id="logArea">
										<textarea cols="63" rows="27" wrap="off" readonly="readonly" id="textarea" style="width:99%;font-family:Courier New, Courier, mono; font-size:11px;background:#475A5F;color:#FFFFFF;"><% nvram_dump("syscmd.log","syscmd.sh"); %></textarea>
									</div>
								</td>
							</tr>
						</table>
					</td>
				</tr>
			</table>
			<!--===================================Ending of Main Content===========================================-->
		</td>
		<td width="10" align="center" valign="top"></td>
	</tr>
</table>
</form>

<div id="footer"></div>
</body>
<script type="text/javascript">
<!--[if !IE]>-->
	(function($){
		var textArea = document.getElementById('textarea');
		textArea.scrollTop = textArea.scrollHeight;
	})(jQuery);
<!--<![endif]-->
</script>
</html>
