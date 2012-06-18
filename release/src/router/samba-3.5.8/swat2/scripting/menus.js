/*
	menu object for SWAT
*/

/*
  display a menu object. Currently only the "simple", "vertical" menu style
  is supported
*/
function menu_display() {
	var i, m = this;
	assert(m.style == "simple" && m.orientation == "vertical");
	write('<div class="' + m.class + '">\n');
	write("<i>" + m.name + "</i><br /><ul>\n");
	for (i = 0; i < m.element.length; i++) {
		var e = m.element[i];
		write("<li><a href=\"" + e.link + "\">" + e.label + "</a></li>\n");
	}
	write("</ul></div>\n");
}


/*
  create a menu object with the defaults filled in, ready for display_menu()
 */
function MenuObj(name, num_elements)
{
	var i, o = new Object();
	o.name = name;
	o.class = "menu";
	o.style = "simple";
	o.orientation = "vertical"
	o.element = new Array(num_elements);
	for (i in o.element) {
		o.element[i] = new Object();
	}
	o.display = menu_display;
	return o;
}

/*
  return a menu object created using a title, followed by
  a set of label/link pairs
*/
function simple_menu() {
	var i, m = MenuObj(arguments[0], (arguments.length-1)/2);
	for (i=0;i<m.element.length;i++) {
		var ndx = i*2;
		m.element[i].label = arguments[ndx+1];
		m.element[i].link = arguments[ndx+2];
	}
	return m;
}

