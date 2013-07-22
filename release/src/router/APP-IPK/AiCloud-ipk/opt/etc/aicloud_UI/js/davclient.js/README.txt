DavClient - a JavaScript WebDAV client library
==============================================

What is it?
-----------

This library allows you to use the WebDAV HTTP extensions from JavaScript code.
WebDAV provides file-system functionality on top of web servers, using this
library you can make use of that for e.g. content-management tools and image
browsers and such.

How do I use it?
----------------

Since this is a rather young project (well, counting the days I've worked on it
at least ;) there is not much documentation, there is an API reference but 
that's not an easy starting point. However, usage isn't very hard::

  function simple_handler(status, statusstr, content) {
    if (status != 200 && status != 202 && status != 204) {
      alert('problem: ' + content);
    } else if (content) {
      alert('content: ' + content);
    };
  };

  var client = new davlib.DavClient();
  client.initialize();
  client.MKCOL('/foo/', simple_handler);
  client.PUT('/foo/bar.txt', 'some content', simple_handler);
  client.GET('/foo/bar.txt', simple_handler);
  client.DELETE('/foo/', simple_handler);

As you can see, for each WebDAV HTTP method there is a method on the DavClient
class provided.

There's also a somewhat more 'high-level' file-system-like API available::

  function errorsonly(error) {
    if (error) {
      alert('error: ' + error);
    };
  };

  function errorsorcontent(error, content) {
    if (error) {
      alert('error: ' + error);
    } else if (content) {
      alert('content: ' + content);
    };
  };

  var fs = new davlib.DavFS();
  fs.initialize();
  fs.mkDir('/foo/', errorsonly);
  fs.write('/foo/bar.txt', 'some content', errorsonly);
  fs.read('/foo/bar.txt', errorsorcontent);
  fs.rmDir('/foo/', errorsonly);

Note that on first sight the APIs don't differ too much, however, when using
the libs you'll find certain details are handled just a bit nicer.

Requirements
------------

This library is tested on Internet Explorer 6 and several versions of Mozilla
and Firefox, however, due to the relatively simple JavaScript it will most 
probably work on all modern browsers (if not, please send me an email!).

This version of the library depends on the 'minisax.js' (version 0.3) and
'dommer' (version 0.4) XML processing libraries, both of which are available on
http://johnnydebris.net/javascript/.

Questions, remarks, etc.
------------------------

If you have questions, remarks, bug reports, patches or whatnot, please send an
email to johnny@johnnydebris.net.

