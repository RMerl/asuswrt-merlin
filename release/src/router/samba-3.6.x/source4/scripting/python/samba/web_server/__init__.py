#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Unix SMB/CIFS implementation.
# Copyright © Jelmer Vernooij <jelmer@samba.org> 2008
#
# Implementation of SWAT that uses WSGI
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#



def render_placeholder(environ, start_response):
    status = '200 OK'
    response_headers = [('Content-type','text/html')]
    start_response(status, response_headers)

    yield "<!doctype html>\n"
    yield "<html>\n"
    yield "  <title>The Samba web service</title>\n"
    yield "</html>\n"

    yield "<body>\n"
    yield "<p>Welcome to this Samba web server.</p>\n"
    yield "<p>This page is a simple placeholder. You probably want to install "
    yield "SWAT. More information can be found "
    yield "<a href='http://wiki.samba.org/index.php/SWAT'>on the wiki</a>.</p>"
    yield "</p>\n"
    yield "</body>\n"
    yield "</html>\n"


__call__ = render_placeholder


if __name__ == '__main__':
    from wsgiref import simple_server
    httpd = simple_server.make_server('localhost', 8090, __call__)
    print "Serving HTTP on port 8090..."
    httpd.serve_forever()
