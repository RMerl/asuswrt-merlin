from difflib import unified_diff
import os
import sys
from conf import hook
from exc.test_failed import TestFailed

""" Post-Test Hook: ExpectedFiles
This is a Post-Test hook that checks the test directory for the files it
contains. A dictionary object is passed to it, which contains a mapping of
filenames and contents of all the files that the directory is expected to
contain.
Raises a TestFailed exception if the expected files are not found or if extra
files are found, else returns gracefully.
"""


@hook()
class ExpectedFiles:
    def __init__(self, expected_fs):
        self.expected_fs = expected_fs

    @staticmethod
    def gen_local_fs_snapshot():
        snapshot = {}
        for parent, dirs, files in os.walk('.'):
            for name in files:
                # pubring.kbx will be created by libgpgme if $HOME doesn't contain the .gnupg directory.
					 # setting $HOME to CWD (in base_test.py) breaks two Metalink tests, so we skip this file here.
                if name == 'pubring.kbx':
                    continue

                f = {'content': ''}
                file_path = os.path.join(parent, name)
                with open(file_path) as fp:
                    f['content'] = fp.read()
                snapshot[file_path[2:]] = f

        return snapshot

    def __call__(self, test_obj):
        local_fs = self.gen_local_fs_snapshot()
        for file in self.expected_fs:
            if file.name in local_fs:
                local_file = local_fs.pop(file.name)
                formatted_content = test_obj._replace_substring(file.content)
                if formatted_content != local_file['content']:
                    for line in unified_diff(local_file['content'],
                                             formatted_content,
                                             fromfile='Actual',
                                             tofile='Expected'):
                        print(line, file=sys.stderr)
                    raise TestFailed('Contents of %s do not match' % file.name)
            else:
                raise TestFailed('Expected file %s not found.' % file.name)
        if local_fs:
            print(local_fs)
            raise TestFailed('Extra files downloaded.')
