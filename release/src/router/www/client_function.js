/* Plugin */
if (!Object.keys) {
  Object.keys = (function() {
    'use strict';
    var hasOwnProperty = Object.prototype.hasOwnProperty,
        hasDontEnumBug = !({ toString: null }).propertyIsEnumerable('toString'),
        dontEnums = [
          'toString',
          'toLocaleString',
          'valueOf',
          'hasOwnProperty',
          'isPrototypeOf',
          'propertyIsEnumerable',
          'constructor'
        ],
        dontEnumsLength = dontEnums.length;

    return function(obj) {
      if (typeof obj !== 'object' && (typeof obj !== 'function' || obj === null)) {
        throw new TypeError('Object.keys called on non-object');
      }

      var result = [], prop, i;

      for (prop in obj) {
        if (hasOwnProperty.call(obj, prop)) {
          result.push(prop);
        }
      }

      if (hasDontEnumBug) {
        for (i = 0; i < dontEnumsLength; i++) {
          if (hasOwnProperty.call(obj, dontEnums[i])) {
            result.push(dontEnums[i]);
          }
        }
      }
      return result;
    };
  }());
}

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

var ipState = new Array();
ipState["Static"] =  "<#BOP_ctype_title5#>";
ipState["DHCP"] =  "<#BOP_ctype_title1#>";
ipState["Manual"] =  "MAC-IP Binding";
ipState["OffLine"] =  "Client is disconnected";

var venderArrayRE = /(adobe|amazon|apple|asus|belkin|bizlink|buffalo|dell|d-link|fujitsu|google|hon hai|htc|huawei|ibm|lenovo|nec|microsoft|panasonic|pioneer|ralink|samsung|sony|synology|toshiba|tp-link|vmware)/;

function convType(str){
	if(str.length == 0)
		return 0;
	/*
	Unknown			0 
	Windows			1
	Router			2
	AP				3
	NAS				4
	IPCam			5
	Unknown			6
	PlayStation		7
	XBox			8
	Android			9
	iOS				10
	AppleTV			11
	Setup Box		12
	NB				13
	Macintosh		14
	ROG				15
	PlayStation4	16
	XBoxOne			17
	Printer			18
	*/
	var siganature = [[], ["win", "pc"], ["rt-", "rp-", "dsl-", "ea-", "wmp-", "pl-"], [], ["nas", "storage"], ["cam"], [], 
					  ["play station", "playstation"], ["xbox"], ["android", "htc"], 
					  ["iphone", "ipad", "ipod"], ["appletv", "apple tv", "apple-tv"], [], 
					  ["nb"], ["mac", "mbp", "mba", "apple"], [], [], [], ["epson", "fuji xerox", "hp", "canon", "brother"]];

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

<% login_state_hook(); %>
/* End */

/* get client info form dhcp lease log */
var leaseArray = {
	hostname: [],
	mac: []
};

var retHostName = function(_mac){
	return leaseArray.hostname[leaseArray.mac.indexOf(_mac.toUpperCase())] || _mac;
}
/* end */

var networkmap_fullscan = '<% nvram_get("networkmap_fullscan"); %>';
var fromNetworkmapdCache = '<% nvram_get("client_info_tmp"); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<');

var originDataTmp;
var originData = {
	customList: decodeURIComponent('<% nvram_char_to_ascii("", "custom_clientlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	asusDevice: decodeURIComponent('<% nvram_char_to_ascii("", "asus_device_list"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	fromDHCPLease: <% dhcpLeaseMacList(); %>,
	staticList: decodeURIComponent('<% nvram_char_to_ascii("", "dhcp_staticlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	fromNetworkmapd: '<% get_client_detail_info(); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	fromBWDPI: '<% bwdpi_device_info(); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	nmpClient: decodeURIComponent('<% nvram_char_to_ascii("", "nmp_client_list"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'), //Record the client connected to the router before.
	wlList_2g: [<% wl_sta_list_2g(); %>],
	wlList_5g: [<% wl_sta_list_5g(); %>],
	wlList_5g_2: [<% wl_sta_list_5g_2(); %>],
	wlListInfo_2g: [<% wl_stainfo_list_2g(); %>],
	wlListInfo_5g: [<% wl_stainfo_list_5g(); %>],
	wlListInfo_5g_2: [<% wl_stainfo_list_5g_2(); %>],
	qosRuleList: decodeURIComponent('<% nvram_char_to_ascii("", "qos_rulelist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	time_scheduling_enable: decodeURIComponent('<% nvram_char_to_ascii("", "MULTIFILTER_ENABLE"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('>'),
	time_scheduling_mac: decodeURIComponent('<% nvram_char_to_ascii("", "MULTIFILTER_MAC"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('>'),
	time_scheduling_devicename: decodeURIComponent('<% nvram_char_to_ascii("", "MULTIFILTER_DEVICENAME"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('>'),
	time_scheduling_daytime: decodeURIComponent('<% nvram_char_to_ascii("", "MULTIFILTER_MACFILTER_DAYTIME"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('>'),
	current_time: '<% uptime(); %>',
	init: true
}

var totalClientNum = {
	online: 0,
	wireless: 0,
	wired: 0,
	wireless_ifnames: [],
}

var setClientAttr = function(){
	this.hostname = "";
	this.type = "";
	this.defaultType = "0";
	this.name = "";
	this.nickName = "";
	this.ip = "offline";
	this.mac = "";
	this.from = "";
	this.macRepeat = 1;
	this.group = "";
	this.rssi = "";
	this.ssid = "";
	this.isWL = 0; // 0: wired, 1: 2.4GHz, 2: 5GHz/5GHz-1 3:5GHz-2.
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
	this.ipMethod = "Static";
	this.opMode = 0;
	this.wlConnectTime = "00:00:00";
	this.dpiVender = "";
	this.dpiType = "";
	this.dpiDevice = "";
	this.internetMode = "allow";
	this.internetState = 1; // 1:Allow Internet access, 0:Block Internet access
}

var ouiClientList = cookie.get("ouiClientList");
if(ouiClientList == null) {
	ouiClientList = "";
}

var clientList = new Array(0);
var ouiClientListArray = new Array();
var time_scheduling_array = new Array();
function genClientList(){
	var updateTimeScheduling = function() {
		if('<% nvram_get("MULTIFILTER_ALL"); %>' == "1") {
			if(time_scheduling_array[thisClientMacAddr] != undefined) {
				if(time_scheduling_array[thisClientMacAddr][0] == "1") {
					clientList[thisClientMacAddr].internetMode = "block";
					if(time_scheduling_array[thisClientMacAddr][1] != "<") {
						clientList[thisClientMacAddr].internetMode = "time";
						clientList[thisClientMacAddr].internetState = 0;
						if(clientInternetState(thisClientMacAddr))
							clientList[thisClientMacAddr].internetState = 1;
					}
					else {
						clientList[thisClientMacAddr].internetState = 0;
					}
				}
			}
		}
	};

	var wirelessList = cookie.get("wireless_list");
	var wirelessListArray = new Array();
	
	leaseArray = {hostname: [], mac: []};
	for(var i = 0; i < originData.fromDHCPLease.length; i += 1) {
		var dhcpMac = originData.fromDHCPLease[i][0].toUpperCase();
		var dhcpName = decodeURIComponent(originData.fromDHCPLease[i][1]);
		if(dhcpMac != "") {
			if(dhcpName == "*") {
				dhcpName = dhcpMac;
			}
			leaseArray.mac.push(dhcpMac);
			leaseArray.hostname.push(dhcpName);
		}
	}

	clientList = [];
	totalClientNum.online = 0;
	totalClientNum.wireless = 0;
	for(var i=0; i<wl_nband_title.length; i++) totalClientNum.wireless_ifnames[i] = 0;

	//initial wirelessListArray
	if(wirelessList != null && wirelessList != "") {
		var wirelessList_row = wirelessList.split("<");
		for(var i = 0; i < wirelessList_row.length; i += 1) {
			var wirelessList_col = wirelessList_row[i].split(">");
			wirelessListArray[wirelessList_col[0]] = "No";
		}
	}

	//initial ouiClientListArray
	if(ouiClientList != null && ouiClientList != "") {
		var ouiClientList_row = ouiClientList.split("<");
		for(var i = 0; i < ouiClientList_row.length; i += 1) {
			var ouiClientList_col = ouiClientList_row[i].split(">");
			ouiClientListArray[ouiClientList_col[0]] = ouiClientList_col[1];
		}
	}

	//initial time_scheduling
	for(var schedulingIdx = 0; schedulingIdx < originData.time_scheduling_mac.length; schedulingIdx += 1) {
		if(originData.time_scheduling_mac[schedulingIdx] != "") {
			var scheduling_array =  new Array();
			scheduling_array[0] =  originData.time_scheduling_enable[schedulingIdx];
			scheduling_array[1] = originData.time_scheduling_daytime[schedulingIdx];
			time_scheduling_array[originData.time_scheduling_mac[schedulingIdx]] = scheduling_array;
		}
	}

	if(fromNetworkmapdCache.length > 1 && networkmap_fullscan == 1)
		originData.fromNetworkmapd = fromNetworkmapdCache;

	for(var i=0; i<originData.asusDevice.length; i++){
		var thisClient = originData.asusDevice[i].split(">");
		var thisClientMacAddr = (typeof thisClient[3] == "undefined") ? false : thisClient[3].toUpperCase();

		if(!thisClientMacAddr || thisClient[2] == '<% nvram_get("lan_ipaddr"); %>'){
			continue;
		}

		if(typeof clientList[thisClientMacAddr] == "undefined"){
			clientList.push(thisClientMacAddr);
			clientList[thisClientMacAddr] = new setClientAttr();
			clientList[thisClientMacAddr].from = "asusDevice";
		}
		else{
			clientList[thisClientMacAddr].from = "asusDevice";
		}
		
		clientList[thisClientMacAddr].type = "2";// asus default setting router icon
		clientList[thisClientMacAddr].defaultType = "2";
		clientList[thisClientMacAddr].name = thisClient[1];
		//Exception Handling AiCam type
		if(thisClient[1].toString().toLowerCase().search("cam") != -1) {
			clientList[thisClientMacAddr].type = "5";// AiCam icon
			clientList[thisClientMacAddr].defaultType = "5";
		}
		clientList[thisClientMacAddr].ip = thisClient[2];
		clientList[thisClientMacAddr].mac = thisClient[3];
		clientList[thisClientMacAddr].isGateway = (thisClient[2] == '<% nvram_get("lan_ipaddr"); %>') ? true : false;
		clientList[thisClientMacAddr].isWebServer = true;
		clientList[thisClientMacAddr].ssid = thisClient[5];
		clientList[thisClientMacAddr].isASUS = true;
		clientList[thisClientMacAddr].opMode = (typeof thisClient[7] == "undefined") ? 0 : thisClient[7];
		clientList[thisClientMacAddr].isOnline = true;
		totalClientNum.online++;

		clientList[thisClientMacAddr].dpiVender = "Asus";
		updateTimeScheduling();
	}

	for(var i=0; i<originData.fromNetworkmapd.length; i++){
		var thisClient = originData.fromNetworkmapd[i].split(">");
		var thisClientMacAddr = (typeof thisClient[3] == "undefined") ? false : thisClient[3].toUpperCase();

		if(!thisClientMacAddr){
			continue;
		}

		if(typeof clientList[thisClientMacAddr] == "undefined"){
			clientList.push(thisClientMacAddr);
			clientList[thisClientMacAddr] = new setClientAttr();
			clientList[thisClientMacAddr].from = "networkmapd";
		}
		else{
			//because asusDevice had count, so not need count again.
			if(clientList[thisClientMacAddr].from == "asusDevice") {
				clientList[thisClientMacAddr].from = "networkmapd";
				continue;
			}
			else {			
				clientList[thisClientMacAddr].macRepeat++;
				totalClientNum.online++;
				continue;
			}
		}

		if(clientList[thisClientMacAddr].type == "") {
			clientList[thisClientMacAddr].type = thisClient[0];
			clientList[thisClientMacAddr].defaultType = thisClient[0];
		}
		
		clientList[thisClientMacAddr].ip = thisClient[2];
		clientList[thisClientMacAddr].mac = thisClient[3];

		var ori_name = (thisClient[1].trim() != "") ? thisClient[1].trim() : retHostName(clientList[thisClientMacAddr].mac);
		if(clientList[thisClientMacAddr].name == ""){
			var replaceClientName = function(_name, _mac) {
				var filterStr = "android";
				var replaceName = _name;
				if(replaceName == _mac) {
					if(ouiClientListArray[_mac]) {
						replaceName = ouiClientListArray[_mac].toUpperCase().charAt(0) + ouiClientListArray[_mac].substring(1);
					}
				}
				else {
					if(_name.search(filterStr) != -1) {
						if(ouiClientListArray[_mac]) {
							replaceName = ouiClientListArray[_mac].toUpperCase().charAt(0) + ouiClientListArray[_mac].substring(1);
						}
					}
				}
				return replaceName;
			};
			clientList[thisClientMacAddr].name = replaceClientName(ori_name, thisClientMacAddr);
		}

		if(ori_name != clientList[thisClientMacAddr].mac){
			clientList[thisClientMacAddr].type = convType(ori_name);
			clientList[thisClientMacAddr].defaultType = clientList[thisClientMacAddr].type;
		}

		clientList[thisClientMacAddr].isGateway = (thisClient[2] == '<% nvram_get("lan_ipaddr"); %>') ? true : false;
		clientList[thisClientMacAddr].isWebServer = (thisClient[4] == 0) ? false : true;
		clientList[thisClientMacAddr].isPrinter = (thisClient[5] == 0) ? false : true;
		clientList[thisClientMacAddr].isITunes = (thisClient[6] == 0) ? false : true;
		clientList[thisClientMacAddr].dpiDevice = (typeof thisClient[7] == "undefined") ? "" : thisClient[7]; //This field just for apple model
		clientList[thisClientMacAddr].isOnline = true;
		totalClientNum.online++;

		var ouiVenderName = "";
		if(ouiClientListArray[thisClientMacAddr] != undefined) {
			ouiVenderName = ouiClientListArray[thisClientMacAddr];
			ouiVenderName = ouiVenderName.toLowerCase();
			ouiVenderName = ouiVenderName.toUpperCase().charAt(0) + ouiVenderName.substring(1);
		}
		clientList[thisClientMacAddr].dpiVender = ouiVenderName;
		updateTimeScheduling();
	}

	for(var i=0; i<originData.fromBWDPI.length; i++){
		var thisClient = originData.fromBWDPI[i].split(">");
		var thisClientMacAddr = (typeof thisClient[0] == "undefined") ? false : thisClient[0].toUpperCase();

		if(typeof clientList[thisClientMacAddr] == "undefined"){
			continue;
		}

		var networkmapd_name = clientList[thisClientMacAddr].name;
		var bwdpi_name = "";

		if(thisClient[1] != ""){
			var replaceClientName = function(_name, _mac) {
				var filterStr = "android";
				var replaceName = _name.trim();
				if(replaceName.search(filterStr) != -1) {
					if(ouiClientListArray[_mac]) {
						replaceName = ouiClientListArray[_mac].toUpperCase().charAt(0) + ouiClientListArray[_mac].substring(1);
					}
				}
				return replaceName;
			};
			clientList[thisClientMacAddr].name = replaceClientName(thisClient[1], thisClientMacAddr);
			clientList[thisClientMacAddr].type = convType(thisClient[1]);
			clientList[thisClientMacAddr].defaultType = clientList[thisClientMacAddr].type;
			bwdpi_name = thisClient[1];
		}

		if(thisClient[2] != "" && thisClient[2] != undefined){
			var venderMatch = thisClient[2].trim().toLowerCase().match(venderArrayRE);
			var venderName = "";
			if(Boolean(venderMatch)) {
				venderName = venderMatch[0].toLowerCase();
				venderName = venderName.toUpperCase().charAt(0) + venderName.substring(1);
				clientList[thisClientMacAddr].dpiVender = venderName;
			}
			else {
				venderName = thisClient[2].trim().toLowerCase();
				venderName = venderName.toUpperCase().charAt(0) + venderName.substring(1);
				clientList[thisClientMacAddr].dpiVender = venderName;
			}
		}

		if(thisClient[3] != "" && thisClient[3] != undefined){
			clientList[thisClientMacAddr].dpiType = thisClient[3].trim();
		}

		if(thisClient[4] != "" && thisClient[4] != undefined){
			clientList[thisClientMacAddr].dpiDevice = thisClient[4].trim();
		}

		if(thisClient[3] != "" && thisClient[3] != undefined && thisClient[4] != "" && thisClient[4] != undefined) {
			var filterStr = "android";
			var replaceName = clientList[thisClientMacAddr].name;
			if(networkmapd_name.toLowerCase().search(filterStr) != -1 || bwdpi_name.toLowerCase().search(filterStr) != -1) {
				if(thisClient[3].toLowerCase().search(filterStr) != -1 && thisClient[4].toLowerCase().search(filterStr) != -1) {
					replaceName = clientList[thisClientMacAddr].dpiVender + "(Android)";
				}
				else {
					replaceName = thisClient[4];
				}
			}
			clientList[thisClientMacAddr].name = replaceName;
		}
	}

	for(var i=0; i<originData.customList.length; i++){
		var thisClient = originData.customList[i].split(">");
		var thisClientMacAddr = (typeof thisClient[1] == "undefined") ? false : thisClient[1].toUpperCase();

		if(!thisClientMacAddr){
			continue;
		}

		if(typeof clientList[thisClientMacAddr] == "undefined"){
			clientList.push(thisClientMacAddr);
			clientList[thisClientMacAddr] = new setClientAttr();
			clientList[thisClientMacAddr].from = "customList";
		}

		clientList[thisClientMacAddr].nickName = thisClient[0];
		clientList[thisClientMacAddr].mac = thisClient[1];
		clientList[thisClientMacAddr].group = thisClient[2];
		clientList[thisClientMacAddr].type = thisClient[3];
		clientList[thisClientMacAddr].callback = thisClient[4];

		if(clientList[thisClientMacAddr].dpiVender == "") {
			var ouiVenderName = "";
			if(ouiClientListArray[thisClientMacAddr] != undefined) {
				ouiVenderName = ouiClientListArray[thisClientMacAddr];
				ouiVenderName = ouiVenderName.toLowerCase();
				ouiVenderName = ouiVenderName.toUpperCase().charAt(0) + ouiVenderName.substring(1);
			}
			clientList[thisClientMacAddr].dpiVender = ouiVenderName;
		}
	}

	for(var i = 0; i < originData.nmpClient.length; i += 1) {
		var thisClient = originData.nmpClient[i].split(">");
		var thisClientMacAddr = ((typeof thisClient[0] == "undefined") || thisClient[0] == "" || thisClient[0].length != 12) ? false : thisClient[0].toUpperCase().substring(0, 2) + ":" + 
		thisClient[0].toUpperCase().substring(2, 4) + ":" + thisClient[0].toUpperCase().substring(4, 6) + ":" + thisClient[0].toUpperCase().substring(6, 8) + ":" + 
		thisClient[0].toUpperCase().substring(8, 10) + ":" + thisClient[0].toUpperCase().substring(10, 12);

		if(!thisClientMacAddr) {
			continue;
		}

		if(typeof clientList[thisClientMacAddr] == "undefined") {
			var thisClientType = (typeof thisClient[4] == "undefined") ? "0" : thisClient[4];
			var thisClientName = (typeof thisClient[2] == "undefined") ? thisClientMacAddr : thisClient[2].trim();

			clientList.push(thisClientMacAddr);
			clientList[thisClientMacAddr] = new setClientAttr();
			clientList[thisClientMacAddr].from = "nmpClient";

			clientList[thisClientMacAddr].type = thisClientType;
			clientList[thisClientMacAddr].defaultType = thisClientType;
			clientList[thisClientMacAddr].mac = thisClientMacAddr;

			var replaceClientName = function(_name, _mac) {
				var filterStr = "android";
				var replaceName = _name;
				if(replaceName == _mac) {
					if(ouiClientListArray[_mac]) {
						replaceName = ouiClientListArray[_mac].toUpperCase().charAt(0) + ouiClientListArray[_mac].substring(1);
					}
				}
				else {
					if(_name.search(filterStr) != -1) {
						if(ouiClientListArray[_mac]) {
							replaceName = ouiClientListArray[_mac].toUpperCase().charAt(0) + ouiClientListArray[_mac].substring(1);
						}
					}
				}
				return replaceName;
			};
			clientList[thisClientMacAddr].name = replaceClientName(thisClientName, thisClientMacAddr);

			if(thisClientName != thisClientMacAddr) {
				clientList[thisClientMacAddr].type = convType(thisClientName);
				clientList[thisClientMacAddr].defaultType = clientList[thisClientMacAddr].type;
			}
		}
	}

	for(var i=0; i<originData.wlList_2g.length; i++){
		var thisClientMacAddr = (typeof originData.wlList_2g[i][0] == "undefined") ? false : originData.wlList_2g[i][0].toUpperCase();

		if(!thisClientMacAddr || typeof clientList[thisClientMacAddr] == "undefined"){
			continue;
		}

		if(originData.wlList_2g[i][1] == "Yes") {
			clientList[thisClientMacAddr].rssi = originData.wlList_2g[i][3];
			clientList[thisClientMacAddr].isWL = 1;

			totalClientNum.wireless += clientList[thisClientMacAddr].macRepeat;
			totalClientNum.wireless_ifnames[clientList[thisClientMacAddr].isWL-1] += clientList[thisClientMacAddr].macRepeat;
			wirelessListArray[thisClientMacAddr] = originData.wlList_2g[i][1];
		} 
	}

	for(var i=0; i<originData.wlList_5g.length; i++){
		var thisClientMacAddr = (typeof originData.wlList_5g[i][0] == "undefined") ? false : originData.wlList_5g[i][0].toUpperCase();

		if(!thisClientMacAddr || typeof clientList[thisClientMacAddr] == "undefined"){
			continue;
		}

		if(originData.wlList_5g[i][1] == "Yes") {
			clientList[thisClientMacAddr].rssi = originData.wlList_5g[i][3];
			clientList[thisClientMacAddr].isWL = 2;
		
			totalClientNum.wireless += clientList[thisClientMacAddr].macRepeat;
			totalClientNum.wireless_ifnames[clientList[thisClientMacAddr].isWL-1] += clientList[thisClientMacAddr].macRepeat;
			wirelessListArray[thisClientMacAddr] = originData.wlList_5g[i][1];
		}
	}

	for(var i=0; i<originData.wlList_5g_2.length; i++){
		var thisClientMacAddr = (typeof originData.wlList_5g_2[i][0] == "undefined") ? false : originData.wlList_5g_2[i][0].toUpperCase();

		if(!thisClientMacAddr || typeof clientList[thisClientMacAddr] == "undefined"){
			continue;
		}

		if(originData.wlList_5g_2[i][1] == "Yes") {
			clientList[thisClientMacAddr].rssi = originData.wlList_5g_2[i][3];
			clientList[thisClientMacAddr].isWL = 3;

			totalClientNum.wireless += clientList[thisClientMacAddr].macRepeat;
			totalClientNum.wireless_ifnames[clientList[thisClientMacAddr].isWL-1] += clientList[thisClientMacAddr].macRepeat;
			wirelessListArray[thisClientMacAddr] = originData.wlList_5g_2[i][1];
		}
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

	for(var i = 0; i < leaseArray.mac.length; i += 1) {
		if(typeof clientList[leaseArray.mac[i]] != "undefined"){
			clientList[leaseArray.mac[i]].ipMethod = "DHCP";
		}
	}

	for(var i=0; i<originData.staticList.length; i++){
		if('<% nvram_get("dhcp_static_x"); %>' == "0") break;

		var thisClient = originData.staticList[i].split(">");
		var thisClientMacAddr = (typeof thisClient[0] == "undefined") ? false : thisClient[0].toUpperCase();

		if(!thisClientMacAddr || typeof clientList[thisClientMacAddr] == "undefined"){
			continue;
		}

		clientList[thisClientMacAddr].hostname = thisClient[2];
		if(typeof clientList[thisClientMacAddr] != "undefined"){
			if(clientList[thisClientMacAddr].ipMethod == "DHCP") {
				if(clientList[thisClientMacAddr].ip == thisClient[1] || clientList[thisClientMacAddr].ip == "offline")
					clientList[thisClientMacAddr].ipMethod = "Manual";
			}
		}
	}

	wirelessList = "";
	Object.keys(wirelessListArray).forEach(function(key) {
		if(key != "") {
			var clientMac = key
			var clientMacState = wirelessListArray[key];
			wirelessList +=  "<" + clientMac + ">" + clientMacState;
			if(typeof clientList[clientMac] != "undefined") {
				var wirelessOnline = (clientMacState.split(">")[0] == "Yes") ? true : false;
				//If wireless device in sleep mode, but still connect to router. The wireless log still be connected in but in fromNetworkmapd not assigned to IP
				if(clientList[clientMac].ip == "offline" && wirelessOnline) {
					clientList[clientMac].isOnline = false;
					totalClientNum.wireless--;
					totalClientNum.wireless_ifnames[clientList[clientMac].isWL-1]--;
				}
				else { //If wireless device offline, but the device value not delete in fromNetworkmapd in real time, so need update the totalClientNum
					if(clientList[clientMac].isOnline && !wirelessOnline) { 
						totalClientNum.online--;
					}
					clientList[clientMac].isOnline = wirelessOnline;
				}
			}
		}
	});
	cookie.set("wireless_list", wirelessList, 30);

	if(stainfo_support) {
		var updateStaInfo = function(wlLog, wlMode) {
			for(var i = 0; i < wlLog.length; i += 1) {
				var thisClientMacAddr = (typeof wlLog[i][0] == "undefined") ? false : wlLog[i][0].toUpperCase();

				if(!thisClientMacAddr || typeof clientList[thisClientMacAddr] == "undefined"){
					continue;
				}

				if(clientList[thisClientMacAddr].isOnline && (clientList[thisClientMacAddr].isWL == wlMode)) {
					clientList[thisClientMacAddr].curTx = (wlLog[i][1].trim() == "") ? "": wlLog[i][1].trim() + " Mbps";
					clientList[thisClientMacAddr].curRx = (wlLog[i][2].trim() == "") ? "": wlLog[i][2].trim() + " Mbps";
					clientList[thisClientMacAddr].wlConnectTime = wlLog[i][3];
				}
			}
		};	
		updateStaInfo(originData.wlListInfo_2g, 1);
		updateStaInfo(originData.wlListInfo_5g, 2);
		updateStaInfo(originData.wlListInfo_5g_2, 3);
	}

	totalClientNum.wired = parseInt(totalClientNum.online - totalClientNum.wireless);
}

//Initialize client list obj immediately
genClientList();

function setClientListOUI() {
	for(var i = 0 ; i < originData.fromNetworkmapd.length; i += 1) {
		var thisClient = originData.fromNetworkmapd[i].split(">");
		
		if(typeof thisClient[3] != "undefined") {
			if(ouiClientList.search(thisClient[3]) == -1) {
				setTimeout(function(callBackMac){
					if('<% nvram_get("x_Setting"); %>' == '1' && wanConnectStatus && clientList[thisClient[3]].internetState) {
						oui_query_set_cookie(callBackMac);
					}
				}, 1000, thisClient[3]);
			}
		}
	}
}

function oui_query_set_cookie(mac) {
	var tab = new Array();
	tab = mac.split(mac.substr(2,1));
	$.ajax({
	    url: 'http://standards.ieee.org/cgi-bin/ouisearch?'+ tab[0] + '-' + tab[1] + '-' + tab[2],
		type: 'GET',
	    success: function(response) {
	    	if(response.responseText.search("Sorry!") == -1) {
				var retData = response.responseText.split("pre")[1].split("(hex)")[1].split(tab[0] + tab[1] + tab[2])[0].split("&lt;/");
				if(ouiClientList == null) {
					ouiClientList = "";
				}

				var venderMatch = retData[0].trim().toLowerCase().match(venderArrayRE);
				if(Boolean(venderMatch)) {
					ouiClientList += "<" +  mac + ">" + venderMatch[0];
				}
				else {
					ouiClientList += "<" +  mac + ">" + retData[0].trim();
				}
				cookie.set("ouiClientList", ouiClientList, 30);
			}
		}    
	});
}

function getUploadIcon(clientMac) {
	var result = "NoIcon";
	$.ajax({
		url: '/ajax_uploadicon.asp?clientmac=' + clientMac,
		async: false,
		dataType: 'script',
		error: function(xhr){
			setTimeout("getUploadIcon('" + clientMac + "');", 1000);
		},
		success: function(response){
			result = upload_icon;
		}
	});
	return result
}

function getUploadIconCount() {
	var count = 0;
	$.ajax({
		url: '/ajax_uploadicon.asp',
		async: false,
		dataType: 'script',
		error: function(xhr){
			setTimeout("getUploadIconCount();", 1000);
		},
		success: function(response){
			count = upload_icon_count;
		}
	});
	return count
}

function getUploadIconList() {
	var list = "";
	$.ajax({
		url: '/ajax_uploadicon.asp',
		async: false,
		dataType: 'script',
		error: function(xhr){
			setTimeout("getUploadIconList();", 1000);
		},
		success: function(response){
			list = upload_icon_list;
		}
	});
	return list
}


function getVenderIconClassName(venderName) {
	var vender_class_name = "";
	if(Boolean(venderName.match(venderArrayRE))) {
		vender_class_name = venderName;
		if(venderName == "hon hai") {
			vender_class_name = "honhai";
		}
	}
	else {
		vender_class_name = "";
	}
	return vender_class_name;
}

function removeElement(element) {
    element && element.parentNode && element.parentNode.removeChild(element);
}
var temp_clickedObj = null;
var client_hide_flag = false;
function hide_edit_client_block() {
	if(client_hide_flag) {
		fadeOut(document.getElementById("edit_client_block"), 10, 0);
		if(temp_clickedObj.className.search("clientIcon") != -1) {
			temp_clickedObj.className = temp_clickedObj.className.replace("clientIcon_clicked","clientIcon");
			temp_clickedObj.className = temp_clickedObj.className.replace(" card_clicked", "");
		}
		else if(temp_clickedObj.className.search("venderIcon") != -1) {
			temp_clickedObj.className = temp_clickedObj.className.replace("venderIcon_clicked","venderIcon");
			temp_clickedObj.className = temp_clickedObj.className.replace(" card_clicked", "");
		}
		document.body.onclick = null;
		temp_clickedObj = null;
	}
	client_hide_flag = true;
}
function show_edit_client_block() {
	client_hide_flag = false;
}

function clientInternetState(mac) {
	var internetState = false;
	var currentTimeStr = originData.current_time;
	var weekArray = ["", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
	var week = parseInt(weekArray.indexOf(currentTimeStr.substring(0,3)));
	var hour = parseInt(currentTimeStr.substring(17,19));

	var timeSchedulingArray = time_scheduling_array[mac][1].split("<");
	for(var i = 0; i < timeSchedulingArray.length; i += 1) {
		if(!isNaN(parseInt(timeSchedulingArray[i]))) {
			var week_start = parseInt(timeSchedulingArray[i].substring(0,1));
			var week_end = parseInt(timeSchedulingArray[i].substring(1,2));
			var hour_start = parseInt(timeSchedulingArray[i].substring(2,4));
			var hour_end = parseInt(timeSchedulingArray[i].substring(4,6));
			if(week_start == 0 && week_end == 0 && hour_start == 0 && hour_end == 0 || week_start > week_end)
				week_end = 7;

			if(week_start == 0 && week_end == 7 && hour_start == 0 && hour_end == 0) { //all time setting
				internetState = true;
				break;
			}
			else if(week_start == week && week_end == week) {
				if(hour_start <= hour && hour_end > hour) {
					internetState = true;
					break;
				}
			}
			else if(week_start == week && week_end > week) {
				if(hour_start <= hour) {
					internetState = true;
					break;
				}
			}
			else if(week_start < week && week_end > week) {
				internetState = true;
				break;
			}
			else if(week_start < week && week_end >= week) {
				if(hour_end > hour) {
					internetState = true;
					break;
				}
			}
		}
	}
	return internetState;
}

var card_firstTimeOpenBlock = false;
var card_custom_usericon_del = "";
var userIconBase64 = "NoIcon";
function popClientListEditTable(mac, obj, name, ip, callBack) {
	card_firstTimeOpenBlock = false;
	var clientInfo = clientList[mac];
	if(clientInfo == undefined) {
		clientInfo = new setClientAttr();
		clientInfo.type = "0";
		clientInfo.name = "New device";
		clientInfo.mac = mac;
		clientInfo.defaultType = "0";
	}
	if(name != "" && name != undefined) {
		clientInfo.nickName = name;
	}
	if(ip != "" && ip != undefined)
		clientInfo.ip = ip;	

	if(obj.className.search("card_clicked") != -1) {
		return true;
	}

	client_hide_flag = false;

	var client_manual_dhcp_list_array = new Array();
	var client_manual_dhcp_list = decodeURIComponent('<% nvram_char_to_ascii("", "dhcp_staticlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<");
	var client_manual_dhcp_list_row = client_manual_dhcp_list.split("<");
	for(var dhcpIndex = 0; dhcpIndex < client_manual_dhcp_list_row.length; dhcpIndex += 1) {
		if(client_manual_dhcp_list_row[dhcpIndex] != "") {
			var client_manual_dhcp_list_col = client_manual_dhcp_list_row[dhcpIndex].split(">");
			client_manual_dhcp_list_array[client_manual_dhcp_list_col[0]] = client_manual_dhcp_list_col[1];
		}
	}
	
	if(document.getElementById("edit_client_block") != null) {
		removeElement(document.getElementById("edit_client_block"));
	}

	var divObj = document.createElement("div");
	divObj.setAttribute("id","edit_client_block");
	divObj.className = "clientlist_content";

	var code = "";
	code += '<table class="card_table" align="center" cellpadding="5" cellspacing="0" title="">';
	code += '<tbody>';

	//device title info. start
	code += '<tr><td colspan="2" style="background-color:#2B373B;">';
	code += '<table style="width:100%" cellpadding="0" cellspacing="0">';
	code += '<tr>';
	code += '<td colspan="2">';
	code += '<div id="card_client_state_div" class="clientState">';
	code += '<span id="card_client_ipMethod" class="ipMethodTag" style="color:#FFFFFF;margin-right:5px;"></span>';
	code += '<span id="card_client_login" class="ipMethodTag" style="color:#FFFFFF;margin-right:5px;"></span>';
	code += '<span id="card_client_printer" class="ipMethodTag" style="color:#FFFFFF;margin-right:5px;"></span>';
	code += '<span id="card_client_iTunes" class="ipMethodTag" style="color:#FFFFFF;margin-right:5px;"></span>';
	code += '<span id="card_client_opMode" class="ipMethodTag" style="color:#FFFFFF;margin-right:5px;"></span>';
	code += '</div>';
	code += '<div id="card_client_interface" style="height:28px;width:28px;float:right;"></div>';
	code += '</td>';
	code += '</tr>';
	code += '</table>';
	code += '</td></tr>';
	//device title info. end

	code += '<tr><td colspan="2"><div class="clientList_line"></div></td></tr>';

	//device icon and device info. start
	code += '<tr>';
	code += '<td style="text-align:center;vertical-align:top;">';
	code += '<div id="card_client_preview_icon" class="client_preview_icon" title="Change client icon" onclick="card_show_custom_image();">';
	code += '<div id="card_client_image" style="width:85px;height:85px;background-size:170%;margin:0 auto;cursor:pointer;"></div>';
	code += '<canvas id="card_canvas_user_icon" class="client_canvasUserIcon" width="85px" height="85px"></canvas>';
	code += '</div>';
	code += '<div class="changeClientIcon">';
	code += '<span title="Change to default client icon" onclick="card_setDefaultIcon();">Default</span>';//untranslated
	code += '<span id="card_changeIconTitle" title="Change client icon" style="margin-left:10px;" onclick="card_show_custom_image();">Change</span>';//untranslated
	code += '</div>';
	code += '</td>';

	code += '<td style="vertical-align:top;width:280px;">';

	code += '<div>';
	code += '<input id="card_client_name" name="card_client_name" type="text" value="" class="input_32_table" maxlength="32" style="width:275px;">';
	code += '</div>';
	code += '<div style="margin-top:10px;">';
	code += '<input id="client_ipaddr_field" type="text" value="" class="input_32_table client_input_text_disabled" disabled>';
	code += '</div>';
	code += '<div style="margin-top:10px;">';
	code += '<input id="client_macaddr_field" type="text" value="" class="input_32_table client_input_text_disabled" disabled>';
	code += '</div>';
	code += '<div style="margin-top:10px;">';
	code += '<input id="client_manufacturer_field" type="text" value="Loading manufacturer.." class="input_32_table client_input_text_disabled" disabled>';
	code += '</div>';
	code += '</td>';
	code += '</tr>';
	//device icon and device info. end

	//device icon list start
	code += '<tr>';
	code += '<td colspan="2">';
	code += '<div id="card_custom_image" class="client_icon_list" style="display:none;">';
	code += '<table width="99%;" id="tbCardClientListIcon" border="1" align="center" cellpadding="4" cellspacing="0">';
	code += '</table>';
	code += '</td>';
	code += '</tr>';
	//device icon list end

	code += '<tr>';
	code += '<td colspan="2" style="text-align: center;">';
	code += '<input id="card_client_confirm" class="button_gen" type="button" onclick="card_confirm(\''+callBack+'\');" value="<#CTL_apply#>">';
	code += '<img id="card_client_loadingIcon" style="margin-left:5px;display:none;" src="/images/InternetScan.gif">';
	code += '</td>';
	code += '</tr>';
	code += '</tbody></table>';

	divObj.innerHTML = code;
	obj.parentNode.appendChild(divObj);
	//Clear the last record clicked obj
	if(temp_clickedObj != null) {
		if(temp_clickedObj.className.search("clientIcon") != -1) {
			temp_clickedObj.className = temp_clickedObj.className.replace("clientIcon_clicked","clientIcon");
			temp_clickedObj.className = temp_clickedObj.className.replace(" card_clicked", "");
		}
		else if(temp_clickedObj.className.search("venderIcon") != -1) {
			temp_clickedObj.className = temp_clickedObj.className.replace("venderIcon_clicked","venderIcon");
			temp_clickedObj.className = temp_clickedObj.className.replace(" card_clicked", "");
		}
		temp_clickedObj = null;
	}
	temp_clickedObj = obj;
	if(obj.className.search("clientIcon") != -1) {
		obj.className = obj.className.replace("clientIcon","clientIcon_clicked");
		obj.className = obj.className  + " card_clicked";
	}
	else if(obj.className.search("venderIcon") != -1) {
		obj.className = obj.className.replace("venderIcon","venderIcon_clicked");
		obj.className = obj.className  + " card_clicked";
	}
	
	fadeIn(document.getElementById("edit_client_block"));
	document.body.onclick = function() {hide_edit_client_block();}
	document.getElementById("edit_client_block").onclick = function() {show_edit_client_block();}

	//build device icon list start
	var clientListIconArray = ["1", "2", "4", "5", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18"];
	var eachColCount = 6;
	var colCount = parseInt(clientListIconArray.length / eachColCount) + 1;
	for(var rowIndex = 0; rowIndex < colCount; rowIndex += 1) {
		var row = document.getElementById("tbCardClientListIcon").insertRow(rowIndex);
		row.id = "tbCardClientListIcon" + rowIndex;
		for(var colIndex = 0; colIndex < eachColCount; colIndex += 1) {
			var cell = row.insertCell(-1);
			cell.id = "tbCardClientListIcon" + (colIndex + (rowIndex * 6));
			if(clientListIconArray[colIndex + (rowIndex * eachColCount)] != undefined) {
				cell.innerHTML = '<div class="type' + clientListIconArray[colIndex + (rowIndex * eachColCount)] + '" onclick="select_image(this.className,\''+clientInfo.dpiVender+'\');"></div>';
			}
			else {
				if(usericon_support) {
					if(document.getElementById("tdCardUserIcon") == null) {
						cell.id = "tdCardUserIcon";
						cell.className = "client_icon_list_td";
						cell.innerHTML = '<div id="divCardUserIcon" class="client_upload_div" style="display:none;">+' +
						'<input type="file" name="cardUploadIcon" id="cardUploadIcon" class="client_upload_file" onchange="previewCardUploadIcon(this);" title="Upload client icon" /></div>';/*untranslated*/
					}
				}
			}
		}
	}
	//build device icon list end

	//settup value
	//initial state start
	document.getElementById("card_client_preview_icon").ondrop = null;
	//initial state end

	//device title info. start
	document.getElementById("card_client_name").value = (clientInfo.nickName == "") ? clientInfo.name : clientInfo.nickName;

	var convRSSI = function(val) {
		if(val == "") return "wired";

		val = parseInt(val);
		if(val >= -50) return 4;
		else if(val >= -80)	return Math.ceil((24 + ((val + 80) * 26)/10)/25);
		else if(val >= -90)	return Math.ceil((((val + 90) * 26)/10)/25);
		else return 1;
	};

	document.getElementById("card_client_ipMethod").style.display = "none";
	document.getElementById("card_client_login").style.display = "none";
	document.getElementById("card_client_printer").style.display = "none";
	document.getElementById("card_client_iTunes").style.display = "none";
	document.getElementById("card_client_opMode").style.display = "none";
	if(clientInfo.isOnline) {
		var rssi_t = 0;
		var connectModeTip = "";
		var clientIconHtml = "";
		rssi_t = convRSSI(clientInfo.rssi);
		if(isNaN(rssi_t)) {
			connectModeTip = "<#tm_wired#>";
		}
		else {
			switch(rssi_t) {
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
				if(clientInfo.curTx != "")
					connectModeTip += "Tx Rate: " + clientInfo.curTx + "\n"; /*untranslated*/
				if(clientInfo.curRx != "")
					connectModeTip += "Rx Rate: " + clientInfo.curRx + "\n"; /*untranslated*/
				connectModeTip += "<#Access_Time#>: " + clientInfo.wlConnectTime + "";
			}
		}

		if(sw_mode != 4){
			clientIconHtml += '<div class="radioIcon radio_' + rssi_t +'" title="' + connectModeTip + '"></div>';
			if(clientInfo.isWL != 0) {
				var bandClass = (navigator.userAgent.toUpperCase().match(/CHROME\/([\d.]+)/)) ? "band_txt_chrome" : "band_txt";
				clientIconHtml += '<div class="band_block"><span class="' + bandClass + '" style="color:#000000;">' + wl_nband_title[clientInfo.isWL-1].replace("Hz", "") + '</span></div>';
			}
			document.getElementById('card_client_interface').innerHTML = clientIconHtml;
			document.getElementById("card_client_interface").title = connectModeTip;
		}
	}

	if(clientInfo.isOnline) {
		if(sw_mode == "1") {
			document.getElementById("card_client_ipMethod").style.display = "";
			document.getElementById("card_client_ipMethod").innerHTML = clientInfo.ipMethod;
			document.getElementById("card_client_ipMethod").onmouseover = function() {return overlib(ipState[clientInfo.ipMethod]);};
			document.getElementById("card_client_ipMethod").onmouseout = function() {nd();};
		}
	}
	else {
		document.getElementById("card_client_ipMethod").style.display = "";
		document.getElementById("card_client_ipMethod").innerHTML = "Off Line"; /*untranslated*/
		document.getElementById("card_client_ipMethod").onmouseover = function() {return overlib(ipState["OffLine"]);};
		document.getElementById("card_client_ipMethod").onmouseout = function() {nd();};
		document.getElementById("card_client_interface").style.display = "none";
	}
	if(clientInfo.isLogin) {
		document.getElementById("card_client_login").style.display = "";
		document.getElementById("card_client_login").innerHTML = "logged-in-user"; /*untranslated*/
	}
	if(clientInfo.isPrinter) {
		document.getElementById("card_client_printer").style.display = "";
		document.getElementById("card_client_printer").innerHTML = "Printer"; /*untranslated*/
	}
	if(clientInfo.isITunes) {
		document.getElementById("card_client_iTunes").style.display = "";
		document.getElementById("card_client_iTunes").innerHTML = "iTunes"; /*untranslated*/
	}
	if(clientInfo.opMode != 0) {
		var opModeDes = ["none", "<#wireless_router#>", "<#OP_RE_item#>", "<#OP_AP_item#>", "<#OP_MB_item#>"];
		document.getElementById("card_client_opMode").style.display = "";
		document.getElementById("card_client_opMode").innerHTML = opModeDes[clientInfo.opMode];
	}
	//device title info. end

	//device icon and device info. start
	document.getElementById("client_ipaddr_field").value = clientInfo.ip;
	document.getElementById("client_macaddr_field").value = clientInfo.mac;
	select_image("type" + parseInt(clientInfo.type), clientInfo.dpiVender);
	if(client_manual_dhcp_list_array[clientInfo.mac] != undefined) { //check mac>ip is combination the the ipLockIcon is manual
		var client_manual_ip = client_manual_dhcp_list_array[clientInfo.mac];
		//handle device offine but dhcp had been setted.
		if(clientInfo.ip == "offline") {
			document.getElementById("client_ipaddr_field").value = client_manual_ip;
		}
	}

	var deviceTitle = (clientInfo.dpiDevice == "") ? clientInfo.dpiVender : clientInfo.dpiDevice;
	if(deviceTitle == undefined || deviceTitle == "") {
		setTimeout(function(){
			if('<% nvram_get("x_Setting"); %>' == '1' && wanConnectStatus && clientInfo.internetState)
				oui_query_card(clientInfo.mac);
		}, 1000);
	}
	else {
		document.getElementById("client_manufacturer_field").value = deviceTitle;
		document.getElementById("client_manufacturer_field").title = "";
		if(deviceTitle.length > 28) {
			document.getElementById("client_manufacturer_field").value = deviceTitle.substring(0, 26) + "..";
			document.getElementById("client_manufacturer_field").title = deviceTitle;
		}
	}
	//device icon and device info. end

	//setting user upload icon attribute start.
	//1.check rc_support
	if(usericon_support) {
		//2.check browswer support File Reader and Canvas or not.
		if(isSupportFileReader() && isSupportCanvas()) {
			document.getElementById("divCardUserIcon").style.display = "";
			//Setting drop event
			var holder = document.getElementById("card_client_preview_icon");
			holder.ondragover = function () { return false; };
			holder.ondragend = function () { return false; };
			holder.ondrop = function (e) {
				e.preventDefault();
				var userIconLimitFlag = userIconNumLimit(document.getElementById("client_macaddr_field").value);
				if(userIconLimitFlag) {	//not over 100	
					var file = e.dataTransfer.files[0];
					//check image
					if(file.type.search("image") != -1) {
						var reader = new FileReader();
						reader.onload = function (event) {
							var img = document.createElement("img");
							img.src = event.target.result;
							var mimeType = img.src.split(",")[0].split(":")[1].split(";")[0];
							var canvas = document.getElementById("card_canvas_user_icon");
							var ctx = canvas.getContext("2d");
							ctx.clearRect(0,0,85,85);
							document.getElementById("card_client_image").style.display = "none";
							document.getElementById("card_canvas_user_icon").style.display = "";
							setTimeout(function() {
								ctx.drawImage(img, 0, 0, 85, 85);
								var dataURL = canvas.toDataURL(mimeType);
								userIconBase64 = dataURL;
							}, 100); //for firefox FPS(Frames per Second) issue need delay
						};
						reader.readAsDataURL(file);
						return false;
					}
					else {
						alert("<#Setting_upload_hint#>");
						return false;
					}
				}
				else {	//over 100 then let usee select delete icon or nothing
					showUploadIconList();
				}
			};
		} 
	}
	//setting user upload icon attribute end.

	//form
	if(document.getElementById("card_clientlist_form") != null) {
		removeElement(document.getElementById("card_clientlist_form"));
	}
	var formHTML = "";
	var formObj = document.createElement("form");
	document.body.appendChild(formObj);
	formObj.method = "POST";
	formObj.setAttribute("id","card_clientlist_form");
	formObj.setAttribute("name","card_clientlist_form");
	formObj.action = "/start_apply2.htm";
	formObj.target = "hidden_frame";

	var currentURL = "";
	if(location.pathname == "/")
		currentURL = "index.asp";
	else
		currentURL = location.pathname.substring(location.pathname.lastIndexOf('/') + 1);

	formHTML += '<input type="hidden" name="current_page" value=' + currentURL + '>';
	formHTML += '<input type="hidden" name="next_page" value=' + currentURL + '>';
	formHTML += '<input type="hidden" name="modified" value="0">';
	formHTML += '<input type="hidden" name="flag" value="background">';
	formHTML += '<input type="hidden" name="action_mode" value="apply">';
	formHTML += '<input type="hidden" name="action_script" value="saveNvram">';
	formHTML += '<input type="hidden" name="action_wait" value="1">';
	formHTML += '<input type="hidden" name="custom_clientlist" value="">';
	formHTML += '<input type="hidden" name="custom_usericon" value="">';
	formHTML += '<input type="hidden" name="custom_usericon_del" value="" disabled>';
	formObj.innerHTML = formHTML;
	card_firstTimeOpenBlock = true;
}

function previewCardUploadIcon(imageObj) {
	var userIconLimitFlag = userIconNumLimit(document.getElementById("client_macaddr_field").value);

	if(userIconLimitFlag) {	//not over 100
		var checkImageExtension = function (imageFileObject) {
		var  picExtension= /\.(jpg|jpeg|gif|png|bmp|ico)$/i;  //analy extension
			if (picExtension.test(imageFileObject)) 
				return true;
			else
				return false;
		};

		//1.check image extension
		if (!checkImageExtension(imageObj.value)) {
			alert("<#Setting_upload_hint#>");
			imageObj.focus();
		}
		else {
			//2.Re-drow image
			var fileReader = new FileReader(); 
			fileReader.onload = function (fileReader) {
				var img = document.createElement("img");
				img.src = fileReader.target.result;
				var mimeType = img.src.split(",")[0].split(":")[1].split(";")[0];
				var canvas = document.getElementById("card_canvas_user_icon");
				var ctx = canvas.getContext("2d");
				ctx.clearRect(0,0,85,85);
				document.getElementById("card_client_image").style.display = "none";
				document.getElementById("card_canvas_user_icon").style.display = "";
				setTimeout(function() {
					ctx.drawImage(img, 0, 0, 85, 85);
					var dataURL = canvas.toDataURL(mimeType);
					userIconBase64 = dataURL;
				}, 100); //for firefox FPS(Frames per Second) issue need delay
			}
			fileReader.readAsDataURL(imageObj.files[0]);
			userIconHideFlag = true;
		}
	}
	else {	//over 100 then let usee select delete icon or nothing
		showUploadIconList();
	}
}

function card_show_custom_image(flag) {
	if(!slideFlag) {
		var display_state = document.getElementById("card_custom_image").style.display;
		if(display_state == "none") {
			slideFlag = true;
			slideDown("card_custom_image", 500);
			document.getElementById("card_changeIconTitle").innerHTML = "<#CTL_close#>";
		}
		else {
			slideFlag = true;
			slideUp("card_custom_image", 500);
			document.getElementById("card_changeIconTitle").innerHTML = "Change";/*untranslated*/
		}
	}
}
function card_confirm(callBack) {
	var validClientListForm = function() {
		document.getElementById("card_client_name").value = document.getElementById("card_client_name").value.trim();
		if(document.getElementById("card_client_name").value.length == 0){
			alert("<#File_Pop_content_alert_desc1#>");
			document.getElementById("card_client_name").style.display = "";
			document.getElementById("card_client_name").focus();
			document.getElementById("card_client_name").select();
			return false;
		}
		else if(document.getElementById("card_client_name").value.indexOf(">") != -1 || document.getElementById("card_client_name").value.indexOf("<") != -1){
			alert("<#JS_validstr2#> '<', '>'");
			document.getElementById("card_client_name").focus();
			document.getElementById("card_client_name").select();
			document.getElementById("card_client_name").value = "";		
			return false;
		}
		else if(!validator.haveFullWidthChar(document.getElementById("card_client_name"))) {
			return false;
		}
		return true;
	};
	var custom_name = originData.customList;
	if(validClientListForm()){
		document.card_clientlist_form.custom_clientlist.disabled = false;
		// customize device name
		var originalCustomListArray = new Array();
		var onEditClient = new Array();
		var clientTypeNum = "";
		if(document.getElementById('card_client_image').className.search("venderIcon") != -1) {
			clientTypeNum = "0";
		}
		else {
			clientTypeNum = document.getElementById("card_client_image").className.replace("clientIcon_no_hover type", "");
			if(clientTypeNum == "0_viewMode") {
				clientTypeNum = "0";
			}
		}
		originalCustomListArray = custom_name;
		onEditClient[0] = document.getElementById("card_client_name").value.trim();
		onEditClient[1] = document.getElementById("client_macaddr_field").value;
		onEditClient[2] = 0;
		onEditClient[3] = clientTypeNum;
		onEditClient[4] = "";
		onEditClient[5] = "";

		for(var i=0; i<originalCustomListArray.length; i++){
			if(originalCustomListArray[i].split('>')[1] == onEditClient[1]){
				onEditClient[4] = originalCustomListArray[i].split('>')[4]; // set back callback for ROG device
				onEditClient[5] = originalCustomListArray[i].split('>')[5]; // set back keeparp for ROG device
				originalCustomListArray.splice(i, 1); // remove the selected client from original list
			}
		}

		originalCustomListArray.push(onEditClient.join('>'));
		custom_name = originalCustomListArray.join('<');
		document.card_clientlist_form.custom_clientlist.value = custom_name;

		// handle user image
		document.card_clientlist_form.custom_usericon.disabled = true;
		if(usericon_support) {
			document.card_clientlist_form.custom_usericon.disabled = false;
			var clientMac = document.getElementById("client_macaddr_field").value.replace(/\:/g, "");
			if(userIconBase64 != "NoIcon") {
				document.card_clientlist_form.custom_usericon.value = clientMac + ">" + userIconBase64;
			}
			else {
				document.card_clientlist_form.custom_usericon.value = clientMac + ">noupload";
			}
		}

		// submit card_clientlist_form
		document.card_clientlist_form.submit();

		// display waiting effect
		document.getElementById("card_client_loadingIcon").style.display = "";
		
		setTimeout(function() {
			var updateClientListObj = function () {
				$.ajax({
					url: '/update_clients.asp',
					dataType: 'script', 
					error: function(xhr) {
						setTimeout("updateClientListObj();", 1000);
					},
					success: function(response){
						genClientList();
						switch(callBack) {
							case "DHCP" :
								showLANIPList();
								showdhcp_staticlist();
								break;
							case "WOL" :
								showLANIPList();
								showwollist();
								break;
							case "ACL" :
								showWLMACList();
								show_wl_maclist_x();
								break;
							case "ParentalControl" :
								showLANIPList();
								gen_mainTable();
								break;
							case "GuestNetwork" :
								showWLMACList();
								show_wl_maclist_x();
								break;
							case "WebProtector" :
								genMain_table();
								break;
							default :
								refreshpage();
						}
					}
				});
			};
			updateClientListObj();
		}, document.card_clientlist_form.action_wait.value * 1000);
	}
}
function card_setDefaultIcon() {
	var mac = document.getElementById("client_macaddr_field").value;
	var defaultType = "0";
	var defaultDpiVender = "";
	if(clientList[mac] != undefined) {
		defaultType = clientList[mac].defaultType;
		defaultDpiVender = clientList[mac].dpiVender;
	}
	select_image("type" + defaultType, defaultDpiVender);
}

//check user icon num is over 100 or not.
function userIconNumLimit(mac) {
	var flag = true;
	var uploadIconMacList = getUploadIconList().replace(/\.log/g, "");
	var selectMac = mac.replace(/\:/g, "");
	var existFlag = (uploadIconMacList.search(selectMac) == -1) ? false : true;
	//check mac exist or not
	if(!existFlag) {
		var userIconCount = getUploadIconCount();
		if(userIconCount >= 100) {	//mac not exist, need check use icnon number whether over 100 or not.
			flag = false;
		}
	}
	return flag;
}
function showUploadIconList() {
	var confirmFlag = true;
	confirmFlag = confirm("The client icon over upload limting, please remove at least one client icon then try to upload again."); /*untranslated*/
	if(confirmFlag) {

		hide_edit_client_block();
		if(document.getElementById("edit_client_block") != null) {
			document.getElementById("edit_client_block").remove();
		}

		if(document.getElementById("edit_uploadicons_block") != null) {
			document.getElementById("edit_uploadicons_block").remove();
		}

		var divObj = document.createElement("div");
		divObj.setAttribute("id","edit_uploadicons_block");
		divObj.className = "usericons_content";

		var code = "";
		code += '<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:15px;">';
		code += '<thead><tr>';
		code += '<td colspan="4">Client upload icon&nbsp;(<#List_limit#>&nbsp;100)</td>'; /*untranslated*/
		code += '</tr></thead>';
		code += '<tr>';
		code += '<th width="45%"><#ParentalCtrl_username#></th>';
		code += '<th width="30%"><#ParentalCtrl_hwaddr#></th>';
		code += '<th width="15%">Upload icon</th>'; /*untranslated*/
		code += '<th width="10%"><#CTL_del#></th>';
		code += '</tr>';
		code += '</table>';
		code += '<div id="card_usericons_block"></div>';
		code += '<div style="margin-top:15px;padding-bottom:10px;width:100%;text-align:center;">';
		code += '<input class="button_gen" type="button" onclick="uploadIcon_cancel();" value="<#CTL_Cancel#>">';
		code += '<input class="button_gen" type="button" onclick="uploadIcon_confirm();" value="<#CTL_ok#>">';
		code += '<img id="card_upload_loadingIcon" style="margin-left:5px;display:none;" src="/images/InternetScan.gif">';
		code += '</div>	';
		divObj.innerHTML = code;
		document.body.appendChild(divObj);

		document.getElementById("edit_uploadicons_block").style.display = "block";

		showUploadIconsTable();
		return false;
	}
	else {
		document.getElementById("cardUploadIcon").value = "";
		return false;
	}
}
function showUploadIconsTable() {
	genClientList();
	var uploadIconMacList = getUploadIconList().replace(/\.log/g, "");
	var custom_usericon_row = uploadIconMacList.split('>');
	var code = "";
	var clientIcon = "";
	var custom_usericon_length = custom_usericon_row.length;
	code +='<table width="95%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="cardUploadIcons_table">';
	if(custom_usericon_length == 1) {
		code +='<tr><td style="color:#FFCC00;" colspan="4"><#IPConnection_VSList_Norule#></td></tr>';
		document.getElementById('edit_uploadicons_block').style.height = "170px";
	}
	else {
		for(var i = 0; i < custom_usericon_length; i += 1) {
			if(custom_usericon_row[i] != "") {
				var formatMac = custom_usericon_row[i].slice(0,2) + ":" + custom_usericon_row[i].slice(2,4) + ":" + custom_usericon_row[i].slice(4,6) + ":" + 
								custom_usericon_row[i].slice(6,8) + ":" + custom_usericon_row[i].slice(8,10)+ ":" + custom_usericon_row[i].slice(10,12);
				code +='<tr>';
				var clientObj = clientList[formatMac];
				var clientName = "";
				if(clientObj != undefined) {
					clientName = (clientObj.nickName.toString() == "") ? clientObj.name.toString() : clientObj.nickName.toString();
				}
				code +='<td width="45%">'+ clientName +'</td>';
				code +='<td width="30%">'+ formatMac +'</td>';
				clientIcon = getUploadIcon(custom_usericon_row[i]);
				code +='<td width="15%"><img id="imgUploadIcon_'+ i +'" class="card_imgUploadIcon" src="' + clientIcon + '"</td>';
				code +='<td width="10%"><input class="remove_btn" onclick="delUploadIcon(this);" value=""/></td></tr>';
			}
		}
		document.getElementById('edit_uploadicons_block').style.height = (61 * custom_usericon_length + 75) + "px";
	}
	code +='</table>';
	document.getElementById("card_usericons_block").innerHTML = code;
};
function delUploadIcon(rowdata) {
	var delIdx = rowdata.parentNode.parentNode.rowIndex;
	var delMac = rowdata.parentNode.parentNode.childNodes[1].innerHTML;
	document.getElementById("cardUploadIcons_table").deleteRow(delIdx);
	card_custom_usericon_del += delMac + ">";
	var trCount = document.getElementById("cardUploadIcons_table").rows.length;
	document.getElementById('edit_uploadicons_block').style.height = (61 * (trCount + 1) + 75) + "px";
	if(trCount == 0) {
		var code = "";
		code +='<table width="95%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="cardUploadIcons_table">';
		code +='<tr><td style="color:#FFCC00;" colspan="4"><#IPConnection_VSList_Norule#></td></tr>';
		code +='</table>';
		document.getElementById('edit_uploadicons_block').style.height = "170px";
		document.getElementById("card_usericons_block").innerHTML = code;
	}
}
function uploadIcon_confirm() {
	document.card_clientlist_form.custom_clientlist.disabled = true;
	document.card_clientlist_form.custom_usericon.disabled = true;
	document.card_clientlist_form.custom_usericon_del.disabled = false;
	document.card_clientlist_form.custom_usericon_del.value = card_custom_usericon_del.replace(/\:/g, "");

	// submit card_clientlist_form
	document.card_clientlist_form.submit();
	document.getElementById("card_upload_loadingIcon").style.display = "";
	setTimeout(function(){
		refreshpage();
		card_custom_usericon_del = "";
		document.card_clientlist_form.custom_usericon_del.disabled = true;
	}, document.card_clientlist_form.action_wait.value * 1000);
}
function uploadIcon_cancel() {
	card_custom_usericon_del = "";
	document.getElementById("edit_uploadicons_block").style.display = "none";
}

function select_image(type,  vender) {
	var sequence = type.substring(4,type.length);
	document.getElementById("card_client_image").style.display = "none";
	document.getElementById("card_canvas_user_icon").style.display = "none";
	var icon_type = type;
	if(type == "type0" || type == "type6") {
		icon_type = "type0_viewMode";
	}
	document.getElementById("card_client_image").className = "clientIcon_no_hover "+ icon_type;
	if(vender != "" && (type == "type0" || type == "type6")) {
		var venderIconClassName = getVenderIconClassName(vender.toLowerCase());
		if(venderIconClassName != "") {
			document.getElementById("card_client_image").className = "venderIcon_no_hover "+ venderIconClassName;
		}
	}

	var userImageFlag = false;
	if(!card_firstTimeOpenBlock) {
		if(usericon_support) {
			var clientMac = document.getElementById('client_macaddr_field').value.replace(/\:/g, "");
			userIconBase64 = getUploadIcon(clientMac);
			if(userIconBase64 != "NoIcon") {
				var img = document.createElement("img");
				img.src = userIconBase64;
				var canvas = document.getElementById("card_canvas_user_icon");
				var ctx = canvas.getContext("2d");
				ctx.clearRect(0,0,85,85);
				document.getElementById("card_client_image").style.display = "none";
				document.getElementById("card_canvas_user_icon").style.display = "";
				ctx.drawImage(img, 0, 0, 85, 85);
				userImageFlag = true;
			}
		}
	}

	if(!userImageFlag) {
		userIconBase64 = "NoIcon";
		document.getElementById("card_client_image").style.display = "";
	}
}

function oui_query_card(mac) {
	var tab = new Array();
	tab = mac.split(mac.substr(2,1));
	$.ajax({
		url: 'http://standards.ieee.org/cgi-bin/ouisearch?'+ tab[0] + '-' + tab[1] + '-' + tab[2],
		type: 'GET',
		success: function(response) {
			if(document.getElementById("edit_client_block") == null) return true;
			if(mac != document.getElementById("client_macaddr_field").value) {//avoid click two device quickly
				oui_query_card(document.getElementById("client_macaddr_field").value);
			}
			else {
				if(response.responseText.search("Sorry!") == -1) {
					var retData = response.responseText.split("pre")[1].split("(hex)")[1].split(tab[0] + tab[1] + tab[2])[0].split("&lt;/");
					document.getElementById("client_manufacturer_field").value = retData[0].trim();
					document.getElementById("client_manufacturer_field").title = "";
					if(retData[0].trim().length > 28) {
						document.getElementById("client_manufacturer_field").value = retData[0].trim().substring(0, 26) + "..";
						document.getElementById("client_manufacturer_field").title = retData[0].trim();
					}
				}
			}
		}
	});
}

/*
 * elem 
 * speed, default is 20, optional
 * opacity, default is 100, range is 0~100, optional
 */
function fadeIn(elem, speed, opacity){
	if(elem != null) {
		var setOpacity = function(ev, v) {
			ev.filters ? ev.style.filter = 'alpha(opacity=' + v + ')' : ev.style.opacity = v / 100;
		};
		speed = speed || 20;
		opacity = opacity || 100;

		//initial element display and set opacity is 0
		elem.style.display = "block";
		setOpacity(elem, 0);

		var val = 0;
		//loop add 5 the opacity value
		(function(){
			setOpacity(elem, val);
			val += 5;
			if (val <= opacity) {
				setTimeout(arguments.callee, speed);
			}
		})();
	}
}

/*
 * elem 
 * speed, default is 20, optional
 * opacity, default is 0, range is 0~100, optional
 */
function fadeOut(elem, speed, opacity) {
	if(elem != null) {
		var setOpacity = function(ev, v) {
			ev.filters ? ev.style.filter = 'alpha(opacity=' + v + ')' : ev.style.opacity = v / 100;
		};
		speed = speed || 20;
		opacity = opacity || 0;

		var val = 100;

		(function(){
			setOpacity(elem, val);
			val -= 5;
			if (val >= opacity) {
				setTimeout(arguments.callee, speed);
			}
			else if (val < 0) {
				elem.style.display = "none";
			}
		})();
	}
}

var slideFlag = false;
function slideDown(objnmae, speed) {
	var obj = document.getElementById(objnmae);
	var mySpeed = speed || 300;
	var intervals = mySpeed / 30; // we are using 30 ms intervals
	var holder = document.createElement('div');
	var parent = obj.parentNode;
	holder.setAttribute('style', 'height: 0px; overflow:hidden');
	parent.insertBefore(holder, obj);
	parent.removeChild(obj);
	holder.appendChild(obj);
	obj.style.display = obj.getAttribute("data-original-display") || "";
	var height = obj.offsetHeight;
	var sepHeight = height / intervals;
	var timer = setInterval(function() {
		var holderHeight = holder.offsetHeight;
		if (holderHeight + sepHeight < height) {
			holder.style.height = (holderHeight + sepHeight) + 'px';
		} 
		else {
			// clean up
			holder.removeChild(obj);
			parent.insertBefore(obj, holder);
			parent.removeChild(holder);
			clearInterval(timer);
			slideFlag = false;
		}
	}, 30);
}

function slideUp(objnmae, speed) {
	var obj = document.getElementById(objnmae);
	var mySpeed = speed || 300;
	var intervals = mySpeed / 30; // we are using 30 ms intervals
	var height = obj.offsetHeight;
	var holder = document.createElement('div');
	var parent = obj.parentNode;
	holder.setAttribute('style', 'height: ' + height + 'px; overflow:hidden');
	parent.insertBefore(holder, obj);
	parent.removeChild(obj);
	holder.appendChild(obj);
	var originalDisplay = (obj.style.display !== 'none') ? obj.style.display : '';
	obj.setAttribute("data-original-display", originalDisplay);
	var sepHeight = height / intervals;
	var timer = setInterval(function() {
		var holderHeight = holder.offsetHeight;
		if (holderHeight - sepHeight > 0) {
			holder.style.height = (holderHeight - sepHeight) + 'px';
		}
		else {
			// clean up
			obj.style.display = 'none';
			holder.removeChild(obj);
			parent.insertBefore(obj, holder);
			parent.removeChild(holder);
			clearInterval(timer);
			slideFlag = false;
		}
	}, 30);
}

function registerIframeClick(iframeName, action) {
	var iframe = document.getElementById(iframeName);
	if(iframe != null) {
		var iframeDoc = iframe.contentDocument || iframe.contentWindow.document;

		if (typeof iframeDoc.addEventListener != "undefined") {
			iframeDoc.addEventListener("click", action, false);
		}
		else if (typeof iframeDoc.attachEvent != "undefined") {
			iframeDoc.attachEvent ("onclick", action);
		}
	}
}

function removeIframeClick(iframeName, action) {
	var iframe = document.getElementById(iframeName);
	if(iframe != null) {
		var iframeDoc = iframe.contentDocument || iframe.contentWindow.document;

		if (typeof iframeDoc.removeEventListener != "undefined") {
			iframeDoc.removeEventListener("click", action, false);
		}
		else if (typeof iframeDoc.detachEvent != "undefined") {
			iframeDoc.detachEvent ("onclick", action);
		}
	}
}

var all_list = new Array();//All
var wired_list = new Array();
var wl1_list = new Array();//2.4G
var wl2_list = new Array();//5G
var wl3_list = new Array();//5G-2

var sorter = {
	"indexFlag" : 3 , // default sort is by IP
	"all_index" : 3,
	"all_display" : true,
	"wired_index" : 3,
	"wired_display" : true,
	"wl1_index" : 3,
	"wl1_display" : true,
	"wl2_index" : 3,
	"wl2_display" : true,
	"wl3_index" : 3,
	"wl3_display" : true,
	"sortingMethod" : "increase", 
	"sortingMethod_wired" : "increase", 
	"sortingMethod_wl1" : "increase", 
	"sortingMethod_wl2" : "increase", 
	"sortingMethod_wl3" : "increase", 

	"num_increase" : function(a, b) {
		if(sorter.indexFlag == 3) { //IP
			var a_num = 0, b_num = 0;
			a_num = inet_network(a[sorter.indexFlag]);
			b_num = inet_network(b[sorter.indexFlag]);
			return parseInt(a_num) - parseInt(b_num);
		}
		else if(sorter.indexFlag == 5 || sorter.indexFlag == 6 || sorter.indexFlag == 7) { //Interface, Tx, Rx
			var a_num = 0, b_num = 0;
			a_num = (a[sorter.indexFlag] == "") ? 0 : a[sorter.indexFlag];
			b_num = (b[sorter.indexFlag] == "") ? 0 : b[sorter.indexFlag];
			return parseInt(a_num) - parseInt(b_num);
		}
		else {
			return parseInt(a[sorter.indexFlag]) - parseInt(b[sorter.indexFlag]);
		}
	},
	"num_decrease" : function(a, b) {
		var a_num = 0, b_num = 0;
		if(sorter.indexFlag == 3) { //IP
			var a_num = 0, b_num = 0;
			a_num = inet_network(a[sorter.indexFlag]);
			b_num = inet_network(b[sorter.indexFlag]);
			return parseInt(b_num) - parseInt(a_num);
		}
		else if(sorter.indexFlag == 5 || sorter.indexFlag == 6 || sorter.indexFlag == 7) { //Interface, Tx, Rx
			var a_num = 0, b_num = 0;
			a_num = (a[sorter.indexFlag] == "") ? 0 : a[sorter.indexFlag];
			b_num = (b[sorter.indexFlag] == "") ? 0 : b[sorter.indexFlag];
			return parseInt(b_num) - parseInt(a_num);
		}
		else {
			return parseInt(b[sorter.indexFlag]) - parseInt(a[sorter.indexFlag]);
		}
	},
	"str_increase" : function(a, b) {
		if(a[sorter.indexFlag].toUpperCase() == b[sorter.indexFlag].toUpperCase()) return 0;
		else if(a[sorter.indexFlag].toUpperCase() > b[sorter.indexFlag].toUpperCase()) return 1;
		else return -1;
	},
	"str_decrease" : function(a, b) {
		if(a[sorter.indexFlag].toUpperCase() == b[sorter.indexFlag].toUpperCase()) return 0;
		else if(a[sorter.indexFlag].toUpperCase() > b[sorter.indexFlag].toUpperCase()) return -1;
		else return 1;
	},
	"addBorder" : function(obj) {
		var objIndex = obj;
		var clickItem = obj.parentNode.id.split("_")[1];
		var sorterLastIndex = 0;
		var sorterClickIndex = 0;
		while( (objIndex = objIndex.previousSibling) != null ) 
			sorterClickIndex++;

		switch (clickItem) {
			case "all" :
				sorterLastIndex = sorter.all_index;
				sorter.all_index = sorterClickIndex;
				sorter.sortingMethod = (sorter.sortingMethod == "increase") ? "decrease" : "increase";
				break;
			case "wired" :
				sorterLastIndex = sorter.wired_index;
				sorter.wired_index = sorterClickIndex;
				sorter.sortingMethod_wired = (sorter.sortingMethod_wired == "increase") ? "decrease" : "increase";
				break;
			case "wl1" :
				sorterLastIndex = sorter.wl1_index;
				sorter.wl1_index = sorterClickIndex;
				sorter.sortingMethod_wl1 = (sorter.sortingMethod_wl1 == "increase") ? "decrease" : "increase";
				break;
			case "wl2" :
				sorterLastIndex = sorter.wl2_index;
				sorter.wl2_index = sorterClickIndex;
				sorter.sortingMethod_wl2 = (sorter.sortingMethod_wl2 == "increase") ? "decrease" : "increase";
				break;
			case "wl3" :
				sorterLastIndex = sorter.wl3_index;
				sorter.wl3_index = sorterClickIndex;
				sorter.sortingMethod_wl3 = (sorter.sortingMethod_wl3 == "increase") ? "decrease" : "increase";
				break;	
		}
		obj.parentNode.childNodes[sorterLastIndex].style.borderTop = '1px solid #222';
		obj.parentNode.childNodes[sorterLastIndex].style.borderBottom = '1px solid #222';	
	},
	"drawBorder" : function(_arrayName) {
		var clickItem = _arrayName.split("_")[0];
		var clickIndex = 2;
		var clickSortingMethod = "increase";
		switch (clickItem) {
			case "all" :
				clickIndex = sorter.all_index;
				clickSortingMethod = sorter.sortingMethod;
				break;
			case "wired" :
				clickIndex = sorter.wired_index;
				clickSortingMethod = sorter.sortingMethod_wired;
				break;
			case "wl1" :
				clickIndex = sorter.wl1_index;
				clickSortingMethod = sorter.sortingMethod_wl1;
				break;
			case "wl2" :
				clickIndex = sorter.wl2_index;
				clickSortingMethod = sorter.sortingMethod_wl2;
				break;
			case "wl3" :
				clickIndex = sorter.wl3_index;
				clickSortingMethod = sorter.sortingMethod_wl3;
				break;
		}
		var borderTopCss = "";
		var borderBottomCss = "";
		if(getBrowser_info().ie != undefined || getBrowser_info().ie != null) {
			borderTopCss = "3px solid #FC0";
			borderBottomCss = "1px solid #FC0";
		}
		else if(getBrowser_info().firefox != undefined || getBrowser_info().firefox != null) {
			borderTopCss = "2px solid #FC0";
			borderBottomCss = "1px solid #FC0";
		}
		else {
			borderTopCss = "2px solid #FC0";
			borderBottomCss = "2px solid #FC0";
		}
		if(clickSortingMethod == "increase") {
			document.getElementById("tr_"+clickItem+"_title").childNodes[clickIndex].style.borderTop = borderTopCss;
			document.getElementById("tr_"+clickItem+"_title").childNodes[clickIndex].style.borderBottom = '1px solid #222';
		}
		else {
			document.getElementById("tr_"+clickItem+"_title").childNodes[clickIndex].style.borderTop = '1px solid #222';
			document.getElementById("tr_"+clickItem+"_title").childNodes[clickIndex].style.borderBottom = borderBottomCss;
		}
	},
	"doSorter" : function(_flag, _Method, _arrayName) {	
		// update variables
		sorter.indexFlag = _flag;
		
		// doSorter
		if(clienlistViewMode == "All") {
			eval(""+_arrayName+".sort(sorter."+_Method+"_"+sorter.sortingMethod+");");
		}
		else if(clienlistViewMode == "ByInterface") {
			switch (_arrayName) {
				case "wired_list" :
					eval(""+_arrayName+".sort(sorter."+_Method+"_"+sorter.sortingMethod_wired+");");
					break;
				case "wl1_list" :
					eval(""+_arrayName+".sort(sorter."+_Method+"_"+sorter.sortingMethod_wl1+");");
					break;
				case "wl2_list" :
					eval(""+_arrayName+".sort(sorter."+_Method+"_"+sorter.sortingMethod_wl2+");");
					break;
				case "wl3_list" :
					eval(""+_arrayName+".sort(sorter."+_Method+"_"+sorter.sortingMethod_wl3+");");
					break;
			}
		}
		drawClientListBlock(_arrayName);
		sorter.drawBorder(_arrayName);
	}
}

var edit_client_name_flag = false;
var clientlist_view_hide_flag = false;
function hide_clientlist_view_block() {
	if(clientlist_view_hide_flag) {
		fadeOut(document.getElementById("clientlist_viewlist_content"), 10, 0);
		document.body.onclick = null;
		document.body.onresize = null;
		clientListViewMacUploadIcon = [];
		removeIframeClick("statusframe", hide_clientlist_view_block);
	}
	clientlist_view_hide_flag = true;
}
function show_clientlist_view_block() {
	clientlist_view_hide_flag = false;
}

function closeClientListView() {
	fadeOut(document.getElementById("clientlist_viewlist_content"), 10, 0);
}

var clienlistViewMode = "All";
function changeClientListViewMode() {
	if(clienlistViewMode == "All")
		clienlistViewMode = "ByInterface";
	else
		clienlistViewMode = "All";

	create_clientlist_listview();
	sorterClientList();
	sorter.all_display = true;
	sorter.wired_display = true;
	sorter.wl1_display = true;
	sorter.wl2_display = true;
	sorter.wl3_display = true;
}

function pop_clientlist_listview() {
	if(document.getElementById("clientlist_viewlist_content") != null) {
		removeElement(document.getElementById("clientlist_viewlist_content"));
	}

	var divObj = document.createElement("div");
	divObj.setAttribute("id","clientlist_viewlist_content");
	divObj.className = "clientlist_viewlist";
	divObj.setAttribute("onselectstart","return false");
	document.body.appendChild(divObj);
	fadeIn(document.getElementById("clientlist_viewlist_content"));
	cal_panel_block_clientList("clientlist_viewlist_content", 0.045);

	if(document.getElementById("view_clientlist_form") != null) {
		removeElement(document.getElementById("view_clientlist_form"));
	}
	var formObj = document.createElement("form");
	formObj.setAttribute("id","view_clientlist_form");
	formObj.setAttribute("name","view_clientlist_form");
	formObj.action = "/start_apply2.htm";
	formObj.target = "hidden_frame";
	var formHtml = "";
	formHtml += "<input type='hidden' name='modified' value='0'>";
	formHtml += "<input type='hidden' name='flag' value='background'>";
	formHtml += "<input type='hidden' name='action_mode' value='apply'>";
	formHtml += "<input type='hidden' name='action_script' value='saveNvram'>";
	formHtml += "<input type='hidden' name='action_wait' value='1'>";
	formHtml += "<input type='hidden' name='custom_clientlist' value=''>";
	formObj.innerHTML = formHtml;
	document.body.appendChild(formObj);

	clientlist_view_hide_flag = false;

	create_clientlist_listview();

	setTimeout("sorterClientList();updateClientListView();", 500);
	
	registerIframeClick("statusframe", hide_clientlist_view_block);

}

function exportClientListLog() {
	var data = [["Internet access state", "Device Type", "Clients Name", "Clients IP Address", "IP Method", "Clients MAC Address", "Interface", "Tx Rate", "Rx Rate", "Access time"]];
	var tempArray = new Array();
	var ipStateExport = new Array();
	ipStateExport["Static"] =  "Static IP";
	ipStateExport["DHCP"] =  "Automatic IP";
	ipStateExport["Manual"] =  "MAC-IP Binding";
	var setArray = function(array) {
		for(var i = 0; i < array.length; i += 1) {
			tempArray = [];
			tempArray[0] = (array[i][0] == 1) ? "Allow Internet access" : "Block Internet access";
			tempArray[1] = array[i][1].replace(",", "");
			tempArray[2] = array[i][2];
			tempArray[3] = array[i][3];
			tempArray[4] = ipStateExport[clientList[array[i][4]].ipMethod];
			tempArray[5] = array[i][4];
			tempArray[6] = (array[i][9] == 0) ? "Wired" : wl_nband_title[array[i][9] - 1];
			tempArray[7] = (array[i][6] == "") ? "-" : array[i][6];
			tempArray[8] = (array[i][7] == "") ? "-" : array[i][7];
			tempArray[9] = (array[i][9] == 0) ? "-" : array[i][8];
			data.push(tempArray);
		}
	};
	switch (clienlistViewMode) {
		case "All" :
			setArray(all_list);
			break;
		case "ByInterface" :
			setArray(wired_list);
			setArray(wl1_list);
			setArray(wl2_list);
			setArray(wl3_list);
			break;
	}
	var csvContent = '';
	data.forEach(function (infoArray, index) {
		dataString = infoArray.join(',');
		csvContent += index < data.length ? dataString + '\n' : dataString;
	});

	var download = function(content, fileName, mimeType) {
		var a = document.createElement('a');
		mimeType = mimeType || 'application/octet-stream';

		if (navigator.msSaveBlob) { // IE10
			return navigator.msSaveBlob(new Blob([content], { type: mimeType }), fileName);
		} 
		else if ('download' in a) { //html5 A[download]
			a.href = 'data:' + mimeType + ',' + encodeURIComponent(content);
			a.setAttribute('download', fileName);
			document.getElementById("clientlist_viewlist_content").appendChild(a);
			setTimeout(function() {
				a.click();
				document.getElementById("clientlist_viewlist_content").removeChild(a);
			}, 66);
			return true;
		} 
		else { //do iframe dataURL download (old ch+FF):
			var f = document.createElement('iframe');
			document.getElementById("clientlist_viewlist_content").appendChild(f);
			f.src = 'data:' + mimeType + ',' + encodeURIComponent(content);

			setTimeout(function() {
				document.getElementById("clientlist_viewlist_content").removeChild(f);
				}, 333);
			return true;
		}
	};

	download(csvContent, 'ClientList.csv', 'data:text/csv;charset=utf-8');
}

function sorterClientList() {
	//initial sort ip
	var indexMapType = ["", "", "str", "num", "str", "num", "num", "num", "str"];
	switch (clienlistViewMode) {
		case "All" :
			sorter.doSorter(sorter.all_index, indexMapType[sorter.all_index], 'all_list');
			break;
		case "ByInterface" :
			sorter.doSorter(sorter.wired_index, indexMapType[sorter.wired_index], 'wired_list');
			if(wl_info.band2g_support)
				sorter.doSorter(sorter.wl1_index, indexMapType[sorter.wl1_index], 'wl1_list');
			if(wl_info.band5g_support)
				sorter.doSorter(sorter.wl2_index, indexMapType[sorter.wl2_index], 'wl2_list');
			if(wl_info.band5g_2_support)
				sorter.doSorter(sorter.wl3_index, indexMapType[sorter.wl3_index], 'wl3_list');
			break;
	}
}

function create_clientlist_listview() {
	all_list = [];
	wired_list = [];
	wl1_list = [];
	wl2_list = [];
	wl3_list = [];

	if(document.getElementById("clientlist_viewlist_block") != null) {
		removeElement(document.getElementById("clientlist_viewlist_block"));
	}

	var divObj = document.createElement("div");
	divObj.setAttribute("id","clientlist_viewlist_block");

	var obj_width_map = [["15%", "20%", "25%", "20%", "20%"],["10%", "10%", "30%", "20%", "20%", "10%"],["6%", "6%", "27%", "20%", "15%", "6%", "6%", "6%", "8%"]];
	var obj_width = stainfo_support ? obj_width_map[2] : obj_width_map[1];
	var wl_colspan = stainfo_support ? 9 : 6;

	var code = "";

	var drawSwitchMode = function(mode) {
		var drawSwitchModeHtml = "";
		drawSwitchModeHtml += "<div style='margin-top:15px;margin-left:15px;'>";
		if(mode == "All") {
			drawSwitchModeHtml += "<div class='block_filter_pressed clientlist_All'>";
			drawSwitchModeHtml += "<div class='block_filter_name' style='color:#93A9B1;'><#All#></div>";
			drawSwitchModeHtml += "</div>";
			drawSwitchModeHtml += "<div class='block_filter clientlist_ByInterface' style='cursor:pointer'>";
			drawSwitchModeHtml += "<div class='block_filter_name' onclick='changeClientListViewMode();'>By interface</div>";/*untranslated*/
			drawSwitchModeHtml += "</div>";
		}
		else {							
			drawSwitchModeHtml += "<div class='block_filter clientlist_All' style='cursor:pointer'>";
			drawSwitchModeHtml += "<div class='block_filter_name' onclick='changeClientListViewMode();'><#All#></div>";
			drawSwitchModeHtml += "</div>";
			drawSwitchModeHtml += "<div class='block_filter_pressed clientlist_ByInterface'>";
			drawSwitchModeHtml += "<div class='block_filter_name' style='color:#93A9B1;'>By interface</div>";/*untranslated*/
			drawSwitchModeHtml += "</div>";
		}
		drawSwitchModeHtml += "</div>";
		return drawSwitchModeHtml;
	};

	code += drawSwitchMode(clienlistViewMode);
	code += "<div style='text-align:right;width:30px;position:relative;margin-top:-45px;margin-left:96%;'><img src='/images/button-close.gif' style='width:30px;cursor:pointer' onclick='closeClientListView();'></div>";
	code += "<table border='0' align='center' cellpadding='0' cellspacing='0' style='width:100%;padding:15px;'><tbody><tr><td>";

	switch (clienlistViewMode) {
		case "All" :
			code += "<table width='100%' border='1' align='center' cellpadding='0' cellspacing='0' class='FormTable_table' style='margin-top:15px;'>";
			code += "<thead><tr height='28px'><td id='td_all_list_title' colspan='" + wl_colspan + "'>All list";/*untranslated*/
			code += "<a id='all_expander'class='clientlist_expander' onclick='showHideContent(\"clientlist_all_list_Block\", this);'>[ Hide ]</a>";/*untranslated*/
			code += "</td></tr></thead>";
			code += "<tr id='tr_all_title' height='40px'>";
			code += "<th width=" + obj_width[0] + "><#Internet#></th>";
			code += "<th width=" + obj_width[1] + ">Icon</th>";/*untranslated*/
			code += "<th width=" + obj_width[2] + " onclick='sorter.addBorder(this);sorter.doSorter(2, \"str\", \"all_list\");' style='cursor:pointer;'><#ParentalCtrl_username#></th>";
			code += "<th width=" + obj_width[3] + " onclick='sorter.addBorder(this);sorter.doSorter(3, \"num\", \"all_list\");' style='cursor:pointer;'>Clients IP Address</th>";/*untranslated*/
			code += "<th width=" + obj_width[4] + " onclick='sorter.addBorder(this);sorter.doSorter(4, \"str\", \"all_list\");' style='cursor:pointer;'><#ParentalCtrl_hwaddr#></th>";
			code += "<th width=" + obj_width[5] + " onclick='sorter.addBorder(this);sorter.doSorter(5, \"num\", \"all_list\");' style='cursor:pointer;'><#wan_interface#></th>";
			if(stainfo_support) {
				code += "<th width=" + obj_width[6] + " onclick='sorter.addBorder(this);sorter.doSorter(6, \"num\", \"all_list\");' style='cursor:pointer;' title='The transmission rates of your wireless device'>Tx Rate (Mbps)</th>";/*untranslated*/
				code += "<th width=" + obj_width[7] + " onclick='sorter.addBorder(this);sorter.doSorter(7, \"num\", \"all_list\");' style='cursor:pointer;' title='The receive rates of your wireless device'>Rx Rate (Mbps)</th>";/*untranslated*/
				code += "<th width=" + obj_width[8] + " onclick='sorter.addBorder(this);sorter.doSorter(8, \"str\", \"all_list\");' style='cursor:pointer;'><#Access_Time#></th>";
			}
			code += "</tr>";
			code += "</table>";
			code += "<div id='clientlist_all_list_Block'></div>";
			break;
		case "ByInterface" :
			code += "<table width='100%' border='1' align='center' cellpadding='0' cellspacing='0' class='FormTable_table' style='margin-top:15px;'>";
			code += "<thead><tr height='28px'><td colspan='" + wl_colspan + "'><#tm_wired#>";
			code += "<a id='wired_expander' class='clientlist_expander' onclick='showHideContent(\"clientlist_wired_list_Block\", this);'>[ Hide ]</a>";/*untranslated*/
			code += "</td></tr></thead>";
			code += "<tr id='tr_wired_title' height='40px'>";
			code += "<th width=" + obj_width[0] + "><#Internet#></th>";
			code += "<th width=" + obj_width[1] + ">Icon</th>";/*untranslated*/
			code += "<th width=" + obj_width[2] + " onclick='sorter.addBorder(this);sorter.doSorter(2, \"str\", \"wired_list\");' style='cursor:pointer;'><#ParentalCtrl_username#></th>";
			code += "<th width=" + obj_width[3] + " onclick='sorter.addBorder(this);sorter.doSorter(3, \"num\", \"wired_list\");' style='cursor:pointer;'>Clients IP Address</th>";/*untranslated*/
			code += "<th width=" + obj_width[4] + " onclick='sorter.addBorder(this);sorter.doSorter(4, \"str\", \"wired_list\");' style='cursor:pointer;'><#ParentalCtrl_hwaddr#></th>";
			code += "<th width=" + obj_width[5] + " ><#wan_interface#></th>";
			if(stainfo_support) {
				code += "<th width=" + obj_width[6] + " title='The transmission rates of your wireless device'>Tx Rate (Mbps)</th>";/*untranslated*/
				code += "<th width=" + obj_width[7] + " title='The receive rates of your wireless device'>Rx Rate (Mbps)</th>";/*untranslated*/
				code += "<th width=" + obj_width[8] + "><#Access_Time#></th>";
			}
			code += "</tr>";
			code += "</table>";
			code += "<div id='clientlist_wired_list_Block'></div>";
	
			var wl_map = {"2.4 GHz": "1",  "5 GHz": "2", "5 GHz-1": "2", "5 GHz-2": "3"};
			obj_width = stainfo_support ? obj_width_map[2] : obj_width_map[1];
			for(var i = 0; i < wl_nband_title.length; i += 1) {
				code += "<table width='100%' border='1' align='center' cellpadding='0' cellspacing='0' class='FormTable_table' style='margin-top:15px;'>";
				code += "<thead><tr height='23px'><td colspan='" + wl_colspan + "'>" + wl_nband_title[i];
				code += "<a id='wl" + wl_map[wl_nband_title[i]] + "_expander' class='clientlist_expander' onclick='showHideContent(\"clientlist_wl" + wl_map[wl_nband_title[i]] + "_list_Block\", this);'>[ Hide ]</a>";/*untranslated*/
				code += "</td></tr></thead>";
				code += "<tr id='tr_wl" + wl_map[wl_nband_title[i]] + "_title' height='40px'>";
				code += "<th width=" + obj_width[0] + "><#Internet#></th>";
				code += "<th width=" + obj_width[1] + ">Icon</th>";/*untranslated*/
				code += "<th width=" + obj_width[2] + " onclick='sorter.addBorder(this);sorter.doSorter(2, \"str\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;'><#ParentalCtrl_username#></th>";
				code += "<th width=" + obj_width[3] + " onclick='sorter.addBorder(this);sorter.doSorter(3, \"num\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;'>Clients IP Address</th>";
				code += "<th width=" + obj_width[4] + " onclick='sorter.addBorder(this);sorter.doSorter(4, \"str\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;'><#ParentalCtrl_hwaddr#></th>";
				code += "<th width=" + obj_width[5] + " onclick='sorter.addBorder(this);sorter.doSorter(5, \"num\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;'><#wan_interface#></th>";
				if(stainfo_support) {
					code += "<th width=" + obj_width[6] + " onclick='sorter.addBorder(this);sorter.doSorter(6, \"num\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;' title='The transmission rates of your wireless device'>Tx Rate (Mbps)</th>";/*untranslated*/
					code += "<th width=" + obj_width[7] + " onclick='sorter.addBorder(this);sorter.doSorter(7, \"num\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;' title='The receive rates of your wireless device'>Rx Rate (Mbps)</th>";/*untranslated*/
					code += "<th width=" + obj_width[8] + " onclick='sorter.addBorder(this);sorter.doSorter(8, \"str\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;'><#Access_Time#></th>";
				}
				code += "</tr>";
				code += "</table>";
				code += "<div id='clientlist_wl" + wl_map[wl_nband_title[i]] + "_list_Block'></div>";
			}
			break;
	}

	code += "<div style='text-align:center;margin-top:15px;'><input  type='button' class='button_gen' onclick='exportClientListLog();' value='<#btn_Export#>'></div>";
	code += "</td></tr></tbody>";
	code += "</table>";

	divObj.innerHTML = code;
	document.getElementById("clientlist_viewlist_content").appendChild(divObj);

	//register event to detect mouse click
	document.body.onclick = function() {hide_clientlist_view_block();}
	document.body.onresize = function() {
		if(document.getElementById("clientlist_viewlist_content") !== null) {
			if(document.getElementById("clientlist_viewlist_content").style.display == "block") {
				cal_panel_block_clientList("clientlist_viewlist_content", 0.045);
			}
		}
	}	

	document.getElementById("clientlist_viewlist_content").onclick = function() {show_clientlist_view_block();}

	//copy clientList to each sort array
	genClientList();
	for(var i = 0; i < clientList.length; i += 1) {
		if(clientList[clientList[i]].isOnline) {
			var deviceTypeName = "Loading manufacturer..";
			if(ouiClientListArray[clientList[clientList[i]].mac] != undefined) {
				var tempDeviceName = ouiClientListArray[clientList[clientList[i]].mac];
				deviceTypeName = tempDeviceName.toUpperCase().charAt(0) + tempDeviceName.substring(1); //Oui Vender name
			}
			if((clientList[clientList[i]].dpiDevice != "" && clientList[clientList[i]].dpiDevice != undefined)) { //BWDPI device
				deviceTypeName = clientList[clientList[i]].dpiDevice;
			}
			var clientName = (clientList[clientList[i]].nickName == "") ? clientList[clientList[i]].name : clientList[clientList[i]].nickName;
			var tempArray = [clientList[clientList[i]].internetState, deviceTypeName, clientName, clientList[clientList[i]].ip, 
							clientList[clientList[i]].mac, clientList[clientList[i]].rssi, clientList[clientList[i]].curTx, clientList[clientList[i]].curRx, 
							clientList[clientList[i]].wlConnectTime, clientList[clientList[i]].isWL, clientList[clientList[i]].dpiVender, clientList[clientList[i]].type, 
							clientList[clientList[i]].macRepeat];
			switch (clienlistViewMode) {
				case "All" :
					all_list.push(tempArray);
					break;
				case "ByInterface" :
					switch (clientList[clientList[i]].isWL) {
						case 0:
							wired_list.push(tempArray);
							break;
						case 1:
							wl1_list.push(tempArray);
							break;
						case 2:
							wl2_list.push(tempArray);
							break;
						case 3:
							wl3_list.push(tempArray);
							break;
					}
					break;
			}
		}
	}

	if(clienlistViewMode == "All") {
		if(!sorter.all_display) {
			document.getElementById("clientlist_all_list_Block").style.display = "none";
			document.getElementById("all_expander").innerHTML = "[ Show ]";/*untranslated*/
		}
	}
	else {
		if(!sorter.wired_display) {
			document.getElementById("clientlist_wired_list_Block").style.display = "none";
			document.getElementById("wired_expander").innerHTML = "[ Show ]";/*untranslated*/
		}
		if(!sorter.wl1_display) {
			document.getElementById("clientlist_wl1_list_Block").style.display = "none";
			document.getElementById("wl1_expander").innerHTML = "[ Show ]";/*untranslated*/
		}
		if(!sorter.wl2_display) {
			document.getElementById("clientlist_wl2_list_Block").style.display = "none";
			document.getElementById("wl2_expander").innerHTML = "[ Show ]";/*untranslated*/
		}
		if(!sorter.wl3_display) {
			document.getElementById("clientlist_wl3_list_Block").style.display = "none";
			document.getElementById("wl3_expander").innerHTML = "[ Show ]";/*untranslated*/
		}
	}
}

var clientListViewMacUploadIcon = new Array();
function drawClientListBlock(objID) {
	var sortArray = "";
	switch(objID) {
		case "all_list" :
			sortArray = all_list;
			break;
		case "wired_list" :
			sortArray = wired_list;
			break;
		case "wl1_list" :
			sortArray = wl1_list;
			break;
		case "wl2_list" :
			sortArray = wl2_list;
			break;	
		case "wl3_list" :
			sortArray = wl3_list;
			break;	

	}
	var listViewProfile = function(_profile) {
		if(_profile == null)
			_profile = ["", "", "", "", "", "", "", "", "", "", "", "", ""];
		
		this.internetState = _profile[0];
		this.deviceTypeName = _profile[1];
		this.name = _profile[2];
		this.ip = _profile[3];
		this.mac = _profile[4];
		this.rssi = _profile[5];
		this.curTx = _profile[6];
		this.curRx = _profile[7];
		this.wlConnectTime = _profile[8];
		this.isWL = _profile[9];
		this.vender = _profile[10];
		this.type = _profile[11];
		this.macRepeat = _profile[12];
	}
	var convRSSI = function(val) {
		if(val == "") return "wired";

		val = parseInt(val);
		if(val >= -50) return 4;
		else if(val >= -80)	return Math.ceil((24 + ((val + 80) * 26)/10)/25);
		else if(val >= -90)	return Math.ceil((((val + 90) * 26)/10)/25);
		else return 1;
	};

	if(document.getElementById("clientlist_" + objID + "_Block") != null) {
		if(document.getElementById("tb_" + objID) != null) {
			removeElement(document.getElementById("tb_" + objID));
		}
		var obj_width_map = [["15%", "20%", "25%", "20%", "20%"],["10%", "10%", "30%", "20%", "20%", "10%"],["6%", "6%", "27%", "20%", "15%", "6%", "6%", "6%", "8%"]];
		//var obj_width = (objID == "wired_list") ? obj_width_map[0] : ((stainfo_support) ? obj_width_map[2] : obj_width_map[1]);
		var obj_width = (stainfo_support) ? obj_width_map[2] : obj_width_map[1];
		var wl_colspan = stainfo_support ? 9 : 6;
		var clientListCode = "";
		//user icon
		userIconBase64 = "NoIcon";

		clientListCode += "<table width='100%' cellspacing='0' cellpadding='0' align='center' class='list_table' id='tb_" + objID + "'>";
		if(sortArray.length == 0) {
			clientListCode += "<tr id='tr_" + objID + "'><td style='color:#FFCC00;' colspan='" + wl_colspan + "'><#IPConnection_VSList_Norule#></td></tr>";
		}
		else {
			clientlist_sort = new Array();
			for(var i = 0; i < sortArray.length; i += 1) {
				clientlist_sort.push(new listViewProfile(sortArray[i]));
			}

			for(var j = 0; j < clientlist_sort.length; j += 1) {
				clientListCode += "<tr height='48px'>";

				if(usericon_support) {
					if(clientListViewMacUploadIcon[clientlist_sort[j].mac] == undefined) {
						var clientMac = clientlist_sort[j].mac.replace(/\:/g, "");
						userIconBase64 = getUploadIcon(clientMac);
						clientListViewMacUploadIcon[clientlist_sort[j].mac] = userIconBase64;
					}
					else {
						userIconBase64 = clientListViewMacUploadIcon[clientlist_sort[j].mac];
					}
				}
			
				var internetStateCss = "";
				var internetStateTip = "";
				if(clientlist_sort[j].internetState) {
					internetStateCss = "internetAllow" ;
					internetStateTip = "Allow Internet access";
				}
				else {
					internetStateCss = "internetBlock";
					internetStateTip = "Block Internet access";
				}
				clientListCode += "<td width='" + obj_width[0] + "' align='center'>";
				clientListCode += "<div class=" + internetStateCss + " title=\"" + internetStateTip + "\"></div>";
				clientListCode += "</td>";
				clientListCode += "<td width='" + obj_width[1] + "' align='center'>";
				// display how many clients that hide behind a repeater.
				if(clientlist_sort[j].macRepeat > 1){
					clientListCode += '<div class="clientlist_circle"';
					clientListCode += 'onmouseover="return overlib(\''+clientlist_sort[j].macRepeat+' clients are connecting to <% nvram_get("productid"); %> through this device.\');"';
					clientListCode += 'onmouseout="nd();"><div>';
					clientListCode += clientlist_sort[j].macRepeat;
					clientListCode += '</div></div>';
				}

				if(userIconBase64 != "NoIcon") {
					clientListCode += "<div style='height:45px;width:42px;' title='"+ clientlist_sort[j].deviceTypeName +"'>";
					clientListCode += "<img class='imgUserIcon_viewlist' src=" + userIconBase64 + "";
					clientListCode += ">";
					clientListCode += "</div>";
				}
				else if( (clientlist_sort[j].type != "0" && clientlist_sort[j].type != "6") || clientlist_sort[j].vender == "") {
					var icon_type = "type" + clientlist_sort[j].type;
					if(clientlist_sort[j].type == "0" || clientlist_sort[j].type == "6") {
						icon_type = "type0_viewMode";
					}
					clientListCode += "<div style='height:45px;width:42px;background-size:77px;cursor:default;' class='clientIcon_no_hover " + icon_type + "' title='"+ clientlist_sort[j].deviceTypeName +"'></div>";
				}
				else if(clientlist_sort[j].vender != "" ) {
					var venderIconClassName = getVenderIconClassName(clientlist_sort[j].vender.toLowerCase());
					if(venderIconClassName != "") {
						clientListCode += "<div style='height:45px;width:42px;background-size:77px;cursor:default;' class='venderIcon_no_hover " + venderIconClassName + "' title='"+ clientlist_sort[j].deviceTypeName +"'></div>";
					}
					else {
						var icon_type = "type" + clientlist_sort[j].type;
						if(clientlist_sort[j].type == "0" || clientlist_sort[j].type == "6") {
							icon_type = "type0_viewMode";
						}
						clientListCode += "<div style='height:45px;width:42px;background-size:77px;cursor:default;' class='clientIcon_no_hover " + icon_type + "' title='"+ clientlist_sort[j].deviceTypeName +"'></div>";
					}				
				}
				clientListCode += "</td>";
				clientListCode += "<td style='word-wrap:break-word; word-break:break-all;' width='" + obj_width[2] + "'>";
				clientListCode += "<div id='div_clientName_"+objID+"_"+j+"' class='viewclientlist_clientName_edit' onclick='editClientName(\""+objID+"_"+j+"\");'>"+clientlist_sort[j].name+"</div>";
				clientListCode += "<input id='client_name_"+objID+"_"+j+"' type='text' value='"+clientlist_sort[j].name+"' class='input_25_table' maxlength='32' style='width:95%;margin-left:0px;display:none;' onblur='saveClientName(\""+objID+"_"+j+"\", "+clientlist_sort[j].type+", this);'>";
				clientListCode += "</td>";
				var ipStyle = ('<% nvram_get("sw_mode"); %>' == "1") ? "line-height:16px;text-align:left;padding-left:10px;" : "line-height:16px;text-align:center;";
				clientListCode += "<td width='" + obj_width[3] + "' style='" + ipStyle + "'>";
				clientListCode += (clientList[clientlist_sort[j].mac].isWebServer) ? "<a class='link' href='http://"+clientlist_sort[j].ip+"' target='_blank'>"+clientlist_sort[j].ip+"</a>" : clientlist_sort[j].ip;
				if('<% nvram_get("sw_mode"); %>' == "1") {
					clientListCode += '<span style="float:right;margin-top:-3px;margin-right:5px;" class="ipMethodTag" onmouseover="return overlib(\''
					clientListCode += ipState[clientList[clientlist_sort[j].mac].ipMethod];
					clientListCode += '\')" onmouseout="nd();">'
					clientListCode += clientList[clientlist_sort[j].mac].ipMethod + '</span>';
				}

				clientListCode += "</td>";
				clientListCode += "<td width='" + obj_width[4] + "'>"+clientlist_sort[j].mac+"</td>";
				var rssi_t = 0;
				rssi_t = convRSSI(clientlist_sort[j].rssi);
				clientListCode += "<td width='" + obj_width[5] + "' align='center'><div style='height:28px;width:28px'><div class='radioIcon radio_" + rssi_t + "'></div>";
				if(clientlist_sort[j].isWL != 0) {
					var bandClass = (navigator.userAgent.toUpperCase().match(/CHROME\/([\d.]+)/)) ? "band_txt_chrome" : "band_txt";
					clientListCode += "<div class='band_block'><span class='" + bandClass + "'>" + wl_nband_title[clientlist_sort[j].isWL-1].replace("Hz", "") + "</span></div>";
				}
				clientListCode += "</div></td>";
				if(stainfo_support) {
					var txRate = "";
					var rxRate = "";
					if(clientlist_sort[j].isWL != 0) {
						txRate = (clientlist_sort[j].curTx == "") ? "-" : clientlist_sort[j].curTx.replace("Mbps","");
						rxRate = (clientlist_sort[j].curRx == "") ? "-" : clientlist_sort[j].curRx.replace("Mbps","");
					}
					else {
						txRate = "-";
						rxRate = "-";
					}
					clientListCode += "<td width='" + obj_width[6] + "'>" + txRate + "</td>";
					clientListCode += "<td width='" + obj_width[7] + "'>" + rxRate + "</td>";
					clientListCode += "<td width='" + obj_width[8] + "'>"+((clientlist_sort[j].wlConnectTime == "00:00:00") ? "-" : clientlist_sort[j].wlConnectTime)+"</td>";
				}
				clientListCode += "</tr>";
			}
		}
		clientListCode += "</table>";
		document.getElementById("clientlist_" + objID + "_Block").innerHTML = clientListCode;
	}
}

function showHideContent(objnmae, thisObj) {
	if(!slideFlag) {
		var state = document.getElementById(objnmae).style.display;
		var clickItem = objnmae.split("_")[1];
		if(state == "none") {
			if(clienlistViewMode == "All")
				sorter.all_display = true;
			else {
				switch (clickItem) {
					case "wired" :
						sorter.wired_display = true;
						break;
					case "wl1" :
						sorter.wl1_display = true;
						break;
					case "wl2" :
						sorter.wl2_display = true;
						break;
					case "wl3" :
						sorter.wl3_display = true;
						break;
				}
			}
			slideFlag = true;
			slideDown(objnmae, 200);
			thisObj.innerHTML = "[ Hide ]";
		}
		else {
			if(clienlistViewMode == "All")
				sorter.all_display = false;
			else {
				switch (clickItem) {
					case "wired" :
						sorter.wired_display = false;
						break;
					case "wl1" :
						sorter.wl1_display = false;
						break;
					case "wl2" :
						sorter.wl2_display = false;
						break;
					case "wl3" :
						sorter.wl3_display = false;
						break;
				}
			}
			slideFlag = true;
			slideUp(objnmae, 200);
			thisObj.innerHTML = "[ Show ]";
		}
	}
}

function updateClientListView(){
	$.ajax({
		url: '/update_clients.asp',
		dataType: 'script', 
		error: function(xhr) {
			setTimeout("updateClientListView();", 1000);
		},
		success: function(response){
			if(document.getElementById("clientlist_viewlist_content").style.display != "none") {
				if((isJsonChanged(originData, originDataTmp) || originData.fromNetworkmapd == "") && !edit_client_name_flag && !slideFlag){
					create_clientlist_listview();
					sorterClientList();
					parent.show_client_status(totalClientNum.online);
				}
				setTimeout("updateClientListView();", 3000);	
			}
		}    
	});
}

function cal_panel_block_clientList(obj, multiple) {
	var isMobile = function() {
		var tmo_support = ('<% nvram_get("rc_support"); %>'.search("tmo") == -1) ? false : true;
		if(!tmo_support)
			return false;
		
		if(	navigator.userAgent.match(/iPhone/i)	|| 
			navigator.userAgent.match(/iPod/i)		||
			navigator.userAgent.match(/iPad/i)		||
			(navigator.userAgent.match(/Android/i) && (navigator.userAgent.match(/Mobile/i) || navigator.userAgent.match(/Tablet/i))) ||
			(navigator.userAgent.match(/Opera/i) && (navigator.userAgent.match(/Mobi/i) || navigator.userAgent.match(/Mini/i))) ||	// Opera mobile or Opera Mini
			navigator.userAgent.match(/IEMobile/i)	||	// IE Mobile
			navigator.userAgent.match(/BlackBerry/i)	//BlackBerry
		 ) {
			return true;
		}
		else {
			return false;
		}
	};
	var blockmarginLeft;
	if (window.innerWidth) {
		winWidth = window.innerWidth;
	}
	else if ((document.body) && (document.body.clientWidth)) {
		winWidth = document.body.clientWidth;
	}

	if (document.documentElement  && document.documentElement.clientHeight && document.documentElement.clientWidth) {
		winWidth = document.documentElement.clientWidth;
	}

	if(winWidth > 1050) {
		winPadding = (winWidth - 1050) / 2;
		winWidth = 1105;
		blockmarginLeft = (winWidth * multiple) + winPadding;
	}
	else if(winWidth <= 1050) {
		if(isMobile()) {
			if(document.body.scrollLeft < 50) {
				blockmarginLeft= (winWidth) * multiple + document.body.scrollLeft;
			}
			else if(document.body.scrollLeft >320) {
				blockmarginLeft = 320;
			}
			else {
				blockmarginLeft = document.body.scrollLeft;
			}	
		}
		else {
			blockmarginLeft = (winWidth) * multiple + document.body.scrollLeft;	
		}
	}

	document.getElementById(obj).style.marginLeft = blockmarginLeft + "px";
}

function getFilePath(file) {
	var currentPath = file;
	var pathLength = location.pathname.split("/").length - 1;
	for(var i = pathLength; i > 1; i -= 1) {
	if(i == 1)
		currentPath = ".." + file;
	else
		currentPath = "../" + file;
	}
	return currentPath
};

function editClientName(index) {
	document.getElementById("div_clientName_"+index).style.display = "none";
	document.getElementById("client_name_"+index).style.display = "";
	document.getElementById("client_name_"+index).focus();
	edit_client_name_flag = true;
}
var view_custom_name = decodeURIComponent('<% nvram_char_to_ascii("", "custom_clientlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<");
function saveClientName(index, type, obj) {

	document.getElementById("div_clientName_"+index).style.display = "";
	document.getElementById("client_name_"+index).style.display = "none";
	edit_client_name_flag = false;
	
	var originalCustomListArray = new Array();
	var onEditClient = new Array();
	originalCustomListArray = view_custom_name.split('<');
	
	onEditClient[0] = document.getElementById("client_name_"+index).value.trim();
	onEditClient[1] = obj.parentNode.parentNode.childNodes[4].innerHTML;
	onEditClient[2] = 0;
	onEditClient[3] = type;
	onEditClient[4] = "";
	onEditClient[5] = "";
	document.getElementById("div_clientName_"+index).innerHTML = document.getElementById("client_name_"+index).value.trim();

	for(var i = 0; i < originalCustomListArray.length; i += 1) {
		if(originalCustomListArray[i].split('>')[1] == onEditClient[1]){
			onEditClient[4] = originalCustomListArray[i].split('>')[4]; // set back callback for ROG device
			onEditClient[5] = originalCustomListArray[i].split('>')[5]; // set back keeparp for ROG device
			originalCustomListArray.splice(i, 1); // remove the selected client from original list
		}
	}
	originalCustomListArray.push(onEditClient.join('>'));
	view_custom_name = originalCustomListArray.join('<');
	document.view_clientlist_form.custom_clientlist.value = view_custom_name;
	document.view_clientlist_form.submit();
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
    var data = $.getJSON(target, {format: "json"});

    data.success(function(msg){
    	parent.retObj = msg;
		parent.db("Success!");
    });

    data.error(function(msg){
		parent.db("Error on fetch data!")
    });
}

function oui_query(mac){
	var tab = new Array();
	tab = mac.split(mac.substr(2,1));
	$.ajax({
	    url: 'http://standards.ieee.org/cgi-bin/ouisearch?'+ tab[0] + '-' + tab[1] + '-' + tab[2],
		type: 'GET',
	    success: function(response) {
			if(overlib.isOut) return nd();
			if (typeof clientList[mac] != "undefined")
				var overlibStrTmp  = retOverLibStr(clientList[mac]);
			else
				var overlibStrTmp = "<p><#MAC_Address#>:</p>" + mac.toUpperCase();
			if(response.responseText.search("Sorry!") == -1) {
				var retData = response.responseText.split("pre")[1].split("(base 16)")[1].replace("PROVINCE OF CHINA", "R.O.C").split("&lt;/");
				overlibStrTmp += "<p><span>.....................................</span></p><p style='margin-top:5px'><#Manufacturer#> :</p>";
				overlibStrTmp += retData[0];
			}

			return overlib(overlibStrTmp);
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

