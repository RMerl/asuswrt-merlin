define(function(){
	var makeRequest;

	function makeRequest_Native(url, callBackSuccess, callBackError) {
		var xmlDoc = new XMLHttpRequest();

		xmlDoc.onreadystatechange =  function(){
			if(xmlDoc.readyState == 4){
				if(xmlDoc.status == 200)
					callBackSuccess(xmlDoc.responseXML);	
				else
					callBackError();
			} 
		};

		xmlDoc.open('GET', url, true);
		xmlDoc.send(null);
	}

	function makeRequest_ActiveX(file, callBackSuccess, callBackError)
	{
		var xmlDoc_ActivX = new ActiveXObject("Microsoft.XMLDOM");
		
		xmlDoc_ActivX.async = false;
		if (xmlDoc_ActivX.readyState==4)
		{
			xmlDoc_ActivX.load(file);
			callBackSuccess(xmlDoc_ActivX);
		}
		else
			callBackError();
	}

	makeRequest.isSupport = true;

    makeRequest.start = function(file, callBackSuccess, callBackError){
		if(window.XMLHttpRequest)
			makeRequest_Native(file, callBackSuccess, callBackError);
		else if(window.ActiveXObject)
			makeRequest_ActiveX(file, callBackSuccess, callBackError);
		else{
			makeRequest.isSupport = false;
			alert("Your browser does not support XMLHTTP.");
		}
    }
    
    return makeRequest;
});