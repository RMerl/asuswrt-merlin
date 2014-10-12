#!/usr/bin/env python
# run tests on all Samba subprojects and push to a git tree on success
# Copyright Andrew Tridgell 2010
# Copyright Jelmer Vernooij 2010
# released under GNU GPL v3 or later

from cStringIO import StringIO
import fcntl
from subprocess import call, check_call, Popen, PIPE
import os, tarfile, sys, time
from optparse import OptionParser
import smtplib
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../selftest"))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../lib/testtools"))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../lib/subunit/python"))
import subunit
import testtools
import subunithelper
import tempfile
from email.mime.application import MIMEApplication
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

samba_master = os.getenv('SAMBA_MASTER', 'git://git.samba.org/samba.git')
samba_master_ssh = os.getenv('SAMBA_MASTER_SSH', 'git+ssh://git.samba.org/data/git/samba.git')

cleanup_list = []

os.environ['CC'] = "ccache gcc"

tasks = {
    "source3" : [ ("autogen", "./autogen.sh", "text/plain"),
                  ("configure", "./configure.developer ${PREFIX}", "text/plain"),
                  ("make basics", "make basics", "text/plain"),
                  ("make", "make -j 4 everything", "text/plain"), # don't use too many processes
                  ("install", "make install", "text/plain"),
                  ("test", "TDB_NO_FSYNC=1 make subunit-test", "text/x-subunit") ],

    "source4" : [ ("configure", "./configure.developer ${PREFIX}", "text/plain"),
                  ("make", "make -j", "text/plain"),
                  ("install", "make install", "text/plain"),
                  ("test", "TDB_NO_FSYNC=1 make subunit-test", "text/x-subunit") ],

    "source4/lib/ldb" : [ ("configure", "./configure --enable-developer -C ${PREFIX}", "text/plain"),
                          ("make", "make -j", "text/plain"),
                          ("install", "make install", "text/plain"),
                          ("test", "make test", "text/plain") ],

    "lib/tdb" : [ ("autogen", "./autogen-waf.sh", "text/plain"),
                  ("configure", "./configure --enable-developer -C ${PREFIX}", "text/plain"),
                  ("make", "make -j", "text/plain"),
                  ("install", "make install", "text/plain"),
                  ("test", "make test", "text/plain") ],

    "lib/talloc" : [ ("autogen", "./autogen-waf.sh", "text/plain"),
                     ("configure", "./configure --enable-developer -C ${PREFIX}", "text/plain"),
                     ("make", "make -j", "text/plain"),
                     ("install", "make install", "text/plain"),
                     ("test", "make test", "text/x-subunit"), ],

    "lib/replace" : [ ("autogen", "./autogen-waf.sh", "text/plain"),
                      ("configure", "./configure --enable-developer -C ${PREFIX}", "text/plain"),
                      ("make", "make -j", "text/plain"),
                      ("install", "make install", "text/plain"),
                      ("test", "make test", "text/plain"), ],

    "lib/tevent" : [ ("configure", "./configure --enable-developer -C ${PREFIX}", "text/plain"),
                     ("make", "make -j", "text/plain"),
                     ("install", "make install", "text/plain"),
                     ("test", "make test", "text/plain"), ],
}


def run_cmd(cmd, dir=None, show=None, output=False, checkfail=True, shell=False):
    if show is None:
        show = options.verbose
    if show:
        print("Running: '%s' in '%s'" % (cmd, dir))
    if output:
        return Popen(cmd, stdout=PIPE, cwd=dir, shell=shell).communicate()[0]
    elif checkfail:
        return check_call(cmd, cwd=dir, shell=shell)
    else:
        return call(cmd, cwd=dir, shell=shell)


def clone_gitroot(test_master, revision="HEAD"):
    run_cmd(["git", "clone", "--shared", gitroot, test_master])
    if revision != "HEAD":
        run_cmd(["git", "checkout", revision])


class RetryChecker(object):
    """Check whether it is necessary to retry."""

    def __init__(self, dir):
        run_cmd(["git", "remote", "add", "-t", "master", "master", samba_master])
        run_cmd(["git", "fetch", "master"])
        cmd = '''set -e
                while :; do
                  sleep 60
                  git describe master/master > old_master.desc
                  git fetch master
                  git describe master/master > master.desc
                  diff old_master.desc master.desc
                done
               '''
        self.proc = Popen(cmd, shell=True, cwd=self.dir)

    def poll(self):
        return self.proc.poll()

    def kill(self):
        self.proc.terminate()
        self.proc.wait()
        self.retry.proc = None


class TreeStageBuilder(object):
    """Handle building of a particular stage for a tree.
    """

    def __init__(self, tree, name, command, fail_quickly=False):
        self.tree = tree
        self.name = name
        self.command = command
        self.fail_quickly = fail_quickly
        self.exitcode = None
        self.stdin = open(os.devnull, 'r')

    def start(self):
        raise NotImplementedError(self.start)

    def poll(self):
        self.exitcode = self.proc.poll()
        return self.exitcode

    def kill(self):
        if self.proc is not None:
            try:
                run_cmd(["killbysubdir", self.tree.sdir], checkfail=False)
            except OSError:
                # killbysubdir doesn't exist ?
                pass
            self.proc.terminate()
            self.proc.wait()
            self.proc = None

    @property
    def failure_reason(self):
        raise NotImplementedError(self.failure_reason)

    @property
    def failed(self):
        return (self.exitcode != 0)


class PlainTreeStageBuilder(TreeStageBuilder):

    def start(self):
        print '%s: [%s] Running %s' % (self.name, self.name, self.command)
        self.proc = Popen(self.command, shell=True, cwd=self.tree.dir,
                          stdout=self.tree.stdout, stderr=self.tree.stderr,
                          stdin=self.stdin)

    @property
    def failure_reason(self):
        return "failed '%s' with exit code %d" % (self.command, self.exitcode)


class AbortingTestResult(subunithelper.TestsuiteEnabledTestResult):

    def __init__(self, stage):
        super(AbortingTestResult, self).__init__()
        self.stage = stage

    def addError(self, test, details=None):
        self.stage.proc.terminate()

    def addFailure(self, test, details=None):
        self.stage.proc.terminate()


class FailureTrackingTestResult(subunithelper.TestsuiteEnabledTestResult):

    def __init__(self, stage):
        super(FailureTrackingTestResult, self).__init__()
        self.stage = stage

    def addError(self, test, details=None):
        if self.stage.failed_test is None:
            self.stage.failed_test = ("error", test)

    def addFailure(self, test, details=None):
        if self.stage.failed_test is None:
            self.stage.failed_test = ("failure", test)


class SubunitTreeStageBuilder(TreeStageBuilder):

    def __init__(self, tree, name, command, fail_quickly=False):
        super(SubunitTreeStageBuilder, self).__init__(tree, name, command,
                fail_quickly)
        self.failed_test = None
        self.subunit_path = os.path.join(gitroot,
            "%s.%s.subunit" % (self.tree.tag, self.name))
        self.tree.logfiles.append(
            (self.subunit_path, os.path.basename(self.subunit_path),
             "text/x-subunit"))
        self.subunit = open(self.subunit_path, 'w')

        formatter = subunithelper.PlainFormatter(False, True, {})
        clients = [formatter, subunit.TestProtocolClient(self.subunit),
                   FailureTrackingTestResult(self)]
        if fail_quickly:
            clients.append(AbortingTestResult(self))
        self.subunit_server = subunit.TestProtocolServer(
            testtools.MultiTestResult(*clients),
            self.subunit)
        self.buffered = ""

    def start(self):
        print '%s: [%s] Running' % (self.tree.name, self.name)
        self.proc = Popen(self.command, shell=True, cwd=self.tree.dir,
            stdout=PIPE, stderr=self.tree.stderr, stdin=self.stdin)
        fd = self.proc.stdout.fileno()
        fl = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)

    def poll(self):
        try:
            data = self.proc.stdout.read()
        except IOError:
            return None
        else:
            self.buffered += data
            buffered = ""
            for l in self.buffered.splitlines(True):
                if l[-1] == "\n":
                    self.subunit_server.lineReceived(l)
                else:
                    buffered += l
            self.buffered = buffered
            self.exitcode = self.proc.poll()
            if self.exitcode is not None:
                self.subunit.close()
            return self.exitcode

    @property
    def failure_reason(self):
        if self.failed_test:
            return "failed '%s' with %s in test %s" (self.command, self.failed_test[0], self.failed_test[1])
        else:
            return "failed '%s' with exit code %d in unknown test" % (self.command, self.exitcode)


class TreeBuilder(object):
    '''handle build of one directory'''

    def __init__(self, name, sequence, fail_quickly=False):
        self.name = name
        self.fail_quickly = fail_quickly

        self.tag = self.name.replace('/', '_')
        self.sequence = sequence
        self.next = 0
        self.stages = []
        self.stdout_path = os.path.join(gitroot, "%s.stdout" % (self.tag, ))
        self.stderr_path = os.path.join(gitroot, "%s.stderr" % (self.tag, ))
        self.logfiles = [
            (self.stdout_path, os.path.basename(self.stdout_path), "text/plain"),
            (self.stderr_path, os.path.basename(self.stderr_path), "text/plain"),
            ]
        if options.verbose:
            print("stdout for %s in %s" % (self.name, self.stdout_path))
            print("stderr for %s in %s" % (self.name, self.stderr_path))
        if os.path.exists(self.stdout_path):
            os.unlink(self.stdout_path)
        if os.path.exists(self.stderr_path):
            os.unlink(self.stderr_path)
        self.stdout = open(self.stdout_path, 'w')
        self.stderr = open(self.stderr_path, 'w')
        self.sdir = os.path.join(testbase, self.tag)
        if name in ['pass', 'fail', 'retry']:
            self.dir = self.sdir
        else:
            self.dir = os.path.join(self.sdir, self.name)
        self.prefix = os.path.join(testbase, "prefix", self.tag)
        run_cmd(["rm", "-rf", self.sdir])
        cleanup_list.append(self.sdir)
        cleanup_list.append(self.prefix)
        os.makedirs(self.sdir)
        run_cmd(["rm",  "-rf", self.sdir])
        clone_gitroot(self.sdir, revision)
        self.start_next()
        self.exitcode = None

    def start_next(self):
        if self.next == len(self.sequence):
            print '%s: Completed OK' % self.name
            self.done = True
            self.stdout.close()
            self.stderr.close()
            return
        (stage_name, cmd, output_mime_type) = self.sequence[self.next]
        cmd = cmd.replace("${PREFIX}", "--prefix=%s" % self.prefix)
        if output_mime_type == "text/plain":
            self.current_stage = PlainTreeStageBuilder(self, stage_name, cmd,
                self.fail_quickly)
        elif output_mime_type == "text/x-subunit":
            self.current_stage = SubunitTreeStageBuilder(self, stage_name, cmd,
                self.fail_quickly)
        else:
            raise Exception("Unknown output mime type %s" % output_mime_type)
        self.stages.append(self.current_stage)
        self.current_stage.start()
        self.next += 1

    def remove_logs(self):
        for path, name, mime_type in self.logfiles:
            os.unlink(path)

    def poll(self):
        self.exitcode = self.current_stage.poll()
        if self.exitcode is not None:
            self.current_stage = None
        return self.exitcode

    def kill(self):
        if self.current_stage is not None:
            self.current_stage.kill()
            self.current_stage = None

    @property
    def failed(self):
        return any([s.failed for s in self.stages])

    @property
    def failed_stage(self):
        for s in self.stages:
            if s.failed:
                return s
        return s

    @property
    def failure_reason(self):
        return "%s: [%s] %s" % (self.name, self.failed_stage.name,
            self.failed_stage.failure_reason)


class BuildList(object):
    '''handle build of multiple directories'''

    def __init__(self, tasklist, tasknames):
        global tasks
        self.tlist = []
        self.tail_proc = None
        self.retry = None
        if tasknames == ['pass']:
            tasks = { 'pass' : [ ("pass", '/bin/true', "text/plain") ]}
        if tasknames == ['fail']:
            tasks = { 'fail' : [ ("fail", '/bin/false', "text/plain") ]}
        if tasknames == []:
            tasknames = tasklist
        for n in tasknames:
            b = TreeBuilder(n, tasks[n], not options.fail_slowly)
            self.tlist.append(b)
        if options.retry:
            self.retry = RetryChecker(self.sdir)
            self.need_retry = False

    def kill_kids(self):
        if self.tail_proc is not None:
            self.tail_proc.terminate()
            self.tail_proc.wait()
            self.tail_proc = None
        if self.retry is not None:
            self.retry.kill()
        for b in self.tlist:
            b.kill()

    def wait_one(self):
        while True:
            none_running = True
            for b in self.tlist:
                if b.current_stage is None:
                    continue
                none_running = False
                if b.poll() is None:
                    continue
                return b
            if options.retry:
                ret = self.retry.poll()
                if ret:
                    self.need_retry = True
                    self.retry = None
                    return None
            if none_running:
                return None
            time.sleep(0.1)

    def run(self):
        while True:
            b = self.wait_one()
            if options.retry and self.need_retry:
                self.kill_kids()
                print("retry needed")
                return (0, None, None, None, "retry")
            if b is None:
                break
            if b.failed:
                self.kill_kids()
                return (b.exitcode, b.name, b.failed_stage, b.tag, b.failure_reason)
            b.start_next()
        self.kill_kids()
        return (0, None, None, None, "All OK")

    def tarlogs(self, name=None, fileobj=None):
        tar = tarfile.open(name=name, fileobj=fileobj, mode="w:gz")
        for b in self.tlist:
            for (path, name, mime_type) in b.logfiles:
                tar.add(path, arcname=name)
        if os.path.exists("autobuild.log"):
            tar.add("autobuild.log")
        tar.close()

    def attach_logs(self, outer):
        f = StringIO()
        self.tarlogs(fileobj=f)
        msg = MIMEApplication(f.getvalue(), "x-gzip")
        msg.add_header('Content-Disposition', 'attachment',
                       filename="logs.tar.gz")
        outer.attach(msg)

    def remove_logs(self):
        for b in self.tlist:
            b.remove_logs()

    def start_tail(self):
        cmd = "tail -f *.stdout *.stderr"
        self.tail_proc = Popen(cmd, shell=True, cwd=gitroot)


def cleanup():
    if options.nocleanup:
        return
    print("Cleaning up ....")
    for d in cleanup_list:
        run_cmd(["rm", "-rf", d])


def find_git_root(p):
    '''get to the top of the git repo'''
    while p != '/':
        if os.path.isdir(os.path.join(p, ".git")):
            return p
        p = os.path.abspath(os.path.join(p, '..'))
    return None


def daemonize(logfile):
    pid = os.fork()
    if pid == 0: # Parent
        os.setsid()
        pid = os.fork()
        if pid != 0: # Actual daemon
            os._exit(0)
    else: # Grandparent
        os._exit(0)

    import resource      # Resource usage information.
    maxfd = resource.getrlimit(resource.RLIMIT_NOFILE)[1]
    if maxfd == resource.RLIM_INFINITY:
        maxfd = 1024 # Rough guess at maximum number of open file descriptors.
    for fd in range(0, maxfd):
        try:
            os.close(fd)
        except OSError:
            pass
    os.open(logfile, os.O_RDWR | os.O_CREAT)
    os.dup2(0, 1)
    os.dup2(0, 2)


def rebase_tree(url):
    print("Rebasing on %s" % url)
    run_cmd(["git", "remote", "add", "-t", "master", "master", url], show=True,
            dir=test_master)
    run_cmd(["git", "fetch", "master"], show=True, dir=test_master)
    if options.fix_whitespace:
        run_cmd(["git", "rebase", "--whitespace=fix", "master/master"],
                show=True, dir=test_master)
    else:
        run_cmd(["git", "rebase", "master/master"], show=True, dir=test_master)
    diff = run_cmd(["git", "--no-pager", "diff", "HEAD", "master/master"],
        dir=test_master, output=True)
    if diff == '':
        print("No differences between HEAD and master/master - exiting")
        sys.exit(0)

def push_to(url):
    print("Pushing to %s" % url)
    if options.mark:
        run_cmd("EDITOR=script/commit_mark.sh git commit --amend -c HEAD",
            dir=test_master, shell=True)
        # the notes method doesn't work yet, as metze hasn't allowed
        # refs/notes/* in master
        # run_cmd("EDITOR=script/commit_mark.sh git notes edit HEAD",
        #     dir=test_master)
    run_cmd(["git", "remote", "add", "-t", "master", "pushto", url], show=True,
        dir=test_master)
    run_cmd(["git", "push", "pushto", "+HEAD:master"], show=True,
        dir=test_master)

def_testbase = os.getenv("AUTOBUILD_TESTBASE")
if def_testbase is None:
    if os.path.exists("/memdisk"):
        def_testbase = "/memdisk/%s" % os.getenv('USER')
    else:
        def_testbase = os.path.join(tempfile.gettempdir(), "autobuild-%s" % os.getenv("USER"))

parser = OptionParser()
parser.add_option("--repository", help="repository to run tests for", default=None, type=str)
parser.add_option("--revision", help="revision to compile if not HEAD", default=None, type=str)
parser.add_option("--tail", help="show output while running", default=False, action="store_true")
parser.add_option("--keeplogs", help="keep logs", default=False, action="store_true")
parser.add_option("--nocleanup", help="don't remove test tree", default=False, action="store_true")
parser.add_option("--testbase", help="base directory to run tests in (default %s)" % def_testbase,
                  default=def_testbase)
parser.add_option("--passcmd", help="command to run on success", default=None)
parser.add_option("--verbose", help="show all commands as they are run",
                  default=False, action="store_true")
parser.add_option("--rebase", help="rebase on the given tree before testing",
                  default=None, type='str')
parser.add_option("--rebase-master", help="rebase on %s before testing" % samba_master,
                  default=False, action='store_true')
parser.add_option("--pushto", help="push to a git url on success",
                  default=None, type='str')
parser.add_option("--push-master", help="push to %s on success" % samba_master_ssh,
                  default=False, action='store_true')
parser.add_option("--mark", help="add a Tested-By signoff before pushing",
                  default=False, action="store_true")
parser.add_option("--fix-whitespace", help="fix whitespace on rebase",
                  default=False, action="store_true")
parser.add_option("--retry", help="automatically retry if master changes",
                  default=False, action="store_true")
parser.add_option("--email", help="send email to the given address on failure",
                  type='str', default=None)
parser.add_option("--always-email", help="always send email, even on success",
                  action="store_true")
parser.add_option("--daemon", help="daemonize after initial setup",
                  action="store_true")
parser.add_option("--fail-slowly", help="continue running tests even after one has already failed",
                  action="store_true")


def email_failure(blist, exitcode, failed_task, failed_stage, failed_tag, errstr):
    '''send an email to options.email about the failure'''
    user = os.getenv("USER")
    text = '''
Dear Developer,

Your autobuild failed when trying to test %s with the following error:
   %s

the autobuild has been abandoned. Please fix the error and resubmit.

You can see logs of the failed task here:

  http://git.samba.org/%s/samba-autobuild/%s.stdout
  http://git.samba.org/%s/samba-autobuild/%s.stderr

A summary of the autobuild process is here:

  http://git.samba.org/%s/samba-autobuild/autobuild.log

or you can get full logs of all tasks in this job here:

  http://git.samba.org/%s/samba-autobuild/logs.tar.gz

The top commit for the tree that was built was:

%s

''' % (failed_task, errstr, user, failed_tag, user, failed_tag, user, user,
       get_top_commit_msg(test_master))

    msg = MIMEMultipart()
    msg['Subject'] = 'autobuild failure for task %s during %s' % (
        failed_task, failed_stage.name)
    msg['From'] = 'autobuild@samba.org'
    msg['To'] = options.email

    main = MIMEText(text)
    msg.attach(main)

    blist.attach_logs(msg)

    s = smtplib.SMTP()
    s.connect()
    s.sendmail(msg['From'], [msg['To']], msg.as_string())
    s.quit()

def email_success(blist):
    '''send an email to options.email about a successful build'''
    user = os.getenv("USER")
    text = '''
Dear Developer,

Your autobuild has succeeded.

'''

    if options.keeplogs:
        text += '''

you can get full logs of all tasks in this job here:

  http://git.samba.org/%s/samba-autobuild/logs.tar.gz

''' % user

    text += '''
The top commit for the tree that was built was:

%s
''' % (get_top_commit_msg(test_master),)

    msg = MIMEMultipart()
    msg['Subject'] = 'autobuild success'
    msg['From'] = 'autobuild@samba.org'
    msg['To'] = options.email

    main = MIMEText(text, 'plain')
    msg.attach(main)

    blist.attach_logs(msg)

    s = smtplib.SMTP()
    s.connect()
    s.sendmail(msg['From'], [msg['To']], msg.as_string())
    s.quit()


(options, args) = parser.parse_args()

if options.retry:
    if not options.rebase_master and options.rebase is None:
        raise Exception('You can only use --retry if you also rebase')

testbase = os.path.join(options.testbase, "b%u" % (os.getpid(),))
test_master = os.path.join(testbase, "master")

if options.repository is not None:
    repository = options.repository
else:
    repository = os.getcwd()

gitroot = find_git_root(repository)
if gitroot is None:
    raise Exception("Failed to find git root under %s" % repository)

# get the top commit message, for emails
if options.revision is not None:
    revision = options.revision
else:
    revision = "HEAD"

def get_top_commit_msg(reporoot):
    return run_cmd(["git", "log", "-1"], dir=reporoot, output=True)

try:
    os.makedirs(testbase)
except Exception, reason:
    raise Exception("Unable to create %s : %s" % (testbase, reason))
cleanup_list.append(testbase)

if options.daemon:
    logfile = os.path.join(testbase, "log")
    print "Forking into the background, writing progress to %s" % logfile
    daemonize(logfile)

while True:
    try:
        run_cmd(["rm", "-rf", test_master])
        cleanup_list.append(test_master)
        clone_gitroot(test_master, revision)
    except:
        cleanup()
        raise

    try:
        if options.rebase is not None:
            rebase_tree(options.rebase)
        elif options.rebase_master:
            rebase_tree(samba_master)
        blist = BuildList(tasks, args)
        if options.tail:
            blist.start_tail()
        (exitcode, failed_task, failed_stage, failed_tag, errstr) = blist.run()
        if exitcode != 0 or errstr != "retry":
            break
        cleanup()
    except:
        cleanup()
        raise

blist.kill_kids()
if options.tail:
    print("waiting for tail to flush")
    time.sleep(1)

if exitcode == 0:
    print errstr
    if options.passcmd is not None:
        print("Running passcmd: %s" % options.passcmd)
        run_cmd(options.passcmd, dir=test_master, shell=True)
    if options.pushto is not None:
        push_to(options.pushto)
    elif options.push_master:
        push_to(samba_master_ssh)
    if options.keeplogs:
        blist.tarlogs("logs.tar.gz")
        print("Logs in logs.tar.gz")
    if options.always_email:
        email_success(blist)
    blist.remove_logs()
    cleanup()
    print(errstr)
else:
    # something failed, gather a tar of the logs
    blist.tarlogs("logs.tar.gz")

    if options.email is not None:
        email_failure(blist, exitcode, failed_task, failed_stage, failed_tag,
            errstr)

    cleanup()
    print(errstr)
    print("Logs in logs.tar.gz")
sys.exit(exitcode)
