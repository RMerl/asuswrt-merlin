Object.extend(Element, {
  removeChildren: function(element) {
  while(element.hasChildNodes()) {
    element.removeChild(element.firstChild);
  }
  },
  textContent: function(node) {
    if ((!node) || !node.hasChildNodes()) {
      // Empty text node
      return '';
    }
    if (node.textContent) {
      // W3C ?
      return node.textContent;
    } else if (node.text) {
      // IE
      return node.text;
    } else if (node.firstChild) {
      // Safari
      return node.firstChild.nodeValue;
    }
    // We shouldn't end up here;
    return '';
  }
});
/* Detta script finns att hamta pa http://www.jojoxx.net och 
   far anvandas fritt sa lange som dessa rader star kvar. */ 
function DataDumper(obj,n,prefix){
	var str=""; prefix=(prefix)?prefix:""; n=(n)?n+1:1; var ind=""; for(var i=0;i<n;i++){ ind+="  "; }
	if(typeof(obj)=="string"){
		str+=ind+prefix+"String:\""+obj+"\"\n";
	} else if(typeof(obj)=="number"){
		str+=ind+prefix+"Number:"+obj+"\n";
	} else if(typeof(obj)=="function"){
		str+=ind+prefix+"Function:"+obj+"\n";
	} else if(typeof(obj) == 'boolean') {
       str+=ind+prefix+"Boolean:" + obj + "\n";
	} else {
		var type="Array";
		for(var i in obj){ type=(type=="Array"&&i==parseInt(i))?"Array":"Object"; }
		str+=ind+prefix+type+"[\n";
		if(type=="Array"){
			for(var i in obj){ str+=DataDumper(obj[i],n,i+"=>"); }
		} else {
			for(var i in obj){ str+=DataDumper(obj[i],n,i+"=>"); }
		}
		str+=ind+"]\n";
	}
	return str;
}
var Cookie = {
  getVar: function(name) {
    var cookie = document.cookie;
    if (cookie.length > 0) {
      cookie += ';';
    }
    re = new RegExp(name + '\=(.*?);' );
    if (cookie.match(re)) {
      return RegExp.$1;
    } else {
      return '';
    }
  },
  setVar: function(name,value,days) {
    days = days || 30;
    var expire = new Date(new Date().getTime() + days*86400);
    document.cookie = name + '=' + value +';expires=' + expire.toUTCString();
  },
  removeVar: function(name) {
    var date = new Date(12);
    document.cookie = name + '=;expires=' + date.toUTCString();
  }
};
var Util = {
  startSpinner: function () {
    $('spinner').src = 'spinner.gif'; 
  },
  stopSpinner: function () {
    $('spinner').src = 'spinner_stopped.gif'; 
  }   
}