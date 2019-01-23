from os import utime
from time import strptime
from calendar import timegm
from conf import hook

""" Pre-Test Hook: LocalFiles
This is a pre-test hook used to generate the specific environment before a test
is run. The LocalFiles hook creates the files which should exist on disk before
invoking Wget.
"""


@hook()
class LocalFiles:
    def __init__(self, local_files):
        self.local_files = local_files

    def __call__(self, _):
        for f in self.local_files:
            with open(f.name, 'w') as fp:
                fp.write(f.content)
            if f.timestamp is not None:
                tstamp = timegm(strptime(f.timestamp, '%Y-%m-%d %H:%M:%S'))
                atime = tstamp
                mtime = tstamp
                utime(f.name, (atime, mtime))
