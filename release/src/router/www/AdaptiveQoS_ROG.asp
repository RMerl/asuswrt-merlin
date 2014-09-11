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
<title><#Web_Title#> - <#EZQoS#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link rel="stylesheet" type="text/css" href="device-map/device-map.css">
<link rel="stylesheet" type="text/css" href="rog.css">
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/client_function.js"></script>
<script>
// disable auto log out
AUTOLOGOUT_MAX_MINUTE = 0;
var $j = jQuery.noConflict();
var rogClientList = [];
var Param = {
	queryId: "",
	PEAK: 1024*100,
	selectedClient: ''
}

function initial(){
	show_menu();
	generateRogClientList();
}

function isSelected(mac){
	return (mac == Param.selectedClient) ? true : false;
}

function converPercent(val){
	return ((val/Param.PEAK)*100) < 1 ? ((val==0)?0:1) : ((val/Param.PEAK)*100) ; 
}

function converUnit(val){
	if(val > 1024*1024)
		return (val/(1024*1024)).toFixed(2) + " MB";
	else
		return (val/1024).toFixed(2) + " KB";
}

function updateBarPercent(mac){
	$j('#' + mac.replace(/:/g, "") + "_Traffic").html(
		retBarHTML(rogClientList[mac].devinfo.tx, "tx", "weight") +
		retBarHTML(rogClientList[mac].devinfo.rx, "rx", "weight") 
	);

	if(isSelected(mac)){
		$j('#' + mac.replace(/:/g, "") + "_Apps").html(
			retAppsDom(mac)
		);
	}

	$j(".barContainer div").each(function(){
		$j(this).css("width", converPercent($j(this).attr("title")) + "%");
	});
}

function retBarHTML(val, narrow, height){
	Param.PEAK = (val > Param.PEAK) ? val : Param.PEAK;

	var htmlCode = "";
	htmlCode += '<tr><td style="width:385px"><div class="barContainer ';
	htmlCode += height;
	htmlCode += '"><div title="';
	htmlCode += val;
	htmlCode += '" style="width:';
	htmlCode += converPercent(val);
	htmlCode += '%;" class="';
	htmlCode += height;
	htmlCode += '"></div></div></td><td style="text-align:right;"><div style="width:80px;">';
	htmlCode += converUnit(val);
	htmlCode += '</div></td><td style="width:20px;"><div class="' + narrow + 'Narrow"></div></td></tr>';
	return htmlCode;
}

function retAppsDom(macAddr){
	var appCode = '';
	var thisRogClient = rogClientList[macAddr];

	for(var app in thisRogClient){
		if(app == 'devinfo'){
			calTotalTraffic(thisRogClient.devinfo.tx, 'tx');
			calTotalTraffic(thisRogClient.devinfo.rx, 'rx');
			continue;
		}

		// init an APP
		appCode += '<div class="appTraffic"><table style="margin-left:50px;"><tr>';

		// Icon
		appCode += '<td style="width:70px;"><div class="appIcons" style="background-image:url(\'http://';
		appCode += clientList[macAddr].ip + ':' + clientList[macAddr].callback + '/' + thisRogClient[app].idx;
		appCode += '\');"></div></div></td>';

		// Name
		appCode += '<td class="dots appName" title="' + app + '">' + app + '</td>';

		// Traffic
		appCode += '<td class="dots" style="width:430px;"><div><table>';
		appCode += retBarHTML(thisRogClient[app].tx, "tx", "slim");
		appCode += retBarHTML(thisRogClient[app].rx, "rx", "slim");
		appCode += '</table></div></td>';

		// end
		appCode += '</tr></table></div>';
	}

	if(appCode == '')
		appCode = '<div class="appTraffic erHint dots">No Traffic in the list</div>';

	return $j(appCode);
}

function drawTraffic(){
	// clean up
	document.getElementById("appTrafficDiv").innerHTML = '';

	var clientCodeToObj;
	for(var i=0; i<rogClientList.length; i++){
		var clientCode = "";
		var thisClient = rogClientList[rogClientList[i]];

		// init a ROG Client
		clientCode += '<div id=' + rogClientList[i] + '><table><tr>';

		// Icon
		clientCode += '<td style="width:70px;"><div class="trafficIcons type';
		clientCode += clientList[rogClientList[i]].type;
		clientCode += '"></div></div></td>';

		// Name
		clientCode += '<td class="appName">' + clientList[rogClientList[i]].name + '</td>';

		// Traffic
		clientCode += '<td><div><table id="';
		clientCode += rogClientList[i].replace(/:/g, "");
		clientCode += '_Traffic"></table></div></td></tr></table></div>';

		// Apps Traffic Field
		clientCode += '<div id="'
		clientCode += rogClientList[i].replace(/:/g, "")
		clientCode += '_Apps"></div>';

		clientCodeToObj = $j(clientCode).click(function(){
			if(this.id.indexOf("Apps") != -1) return false;

			if(!isSelected(this.id)){				
				Param.selectedClient = this.id;
				$j(".appTraffic").remove();
				updateBarPercent(this.id);
			}
			else{
				Param.selectedClient = '';
				$j(".appTraffic").remove();
				calTotalTraffic(0, 'tx');
				calTotalTraffic(0, 'rx');
			}
		});

		$j("#appTrafficDiv").append(clientCodeToObj);
		$j("#appTrafficDiv").append('<div class="splitLine"></div>');

		updateBarPercent(rogClientList[i]);
	}

	if(document.getElementById("appTrafficDiv").innerHTML == '')
		document.getElementById("appTrafficDiv").innerHTML = '<div class="erHint" style="margin-top:20px">There is no ROG Client in the list</div>';
}

function updateClientInfo(target, mac){    
    var data = $j.getJSON(target, {format: "json"});

    data.success(function(msg){
		rogClientList[mac] = msg;
    });

    data.error(function(){
		rogClientList[mac] = {'devinfo':{rx:0,tx:0}};
    });

	updateBarPercent(mac);
}

function generateRogClientList(){
	if(clientList.length == 0){
		setTimeout("generateRogClientList();", 100);
		return false;
	}

	for(var i=0; i<clientList.length; i++){
		var clientObj = clientList[clientList[i]];

		if(clientObj.callback == "")
			continue;

		if(rogClientList.indexOf(clientObj.mac) == -1){
			rogClientList.push(clientObj.mac);
			rogClientList[clientObj.mac] = {'devinfo':{rx:0,tx:0}};
			updateClientInfo("http://" + clientList[clientObj.mac].ip + ":" + clientList[clientObj.mac].callback + "/callback.asp?output=netdev&jsoncallback=?", clientObj.mac);
		}
	}

	setTimeout("drawTraffic();", 500);
	
	Param.queryId = setInterval(function(){
		for(var i=0; i<rogClientList.length; i++){
			updateClientInfo("http://" + clientList[rogClientList[i]].ip + ":" + clientList[rogClientList[i]].callback + "/callback.asp?output=netdev&jsoncallback=?", rogClientList[i]);
		}
	}, 3000);
}

function calTotalTraffic(val, narrow){
	var traffic_mb = val/1024/1024;
	var angle = 0;
	var rotate = "";

	if(traffic_mb <= 1){
		angle = (traffic_mb*33) + (-123);
	}
	else if(traffic_mb > 1 && traffic_mb <= 5){
		angle = ((traffic_mb-1)/4)*32 + 33 +(-123);	
	}
	else if(traffic_mb > 5 && traffic_mb <= 10){
		angle = ((traffic_mb - 5)/5)*25 + (33 + 32) + (-123);
	}
	else if(traffic_mb > 10 && traffic_mb <= 20){
		angle = ((traffic_mb - 10)/10)*32 + (33 + 32 + 25) + (-123)
	}
	else if(traffic_mb > 20 && traffic_mb <= 30){
		angle = ((traffic_mb - 20)/10)*31 + (33 + 32 + 25 + 32) + (-123);	
	}
	else if(traffic_mb > 30 && traffic_mb <= 50){
		angle = ((traffic_mb - 30)/20)*28 + (33 + 32 + 25 + 32 + 31) + (-123);	
	}
	else if(traffic_mb > 50 && traffic_mb <= 75){
		angle = ((traffic_mb - 50)/25)*30 + (33 + 32 + 25 + 32 + 31 + 28) + (-123);
	}
	else if(traffic_mb > 75 && traffic_mb <= 100){
		angle = ((traffic_mb - 75)/25)*34 + (33 + 32 + 25 + 32 + 31 + 28 + 30) + (-123);
	}
	else{
		angle = 123;		
	}
	
	rotate = "rotate("+angle.toFixed(1)+"deg)";
	$j('#indicator_' + narrow).css({
		"-webkit-transform": rotate,
		"-moz-transform": rotate,
		"-o-transform": rotate,
		"msTransform": rotate,
		"transform": rotate
	});

	document.getElementById(narrow + '_speed').innerHTML = traffic_mb.toFixed(2);
}
</script>
</head>
<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="AdaptiveQoS_ROG.asp">
<input type="hidden" name="next_page" value="AdaptiveQoS_ROG.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="flag" value="">
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
									<div class="formfonttitle"><#Menu_TrafficManager#> - ROG First</div>

									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

									<div>
										<table style="width:99%;">
											<tr>
												<td style="width:50%;">
													<div class="formfonttitle" style="margin-bottom:0px;">Upload</div>
													<div class="trafficNum" id="tx_speed">0.00</div>
													<div class="speedMeter"></div>
													<div class="speedIndictor" id="indicator_tx"></div>
												</td>
												<td>	
													<div class="formfonttitle" style="margin-bottom:0px;">Download</div>
													<div class="trafficNum" id="rx_speed">0.00</div>
													<div class="speedMeter"></div>
													<div class="speedIndictor" id="indicator_rx"></div>
												</td>
											</tr>
										</table>	
									</div>

									<div id="appTrafficDiv">
										<div style="width: 100%;text-align: center;margin-top: 50px;">
											<img src="/images/InternetScan.gif" style="width: 50px;">
										</div>
									</div>
								</td>
							</tr>
						</table>
					</td>  
				</tr>
			</table>
		</td>		
	</tr>
</table>
<div id="footer"></div>
</body>
</html>
