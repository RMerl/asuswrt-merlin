/*****************************************************************************
 *
 * Copyright (c) 2004-2007 Guido Wesdorp. All rights reserved.
 *
 * This software is distributed under the terms of the JSBase
 * License. See LICENSE.txt for license text.
 *
 *****************************************************************************/

var global = this;
global.exception = new function() {
    /* Exception handling in a somewhat coherent manner */

    var exception = this;

    this.Exception = function(message) {
        /* base class of exceptions */
        if (message !== undefined) {
            this._initialize('Exception', message);
        };
    };

    this.Exception.prototype._initialize = function(name, message) {
        this.name = name;
        this.message = message;
        var stack = this.stack = exception._createStack();
        this.lineNo = exception._getLineNo(stack);
        this.fileName = exception._getFileName(stack);
    };

    this.Exception.prototype.toString = function() {
        var lineNo = this.lineNo;
        var fileName = this.fileName;
        var stack = this.stack;
        var exc = this.name + ': ' + this.message + '\n';
        if (lineNo || fileName || stack) {
            exc += '\n';
        };
        if (fileName) {
            exc += 'file: ' + fileName;
            if (lineNo) {
                exc += ', ';
            } else {
                exc += '\n';
            };
        };
        if (lineNo) {
            exc += 'line: ' + lineNo + '\n';
        };
        if (stack) {
            exc += '\n';
            var lines = stack.split('\n');
            for (var i=0; i < lines.length; i++) {
                var line = lines[i];
                if (line.charAt(0) == '(') {
                    line = 'function' + line;
                };
                exc += line + '\n';
            };
        };
        return exc;
    };

    this.ValueError = function(message) {
        /* raised on providing invalid values */
        if (message !== undefined) {
            this._initialize('ValueError', message);
        };
    };

    this.ValueError.prototype = new this.Exception;

    this.AssertionError = function(message) {
        /* raised when an assertion fails */
        if (message !== undefined) {
            this._initialize('AssertionError', message);
        };
    };

    this.AssertionError.prototype = new this.Exception;

    // XXX need to define a load of useful exceptions here
    this.NotSupported = function(message) {
        /* raised when a feature is not supported on the running platform */
        if (message !== undefined) {
            this._initialize('NotSupported', message);
        };
    };

    this.NotSupported.prototype = new this.Exception;
    
    this.NotFound = function(message) {
        /* raised when something is not found */
        if (message !== undefined) {
            this._initialize('NotFound', message);
        };
    };

    this.NotFound.prototype = new this.Exception;

    this.HTTPError = function(status) {
        if (status !== undefined) {
            // XXX need to get the message for the error here...
            this._initialize('HTTPError', status);
        };
    };

    this.HTTPError.prototype = new this.Exception;

    this.MissingDependency = function(missing, from) {
        /* raised when some dependency can not be resolved */
        if (missing !== undefined) {
            var message = missing;
            if (from) {
                message += ' (from ' + from + ')';
            };
            this._initialize('MissingDependency', message);
        };
    };

    this.NotFound.prototype = new this.Exception;

    this._createStack = function() {
        /* somewhat nasty trick to get a stack trace in (works only in Moz) */
        var stack = undefined;
        try {notdefined()} catch(e) {stack = e.stack};
        if (stack) {
            stack = stack.split('\n');
            stack.shift();
            stack.shift();
        };
        return stack ? stack.join('\n') : '';
    };

    this._getLineNo = function(stack) {
        /* tries to get the line no in (works only in Moz) */
        if (!stack) {
            return;
        };
        stack = stack.toString().split('\n');
        var chunks = stack[0].split(':');
        var lineno = chunks[chunks.length - 1];
        if (lineno != '0') {
            return lineno;
        };
    };

    this._getFileName = function(stack) {
        /* tries to get the filename in (works only in Moz) */
        if (!stack) {
            return;
        };
        stack = stack.toString().split('\n');
        var chunks = stack[0].split(':');
        var filename = chunks[chunks.length - 2];
        return filename;
    };
}();
