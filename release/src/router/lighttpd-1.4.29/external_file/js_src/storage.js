var myStorage = function() {
	
	return {
		"get": function(storageName) {
			if(window.sessionStorage)
				return window.sessionStorage[storageName];
			else
				return $.cookie(storageName);
    },
    "set": function(storageName, val) {
    	if(window.sessionStorage)
				window.sessionStorage[storageName] = val;
			else{				
				$.cookie(storageName, val);
			}
    },
    "getl": function(storageName) {
			if(window.localStorage)
				return window.localStorage[storageName];
			else
				return $.cookie(storageName);
    },
    "setl": function(storageName, val) {
    	if(window.localStorage)
    		window.localStorage[storageName] = val;
			else
				$.cookie(storageName, val);
    },
    "gett": function(storageName) {
			return $.cookie(storageName);
    },
    "sett": function(storageName, val) {
    	$.cookie(storageName, val);
    }
	}
};