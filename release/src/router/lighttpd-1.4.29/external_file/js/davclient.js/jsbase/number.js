/*****************************************************************************
 *
 * Copyright (c) 2004-2007 Guido Wesdorp. All rights reserved.
 *
 * This software is distributed under the terms of the JSBase
 * License. See LICENSE.txt for license text.
 *
 *****************************************************************************/

var global = this;
global.number = new function() {
    this.toBase = function(number, base, padwidth, chars) {
        /* convert number to a base <base> number (string) */
        // XXX really not sure whether this algorithm is the best, there will
        // most probably be more optimal ways to achieve this...
        if (!chars) {
            chars = '0123456789abcdefghijklmnopqrstuvwxyz';
        };
        if (base > chars.length) {
            throw('not enough formatting characters for this base');
        };
        var ret = '';
        var i = 0;
        while (number) {
            var curr = Math.pow(base, i);
            var next = curr * base;
            var index = (number % next) / curr;
            ret = chars.charAt(index) + ret;
            number -= (index * curr);
            i++;
        };
        if (padwidth) {
            var add = (padwidth - ret.length);
            for (var i=0; i < add; i++) {
                ret = chars[0] + ret;
            };
        };
        if (ret == '') {
            ret = '0';
        };
        return ret;
    };
}();
