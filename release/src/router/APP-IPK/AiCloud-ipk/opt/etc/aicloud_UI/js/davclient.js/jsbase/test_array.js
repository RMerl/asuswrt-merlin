load('string.js');
load('array.js');

function test_indexOf() {
    var a1 = ['foo', 'bar', 'baz'];
    testing.assertEquals(array.indexOf(a1, 'foo'), 0);
    testing.assertEquals(array.indexOf(a1, 'qux'), -1);
};

function test_removeDoubles() {
    var a1 = ['foo', 'bar', 'baz'];
    testing.assertEquals(array.removeDoubles(a1), a1);

    var a2 = ['foo', 'bar', 'foo', 'baz', 'bar', 'baz'];
    testing.assertEquals(array.removeDoubles(a2), a1);

    var a3 = ['foo', 'foo', 'foo', 'foo'];
    testing.assertEquals(array.removeDoubles(a3), ['foo']);

    var a4 = [];
    testing.assertEquals(array.removeDoubles(a4), []);
};

function test_map() {
    var a1 = ['foo', 'bar', 'baz'];
    array.map(a1, function(x) {return x.charAt(0)});
    testing.assertEquals(a1, ['f', 'b', 'b']);
};

function test_reversed() {
    var a1 = ['foo', 'bar', 'baz'];
    testing.assertEquals(array.reversed(a1), ['baz', 'bar', 'foo']);
};

function test_iterate() {
    var a = ['foo', 'bar', 'baz'];
    var expected = ['foo', 'bar', 'baz', undefined];
    for (var n=0; n < 2; n++) {
        for (var i=0; i < expected.length; i++) {
            testing.assertEquals(array.iterate(a), expected[i]);
        };
    };
};

function test_iterator() {
    var a = ['foo', 'bar', 'baz'];
    var expected = ['foo', 'bar', 'baz', undefined];
    for (var n=0; n < 2; n++) {
        for (var i=0; i < expected.length; i++) {
            if (i === undefined) {
                testing.assertThrows(array.StopIteration, array.iterate, array,
                                     a);
            } else {
                testing.assertEquals(expected[i], array.iterate(a));
            };
        };
    };
};

