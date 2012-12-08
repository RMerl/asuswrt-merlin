/* Javascript functions for Asuswrt-Merlin */
function getRadioValue(obj) {
	for (var i=0; i<obj.length; i++) {
		if (obj[i].checked)
			return obj[i].value;
	}
	return 0;
}

function setRadioValue(obj,val) {
	for (var i=0; i<obj.length; i++) {
		if (obj[i].value==val)
			obj[i].checked = true;
	}
}

hostnamecache = [];

function populateCache() {
	var s;

	// Retrieve NETBIOS client names through networkmap list
	// This might NOT be accurate if the device IP is dynamic.
	// TODO: Should we force a network scan first to update that list?
	//       See what happens if we access this right after a reboot
	var client_list_array = '<% get_client_detail_info(); %>';

	if (client_list_array) {
		s = client_list_array.split('<');
		for (var i = 0; i < s.length; ++i) {
			var t = s[i].split('>');
			if (t.length == 7) {
				if (t[1] != '')
					hostnamecache[t[2]] = t[1].split(' ').splice(0,1);
			}
		}
	}

	// Retrieve manually entered descriptions in static lease list
	// We want to override netbios names if we havee a better name

	dhcpstaticlist = '<% nvram_get("dhcp_staticlist"); %>';

	if (dhcpstaticlist) {
		s = dhcpstaticlist.split('&#60');
		for (var i = 0; i < s.length; ++i) {
			var t = s[i].split('&#62');
			if ((t.length == 3) || (t.length == 4)) {
				if (t[2] != '')
					hostnamecache[t[1]] = t[2].split(' ').splice(0,1);
			}
		}
	}

	hostnamecache[fixIP(ntoa(aton(nvram.lan_ipaddr) & aton(nvram.lan_netmask)))] = 'LAN';
}

function cmpFloat(a, b)
{
        a = parseFloat(a);
        b = parseFloat(b);
        return ((isNaN(a)) ? -Number.MAX_VALUE : a) - ((isNaN(b)) ? -Number.MAX_VALUE : b);
}

