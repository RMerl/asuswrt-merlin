
========
lighttpd
========

-------------
a light httpd
-------------

:author: Jan Kneschke
:Date: $Date: 2004/11/03 22:25:54 $
:Revision: $Revision: 1.8 $

:abstract:
  lighttpd a secure, fast, compliant and very flexible web-server
  which has been optimized for high-performance environments. It has a very
  low memory footprint compared to other webservers and takes care of cpu-load.
  Its advanced feature-set (FastCGI, CGI, Auth, Output-Compression,
  URL-Rewriting and many more) make lighttpd the perfect webserver-software
  for every server that is suffering load problems.

:documentation:
  http://wiki.lighttpd.net/

the naming
----------

lighttpd is a __httpd__ which is

- fast as __light__ning and
- __light__ when it comes to memory consumption and system requirements

Features
--------

Network
```````

- IPv4, IPv6

Protocols
`````````

- HTTP/1.0 (http://www.ietf.org/rfc/rfc1945.txt)
- HTTP/1.1 (http://www.ietf.org/rfc/rfc2616.txt)
- HTTPS (provided by openssl)
- CGI/1.1 (http://CGI-Spec.Golux.Com/)
- FastCGI (http://www.fastcgi.com/devkit/doc/fcgi-spec.html)

Advanced Features
`````````````````

- load-balanced FastCGI
  (one webserver distributes requests to multiple PHP-servers via FastCGI)
- custom error pages (for Response-Code 400-599)
- virtual hosts
- directory listings
- streaming CGI and FastCGI
- URL-Rewriting
- HTTP-Redirection
- output-compression with transparent caching

FastCGI-Support
```````````````

- parses the Response-header and completes the HTTP-header accordingly
- Keep-Alive handling based on Content-Length header

PHP-Support
```````````

- same speed as or faster than apache + mod_php4
- handles various PHP bugs in the FastCGI SAPI
- includes a utility to spawn FastCGI processes (necessary for PHP 4.3.x)

Security features
`````````````````

- chroot(), set UID, set GID
- protecting docroot

HTTP/1.1 features
`````````````````

- Ranges (start-end, start-, -end, multiple ranges)
- HTTP/1.0 Keep-Alive + HTTP/1.1 persistent Connections
- methods: GET, HEAD, POST
- Last-Modified + If-Modified handling
- sends Content-Length if possible
- sends Transfer-Encoding: chunk, if Content-Length is not possible
- sends Content-Type
- on-the-fly output compression (deflate, gzip)
- authentication: basic and digest
  (http://www.ietf.org/rfc/rfc2617.txt)

HTTP/1.1 compliance
```````````````````

- Sends 206 for Range Requests
- Sends 304 for If-Modified Requests
- Sends 400 for missing Host on HTTP/1.1 requests
- Sends 400 for broken Request-Line
- Sends 411 for missing Content-Length on POST requests
- Sends 416 for "out-of-range" on Range: Header
- Sends 501 for request-method != (GET|POST|HEAD)
- Sends 505 for protocol != HTTP/1.0 or HTTP/1.1
- Sends Date: on every requests

Intended Audience
-----------------

- Ad-Server Front-Ends ("Banner-Schleuder")
  - delivering small files rapidly
- php-servers under high load
  (load-balancing the php-request over multiple PHP-servers)

Works with
----------

It has been tested to work with

- IE 6.0
- Mozilla 1.x
- Konqueror 3.1
  (for Keep-Alive/Persistent Connections, Accept-Encoding for PHP + gzip)
- wget
  (for Resuming)
- acrobat plugin
  (for multiple ranges)


Works on
--------

lighttpd has been verified to compile and work on

- Linux
- FreeBSD
- NetBSD
- Solaris 8 + 9
- SGI IRIX 6.5

missing for HTTP/1.1 compliance
-------------------------------
- parsing chunked POST request

-----------------
Starting lighttpd
-----------------

As daemon in the background: ::

  $ lighttpd -f <configfile>

or without detaching from the console: ::

  $ lighttpd -D -f <configfile>


