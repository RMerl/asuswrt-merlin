/*
	js functions and code common to all pages
*/

/* define some global variables for this request */
global.page = new Object();

/* fill in some defaults */
global.page.title = "Samba Web Administration Tool";

libinclude("base.js");

/* to cope with browsers that don't support cookies we append the sessionid
   to the URI */
global.SESSIONURI = "";
if (request['COOKIE_SUPPORT'] != "True") {
	global.SESSIONURI="?SwatSessionId=" + request['SESSION_ID'];
}

/*
  possibly adjust a local URI to have the session id appended
  used for browsers that don't support cookies
*/
function session_uri(uri) {
	return uri + global.SESSIONURI;
}

/*
  like printf, but to the web page
*/
function writef()
{
	write(vsprintf(arguments));
}

/*
  like writef with a <br>
*/
function writefln()
{
	write(vsprintf(arguments));
	write("<br/>\n");
}


/* if the browser was too dumb to set the HOST header, then
   set it now */
if (headers['HOST'] == undefined) {
	headers['HOST'] = server['SERVER_HOST'] + ":" + server['SERVER_PORT'];
}

/*
  show the page header. page types include "plain" and "column" 
*/
function page_header(pagetype, title, menu) {
	global.page.pagetype = pagetype;
	global.page.title = title;
	global.page.menu = menu;
	include("/scripting/header_" + pagetype + ".esp");
}

/*
  show the page footer, getting the page type from page.pagetype
  set in page_header()
*/
function page_footer() {
	include("/scripting/footer_" + global.page.pagetype + ".esp");
}


/*
  display a table element
*/
function table_element(i, o) {
	write("<tr><td>" + i + "</td><td>");
	if (typeof(o[i]) == "object") {
		var j, first;
		first = true;
		for (j in o[i]) {
			if (first == false) {
				write("<br />");
			}
			write(o[i][j]);
			first = false;
		}
	} else {
		write(o[i]);
	}
	write("</td></tr>\n");
}

/*
  display a ejs object as a table. The header is optional
*/
function simple_table(v) {
	if (v.length == 0) {
		return;
	}
	write("<table class=\"data\">\n");
	var r;
	for (r in v) {
		table_element(r, v);
	}
	write("</table>\n");
}

/*
  display an array of objects, with the header for each element from the given 
  attribute
*/
function multi_table(array, header) {
	var i, n;
	write("<table class=\"data\">\n");
	for (i=0;i<array.length;i++) {
		var r, v = array[i];
		write('<tr><th colspan="2">' + v[header] + "</th></tr>\n");
		for (r in v) {
			if (r != header) {
			    table_element(r, v);
			}
		}
	}
	write("</table>\n");
}

