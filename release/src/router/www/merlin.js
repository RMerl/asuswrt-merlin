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

function cmpFloat(a, b) {
        a = parseFloat(a);
        b = parseFloat(b);
        return ((isNaN(a)) ? -Number.MAX_VALUE : a) - ((isNaN(b)) ? -Number.MAX_VALUE : b);
}

function cmpDualFields(a, b) {
	if (cmpHist(a, b) == 0)
		return aton(a[1])-aton(b[1]);
	else
		return cmpHist(a,b);
}

function getYMD(n) {
	// [y,m,d]
	return [(((n >> 16) & 0xFF) + 1900), ((n >>> 8) & 0xFF), (n & 0xFF)];
}
