load('string.js');
load('number.js');

function test_toBase() {
    testing.assertEquals(number.toBase(5, 2), '101');
    testing.assertEquals(number.toBase(5, 3), '12');
    testing.assertEquals(number.toBase(255, 16), 'ff');
    testing.assertEquals(number.toBase(0, 16), '0');
    testing.assertEquals(number.toBase(438726438764873, 10), '438726438764873');
};
