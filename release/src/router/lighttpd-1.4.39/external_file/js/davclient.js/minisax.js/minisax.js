/*
    minisax.js - Simple API for XML (SAX) library for JavaScript
    Copyright (C) 2004-2007 Guido Wesdorp
    email johnny@johnnydebris.net

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

    $Id: minisax.js,v 1.1.1.1 2011/11/09 06:36:04 sungmin Exp $

*/

function SAXParser() {
    /* Simple SAX library

        Uses a couple of regular expressions to parse XML, supports only
        a subset of XML (no DTDs and CDATA sections, no entities) but it's
        fast and has proper support for namespaces.
    */
};

SAXParser.prototype.initialize = function(xml, handler) {
    /* initialization

        this method *must* be called directly after initialization,
        and can be used afterwards to re-use the parser object for
        parsing a new stream
    */
    this.xml = xml;
    this.handler = handler;
    this.handler.namespaceToPrefix = {};

    this.starttagreg = /\<([^: \t\n]+:)?([a-zA-Z0-9\-_]+)([^\>]*?)(\/?)\>/m;
    this.endtagreg = /\<\/([^: \t\n]+:)?([a-zA-Z0-9\-_]+)[^\>]*\>/m;
    this.attrstringreg = /(([^:=]+:)?[^=]+=\"[^\"]*\")/m;
    this.attrreg = /([^=]*)=\"([^\"]*)\"/m;

    // this is a bit nasty: we need to record a stack of namespace
    // mappings, each level can override existing namespace ids 
    // so we create a new copy of all existing namespaces first, then
    // we can override prefixes on that level downward, popping when
    // moving up a level
    this._namespace_stack = [];

    this._current_nodename_stack = [];
    this._current_namespace_stack = [];
};

SAXParser.prototype.parse = function() {
    /* parses the XML and generates SAX events */
    var xml = this._removeXMLdeclaration(this.xml);
    this.handler.startDocument();
    while (1) {
        var chunk = this._getNextChunk(xml);
        if (chunk == '') {
            break;
        };
        xml = xml.substr(chunk.length);
        if (chunk.charAt(0) == '<') {
            if (chunk.charAt(1) == '/') {
                // end tag
                this.handleEndTag(chunk);
                this._namespace_stack.pop();
            } else if (chunk.charAt(1) == '!') {
                // XXX note that we don't support DTDs and CDATA yet
                chunk = string.deentitize(chunk);
                if (!chunk.indexOf('-->') == chunk.length - 3) {
                    var more = xml.substr(0, xml.indexOf('-->'));
                    xml = xml.substr(more.length);
                    chunk += more;
                };
                chunk = chunk.substr(4, chunk.length - 7);
                this.handler.comment(chunk);
            } else {
                // start tag
                var singleton = false;
                if (chunk.charAt(chunk.length - 2) == '/') {
                    singleton = true;
                };
                this._pushNamespacesToStack();
                this.handleStartTag(chunk, singleton);
                if (singleton) {
                    this._namespace_stack.pop();
                };
            };
        } else {
            chunk = string.deentitize(chunk);
            this.handler.characters(chunk);
        };
    };
    this.handler.endDocument();
};

SAXParser.prototype.handleStartTag = function(tag, is_singleton) {
    /* parse the starttag and send events */
    
    // parse the tag into chunks
    var match = this.starttagreg.exec(tag);
    if (!match) {
        throw('Broken start tag: ' + tag);
    };
    
    // parse the tagname
    var prefix = match[1];
    var nodename = match[2];
    if (prefix) {
        prefix = prefix.substr(0, prefix.length - 1);
    } else {
        prefix = '';
    };
    
    // first split the attributes and check for namespace declarations
    var attrs = this._splitAttributes(match[3]);
    attrs = this._getAndHandleNamespaceDeclarations(attrs);
    
    // now handle the attributes
    var attributes = {};
    for (var i=0; i < attrs.length; i++) {
        this.handleAttribute(attrs[i], attributes);
    };
    
    var namespace = this._namespace_stack[
                    this._namespace_stack.length - 1
                ][prefix];

    this.handler.startElement(namespace, nodename, attributes);
    if (is_singleton) {
        this.handler.endElement(namespace, nodename);
    } else {
        // store the nodename and namespace for validation on close tag
        this._current_nodename_stack.push(nodename);
        this._current_namespace_stack.push(namespace);
    };
};

SAXParser.prototype.handleEndTag = function(tag) {
    /* handle an end tag */
    var match = this.endtagreg.exec(tag);
    if (!match) {
        throw('Broken end tag: ' + tag);
    };
    var prefix = match[1];
    var nodename = match[2];
    if (prefix) {
        prefix = prefix.substr(0, prefix.length - 1);
    } else {
        prefix = '';
    };
    namespace = this._namespace_stack[
                        this._namespace_stack.length - 1
                    ][prefix];

    // validate, if the name or namespace of the end tag do not match
    // the ones of the start tag, throw an exception
    var current_nodename = this._current_nodename_stack.pop();
    var current_namespace = this._current_namespace_stack.pop();
    if (nodename != current_nodename || 
            namespace != current_namespace) {
        var exc = 'Ending ';
        if (namespace != '') {
            exc += namespace + ':';
        };
        exc += nodename + ' doesn\'t match opening ';
        if (current_namespace != '') {
            exc += current_namespace + ':';
        };
        exc += current_nodename;
        throw(exc); 
    }
    this.handler.endElement(namespace, nodename);
};

SAXParser.prototype.handleAttribute = function(attr, attributemapping) {
    /* parse an attribute */
    var match = this.attrreg.exec(attr);
    if (!match) {
        throw('Broken attribute: ' + attr);
    };
    var prefix = '';
    var name = match[1];
    var lname = match[1];
    var value = string.deentitize(match[2]);
    if (name.indexOf(':') > -1) {
        var tuple = name.split(':');
        prefix = tuple[0];
        lname = tuple[1];
    };
    var namespace = '';
    if (prefix == 'xml') {
        namespace = 'http://www.w3.org/XML/1998/namespace';
        if (!this.handler.namespaceToPrefix[namespace]) {
            this.handler.namespaceToPrefix[namespace] = prefix;
        };
    } else if (prefix != '') {
        namespace = this._namespace_stack[
                            this._namespace_stack.length - 1
                        ][prefix];
    };
    // now place the attr in the mapping
    if (!attributemapping[namespace]) {
        attributemapping[namespace] = {};
    };
    attributemapping[namespace][lname] = value;
};

SAXParser.prototype._removeXMLdeclaration = function(xml) {
    /* removes the xml declaration and/or processing instructions */
    var declreg = /\<\?[^>]*\?\>/g;
    xml = xml.replace(declreg, '');
    return xml;
};

SAXParser.prototype._getNextChunk = function(xml) {
    /* get the next chunk 
    
        up to the opening < of the next tag or the < of the current 
    */
    if (xml.charAt(0) == '<') {
        return xml.substr(0, xml.indexOf('>') + 1);
    } else {
        return xml.substr(0, xml.indexOf('<'));
    };
};

SAXParser.prototype._splitAttributes = function(attrstring) {
    /* split the attributes in the end part of an opening tag */
    var attrs = string.strip(attrstring);
    var attrlist = [];
    while (1) {
        var match = this.attrstringreg.exec(attrstring);
        if (!match) {
            break;
        };
        attrlist.push(string.strip(match[1]));
        attrstring = attrstring.replace(match[0], '');
    };
    return attrlist;
};

SAXParser.prototype._getAndHandleNamespaceDeclarations = function(attrarray) {
    /* get namespace declarations (if any) and handle them */
    var leftover = [];
    for (var i=0; i < attrarray.length; i++) {
        var attr = attrarray[i];
        var match = this.attrreg.exec(attr);
        if (!match) {
            throw('Broken attribute: ' + attr);
        };
        if (match[1].indexOf('xmlns') == -1) {
            leftover.push(attr);
            continue;
        };
        var nsname = match[1];
        var value = string.deentitize(match[2]);
        if (nsname.indexOf(':') > -1) {
            nsname = nsname.split(':')[1];
            this._registerNamespace(value, nsname);
        } else {
            this._registerNamespace(value);
        };
    };
    return leftover;
};

SAXParser.prototype._registerNamespace = function(namespace, prefix) {
    /* maintain a namespace to id mapping */
    if (!prefix) {
        prefix = '';
    };
    if (!this.handler.namespaceToPrefix[namespace]) {
        this.handler.namespaceToPrefix[namespace] = prefix;
    };
    this._namespace_stack[this._namespace_stack.length - 1][prefix] = 
                                                            namespace;
};

SAXParser.prototype._pushNamespacesToStack = function() {
    /* maintains a namespace stack */
    var newnss = {};
    for (var prefix in 
            this._namespace_stack[this._namespace_stack.length - 1]) {
        newnss[prefix] = this._namespace_stack[
                                this._namespace_stack.length - 1
                            ][prefix];
    };
    this._namespace_stack.push(newnss);
};

function SAXHandler() {
    /* base-class and 'interface' for SAX handlers

        serves as documentation and base class so subclasses don't need
        to provide all methods themselves, but doesn't do anything
    */
};

SAXHandler.prototype.startDocument = function() {
    /* is called before the tree is parsed */
};

SAXHandler.prototype.startElement = function(namespaceURI, nodeName, 
                                                attributes) {
    /* is called on encountering a new node

        namespace is the namespace of the node (URI, undefined if the node
        is not in a namespace), nodeName is the localName of the node,
        attributes is a mapping from namespace name to a mapping
        {name: value, ...}
    */
};

SAXHandler.prototype.endElement = function(namespaceURI, nodeName) {
    /* is called on leaving a node 
    
        namespace is the namespace of the node (URI, undefined if the node 
        is not defined inside a namespace), nodeName is the localName of 
        the node
    */
};

SAXHandler.prototype.characters = function(chars) {
    /* is called on encountering a textnode

        chars is the node's nodeValue
    */
};

SAXHandler.prototype.comment = function(comment) {
    /* is called when encountering a comment node

        comment is the node's nodeValue
    */
};

SAXHandler.prototype.endDocument = function() {
    /* is called after all nodes were visited */
};

