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
ipState["Manual"] =  "Manually Assigned IP";
function convType(str){
	if(str.length == 0)
		return 0;

	var siganature = [[], ["win", "pc"], [], [], ["nas", "storage"], ["cam"], [], 
					  ["ps", "play station", "playstation"], ["xbox"], ["android", "htc"], 
					  ["iphone", "ipad", "ipod", "ios"], ["appletv", "apple tv", "apple-tv"], [], 
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
	wlList_2g: [<% wl_sta_list_2g(); %>],
	wlList_5g: [<% wl_sta_list_5g(); %>],
	wlList_5g_2: [<% wl_sta_list_5g_2(); %>],
	wlListInfo_2g: [<% wl_stainfo_list_2g(); %>],
	wlListInfo_5g: [<% wl_stainfo_list_5g(); %>],
	wlListInfo_5g_2: [<% wl_stainfo_list_5g_2(); %>],
	qosRuleList: decodeURIComponent('<% nvram_char_to_ascii("", "qos_rulelist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	init: true
}

var totalClientNum = {
	online: 0,
	wireless: 0,
	wired: 0,
	wireless_ifnames: [],
}

var setClientAttr = function(){
	this.type = "";
	this.name = "";
	this.ip = "offline";
	this.mac = "";
	this.from = "";
	this.macRepeat = 1;
	this.group = "";
	this.dpiType = "";
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
	this.wlConnectTime = "00:00:00";
}


var clientList = new Array(0);
function genClientList(){

	var wirelessList = cookie.get("wireless_list");
	var wirelessListArray = new Array();
	
	leaseArray = {hostname: [], mac: []};
	for(var i = 0; i < originData.fromDHCPLease.length; i += 1) {
		var dhcpMac = originData.fromDHCPLease[i][0].toUpperCase();
		var dhcpName = decodeURIComponent(originData.fromDHCPLease[i][1]);
		if(dhcpMac != "") {
			leaseArray.mac.push(dhcpMac);
			leaseArray.hostname.push(dhcpName);
		}
	}

	clientList = [];
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
			clientList[thisClientMacAddr].from = "networkmapd";
		}
		else{
			if(clientList[thisClientMacAddr].from == "networkmapd")
				clientList[thisClientMacAddr].macRepeat++;
			else
				clientList[thisClientMacAddr].from = "networkmapd";
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

		if(!thisClientMacAddr){
			continue;
		}

		if(typeof clientList[thisClientMacAddr] == "undefined"){
			clientList.push(thisClientMacAddr);
			clientList[thisClientMacAddr] = new setClientAttr();
			clientList[thisClientMacAddr].from = "customList";
		}

		clientList[thisClientMacAddr].name = thisClient[0];
		clientList[thisClientMacAddr].mac = thisClient[1];
		clientList[thisClientMacAddr].group = thisClient[2];
		clientList[thisClientMacAddr].type = thisClient[3];
		clientList[thisClientMacAddr].callback = thisClient[4];
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


function removeElement(element) {
    element && element.parentNode && element.parentNode.removeChild(element);
}
/*
 * elem 
 * speed, default is 20, optional
 * opacity, default is 100, range is 0~100, optional
 */
function fadeIn(elem, speed, opacity){
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

/*
 * elem 
 * speed, default is 20, optional
 * opacity, default is 0, range is 0~100, optional
 */
function fadeOut(elem, speed, opacity) {
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
	"indexFlag" : 2 , // default sort is by signal
	"all_index" : 2,
	"all_display" : true,
	"wired_index" : 2,
	"wired_display" : true,
	"wl1_index" : 2,
	"wl1_display" : true,
	"wl2_index" : 2,
	"wl2_display" : true,
	"wl3_index" : 2,
	"wl3_display" : true,
	"sortingMethod" : "increase", 
	"sortingMethod_wired" : "increase", 
	"sortingMethod_wl1" : "increase", 
	"sortingMethod_wl2" : "increase", 
	"sortingMethod_wl3" : "increase", 

	"num_increase" : function(a, b) {
		if(sorter.indexFlag == 2) {
			var a_num = 0, b_num = 0;
			a_num = inet_network(a[sorter.indexFlag]);
			b_num = inet_network(b[sorter.indexFlag]);
			return parseInt(a_num) - parseInt(b_num);
		}
		else if(sorter.indexFlag == 4 || sorter.indexFlag == 5 || sorter.indexFlag == 6) {
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
		if(sorter.indexFlag == 2) {
			var a_num = 0, b_num = 0;
			a_num = inet_network(a[sorter.indexFlag]);
			b_num = inet_network(b[sorter.indexFlag]);
			return parseInt(b_num) - parseInt(a_num);
		}
		else if(sorter.indexFlag == 4 || sorter.indexFlag == 5 || sorter.indexFlag == 6) {
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
	document.body.appendChild(divObj);
	fadeIn(document.getElementById("clientlist_viewlist_content"));
	cal_panel_block_clientList("clientlist_viewlist_content", 0.045);

	clientlist_view_hide_flag = false;

	create_clientlist_listview();

	updateClientListView();

	registerIframeClick("statusframe", hide_clientlist_view_block);

}

function exportClientListLog() {
	var data = [["Clients Name", "Clients IP Address", "Clients MAC Address", "Interface", "Tx Rate", "Rx Rate", "Access time"]];
	var tempArray = new Array();
	var setArray = function(array) {
		for(var i = 0; i < array.length; i += 1) {
			tempArray = [];
			tempArray[0] = array[i][1];
			tempArray[1] = array[i][2];
			tempArray[2] = array[i][3];
			tempArray[3] = (array[i][8] == 0) ? "Wired" : wl_nband_title[array[i][8] - 1];
			tempArray[4] = (array[i][5] == "") ? "-" : array[i][5];
			tempArray[5] = (array[i][6] == "") ? "-" : array[i][6];
			tempArray[6] = (array[i][8] == 0) ? "-" : array[i][7];
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

	var obj_width_map = [["15%", "45%", "20%", "20%"],["10%", "45%", "19%", "19%", "7%"],["7%", "30%", "18%", "17%", "6%", "7%", "7%", "8%"]];
	var obj_width = stainfo_support ? obj_width_map[2] : obj_width_map[1];
	var wl_colspan = stainfo_support ? 8 : 5;

	var code = "";

	var drawSwitchMode = function(mode) {
		var drawSwitchModeHtml = "";
		drawSwitchModeHtml += "<div style='margin-top:15px;margin-left:10px;'>";
		if(mode == "All") {
			drawSwitchModeHtml += "<div class='block_filter_pressed clientlist_All'>";
			drawSwitchModeHtml += "<div class='block_filter_name' style='color:#93A9B1;'>All</div>";
			drawSwitchModeHtml += "</div>";
			drawSwitchModeHtml += "<div class='block_filter clientlist_ByInterface' style='cursor:pointer'>";
			drawSwitchModeHtml += "<div class='block_filter_name' onclick='changeClientListViewMode();'>By interface</div>";
			drawSwitchModeHtml += "</div>";
		}
		else {							
			drawSwitchModeHtml += "<div class='block_filter clientlist_All' style='cursor:pointer'>";
			drawSwitchModeHtml += "<div class='block_filter_name' onclick='changeClientListViewMode();'>All</div>";
			drawSwitchModeHtml += "</div>";
			drawSwitchModeHtml += "<div class='block_filter_pressed clientlist_ByInterface'>";
			drawSwitchModeHtml += "<div class='block_filter_name' style='color:#93A9B1;'>By interface</div>";
			drawSwitchModeHtml += "</div>";
		}
		drawSwitchModeHtml += "</div>";
		return drawSwitchModeHtml;
	};

	code += drawSwitchMode(clienlistViewMode);
	code += "<div style='text-align:right;width:30px;position:relative;margin-top:-45px;margin-left:96%;'><img src='/images/button-close.gif' style='width:30px;cursor:pointer' onclick='closeClientListView();'></div>";
	code += "<table border='0' align='center' cellpadding='0' cellspacing='0' style='width:100%;padding:0px 10px 0px 10px;'><tbody><tr><td>";

	switch (clienlistViewMode) {
		case "All" :
			code += "<table width='100%' border='1' align='center' cellpadding='0' cellspacing='0' class='FormTable_table' style='margin-top:30px;'>";
			code += "<thead><tr height='28px'><td id='td_all_list_title' colspan='" + wl_colspan + "'>All list";
			code += "<a id='all_expander'class='clientlist_expander' onclick='showHideContent(\"clientlist_all_list_Block\", this);'>[ Hide ]</a>";
			code += "</td></tr></thead>";
			code += "<tr id='tr_all_title' height='40px'>";
			code += "<th width=" + obj_width[0] + ">Icon</th>";
			code += "<th width=" + obj_width[1] + " onclick='sorter.addBorder(this);sorter.doSorter(1, \"str\", \"all_list\");' style='cursor:pointer;'><#ParentalCtrl_username#></th>";
			code += "<th width=" + obj_width[2] + " onclick='sorter.addBorder(this);sorter.doSorter(2, \"num\", \"all_list\");' id='ipTh' style='cursor:pointer;'>Clients IP Address</th>";
			code += "<th width=" + obj_width[3] + " onclick='sorter.addBorder(this);sorter.doSorter(3, \"str\", \"all_list\");' style='cursor:pointer;'><#ParentalCtrl_hwaddr#></th>";
			code += "<th width=" + obj_width[4] + " onclick='sorter.addBorder(this);sorter.doSorter(4, \"num\", \"all_list\");' style='cursor:pointer;'>Inter<br>face</th>";
			if(stainfo_support) {
				code += "<th width=" + obj_width[5] + " onclick='sorter.addBorder(this);sorter.doSorter(5, \"num\", \"all_list\");' style='cursor:pointer;'>Tx Rate<br>(Mbps)</th>";
				code += "<th width=" + obj_width[6] + " onclick='sorter.addBorder(this);sorter.doSorter(6, \"num\", \"all_list\");' style='cursor:pointer;'>Rx Rate<br>(Mbps)</th>";
				code += "<th width=" + obj_width[7] + " onclick='sorter.addBorder(this);sorter.doSorter(7, \"str\", \"all_list\");' style='cursor:pointer;'><#Access_Time#></th>";
			}
			code += "</tr>";
			code += "</table>";
			code += "<div id='clientlist_all_list_Block'></div>";
			break;
		case "ByInterface" :
			code += "<table width='100%' border='1' align='center' cellpadding='0' cellspacing='0' class='FormTable_table' style='margin-top:30px;'>";
			code += "<thead><tr height='28px'><td colspan='" + wl_colspan + "'><#tm_wired#>";
			code += "<a id='wired_expander' class='clientlist_expander' onclick='showHideContent(\"clientlist_wired_list_Block\", this);'>[ Hide ]</a>";
			code += "</td></tr></thead>";
			code += "<tr id='tr_wired_title' height='40px'>";
			code += "<th width=" + obj_width[0] + ">Icon</th>";
			code += "<th width=" + obj_width[1] + " onclick='sorter.addBorder(this);sorter.doSorter(1, \"str\", \"wired_list\");' style='cursor:pointer;'><#ParentalCtrl_username#></th>";
			code += "<th width=" + obj_width[2] + " onclick='sorter.addBorder(this);sorter.doSorter(2, \"num\", \"wired_list\");' id='ipTh' style='cursor:pointer;'>Clients IP Address</th>";
			code += "<th width=" + obj_width[3] + " onclick='sorter.addBorder(this);sorter.doSorter(3, \"str\", \"wired_list\");' style='cursor:pointer;'><#ParentalCtrl_hwaddr#></th>";
			code += "<th width=" + obj_width[4] + " >Inter<br>face</th>";
			if(stainfo_support) {
				code += "<th width=" + obj_width[5] + ">Tx Rate<br>(Mbps)</th>";
				code += "<th width=" + obj_width[6] + ">Rx Rate<br>(Mbps)</th>";
				code += "<th width=" + obj_width[7] + "><#Access_Time#></th>";
			}
			code += "</tr>";
			code += "</table>";
			code += "<div id='clientlist_wired_list_Block'></div>";
	
			var wl_map = {"2.4 GHz": "1",  "5 GHz": "2", "5 GHz-1": "2", "5 GHz-2": "3"};
			obj_width = stainfo_support ? obj_width_map[2] : obj_width_map[1];
			for(var i = 0; i < wl_nband_title.length; i += 1) {
				code += "<table width='100%' border='1' align='center' cellpadding='0' cellspacing='0' class='FormTable_table' style='margin-top:15px;'>";
				code += "<thead><tr height='23px'><td colspan='" + wl_colspan + "'>" + wl_nband_title[i];
				code += "<a id='wl" + wl_map[wl_nband_title[i]] + "_expander' class='clientlist_expander' onclick='showHideContent(\"clientlist_wl" + wl_map[wl_nband_title[i]] + "_list_Block\", this);'>[ Hide ]</a>";
				code += "</td></tr></thead>";
				code += "<tr id='tr_wl" + wl_map[wl_nband_title[i]] + "_title' height='40px'>";
				code += "<th width=" + obj_width[0] + ">Icon</th>";
				code += "<th width=" + obj_width[1] + " onclick='sorter.addBorder(this);sorter.doSorter(1, \"str\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;'><#ParentalCtrl_username#></th>";
				code += "<th width=" + obj_width[2] + " onclick='sorter.addBorder(this);sorter.doSorter(2, \"num\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;'>Clients IP Address</th>";
				code += "<th width=" + obj_width[3] + " onclick='sorter.addBorder(this);sorter.doSorter(3, \"str\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;'><#ParentalCtrl_hwaddr#></th>";
				code += "<th width=" + obj_width[4] + " onclick='sorter.addBorder(this);sorter.doSorter(4, \"num\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;'>Inter<br>face</th>";
				if(stainfo_support) {
					code += "<th width=" + obj_width[5] + " onclick='sorter.addBorder(this);sorter.doSorter(5, \"num\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;'>Tx Rate<br>(Mbps)</th>";
					code += "<th width=" + obj_width[6] + " onclick='sorter.addBorder(this);sorter.doSorter(6, \"num\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;'>Rx Rate<br>(Mbps)</th>";
					code += "<th width=" + obj_width[7] + " onclick='sorter.addBorder(this);sorter.doSorter(7, \"str\", \"wl"+wl_map[wl_nband_title[i]]+"_list\");' style='cursor:pointer;'><#Access_Time#></th>";
				}
				code += "</tr>";
				code += "</table>";
				code += "<div id='clientlist_wl" + wl_map[wl_nband_title[i]] + "_list_Block'></div>";
			}
			break;
	}

	code += "<div style='text-align:center;margin-top:15px;'><input  type='button' class='button_gen' onclick='exportClientListLog();' value='Export'></div>";
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
			var tempArray = [clientList[clientList[i]].type, clientList[clientList[i]].name, clientList[clientList[i]].ip,
							clientList[clientList[i]].mac, clientList[clientList[i]].rssi, clientList[clientList[i]].curTx,
							clientList[clientList[i]].curRx, clientList[clientList[i]].wlConnectTime, clientList[clientList[i]].isWL,
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

	//initial sort ip
	var indexMapType = ["", "str", "num", "str", "num", "num", "str", "str"];
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

	if(clienlistViewMode == "All") {
		if(!sorter.all_display) {
			document.getElementById("clientlist_all_list_Block").style.display = "none";
			document.getElementById("all_expander").innerHTML = "[ Show ]";
		}
	}
	else {
		if(!sorter.wired_display) {
			document.getElementById("clientlist_wired_list_Block").style.display = "none";
			document.getElementById("wired_expander").innerHTML = "[ Show ]";
		}
		if(!sorter.wl1_display) {
			document.getElementById("clientlist_wl1_list_Block").style.display = "none";
			document.getElementById("wl1_expander").innerHTML = "[ Show ]";
		}
		if(!sorter.wl2_display) {
			document.getElementById("clientlist_wl2_list_Block").style.display = "none";
			document.getElementById("wl2_expander").innerHTML = "[ Show ]";
		}
		if(!sorter.wl3_display) {
			document.getElementById("clientlist_wl3_list_Block").style.display = "none";
			document.getElementById("wl3_expander").innerHTML = "[ Show ]";
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
			_profile = ["", "", "", "", "", "", "", "", "", "", ""];
		this.type = _profile[0];
		this.name = _profile[1];
		this.ip = _profile[2];
		this.mac = _profile[3];
		this.rssi = _profile[4];
		this.curTx = _profile[5];
		this.curRx = _profile[6];
		this.wlConnectTime = _profile[7];
		this.isWL = _profile[8];
		this.macRepeat = _profile[9];
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
		var obj_width_map = [["15%", "45%", "20%", "20%"],["10%", "45%", "19%", "19%", "7%"],["7%", "30%", "18%", "17%", "6%", "7%", "7%", "8%"]];
		//var obj_width = (objID == "wired_list") ? obj_width_map[0] : ((stainfo_support) ? obj_width_map[2] : obj_width_map[1]);
		var obj_width = (stainfo_support) ? obj_width_map[2] : obj_width_map[1];
		var wl_colspan = stainfo_support ? 8 : 5;
		var clientListCode = "";
		//user icon
		var userIconBase64 = "NoIcon";

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
				clientListCode += "<tr height='47px'>";

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
			
				clientListCode += "<td width='" + obj_width[0] + "' align='center'>";
				// display how many clients that hide behind a repeater.
				if(clientlist_sort[j].macRepeat > 1){
					clientListCode += '<div class="clientlist_circle"';
					clientListCode += 'onmouseover="return overlib(\''+clientlist_sort[j].macRepeat+' clients are connecting to <% nvram_get("productid"); %> through this device.\');"';
					clientListCode += 'onmouseout="nd();"><div>';
					clientListCode += clientlist_sort[j].macRepeat;
					clientListCode += '</div></div>';
				}

				if(userIconBase64 != "NoIcon") {
					clientListCode += "<div style='height:45px;width:42px;'>";
					clientListCode += "<img class='imgUserIcon_viewlist' src=" + userIconBase64 + "";
					clientListCode += ">";
					clientListCode += "</div>";
				}
				else {
					var icon_type = "type" + clientlist_sort[j].type;
					if(clientlist_sort[j].type == "0" || clientlist_sort[j].type == "6") {
						icon_type = "type0_viewMode";
					}
					clientListCode += "<div style='height:45px;width:42px;background-size:72px;cursor:default;' class='clientIcon_no_hover " + icon_type + "'></div>";
				}
				clientListCode += "</td>";
				clientListCode += "<td width='" + obj_width[1] + "'>"+clientlist_sort[j].name+"</td>";
				var ipStyle = ('<% nvram_get("sw_mode"); %>' == "1") ? "line-height:16px;text-align:left;padding-left:10px;" : "line-height:16px;text-align:center;";
				clientListCode += "<td width='" + obj_width[2] + "' style='" + ipStyle + "'>";
				clientListCode += (clientList[clientlist_sort[j].mac].isWebServer) ? "<a class='link' href='http://"+clientlist_sort[j].ip+"' target='_blank'>"+clientlist_sort[j].ip+"</a>" : clientlist_sort[j].ip;
				if('<% nvram_get("sw_mode"); %>' == "1") {
					clientListCode += '<span style="float:right;margin-top:-3px;margin-right:5px;" class="ipMethodTag" onmouseover="return overlib(\''
					clientListCode += ipState[clientList[clientlist_sort[j].mac].ipMethod];
					clientListCode += '\')" onmouseout="nd();">'
					clientListCode += clientList[clientlist_sort[j].mac].ipMethod + '</span>';
				}

				clientListCode += "</td>";
				clientListCode += "<td width='" + obj_width[3] + "'>"+clientlist_sort[j].mac+"</td>";
				var rssi_t = 0;
				rssi_t = convRSSI(clientlist_sort[j].rssi);
				clientListCode += "<td width='" + obj_width[4] + "' align='center'><div style='height:28px;width:25px'><div class='radioIcon radio_" + rssi_t + "'></div>";
				if(clienlistViewMode == "All" && clientlist_sort[j].isWL != 0) {
					var bandClass = (navigator.userAgent.toUpperCase().match(/CHROME\/([\d.]+)/)) ? "band_chrome" : "band";
					clientListCode += "<div class='" + bandClass + "'>" + wl_nband_title[clientlist_sort[j].isWL-1].replace("Hz", "") + "</div>";
				}
				clientListCode += "</div></td>";
				if(stainfo_support) {
					clientListCode += "<td width='" + obj_width[5] + "'>"+((clientlist_sort[j].curTx == "") ? "-" : clientlist_sort[j].curTx.replace("Mbps",""))+"</td>";
					clientListCode += "<td width='" + obj_width[6] + "'>"+((clientlist_sort[j].curRx == "") ? "-" : clientlist_sort[j].curRx.replace("Mbps",""))+"</td>";
					clientListCode += "<td width='" + obj_width[7] + "'>"+((clientlist_sort[j].wlConnectTime == "00:00:00") ? "-" : clientlist_sort[j].wlConnectTime)+"</td>";
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
				if((isJsonChanged(originData, originDataTmp) || originData.fromNetworkmapd == "") && !slideFlag){
					create_clientlist_listview();
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
	if (typeof clientList[mac] != "undefined"){
		if(clientList[mac].callback != ""){
			ajaxCallJsonp("http://" + clientList[mac].ip + ":" + clientList[mac].callback + "/callback.asp?output=netdev&jsoncallback=?");
		}
	}

	var tab = new Array();
	tab = mac.split(mac.substr(2,1));
	$.ajax({
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
			if (current_url == "clients.asp")
				return overlib(overlibStrTmp, HAUTO, VAUTO);
			else
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

