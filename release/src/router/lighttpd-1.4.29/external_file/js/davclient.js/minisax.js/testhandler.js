/*
    testhandler.js - SAX handler for example and tests of minisax.js
    Copyright (C) 2004-2005 Guido Wesdorp
    email johnny@debris.demon.nl

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

    $Id: testhandler.js,v 1.1.1.1 2011/11/09 06:36:04 sungmin Exp $

*/

function TestHandler() {
    /* generates XML from XML

        test code (and an example, although there's a bit much
        cruft to get the result nice-looking) for the SAX parser
    */

    this.startDocument = function() {
        this.xml = '';
        this.namespaces = {};
        this.lastns = 0;
        this.firstelement = '';
    };
    
    this.startElement = function(namespace, nodename, attributes) {
        var xml = '<';
        if (namespace != '') {
            var nsname;
            if (namespace in this.namespaces) {
                nsname = this.namespaces[namespace];
            } else {
                nsname = this._createNamespaceName();
                this.namespaces[namespace] = nsname;
            };
            if (nsname != 'ns0') {
                xml += nsname + ':';
            };
        };
        xml += nodename;
        for (anamespace in attributes) {
            var attrdict = attributes[anamespace];
            var nsname = null;
            if (anamespace != "") {
                if (anamespace in this.namespaces) {
                    nsname = this.namespaces[anamespace];
                } else {
                    nsname = this._createNamespaceName();
                    this.namespaces[anamespace] = nsname;
                };
            };
            for (var aname in attrdict) {
                xml += ' ';
                if (nsname) {
                    xml += nsname + ':';
                };
                xml += aname + '="' + string.entitize(attrdict[aname]) + '"';
            };
        };
        if (this.firstelement == '') {
            this.firstelement = xml;
        } else {
            this.xml += xml + '>';
        };
    };

    this.endElement = function(namespace, nodename) {
        this.xml += '</';
        if (namespace) {
            this.xml += this.namespaces[namespace] + ':';
        };
        this.xml += nodename + '>';
    };

    this.characters = function(content) {
        this.xml += string.entitize(content);
    };

    this.endDocument = function() {
        var xml = this.firstelement;
        for (namespace in this.namespaces) {
            xml += ' xmlns';
            if (this.namespaces[namespace] != 'ns0') {
                xml += ':' + this.namespaces[namespace];
            };
            xml += '="' + namespace + '"';
        };
        xml += '>' + this.xml;
        this.xml = xml;
    };

    this._createNamespaceName = function() {
        var name = 'ns' + this.lastns;
        this.lastns++;
        return name;
    };
};

TestHandler.prototype = new SAXHandler;


