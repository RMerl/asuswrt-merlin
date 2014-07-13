var selectedClientOrder;

function getclients_noMonitor(flag){							// return the number of the client
	var clients = new Array();
	var wired_client_num = 0;

	// get wired clients  2008.12 magic  read arp table
	for(var i = 0, wired_client_num = 0; i < arps.length; ++i, ++wired_client_num){
		clients[i] = new Array(5);
		//clients[i][0] = "<#CTL_unknown#>";	// hostname
		clients[i][0] = "";	// hostname
		clients[i][1] = arps[i][0];	// ip
		clients[i][2] = arps[i][3];	// MAC
		clients[i][3] = "";	
		clients[i][4] = null;
		clients[i][5] = null;
		clients[i][6] = "0";
		clients[i][7] = "0";
		clients[i][8] = "0";
	}
	
	// get the hostnames and IPs of the clients
	for(var i = 0; i < clients.length; ++i){
		// a client can have more leases and the later the newer.
		for(var j = leases.length-1; j >= 0; --j){
			if(leases[j][3] == "Expired")
				continue;
			
			if(clients[i][2] == leases[j][1]){
				// get hostname
				if(leases[j][0].length > 0)
					clients[i][0] = leases[j][0];
				
				// get ip
				if(leases[j][2].length > 0)
					clients[i][1] = leases[j][2];
				
				break;
			}
		}
		
		// a MAC can have more ip and the fronter the newer.
		for(var j = 0; j < arps.length; ++j){
			if(clients[i][2] == arps[j][3]){
				// get ip
				if(arps[j][0].length > 0)
					clients[i][1] = arps[j][0];
				
				break;
			}
		}
		// 2008.12 magic which is the wireless clients.
		for(var j = 0; j < wireless.length; ++j){
			if(clients[i][2] == wireless[j][0]){
				clients[i][3] = 10;	// 10 is meant the client is wireless.
				
				clients[i][4] = new Array(2);
				clients[i][4][0] = wireless[j][1];
				clients[i][4][1] = wireless[j][2];
				
				if(clients[i][4][0] == "Yes")
					clients[i][4][0] = "Associated";
				else
					clients[i][4][0] = "Disassociated";
				
				break;
			}
		}

		// judge which is the local device.
		if(clients[i][1] == login_ip_str()
				&& clients[i][2] == login_mac_str()){
			if(clients[i][0] == null || clients[i][0].length <= 0)
				clients[i][0] = "<#CTL_localdevice#>";
			else
				clients[i][0] += "(<#CTL_localdevice#>)";
		}
		
		if(flag == 1)
			clients[i][2] = simplyMAC(clients[i][2]);
	}
	
	return clients;
}

function getclients_Monitor(flag){
	var clients = new Array();
	
	for(var i = 0; i < ipmonitor.length; ++i){
		clients[i] = new Array(9);
								
		clients[i][0] = ipmonitor[i][2];  // Device name
		
		for(var j = leases.length-1; j >= 0; --j){
			if(leases[j][3] == "Expired")
				continue;
			
			if(ipmonitor[i][0] == leases[j][2]){
				if(ipmonitor[i][2] != leases[j][0]){
					clients[i][0] = leases[j][0];
				}
				else{
					clients[i][0] = ipmonitor[i][2];
				}
			}
		}			
		clients[i][1] = ipmonitor[i][0];	// IP
		clients[i][2] = ipmonitor[i][1];	// MAC
		clients[i][3] = null;	// if this is a wireless client.
		clients[i][4] = null;	// wireless information.
		clients[i][5] = ipmonitor[i][3];	// TYPE
		clients[i][6] = ipmonitor[i][4];	// if there's the HTTP service.
		clients[i][7] = ipmonitor[i][5];	// if there's the Printer service.
		clients[i][8] = ipmonitor[i][6];	// if there's the iTune service.
		
		for(var j = 0; j < wireless.length; ++j){
			if(clients[i][2] == wireless[j][0]){
				clients[i][3] = 10;	// 10 is meant the client is wireless.
				
				clients[i][4] = new Array(2);
				clients[i][4][0] = wireless[j][1];
				clients[i][4][1] = wireless[j][2];
				
				if(clients[i][4][0] == "Yes")
					clients[i][4][0] = "Associated";
				else
					clients[i][4][0] = "Disassociated";
				
				break;
			}
		}
		
		// judge which is the local device.
		if(clients[i][1] == login_ip_str()
				&& clients[i][2] == login_mac_str()){
			if(clients[i][0] == null || clients[i][0].length <= 0)
				clients[i][0] = "<#CTL_localdevice#>";
			else
				clients[i][0] += "(<#CTL_localdevice#>)";
		}
		
		if(flag == 1)
			clients[i][2] = simplyMAC(clients[i][2]);
	}
	
	return clients;
}

function getclients(flag){
	if(networkmap_fullscan == "done")
		return getclients_Monitor(flag);
	else
		return getclients_noMonitor(flag);
}

function simplyMAC(fullMAC){
	var ptr;
	var tempMAC;
	var pos1, pos2;
	
	ptr = fullMAC;
	tempMAC = "";
	pos1 = pos2 = 0;
	
	for(var i = 0; i < 5; ++i){
		pos2 = pos1+ptr.indexOf(":");
		
		tempMAC += fullMAC.substring(pos1, pos2);
		
		pos1 = pos2+1;
		ptr = fullMAC.substring(pos1);
	}
	
	tempMAC += fullMAC.substring(pos1);
	
	return tempMAC;
}

function test_all_clients(clients){
	var str = "";
	var Row;
	var Item;
	
	str += clients.length+"\n";
	
	Row = 1;
	for(var i = 0; i < clients.length; ++i){
		if(Row == 1)
			Row = 0;
		else
			str += "\n";
		
		Item = 1;
		for(var j = 0; j < 9; ++j){
			if(Item == 1)
				Item = 0;
			else
				str += ", ";
			
			str += clients[i][j];
		}
		
		str += "\n";
	}
	
	alert(str);
}

/* get client info form dhcp lease log */
loadXMLDoc("/getdhcpLeaseInfo.asp");
var _xmlhttp;
function loadXMLDoc(url){
	if(parent.sw_mode != 1) return false;
	var ie = window.ActiveXObject;
	if(ie)
		_loadXMLDoc_ie(url);
	else
		_loadXMLDoc(url);
}

var _xmlDoc_ie;
function _loadXMLDoc_ie(file){
	_xmlDoc_ie = new ActiveXObject("Microsoft.XMLDOM");
	_xmlDoc_ie.async = false;
	if (_xmlDoc_ie.readyState==4){
		_xmlDoc_ie.load(file);
		setTimeout("parsedhcpLease(_xmlDoc_ie);", 1000);
	}
}

function _loadXMLDoc(url) {
	_xmlhttp = new XMLHttpRequest();
	if (_xmlhttp && _xmlhttp.overrideMimeType)
		_xmlhttp.overrideMimeType('text/xml');
	else
		return false;

	_xmlhttp.onreadystatechange = state_Change;
	_xmlhttp.open('GET', url, true);
	_xmlhttp.send(null);
}

function state_Change(){
	if(_xmlhttp.readyState==4){// 4 = "loaded"
		if(_xmlhttp.status==200){// 200 = OK
			parsedhcpLease(_xmlhttp.responseXML);    
		}
		else{
			return false;
		}
	}
}

var leasehostname;
var leasemac;
function parsedhcpLease(xmldoc){
	var dhcpleaseXML = xmldoc.getElementsByTagName("dhcplease");
	leasehostname = dhcpleaseXML[0].getElementsByTagName("hostname");
	leasemac = dhcpleaseXML[0].getElementsByTagName("mac");

	genClientList();
}

var retHostName = function(_mac){
	if(parent.sw_mode != 1 || !leasemac) return false;
	for(var idx=0; idx<leasemac.length; idx++){
		if(!(leasehostname[idx].childNodes[0].nodeValue.split("value=")[1]) || !(leasemac[idx].childNodes[0].nodeValue.split("value=")[1]))
			continue;

		if( _mac.toLowerCase() == leasemac[idx].childNodes[0].nodeValue.split("value=")[1].toLowerCase()){
			if(leasehostname[idx].childNodes[0].nodeValue.split("value=")[1] != "*")
				return leasehostname[idx].childNodes[0].nodeValue.split("value=")[1];
			else
				return _mac;
		}
	}
	return _mac;
}
/* end */

/* Plugin */
var isJsonChanged = function(objNew, objOld){
	for(var i in objOld){	
		if(typeof objOld[i] == "object"){
			if(objNew[i].join() != objOld[i].join()){
				return true;
			}
		}
		else{
			if(typeof objNew[i] == "undefined" || objOld[i] != objNew[i]){
				return true;				
			}
		}
	}

    return false;
};

function convType(str){
	if(str.length == 0)
		return 0;

	var siganature = [[], ["win", "pc"], [], [], ["nas", "storage"], ["cam"], [], 
					  ["ps", "play station", "playstation"], ["xbox"], ["android", "htc"], 
					  ["iphone", "ipad", "ipod", "ios"], ["appletv", "apple tv"], [], 
					  ["nb"], ["mac", "mbp", "mba", "apple"]];

	for(var i=0; i<siganature.length; i++){
		for(var j=0; j<siganature[i].length; j++){
			if(str.toString().toLowerCase().search(siganature[i][j].toString().toLowerCase()) != -1){
				return i;
				break;
			}
		}
	}

	return 0;
}
/* End */

<% login_state_hook(); %>
var originDataTmp;
var originData = {
	customList: decodeURIComponent('<% nvram_char_to_ascii("", "custom_clientlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	asusDevice: '<% nvram_get("asus_device_list"); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	fromDHCPLease: '',
	fromNetworkmapd: '<% get_client_detail_info(); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	fromBWDPI: '<% bwdpi_device_info(); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	wlList_2g: [<% wl_sta_list_2g(); %>],
	wlList_5g: [<% wl_sta_list_5g(); %>],
	qosRuleList: '<% nvram_get("qos_rulelist"); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	init: true
}

var totalClientNum = {
	online: 0,
	wireless: 0,
	wired: 0
}

var setClientAttr = function(){
	this.type = "";
	this.name = "";
	this.ip = "offline";
	this.mac = "";
	this.group = "";
	this.dpiType = "";
	this.rssi = "";
	this.ssid = "";
	this.isWL = 0; // 0:wired, 1:2.4g, 2:5g.
	this.qosLevel = "";
	this.curTx = "";
	this.curRx = "";
	this.totalTx = "";
	this.totalRx = "";
	this.callback = "";
	this.keeparp = "";
	this.isGateway = false;
	this.isWebServer = false;
	this.isPrinter = false;
	this.isITunes = false;
	this.isASUS = false;
	this.isLogin = false;
	this.isOnline = false;
	this.isCustom = false;
}

var clientList = new Array(0);
function genClientList(){
	clientList = [];
	totalClientNum.wireless = 0;

	for(var i=0; i<originData.asusDevice.length; i++){
		var thisClient = originData.asusDevice[i].split(">");
		var thisClientMacAddr = (typeof thisClient[3] == "undefined") ? false : thisClient[3].toUpperCase();

		if(!thisClientMacAddr || thisClient[2] == '<% nvram_get("lan_ipaddr"); %>'){
			continue;
		}

		clientList.push(thisClientMacAddr);
		clientList[thisClientMacAddr] = new setClientAttr();

		clientList[thisClientMacAddr].type = thisClient[0];
		clientList[thisClientMacAddr].name = thisClient[1];
		clientList[thisClientMacAddr].ip = thisClient[2];
		clientList[thisClientMacAddr].mac = thisClient[3];
		clientList[thisClientMacAddr].isGateway = (thisClient[2] == '<% nvram_get("lan_ipaddr"); %>') ? true : false;
		clientList[thisClientMacAddr].isWebServer = true;
		clientList[thisClientMacAddr].isPrinter = thisClient[5];
		clientList[thisClientMacAddr].isITunes = thisClient[6];
		clientList[thisClientMacAddr].ssid = thisClient[7];
		clientList[thisClientMacAddr].isASUS = true;
	}

	totalClientNum.online = parseInt(originData.fromNetworkmapd.length - 1);
	for(var i=0; i<originData.fromNetworkmapd.length; i++){
		var thisClient = originData.fromNetworkmapd[i].split(">");
		var thisClientMacAddr = (typeof thisClient[3] == "undefined") ? false : thisClient[3].toUpperCase();

		if(!thisClientMacAddr){
			continue;
		}

		if(typeof clientList[thisClientMacAddr] == "undefined"){
			clientList.push(thisClientMacAddr);
			clientList[thisClientMacAddr] = new setClientAttr();
		}

		if(clientList[thisClientMacAddr].type == "")
			clientList[thisClientMacAddr].type = thisClient[0];
		
		clientList[thisClientMacAddr].ip = thisClient[2];
		clientList[thisClientMacAddr].mac = thisClient[3];

		if(clientList[thisClientMacAddr].name == ""){
			clientList[thisClientMacAddr].name = (thisClient[1] != "") ? thisClient[1].trim() : retHostName(clientList[thisClientMacAddr].mac);
		}

		if(clientList[thisClientMacAddr].name != clientList[thisClientMacAddr].mac){
			clientList[thisClientMacAddr].type = convType(clientList[thisClientMacAddr].name);
		}

		clientList[thisClientMacAddr].isGateway = (thisClient[2] == '<% nvram_get("lan_ipaddr"); %>') ? true : false;
		clientList[thisClientMacAddr].isWebServer = (thisClient[4] == 0) ? false : true;
		clientList[thisClientMacAddr].isPrinter = (thisClient[5] == 0) ? false : true;
		clientList[thisClientMacAddr].isITunes = (thisClient[6] == 0) ? false : true;
		clientList[thisClientMacAddr].isOnline = true;
	}

	for(var i=0; i<originData.fromBWDPI.length; i++){
		var thisClient = originData.fromBWDPI[i].split(">");
		var thisClientMacAddr = (typeof thisClient[0] == "undefined") ? false : thisClient[0].toUpperCase();

		if(typeof clientList[thisClientMacAddr] == "undefined"){
			continue;
		}

		if(thisClient[1] != ""){
			clientList[thisClientMacAddr].name = thisClient[1];
			clientList[thisClientMacAddr].type = convType(thisClient[1]);
		}

		if(thisClient[2] != ""){
			clientList[thisClientMacAddr].dpiType = thisClient[2];
			clientList[thisClientMacAddr].type = convType(thisClient[2]);			
		}
	}

	for(var i=0; i<originData.customList.length; i++){
		var thisClient = originData.customList[i].split(">");
		var thisClientMacAddr = (typeof thisClient[1] == "undefined") ? false : thisClient[1].toUpperCase();

		if(!thisClientMacAddr || typeof clientList[thisClientMacAddr] == "undefined"){
			continue;
		}

		if(typeof clientList[thisClientMacAddr] == "undefined"){
			clientList.push(thisClientMacAddr);
			clientList[thisClientMacAddr] = new setClientAttr();
		}

		clientList[thisClientMacAddr].name = thisClient[0];
		clientList[thisClientMacAddr].mac = thisClient[1];
		clientList[thisClientMacAddr].group = thisClient[2];
		clientList[thisClientMacAddr].type = thisClient[3];
		clientList[thisClientMacAddr].callback = thisClient[4];
		clientList[thisClientMacAddr].isCustom = true;
	}

	for(var i=0; i<originData.wlList_2g.length; i++){
		var thisClientMacAddr = (typeof originData.wlList_2g[i][0] == "undefined") ? false : originData.wlList_2g[i][0].toUpperCase();

		if(!thisClientMacAddr || typeof clientList[thisClientMacAddr] == "undefined"){
			continue;
		}

		clientList[thisClientMacAddr].rssi = originData.wlList_2g[i][3];
		clientList[thisClientMacAddr].isWL = 1;
		totalClientNum.wireless++;
	}

	for(var i=0; i<originData.wlList_5g.length; i++){
		var thisClientMacAddr = (typeof originData.wlList_5g[i][0] == "undefined") ? false : originData.wlList_5g[i][0].toUpperCase();

		if(!thisClientMacAddr || typeof clientList[thisClientMacAddr] == "undefined"){
			continue;
		}

		clientList[thisClientMacAddr].rssi = originData.wlList_5g[i][3];
		clientList[thisClientMacAddr].isWL = 2;
		totalClientNum.wireless++;
	}


	if(typeof login_mac_str == "function"){
		var thisClientMacAddr = (typeof login_mac_str == "undefined") ? false : login_mac_str().toUpperCase();

		if(typeof clientList[thisClientMacAddr] != "undefined"){
			clientList[thisClientMacAddr].isLogin = true;
		}
	}

	for(var i=0; i<originData.qosRuleList.length; i++){
		var thisClient = originData.qosRuleList[i].split(">");
		var thisClientMacAddr = (typeof thisClient[1] == "undefined") ? false : thisClient[1].toUpperCase();

		if(!thisClientMacAddr || typeof clientList[thisClientMacAddr] == "undefined"){
			continue;
		}

		if(typeof clientList[thisClientMacAddr] != "undefined"){
			clientList[thisClientMacAddr].qosLevel = thisClient[5];
		}
	}

	totalClientNum.wired = parseInt(totalClientNum.online - totalClientNum.wireless);
}


/* Exported from device-map/clients.asp */

function retOverLibStr(client){
	var overlibStr = "<p><#MAC_Address#>:</p>" + client.mac.toUpperCase();

	if(client.ssid)
		overlibStr += "<p>SSID:</p>" + client.ssid;
	if(client.isLogin)
		overlibStr += "<p><#CTL_localdevice#>:</p>YES";
	if(client.isPrinter)
		overlibStr += "<p><#Device_service_Printer#></p>YES";
	if(client.isITunes)
		overlibStr += "<p><#Device_service_iTune#></p>YES";
	if(client.isWL > 0)
		overlibStr += "<p><#Wireless_Radio#>:</p>" + ((client.isWL == 2) ? "5GHz (" : "2.4GHz (") + client.rssi + "db)";

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

function oui_query(mac){
	if (typeof clientList[mac] != "undefined"){
		if(clientList[mac].callback != ""){
			ajaxCallJsonp("http://" + clientList[mac].ip + ":" + clientList[mac].callback + "/callback.asp?output=netdev&jsoncallback=?");
		}
	}

	var tab = new Array();
	tab = mac.split(mac.substr(2,1));
	$j.ajax({
	    url: 'http://standards.ieee.org/cgi-bin/ouisearch?'+ tab[0] + '-' + tab[1] + '-' + tab[2],
		type: 'GET',
	    error: function(xhr) {
			if(overlib.isOut)
				return true;
			else
				oui_query(mac);
	    },
	    success: function(response) {
			if(overlib.isOut) return nd();

			var retData = response.responseText.split("pre")[1].split("(base 16)")[1].replace("PROVINCE OF CHINA", "R.O.C").split("&lt;/");

			if (typeof clientList[mac] != "undefined")
				overlibStrTmp  = retOverLibStr(clientList[mac]);
			else
				overlibStrTmp = "<p><#MAC_Address#>:</p>" + mac.toUpperCase();

			overlibStrTmp += "<p><span>.....................................</span></p><p style='margin-top:5px'><#Manufacturer#> :</p>";
			overlibStrTmp += retData[0];

			return overlib(overlibStrTmp, HAUTO, VAUTO);
		}    
	});
}

function clientFromIP(ip) {
	for(var i=0; i<clientList.length;i++){
		var clientObj = clientList[clientList[i]];
		if(clientObj.ip == ip) return clientObj;
	}
	return 0;
}

/* End exported functions */

