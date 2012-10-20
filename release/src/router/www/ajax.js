// Use AJAX to get nvram
var http_request = false;

function makeRequest(url) {
	http_request = new XMLHttpRequest();
	if (http_request && http_request.overrideMimeType)
		http_request.overrideMimeType('text/xml');
	else
		return false;

	http_request.onreadystatechange = function(){
			alertContents(this);
		};
	http_request.open('GET', url, true);
	http_request.send(null);
}

var xmlDoc_ie_ajax;

function makeRequest_ie(file)
{
	xmlDoc_ie_ajax = new ActiveXObject("Microsoft.XMLDOM");
	xmlDoc_ie_ajax.async = false;
	if (xmlDoc_ie_ajax.readyState==4)
	{
		xmlDoc_ie_ajax.load(file);
		refresh_wpsinfo(xmlDoc_ie_ajax);
	}
}

function alertContents(request_obj)
{
	if (request_obj != null && request_obj.readyState != null && request_obj.readyState == 4)
	{
		if (request_obj.status != null && request_obj.status == 200)
		{
			var xmldoc_mz = request_obj.responseXML;
			refresh_wpsinfo(xmldoc_mz);
		}
	}
}
