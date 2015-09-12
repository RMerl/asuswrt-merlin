<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN"
"http://www.w3.org/TR/html4/loose.dtd">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<link href="/form_style.css" rel="stylesheet" type="text/css" />
<link href="/NM_style.css" rel="stylesheet" type="text/css" />
<link href="/device-map/device-map.css" rel="stylesheet" type="text/css" />
<title>device-map/clients.asp</title>
<style>
p{
	font-weight: bolder;
}
.type0:hover{
	background-image:url('/images/New_ui/networkmap/client.png') !important;
	background-position:52% 70% !important;
}
.type6:hover{
	background-image:url('/images/New_ui/networkmap/client.png') !important;
	background-position:52% 70% !important;
}
.circle {
	position: absolute;
	width: 23px;
	height: 23px;
	border-radius: 50%;
	background: #333;
	margin-top: -77px;
	margin-left: 57px;
}
.circle div{
	height: 23px;
	text-align: center;
	margin-top: 4px;
}
.nav {
	display:none;
    float: left;
    width: 107%;
    margin-bottom: 30px;
    margin-top: -7px;
}
.nav ul{
    margin: 0;
    padding: 0;
    border-top:solid 2px #666;
}
.nav li{
	font-family:Arial;
    position: relative;
    float: left;
    color: #FFF;
    list-style: none;
    background:#4d595d;
    cursor:pointer;
    width: 100%;
}
.nav li:hover{
	background-color:#77A5C6;
}
.nav li a {
    display: block; 
    padding: 6px;      
    color: #FFF;
    border-bottom:solid 1px #666;
    text-decoration: none;
    cursor:pointer;
} 
.ipMethod{
	background-color: #222;
	font-size: 10px;
	font-family: monospace;
	padding: 2px 3px;
	border-radius: 3px;
}
.imgUserIcon{
	cursor: pointer;
	position: relative; 
	left: 15px; 
	width: 52px;
	height: 52px;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
}
</style>
<script type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/jquery.xdomainajax.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/tmmenu.js"></script>
<script>
if(parent.location.pathname.search("index") === -1) top.location.href = "../index.asp";

var wirelessOverFlag = false;
overlib.isOut = true;

var pagesVar = {
	curTab: "online",
	CLIENTSPERPAGE: 7,
	startIndex: 0,
	endIndex: 7, /* refer to startIndex + CLIENTSPERPAGE */
	startArray: [0],

	resetVar: function(){
		pagesVar.CLIENTSPERPAGE = 7;
		pagesVar.startIndex = 0;
		pagesVar.endIndex = pagesVar.startIndex + pagesVar.CLIENTSPERPAGE;
		pagesVar.startArray = [0];

		document.getElementById("select_wlclient_band").style.display = "none";
	}
}

var mapscanning = 0;

var clientMacUploadIcon = new Array();

function generate_wireless_band_list(){
	if(wl_nband_title.length == 1) return false;

	var code = '<ul>';
	for(var i=0; i<wl_nband_title.length; i++){
		code += '<li><a onclick="switchTab_drawClientList(\'';
		code += i;
		code += '\')">&nbsp;&nbsp;';
		code += wl_nband_title[i];
		code += '&nbsp;&nbsp;(<b style="font-size:11px;" id="liWirelessNum';
		code += i;
		code += '">0</b>)</a></li>';
	}
	code += '</ul>';

	document.getElementById('select_wlclient_band').innerHTML = code;
}

function initial(){
	parent.hideEditBlock();
	generate_wireless_band_list();
	updateClientList();
}

function convRSSI(val){
	if(val == "") return "wired";

	val = parseInt(val);
	if(val >= -50) return 4;
	else if(val >= -80)	return Math.ceil((24 + ((val + 80) * 26)/10)/25);
	else if(val >= -90)	return Math.ceil((((val + 90) * 26)/10)/25);
	else return 1;
}

function drawClientList(tab){
	var clientHtml = '<table width="100%" cellspacing="0" align="center"><tbody><tr><td>';
	var clientHtmlTd = '';
	var i = pagesVar.startIndex;
	//user icon
	var userIconBase64 = "NoIcon";

	if(typeof tab == "undefined"){
		tab = pagesVar.curTab;
	}
	genClientList();
	setClientListOUI();
	pagesVar.endIndex = pagesVar.startIndex + pagesVar.CLIENTSPERPAGE;
	while(i < pagesVar.endIndex){
		var clientObj = clientList[clientList[i]];	

		// fileter /*
		if(i > clientList.length-1) break;
		if(tab == 'online' && !clientObj.isOnline){i++; pagesVar.endIndex++; continue;}
		if((tab == 'wired' && clientObj.isWL != 0) || !clientObj.isOnline){i++; pagesVar.endIndex++; continue;}
		if((tab == 'wireless' && clientObj.isWL == 0) || !clientObj.isOnline){i++; pagesVar.endIndex++; continue;}
		if((tab == 'wireless0' && (clientObj.isWL == 0 || clientObj.isWL == 2 || clientObj.isWL == 3)) || !clientObj.isOnline){i++; pagesVar.endIndex++; continue;}
		if((tab == 'wireless1' && (clientObj.isWL == 0 || clientObj.isWL == 1 || clientObj.isWL == 3)) || !clientObj.isOnline){i++; pagesVar.endIndex++; continue;}
		if((tab == 'wireless2' && (clientObj.isWL == 0 || clientObj.isWL == 1 || clientObj.isWL == 2)) || !clientObj.isOnline){i++; pagesVar.endIndex++; continue;}
		if(tab == 'custom' && clientObj.from != "customList"){i++; pagesVar.endIndex++; continue;}
		var clientName = (clientObj.nickName == "") ? clientObj.name : clientObj.nickName;
		if(clientName.toLowerCase().indexOf(document.getElementById("searchingBar").value.toLowerCase()) == -1){i++; pagesVar.endIndex++; continue;}
		// filter */ 

		clientHtmlTd += '<div class="clientBg" onclick="popupCustomTable(\'' + clientObj.mac + '\');"><table width="100%" height="85px" border="0"><tr><td rowspan="3" width="85px">';
		if(usericon_support) {
			if(clientMacUploadIcon[clientObj.mac] == undefined) {
				var clientMac = clientObj.mac.replace(/\:/g, "");
				userIconBase64 = getUploadIcon(clientMac);
				clientMacUploadIcon[clientObj.mac] = userIconBase64;
			}
			else {
				userIconBase64 = clientMacUploadIcon[clientObj.mac];
			}
		}
		
		var deviceTitle = (clientObj.dpiDevice == "") ? clientObj.dpiVender : clientObj.dpiDevice;
		if(userIconBase64 != "NoIcon") {
			clientHtmlTd += '<div title="'+ deviceTitle + '"">';
			clientHtmlTd += '<img id="imgUserIcon_'+ i +'" class="imgUserIcon" src="' + userIconBase64 + '"';
			clientHtmlTd += '</div>';
		}
		else if( (clientObj.type != "0" && clientObj.type != "6") || clientObj.dpiVender == "") {
			clientHtmlTd += '<div class="clientIcon type';
			clientHtmlTd += clientObj.type;
			clientHtmlTd += '" title="';
			clientHtmlTd += deviceTitle;
			clientHtmlTd += '"></div>';
		}
		else if(clientObj.dpiVender != "") {
			var venderIconClassName = getVenderIconClassName(clientObj.dpiVender.toLowerCase());
			if(venderIconClassName != "") {
				clientHtmlTd += '<div class="venderIcon ';
				clientHtmlTd += venderIconClassName;
				clientHtmlTd += '" title="';
				clientHtmlTd += deviceTitle;
				clientHtmlTd += '"></div>';
			}
			else {
				clientHtmlTd += '<div class="clientIcon type';
				clientHtmlTd += clientObj.type;
				clientHtmlTd += '" title="';
				clientHtmlTd += deviceTitle;
				clientHtmlTd += '"></div>';
			}
		}

		clientHtmlTd += '</td><td style="height:30px;font-size:11px;word-break:break-all;"><div>';
		clientHtmlTd += clientName;
		clientHtmlTd += '</div></td>';
		
		clientHtmlTd += '<td style="width:55px">';
		if(!clientObj.internetState) {
			clientHtmlTd += '<div class="internetBlock" title="Block Internet access" style="height:20px;width:20px;margin-right:5px;float:right;"></div>';/*untranslated*/
		}

		if(clientObj.internetMode == "time") {
			clientHtmlTd += '<div class="internetTimeLimits" title="Time Scheduling" style="background-size:25px 20px;height:20px;width:25px;margin-right:5px;float:right;"></div>';/*untranslated*/
		}
		if(parent.sw_mode == 1){
			clientHtmlTd += '</td></tr><tr><td style="height:20px;" title=\'' + ipState[clientObj.ipMethod] + '\'>';
		}
		else {
			clientHtmlTd += '</td></tr><tr><td style="height:20px;">';
		}
		clientHtmlTd += (clientObj.isWebServer) ? '<a class="link" href="http://' + clientObj.ip + '" target="_blank">' + clientObj.ip + '</a>' : clientObj.ip;

		clientHtmlTd += '</td><td>';
		var rssi_t = 0;
		var connectModeTip = "";
		rssi_t = convRSSI(clientObj.rssi);
		if(isNaN(rssi_t))
			connectModeTip = "<#tm_wired#>";
		else {
			switch (rssi_t) {
				case 1:
					connectModeTip = "<#Radio#>: <#PASS_score1#>\n";
					break;
				case 2:
					connectModeTip = "<#Radio#>: <#PASS_score2#>\n";
					break;
				case 3:
					connectModeTip = "<#Radio#>: <#PASS_score3#>\n";
					break;
				case 4:
					connectModeTip = "<#Radio#>: <#PASS_score4#>\n";
					break;
			}
			if(stainfo_support) {
				if(clientObj.curTx != "")
					connectModeTip += "Tx Rate: " + clientObj.curTx + "\n";
				if(clientObj.curRx != "")
					connectModeTip += "Rx Rate: " + clientObj.curRx + "\n";
				connectModeTip += "<#Access_Time#>: " + clientObj.wlConnectTime + "";
			}
		}

		if(parent.sw_mode != 4) {
			clientHtmlTd += '<div style="height:28px;width:28px;float:right;margin-right:5px;margin-bottom:-20px;">';
			clientHtmlTd += '<div class="radioIcon radio_' + rssi_t +'" title="' + connectModeTip + '"></div>';
			if(clientObj.isWL != 0) {
				var bandClass = (navigator.userAgent.toUpperCase().match(/CHROME\/([\d.]+)/)) ? "band_txt_chrome" : "band_txt";
				clientHtmlTd += '<div class="band_block"><span class='+bandClass+'>' + wl_nband_title[clientObj.isWL-1].replace("Hz", "") + '</span></div>';
			}
			clientHtmlTd += '</div>';
		}

		clientHtmlTd += '</td></tr>';
		clientHtmlTd += '<tr><td colspan="2"><div style="margin-top:-15px;width:140px;" class="link" onclick="oui_query(\'';
		clientHtmlTd += clientObj.mac;
		clientHtmlTd += '\');event.cancelBubble=true;return overlib(\'';
		clientHtmlTd += retOverLibStr(clientObj);
		clientHtmlTd += '\', HAUTO, VAUTO);" onmouseout="nd();">';
		clientHtmlTd += clientObj.mac;
		clientHtmlTd += '</td></tr></table></div>';

		// display how many clients that hide behind a repeater.
		if(clientObj.macRepeat > 1){
			clientHtmlTd += '<div class="circle"><div>';
			clientHtmlTd += clientObj.macRepeat;
			clientHtmlTd += '</div></div>';
		}

		i++;
	}

	if(clientHtmlTd == ''){
		if(networkmap_fullscan == 1)
			clientHtmlTd = '<div style="color:#FC0;height:30px;text-align:center;margin-top:15px"><#Device_Searching#><img src="/images/InternetScan.gif"></div>';
		else
			clientHtmlTd = '<div style="color:#FC0;height:30px;text-align:center;margin-top:15px"><#IPConnection_VSList_Norule#></div>';
	}

	clientHtml += clientHtmlTd;
	clientHtml += '</td></tr></tbody></table>';
	document.getElementById("client_list_Block").innerHTML = clientHtml;

	// page switcher
	document.getElementById("leftBtn").style.visibility = (pagesVar.startIndex == 0) ? "hidden" : "visible";
	document.getElementById("rightBtn").style.visibility = (pagesVar.endIndex >= clientList.length) ? "hidden" : "visible";

	// Wired
	document.getElementById("tabWired").style.display = (totalClientNum.wired == 0) ? "none" : "";
	document.getElementById("tabWiredNum").innerHTML = 	totalClientNum.wired;

	// Wireless
	document.getElementById("tabWireless").style.display = (totalClientNum.wireless == 0) ? "none" : "";
	document.getElementById("tabWirelessNum").innerHTML = totalClientNum.wireless;
	if(totalClientNum.wireless == 0) 
		wirelessOverFlag = false;

	if(wl_nband_title.length > 1){
		for(var i=0; i<wl_nband_title.length; i++){
			document.getElementById("liWirelessNum" + i).innerHTML = totalClientNum.wireless_ifnames[i];
		}
	}

	if(typeof tab.split("wireless")[1] == 'undefined' || tab.split("wireless")[1] == '' || tab.split("wireless")[1] == 'NaN'){
		if(!wirelessOverFlag)
			document.getElementById("select_wlclient_band").style.display = "none";
		document.getElementById("searchingBar").placeholder = 'Search';
	}
	else{
		document.getElementById("searchingBar").placeholder = '[' + wl_nband_title[tab.split("wireless")[1]] + '](' + totalClientNum.wireless_ifnames[tab.split("wireless")[1]] + ')';
	}

	if(pagesVar.curTab != tab){
		document.getElementById("client_list_Block").style.display = 'none';
		$("#client_list_Block").fadeIn(300);
		pagesVar.curTab = tab;
	}

	$(".circle").mouseover(function(){
		return overlib(this.firstChild.innerHTML + " clients are connecting to <% nvram_get("productid"); %> through this device.");
	});

	$(".circle").mouseout(function(){
		nd();
	});
}

function updatePagesVar(direction){
	if(typeof direction != "undefined"){
		if(direction == '-'){
			pagesVar.startArray.length--;
			pagesVar.startIndex = pagesVar.startArray.slice(-1)[0];
			pagesVar.endIndex = pagesVar.startIndex + pagesVar.CLIENTSPERPAGE;
		}
		else if(direction == '+'){
			pagesVar.startIndex = pagesVar.endIndex;
			pagesVar.endIndex = pagesVar.startIndex + pagesVar.CLIENTSPERPAGE;
			pagesVar.startArray.push(pagesVar.startIndex);
		}

		pagesVar.startIndex = (pagesVar.startIndex < 0) ? 0 : pagesVar.startIndex;
		pagesVar.endIndex = (pagesVar.endIndex > clientList.length) ? clientList.length : pagesVar.endIndex;
		pagesVar.endIndex = (pagesVar.endIndex < pagesVar.CLIENTSPERPAGE) ? pagesVar.CLIENTSPERPAGE : pagesVar.endIndex;
	}
		
	drawClientList(pagesVar.curTab);
}

function retOverLibStr(client){
	var overlibStr = "<p><#MAC_Address#>:</p>" + client.mac.toUpperCase();

	if(client.ssid)
		overlibStr += "<p>SSID:</p>" + client.ssid.replace(/"/g, '&quot;');
	if(client.isLogin)
		overlibStr += "<p>Logged In User:</p>YES";
	if(client.isPrinter)
		overlibStr += "<p><#Device_service_Printer#></p>YES";
	if(client.isITunes)
		overlibStr += "<p><#Device_service_iTune#></p>YES";
	if(client.isWL > 0){
		overlibStr += "<p><#Wireless_Radio#>:</p>" + wl_nband_title[client.isWL-1] + " (" + client.rssi + " dBm)";
		if(stainfo_support) {
			overlibStr += "<p>Tx Rate:</p>" + ((client.curTx != "") ? client.curTx : "-");
			overlibStr += "<p>Rx Rate:</p>" + ((client.curRx != "") ? client.curRx : "-");
			overlibStr += "<p><#Access_Time#>:</p>" + client.wlConnectTime;
		}
	}
	return overlibStr;
}

function ajaxCallJsonp(target){    
    var data = $j.getJSON(target, {format: "json"});

    data.success(function(msg){
    	parent.retObj = msg;
		parent.db("Success!");
    });

    data.error(function(msg){
		parent.db("Error on fetch data!")
    });
}

function popupCustomTable(mac){
	parent.popupEditBlock(clientList[mac]);
}

function updateClientList(e){
	$.ajax({
		url: '/update_clients.asp',
		dataType: 'script', 
		error: function(xhr) {
			setTimeout("updateClientList();", 1000);
		},
		success: function(response){
			document.getElementById("loadingIcon").style.visibility = (networkmap_fullscan == 1 && parent.manualUpdate) ? "visible" : "hidden";

			if(isJsonChanged(originData, originDataTmp) || originData.fromNetworkmapd == ""){
				drawClientList();
				parent.show_client_status(totalClientNum.online);
			}

			if(networkmap_fullscan == 0) parent.manualUpdate = false; 
			setTimeout("updateClientList();", 3000);				
		}    
	});
}
</script>
</head>

<body class="statusbody" onload="initial();">
<iframe name="applyFrame" id="applyFrame" src="" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="form" id="refreshForm" action="/apply.cgi" target="applyFrame">
<input type="hidden" name="action_mode" value="refresh_networkmap">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="current_page" value="device-map/clients.asp">
<input type="hidden" name="next_page" value="device-map/clients.asp">
</form>

<table width="320px" border="0" cellpadding="0" cellspacing="0">
	<tr>
		<td>		
			<table width="100px" border="0" align="left" style="margin-left:8px;" cellpadding="0" cellspacing="0">
				<td>
					<div id="tabOnline" class="tabclick_NW" align="center">
						<span>
							Online
						</span>
					</div>
					<script>
						document.getElementById('tabOnline').onclick = function(){
							pagesVar.resetVar();
							drawClientList('online');
							document.getElementById('tabOnline').className = 'tabclick_NW';
							document.getElementById('tabWired').className = 'tab_NW';
							document.getElementById('tabWireless').className = 'tab_NW';
							document.getElementById('tabCustom').className = 'tab_NW';
						}
					</script>
				</td>
				<td>
					<div id="tabWired" class="tab_NW" align="center" style="display:none">
						<span>
							Wired (<b style="font-size:10px;" id="tabWiredNum">0</b>)
						</span>
					</div>
					<script>
						document.getElementById('tabWired').onclick = function(){
							pagesVar.resetVar();
							drawClientList('wired');
							document.getElementById('tabOnline').className = 'tab_NW';
							document.getElementById('tabWired').className = 'tabclick_NW';
							document.getElementById('tabWireless').className = 'tab_NW';
							document.getElementById('tabCustom').className = 'tab_NW';
						}
					</script>
				</td>
				<td>
					<div id="tabWireless" class="tab_NW" align="center" style="display:none">											
    					<span id="tabWirelessSpan">
							Wireless (<b style="font-size:10px;" id="tabWirelessNum">0</b>)
						</span>
						<nav class="nav" id="select_wlclient_band"></nav>    
					</div>
					<script>
						function switchTab_drawClientList(wband){
							pagesVar.resetVar();
							drawClientList('wireless' + wband);
							document.getElementById('tabOnline').className = 'tab_NW';
							document.getElementById('tabWired').className = 'tab_NW';
							document.getElementById('tabWireless').className = 'tabclick_NW';
							document.getElementById('tabCustom').className = 'tab_NW';
						}

						$('#tabWirelessSpan').click(function(){
							switchTab_drawClientList('');
						});

						$('#tabWireless').mouseenter(function(){
							if(wl_nband_title.length > 0){
								$("#select_wlclient_band").slideDown("fast", function(){});
								wirelessOverFlag = true;
							}
						});

						$('#tabWireless').mouseleave(function(){
							$("#select_wlclient_band").css({"display": "none"});
							wirelessOverFlag = false;
						});
					</script>
				</td>
				<td>
					<div id="tabCustom" class="tab_NW" align="center" style="display:none">
						<span>
							History (<b style="font-size:10px;" id="tabCustomNum">0</b>)
						</span>
					</div>
					<script>
						document.getElementById('tabCustom').onclick = function(){
							pagesVar.resetVar();
							drawClientList('custom');
							document.getElementById('tabOnline').className = 'tab_NW';
							document.getElementById('tabWired').className = 'tab_NW';
							document.getElementById('tabWireless').className = 'tab_NW';
							document.getElementById('tabCustom').className = 'tabclick_NW';
						}
					</script>
				</td>
				<td>
				</td>
			</table>
		</td>
	</tr>

	<tr>
		<td>				
			<table width="95%" border="0" align="center" cellpadding="4" cellspacing="0" style="background-color:#4d595d;">
  				<tr>
    				<td style="padding:3px 3px 5px 5px;">
						<input type="text" placeholder="Search" id="searchingBar" class="input_25_table" style="width:96%;margin-top:3px;margin-bottom:3px" maxlength="" value="" autocorrect="off" autocapitalize="off">
						<script>
							document.getElementById('searchingBar').onkeyup = function(){
								pagesVar.resetVar();
								drawClientList();
							}
						</script>

						<div id="client_list_Block"></div>
	    			</td>
  				</tr>
 			</table>
		</td>
	</tr>
</table>

<br/>
<img height="25" id="leftBtn" onclick="updatePagesVar('-');" style="cursor:pointer;margin-left:10px;" src="/images/arrow-left.png">
<input type="button" id="refresh_list" class="button_gen" value="<#CTL_refresh#>" style="margin-left:70px;">
	<script>
		document.getElementById('refresh_list').onclick = function(){
			parent.manualUpdate = true;
			document.form.submit();
		}
	</script>
<img src="/images/InternetScan.gif" id="loadingIcon" style="visibility:hidden">
<img height="25" id="rightBtn" onclick="updatePagesVar('+');" style="cursor:pointer;margin-left:25px;" src="/images/arrow-right.png">
</body>
</html>
