/*
    minisax.js - Simple API for XML (SAX) library for JavaScript
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

    Note:
    
    This file contains some code written by several Kupu developers. Kupu is an 
    HTML editor written in JavaScript, released under a BSD-style license. See
    http://kupu.oscom.org for more information about Kupu and the license.
    
*/

Array.prototype.map = function(func) {
    /* apply 'func' to each element in the array */
    for (var i=0; i < this.length; i++) {
        this[i] = func(this[i]);
    };
};

Array.prototype.reversed = function() {
    var ret = [];
    for (var i = this.length; i > 0; i--) {
        ret.push(this[i - 1]);
    };
    return ret;
};

// JavaScript has a friggin' blink() function, but not for string stripping...
String.prototype.strip = function() {
    var stripspace = /^\s*([\s\S]*?)\s*$/;
    return stripspace.exec(this)[1];
};

String.prototype.entitize = function() {
    var ret = this.replace(/&/g, '&amp;');
    ret = ret.replace(/"/g, '&quot;');
    ret = ret.replace(/</g, '&lt;');
    ret = ret.replace(/>/g, '&gt;');
    return ret;
};

String.prototype.deentitize = function() {
    var ret = this.replace(/&gt;/g, '>');
    ret = ret.replace(/&lt;/g, '<');
    ret = ret.replace(/&quot;/g, '"');
    ret = ret.replace(/&amp;/g, '&');
    return ret;
};

