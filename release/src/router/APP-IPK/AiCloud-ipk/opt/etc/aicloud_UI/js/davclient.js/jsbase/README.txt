JSBase
=======

What is it?
------------

This is a collection of bits of JavaScript helper functionality, to solve 
common problems encountered when programming JavaScript code. The library was
mostly written because I needed certain generic functionality available for
using in different JS projects, but will be valuable for anyone writing JS
code. The strenghts of this library are:

  * everything is written in 'modules', which provide their own namespace and
    are seperately loadable (certain modules do depend on others, but I tried
    to keep this down to a minimum)

  * care has been taken that it's possible to use this library *at any time, 
    on any platform*, by not depending on any variables being available, or 
    isolating the instances where a variable (for instance 'window') is 
    required

  * the library should not clash with other libraries
    
  * no overriding or extending of built-in data types or functionality

  * the library is light-weight, and has no impact on the way the JS is 
    written, although it shows some patterns to use concerning 'OO' programming
    in JS, it does not make the code harder to read or understand for people
    that don't know the library by using or enforcing complex patterns

  * the functionality in this library can, with the exception of few very 
    browser-specific functions, can be used in any JS environment (e.g. the
    unit tests are ran using SpiderMonkey, see 'unit tests' below)

  * the library is simple and self-explanatory (of course this is not an excuse
    for not providing documentation! my excuse for that is lack of time...)

What kind of functionality should I expect?
--------------------------------------------

This library provides solutions to problems in the core language (e.g.
functions to deal more easily with arrays and strings) and somewhat lower level
problems in JS environments (e.g. a cross-platform print() function to write to
stdout or the document in some way). There are some helpers to solve browser
incompatibility problems (e.g. a 'getXMLHttpRequest()' function and
cross-browser event handling) and wrappers for things that are just very badly
implemented in browsers (e.g.  a schedule() function which you can pass a
normal callable and additional arguments to, a wrapper to solve the problem
where 'this' in a method body refers to something else than the object a method
is implemented on and proper exception classes).

More detailed description of the modules
------------------------------------------

The modules, with a short description:

  * array.js

    a set of functions to help with arrays, basically stuff that the JS core 
    should have provided for

  * exception.js

    this provides the Exception base class and some subclasses, the excepsions
    raised by the library use these to allow distinguishing between different
    exceptions more easily, and also they provide a traceback if the platform
    allows that (mainly Mozilla-related platforms)

  * function.js

    this currently provides a single function, that helps fixing the 'this'
    problem, where 'this' inside methods points to something else than the
    object which the methods are defined on (happens e.g. on event handling
    in browsers)

  * misclib.js

    a set of miscellaneous functionality, such as a schedule() method for 
    scheduling function calls (to replace window.setTimeout), cross-browser
    event handling (also fixing memory leaks in IE), a repr() function to
    create string representations of objects (also recursively, if desired),
    a dir() function to view the attributes of objects, and a print() function
    that makes messages visible in different ways, depending on the platform
    you're on (allowing for presenting text to a user both in browsers and 
    command line environments)
 
  * number.js

    number related functions

  * server.js

    cross-browser server interaction, some higher-level 'AJAX'-related 
    functions

  * string.js

    string related functions

  * testing.js

    functionality to help debugging and testing your applications, it both
    harbours a couple of helper functions you can call from your code, as
    functionality to unit-test that code (with py.test integration, see below)
    
Unit testing
-------------

The 'testing.js' module contains some functions that can be used to write very
simple unit tests (see the module for more information for now). Also there's
a 'conftest.py' and some stuff in the 'testing' dir that allows you to run
JavaScript unit tests using the Python 'py.test' unit test runner, part of the
`py lib`_ library.

.. _`py lib`: http://codespeak.net/py

Other files
------------

The other files in the directory are documentation and license information
and such, and unit tests for the package and related files.

Note
----

Note that the library is currently not in a state in which it can be considered
a complete solution: it merely provides me personally the stuff I needed the
period I used it... Hopefully at some point it will be complete enough to use
by others, but right now don't expect it to solve all your general JS problems
yet.

Questions, bug reports, etc.
-----------------------------

If you have questions, bug reports, patches or remarks, please send an email
to johnny@johnnydebris.net.

