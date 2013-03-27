/*
    tests.js - unit tests for dommer.js
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

    $Id: test_dommer.js,v 1.1.1.1 2011/11/09 06:36:04 sungmin Exp $

*/

load('../minisax.js/minisax.js');
load('../jsbase/string.js');
load('../jsbase/array.js');
load('dommer.js');

var global = this;
function setup() {
    global.dom = new dommer.DOM();
    global.doc = global.dom.createDocument();
    global.docfrag = global.doc;
};

function tearDown() {
    global.doc = null;
    delete global.dom;
};

function test_property_access() {
    var doc = global.doc;
    var el = doc.createElement('foo');
    testing.assertThrows(undefined, function() {el.nodeName = 'foo'});
};

function test_createElement() {
    var doc = global.doc;
    
    var el1 = doc.createElement('foo');
    testing.assertEquals(el1.nodeType, 1);
    testing.assertEquals(el1.nodeName.toLowerCase(), 'foo');
    testing.assertEquals(el1.namespaceURI, null);
    testing.assertEquals(el1.ownerDocument, doc);

    var el2 = doc.createElementNS('foo:', 'bar');
    testing.assertEquals(el2.nodeType, 1);
    testing.assertEquals(el2.nodeName.toLowerCase(), 'bar');
    testing.assertEquals(el2.namespaceURI, 'foo:');
    testing.assertEquals(el2.ownerDocument, doc);

    var attr1 = doc.createAttribute('foo');
    testing.assertEquals(attr1.nodeType, 2);
    testing.assertEquals(attr1.nodeName, 'foo');
    testing.assertEquals(attr1.namespaceURI, null);
};

function test_appendChild() {
    var doc = global.doc;
    var docfrag = global.docfrag;

    var el = doc.createElement('foo');
    docfrag.appendChild(el);
    if (docfrag == doc) {
        // not for browser dom
        testing.assertEquals(doc.documentElement, el);
    };
    testing.assertEquals(el.ownerDocument, doc);
    testing.assertEquals(el.parentNode, docfrag);
    testing.assertEquals(el.firstChild, null);
    testing.assertEquals(el.lastChild, null);
    testing.assertEquals(el.previousSibling, null);
    testing.assertEquals(el.nextSibling, null);
    testing.assertEquals(docfrag.childNodes[0], el);

    var el2 = doc.createElement('foo');
    if (doc == docfrag) {
        testing.assertThrows(undefined, doc.appendChild, doc, el2);
    };
    el.appendChild(el2);
    testing.assertEquals(el2.parentNode, el);
    testing.assertEquals(el.childNodes[0], el2);
    testing.assertEquals(el.firstChild, el2);
    testing.assertEquals(el.lastChild, el2);
    testing.assertEquals(el2.previousSibling, null);
    testing.assertEquals(el2.nextSibling, null);

    var el3 = doc.createElement('foo');
    el.appendChild(el3);
    testing.assertEquals(el3.parentNode, el);
    testing.assertEquals(el.childNodes[1], el3);
    testing.assertEquals(el.firstChild, el2);
    testing.assertEquals(el.lastChild, el3);
    testing.assertEquals(el2.previousSibling, null);
    testing.assertEquals(el2.nextSibling, el3);
    testing.assertEquals(el3.previousSibling, el2);
    testing.assertEquals(el3.nextSibling, null);
};

function test_removeChild() {
    var doc = global.doc;
    var docfrag = global.docfrag;

    var el = doc.createElement('foo');
    docfrag.appendChild(el);

    var el2 = doc.createElement('bar');
    testing.assertEquals(el2.parentNode, null);
    testing.assertEquals(el.firstChild, null);
    testing.assertEquals(el.lastChild, null);
    if (doc == docfrag) {
        testing.assertThrows(dommer.DOMException, el.removeChild, el, el2);
    };
    
    el.appendChild(el2);
    testing.assertEquals(el2.parentNode, el);
    testing.assertEquals(el.firstChild, el2);
    testing.assertEquals(el.lastChild, el2);
    testing.assertEquals(el.childNodes[0], el2);

    el3 = doc.createElement('baz');
    el.appendChild(el3);
    testing.assertEquals(el3.parentNode, el);
    testing.assertEquals(el.firstChild, el2);
    testing.assertEquals(el.lastChild, el3);
    testing.assertEquals(el2.nextSibling, el3);
    testing.assertEquals(el3.previousSibling, el2);

    el.removeChild(el2);
    testing.assertEquals(el2.parentNode, null);
    testing.assertEquals(el.firstChild, el3);
    testing.assertEquals(el.lastChild, el3);
    testing.assertEquals(el.childNodes[0], el3);
    testing.assertEquals(el2.nextSibling, null);
    testing.assertEquals(el3.previousSibling, null);
};

function test_replaceChild() {
    var doc = global.doc;
    var docfrag = global.docfrag;

    var el = doc.createElement('foo');
    docfrag.appendChild(el);

    var child1 = doc.createElement('bar');
    var child2 = doc.createElement('baz');
    var child3 = doc.createElement('qux');
    el.appendChild(child1);

    testing.assertEquals(el.childNodes.length, 1);
    testing.assertEquals(el.childNodes[0], child1);
    testing.assertEquals(child1.parentNode, el);

    el.appendChild(child3);
    testing.assertEquals(el.firstChild, child1);
    testing.assertEquals(el.lastChild, child3);
    testing.assertEquals(child1.nextSibling, child3);
    testing.assertEquals(child3.previousSibling, child1);

    el.replaceChild(child2, child1);
    testing.assertEquals(child1.parentNode, null);
    testing.assertEquals(el.childNodes.length, 2);
    testing.assertEquals(el.firstChild, child2);
    testing.assertEquals(el.lastChild, child3);
    testing.assertEquals(child2.parentNode, el);
    testing.assertEquals(child1.nextSibling, null);
    testing.assertEquals(child2.nextSibling, child3);
    testing.assertEquals(child3.previousSibling, child2);
};

function test_getAttribute() {
    var doc = global.doc;

    var el = doc.createElement('foo');
    el.setAttribute('bar', 'baz');
    testing.assertFalse(el.getAttribute('foo'));
    var attr = el.getAttribute('bar');
    testing.assertEquals(el.getAttribute('bar'), 'baz');
    var attr = el.getAttributeNode('bar');
    testing.assertEquals(attr.nodeValue, 'baz');
    testing.assertEquals(attr.nodeType, 2);
    testing.assertEquals(attr.namespaceURI, null);
    testing.assertEquals(attr.ownerDocument, doc);
    testing.assertEquals(attr.parentNode, null);
    testing.assertEquals(attr.ownerElement, el);

    el.setAttributeNS('foo:', 'bar', 'baz');
    var attr = el.getAttributeNodeNS('foo:', 'bar');
    testing.assertEquals(attr.nodeValue, 'baz');
    testing.assertEquals(attr.namespaceURI, 'foo:');
};

function test_setAttribute() {
    var doc = global.doc;
    
    var el = doc.createElement('foo');
    testing.assertThrows(undefined, el.setAttribute, el, 'foo&bar');
};

function test_toXML() {
    var doc = global.doc;
    
    var foo = doc.createElement('foo');
    testing.assertEquals(foo.toXML(), '<foo />');
    
    foo.setAttribute('bar', 'baz');
    testing.assertEquals(foo.getAttributeNode('bar').toXML(), 'bar="baz"');
    testing.assertEquals(foo.toXML(), '<foo bar="baz" />');
    
    var parent = doc.createElement('parent');
    parent.appendChild(foo);
    testing.assertEquals(parent.toXML(), '<parent><foo bar="baz" /></parent>');
    
    doc.appendChild(parent);
    testing.assertEquals(doc.toXML(), '<parent><foo bar="baz" /></parent>');
    // using DOM.toXML() adds an XML declaration
    testing.assertEquals(global.dom.toXML(global.doc), 
            '<?xml version="1.0"?>\n<parent><foo bar="baz" /></parent>');

    var elwithns = doc.createElementNS('foo:', 'bar');
    parent.appendChild(elwithns);
    testing.assertEquals(elwithns.toXML(), '<bar xmlns="foo:" />');
    testing.assertEquals(doc.toXML(), '<parent><foo bar="baz" />' +
                                    '<bar xmlns="foo:" /></parent>');

    var el1 = doc.createElementNS('foo:', 'bar');
    el1.setPrefix('foo');
    var el2 = doc.createElementNS('foo:', 'baz');
    el2.setPrefix('foo');
    el1.appendChild(el2);
    testing.assertEquals(el1.toXML(),
        '<foo:bar xmlns:foo="foo:"><foo:baz /></foo:bar>');

    var el1 = doc.createElementNS('foo:', 'bar');
    el1.setPrefix('foo');
    var el2 = doc.createElementNS('baz:', 'qux');
    el2.setPrefix('foo');
    el1.appendChild(el2);
    testing.assertEquals(el1.toXML(), 
        '<foo:bar xmlns:foo="foo:"><foo:qux xmlns:foo="baz:" /></foo:bar>');

    var el1 = doc.createElementNS('foo:', 'foo:bar');
    el1.setAttributeNS('baz:', 'baz:qux', 'quux');
    testing.assertEquals(el1.toXML(), 
            '<foo:bar baz:qux="quux" xmlns:foo="foo:" xmlns:baz="baz:" />');
};

function test_namedNodeMap() {
    var nodemap = new dommer.NamedNodeMap();

    var nodes = {};
    for (var i=0; i < 10; i++) {
        var node = global.doc.createElement('node-' + i);
        nodes[i] = node;
        nodemap.setNamedItem(node);
    };

    testing.assertEquals(nodemap.length, 10);
    testing.assertEquals(nodemap[0].nodeName.toLowerCase(), 'node-0');
    testing.assertEquals(nodemap[0], nodemap.getNamedItem('node-0'));
    testing.assertEquals(nodemap[nodemap.length - 1], 
                         nodemap.getNamedItem('node-9'));

    nodemap.removeNamedItem(nodemap[4]);
    testing.assertEquals(nodemap.length, 9);
    testing.assertEquals(nodemap[3], nodes[3]);
    testing.assertEquals(nodemap[4], nodes[5]);
    testing.assertEquals(nodemap[8], nodes[9]);
};

function test_cloneNode() {
    var doc = global.doc;

    var foo = doc.createElement('foo');
    foo.setAttribute('bar', 'bar');
    var baz = doc.createElement('baz');
    baz.appendChild(foo);
    
    var clone = baz.cloneNode(false);
    testing.assertEquals(clone.nodeName.toLowerCase(), 'baz');
    testing.assertFalse(clone.hasChildNodes());
    
    var clone = baz.cloneNode(true);
    testing.assertEquals(clone.nodeName.toLowerCase(), 'baz');
    testing.assert(clone.hasChildNodes());
    testing.assertEquals(clone.childNodes.length, 1);
    testing.assertEquals(clone.childNodes[0].nodeName.toLowerCase(), 'foo');
    testing.assertEquals(clone.childNodes[0].attributes.length, 1);
    testing.assertEquals(
        clone.childNodes[0].attributes[0].nodeName.toLowerCase(), 'bar');
    testing.assertEquals(clone.childNodes[0].getAttribute('bar'), 'bar');
};

function test_parseXML() {
    var dom = new dommer.DOM();

    var xml1 = '<foo />';
    var doc = dom.parseXML(xml1);
    testing.assertEquals(doc.documentElement.nodeName, 'foo');
    testing.assertEquals(doc.childNodes.length, 1);
    testing.assertEquals(doc.documentElement.childNodes.length, 0);

    var xml2 = '<foo><bar /></foo>';
    var doc = dom.parseXML(xml2);
    testing.assertEquals(doc.documentElement.nodeName, 'foo');
    testing.assertEquals(doc.documentElement.childNodes.length, 1);
    testing.assertEquals(doc.documentElement.childNodes[0].nodeName, 'bar');

    var xml3 = '<foo><bar baz="baz" /></foo>';
    var doc = dom.parseXML(xml3);
    var bar = doc.documentElement.childNodes[0];
    testing.assertEquals(bar.attributes.length, 1);
    testing.assertEquals(bar.attributes[0].nodeName, 'baz');
    testing.assertEquals(bar.attributes[0].nodeValue, 'baz');

    var xml4 = '<foo xmlns="bar:" />';
    var doc = dom.parseXML(xml4);
    testing.assertEquals(doc.documentElement.namespaceURI, 'bar:');
    testing.assertEquals(doc.documentElement.prefix, null);

    var xml5 = '<foo:bar xmlns:foo="baz:" />';
    var doc = dom.parseXML(xml5);
    testing.assertEquals(doc.documentElement.nodeName, 'foo:bar'); 
    testing.assertEquals(doc.documentElement.localName, 'bar');
    testing.assertEquals(doc.documentElement.prefix, 'foo');
    testing.assertEquals(doc.documentElement.namespaceURI, 'baz:');

    var xml6 = '<foo>bar</foo>';
    var doc = dom.parseXML(xml6);
    testing.assertEquals(doc.documentElement.childNodes.length, 1);
    testing.assertEquals(doc.documentElement.firstChild.nodeType, 3);
    testing.assertEquals(doc.documentElement.firstChild.nodeValue, 'bar');

    var xml7 = '<foo xmlns="foo:" xmlns:bar="bar:" bar:baz="qux" />';
    var doc = dom.parseXML(xml7);
    testing.assertEquals(doc.documentElement.attributes.length, 1);
    testing.assertEquals(doc.documentElement.attributes[0].nodeName,
                         'bar:baz');
    testing.assertEquals(doc.documentElement.attributes[0].localName, 'baz');
    testing.assertEquals(doc.documentElement.attributes[0].nodeValue, 'qux');
    testing.assertEquals(doc.documentElement.attributes[0].namespaceURI,
                         'bar:');
    testing.assertEquals(doc.documentElement.attributes[0].prefix, 'bar');

    var xml8 = '<foo:bar xmlns:foo="foo:"><foo:baz xmlns:foo="bar:" />' +
               '</foo:bar>';
    var doc = dom.parseXML(xml8);
    testing.assertEquals(doc.toXML(), xml8);

    var xml9 = '<foo:bar xmlns:foo="foo:"><bar:baz xmlns:bar="foo:" />' +
               '</foo:bar>';
    var expected = '<foo:bar xmlns:foo="foo:"><foo:baz /></foo:bar>';
    var doc = dom.parseXML(xml9);
    testing.assertEquals(doc.toXML(), expected);
};

