/* Exported from device-map/clients.asp */

lan_ipaddr = "<% nvram_get("lan_ipaddr"); %>";
lan_netmask = "<% nvram_get("lan_netmask"); %>";

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
		}else{
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
	populateCache();
}

var retHostName = function(_mac){
	if(parent.sw_mode != 1) return false;
	if(leasemac == "") return "";

	for(var idx=0; idx<leasemac.length; idx++){
		if(!(leasehostname[idx].childNodes[0].nodeValue.split("value=")[1]) || !(leasemac[idx].childNodes[0].nodeValue.split("value=")[1]))
			continue;

		if( _mac.toLowerCase() == leasemac[idx].childNodes[0].nodeValue.split("value=")[1].toLowerCase()){
			if(leasehostname[idx].childNodes[0].nodeValue.split("value=")[1] != "*")
				return leasehostname[idx].childNodes[0].nodeValue.split("value=")[1];
			else
				return "";
		}
	}
	return "";
}
/* end */


hostnamecache = { "ready":0 };

function populateCache() {
	var s;

	var client_list_array = '<% get_client_detail_info(); %>';

	if (client_list_array) {
		s = client_list_array.split('<');
		for (var i = 0; i < s.length; ++i) {
			var t = s[i].split('>');
			if (t.length == 7) hostnamecache[t[2]] = t[1].trim();
		}
	}

	hostnamecache[fixIP(ntoa(aton(lan_ipaddr) & aton(lan_netmask)))] = 'LAN';
	hostnamecache["ready"] = 1;
	return;
}

function oui_query(mac) {
	var tab = new Array()
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
			if(overlib.isOut)
				return nd();
			var retData = response.responseText.split("pre")[1].split("(base 16)")[1].split("&lt;/");
			overlib_str_tmp += "<p><span>.....................................</span></p>";
			return overlib(overlib_str_tmp + "<p style='margin-top:5px'>Manufacturer:</p>" + retData[0]);
		}    
  });
}
