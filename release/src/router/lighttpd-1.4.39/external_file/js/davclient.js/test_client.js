// don't forget to set the proper hostname
var client = new davlib.DavClient();
client.initialize();

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
    } else {
        writeToDiv('success');
    };
};

// since the lib is async I wrote the functions in the order
// they are executed to give a bit of an overview
function runTests() {
    testMakeDir();
};

function wrapContinueHandler(currname, handler, expected_status) {
    var wrapped = function(status, statusstr, content) {
        writeToDiv('status: ' + status + ' (' + statusstr + ')');
        if (content.locktoken) {
            window.LAST_LOCKTOKEN = content.locktoken;
        };
        if (content) {
            if (content.properties) {
                content = content.properties['foo:'] ? 
                          content.properties['foo:']['foo'].toXML() : 
                          'no property foo:foo';
            } else if (content.toSource) {
                content = content.toSource();
            };
            writeToDiv('content: ' + content);
        };
        writeToDiv('Expected status: ' + expected_status);
        if (status == expected_status) {
            writeToDiv('OK!');
        } else {
            writeToDiv('NOT OK!!');
        };
        writeToDiv('--------------------');
        handler();
    };
    return wrapped;
};

function testMakeDir() {
    writeToDiv('Going to create dir /davtests/foo/:');
    client.MKCOL('/davtests/foo/',
                wrapContinueHandler('make dir', testMove, 201));
};

function testMove() {
    writeToDiv('Going to move /davtests/foo/ to /davtests/bar/:');
    client.MOVE('/davtests/foo/', '/davtests/bar',
                wrapContinueHandler('move dir', testCopy, 201));
};

function testCopy() {
    writeToDiv('Going to copy /davtests/bar/ to /davtests/foo/:');
    client.COPY('/davtests/bar/', '/davtests/foo/',
                wrapContinueHandler('copy dir', testDeleteDir, 201));
};

function testDeleteDir() {
    writeToDiv('Going to delete dir /davtests/bar/:');
    client.DELETE('/davtests/bar/',
                  wrapContinueHandler('delete dir', testReadFile1, 204));
};

function testReadFile1() {
    writeToDiv('Going to read file /davtests/foo/bar:');
    client.GET('/davtests/foo/bar',
               wrapContinueHandler('read file', testWriteFile1, 404));
};

function testWriteFile1() {
    writeToDiv('Going to create file /davtests/foo/bar:');
    client.PUT('/davtests/foo/bar', 'foo', 
               wrapContinueHandler('create file', testReadFile2, 201));
};

function testReadFile2() {
    writeToDiv('Going to read file /davtests/foo/bar:');
    client.GET('/davtests/foo/bar',
               wrapContinueHandler('read file', testGetProps1, 200));
};

function testGetProps1() {
    writeToDiv('Going to read properties of file /davtests/foo/bar:');
    client.PROPFIND('/davtests/foo/bar',
                    wrapContinueHandler('read properties', testSetProps1,
                    207));
};

function testSetProps1() {
    writeToDiv('Going to set properties of file /davtests/foo/bar:');
    client.PROPPATCH('/davtests/foo/bar', 
                     wrapContinueHandler('set properties', testGetProps2, 207),
                     window,
                     {'foo:': {'foo': '<foo xmlns="foo:">bar</foo>'}});
};

function testGetProps2() {
    writeToDiv('Going to read properties of /davtests/foo/bar:');
    client.PROPFIND('/davtests/foo/bar',
                    wrapContinueHandler('read properties', testLock, 207));
};

function testLock() {
    writeToDiv('Going to lock file /davtests/foo/bar:');
    client.LOCK('/davtests/foo/bar', 'http://johnnydebris.net/',
                wrapContinueHandler('lock file', testDelete, 200));
};

function testDelete() {
    writeToDiv('Going to delete dir /davtests/foo:');
    client.DELETE('/davtests/foo/', 
                  wrapContinueHandler('delete dir', testUnlock, 424));
};

function testUnlock() {
    writeToDiv('Going to unlock file /davtests/foo/bar:');
    client.UNLOCK('/davtests/foo/bar', window.LAST_LOCKTOKEN,
                  wrapContinueHandler('unlock file', testDelete2, 204));
};

function testDelete2() {
    writeToDiv('Going to delete dir /davtests/foo:');
    client.DELETE('/davtests/foo/',
                  wrapContinueHandler('delete dir', finish, 204));
};

function finish() {
    writeToDiv('Finished');
};

