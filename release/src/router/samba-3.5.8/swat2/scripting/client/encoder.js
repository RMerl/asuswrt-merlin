/*
	client side js functions for encoding/decoding objects into linear strings

	Copyright Andrew Tridgell 2005
	released under the GNU GPL Version 3 or later
*/
/*
	usage:

	  enc = encodeObject(obj);
	  obj = decodeObject(enc);

       The encoded format of the object is a string that is safe to
       use in URLs

       Note that only data elements are encoded, not functions
*/

function count_members(o) {
	var i, count = 0;
	for (i in o) { 
		count++;  
	}
	return count;
}

function encodeObject(o) {
	var i, r = count_members(o) + ":";
	for (i in o) {
		var t = typeof(o[i]);
		if (t == 'object') {
			r = r + "" + i + ":" + t + ":" + encodeObject(o[i]);
		} else if (t == 'string') {
			var s = encodeURIComponent(o[i]).replace(/%/g,'#');
			r = r + "" + i + ":" + t + ":" + s + ":";
		} else if (t == 'boolean' || t == 'number') {
			r = r + "" + i + ":" + t + ":" + o[i] + ":";
		} else if (t == 'undefined' || t == 'null') {
			r = r + "" + i + ":" + t + ":";
		} else if (t != 'function') {
			alert("Unable to encode type " + t);
		}
	}
	return r;
}

function decodeObjectArray(a) {
	var o = new Object();
	var i, count = a[a.i]; a.i++;
	for (i=0;i<count;i++) {
		var name  = a[a.i]; a.i++;
		var type  = a[a.i]; a.i++;
		var value;
		if (type == 'object') {
			o[name] = decodeObjectArray(a);
		} else if (type == "string") {
			value = decodeURIComponent(a[a.i].replace(/#/g,'%')); a.i++;
			o[name] = value;
		} else if (type == "boolean") {
			value = a[a.i]; a.i++;
			if (value == 'true') {
				o[name] = true;
			} else {
				o[name] = false;
			}
		} else if (type == "undefined") {
			o[name] = undefined;
		} else if (type == "null") {
			o[name] = null;
		} else if (type == "number") {
			value = a[a.i]; a.i++;
			o[name] = value * 1;
		} else {
			alert("Unable to delinearise type " + type);
		}
	}
	return o;
}

function decodeObject(str) {
	var a = str.split(':');
	a.i = 0;
	return decodeObjectArray(a);
}
