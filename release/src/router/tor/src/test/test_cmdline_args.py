#!/usr/bin/python

import binascii
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

TOR = "./src/or/tor"
TOP_SRCDIR = "."

if len(sys.argv) > 1:
    TOR = sys.argv[1]
    del sys.argv[1]

if len(sys.argv) > 1:
    TOP_SRCDIR = sys.argv[1]
    del sys.argv[1]

class UnexpectedSuccess(Exception):
    pass

class UnexpectedFailure(Exception):
    pass

if sys.version < '3':
    def b2s(b):
       return b
    def s2b(s):
       return s
    def NamedTemporaryFile():
       return tempfile.NamedTemporaryFile(delete=False)
else:
    def b2s(b):
       return str(b, 'ascii')
    def s2b(s):
       return s.encode('ascii')
    def NamedTemporaryFile():
       return tempfile.NamedTemporaryFile(mode="w",delete=False,encoding="ascii")

def contents(fn):
    f = open(fn)
    try:
        return f.read()
    finally:
        f.close()

def run_tor(args, failure=False):
    p = subprocess.Popen([TOR] + args, stdout=subprocess.PIPE)
    output, _ = p.communicate()
    result = p.poll()
    if result and not failure:
        raise UnexpectedFailure()
    elif not result and failure:
        raise UnexpectedSuccess()
    return b2s(output)

def spaceify_fp(fp):
    for i in range(0, len(fp), 4):
        yield fp[i:i+4]

def lines(s):
    out = s.split("\n")
    if out and out[-1] == '':
        del out[-1]
    return out

def strip_log_junk(line):
    m = re.match(r'([^\[]+\[[a-z]*\] *)(.*)', line)
    if not m:
        return ""+line
    return m.group(2).strip()

def randstring(entropy_bytes):
    s = os.urandom(entropy_bytes)
    return b2s(binascii.b2a_hex(s))

def findLineContaining(lines, s):
    for ln in lines:
        if s in ln:
            return True
    return False

class CmdlineTests(unittest.TestCase):

    def test_version(self):
        out = run_tor(["--version"])
        self.assertTrue(out.startswith("Tor version "))
        self.assertEqual(len(lines(out)), 1)

    def test_quiet(self):
        out = run_tor(["--quiet", "--quumblebluffin", "1"], failure=True)
        self.assertEqual(out, "")

    def test_help(self):
        out = run_tor(["--help"], failure=False)
        out2 = run_tor(["-h"], failure=False)
        self.assertTrue(out.startswith("Copyright (c) 2001"))
        self.assertTrue(out.endswith(
            "tor -f <torrc> [args]\n"
            "See man page for options, or https://www.torproject.org/ for documentation.\n"))
        self.assertTrue(out == out2)

    def test_hush(self):
        torrc = NamedTemporaryFile()
        torrc.close()
        try:
            out = run_tor(["--hush", "-f", torrc.name,
                           "--quumblebluffin", "1"], failure=True)
        finally:
            os.unlink(torrc.name)
        self.assertEqual(len(lines(out)), 2)
        ln = [ strip_log_junk(l) for l in lines(out) ]
        self.assertEqual(ln[0], "Failed to parse/validate config: Unknown option 'quumblebluffin'.  Failing.")
        self.assertEqual(ln[1], "Reading config failed--see warnings above.")

    def test_missing_argument(self):
        out = run_tor(["--hush", "--hash-password"], failure=True)
        self.assertEqual(len(lines(out)), 2)
        ln = [ strip_log_junk(l) for l in lines(out) ]
        self.assertEqual(ln[0], "Command-line option '--hash-password' with no value. Failing.")

    def test_hash_password(self):
        out = run_tor(["--hash-password", "woodwose"])
        result = lines(out)[-1]
        self.assertEqual(result[:3], "16:")
        self.assertEqual(len(result), 61)
        r = binascii.a2b_hex(result[3:])
        self.assertEqual(len(r), 29)

        salt, how, hashed = r[:8], r[8], r[9:]
        self.assertEqual(len(hashed), 20)
        if type(how) == type("A"):
          how = ord(how)

        count = (16 + (how & 15)) << ((how >> 4) + 6)
        stuff = salt + s2b("woodwose")
        repetitions = count // len(stuff) + 1
        inp = stuff * repetitions
        inp = inp[:count]

        self.assertEqual(hashlib.sha1(inp).digest(), hashed)

    def test_digests(self):
        main_c = os.path.join(TOP_SRCDIR, "src", "or", "main.c")

        if os.stat(TOR).st_mtime < os.stat(main_c).st_mtime:
            self.skipTest(TOR+" not up to date")
        out = run_tor(["--digests"])
        main_line = [ l for l in lines(out) if l.endswith("/main.c") ]
        digest, name = main_line[0].split()
        f = open(main_c, 'rb')
        actual = hashlib.sha1(f.read()).hexdigest()
        f.close()
        self.assertEqual(digest, actual)

    def test_dump_options(self):
        default_torrc = NamedTemporaryFile()
        torrc = NamedTemporaryFile()
        torrc.write("SocksPort 9999")
        torrc.close()
        default_torrc.write("SafeLogging 0")
        default_torrc.close()
        out_sh = out_nb = out_fl = None
        opts = [ "-f", torrc.name,
                 "--defaults-torrc", default_torrc.name ]
        try:
            out_sh = run_tor(["--dump-config", "short"]+opts)
            out_nb = run_tor(["--dump-config", "non-builtin"]+opts)
            out_fl = run_tor(["--dump-config", "full"]+opts)
            out_nr = run_tor(["--dump-config", "bliznert"]+opts,
                             failure=True)

            out_verif = run_tor(["--verify-config"]+opts)
        finally:
            os.unlink(torrc.name)
            os.unlink(default_torrc.name)

        self.assertEqual(len(lines(out_sh)), 2)
        self.assertTrue(lines(out_sh)[0].startswith("DataDirectory "))
        self.assertEqual(lines(out_sh)[1:],
            [ "SocksPort 9999" ])

        self.assertEqual(len(lines(out_nb)), 2)
        self.assertEqual(lines(out_nb),
            [ "SafeLogging 0",
              "SocksPort 9999" ])

        out_fl = lines(out_fl)
        self.assertTrue(len(out_fl) > 100)
        self.assertTrue("SocksPort 9999" in out_fl)
        self.assertTrue("SafeLogging 0" in out_fl)
        self.assertTrue("ClientOnly 0" in out_fl)

        self.assertTrue(out_verif.endswith("Configuration was valid\n"))

    def test_list_fingerprint(self):
        tmpdir = tempfile.mkdtemp(prefix='ttca_')
        torrc = NamedTemporaryFile()
        torrc.write("ORPort 9999\n")
        torrc.write("DataDirectory %s\n"%tmpdir)
        torrc.write("Nickname tippi")
        torrc.close()
        opts = ["-f", torrc.name]
        try:
            out = run_tor(["--list-fingerprint"]+opts)
            fp = contents(os.path.join(tmpdir, "fingerprint"))
        finally:
            os.unlink(torrc.name)
            shutil.rmtree(tmpdir)

        out = lines(out)
        lastlog = strip_log_junk(out[-2])
        lastline = out[-1]
        fp = fp.strip()
        nn_fp = fp.split()[0]
        space_fp = " ".join(spaceify_fp(fp.split()[1]))
        self.assertEqual(lastlog,
              "Your Tor server's identity key fingerprint is '%s'"%fp)
        self.assertEqual(lastline, "tippi %s"%space_fp)
        self.assertEqual(nn_fp, "tippi")

    def test_list_options(self):
        out = lines(run_tor(["--list-torrc-options"]))
        self.assertTrue(len(out)>100)
        self.assertTrue(out[0] <= 'AccountingMax')
        self.assertTrue("UseBridges" in out)
        self.assertTrue("SocksPort" in out)

    def test_cmdline_args(self):
        default_torrc = NamedTemporaryFile()
        torrc = NamedTemporaryFile()
        torrc.write("SocksPort 9999\n")
        torrc.write("SocksPort 9998\n")
        torrc.write("ORPort 9000\n")
        torrc.write("ORPort 9001\n")
        torrc.write("Nickname eleventeen\n")
        torrc.write("ControlPort 9500\n")
        torrc.close()
        default_torrc.write("")
        default_torrc.close()
        out_sh = out_nb = out_fl = None
        opts = [ "-f", torrc.name,
                 "--defaults-torrc", default_torrc.name,
                 "--dump-config", "short" ]
        try:
            out_1 = run_tor(opts)
            out_2 = run_tor(opts+["+ORPort", "9003",
                                  "SocksPort", "9090",
                                  "/ControlPort",
                                  "/TransPort",
                                  "+ExtORPort", "9005"])
        finally:
            os.unlink(torrc.name)
            os.unlink(default_torrc.name)

        out_1 = [ l for l in lines(out_1) if not l.startswith("DataDir") ]
        out_2 = [ l for l in lines(out_2) if not l.startswith("DataDir") ]

        self.assertEqual(out_1,
                          ["ControlPort 9500",
                           "Nickname eleventeen",
                           "ORPort 9000",
                           "ORPort 9001",
                           "SocksPort 9999",
                           "SocksPort 9998"])
        self.assertEqual(out_2,
                          ["ExtORPort 9005",
                           "Nickname eleventeen",
                           "ORPort 9000",
                           "ORPort 9001",
                           "ORPort 9003",
                           "SocksPort 9090"])

    def test_missing_torrc(self):
        fname = "nonexistent_file_"+randstring(8)
        out = run_tor(["-f", fname, "--verify-config"], failure=True)
        ln = [ strip_log_junk(l) for l in lines(out) ]
        self.assertTrue("Unable to open configuration file" in ln[-2])
        self.assertTrue("Reading config failed" in ln[-1])

        out = run_tor(["-f", fname, "--verify-config", "--ignore-missing-torrc"])
        ln = [ strip_log_junk(l) for l in lines(out) ]
        self.assertTrue(findLineContaining(ln, ", using reasonable defaults"))
        self.assertTrue("Configuration was valid" in ln[-1])

if __name__ == '__main__':
    unittest.main()
