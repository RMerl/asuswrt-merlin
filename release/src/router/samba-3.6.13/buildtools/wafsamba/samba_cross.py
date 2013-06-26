# functions for handling cross-compilation

import Utils, Logs, sys, os, Options, re
from Configure import conf

real_Popen = None

ANSWER_UNKNOWN = (254, "")
ANSWER_FAIL    = (255, "")
ANSWER_OK      = (0, "")

cross_answers_incomplete = False


def add_answer(ca_file, msg, answer):
    '''add an answer to a set of cross answers'''
    try:
        f = open(ca_file, 'a')
    except:
        Logs.error("Unable to open cross-answers file %s" % ca_file)
        sys.exit(1)
    if answer == ANSWER_OK:
        f.write('%s: OK\n' % msg)
    elif answer == ANSWER_UNKNOWN:
        f.write('%s: UNKNOWN\n' % msg)
    elif answer == ANSWER_FAIL:
        f.write('%s: FAIL\n' % msg)
    else:
        (retcode, retstring) = answer
        f.write('%s: (%d, "%s")' % (msg, retcode, retstring))
    f.close()


def cross_answer(ca_file, msg):
    '''return a (retcode,retstring) tuple from a answers file'''
    try:
        f = open(ca_file, 'r')
    except:
        add_answer(ca_file, msg, ANSWER_UNKNOWN)
        return ANSWER_UNKNOWN
    for line in f:
        line = line.strip()
        if line == '' or line[0] == '#':
            continue
        if line.find(':') != -1:
            a = line.split(':')
            thismsg = a[0].strip()
            if thismsg != msg:
                continue
            ans = a[1].strip()
            if ans == "OK" or ans == "YES":
                f.close()
                return ANSWER_OK
            elif ans == "UNKNOWN":
                f.close()
                return ANSWER_UNKNOWN
            elif ans == "FAIL" or ans == "NO":
                f.close()
                return ANSWER_FAIL
            elif ans[0] == '"':
                return (0, ans.strip('"'))
            elif ans[0] == "'":
                return (0, ans.strip("'"))
            else:
                m = re.match('\(\s*(-?\d+)\s*,\s*\"(.*)\"\s*\)', ans)
                if m:
                    f.close()
                    return (int(m.group(1)), m.group(2))
                else:
                    raise Utils.WafError("Bad answer format '%s' in %s" % (line, ca_file))
    f.close()
    add_answer(ca_file, msg, ANSWER_UNKNOWN)
    return ANSWER_UNKNOWN


class cross_Popen(Utils.pproc.Popen):
    '''cross-compilation wrapper for Popen'''
    def __init__(*k, **kw):
        (obj, args) = k

        if '--cross-execute' in args:
            # when --cross-execute is set, then change the arguments
            # to use the cross emulator
            i = args.index('--cross-execute')
            newargs = args[i+1].split()
            newargs.extend(args[0:i])
            args = newargs
        elif '--cross-answers' in args:
            # when --cross-answers is set, then change the arguments
            # to use the cross answers if available
            i = args.index('--cross-answers')
            ca_file = args[i+1]
            msg     = args[i+2]
            ans = cross_answer(ca_file, msg)
            if ans == ANSWER_UNKNOWN:
                global cross_answers_incomplete
                cross_answers_incomplete = True
            (retcode, retstring) = ans
            args = ['/bin/sh', '-c', "echo -n '%s'; exit %d" % (retstring, retcode)]
        real_Popen.__init__(*(obj, args), **kw)


@conf
def SAMBA_CROSS_ARGS(conf, msg=None):
    '''get exec_args to pass when running cross compiled binaries'''
    if not conf.env.CROSS_COMPILE:
        return []

    global real_Popen
    if real_Popen is None:
        real_Popen  = Utils.pproc.Popen
        Utils.pproc.Popen = cross_Popen

    ret = []

    if conf.env.CROSS_EXECUTE:
        ret.extend(['--cross-execute', conf.env.CROSS_EXECUTE])
    elif conf.env.CROSS_ANSWERS:
        if msg is None:
            raise Utils.WafError("Cannot have NULL msg in cross-answers")
        ret.extend(['--cross-answers', os.path.join(Options.launch_dir, conf.env.CROSS_ANSWERS), msg])

    if ret == []:
        raise Utils.WafError("Cannot cross-compile without either --cross-execute or --cross-answers")

    return ret

@conf
def SAMBA_CROSS_CHECK_COMPLETE(conf):
    '''check if we have some unanswered questions'''
    global cross_answers_incomplete
    if conf.env.CROSS_COMPILE and cross_answers_incomplete:
        raise Utils.WafError("Cross answers file %s is incomplete" % conf.env.CROSS_ANSWERS)
    return True
