/*
   Beginnnigs of a script manager for SWAT.

   Copyright (C) Deryck Hodge 2005
   released under the GNU GPL Version 3 or later
*/

var head = document.getElementsByTagName('head')[0];
var scripts = document.getElementsByTagName('script');

function __has_js_script(file)
{
	var i;
	for (i=0; i<scripts.length; i++) {
		if (scripts[i].src.indexOf(file) > -1) {
			return true;
		} else {
			return false;
		}
	}
}

function __get_js_script(file)
{
	var i;
	for (i=0; i<scripts.length; i++) {
		if (scripts[i].src.indexOf(file) > -1) {
			return scripts[i];
		}
	}
}

function __add_js_script(path)
{
	// Create a unique ID for this script
	var srcID = new Date().getTime();

	var script = document.createElement('script');
	script.type = 'text/javascript';
	script.id = srcID;

	head.appendChild(script);

	// IE works only with the path set after appending to the document
	document.getElementById(srcID).src = path;
}

function __remove_js_script(path)
{
	var script = __get_js_script(path);
	script.parentNode.removeChild(script);
}

document.js = new Object();
document.js.scripts = scripts;
document.js.hasScript = __has_js_script;
document.js.getScript = __get_js_script;
document.js.add = __add_js_script;
document.js.remove = __remove_js_script;

