/*****************************************************************************
 *
 * Copyright (c) 2004-2007 Guido Wesdorp. All rights reserved.
 *
 * This software is distributed under the terms of the jsbase
 * License. See LICENSE.txt for license text.
 *
 *****************************************************************************/

var global = this;
global.func = new function() {
    this.fixContext = function(f, context) {
        /* Return a function wrapped, so that 'this' points to 'context'
            
            Under certain circumstances, methods get called with the 'this' 
            variable pointing to something else than the object the method is
            defined on. In that case, one can use some trick with nested scopes 
            and Function.apply() to get this to refer to the object again. This
            method provides a generic wrapper, that not only fixes 'this'
            but also allows passing additional arguments to handlers.

            Example:

            // create an object with a method that will serve as an event 
            // handler later on
            function Foo() {
                this.bar = 1;
                this.baz = function(event, some_arg) {
                    // if the following would be done when this method would not
                    // get wrapped, 'this' would point to the Event object rather
                    // than the Foo one, resulting in a name lookup problem
                    alert('this.bar: ' + this.bar + ', some_arg: ' + some_arg);
                };
            };

            // create a Foo
            var foo = new Foo();

            // get some element to register some event on
            var el = document.getElementById('some_element');

            // now we're going to register the foo.baz method as an 
            // event handler for the 'onclick' event on the element,
            // but we're wrapping it first, and pass in some argument just
            // for show
            // using the Mozilla style of event registration, see 
            // event.js' registerEventHandler() for a cross-browser way
            el.addEventListener('click', func.fixContext(foo.bar, foo, 2), 
                                    false);

            The method will be called with the following arguments:

            - the object that 'this' would point to when the method would have
                been used unwrapped

            - the additional arguments the fixContext method was called with

            - the arguments the wrapped version was called with

        */
        var orgargs = [];
        for (var i=0; i < arguments.length; i++) {
            if (i == 0 || (context && i == 1)) {
                continue;
            };
            orgargs.push(arguments[i]);
        };
        if (!context) {
            context = window;
        };
        var wrapper = function() {
            var funcargs = [this];
            for (var i=0; i < orgargs.length; i++) {
                funcargs.push(orgargs[i]);
            };
            for (var i=0; i < arguments.length; i++) {
                funcargs.push(arguments[i]);
            };
            return f.apply(context, funcargs);
        };
        return wrapper;
    };
}();
