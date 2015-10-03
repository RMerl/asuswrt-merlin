var child_macaddr = '<% nvram_get("lan_hwaddr"); %>';
var sw_mode_submit = '<% nvram_get("sw_mode"); %>';
if(sw_mode_submit == 3 && '<% nvram_get("wlc_psta"); %>' == 2)
	sw_mode_submit = 2;
if(parent.document.QKform.sw_mode)
	sw_mode_submit = parent.document.QKform.sw_mode.value;

if(parent.parent_macaddr){
	if(child_macaddr != parent.parent_macaddr){
		setTimeout('genErrorHint();', 500); 
	}
}

function genErrorHint(){	
	if(document.getElementsByTagName("body")[0]){
		var htmlCode = "";
		htmlCode += '<div class="QISmain" id="QISmain">';
		htmlCode += '<div id="descTltle"><table width="100%"><tr><td align="left">';
		htmlCode += '<div class="description_down"><#qis_finish_title1#></div></td><td align="right"></td></tr></table></div>';
		htmlCode += '<div style="margin:5px;" id="splitLine">';
		htmlCode += '<img style="width: 720px; *width: 710px; height: 2px;" src="/images/New_ui/export/line_export.png"></div>';
		if(sw_mode_submit == "4"){
				htmlCode += "<br><br><br><p style='font-size:20px;line-height: 25px;color:#FC0;' class='QISGeneralFont'>* <#OP_MB_desc7#> <#OP_MB_desc6#>";
				htmlCode += "<br><br>* <#OP_MB_desc8#>";
				htmlCode += "<br><br>* <#OP_MB_desc9#>";
		}else{
				htmlCode += "<br><br><br><p style='font-size:20px;line-height: 25px;color:#FC0;' class='QISGeneralFont'>* <#OP_RE_desc1#><br><#OP_RE_desc2#>";
				htmlCode += "<br><br>* <#DrSurf_sweet_advise1#>";
		}

		if(sw_mode_submit == "2")
				htmlCode += "<br><br>* <#OP_MB_desc9#></p>";
		else
				htmlCode += "</p>";
		document.getElementsByTagName("body")[0].innerHTML = htmlCode;
	}
	else{
		alert("<#OP_RE_desc1#> <#OP_RE_desc2#> <#DrSurf_sweet_advise1#>");
	}
} 

if(parent.ISMOBILE){
}
else{
	var cssNode = document.createElement('link');
	cssNode.type = 'text/css';
	cssNode.rel = 'stylesheet';
	cssNode.href = "/qis/qis_style.css";
	document.getElementsByTagName("head")[0].appendChild(cssNode);
}

function changeiuiBackground(loading){
	if(!parent.ISMOBILE) return false;

	if(loading){
		document.getElementById("loadingIcon_iui").style.background = 'url(/images/InternetScan.gif) no-repeat right center';
		document.getElementById("loadingIcon_iui").style.marginRight = '5px';
	}
	else{
	}
}

function changeiuiBackground2(obj,loading){
	if(!parent.ISMOBILE) return false;

	if(loading){
		document.getElementById(obj).style.background = 'url(/images/InternetScan.gif) no-repeat right center';
		document.getElementById(obj).style.marginRight = '5px';
	}
	else{
	}
}

function hideObjs(){
	if(document.getElementById("splitLine"))
		document.getElementById("splitLine").style.display = "none";
	
	if(document.getElementById("applyBtn"))
		document.getElementById("applyBtn").style.display = "none";

	if(document.getElementById("descTltle"))
		document.getElementById("descTltle").style.display = "none";

	if(document.getElementById("mainDiv"))
		document.getElementById("mainDiv").style.marginTop = "-20px";
}

function myfindParent(node, localName)
{
	while (node && (node.nodeType != 1 || node.localName.toLowerCase() != localName))
		node = node.parentNode;
	return node;
}

function myhasClass(self, name){
		var re = new RegExp("(^|\\s)"+name+"($|\\s)");
		return re.exec(self.getAttribute("class")) != null;
}

function showHidden_slide(divIdtoBeHidden, idArray){
	var sec = 20;

	for(var i in idArray)
		document.getElementById(idArray[i]).style.display = "none";	

	document.getElementById(divIdtoBeHidden).style.display = "";
	setTimeout("document.getElementById(\""+divIdtoBeHidden+"\").style.left=\"80%\";", sec);
	setTimeout("document.getElementById(\""+divIdtoBeHidden+"\").style.left=\"60%\";", sec*2);
	setTimeout("document.getElementById(\""+divIdtoBeHidden+"\").style.left=\"40%\";", sec*3);
	setTimeout("document.getElementById(\""+divIdtoBeHidden+"\").style.left=\"20%\";", sec*4);
	setTimeout("document.getElementById(\""+divIdtoBeHidden+"\").style.left=\"0%\";", sec*5);	
	parent.hideHidden_slide(divIdtoBeHidden, idArray);
	window.scrollTo(0, 1);
}

function mySel(obj, isMulti){
	var _className = obj.className;
	if(obj.getAttribute("myselected") == "false"){
		if(!isMulti){
			for(var i=0; i < document.getElementsByClassName("iuiA_sel").length; i++){
				document.getElementsByClassName("iuiA_sel")[i].setAttribute("myselected", "false");
				document.getElementsByClassName("iuiA_sel")[i].className = "iuiA_unsel";
			}

			for(var i=0; i < document.getElementsByClassName("iuiA_sel_check").length; i++){
				document.getElementsByClassName("iuiA_sel_check")[i].setAttribute("myselected", "false");
				document.getElementsByClassName("iuiA_sel_check")[i].className = "iuiA_unsel_check";
			}
		}

		obj.className = _className.replace("unsel", "sel");
		obj.setAttribute("myselected", "true");
	}
	else{
		obj.className = _className.replace("sel", "unsel");
		obj.setAttribute("myselected", "false");
	}
}
