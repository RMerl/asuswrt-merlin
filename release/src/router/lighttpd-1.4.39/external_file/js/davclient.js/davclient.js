/*
    davclient.js - Low-level JavaScript WebDAV client implementation
    Copyright (C) 2004-2007 Guido Wesdorp
    email johnny@johnnydebris.net

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

var global = this;
// create a namespace for our stuff... notice how we define a class and create
// an instance at the same time
global.davlib = new function() {
    /* WebDAV for JavaScript
    
        This is a library containing a low-level and (if loaded, see 
        'davfs.js') a high-level API for working with WebDAV capable servers
        from JavaScript. 

        Quick example of the low-level interface:

          var client = new davlib.DavClient();
          client.initialize();

          function alertContent(status, statusstring, content) {
            if (status != 200) {
              alert('error: ' + statusstring);
              return;
            };
            alert('content received: ' + content);
          };
          
          client.GET('/foo/bar.txt', alertContent);

        Quick example of the high-level interface:

          var fs = new davlib.DavFS();
          fs.initialize();

          function alertContent(error, content) {
            if (error) {
              alert('error: ' + error);
              return;
            };
            alert('content: ' + content);
          };

          fs.read('/foo/bar.txt', alertContent);

        (Note that since we only read a simple file here, changes between the
        high- and low-level APIs are very small.)

        For more examples and information, see the DavClient and DavFS 
        constructors, and the README.txt file and the tests in the package. 
        For references of single methods, see the comments in the code (sorry 
        folks, but keeping code and documents up-to-date is really a nuisance 
        and I'd like to avoid that until things are stable enough).

    */
    var davlib = this;
	var g_storage = new myStorage();
	
    this.DEBUG = 0;

    this.STATUS_CODES = {
        '100': 'Continue',
        '101': 'Switching Protocols',
        '102': 'Processing',
        '200': 'OK',
        '201': 'Created',
        '202': 'Accepted',
        '203': 'None-Authoritive Information',
        '204': 'No Content',
        // seems that there's some bug in IE (or Sarissa?) that 
        // makes it spew out '1223' status codes when '204' is
        // received... needs some investigation later on
        '1223': 'No Content',
        '205': 'Reset Content',
        '206': 'Partial Content',
        '207': 'Multi-Status',
        '300': 'Multiple Choices',
        '301': 'Moved Permanently',
        '302': 'Found',
        '303': 'See Other',
        '304': 'Not Modified',
        '305': 'Use Proxy',
        '307': 'Redirect',
        '400': 'Bad Request',
        '401': 'Unauthorized',
        '402': 'Payment Required',
        '403': 'Forbidden',
        '404': 'Not Found',
        '405': 'Method Not Allowed',
        '406': 'Not Acceptable',
        '407': 'Proxy Authentication Required',
        '408': 'Request Time-out',
        '409': 'Conflict',
        '410': 'Gone',
        '411': 'Length Required',
        '412': 'Precondition Failed',
        '413': 'Request Entity Too Large',
        '414': 'Request-URI Too Large',
        '415': 'Unsupported Media Type',
        '416': 'Requested range not satisfiable',
        '417': 'Expectation Failed',
        '422': 'Unprocessable Entity',
        '423': 'Locked',
        '424': 'Failed Dependency',
        '500': 'Internal Server Error',
        '501': 'Not Implemented',
        '502': 'Bad Gateway',
        '503': 'Service Unavailable',
        '504': 'Gateway Time-out',
        '505': 'HTTP Version not supported',
        '507': 'Insufficient Storage'
    };

    this.DavClient = function() {
        /* Low level (subset of) WebDAV client implementation 
        
            Basically what one would expect from a basic DAV client, it
            provides a method for every HTTP method used in basic DAV, it
            parses PROPFIND requests to handy JS structures and accepts 
            similar structures for PROPPATCH.
            
            Requests are handled asynchronously, so instead of waiting until
            the response is sent back from the server and returning the
            value directly, a handler is registered that is called when
            the response is available and the method that sent the request
            is ended. For that reason all request methods accept a 'handler'
            argument, which will be called (with 3 arguments: statuscode,
            statusstring and content (the latter only where appropriate))
            when the request is handled by the browser.
            The reason for this choice is that Mozilla sometimes freezes
            when using XMLHttpRequest for synchronous requests.

            The only 'public' methods on the class are the 'initialize'
            method, that needs to be called first thing after instantiating
            a DavClient object, and the methods that have a name similar to
            an HTTP method (GET, PUT, etc.). The latter all get at least a
            'path' argument, a 'handler' argument and a 'context' argument:

                'path' - an absolute path to the target resource
                'handler' - a function or method that will be called once
                        the request has finished (see below)
                'context' - the context used to call the handler, the
                        'this' variable inside methods, so usually the
                        object (instance) the handler is bound to (ignore 
                        when the handler is a function)

            All handlers are called with the same 3 arguments:
            
                'status' - the HTTP status code
                'statusstring' - a string representation (see STATUS_CODES
                        array above) of the status code
                'content' - can be a number of different things:
                        * when there was an error in a method that targets
                            a single resource, this contains the error body
                        * when there was an error in a method that targets
                            a set of resources (multi-status) it contains
                            a Root object instance (see below) that contains
                            the error messages of all the objects
                        * if the method was GET and there was no error, it
                            will contain the contents of the resource
                        * if the method was PROPFIND and there was no error,
                            it will contain a Root object (see below) that
                            contains the properties of all the resources
                            targeted
                        * if there was no error and there is no content to
                            return, it will contain null
                'headers' - a mapping (associative array) from lowercase header
                            name to value (string)

            Basic usage example:

                function handler(status, statusstring, content, headers) {
                    if (content) {
                        if (status != '200' && status != '204') {
                            if (status == '207') {
                                alert('not going to show multi-status ' +
                                        here...');
                            };
                            alert('Error: ' + statusstring);
                        } else {
                            alert('Content: ' + content);
                        };
                    };
                };

                var dc = new DavClient();
                dc.initialize('localhost');

                // create a directory
                dc.MKCOL('/foo', handler);

                // create a file and save some contents
                dc.PUT('/foo/bar.txt', 'baz?', handler);

                // load and alert it ( happens in the handler)
                dc.GET('/foo/bar.txt', handler);

                // lock the file, we need to store the lock token from 
                // the result
                function lockhandler(status, statusstring, content, headers) {
                    if (status != '200') {
                        alert('Error unlocking: ' + statusstring);
                    } else {
                        window.CURRENT_LOCKTOKEN = headers.locktoken;
                    };
                };
                dc.LOCK('/foo/bar.txt', 'http://johnnydebris.net/', 
                            lockhandler);

                // run the following bit only if the lock was set properly
                if (window.CURRENT_LOCKTOKEN) {
                    // try to delete it: this will fail
                    dc.DELETE('/foo/bar.txt', handler);
                    
                    // now unlock it using the lock token stored above
                    dc.UNLOCK('/foo/bar.txt', window.CURRENT_LOCKTOKEN,
                              handler);
                };

                // delete the dir
                dc.DELETE('/foo', handler);

            For detailed information about the HTTP methods and how they
            can/should be used in a DAV context, see http://www.webdav.org.

            This library depends on version 0.3 of the 'dommer' package
            and version 0.2 of the 'minisax.js' package, both of which
            should be available from http://johnnydebris.net under the
            same license as this one (GPL).

            If you have questions, bug reports, or patches, please send an 
            email to johnny@johnnydebris.net.
        */
    };

    this.DavClient.prototype.initialize = function(host, port, protocol) {
        /* the 'constructor' (needs to be called explicitly!!) 
        
            host - the host name or IP
            port - HTTP port of the host (optional, defaults to 80)
            protocol - protocol part of URLs (optional, defaults to http)
        */
        
        this.host = host || location.hostname;
        this.port = port || location.port;// || 80;
        this.protocol = (protocol || 
                         location.protocol.substr(0, 
                                                  location.protocol.length - 1
                                                  ) ||
                         'http');
        
        this.request = null;
    };

    this.DavClient.prototype.OPTIONS = function(path, handler, context) {
        /* perform an OPTIONS request

            find out which HTTP methods are understood by the server
        */
        // XXX how does this work with * paths?
        var request = this._getRequest('OPTIONS', path, handler, context);
        request.send('');
    };

    this.DavClient.prototype.GET = function(path, handler, context) {
        /* perform a GET request 
        
            retrieve the contents of a resource
        */
        var request = this._getRequest('GET', path, handler, context);
        request.send('');
    };
    
		//charles mark	
    this.DavClient.prototype.PUT = function(path, content, sliceAsBinary,
											start, stop, filesize, autoCreateFolder,
											handler, 
                                            updateProgress, context, locktoken ) {
        /* perform a PUT request
            save the contents of a resource to the server
            'content' - the contents of the resource
        */
        var request = this._getRequest('PUT', path, handler, context);
        request.setRequestHeader("Content-type", "text/xml,charset=UTF-8");
		/*
			 for example  Content-Range: bytes 21010-47021/47022
		*/
		//alert("set request start ="+start+"stop="+stop+"filesize="+filesize+", sliceAsBinary="+sliceAsBinary);	
        request.setRequestHeader("Content-Range", "bytes "+start+"-"+stop+"/"+filesize);
        
        if (locktoken) {
            request.setRequestHeader('If', '<' + locktoken + '>');
        };
        
        if(autoCreateFolder){
        	request.setRequestHeader("Auto-CreateFolder", autoCreateFolder);
      	}
      
        if(request.upload){
        	request.upload.onprogress = updateProgress;
        }
       	
        try{
        	if(sliceAsBinary == 0 && request.sendAsBinary != null){
        		request.sendAsBinary(content);
			}
			else if(sliceAsBinary == 1){
				request.send(content);
        	}
        	else{
        		throw '';
        	}
        } 
        catch(e) {
          // not a Mozilla or Konqueror based browser
          alert("Fail to PUT data to webdav " + e);
        };
    };

    this.DavClient.prototype.DELETE = function(path, handler, 
                                               context, locktoken) {
        /* perform a DELETE request 
        
            remove a resource (recursively)
        */
        var request = this._getRequest('DELETE', path, handler, context);
        
        if (locktoken) {
            request.setRequestHeader('If', '<' + locktoken + '>');
        };
        //request.setRequestHeader("Depth", "Infinity");
        request.send('');
    };

    this.DavClient.prototype.MKCOL = function(path, handler, 
                                              context, locktoken) {
        /* perform a MKCOL request

            create a collection
        */
        var request = this._getRequest('MKCOL', path, handler, context);
        if (locktoken) {
            request.setRequestHeader('If', '<' + locktoken + '>');
        };
        request.send('');
    };

    this.DavClient.prototype.COPY = function(path, topath, handler, 
                                             context, overwrite, locktoken) {
        /* perform a COPY request

            create a copy of a resource

            'topath' - the path to copy the resource to
            'overwrite' - whether or not to fail when the resource 
                    already exists (optional)
        */
        var request = this._getRequest('COPY', path, handler, context);
        var tourl = this._generateUrl(topath);
        request.setRequestHeader("Destination", tourl);        
        if (overwrite) {
            request.setRequestHeader("Overwrite", overwrite);
        };
        
        if (locktoken) {
            request.setRequestHeader('If', '<' + locktoken + '>');
        };
        request.send('');
    };

    this.DavClient.prototype.MOVE = function(path, topath, handler, 
                                             context, overwrite, locktoken) {
        /* perform a MOVE request

            move a resource from location

            'topath' - the path to move the resource to
            'overwrite' - whether or not to fail when the resource
                    already exists (optional)
        */        
        var request = this._getRequest('MOVE', path, handler, context);
        var tourl = this._generateUrl(topath); 
        
        request.setRequestHeader("Destination", tourl);
        if (overwrite) {
            request.setRequestHeader("Overwrite", overwrite);
        };
        
        if (locktoken) {        	
            request.setRequestHeader('If', '<' + locktoken + '>');
        };
        request.send('');
    };

    this.DavClient.prototype.PROPFIND = function(path, auth, handler, 
                                                 context, depth, mtype) {
        /* perform a PROPFIND request

            read the metadata of a resource (optionally including its children)

            'depth' - control recursion depth, default 0 (only returning the
                    properties for the resource itself)
        */  
        
        var request = this._getRequest('PROPFIND', path, handler, context);
        depth = depth || 0;
        
        if(auth!=undefined){
        	request.setRequestHeader('Authorization', auth);
        	//alert("setRequestHeader->" + auth);
       	}
        
        request.setRequestHeader('Depth', depth);
		request.setRequestHeader('Mtype', mtype);
        request.setRequestHeader('Content-type', 'text/xml; charset=UTF-8');
        // XXX maybe we want to change this to allow getting selected props
                
        var xml = '<?xml version="1.0" encoding="UTF-8" ?>' +
                  '<D:propfind xmlns:D="DAV:">' +
                  '<D:allprop />' +
                  '</D:propfind>';
        
        /*
        var xml = '<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>' +
									'<D:propfind xmlns:D="DAV:">' +
									'<D:prop>' +
									'<D:getlastmodified/>' +
									'<D:getcontentlength/>' +
									'<D:getcontenttype/>' +
									//'<D:resourcetype/>' +
									//'<D:getetag/>' +
									//'<D:lockdiscovery/>' +
									'<D:getmac/>' +
									'<D:getuniqueid/>' +
									'<D:getonline/>' +
									'<D:gettype/>' +
									'<D:getattr/>' +
									'</D:prop>' +
									'</D:propfind>';
        */
        
        request.send(xml);        
    };
	
	this.DavClient.prototype.OAUTH = function(path,auth,enc,handler,context,locktoken){			
		var request = this._getRequest('OAUTH',path,handler,context);
		request.setRequestHeader('Authorization', auth);
		request.setRequestHeader('Cookie', enc);
		
		request.send('');
	};
		
	this.DavClient.prototype.WOL = function(path,mac,handler,context,locktoken){			
		var request = this._getRequest('WOL',path,handler,context);
		request.setRequestHeader("WOLMAC",mac);		
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.GSL = function(path,urlPath,fileName,expire,toShare,handler,context,locktoken){			
		var request = this._getRequest('GSL',path,handler,context);
		request.setRequestHeader("URL",urlPath);
		request.setRequestHeader("FILENAME",fileName);
		request.setRequestHeader("EXPIRE",expire);
		request.setRequestHeader("TOSHARE",toShare);
		
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.GSLL = function(path,handler,context,locktoken){
		var request = this._getRequest('GSLL',path,handler,context);
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.REMOVESL = function(path,sharelink,handler,context,locktoken){			
		var request = this._getRequest('REMOVESL',path,handler,context);
		request.setRequestHeader("SHARELINK",sharelink);
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.LOGOUT = function(path,handler,context,locktoken){			
		var request = this._getRequest('LOGOUT',path,handler,context);			
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.GETSRVTIME = function(path,handler,context,locktoken){			
		var request = this._getRequest('GETSRVTIME',path,handler,context);			
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.RESCANSMBPC = function(path,handler,context,locktoken){			
		var request = this._getRequest('RESCANSMBPC',path,handler,context);			
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.GETROUTERMAC = function(path,handler,context,locktoken){			
		var request = this._getRequest('GETROUTERMAC',path,handler,context);			
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.GETROUTERINFO = function(path,handler,context,locktoken){			
		var request = this._getRequest('GETROUTERINFO',path,handler,context);			
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.GETNOTICE = function(path,timestamp,handler,context,locktoken){
		var request = this._getRequest('GETNOTICE',path,handler,context);	
		request.setRequestHeader("TIMESTAMP",timestamp);
		request.send('');
	};
		
	this.DavClient.prototype.GETFIRMVER = function(path,handler,context,locktoken){			
		var request = this._getRequest('GETFIRMVER',path,handler,context);			
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.GETLATESTVER = function(path,handler,context,locktoken){			
		var request = this._getRequest('GETLATESTVER',path,handler,context);
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.GETDISKSPACE = function(path,diskname,handler,context,locktoken){			
		var request = this._getRequest('GETDISKSPACE',path,handler,context);
		request.setRequestHeader("DISKNAME",diskname);
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.PROPFINDMEDIALIST = function(path, handler, context, media_type, start, 
			end, keyword, orderby, orderrule, parentid){
		/* perform a PROPFINDMEDIALIST request
		*/
		
		var request = this._getRequest('PROPFINDMEDIALIST', path, handler, context);
		request.setRequestHeader('Content-type', 'text/xml; charset=UTF-8');
		
		if(media_type){
			request.setRequestHeader("MediaType", media_type);
		};
			
		if(start) {
			request.setRequestHeader("Start", start);
		};
												  
		if (end) {
			request.setRequestHeader("End", end);
		};
		
		if (keyword) {
			request.setRequestHeader("Keyword", keyword);
		};
		
		if (orderby) {
			request.setRequestHeader("Orderby", orderby);
		};
			
		if (orderrule) {
			request.setRequestHeader("Orderrule", orderrule);
		};
			
		if (parentid) {
			request.setRequestHeader("Parentid", parentid);
		};
		/*
		var xml = '<?xml version="1.0" encoding="UTF-8" ?>' +
                  '<D:propfind xmlns:D="DAV:">' +
                  '<D:allprop />' +
                  '</D:propfind>';
		*/
		var xml = '<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>' +
				  '<D:propfind xmlns:D="DAV:">' +
				  '<D:prop>' +
 				  '<D:getlastmodified/>' +
				  '<D:getcontentlength/>' +
				  '<D:getcontenttype/>' +
				  '<D:getmatadata/>' +						
				  '</D:prop>' +
				  '</D:propfind>';
									  
		request.send(xml);
	};
		
	this.DavClient.prototype.GETMUSICCLASSIFICATION = function(path,classify,handler,context,locktoken){			
		var request = this._getRequest('GETMUSICCLASSIFICATION',path,handler,context);	
		if (classify) {
			request.setRequestHeader("Classify", classify);
		};		
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.GETMUSICPLAYLIST = function(path,id,handler,context,locktoken){			
		var request = this._getRequest('GETMUSICPLAYLIST',path,handler,context);	
		if (id) {
			request.setRequestHeader("id", id);
		};		
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.GETPRODUCTICON = function(path,handler,context,locktoken){			
		var request = this._getRequest('GETPRODUCTICON',path,handler,context);
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
				
	this.DavClient.prototype.GETTHUMBIMAGE = function(path, file, handler,context,locktoken){			
		var request = this._getRequest('GETTHUMBIMAGE',path,handler,context);
		if (file) {
			request.setRequestHeader("File", file);
		};
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
	this.DavClient.prototype.GETVIDEOSUBTITLE = function(path,name,handler,context,locktoken){			
		var request = this._getRequest('GETVIDEOSUBTITLE',path,handler,context);
		request.setRequestHeader("FILENAME", name);
		if(locktoken){
			request.setRequestHeader('If','<'+locktoken+'>');
		};
		request.send('');
	};
		
		this.DavClient.prototype.UPLOADTOFACEBOOK = function(path,name,title,album,token,handler,context,locktoken){			
			var request = this._getRequest('UPLOADTOFACEBOOK',path,handler,context);
			request.setRequestHeader("FILENAME", name);
			request.setRequestHeader("TITLE", title);
			request.setRequestHeader("ALBUM", album);
			request.setRequestHeader("TOKEN", token);
			if(locktoken){
				request.setRequestHeader('If','<'+locktoken+'>');
			};
			request.send('');
		};
		
		this.DavClient.prototype.UPLOADTOFLICKR = function(path,name,title,token,handler,context,locktoken){
			var request = this._getRequest('UPLOADTOFLICKR',path,handler,context);
			request.setRequestHeader("FILENAME", name);
			request.setRequestHeader("TITLE", title);
			request.setRequestHeader("TOKEN", token);			
			if(locktoken){
				request.setRequestHeader('If','<'+locktoken+'>');
			};
			request.send('');
		};
		
		this.DavClient.prototype.UPLOADTOPICASA = function(path,name,title,uid,aid,token,handler,context,locktoken){
			var request = this._getRequest('UPLOADTOPICASA',path,handler,context);
			request.setRequestHeader("FILENAME", name);
			request.setRequestHeader("TITLE", title);
			request.setRequestHeader("UID", uid);
			request.setRequestHeader("AID", aid);
			request.setRequestHeader("TOKEN", token);			
			if(locktoken){
				request.setRequestHeader('If','<'+locktoken+'>');
			};
			request.send('');
		};
		
		this.DavClient.prototype.UPLOADTOTWITTER = function(path,name,title,token,secret,nonce,timestamp,signature,photo_size_limit,handler,context,locktoken){
			var request = this._getRequest('UPLOADTOTWITTER',path,handler,context);
			request.setRequestHeader("FILENAME", name);
			request.setRequestHeader("TITLE", title);
			request.setRequestHeader("TOKEN", token);			
			request.setRequestHeader("SECRET", secret);
			request.setRequestHeader("NONCE", nonce);
			request.setRequestHeader("TIMESTAMP", timestamp);
			request.setRequestHeader("SIGNATURE", signature);
			request.setRequestHeader("PHOTOSIZELIMIT", photo_size_limit);
			if(locktoken){
				request.setRequestHeader('If','<'+locktoken+'>');
			};
			request.send('');
		};
		
		this.DavClient.prototype.GENROOTCERTIFICATE = function(path,keylen,caname,email,country,state,ln,orag,ounit,cn,handler,context,locktoken){			
			var request = this._getRequest('GENROOTCERTIFICATE',path,handler,context);	
			request.setRequestHeader("KEYLEN", keylen); //- Private key length
			request.setRequestHeader("CANAME", caname); //- CA name
			request.setRequestHeader("EMAIL", email); //- email address
			request.setRequestHeader("COUNTRY", country); //- Country Name(2 letter code)
			request.setRequestHeader("STATE", state); //- State or Province Name(full name)
			request.setRequestHeader("LN", ln); //- Locality Name(eg, city)
			request.setRequestHeader("ORAG", orag);//- Organization Name(eg, company)
			request.setRequestHeader("OUNIT", ounit); //- Organizational Unit Name(eg, section)
			request.setRequestHeader("CN", cn); //- Common Name(eg. your name or your server's hostname)
			request.send('');
		};
		
		this.DavClient.prototype.SETROOTCERTIFICATE = function(path,key,cert,intermediate_crt,handler,context,locktoken){			
			var request = this._getRequest('SETROOTCERTIFICATE',path,handler,context);
			request.setRequestHeader('Content-type', 'text/xml; charset=UTF-8');
			
			var xml = '<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>';
			xml += '<content>';
			xml += '<key>' + key + '</key>';
	 		xml += '<cert>' + cert + '</cert>';
	 		
	 		if(intermediate_crt!="")
	 			xml += '<intermediate_crt>' + intermediate_crt + '</intermediate_crt>';
	 				  
			xml += '</content>'; 
			 
			request.send(xml);	
		};
		
		this.DavClient.prototype.GETX509CERTINFO = function(path, handler,context,locktoken){			
			var request = this._getRequest('GETX509CERTINFO',path,handler,context);
			request.send('');
		};
			
		this.DavClient.prototype.APPLYAPP = function(path, action, nvram, service, handler,context,locktoken){			
			var request = this._getRequest('APPLYAPP',path,handler,context);
			request.setRequestHeader("ACTION_MODE", action);
			request.setRequestHeader("SET_NVRAM", nvram);
			request.setRequestHeader("RC_SERVICE", service);
			request.send('');
		};
		
		this.DavClient.prototype.NVRAMGET = function(path, key, handler,context,locktoken){			
			var request = this._getRequest('NVRAMGET',path,handler,context);
			request.setRequestHeader("KEY", key);
			request.send('');
		};
		
		this.DavClient.prototype.GETCPUUSAGE = function(path, handler,context,locktoken){			
			var request = this._getRequest('GETCPUUSAGE',path,handler,context);
			request.send('');
		};
		
		this.DavClient.prototype.GETMEMORYUSAGE = function(path, handler,context,locktoken){			
			var request = this._getRequest('GETMEMORYUSAGE',path,handler,context);
			request.send('');
		};
		
		this.DavClient.prototype.UPDATEACCOUNT = function(path,id,username,password,type,permission,handler,context,locktoken){			
			var request = this._getRequest('UPDATEACCOUNT',path,handler,context);
			
			request.setRequestHeader('Content-type', 'text/xml; charset=UTF-8');
			
			var xml = '<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>';
			xml += '<content>';
			xml += '<id>' + id + '</id>';
			xml += '<username>' + username + '</username>';
			xml += '<password>' + password + '</password>';
			xml += '<type>' + type + '</type>';
			xml += '<permission>' + permission + '</permission>';
			xml += '</content>'; 
			 
			request.send(xml);
		};
		
		this.DavClient.prototype.GETACCOUNTINFO = function(path,username,handler,context,locktoken){			
			var request = this._getRequest('GETACCOUNTINFO',path,handler,context);
			request.setRequestHeader("USERNAME", username);
			request.send('');	
		};
		
		this.DavClient.prototype.GETACCOUNTLIST = function(path,handler,context,locktoken){			
			var request = this._getRequest('GETACCOUNTLIST',path,handler,context);
			request.send('');	
		};
		
		this.DavClient.prototype.DELETEACCOUNT = function(path,username,handler,context,locktoken){			
			var request = this._getRequest('DELETEACCOUNT',path,handler,context);
			request.setRequestHeader("USERNAME", username);
			request.send('');	
		};
		
		this.DavClient.prototype.UPDATEACCOUNTINVITE = function(path,invite_token,permission,enable_smart_access,security_code,handler,context,locktoken){			
			var request = this._getRequest('UPDATEACCOUNTINVITE',path,handler,context);
			
			request.setRequestHeader('Content-type', 'text/xml; charset=UTF-8');
			
			var xml = '<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>';
			xml += '<content>';
			xml += '<token>' + invite_token + '</token>';
			xml += '<permission>' + permission + '</permission>';
			xml += '<enable_smart_access>' + enable_smart_access + '</enable_smart_access>';
			xml += '<security_code>' + security_code + '</security_code>';
			xml += '</content>'; 
			 
			request.send(xml);	
		};
		
		this.DavClient.prototype.GETACCOUNTINVITEINFO = function(path,token,handler,context,locktoken){			
			var request = this._getRequest('GETACCOUNTINVITEINFO',path,handler,context);
			request.setRequestHeader("TOKEN", token);
			request.send('');	
		};
		
		this.DavClient.prototype.GETACCOUNTINVITELIST = function(path,handler,context,locktoken){			
			var request = this._getRequest('GETACCOUNTINVITELIST',path,handler,context);
			request.send('');	
		};
		
		this.DavClient.prototype.DELETEACCOUNTINVITE = function(path,token,handler,context,locktoken){			
			var request = this._getRequest('DELETEACCOUNTINVITE',path,handler,context);
			request.setRequestHeader("TOKEN", token);
			request.send('');	
		};
		
		this.DavClient.prototype.OPENSTREAMINGPORT = function(path,open,handler,context,locktoken){			
			var request = this._getRequest('OPENSTREAMINGPORT',path,handler,context);
			request.setRequestHeader("OPEN", open);
			request.send('');	
		};
			
    	// XXX not sure about the order of the args here
    	this.DavClient.prototype.PROPPATCH = function(path, handler, context, 
                                                  setprops, delprops,
                                                  locktoken) {
        	/* perform a PROPPATCH request

            set the metadata of a (single) resource

            'setprops' - a mapping {<namespace>: {<key>: <value>}} of
                    variables to set
            'delprops' - a mapping {<namespace>: [<key>]} of variables
                    to delete
        	*/
			var request = this._getRequest('PROPPATCH', path, handler, context);
			request.setRequestHeader('Content-type', 'text/xml; charset=UTF-8');
			if (locktoken) {
				request.setRequestHeader('If', '<' + locktoken + '>');
			};
			var xml = this._getProppatchXml(setprops, delprops);
			request.send(xml);
		};

    	this.DavClient.prototype.LOCK = function(path, owner, handler, context, 
                                             scope, type, depth, timeout,
                                             locktoken) {
	        /* perform a LOCK request
	
	            set a lock on a resource
	
	            'owner' - a URL to identify the owner of the lock to be set
	            'scope' - the scope of the lock, can be 'exclusive' or 'shared'
	            'type' - the type of lock, can be 'write' (somewhat strange, eh?)
	            'depth' - can be used to lock (part of) a branch (use 'infinity' as
	                        value) or just a single target (default)
	            'timeout' - set the timeout in seconds
	        */
	        if (!scope) {
	            scope = 'exclusive';
	        };
	        if (!type) {
	            type = 'write';
	        };
	        var request = this._getRequest('LOCK', path, handler, context);
	        if (depth) {
	            request.setRequestHeader('Depth', depth);
	        }else{
	        	request.setRequestHeader('Depth', 0);
	        }
	        if (!timeout) {
	            timeout = "Infinite, Second-4100000000";
	        } else {
	            timeout = 'Second-' + timeout;
	        };
	        if (locktoken) {
	            request.setRequestHeader('If', '<' + locktoken + '>');
	        };
	        request.setRequestHeader("Content-Type", "text/xml; charset=UTF-8");
	        request.setRequestHeader('Timeout', timeout);
	        var xml = this._getLockXml(owner, scope, type);        
	        request.send(xml);
	    };

	    this.DavClient.prototype.UNLOCK = function(path, locktoken, 
	                                               handler, context) {
	        /* perform an UNLOCK request
	
	            unlock a previously locked file
	
	            'token' - the opaque lock token, as can be retrieved from 
	                        content.locktoken using a LOCK request.
	        */
	        var request = this._getRequest('UNLOCK', path, handler, context);
	        request.setRequestHeader("Lock-Token", '<' + locktoken + '>');
	        request.send('');
	    };

	    this.DavClient.prototype._getRequest = function(method, path, 
	                                                    handler, context) {
	        /* prepare a request */
	        var request = davlib.getXmlHttpRequest();
	        if (method == 'LOCK') {
	            // LOCK requires parsing of the body on 200, so has to be treated
	            // differently
	            request.onreadystatechange = this._wrapLockHandler(handler, 
	                                                            request, context);
	        } else {
	            request.onreadystatechange = this._wrapHandler(handler, 
	                                                            request, context);
	        };
	        	
	        var url = this._generateUrl(path);        
	        request.open(method, url, true);
	        
	        // refuse all encoding, since the browsers don't seem to support it...
	        //request.setRequestHeader('Accept-Encoding', ' ');
	        
	        /*
	        var auth = g_storage.get("auth");
	        if(auth!="" && auth!=undefined){
	        	alert(path+" / " + auth);
	        	request.setRequestHeader('Authorization', auth);
	        }
	        */
	       
	        return request
	    };

	    this.DavClient.prototype._wrapHandler = function(handler, request,
	                                                     context) {
	        /* wrap the handler with a callback
	
	            The callback handles multi-status parsing and calls the client's
	            handler when done
	        */
	        var self = this;
	        function HandlerWrapper() {
	            this.execute = function() {            	
	                if (request.readyState == 4) {
	                    var status = request.status.toString();
	                    var headers = self._parseHeaders(
	                                        request.getAllResponseHeaders());
	                    var content = request.responseText;
	                    if (status == '207') {                    	
	                        //content = self._parseMultiStatus(content);                        
	                    };
	                    var statusstring = davlib.STATUS_CODES[status];
	                    
	                    if(handler!=null)
	                    	handler.call(context, status, statusstring, 
	                                    content, headers);
	                };
	            };
	        };
	        return (new HandlerWrapper().execute);
	    };

    this.DavClient.prototype._wrapLockHandler = function(handler, request, 
                                                         context) {
        /* wrap the handler for a LOCK response

            The callback handles parsing of specific XML for LOCK requests
        */
        var self = this;
        function HandlerWrapper() {
            this.execute = function() {
                if (request.readyState == 4) {
                    var status = request.status.toString();
                    var headers = self._parseHeaders(
                                        request.getAllResponseHeaders());
                    var content = request.responseText;
                    if (status == '200') {
                        content = self._parseLockinfo(content);
                    } else if (status == '207') {
                        content = self._parseMultiStatus(content);
                    };
                    var statusstring = davlib.STATUS_CODES[status];
                    handler.call(context, status, statusstring, 
                                 content, headers);
                };
            };
        };
        return (new HandlerWrapper().execute);
    };

    this.DavClient.prototype._generateUrl = function(path){
        /* convert a url from a path */
        var url = this.protocol + '://' + this.host;
        
        //if (this.port && this.protocol!='https') {
        if (this.port) {
            url += ':' + this.port;
        };
        
        url += path;
        
        return url;
    };

    this.DavClient.prototype._parseMultiStatus = function(xml) {
        /* parse a multi-status request 
        
            see MultiStatusSaxHandler below
        */
        var handler = new davlib.MultiStatusSAXHandler();
        var parser = new SAXParser();
        parser.initialize(xml, handler);
        parser.parse();
        return handler.root;
    };

    this.DavClient.prototype._parseLockinfo = function(xml) {
        /* parse a multi-status request 
        
            see MultiStatusSaxHandler below
        */
        var handler = new davlib.LockinfoSAXHandler();
        var parser = new SAXParser();
        parser.initialize(xml, handler);
        parser.parse();
        return handler.lockInfo;
    };

    this.DavClient.prototype._getProppatchXml = function(setprops, delprops) {
        /* create the XML for a PROPPATCH request

            setprops is a mapping from namespace to a mapping
            of key/value pairs (where value is an *entitized* XML string), 
            delprops is a mapping from namespace to a list of names
        */
        var xml = '<?xml version="1.0" encoding="UTF-8" ?>\n' +
                    '<D:propertyupdate xmlns:D="DAV:">\n';

        var shouldsetprops = false;
        for (var attr in setprops) {
            shouldsetprops = true;
        };
        if (shouldsetprops) {
            xml += '<D:set>\n';
            for (var ns in setprops) {
                for (var key in setprops[ns]) {
                    xml += '<D:prop>\n' +
                            this._preparePropElement(ns, key,
                                                     setprops[ns][key]) +
                            '</D:prop>\n';
                };
            };
            xml += '</D:set>\n';
        };

        var shoulddelprops = false;
        for (var attr in delprops) {
            shoulddelprops = true;
        };
        if (shoulddelprops) {
            xml += '<D:remove>\n<D:prop>\n';
            for (var ns in delprops) {
                for (var i=0; i < delprops[ns].length; i++) {
                    xml += '<' + delprops[ns][i] + ' xmlns="' + ns + '"/>\n';
                };
            };
            xml += '</D:prop>n</D:remove>\n';
        };

        xml += '</D:propertyupdate>';

        return xml;
    };

    this.DavClient.prototype._getLockXml = function(owner, scope, type) {
        var xml = '<?xml version="1.0" encoding="utf-8"?>\n'+
                    '<D:lockinfo xmlns:D="DAV:">\n' +
                    '<D:lockscope><D:' + scope + ' /></D:lockscope>\n' +
                    '<D:locktype><D:' + type + ' /></D:locktype>\n' +
                    '<D:owner>\n<D:href>' + 
                    string.entitize(owner) + 
                    '</D:href>\n</D:owner>\n' +
                    '</D:lockinfo>\n';
        return xml;
    };

    this.DavClient.prototype._preparePropElement = function(ns, key, value) {
        /* prepare the DOM for a property

            all properties have a DOM value to allow the same structure
            as in WebDAV
        */
        var dom = new dommer.DOM();
        // currently we expect the value to be a well-formed bit of XML that 
        // already contains the ns and key information...
        var doc = dom.parseXML(value);
        // ... so we don't need the following bit
        /*
        doc.documentElement._setProtected('nodeName', key);
        var pl = key.split(':');
        doc.documentElement._setProtected('prefix', pl[0]);
        doc.documentElement._setProtected('localName', pl[1]);
        doc.namespaceURI = ns;
        doc.documentElement._setProtected('namespaceURI', ns);
        */
        return doc.documentElement.toXML();
    };

    this.DavClient.prototype._parseHeaders = function(headerstring) {
        var lines = headerstring.split('\n');
        var headers = {};
        for (var i=0; i < lines.length; i++) {
            var line = string.strip(lines[i]);
            if (!line) {
                continue;
            };
            var chunks = line.split(':');
            var key = string.strip(chunks.shift());
            var value = string.strip(chunks.join(':'));
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
    };

    // MultiStatus parsing stuff

    this.Resource = function(href, props) {
        /* a single resource in a multi-status tree */
        this.items = [];
        this.parent;
        this.properties = {}; // mapping from namespace to key/dom mappings
    };

    this.Root = function() {
        /* although it subclasses from Resource this is merely a container */
    };

    this.Root.prototype = new this.Resource;

    // XXX this whole thing is rather messy...
    this.MultiStatusSAXHandler = function() {
        /* SAX handler to parse a multi-status response */
    };

    this.MultiStatusSAXHandler.prototype = new SAXHandler;

    this.MultiStatusSAXHandler.prototype.startDocument = function() {
        this.resources = [];
        this.depth = 0;
        this.current = null;
        this.current_node = null;
        this.current_prop_namespace = null;
        this.current_prop_name = null;
        this.current_prop_handler = null;
        this.prop_start_depth = null;
        // array with all nodenames to be able to build a path
        // to a node and check for parent and such
        this.elements = [];
    };

    this.MultiStatusSAXHandler.prototype.endDocument = function() {
        this.buildTree();
    };

    this.MultiStatusSAXHandler.prototype.startElement = function(namespace, 
                                                        nodeName, attributes) {
        this.depth++;
        this.elements.push([namespace, nodeName]);
        davlib.debug('start: ' + namespace + ':' + nodeName);
        davlib.debug('parent: ' + (this.elements.length ? 
                                   this.elements[this.elements.length - 2] :
                                   ''));
        if (this.current_node == 'property') {
            this.current_prop_handler.startElement(namespace, nodeName, 
                                                   attributes);
            return;
        };

        if (namespace == 'DAV:' && nodeName == 'response') {
            var resource = new davlib.Resource();
            if (this.current) {
                resource.parent = this.current;
            };
            this.current = resource;
            this.resources.push(resource);
        } else {
            var parent = this.elements[this.elements.length - 2];
            if (!parent) {
                return;
            };
            if (namespace == 'DAV:' && parent[0] == 'DAV:' && 
                    parent[1] == 'response' || parent[1] == 'propstat') {
                // default response vars
                if (nodeName == 'href') {
                    this.current_node = 'href';
                } else if (nodeName == 'status') {
                    this.current_node = 'status';
                };
            } else if (parent[0] == 'DAV:' && parent[1] == 'prop') {
                // properties
                this.current_node = 'property';
                this.current_prop_namespace = namespace;
                this.current_prop_name = nodeName;
                // use a DOMHandler to propagate calls to for props
                this.current_prop_handler = new dommer.DOMHandler();
                this.current_prop_handler.startDocument();
                this.current_prop_handler.startElement(namespace, nodeName, 
                                                       attributes);
                this.start_prop_depth = this.depth;
                davlib.debug('start property');
            };
        };
    };

    this.MultiStatusSAXHandler.prototype.endElement = function(namespace, 
                                                               nodeName) {
        davlib.debug('end: ' + namespace + ':' + nodeName);
        if (namespace == 'DAV:' && nodeName == 'response') {
            if (this.current) {
                this.current = this.current.parent;
            };
        } else if (this.current_node == 'property' && 
                namespace == this.current_prop_namespace && 
                nodeName == this.current_prop_name &&
                this.start_prop_depth == this.depth) {
            davlib.debug('end property');
            this.current_prop_handler.endElement(namespace, nodeName);
            this.current_prop_handler.endDocument();
            var dom = new dommer.DOM();
            var doc = dom.buildFromHandler(this.current_prop_handler);
            if (!this.current.properties[namespace]) {
                this.current.properties[namespace] = {};
            };
            this.current.properties[namespace][this.current_prop_name] = doc;
            this.current_prop_namespace = null;
            this.current_prop_name = null;
            this.current_prop_handler = null;
        } else if (this.current_node == 'property') {
            this.current_prop_handler.endElement(namespace, nodeName);
            this.depth--;
            this.elements.pop();
            return;
        };
        this.current_node = null;
        this.elements.pop();
        this.depth--;
    };

    this.MultiStatusSAXHandler.prototype.characters = function(data) {
        if (this.current_node) {
            if (this.current_node == 'status') {
                this.current[this.current_node] = data.split(' ')[1];
            } else if (this.current_node == 'href') {
                this.current[this.current_node] = data;
            } else if (this.current_node == 'property') {
                this.current_prop_handler.characters(data);
            };
        };
    };

    this.MultiStatusSAXHandler.prototype.buildTree = function() {
        /* builds a tree from the list of elements */
        // XXX Splitting this up wouldn't make it less readable,
        // I'd say...
        
        // first find root element
        var minlen = -1;
        var root;
        var rootpath;
        // var url_reg = /^.*:\/\/[^\/]*(\/.*)$/;
        for (var i=0; i < this.resources.length; i++) {
            var resource = this.resources[i];
            resource.path = resource.href.split('/');
            if (resource.path[resource.path.length - 1] == '') {
                resource.path.pop();
            };
            var len = resource.path.length;
            if (minlen == -1 || len < minlen) {
                minlen = len;
                root = resource;
                root.parent = null;
            };
        };

        // now build the tree
        // first get a list without the root
        var elements = [];
        for (var i=0; i < this.resources.length; i++) {
            var resource = this.resources[i];
            if (resource == root) {
                continue;
            };
            elements.push(resource);
        };
        while (elements.length) {
            var leftovers = [];
            for (var i=0; i < elements.length; i++) {
                var resource = elements[i];
                var path = resource.path;
                var current = root;
                // we want to walk each element on the path to see if there's
                // a corresponding element already available, and if so 
                // continue walking until we find the parent element of the
                // resource
                if (path.length == root.path.length + 1) {
                    root.items.push(resource);
                    resource.parent = root;
                } else {
                    // XXX still untested, and rather, ehrm, messy...
                    for (var j = root.path.length; j < path.length - 1; 
                            j++) {
                        for (var k=0; k < current.items.length; k++) {
                            var item = current.items[k];
                            if (item.path[item.path.length - 1] ==
                                    path[j]) {
                                if (j == path.length - 2) {
                                    // we have a match at the end of the path
                                    // and all elements before that, this is 
                                    // the current resource's parent
                                    item.items.push(resource);
                                    resource.parent = item;
                                } else {
                                    // a match means we this item is one in our
                                    // path to the root, follow it
                                    current = item;
                                };
                                break;
                            };
                        };
                    };
                    leftovers.push(resource);
                };
            };
            elements = leftovers;
        };

        this.root = root;
    };

    this.LockinfoSAXHandler = function() {
        /* SAX handler to parse a LOCK response */
    };

    this.LockinfoSAXHandler.prototype = new SAXHandler;

    this.LockinfoSAXHandler.prototype.startDocument = function() {
        this.lockInfo = {};
        this.currentItem = null;
        this.insideHref = false;
    };

    this.LockinfoSAXHandler.prototype.startElement = function(namespace, 
                                                              nodeName,
                                                              attributes) {
        if (namespace == 'DAV:') {
            if (nodeName == 'locktype' ||
                    nodeName == 'lockscope' ||
                    nodeName == 'depth' ||
                    nodeName == 'timeout' ||
                    nodeName == 'owner' ||
                    nodeName == 'locktoken') {
                this.currentItem = nodeName;
            } else if (nodeName == 'href') {
                this.insideHref = true;
            };
        };
    };

    this.LockinfoSAXHandler.prototype.endElement = function(namespace, 
                                                            nodeName) {
        if (namespace == 'DAV:') {
            if (nodeName == 'href') {
                this.insideHref = false;
            } else {
                this.currentItem = null;
            };
        };
    };

    this.LockinfoSAXHandler.prototype.characters = function(data) {
        if (this.currentItem && 
                (this.currentItem != 'owner' || this.insideHref) &&
                (this.currentItem != 'locktoken' || this.insideHref)) {
            this.lockInfo[this.currentItem] = data;
        };
    };

    // some helper functions
    this.getXmlHttpRequest = function() {
    		
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
				
    	/*
        instantiate an XMLHTTPRequest 

        this can be improved by testing the user agent better and, in case 
        of IE, finding out which MSXML is installed and such, but it 
        seems to work just fine for now
        */
        
        try{
        	if(isIE&&getInternetExplorerVersion()<=8&&window.ActiveXObject){
        		return new window.ActiveXObject("Microsoft.XMLHTTP");
          	}
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
    };

    this.debug = function(text) {
        /* simple debug function

            set the DEBUG global to some true value, and messages will appear
            on the bottom of the document
        */
        if (!davlib.DEBUG) {
            return;
        };
        var div = document.createElement('div');
        var text = document.createTextNode(text);
        div.appendChild(text);
        document.getElementsByTagName('body')[0].appendChild(div);
    };
}();

