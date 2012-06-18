/*
	js functions for forms
*/


/*
  display a simple form from a ejs Form object
  caller should fill in
    f.name          = form name
    f.action        = action to be taken on submit (optional, defaults to current page)
    f.class         = css class (optional, defaults to 'form')
    f.submit        = an array of submit labels
    f.add(name, label, [type], [value])  =
	Add another element
    f.element[i].label = element label
    f.element[i].name  = element name (defaults to label)
    f.element[i].type  = element type (defaults to text)
    f.element[i].value = current value (optional, defaults to "")
 */
function form_display() {
	var f = this;
	var i, size = 20;
	write('<form name="' + f.name +
	      '" method="post" action="' + f.action + 
	      '" class="' + f.class + '">\n');
	if (f.element.length > 0) {
		write("<table>\n");
	}
	for (i in f.element) {
		var e = f.element[i];
		if (e.name == undefined) {
			e.name = e.label;
		}
		if (e.value == undefined) {
			e.value = "";
		}
		if (strlen(e.value) > size) {
			size = strlen(e.value) + 4;
		}
	}
	for (i in f.element) {
		var e = f.element[i];
		write("<tr>");
		write("<td>" + e.label + "</td>");
		if (e.type == "select") {
			write('<td><select name="' + e.name + '">\n');
			for (s in e.list) {
				if (e.value == e.list[s]) {
					write('<option selected=selected>' + e.list[s] + '</option>\n');
				} else {
					write('<option>' + e.list[s] + '</option>\n');
				}
			}
			write('</select></td>\n');
		} else {
			var sizestr = "";
			if (e.type == "text" || e.type == "password") {
				sizestr = sprintf('size="%d"', size);
			}
			writef('<td><input name="%s" type="%s" value="%s" %s /></td>\n',
			       e.name, e.type, e.value, sizestr);
		}
		write("</tr>");
	}
	if (f.element.length > 0) {
		write("</table>\n");
	}
	for (i in f.submit) {
		write('<input name="submit" type="submit" value="' + f.submit[i] + '" />\n');
	}
	write("</form>\n");
}

function __addMethod(name, label)
{
	var f = this;
	var i = f.element.length;
	f.element[i] = new Object();
	f.element[i].name = name;
	f.element[i].label = label;
	f.element[i].type = "text";
	f.element[i].value = "";
	if (arguments.length > 2) {
		f.element[i].type = arguments[2];
	}
	if (arguments.length > 3) {
		f.element[i].value = arguments[3];
	}
}

/*
  create a Form object with the defaults filled in, ready for display()
 */
function FormObj(name, num_elements, num_submits)
{
	var f = new Object();
	f.name = name;
	f.element = new Array(num_elements);
	f.submit =  new Array(num_submits);
	f.action = session_uri(request.REQUEST_URI);
	f.class = "defaultform";
	f.add = __addMethod;
	for (i in f.element) {
		f.element[i] = new Object();
		f.element[i].type = "text";
		f.element[i].value = "";
	}
	f.display = form_display;

	return f;
}

