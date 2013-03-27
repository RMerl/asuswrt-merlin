/*****************************************************************************
 *
 * Copyright (c) 2004-2007 Guido Wesdorp. All rights reserved.
 *
 * This software is distributed under the terms of the JSBase
 * License. See LICENSE.txt for license text.
 *
 *****************************************************************************/

if (!this.jsbase) {
    this.jsbase = {};
};
this.jsbase.server = this.server = new function() {
    /* helper functions to communicate with the server 

        'AJAX' like helper stuff
    */
    
    var server = this;
    
    this.getXHR = this.getXMLHttpRequest = function() {
        try{
            return new XMLHttpRequest();
        } catch(e) {
            // not a Mozilla or Konqueror based browser
        };
        try {
            // XXX not sure if it's really better, but perhaps we should use
            // the more verbose way (with version numbers and all) for A-X
            // instantiation?
            return new ActiveXObject('Microsoft.XMLHTTP');
        } catch(e) {
            // not IE either...
        };
        return undefined;
    };

    var Module = this._Module = function(name, code) {
        this.name = name;
        this._code = code;
        eval(code);
    };

    Module.prototype.toString = function() {
        return "[Module '" + this.name + "']";
    };

    Module.prototype.toSource = function() {
        return this._code;
    };

    this.load_async = function(path, callback, errback) {
        /* load data from a path/url

            on successful load <callback> is called with arguments 'status',
            'content', on error <errback>, with the same arguments
        */
        if (!errback) {
            errback = callback;
        };
        var xhr = this.getXHR();
        xhr.open('GET', path, true);
        var handler = function() {
            if (xhr.readyState == 4) {
                if (xhr.status != 200 && xhr.status != 302) {
                    errback(xhr.status, xhr.responseText);
                    return;
                };
                callback(xhr.status, xhr.responseText);
            };
        };
        xhr.onreadystatechange = handler;
        xhr.send('');
    };
    
    this.load_sync = function(path) {
        var xhr = this.getXHR();
        xhr.open('GET', path, false);
        xhr.send('');
        if (xhr.status != 200 && xhr.status != 302) {
            if (global.exception) {
                throw(new exception.HTTPError(xhr.status));
            } else {
                throw(xhr.status);
            };
        };
        return xhr.responseText;
    };
    
    this.import_async = function(name, path, errback) {
        /* import a JS 'module' asynchronous

            'name' is the name the module will be available under, 
            'path' is the path to the object (must be on the same server),
            'errback' is an (optional) error handling function

            NOTE: in a module, only variables attached to 'this' on the module
            root level are exposed
        */
        var handler = function(status, data) {
            var module = new Module(name, data);
            window[name] = module;
        };
        this.load_data(path, handler, errback);
    };

    this.import_sync = function(path) {
        return new Module('<unknown>', this.load_sync(path));
    };
}();
