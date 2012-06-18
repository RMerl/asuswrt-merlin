/*
	client side js functions for registry editing

	Copyright Andrew Tridgell 2005
	released under the GNU GPL Version 3 or later
*/


/*
  callback from the key enumeration call
*/
function __folder_keys(fParent, list) 
{
	var i;
	if (fParent.working == 1) {
		fParent.working = 0;
		fParent.removeAll();
	}
	for (i=0;i<list.length;i++) {
		var fChild;
		fChild = new QxTreeFolder(list[i]);
		fParent.add(fChild);
		fChild.binding = fParent.binding;
		if (fParent.reg_path == '\\') {
			fChild.reg_path = list[i];
		} else {
			fChild.reg_path = fParent.reg_path + '\\' + list[i];
		}
		fChild.working = 1;
		fChild.add(new QxTreeFolder('Working ...'));
		fChild.addEventListener("click", function() { 
			var el = this; __folder_click(el); 
		});
	}
	fParent.setOpen(1);
}

/*
  callback from the key enumeration call
*/
function __folder_values(fParent, list) 
{
	var i;
	if (list.length == 0) {
		return;
	}
	if (fParent.working == 1) {
		fParent.working = 0;
		fParent.removeAll();
	}
	for (i=0;i<list.length;i++) {
		var fChild;
		fChild = new QxTreeFile(list[i].name);
		fChild.parent = fParent;
		fChild.details = list[i];
		fParent.add(fChild);
	}
	fParent.setOpen(1);
}

/*
  called when someone clicks on a folder
*/
function __folder_click(node) 
{
	if (!node.populated) {
		node.populated = true;
		server_call_url("/scripting/server/regedit.esp", 'enum_keys', 
				function(list) { __folder_keys(node, list); }, 
				node.binding, node.reg_path);
		server_call_url("/scripting/server/regedit.esp", 'enum_values', 
				function(list) { __folder_values(node, list); }, 
				node.binding, node.reg_path);
	}
}

/* return a registry tree for the given server */
function __registry_tree(binding) 
{
	var tree = new QxTree("registry: " + binding);
	tree.binding = binding;
	tree.reg_path = "\\";
	tree.populated = false;
	with(tree) {
		setBackgroundColor(255);
		setBorder(QxBorder.presets.inset);
		setOverflow("scroll");
		setStyleProperty("padding", "2px");
		setWidth("50%");
		setHeight("90%");
		setTop("10%");
	}
	tree.addEventListener("click", function() { 
		var el = this; __folder_click(el); 
	});
	return tree;
}

/*
  the table of values
*/
function __values_table()
{
	var headings = new Array("Name", "Type", "Size", "Value");
	var table = document.createElement('table');
	table.border = "1";
	var body = document.createElement('tbody');
	table.appendChild(body);
	var th = document.createElement('th');
	for (var i=0;i<headings.length;i++) {
		var td = document.createElement('td');
		td.appendChild(document.createTextNode(headings[i]));
		th.appendChild(td);
	}
	body.appendChild(th);
	return table;
}

/*
  create a registry editing widget and return it as a object
*/
function regedit_widget(binding) 
{
	var fieldSet = new QxFieldSet();

	fieldSet.binding = binding;

	with(fieldSet) {
		setWidth("100%");
		setHeight("100%");
	};

	var gl = new QxGridLayout("auto,auto,auto,auto,auto", "50%,50%");
	gl.setEdge(0);
	gl.setCellPaddingTop(3);
	gl.setCellPaddingBottom(3);

	var t = __registry_tree(fieldSet.binding);

	function change_binding(e) {
		fieldSet.binding = e.getNewValue();
		srv_printf("changed binding to %s\\n", fieldSet.binding);
		gl.remove(t);
		t = __registry_tree(fieldSet.binding);
		gl.add(t, { row : 2, col : 1 });
	}

	var b = new QxTextField(fieldSet.binding);
	b.addEventListener("changeText", change_binding);

	var values = new __values_table();

	gl.add(b,      { row : 1, col : 1 });
	gl.add(t,      { row : 2, col : 1 });
//	gl.add(values, { row : 2, col : 2 });
	
	fieldSet.add(gl);

	return fieldSet;
};
