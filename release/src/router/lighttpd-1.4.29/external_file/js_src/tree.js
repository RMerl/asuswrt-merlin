var storageList = function(cookieName) {
	var storageName = cookieName;
	var storage = new myStorage();
	
	//Load the items or a new array if null.
	var items = storage.get(cookieName) ? storage.get(cookieName).split(/,/) : new Array();
	
	return {
    "add": function(val) {
    	
    		if(items.contains(val)){
    			return;
    		}
    		
        //Add to the items.
        items.push(val);
        
        //Save the items to a storage.
        storage.set(storageName, items.join(','));
    },
    "clear": function() {
        items = null;
        storage.set(storageName, '');
    },
    "items": function() {
    		//items.sort();
        return items;
    },
    "size": function() {
    	return items.length;
  	},
  	"isUIDExists": function(uid) {
  		for (var i=0; i< items.length; i++) {
				var nodeValues = items[i].split("|");
				if (nodeValues[2] == uid) return true;
			}
			
			return false;
  	},
  	"removeByPID": function(pid){
  		for (var i=0; i< items.length; i++) {
				var nodeValues = items[i].split("|");
				if (nodeValues[1] == pid){
					items.splice(i,1);
					i--;
				}
			}
  	}
  }
};

// Arrays for nodes and icons
var nodes			= new Array();;
var openNodes	= new Array();
var icons			= new Array(6);
var s = '';

// Loads all icons that are used in the tree
function preloadIcons() {
	icons[0] = new Image();
	icons[0].src = "/smb/css/img/plus.gif";
	icons[1] = new Image();
	icons[1].src = "/smb/css/img/plusbottom.gif";
	icons[2] = new Image();
	icons[2].src = "/smb/css/img/minus.gif";
	icons[3] = new Image();
	icons[3].src = "/smb/css/img/minusbottom.gif";
	icons[4] = new Image();
	icons[4].src = "/smb/css/img/folder.png";
	icons[5] = new Image();
	icons[5].src = "/smb/css/img/folderopen.png";
	icons[6] = new Image();
	icons[6].src = "/smb/css/img/computer.png";
	icons[7] = new Image();
	icons[7].src = "/smb/css/img/usb.png";
}
// Create the tree
function createTree(root_name, arrName, startNode, openNode, selectNodeCallbackFunc) {
	nodes = arrName;
	var tree_div = document.getElementById("tree");
	s = '';
	
	if (nodes.length > 0) {
		preloadIcons();
		if (startNode == null) startNode = 0;
		
		if (openNode != 0 || openNode != null) setOpenNodes(openNode);
		
		if (startNode !=0) {
			var nodeValues = nodes[getArrayId(startNode)].split("|");
			s += "<a href=\"" + nodeValues[0] + "\" onmouseover=\"window.status='" + nodeValues[0] + "';return true;\" onmouseout=\"window.status=' ';return true;\"><img src=\"/smb/css/img/folderopen.png\" align=\"absbottom\" alt=\"\" />" + mydecodeURI(nodeValues[0]) + "</a><br />";
		} 
		else{ 
			s += "<a link=\"\" nodeid=\"0\" parentid=\"0\" onmouseover=\"window.status='/';return true;\" onmouseout=\"window.status=' ';return true;\">";
			s += "<img src=\"/smb/css/img/base.png\" href=\"/\" align=\"absbottom\" alt=\"\" />" + root_name + "</a><br/>";			
		}
		
		var recursedNodes = new Array();
		addNode(startNode, recursedNodes);
				
		tree_div.innerHTML = s;
	}
	
	$('div#tree a').click(function(){
		if(selectNodeCallbackFunc){
			
			var full_path = "";
			var parentid = $(this).attr('parentid');
			var nodename, nodeip;
			
			if(parentid==undefined)
				return;
			
			var a = getNodeName(parentid);
			
			while(a!=null){
				parentid = a[1];
				nodename = a[0];
				nodeip = a[2];
				
				a = getNodeName(parentid);
				
				full_path = "/" + nodename + full_path;				
			}
			
			full_path = full_path+'/'+$(this).attr('link');			
							
			selectNodeCallbackFunc(full_path, $(this).attr('nodeid'), $(this).attr('parentid'), $(this).attr('online'), $(this).attr('mac'), $(this).attr('type'));
		}
	});
	
}

function getNodeName(id){
	for (var i=0; i<nodes.length; i++) {
		var nodeValues = nodes[i].split("|");
		
		if (nodeValues[2]==id) {
			//- [nadeName, parentid, nodeServerIP]
			return [ nodeValues[0], nodeValues[1], nodeValues[6] ];
		}
	}
	
	return null;
}

// Returns the position of a node in the array
function getArrayId(node) {
	for (i=0; i<nodes.length; i++) {
		var nodeValues = nodes[i].split("|");
		if (nodeValues[2]==node) return i;
	}
}
// Puts in array nodes that will be open
function setOpenNodes(openNode) {
	for (i=0; i<nodes.length; i++) {
		var nodeValues = nodes[i].split("|");
		if (nodeValues[2]==openNode) {
			
			if(!openNodes.contains(openNode))
				openNodes.push(openNode);
				
			setOpenNodes(nodeValues[1]);
		}
	} 
}
// Checks if a node is open
function isNodeOpen(node) {
	for (i=0; i<openNodes.length; i++)
		if (openNodes[i]==node) return true;
	return false;
}
// Checks if a node has any children
function hasChildNode(parentNode) {
	for (i=0; i< nodes.length; i++) {
		var nodeValues = nodes[i].split("|");
		if (nodeValues[1] == parentNode) return true;
	}
	return false;
}
// Checks if a node is the last sibling
function lastSibling (node, parentNode) {
	var lastChild = 0;
	for (i=0; i< nodes.length; i++) {
		var nodeValues = nodes[i].split("|");
		if (nodeValues[1] == parentNode)
			lastChild = nodeValues[2];
	}
	if (lastChild==node) return true;
	return false;
}

function getNode(id){
	for (i=0; i< nodes.length; i++) {
		var nodeValues = nodes[i].split("|");
		if (nodeValues[2] == id)
			return nodes[i];
	}
	
	return "";
}

// Adds a new node to the tree
function addNode(parentNode, recursedNodes) {
	var tree_div = $('div.tree');
	
	for (var i = 0; i < nodes.length; i++) {

		var nodeValues = nodes[i].split("|");
		if (nodeValues[1] == parentNode) {
			
			var ls	= lastSibling(nodeValues[2], nodeValues[1]);
			var hcn	= hasChildNode(nodeValues[2]);
			var ino = isNodeOpen(nodeValues[2]);

			// Write out line & empty icons
			for (g=0; g<recursedNodes.length; g++) {
				if (recursedNodes[g] == 1) 
					s += "<img src=\"/smb/css/img/line.gif\" align=\"absbottom\" alt=\"\" />";
				else  
					s += "<img src=\"/smb/css/img/empty.gif\" align=\"absbottom\" alt=\"\" />";
			}

			// put in array line & empty icons
			if (ls) recursedNodes.push(0);
			else recursedNodes.push(1);

			// Write out join icons
			if (hcn) {
				if (ls) {
					s += "<a href=\"javascript: oc('" + nodeValues[0] + "', '" + nodeValues[1] + "', '" + nodeValues[2] + "', '" + nodeValues[5] +"', 1);\"><img id=\"join" + nodeValues[2] + "\" src=\"/smb/css/img/";
					if (ino) 
						s += "minus";
					else 
						s += "plus";
					s += "bottom.gif\" align=\"absbottom\" alt=\"Open/Close node\" /></a>";
				} 
				else {
					s += "<a href=\"javascript: oc('" + nodeValues[0] + "', '" + nodeValues[1] + "', '" + nodeValues[2] + "', '" + nodeValues[5] + "', 0);\"><img id=\"join" + nodeValues[2] + "\" src=\"/smb/css/img/";
					if (ino) 
						s += "minus";
					else 
						s += "plus";
					s += ".gif\" align=\"absbottom\" alt=\"Open/Close node\" /></a>";
				}
			} 
			else {
				if(ls) 
					s += "<img src=\"/smb/css/img/joinbottom.gif\" align=\"absbottom\" alt=\"\" />";
				else 
					s += "<img src=\"/smb/css/img/join.gif\" align=\"absbottom\" alt=\"\" />";
			}

			// Start link
			s += "<a link=\"" + nodeValues[0] + "\" ";
			s += "nodeid=\"" + nodeValues[2] + "\" ";
			s += "parentid=\"" + nodeValues[1] + "\" ";
			s += "online=\"" + nodeValues[3] + "\" ";
			s += "ip=\"" + nodeValues[6] + "\" ";
			s += "mac=\"" + nodeValues[4] + "\" ";
			s += "type=\"" + nodeValues[5] + "\" ";
			s += "title=\"" + mydecodeURI(nodeValues[0]) + " - " + nodeValues[6] + "\" ";
			//s += "title=\"" + mydecodeURI(nodeValues[0]) + "\" ";
			s += "onmouseover=\"window.status='" + nodeValues[0] + "';return true;\" onmouseout=\"window.status=' ';return true;\">";
			
			// Write out folder & page icons
			if (hcn) {
				if(nodeValues[1]==0){
					
					if(nodeValues[5]=="usbdisk"){
						//- USB Node
						s += "<img id=\"icon" + nodeValues[2] + "\" src=\"/smb/css/img/usb";
						s += ".png\" align=\"absbottom\" alt=\"Computer\" />";
					}
					else{
						//- Computer Node
						s += "<img id=\"icon" + nodeValues[2] + "\" src=\"/smb/css/img/computer";
						if (nodeValues[3]==0) s += "off";
						s += ".png\" align=\"absbottom\" alt=\"Computer\" />";
					}
				}
				else{
					s += "<img id=\"icon" + nodeValues[2] + "\" src=\"/smb/css/img/folder";
					if (ino) s += "open";
					s += ".png\" align=\"absbottom\" alt=\"Folder\" />";
				}
			}
			else{
				if(nodeValues[1]==0){
					if(nodeValues[5]=="usbdisk"){
						//- USB Node
						s += "<img id=\"icon" + nodeValues[2] + "\" class='page' src=\"/smb/css/img/usb.png\" align=\"absbottom\" alt=\"Computer\" />";
					}
					else{
						//- Computer Node
						s += "<img id=\"icon" + nodeValues[2] + "\" class='page' src=\"/smb/css/img/computer";
						if (nodeValues[3]==0) s += "off";
						s += ".png\" align=\"absbottom\" alt=\"Computer\" />";
					}
				}
				else
					s += "<img id=\"icon" + nodeValues[2] + "\" class='page' src=\"/smb/css/img/folder.png\" align=\"absbottom\" alt=\"Page\" />";
			}
			
			// Write out node name
			s += mydecodeURI(nodeValues[0]);
			
			if(nodeValues[1]==0&&nodeValues[3]=="0")
				s += "(" + m.getString("title_offline") + ")";
				
			// End link
			s += "</a><br />";
			
			// If node has children write out divs and go deeper
			if (hcn) {
				s += "<div id=\"div" + nodeValues[2] + "\"";
				if (!ino) s += " style=\"display: none;\"";
				s += ">";
				addNode(nodeValues[2], recursedNodes);
				s += "</div>";
			}

			// remove last line or empty icon 
			recursedNodes.pop();
		}
	}
}


function openNode(node){
	if(!openNodes.contains(node)){		
		openNodes.push(node);
		//var openuid = g_storage.get('openuid');
		
		g_storage.set('openuid', node);
	}	
}

function closeNode(node){	
	if(openNodes.contains(node)){
		var node_info = getNodeName(node);
		
		for (var i = 0; i < nodes.length; i++) {
			
			var nodeValues = nodes[i].split("|");
						
			if (nodeValues[1]==node) {
				closeNode(nodeValues[2]);
			}
		}
		
		//alert('close: ' + node_info[0]);	
		openNodes.removeItem(node);
		
	}	
}

// Opens or closes a node
function oc(nodename, pnode, node, type, bottom) {
	var theDiv = document.getElementById("div" + node);
	var theJoin	= document.getElementById("join" + node);
	var theIcon = document.getElementById("icon" + node);
		
	if (theDiv.style.display == 'none') {
		//- Open Node
		openNode(node);
		
		if (bottom==1) theJoin.src = icons[3].src;
		else theJoin.src = icons[2].src;
			
		if(pnode=="0"){
			if(type=="usbdisk")
				theIcon.src = icons[7].src;
			else
				theIcon.src = icons[6].src;
		}
		else
			theIcon.src = icons[5].src;
		theDiv.style.display = '';
	} else {
		//- Close Node		
		closeNode(node);
		/*
		for (var i = 0; i < nodes.length; i++) {
			
			var nodeValues = nodes[i].split("|");
			
			if (nodeValues[2]==node) {
				//- Set parent node to open node
				g_storage.set('openuid', nodeValues[1]);
				alert("open " + getNodeName(nodeValues[1])[0] + ", " + nodeValues[1]);
			}
		}
		*/
		if (bottom==1) theJoin.src = icons[1].src;
		else theJoin.src = icons[0].src;
			
		if(pnode=="0"){
			if(type=="usbdisk")
				theIcon.src = icons[7].src;
			else
				theIcon.src = icons[6].src;
		}
		else
			theIcon.src = icons[4].src;
		theDiv.style.display = 'none';
	}
}

// Push and pop not implemented in IE
if(!Array.prototype.push) {
	function array_push() {
		for(var i=0;i<arguments.length;i++)
			this[this.length]=arguments[i];
		return this.length;
	}
	Array.prototype.push = array_push;
}
if(!Array.prototype.pop) {
	function array_pop(){
		lastElement = this[this.length-1];
		this.length = Math.max(this.length-1,0);
		return lastElement;
	}
	Array.prototype.pop = array_pop;
}

if(!Array.prototype.contains) {
	function array_contains(obj){
		for (var i = 0; i < this.length; i++) {
			if (this[i] === obj) {
            return true;
        }
    }
    return false;
	}
	Array.prototype.contains = array_contains;
}

if(!Array.prototype.removeItem) {
	function array_removeItem(obj){
		for (var i = 0; i < this.length; i++) {
			if (this[i] === obj) {
				 		this.splice(i,1);
				 		//alert('remove: ' + obj + ', ' + this.length);
            return true;
        }
    }
    return false;
	}
	Array.prototype.removeItem = array_removeItem;
}