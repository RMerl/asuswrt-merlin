/*****************************************************************************
 *
 * Copyright (c) 2004-2007 Guido Wesdorp. All rights reserved.
 *
 * This software is distributed under the terms of the JSBase
 * License. See LICENSE.txt for license text.
 *
 *****************************************************************************/

var global = this;
global.testing = new function() {
    var testing = this;

    this.DEBUG = true; // set to false to disable debug messages
    
    this.assert = function(expr, message) {
        /* raise an exception if expr resolves to false */
        if (!testing.DEBUG) {
            return;
        };
        if (!expr) {
            if (!message) {
                message = 'false assertion'
            } else {
                message = '' + message;
            };
            if (global.exception && global.exception.AssertionError) {
                throw(new exception.AssertionError(message));
            } else {
                throw(message);
            };
        };
    };

    this.assertFalse = function(s) {
        if (s) {
            throw(new exception.AssertionError('!! ' + misclib.repr(s)));
        };
    };

    this.assertEquals = function(var1, var2, message) {
        /* assert whether 2 vars have the same value */
        if (var1 != var2 &&
                (!(var1 instanceof Array && var2 instanceof Array) ||
                    misclib.repr(var1) != misclib.repr(var2)) &&
                (!(var1 instanceof Date && var2 instanceof Date) ||
                    var1.getTime() != var2.getTime())) {
            throw(new exception.AssertionError('' + misclib.repr(s1) + ' != ' + 
                                                misclib.repr(s2)));
        };
    };

    this.assertThrows = function(exctype, callable, context) {
        /* assert whether a certain exception is raised */
        // we changed the argument order here, so an explicit check is the
        // least we can do ;)
        if (typeof callable != 'function') {
            var msg = 'wrong argument type for callable';
            if (global.exception) {
                throw(new exception.ValueError(msg));
            } else {
                throw(msg);
            };
        };
        if (!context) {
            context = null;
        };
        var exception_thrown = false;
        // remove the first three args, they're the function's normal args
        var args = [];
        for (var i=3; i < arguments.length; i++) {
            args.push(arguments[i]);
        };
        try {
            callable.apply(context, args);
        } catch(e) {
            // allow catching undefined exceptions too
            if (exctype === undefined) {
            } else if (exctype) {
                var isinstance = false;
                try {
                    if (e instanceof exctype) {
                        isinstance = true;
                    };
                } catch(f) {
                };
                if (!isinstance) {
                    if (exctype.toSource && e.toSource) {
                        exctype = exctype.toSource();
                        e = e.toSource();
                    };
                    if (exctype.toString && e.toString) {
                        exctype = exctype.toString();
                        e = e.toString();
                    };
                    if (e != exctype) {
                        throw(new exception.AssertionError(
                                        'exception ' + e + 
                                        ', while expecting ' + exctype));
                    };
                };
            };
            exception_thrown = true;
        };
        if (!exception_thrown) {
            if (exctype) {
                throw(new exception.AssertionError(
                        "function didn\'t raise exception \'" + 
                        exctype.toString() + "'"));
            } else {
                throw(new exception.AssertionError(
                        "function didn\'t raise exception"));
            };
        };
    };

    this.debug = function(str) {
        /* append a message to the document with a string */
        if (!testing.DEBUG) {
            return;
        };
        try {
            var div = document.createElement('pre');
            div.appendChild(document.createTextNode('' + str));
            div.style.color = 'red';
            div.style.borderWidth = '1px';
            div.style.borderStyle = 'solid';
            div.style.borderColor = 'black';
            document.getElementsByTagName('body')[0].appendChild(div);
        } catch(e) {
            alert(str);
        };
    };
}();

