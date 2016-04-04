# JS unit test support for py.test - (c) 2007 Guido Wesdorp. All rights
# reserved
#
# This software is distributed under the terms of the JSBase
# License. See LICENSE.txt for license text.

import py
here = py.magic.autopath().dirpath()

from jsbase.conftest import JSTest, JSChecker, Directory as _Directory

class Directory(_Directory):
    def run(self):
        if self.fspath == here:
            return [p.basename for p in self.fspath.listdir('test_*') if
                    p.ext == '.js']
        return super(Directory, self).run()

