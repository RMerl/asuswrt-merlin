// don't forget to set the proper hostname
var fs = new davlib.DavFS();
fs.initialize();

function writeToDiv(line, emphasize) {
    var div = document.getElementById('testdiv');
    var textnode = document.createTextNode(line);
    var newdiv = document.createElement('div');
    newdiv.appendChild(textnode);
    if (emphasize) {
        newdiv.style.color = 'red';
    } else {
        newdiv.style.color = 'blue';
    };
    div.appendChild(newdiv);
};

function assert(statement, debugprint) {
    if (!statement) {
        writeToDiv('FAILURE: ' + debugprint, 1);
        throw(new exceptions.AssertionError('failure ' + debugprint));
    } else {
        writeToDiv('success');
    };
};

// since the lib is async I wrote the functions in the order
// they are executed to give a bit of an overview
function runTests() {
    testMkDir();
};

function testMkDir() {
    function handler(error) {
        assert(!error, 'MkDir test failed, expected no error, got ' + error);
        testWrite();
    };
    writeToDiv('Going to create dir /davtests/foodir/:');
    fs.mkDir('/davtests/foodir/', handler);
};

function testWrite() {
    function handler(error) {
        assert(!error, 'Write test failed, expected no error, got ' + error);
        testCopy();
    };
    writeToDiv('Going to write to file /davtests/foodir/foo:');
    fs.write('/davtests/foodir/foo', 'foo!', handler);
};

function testCopy() {
    function handler(error) {
        assert(!error, 'Copy test failed, expected no error, got ' + error);
        testListDir();
    };
    writeToDiv('Going to copy file /davtests/foodir/foo to ' +
               '/davtests/foodir/bar:');
    fs.copy('/davtests/foodir/foo', '/davtests/foodir/bar', handler);
};

function testListDir() {
    function handler(error, content) {
        assert(!error, 'ListDir test failed, expected no error, got ' +
               error);
        if (!error) {
            writeToDiv('content: ' + 
                       (content.toSource ? content.toSource() : content));
        };
        testMove();
    };
    writeToDiv('Going to list dir /davtests/foodir/:');
    fs.listDir('/davtests/foodir/', handler);
};

function testMove() {
    function handler(error) {
        assert(!error, 'Move test failed, expected no error, got ' + error);
        testGetProps();
    };
    writeToDiv('Going to move file /davtests/foodir/foo to ' +
               '/davtests/foodir/bar:');
    fs.move('/davtests/foodir/foo', '/davtests/foodir/bar', handler);
};

function testGetProps() {
    function handler(error, content) {
        assert(!error, 'GetProps test failed, expected no error, got ' + 
               error);
        if (!error) {
            writeToDiv('DAV: properties:');
            for (var name in content['DAV:']) {
                writeToDiv(name + ': ' + content['DAV:'][name].toXML());
            };
        };
        testLocked1();
    };
    writeToDiv('Going to get properties of file /davtests/foodir/bar:');
    fs.getProps('/davtests/foodir/bar', handler);
};

function testLocked1() {
    function handler(error, content) {
        assert((!error && !content),
               'Locked test 1 failed, expected no error and false as return ' +
               'value, received ' + error + ' as error and ' + content + ' ' +
               'as content');
        testLock();
    };
    writeToDiv('Going to check whether file /davtests/foodir/bar is unlocked:'
               );
    fs.isLocked('/davtests/foodir/bar', handler);
};

function testLock() {
    function handler(error, locktoken) {
        assert(!error, 'Lock test failed, expected no error, got ' + error);
        testLocked2(locktoken);
    };
    writeToDiv('Going to lock file /davtests/foodir/bar:');
    fs.lock('/davtests/foodir/bar', 'johnny', handler);
};

function testLocked2(locktoken) {
    function handler(error, content) {
        assert((!error && content),
               'Locked test 2 failed, expected no error and true as return ' +
               'value, received ' + error + ' as error and ' + content + ' ' +
               'as content');
        testRemoveLocked(locktoken);
    };
    writeToDiv('Going to check whether file /davtests/foodir/bar is locked:');
    fs.isLocked('/davtests/foodir/bar', handler);
};

function testRemoveLocked(locktoken) {
    function handler(error) {
        assert(error, 'RemoveLocked test failed, expected a locked error, ' +
               'but did not occur');
        testUnlock(locktoken);
    };
    writeToDiv('Going to try to remove locked file /davtests/foodir/bar ' +
               '(success means file could not be removed):');
    fs.remove('/davtests/foodir/bar', handler);
};

function testUnlock(locktoken) {
    function handler(error) {
        assert(!error, 'Unlock test failed, expected no error, got ' + error);
        testRemove();
    };
    writeToDiv('Going to remove file /davtests/foodir/bar:');
    fs.unlock('/davtests/foodir/bar', locktoken, handler);
};

function testRemove() {
    function handler(error) {
        assert(!error, 'Remove test failed, expected no error, got ' + error);
        testRemoveDir();
    };
    writeToDiv('Going to remove file /davtests/foodir/bar:');
    fs.remove('/davtests/foodir/bar', handler);
};

function testRemoveDir() {
    function handler(error) {
        assert(!error, 'Remove test failed, expected no error, got ' + error);
    };
    writeToDiv('Going to remove dir /davtests/foodir/:');
    fs.remove('/davtests/foodir/', handler);
};

