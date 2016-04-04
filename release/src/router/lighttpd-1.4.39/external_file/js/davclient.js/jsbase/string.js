/*****************************************************************************
 *
 * Copyright (c) 2004-2007 Guido Wesdorp. All rights reserved.
 *
 * This software is distributed under the terms of the JSBase
 * License. See LICENSE.txt for license text.
 *
 *****************************************************************************/

/* Additions to the core String API */

var global = this;
global.string = new function() {
    var string = this;

    this.strip = function(s) {
        /* returns a string with all leading and trailing whitespace removed */
        var stripspace = /^\s*([\s\S]*?)\s*$/;
        return stripspace.exec(s)[1];
    };

    this.reduceWhitespace = function(s) {
        /* returns a string in which all whitespace is reduced 
            to a single, plain space */
        s = s.replace(/\r/g, ' ');
        s = s.replace(/\t/g, ' ');
        s = s.replace(/\n/g, ' ');
        while (s.indexOf('  ') > -1) {
            s = s.replace('  ', ' ');
        };
        return s;
    };

    this.entitize = function(s) {
        /* replace all standard XML entities */
        var ret = s.replace(/&/g, '&amp;');
        ret = ret.replace(/"/g, '&quot;');
        //ret = ret.replace(/'/g, '&apos;');
        ret = ret.replace(/</g, '&lt;');
        ret = ret.replace(/>/g, '&gt;');
        return ret;
    };

    this.deentitize = function(s) {
        /* convert all standard XML entities to the corresponding characters */
        var ret = s.replace(/&gt;/g, '>');
        ret = ret.replace(/&lt;/g, '<');
        //ret = ret.replace(/&apos;/g, "'");
        ret = ret.replace(/&quot;/g, '"');
        ret = ret.replace(/&amp;/g, '&');
        return ret;
    };

    this.urldecode = function(s) {
        /* decode an URL-encoded string 
        
            reverts the effect of calling 'escape' on a string (see 
            'String.urlencode' below)
        */
        var reg = /%([a-fA-F0-9]{2})/g;
        var str = s;
        while (true) {
            var match = reg.exec(str);
            if (!match || !match.length) {
                break;
            };
            var repl = new RegExp(match[0], 'g');
            str = str.replace(repl, 
                    String.fromCharCode(parseInt(match[1], 16)));
        };
        // XXX should we indeed replace these?
        str = str.replace(/\+/g, ' ');
        return str;
    };

    this.urlencode = function(s) {
        /* wrapper around the 'escape' core function

            provided for consistency, since I also have a string.urldecode()
            defined
        */
        return escape(s);
    };

    this.escape = function(s) {
        /* escapes quotes and special chars (\n, \a, \r, \t, etc.) 
        
            adds double slashes
        */
        // XXX any more that need escaping?
        s = s.replace(/\\/g, '\\\\');
        s = s.replace(/\n/g, '\\\n');
        s = s.replace(/\r/g, '\\\r');
        s = s.replace(/\t/g, '\\\t');
        s = s.replace(/'/g, "\\'");
        s = s.replace(/"/g, '\\"');
        return s;
    };

    this.unescape = function(s) {
        /* remove double slashes */
        s = s.replace(/\\\n/g, '\n');
        s = s.replace(/\\\r/g, '\r');
        s = s.replace(/\\\t/g, '\t');
        s = s.replace(/\\'/g, '\'');
        s = s.replace(/\\"/g, '"');
        s = s.replace(/\\\\/g, '\\');
        return s;
    };

    this.centerTruncate = function(s, maxlength) {
        if (s.length <= maxlength) {
            return s;
        };
        var chunklength = maxlength / 2 - 3;
        var start = s.substr(0, chunklength);
        var end = s.substr(s.length - chunklength);
        return start + ' ... ' + end;
    };

    this.startsWith = function(s, start) {
        return s.substr(0, start.length) == start;
    };

    this.endsWith = function(s, end) {
        return s.substr(s.length - end.length) == end;
    };
    
    this.format = function(s, indent, maxwidth) {
        /* perform simple formatting on the string */
        if (indent.length > maxwidth) {
            throw('Size of indent must be smaller than maxwidth');
        };
        s = string.reduceWhitespace(s);
        var words = s.split(' ');
        var lines = [];
        while (words.length) {
            var currline = indent;
            while (1) {
                var word = words.shift();
                if (!word || 
                        (currline.length > indent.length && 
                            currline.length + word.length > maxwidth)) {
                    break;
                };
                currline += word + ' ';
            };
            lines.push(currline);
        };
        return lines.join('\r\n');
    };
}();
