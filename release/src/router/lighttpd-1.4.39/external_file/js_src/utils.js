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

String.prototype.width = function(font) {
  var f = font || '12px arial',
      o = $('<div>' + this + '</div>')
            .css({'position': 'absolute', 'float': 'left', 'white-space': 'nowrap', 'visibility': 'hidden', 'font': f})
            .appendTo($('body')),
      w = o.width();

  o.remove();

  return w;
}

function getFileExt(filename){
	var ext = /^.+\.([^.]+)$/.exec(filename);
  	return ext == null ? "" : ext[1].toLowerCase();
}

function isPrivateIP(ip){
	var location_host = (ip==""||ip==undefined) ? String(window.location.host) : ip;
	
	if( location_host.indexOf("192.168") == 0 ||
	    location_host.indexOf("127") == 0 ||
	    location_host.indexOf("10") == 0 ||
	    location_host.indexOf("www.asusnetwork.net") == 0 ||
	    location_host.indexOf("router.asus.com") == 0 ){
		return 1;
	}
	
	return 0;
}

function getOS(){
	var os, ua = navigator.userAgent;
	if (ua.match(/Win(dows )?NT 6\.0/)) {
		os = "Windows Vista";				// Windows Vista ???
	}
	else if (ua.match(/Win(dows )?NT 6\.1/)) {
		os = "Windows 7";				// Windows Vista ???
	}
	else if (ua.match(/Win(dows )?NT 5\.2/)) {
		os = "Windows Server 2003";			// Windows Server 2003 ???
	}
	else if (ua.match(/Win(dows )?(NT 5\.1|XP)/)) {
		os = "Windows XP";				// Windows XP ???
	}
	else if (ua.match(/Win(dows)? (9x 4\.90|ME)/)) {
		os = "Windows ME";				// Windows ME ???
	}
	else if (ua.match(/Win(dows )?(NT 5\.0|2000)/)) {
		os = "Windows 2000";				// Windows 2000 ???
	}
	else if (ua.match(/Win(dows )?98/)) {
		os = "Windows 98";				// Windows 98 ???
	}
	else if (ua.match(/Win(dows )?NT( 4\.0)?/)) {
		os = "Windows NT";				// Windows NT ???
	}
	else if (ua.match(/Win(dows )?95/)) {
		os = "Windows 95";				// Windows 95 ???
	}
	else if (ua.match(/Mac|PPC/)) {
		os = "Mac OS";					// Macintosh ???
	}
	else if (ua.match(/Linux/)) {
		os = "Linux";					// Linux ???
	}
	else if (ua.match(/(Free|Net|Open)BSD/)) {
		os = RegExp.$1 + "BSD";				// BSD ????
	}
	else if (ua.match(/SunOS/)) {
		os = "Solaris";					// Solaris ???
	}
	else {
		os = "N/A";					// ???? OS ???
	}	
	
	return os;
}

function isWinOS(){
	var osVer = navigator.appVersion.toLowerCase();
	if( osVer.indexOf("win") != -1 )
		return 1;
	
	return 0;
}

function isMacOS(){
	var osVer = navigator.appVersion.toLowerCase();
	if( osVer.indexOf("mac") != -1 && osVer.indexOf("iphone") == -1 && osVer.indexOf("ipad") == -1 && osVer.indexOf("ipod") == -1 )
		return 1;
	
	return 0;
}

function isBrowser(testBrowser){
	var browserVer = navigator.userAgent.toLowerCase();
	if( browserVer.indexOf(testBrowser) != -1 )
		return 1;
	return 0;
}

function getUrlVars(){
	var vars = [], hash;
  var hashes = window.location.href.slice(window.location.href.indexOf('?') + 1).split('&');
  for(var i = 0; i < hashes.length; i++){
  	hash = hashes[i].split('=');
    vars.push(hash[0]);
    vars[hash[0]] = hash[1];
  }
  return vars;
}

function parseXml(xml) {
	if(isIE()){
		var xmlDoc = new ActiveXObject("Microsoft.XMLDOM"); 
    	xmlDoc.loadXML(xml);
    	xml = xmlDoc;
  	}   
  	return xml;
}

function addPathSlash(val){	
	
	if(val.lastIndexOf("/")==val.length-1)
		return val;
		
	val += "/"
	
	return val;
}

function size_format(size) {
	var units = ['B', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];
  var i = 0;
  while(size >= 1024) {
  	size /= 1024;
    ++i;
  }
  return size.toFixed(1) + ' ' + units[i];
}

function mydecodeURI(iurl){
	
	//iurl = String(iurl).replace("%7f", "~"); 
	
	try{
		var myurl = decodeURIComponent(iurl);
		/*
		myurl = String(myurl).replace("%22", "\""); 
		myurl = String(myurl).replace("%23", "#");
		myurl = String(myurl).replace("%24", "$");
		myurl = String(myurl).replace("%25", "%");
		myurl = String(myurl).replace("%26", "&");
		myurl = String(myurl).replace("%2b", "+");
		myurl = String(myurl).replace("%40", "@");
		*/
	}
	catch(err){
		//Handle errors here
	  	//alert('catch error: '+ err);
		return iurl;
	}
	
	return myurl;
}

function myencodeURI(iurl){
	try{
		var myurl = iurl;
		/*
		myurl = String(myurl).replace("%", "%25");
		
		myurl = String(myurl).replace("\"", "%22"); 
		myurl = String(myurl).replace("#", "%23");
		myurl = String(myurl).replace("$", "%24");
		myurl = String(myurl).replace("&", "%26");
		myurl = String(myurl).replace("+", "%2b");
		myurl = String(myurl).replace("@", "%40");
		
		myurl = encodeURI(myurl);
		
		myurl = String(myurl).replace("\"", "%22"); 
		myurl = String(myurl).replace("#", "%23");
		myurl = String(myurl).replace("$", "%24");
		myurl = String(myurl).replace("&", "%26");
		myurl = String(myurl).replace("+", "%2b");
		myurl = String(myurl).replace("@", "%40");
		*/
		myurl = encodeURIComponent(myurl);
	}
	catch(err){
		//Handle errors here
	  	//alert('catch error: '+ err);
		return iurl;
	}
	
	return myurl;
}

function isIE(){
	var is_ie = false;
	
	if(navigator.userAgent.indexOf("MSIE")!=-1){
		is_ie = true;
	}
	else if (!document.all) {
		
    	if (navigator.appName == 'Netscape')
	   	{
			var ua = navigator.userAgent;
	        var re  = new RegExp("Trident/.*rv:([0-9]{1,}[\.0-9]{0,})");
	        if (re.exec(ua) != null){	        	
	        	rv = parseFloat( RegExp.$1 );
	        	if(rv>=11)
	        		is_ie = true;
	        }
		}
	}
	
	return is_ie;
}

function getInternetExplorerVersion(){
	var rv = -1; // Return value assumes failure.
	if (navigator.appName == 'Microsoft Internet Explorer')
   	{
    	var ua = navigator.userAgent;
      	var re  = new RegExp("MSIE ([0-9]{1,}[\.0-9]{0,})");
      	if (re.exec(ua) != null)
        	rv = parseFloat( RegExp.$1 );
   	}
   	else if (navigator.appName == 'Netscape')
   	{
		var ua = navigator.userAgent;
        var re  = new RegExp("Trident/.*rv:([0-9]{1,}[\.0-9]{0,})");
        if (re.exec(ua) != null)
        	rv = parseFloat( RegExp.$1 );
	}
	return rv;
}

function getPageSize() {
	var body_width = $(window).width();
	var body_height = $(window).height();
	return [ body_width, body_height ];
}

function getLockToken(content){
	var parser;
	var xmlDoc;
			
	if (window.DOMParser){					
		parser=new DOMParser();
		xmlDoc=parser.parseFromString(content,"text/xml");
	}
	else { // Internet Explorer
		xmlDoc=new ActiveXObject("Microsoft.XMLDOM");
    xmlDoc.async="false";
		xmlDoc.loadXML(content);
					
		if(!xmlDoc.documentElement){
			alert("Fail to load xml!");
			return;
		}
	}
					
	var j, k, l, n;						
	var x = xmlDoc.documentElement.childNodes;
	for (var i=0;i<x.length;i++){
				
		var y = x[i].childNodes;
		for (var j=0;j<y.length;j++){
			if(y[j].nodeType==1&&y[j].nodeName=="D:activelock"){
				var z = y[j].childNodes;
											
				for(var k=0;k<z.length;k++){
					if(z[k].nodeName=="D:locktoken"){
								
						var a = z[k].childNodes;
													
						for(var l=0;l<a.length;l++)
						{
							if(a[l].childNodes.length<=0)
								continue;
									
							if(a[l].nodeName=="D:href"){
								var locktoken = String(a[l].childNodes[0].nodeValue);
								return locktoken;
							}
						}
					}
				}
			}
		}
	}
}

function DrawImage(ImgD,FitWidth,FitHeight,FitWidthOnly){
	
	if( isBrowser("msie") && getInternetExplorerVersion()<=8 )
		return;
	
	var image=new Image();
	image.src=ImgD.src;	
    if(image.width>0 && image.height>0){
		if(FitWidthOnly){
			ImgD.width=FitWidth;
			ImgD.height=(image.height*FitWidth)/image.width;
			return
		}
		
        if(image.width/image.height>= FitWidth/FitHeight){
        	if(image.width>FitWidth){
            	ImgD.width=FitWidth;
                ImgD.height=(image.height*FitWidth)/image.width;
            }else{
            	ImgD.width=image.width;
                ImgD.height=image.height;
            }
         } else{
            if(image.height>FitHeight){
            	ImgD.height=FitHeight;
                ImgD.width=(image.width*FitHeight)/image.height;
            }else{
            	ImgD.width=image.width;
                ImgD.height=image.height;
            }
        }
     }
}

/*
function getXmlHttpRequest() {
    		
	var sAgent = navigator.userAgent.toLowerCase();
  var isIE = (sAgent.indexOf("msie")!=-1); //IE
  var getInternetExplorerVersion = function(){
		var rv = -1; // Return value assumes failure.
		if (navigator.appName == 'Microsoft Internet Explorer')
		{
			var ua = navigator.userAgent;
			var re  = new RegExp("MSIE ([0-9]{1,}[\.0-9]{0,})");
			if (re.exec(ua) != null)
				rv = parseFloat( RegExp.$1 );
		}
		return rv;
	};
	        
  try{
  	if(isIE&&getInternetExplorerVersion()<=8&&window.ActiveXObject)
    	return new window.ActiveXObject("Microsoft.XMLHTTP");
    } 
    catch(e) { 
    };
				
		try{
			return new XMLHttpRequest();
	  } 
	  catch(e) {
	  	// not a Mozilla or Konqueror based browser
	  };
        
    alert('Your browser does not support XMLHttpRequest, required for ' +
                'WebDAV access.');
    throw('Browser not supported');
}

function wrapHandler(handler, request, context) {
	var self = this;
  function HandlerWrapper() {
  	this.execute = function() {            	
    	if (request.readyState == 4) {
      	var status = request.status.toString();
        var headers = parseHeaders(request.getAllResponseHeaders());
        var content = request.responseText;
        if (status == '207') {                    	
        	//content = self._parseMultiStatus(content);                        
        };
        var statusstring = "";//davlib.STATUS_CODES[status];
        handler.call(context, status, statusstring, 
                     content, headers);
      };
    };
  };
  return (new HandlerWrapper().execute);
}

function parseHeaders(headerstring) {
	var lines = headerstring.split('\n');
  var headers = {};
  for (var i=0; i < lines.length; i++) {
  	var line = $.trim(lines[i]);
    if (!line) {
    	continue;
    };
    var chunks = line.split(':');
    var key = $.trim(chunks.shift());
    var value = $.trim(chunks.join(':'));
    var lkey = key.toLowerCase();
    if (headers[lkey] !== undefined) {
    	if (!headers[lkey].push) {
      	headers[lkey] = [headers[lkey, value]];
      } else {
      	headers[lkey].push(value);
      };
    } else {
    	headers[lkey] = value;
    };
  };
  return headers;
}
*/