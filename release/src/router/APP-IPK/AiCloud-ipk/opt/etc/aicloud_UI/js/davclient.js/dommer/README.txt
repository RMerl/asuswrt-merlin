Dommer
======

What is it?
-----------

Dommer is a DOM level 2 library (currently not complete, not sure if it ever
will either, but it's a usable subset) that is almost entirely compliant with
the DOM standard. This allows DOM developers to write proper XML parsing code
for all browsers that support core JavaScript. DOM for instance properly
supports namespaces, a feature which IE's DOM support lacks, thus allowing
XML to be parsed with proper namespace and prefix handling on that platform.

What's missing?
---------------

Currently a number of features are missing, features marked with a * will most
probably be added in the near future, others are not certain to be implemented
at all.

- CDATA*
- DTD processing
- HTML support
- some more exotic DOM functions such as isSupported()
- (there's probably more I can't remember right now ;)

What's different from the standard?
-----------------------------------

Since there's no proper way to update one property when another is set in IE,
all properties on a node should be considered read-only, they should never be
modified except with dedicated API calls. To allow setting the prefix of a
node without updating e.g. .nodeName, a new method was added to the Node
interface, called 'setPrefix'. If this library is used *solely* on platforms
that *do* support dynamic property updates (__defineGetter__ and
__defineSetter__), you can set the switch WARN_ON_PREFIX to false and set
.prefix the conventional way.

Usage
-----

For an example of how to use the library, see 'example.html'. For API
documentation, see any DOM level 2 reference.

Note that this library depends on version 0.3 of the 'minisax.js' library, 
and version 0.1 of the JSBase lib, both of which can be downloaded (under the
same GPL license) from http://johnnydebris.net/javascript/.

Questions, remarks, bug reports, etc.
-------------------------------------

If you have questions, remarks, bug reports, etc. you can send email to
johnny@johnnydebris.net. More information and newer versions of this library
can be found on http://johnnydebris.net.

Note: this product contains some code written by the Kupu developers (some of
the functionality in helpers.js). Kupu is an HTML editor written in
JavaScript, released under a BSD-style license. See http://kupu.oscom.org for
more information about Kupu and its license.

