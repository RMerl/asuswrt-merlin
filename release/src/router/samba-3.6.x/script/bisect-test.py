#!/usr/bin/env python
# use git bisect to work out what commit caused a test failure
# Copyright Andrew Tridgell 2010
# released under GNU GPL v3 or later


from subprocess import call, check_call, Popen, PIPE
import os, tempfile, sys
from optparse import OptionParser

parser = OptionParser()
parser.add_option("", "--good", help="known good revision (default HEAD~100)", default='HEAD~100')
parser.add_option("", "--bad", help="known bad revision (default HEAD)", default='HEAD')
parser.add_option("", "--skip-build-errors", help="skip revision where make fails",
                  action='store_true', default=False)
parser.add_option("", "--autogen", help="run autogen before each build",action="store_true", default=False)
parser.add_option("", "--autogen-command", help="command to use for autogen (default ./autogen.sh)",
                  type='str', default="./autogen.sh")
parser.add_option("", "--configure", help="run configure.developer before each build",
    action="store_true", default=False)
parser.add_option("", "--configure-command", help="the command for configure (default ./configure.developer)",
                  type='str', default="./configure.developer")
parser.add_option("", "--build-command", help="the command to build the tree (default 'make -j')",
                  type='str', default="make -j")
parser.add_option("", "--test-command", help="the command to test the tree (default 'make test')",
                  type='str', default="make test")
parser.add_option("", "--clean", help="run make clean before each build",
                  action="store_true", default=False)


(opts, args) = parser.parse_args()


def run_cmd(cmd, dir=".", show=True, output=False, checkfail=True):
    if show:
        print("Running: '%s' in '%s'" % (cmd, dir))
    if output:
        return Popen([cmd], shell=True, stdout=PIPE, cwd=dir).communicate()[0]
    elif checkfail:
        return check_call(cmd, shell=True, cwd=dir)
    else:
        return call(cmd, shell=True, cwd=dir)

def find_git_root():
    '''get to the top of the git repo'''
    p=os.getcwd()
    while p != '/':
        if os.path.isdir(os.path.join(p, ".git")):
            return p
        p = os.path.abspath(os.path.join(p, '..'))
    return None

cwd = os.getcwd()
gitroot = find_git_root()

# create a bisect script
f = tempfile.NamedTemporaryFile(delete=False)
f.write("set -x\n")
f.write("cd %s || exit 125\n" % cwd)
if opts.autogen:
    f.write("%s || exit 125\n" % opts.autogen_command)
if opts.configure:
    f.write("%s || exit 125\n" % opts.configure_command)
if opts.clean:
    f.write("make clean || exit 125\n")
if opts.skip_build_errors:
    build_err = 125
else:
    build_err = 1
f.write("%s || exit %u\n" % (opts.build_command, build_err))
f.write("%s || exit 1\n" % opts.test_command)
f.write("exit 0\n")
f.close()

def cleanup():
    run_cmd("git bisect reset", dir=gitroot)
    os.unlink(f.name)
    sys.exit(-1)

# run bisect
ret = -1
try:
    run_cmd("git bisect reset", dir=gitroot, show=False, checkfail=False)
    run_cmd("git bisect start %s %s --" % (opts.bad, opts.good), dir=gitroot)
    ret = run_cmd("git bisect run bash %s" % f.name, dir=gitroot, show=True, checkfail=False)
except KeyboardInterrupt:
    print("Cleaning up")
    cleanup()
except Exception, reason:
    print("Failed bisect: %s" % reason)
    cleanup()

run_cmd("git bisect reset", dir=gitroot)
os.unlink(f.name)
sys.exit(ret)
