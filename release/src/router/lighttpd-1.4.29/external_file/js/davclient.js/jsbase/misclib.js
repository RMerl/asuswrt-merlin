/*****************************************************************************
 *
 * Copyright (c) 2004-2007 Guido Wesdorp. All rights reserved.
 *
 * This software is distributed under the terms of the JSBase
 * License. See LICENSE.txt for license text.
 *
 *****************************************************************************/

var global = this;
global.misclib = new function() {
    /* This lib mostly contains fixes for common problems in JS, additions to
        the core global functions and wrappers for fixing browser issues.

        See the comments in the code for further explanations.
    */

    // reference to the lib to reach it from the functions/classes
    var misclib = this;

    var Timer = function() {
        /* Wrapper around window.setTimeout, to solve number of problems:
        
            * setTimeout gets a string rather than a function reference as an
                argument

            * setTimeout can not handle arguments to the callable properly, 
                since it's evaluated in the window namespace, not in the current
                scope

            Usage:

                // call the global 'schedule' function to register a function
                // called 'foo' on the current (imaginary) object, passing in
                // a variable called 'bar' as an argument, so it gets called 
                // after 1000 msecs
                window.misclib.schedule(this, this.foo,  1000, bar);

            Note that if you don't care about 'this' inside the function 
            you register, you can just pass in a ref to the 'window' object
            as the first argument. No lookup is done in the namespace, the
            first argument is only used as the 'this' variable inside the
            function.
        */
        this.lastid = 0;
        this.functions = {};
        
        this.registerCallable = function(object, func, timeout) {
            /* register a function to be called with a timeout

                args: 
                    object - the context in which to call the function ('this')
                    func - the function
                    timeout - timeout in millisecs
                    
                all other args will be passed 1:1 to the function when called
            */
            var args = new Array();
            for (var i=3; i < arguments.length; i++) {
                args.push(arguments[i]);
            }
            var id = this._createUniqueId();
            this.functions[id] = new Array(object, func, args);
            // setTimeout will be called in the module namespace, where the 
            // timer_instance variable also resides (see below)
            setTimeout("window.misclib.timer_instance." +
                        "_handleFunction(" + id + ")", timeout);
        };

        this._handleFunction = function(id) {
            /* private method that does the actual function call */
            var obj = this.functions[id][0];
            var func = this.functions[id][1];
            var args = this.functions[id][2];
            this.functions[id] = null;
            func.apply(obj, args);
        };

        this._createUniqueId = function() {
            /* create a unique id to store the function by */
            while (this.lastid in this.functions && 
                        this.functions[this.lastid]) {
                this.lastid++;
                if (this.lastid > 100000) {
                    this.lastid = 0;
                }
            }
            return this.lastid;
        };
    };

    // create a timer instance in the module namespace
    var timer_instance = this.timer_instance = new Timer();

    // this is the function that should be called to register callables
    this.schedule = function() {
            timer_instance.registerCallable.apply(timer_instance, arguments)};

    // cross-browser event registration
    var events_supported = true;
    try {
        window;
    } catch(e) {
        events_supported = false;
    };
    
    if (events_supported) {
        // first some privates
        
        // a registry for events so they can be unregistered on unload of the 
        // body
        var event_registry = {};

        // an iterator for unique ids
        var currid = 0;

        // just to make some guy on irc happy, store references to 
        // browser-specific wrappers so we don't have to find out the type of
        // browser on every registration (yeah, yeah, i admit it was quite a 
        // good tip... ;)
        var reg_handler = null;
        var unreg_handler = null;
        if (window.addEventListener) {
            reg_handler = function(element, eventname, handler) {
                // XXX not sure if anyone ever uses the last argument...
                element.addEventListener(eventname, handler, false);
            };
            unreg_handler = function(element, eventname, handler) {
                element.removeEventListener(eventname, handler, false);
            };
        } else if (window.attachEvent) {
            reg_handler = function(element, eventname, handler) {
                element.attachEvent('on' + eventname, handler);
            };
            dereg_handler = function(element, eventname, handler) {
                element.detachEvent('on' + eventname, handler);
            };
        } else {
            reg_handler = function(element, eventname, handler) {
                var message = 'Event registration not supported or not ' +
                                'understood on this platform';
                if (global.exception) {
                    throw(new exception.NotSupported(message));
                } else {
                    throw(message);
                };
            };
        };

        this.addEventHandler = function(element, eventname, handler, context) {
            /* Method to register an event handler

                Works in standards compliant browsers and IE, and solves a
                number of different problems. Most obviously it makes that 
                there's only a single function to use and memorize, but also 
                it makes that 'this' inside the function points to the context 
                (usually you'll want to pass in the object the method is 
                defined on), and it fixes memory leaks (IE is infamous for 
                leaking memory, which can lead to problems if you register a 
                lot of event handlers, especially since the memory leakage 
                doesn't disappear on page reloads, see e.g. 
                http://www.bazon.net/mishoo/articles.epl?art_id=824).
            
                Arguments:
                
                    * element - the object to register the event on
                    
                    * eventname - a string describing the event (Mozilla style, 
                        so without the 'on')
                    
                    * handler - a reference to the function to be called when 
                        the event occurs

                    * context - the 'this' variable inside the function

                The arguments passed to the handler:

                    * event - a reference to the event fired

                    * all arguments passed in to this function besides the ones
                        described
                        
            */
            var args = new Array();
            for (var i=4; i < arguments.length; i++) {
                args.push(arguments[i]);
            };
            var wrapper = function(event) {
                args = [event].concat(args);
                handler.apply(context, args)
                args.shift();
            };
            currid++;
            event_registry[currid] = [element, eventname, wrapper];
            reg_handler(element, eventname, wrapper);
            // return the wrapper so the event can be unregistered later on
            return currid;
        };

        this.removeEventHandler = function(regid) {
            /* method to remove an event handler for both IE and Mozilla 
            
                pass in the id returned by addEventHandler
            */
            // remove the entry from the event_registry
            var args = event_registry[regid];
            if (!args) {
                var message = 'removeEventListener called with ' +
                                'unregistered id';
                if (global.exception) {
                    throw((new exception.NotFound(message)));
                } else {
                    throw(message);
                };
            };
            delete event_registry[regid];
            unreg_handler(args[0], args[1], args[2]);
        };

        this.removeAllEventHandlers = function() {
            for (var id in event_registry) {
                try {
                    misclib.removeEventHandler(id);
                } catch(e) {
                    // element must have been discarded or something...
                };
            };
        };
    };

    // string representation of objects
    this.safe_repr = function(o) {
        var visited = {}; 
        var ret = misclib._repr_helper(o, visited);
        delete visited;
        return ret;
    };

    this.repr = function(o, dontfallback) {
        try {
            return misclib._repr_helper(o);
        } catch(e) {
            // when something fails here, we assume it's because of recursion
            // and fall back to safe_repr()
            if (dontfallback) {
                throw(e);
            };
            return misclib.safe_repr(o);
        };
    };

    this._repr_helper = function(o, visited) {
        var newid = 0;
        if (visited) {
            // XXX oomph... :|
            for (var attr in visited) {
                if (visited[attr] === o) {
                    return '#' + attr + '#';
                };
                newid++;
            };
        };
        var ret = 'unknown?';
        var type = typeof o;
        switch (type) {
            case 'undefined':
                ret = 'undefined'
                break;
            case 'string':
                ret = '"' + string.escape(o) + '"';
                break;
            case 'object':
                if (o instanceof Array) {
                    if (visited) {
                        visited[newid] = o;
                    };
                    var r = [];
                    for (var i=0; i < o.length; i++) {
                        var newo = o[i];
                        r.push(misclib._repr_helper(newo, visited));
                    };
                    ret = ''
                    if (visited) {
                        ret += '#' + newid + '=';
                    };
                    ret += '[' + r.join(', ') + ']';
                } else if (o instanceof Date) {
                    ret = '(new Date(' + o.valueOf() + '))';
                } else {
                    if (visited) {
                        visited[newid] = o;
                    };
                    var r = [];
                    for (var attr in o) {
                        var newo = o[attr];
                        r.push(attr + ': ' + misclib._repr_helper(newo, 
                                                                visited));
                    };
                    ret = '';
                    if (visited) {
                        ret += '#' + newid + '=';
                    };
                    ret += '{' + r.join(', ') + '}';
                };
                break;
            default:
                ret = o.toString();
                break;
        };
        return ret;
    };

    this.dir = function(obj) {
        /* list all the attributes of an object */
        var ret = [];
        for (var attr in obj) {
            ret.push(attr);
        };
        return ret;
    };

    this.print = function(message, win) {
        /* write output in a way that the user can see it

            creates a div in browsers, prints to stdout in spidermonkey

            this is here not only as a convenient way to print output, but also
            so that it's possible to override, that way customizing prints
            from code
        */
        if (win === undefined) {
            win = window;
        };
        message = '' + message;
        var p = null;
        try {
          win.foo;
        } catch(e) {
          // no window, so we guess we're inside some JS console, assuming it
          // has a print() function... if there are situations in which this
          // doesn't suffice, please mail me (johnny@johnnydebris.net)
          print(message);
          return;
        };
        var div = document.createElement('div');
        div.className = 'printed';
        div.appendChild(document.createTextNode(message));
        document.getElementsByTagName('body')[0].appendChild(div);
    };
}();
