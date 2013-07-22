/*
    davfs.js - High-level JavaScript WebDAV client implementation
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

    This is a high-level WebDAV interface, part of the 'davlib' JS package. 
    
    For more details, see davclient.js in the same package, or visit
    http://johnnydebris.net.

*/

//-----------------------------------------------------------------------------
// ResourceCache - simple cache to help with the FS
//-----------------------------------------------------------------------------

var global = this;
if (!global.davlib) {
    alert('Error: davclient.js needs to be loaded before davfs.js');
    try {
        var exc = new exception.MissingDependency('davclient.js', 'davfs.js');
    } catch(e) {
        var exc = 'Missing Dependency: davclient.js (from davfs.js)';
    };
    throw(exc);
};

davlib.ResourceCache = function() {
};

davlib.ResourceCache.prototype.initialize = function() {
    this._cache = {};
};

davlib.ResourceCache.prototype.addResource = function(path, resource) {
    this._cache[path] = resource;
};

davlib.ResourceCache.prototype.getResource = function(path) {
    return this._cache[path];
};

davlib.ResourceCache.prototype.invalidate = function(path) {
    delete this._cache[path];
};

//-----------------------------------------------------------------------------
// DavFS - high-level DAV client library
//-----------------------------------------------------------------------------

davlib.DavFS = function() {
    /* High level implementation of a WebDAV client */
};

davlib.DavFS.prototype.initialize = function(host, port, protocol) {
    this._client = new davlib.DavClient();
    this._client.initialize(host, port, protocol);
    this._cache = new davlib.ResourceCache();
    this._cache.initialize();
};

davlib.DavFS.prototype.read = function(path, handler, context) {
    /* get the contents of a file 
    
        when done, handler is called with 2 arguments, the status code
        and the content, optionally in context <context>
    */
    this._client.GET(path, this._wrapHandler(handler,
                        this._prepareArgsRead, context), this);
};

davlib.DavFS.prototype.write = function(path, content, handler, context,
                                        locktoken) {
    /* write the new contents of an existing or new file

        when done handler will be called with one argument,
        the status code of the response, optionally in context
        <context>
    */
    this._client.PUT(path, content, this._wrapHandler(handler, 
                        this._prepareArgsSimpleError, context), this,
                        locktoken);
};

davlib.DavFS.prototype.remove = function(path, handler, context, locktoken) {
    /* remove a file or directory recursively from the fs

        when done handler will be called with one argument,
        the status code of the response, optionally in context
        <context>

        when the status code is 'Multi-Status', the handler will
        get a second argument passed in, which is a parsed tree of
        the multi-status response body
    */
    this._client.DELETE(path, this._wrapHandler(handler, 
                        this._prepareArgsMultiError, context), this,
                        locktoken);
};

davlib.DavFS.prototype.mkDir = function(path, handler, context, locktoken) {
    /* create a dir (collection)
    
        when done, handler is called with 2 arguments, the status code
        and the content, optionally in context <context>
    */
    this._client.MKCOL(path, this._wrapHandler(handler, 
                        this._prepareArgsSimpleError, context), this,
                        locktoken);
};

davlib.DavFS.prototype.copy = function(path, topath, handler, context, 
                                       overwrite, locktoken) {
    /* copy an item (resource or collection) to another location
    
        when done, handler is called with 1 argument, the status code,
        optionally in context <context>
    */
    // XXX not really sure if we should send the lock token for the from or
    // the to path...
    this._client.COPY(path, topath, this._wrapHandler(handler, 
                        this._prepareArgsMultiError, context), this, 
                        overwrite, locktoken);
};

davlib.DavFS.prototype.move = function(path, topath, handler, context,
                                       locktoken) {
    /* move an item (resource or collection) to another location
    
        when done, handler is called with 1 argument, the status code,
        optionally in context <context>
    */
    this._client.MOVE(path, topath, this._wrapHandler(handler, 
                      this._prepareArgsMultiError, context), this,
                      locktoken);
};

davlib.DavFS.prototype.listDir = function(path, handler, context, cached) {
    /* list the contents of a collection
    
        when done, handler is called with 2 arguments, the status code
        and an array with filenames, optionally in context <context>
    */
    if (cached) {
        var item = this._cache.getResource(path);
        // XXX perhaps we should keep items set to null so we know
        // the difference between an item that isn't scanned and one
        // that just has no children
        if (item && item.items.length > 0) {
            var dir = [];
            for (var i=0; i < item.items.length; i++) {
                dir.push(item.items[i].href);
            };
            handler.apply('200', dir);
            return;
        };
    };
    this._client.PROPFIND(path, this._wrapHandler(handler, 
                          this._prepareArgsListDir, context), this, 1);
};

davlib.DavFS.prototype.getProps = function(path, handler, context, cached) {
    /* get the value of one or more properties */
    if (cached) {
        var item = this._cache.getResource(path);
        // XXX perhaps we should keep items set to null so we know
        // the difference between an item that isn't scanned and one
        // that just has no children
        if (item) {
            timer_instance.registerFunction(context, handler, 0, 
                                            null, item.properties);
            return;
        };
    };
    this._client.PROPFIND(path, this._wrapHandler(handler,
                          this._prepareArgsGetProps, context), this, 0);
};

davlib.DavFS.prototype.setProps = function(path, setprops, delprops, 
                                           handler, context, locktoken) {
    this._client.PROPPATCH(path, 
                           this._wrapHandler(handler, 
                                this._prepareArgsMultiError, context), 
                           this, setprops, delprops, locktoken);
};

davlib.DavFS.prototype.lock = function(path, owner, handler, context, 
                                       scope, type, depth, timeout, 
                                       locktoken) {
    /* Lock an item

        when done, handler is called with 2 arguments, the status code
        and an array with filenames, optionally in context <context>

        optional args:
        
        <owner> is a URL that identifies the owner, <type> can currently 
        only be 'write' (according to the DAV specs), <depth> should be 
        either 1 or 'Infinity' (default), timeout is in seconds and
        locktoken is the result of a previous lock (serves to refresh a 
        lock)
    */
    this._client.LOCK(path, owner, 
                      this._wrapHandlerLock(handler,
                            this._prepareArgsMultiError, context),
                      this, scope, depth, timeout, locktoken);
};

davlib.DavFS.prototype.unlock = function(path, locktoken, handler, context) {
    /* Release a lock from an item

        when done, handler is called with 1 argument, the status code,
        optionally in context <context>

        <locktoken> is a lock token returned by a previous DavFS.lock()
        call
    */
    this._client.UNLOCK(path, locktoken,
                        this._wrapHandler(handler,
                            this._prepareArgsMultiError, context),
                        this);
};

// TODO... :\
/*
davlib.DavFS.prototype.isReadable = function(path, handler, context) {};

davlib.DavFS.prototype.isWritable = function(path, handler, context) {};

davlib.DavFS.prototype.isLockable = function(path, handler, context) {};
*/

davlib.DavFS.prototype.isLocked = function(path, handler, context, cached) {
    function sub(error, content) {
        if (!error) {
            var ns = content['DAV:'];
            if (!ns || !ns['lockdiscovery'] ||
                    !ns['lockdiscovery'].documentElement.getElementsByTagName(
                        'activelock').length) {
                content = false;
            } else {
                content = true;
            };
            handler(error, content);
        };
    };
    this.getProps(path, sub, context, cached);
};

davlib.DavFS.prototype._prepareArgsRead = function(status, statusstring, 
                                                   content) {
    var error;
    if (status != '200' && status != '201' && status != '204') {
        error = statusstring;
    };
    return new Array(error, content);
};

davlib.DavFS.prototype._prepareArgsSimpleError = function(status, statusstring, 
                                                          content) {
    var error;
    // ignore weird IE status code 1223...
    if (status != '200' && status != '201' && 
            status != '204' && status != '1223') {
        error = statusstring;
    };
    return new Array(error);
};

davlib.DavFS.prototype._prepareArgsMultiError = function(status, statusstring, 
                                                         content) {
    var error = null;
    if (status == '207') {
        error = this._getErrorsFromMultiStatusTree(content);
        if (!error.length) {
            error = null;
        };
    } else if (status != '200' && status != '201' && status != '204') {
        error = statusstring;
    };
    return new Array(error);
};

davlib.DavFS.prototype._prepareArgsListDir = function(status, statusstring,
                                                      content) {
    var error;
    if (status == '207') {
        error = this._getErrorsFromMultiStatusTree(content);
        if (error.length == 0) {
            error = undefined;
            // some caching tricks, store the current item and also
            // all its children (can't be deeper in the current setup)
            // in the cache, the children don't contain subchildren yet
            // but they do contain properties
            this._cache.addResource(content.href, content);
            for (var i=0; i < content.items.length; i++) {
                var child = content.items[i];
                this._cache.addResource(child.href, child);
            };
            // the caller is interested only in the filenames
            // (we assume ;)
            content = this._getDirFromMultiStatusTree(content);
        };
    } else if (status != '200' && status != '201' && status != '204') {
        error = statusstring;
    };
    return new Array(error, content);
};

davlib.DavFS.prototype._prepareArgsGetProps = function(status, statusstring, 
                                                       content) {
    var error;
    if (status == '207') {
        error = this._getErrorsFromMultiStatusTree(content);
        if (error.length == 0) {
            error = undefined;
            this._cache.addResource(content.href, content);
            content = content.properties;
        };
    } else if (status != '200' && status != '201' && status != '204') {
        error = statusstring;
    };
    return new Array(error, content);
};

davlib.DavFS.prototype._wrapHandler = function(handler, prepareargs, context) {
    /* return the handler wrapped in some class that fixes the context
        and arguments
    */
    function sub(status, statusstring, content) {
        var args = prepareargs.call(this, status, statusstring, 
                                        content);
        handler.apply(context, args);
    };
    return sub;
};

davlib.DavFS.prototype._wrapHandlerLock = function(handler, prepareargs,
                                                   context) {
    /* return the handler wrapped in some class that fixes the context
        and arguments
    */
    function sub(status, statusstring, content) {
        var error;
        if (status == '207') {
            error = this._getErrorsFromMultiStatusTree(content);
            if (error.length == 0) {
                error = undefined;
                this._cache.addResource(content.href, content);
                content = content.properties;
            };
        } else if (status != '200') {
            error = statusstring;
        } else {
            content = content.locktoken;
        };
        handler.apply(context, (new Array(error, content)));
    };
    return sub;
};

davlib.DavFS.prototype._getErrorsFromMultiStatusTree = function(curritem) {
    var errors = new Array();
    var status = curritem.status;
    if (status && status != '200' && status != '204') {
        errors.push(STATUS_CODES[status]);
    };
    for (var i=0; i < curritem.items.length; i++) {
        var itemerrors = this._getErrorsFromMultiStatusTree(
                                    curritem.items[i]);
        if (itemerrors) {
            for (var j=0; j < itemerrors.length; j++) {
                errors.push(itemerrors[j]);
            };
        };
    };
    return errors;
};

davlib.DavFS.prototype._getDirFromMultiStatusTree = function(root) {
    var list = new Array();
    this._cache.addResource(root.href, root);
    for (var i=0; i < root.items.length; i++) {
        var item = root.items[i];
        list.push(item.href);
    };
    list.sort();
    return list;
};

