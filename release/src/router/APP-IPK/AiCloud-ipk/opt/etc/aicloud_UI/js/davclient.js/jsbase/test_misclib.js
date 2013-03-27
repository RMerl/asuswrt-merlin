load('string.js');
load('misclib.js');

function test_repr() {
    var b = true;
    testing.assertEquals(misclib.repr(b), 'true');

    var n = 10;
    testing.assertEquals(misclib.repr(n), '10');

    var s = 'foo';
    testing.assertEquals(misclib.repr(s), '"foo"');

    var s2 = 'foo\\bar';
    testing.assertEquals(misclib.repr(s2), '"foo\\\\bar"');

    var a = [false, 1, '2'];
    testing.assertEquals(misclib.repr(a), '[false, 1, "2"]');

    var d = new Date(1234567);
    testing.assertEquals(misclib.repr(d), '(new Date(1234567))');

    var o = {foo: [1,2,3],
                bar: {foo: true}};
    testing.assertEquals(misclib.repr(o), '{foo: [1, 2, 3], bar: {foo: true}}');

    var f = {};
    f.f = f;
    testing.assertThrows(undefined, misclib.repr, misclib, f, true);
};

function test_safe_repr() {
    var f = {};
    testing.assertEquals(misclib.safe_repr(f), '#0={}');

    f.foo = f;
    testing.assertEquals(misclib.safe_repr(f), '#0={foo: #0#}');

    f.bar = {};
    testing.assertEquals(misclib.safe_repr(f), '#0={foo: #0#, bar: #1={}}');

    f.bar.f = f;
    testing.assertEquals(misclib.safe_repr(f), '#0={foo: #0#, bar: #1={f: #0#}}');

    f.bar = [];
    testing.assertEquals(misclib.safe_repr(f), '#0={foo: #0#, bar: #1=[]}');

    f.bar[0] = f;
    testing.assertEquals(misclib.safe_repr(f), '#0={foo: #0#, bar: #1=[#0#]}');
};
