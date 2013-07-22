# JS unit test support for py.test - (c) 2007 Guido Wesdorp. All rights
# reserved
#
# This software is distributed under the terms of the JSBase
# License. See LICENSE.txt for license text.

import py

here = py.magic.autopath().dirpath()

class JSTest(py.test.collect.Item):
    def run(self):
        path = self.fspath
        test = self.name.split('/')[-1]
        paths = [path.strpath, 
                    (here / 'exception.js').strpath,
                    (here / 'testing.js').strpath,
                    (here / 'misclib.js').strpath,
                ]
        testjs = (here / 'testing/testbase.js').read() % (
                    paths, test, '__main__')
        curdir = str(py.path.local('.'))
        py.std.os.chdir(str(self.fspath.dirpath()))
        try:
            jspath = self.fspath.new(basename='__testbase_temp.js')
            try:
                jspath.write(testjs)
                pipe = py.std.os.popen('js "%s"' % (jspath,))
                try:
                    data = {}
                    for line in pipe:
                        done = self._handle_line(line, data)
                        if done:
                            errdata = data[data['current']]
                            if errdata:
                                self.fail(errdata)
                finally:
                    pipe.close()
            finally:
                jspath.remove()
        finally:
            py.std.os.chdir(curdir)

    def fail(self, errdata):
        py.test.fail(
        '\nJS traceback (most recent last): \n%s\n%s\n' % (
                (errdata[1:] and 
                    self._format_tb(errdata[1:-5]) or
                    'no traceback available'
                ),
                errdata[0], 
            )
        )

    _handling_traceback = False
    def _handle_line(self, line, data):
        line = line[:-1]
        if line.startswith('end test'):
            return True
        if self._handling_traceback and line != 'end traceback':
            data[data['current']].append(line)
        if line.startswith('PRINTED: '):
            print line[9:]
        elif line.startswith('running test '):
            testname = line[13:]
            data['current'] = testname
            data[testname] = []
        elif line.startswith('success'):
            pass
        elif line.startswith('failure: '):
            data[data['current']].append(line[9:])
        elif line.startswith('traceback'):
            self._handling_traceback = True
        elif line.startswith('end traceback'):
            self._handling_traceback = False

    def _format_tb(self, tb):
        tb.reverse()
        ret = []
        for line in tb:
            line = line.strip()
            if not line:
                continue
            funcsig, lineinfo = line.split('@', 1)
            fpath, lineno = lineinfo.rsplit(':', 1)
            fname = py.path.local(fpath).basename
            # XXX might filter out too much... but it's better than leaving it
            # all in (since it adds a couple of lines to the end of the tb,
            # making it harder to find the problem line)
            if fname in ['__testbase_temp.js', '__testbase_find.js',
                            'exception.js']: 
                continue
            lineno = int(lineno)
            if lineno == 0:
                fname = "<unknown>"
            ret.append('File "%s", line %s, in %s' % (
                        fname, lineno, funcsig or '?'))
            if lineno > 0:
                line = py.path.local(fpath).readlines()[lineno - 1]
                ret.append('    %s' % (line.strip(),))
        return '\n'.join(['  %s' % (r,) for r in ret]) 
            
class JSChecker(py.test.collect.Module):
    def __repr__(self): 
        return py.test.collect.Collector.__repr__(self) 

    def setup(self): 
        pass 

    def teardown(self): 
        pass 

    def run(self): 
        findjs = here.join('testing/findtests.js').read() % (
                    self.fspath.strpath, '__main__')
        curdir = str(py.path.local('.'))
        py.std.os.chdir(str(self.fspath.dirpath()))
        tests = []
        try:
            jspath = self.fspath.new(basename='__findtests.js')
            try:
                jspath.write(findjs)
                stdin, pipe, stderr = py.std.os.popen3('js "%s"' % (jspath,))
                try:
                    error = stderr.next()
                    print 'Error read:', error
                except StopIteration:
                    pass
                else:
                    if error.find('command not found') > -1:
                        py.test.skip(
                            'error running "js" (SpiderMonkey), which is '
                            'required to run JS tests')
                    else:
                        py.test.fail(error)
                    return
                try:
                    for line in pipe:
                        tests.append(line.strip())
                finally:
                    py.std.sys.stdout = py.std.sys.__stdout__
                    pipe.close()
            finally:
                jspath.remove()
        finally:
            py.std.os.chdir(curdir)
        return ['%s/%s' % (self.fspath.basename, test) for test in tests]

    def join(self, name):
        if py.path.local(name).dirpath().strpath.endswith('.js'):
            return JSTest(name, self)
        return super(JSChecker, self).join(name)

class Directory(py.test.collect.Directory):
    def run(self):
        if self.fspath == here:
            return [p.basename for p in self.fspath.listdir('test_*') if
                    p.ext in ['.py', '.js']]
        return super(Directory, self).run()

    def join(self, name):
        if not name.endswith('.js'):
            return super(Directory, self).join(name)
        p = self.fspath.join(name)
        if p.check(file=1):
            return JSChecker(p, parent=self)
