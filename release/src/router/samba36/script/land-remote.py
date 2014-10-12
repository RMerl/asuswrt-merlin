#!/usr/bin/python
# Ship a local branch to a remote host (sn-104?) over ssh and run autobuild in it.
# Copyright (C) 2010 Jelmer Vernooij <jelmer@samba.org>
# Published under the GPL, v3 or later

import optparse
import os
import subprocess
import sys

samba_master = os.getenv('SAMBA_MASTER', 'git://git.samba.org/samba.git')

parser = optparse.OptionParser("autoland-remote [options] [trees...]")
parser.add_option("--remote-repo", help="Location of remote repository (default: temporary repository)", type=str, default=None)
parser.add_option("--host", help="Host to land on (SSH connection string)", type=str, default="sn-devel-104.sn.samba.org")
parser.add_option("--foreground", help="Don't daemonize", action="store_true", default=False)
parser.add_option("--email", help="Email address to send build/test output to", type=str, default=None, metavar="EMAIL")
parser.add_option("--always-email", help="always send email, even on success", action="store_true")
parser.add_option("--rebase-master", help="rebase on master before testing", default=False, action='store_true')
parser.add_option("--push-master", help="push to samba.org master on success",
                  default=False, action='store_true')
parser.add_option("--pushto", help="push to a git url on success",
                  default=None, type='str')
parser.add_option("--rebase", help="rebase on the given tree before testing", default=None, type='str')
parser.add_option("--passcmd", help="command to run on success", default=None)
parser.add_option("--tail", help="show output while running", default=False, action="store_true")
parser.add_option("--keeplogs", help="keep logs", default=False, action="store_true")
parser.add_option("--nocleanup", help="don't remove test tree", default=False, action="store_true")
parser.add_option("--revision", help="revision to compile if not HEAD", default=None, type=str)
parser.add_option("--fix-whitespace", help="fix whitespace on rebase",
                  default=False, action="store_true")
parser.add_option("--fail-slowly", help="continue running tests even after one has already failed",
                  action="store_true")

(opts, extra_args) = parser.parse_args()

if opts.email is None and os.getenv("EMAIL") is not None:
    opts.email = os.getenv("EMAIL")

if opts.email:
    print "Sending email to %s" % opts.email

if not opts.foreground and not opts.email:
    print "Not running in foreground and --email not specified."
    sys.exit(1)

if not opts.foreground and opts.push_master:
    print "Pushing to master, forcing run in foreground."
    opts.foreground = True

if not opts.remote_repo:
    print "%s$ mktemp -d" % opts.host
    f = subprocess.Popen(["ssh", opts.host, "mktemp", "-d"], stdout=subprocess.PIPE)
    (stdout, stderr) = f.communicate()
    if f.returncode != 0:
        sys.exit(1)
    remote_repo = stdout.rstrip()
    print "Remote tempdir: %s" % remote_repo
    # Bootstrap, git.samba.org is usually more easily accessible.
    #remote_args = ["git", "clone", samba_master, remote_repo]
    remote_args = ["if [ -d /data/git/samba.git ]; then git clone --shared /data/git/samba.git %s; else git clone --shared %s %s; fi" % (remote_repo, samba_master, remote_repo)]
    #remote_args = ["git", "init", remote_repo]
    print "%s$ %s" % (opts.host, " ".join(remote_args))
    subprocess.check_call(["ssh", opts.host] + remote_args)
else:
    remote_repo = opts.remote_repo

print "Pushing local branch"

if opts.revision is not None:
    revision = opts.revision
else:
    revision = "HEAD"
args = ["git", "push", "--force", "git+ssh://%s/%s" % (opts.host, remote_repo), "%s:land" % revision]
print "$ " + " ".join(args)
subprocess.check_call(args)
remote_args = ["cd", remote_repo, ";", "git", "checkout", "land", ";", "python", "-u", "./script/land.py", "--repository=%s" % remote_repo]

if (opts.email and not (opts.foreground or opts.pushto or opts.push_master)):
    # Force always emailing if there's nothing else to do
    opts.always_email = True

if opts.email:
    remote_args.append("--email=%s" % opts.email)
if opts.always_email:
    remote_args.append("--always-email")
if not opts.foreground:
    remote_args.append("--daemon")
if opts.nocleanup:
    remote_args.append("--nocleanup")
if opts.fix_whitespace:
    remote_args.append("--fix-whitespace")
if opts.tail:
    remote_args.append("--tail")
if opts.keeplogs:
    remote_args.append("--keeplogs")
if opts.rebase_master:
    remote_args.append("--rebase-master")
if opts.rebase:
    remote_args.append("--rebase=%s" % opts.rebase)
if opts.passcmd:
    remote_args.append("--passcmd=%s" % opts.passcmd)
if opts.pushto:
    remote_args.append("--pushto=%s" % opts.pushto)
if opts.push_master:
    remote_args.append("--push-master")
if opts.fail_slowly:
    remote_args.append("--fail-slowly")

remote_args += extra_args
print "%s$ %s" % (opts.host, " ".join(remote_args))
args = ["ssh", "-A", opts.host] + remote_args
sys.exit(subprocess.call(args))
