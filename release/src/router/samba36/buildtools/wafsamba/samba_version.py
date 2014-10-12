import os
import Utils
import samba_utils
import sys

def bzr_version_summary(path):
    try:
        import bzrlib
    except ImportError:
        return ("BZR-UNKNOWN", {})

    import bzrlib.ui
    bzrlib.ui.ui_factory = bzrlib.ui.make_ui_for_terminal(
        sys.stdin, sys.stdout, sys.stderr)
    from bzrlib import branch, osutils, workingtree
    from bzrlib.plugin import load_plugins
    load_plugins()

    b = branch.Branch.open(path)
    (revno, revid) = b.last_revision_info()
    rev = b.repository.get_revision(revid)

    fields = {
        "BZR_REVISION_ID": revid,
        "BZR_REVNO": revno,
        "COMMIT_DATE": osutils.format_date_with_offset_in_original_timezone(rev.timestamp,
            rev.timezone or 0),
        "COMMIT_TIME": int(rev.timestamp),
        "BZR_BRANCH": rev.properties.get("branch-nick", ""),
        }

    # If possible, retrieve the git sha
    try:
        from bzrlib.plugins.git.object_store import get_object_store
    except ImportError:
        # No git plugin
        ret = "BZR-%d" % revno
    else:
        store = get_object_store(b.repository)
        full_rev = store._lookup_revision_sha1(revid)
        fields["GIT_COMMIT_ABBREV"] = full_rev[:7]
        fields["GIT_COMMIT_FULLREV"] = full_rev
        ret = "GIT-" + fields["GIT_COMMIT_ABBREV"]

    if workingtree.WorkingTree.open(path).has_changes():
        fields["COMMIT_IS_CLEAN"] = 0
        ret += "+"
    else:
        fields["COMMIT_IS_CLEAN"] = 1
    return (ret, fields)


def git_version_summary(path, env=None):
    # Get version from GIT
    if not 'GIT' in env and os.path.exists("/usr/bin/git"):
        # this is useful when doing make dist without configuring
        env.GIT = "/usr/bin/git"

    if not 'GIT' in env:
        return ("GIT-UNKNOWN", {})

    environ = dict(os.environ)
    environ["GIT_DIR"] = '%s/.git' % path
    environ["GIT_WORK_TREE"] = path
    git = Utils.cmd_output(env.GIT + ' show --pretty=format:"%h%n%ct%n%H%n%cd" --stat HEAD', silent=True, env=environ)

    lines = git.splitlines()
    if not lines or len(lines) < 4:
        return ("GIT-UNKNOWN", {})

    fields = {
            "GIT_COMMIT_ABBREV": lines[0],
            "GIT_COMMIT_FULLREV": lines[2],
            "COMMIT_TIME": int(lines[1]),
            "COMMIT_DATE": lines[3],
            }

    ret = "GIT-" + fields["GIT_COMMIT_ABBREV"]

    if env.GIT_LOCAL_CHANGES:
        clean = Utils.cmd_output('%s diff HEAD | wc -l' % env.GIT, silent=True).strip()
        if clean == "0":
            fields["COMMIT_IS_CLEAN"] = 1
        else:
            fields["COMMIT_IS_CLEAN"] = 0
            ret += "+"

    return (ret, fields)


class SambaVersion(object):

    def __init__(self, version_dict, path, env=None):
        '''Determine the version number of samba

See VERSION for the format.  Entries on that file are 
also accepted as dictionary entries here
        '''

        self.MAJOR=None
        self.MINOR=None
        self.RELEASE=None
        self.REVISION=None
        self.TP_RELEASE=None
        self.ALPHA_RELEASE=None
        self.PRE_RELEASE=None
        self.RC_RELEASE=None
        self.IS_SNAPSHOT=True
        self.RELEASE_NICKNAME=None
        self.VENDOR_SUFFIX=None
        self.VENDOR_PATCH=None

        for a, b in version_dict.iteritems():
            if a.startswith("SAMBA_VERSION_"):
                setattr(self, a[14:], b)
            else:
                setattr(self, a, b)

        if self.IS_GIT_SNAPSHOT == "yes":
            self.IS_SNAPSHOT=True
        elif self.IS_GIT_SNAPSHOT == "no":
            self.IS_SNAPSHOT=False
        else:
            raise Exception("Unknown value for IS_GIT_SNAPSHOT: %s" % self.IS_GIT_SNAPSHOT)

 ##
 ## start with "3.0.22"
 ##
        self.MAJOR=int(self.MAJOR)
        self.MINOR=int(self.MINOR)
        self.RELEASE=int(self.RELEASE)

        SAMBA_VERSION_STRING = ("%u.%u.%u" % (self.MAJOR, self.MINOR, self.RELEASE))

##
## maybe add "3.0.22a" or "4.0.0tp11" or "4.0.0alpha1" or "3.0.22pre1" or "3.0.22rc1"
## We do not do pre or rc version on patch/letter releases
##
        if self.REVISION is not None:
            SAMBA_VERSION_STRING += self.REVISION
        if self.TP_RELEASE is not None:
            self.TP_RELEASE = int(self.TP_RELEASE)
            SAMBA_VERSION_STRING += "tp%u" % self.TP_RELEASE
        if self.ALPHA_RELEASE is not None:
            self.ALPHA_RELEASE = int(self.ALPHA_RELEASE)
            SAMBA_VERSION_STRING += ("alpha%u" % self.ALPHA_RELEASE)
        if self.PRE_RELEASE is not None:
            self.PRE_RELEASE = int(self.PRE_RELEASE)
            SAMBA_VERSION_STRING += ("pre%u" % self.PRE_RELEASE)
        if self.RC_RELEASE is not None:
            self.RC_RELEASE = int(self.RC_RELEASE)
            SAMBA_VERSION_STRING += ("rc%u" % self.RC_RELEASE)

        if self.IS_SNAPSHOT:
            if os.path.exists(os.path.join(path, ".git")):
                suffix, self.vcs_fields = git_version_summary(path, env=env)
            elif os.path.exists(os.path.join(path, ".bzr")):
                suffix, self.vcs_fields = bzr_version_summary(path)
            else:
                suffix = "UNKNOWN"
                self.vcs_fields = {}
            SAMBA_VERSION_STRING += "-" + suffix
        else:
            self.vcs_fields = {}

        self.OFFICIAL_STRING = SAMBA_VERSION_STRING

        if self.VENDOR_SUFFIX is not None:
            SAMBA_VERSION_STRING += ("-" + self.VENDOR_SUFFIX)
            self.VENDOR_SUFFIX = self.VENDOR_SUFFIX

            if self.VENDOR_PATCH is not None:
                SAMBA_VERSION_STRING += ("-" + self.VENDOR_PATCH)
                self.VENDOR_PATCH = self.VENDOR_PATCH

        self.STRING = SAMBA_VERSION_STRING

        if self.RELEASE_NICKNAME is not None:
            self.STRING_WITH_NICKNAME = "%s (%s)" % (self.STRING, self.RELEASE_NICKNAME)
        else:
            self.STRING_WITH_NICKNAME = self.STRING

    def __str__(self):
        string="/* Autogenerated by waf */\n"
        string+="#define SAMBA_VERSION_MAJOR %u\n" % self.MAJOR
        string+="#define SAMBA_VERSION_MINOR %u\n" % self.MINOR
        string+="#define SAMBA_VERSION_RELEASE %u\n" % self.RELEASE
        if self.REVISION is not None:
            string+="#define SAMBA_VERSION_REVISION %u\n" % self.REVISION

        if self.TP_RELEASE is not None:
            string+="#define SAMBA_VERSION_TP_RELEASE %u\n" % self.TP_RELEASE

        if self.ALPHA_RELEASE is not None:
            string+="#define SAMBA_VERSION_ALPHA_RELEASE %u\n" % self.ALPHA_RELEASE

        if self.PRE_RELEASE is not None:
            string+="#define SAMBA_VERSION_PRE_RELEASE %u\n" % self.PRE_RELEASE

        if self.RC_RELEASE is not None:
            string+="#define SAMBA_VERSION_RC_RELEASE %u\n" % self.RC_RELEASE

        for name in sorted(self.vcs_fields.keys()):
            string+="#define SAMBA_VERSION_%s " % name
            value = self.vcs_fields[name]
            if isinstance(value, basestring):
                string += "\"%s\"" % value
            elif type(value) is int:
                string += "%d" % value
            else:
                raise Exception("Unknown type for %s: %r" % (name, value))
            string += "\n"

        string+="#define SAMBA_VERSION_OFFICIAL_STRING \"" + self.OFFICIAL_STRING + "\"\n"

        if self.VENDOR_SUFFIX is not None:
            string+="#define SAMBA_VERSION_VENDOR_SUFFIX " + self.VENDOR_SUFFIX + "\n"
            if self.VENDOR_PATCH is not None:
                string+="#define SAMBA_VERSION_VENDOR_PATCH " + self.VENDOR_PATCH + "\n"

        if self.RELEASE_NICKNAME is not None:
            string+="#define SAMBA_VERSION_RELEASE_NICKNAME " + self.RELEASE_NICKNAME + "\n"

        # We need to put this #ifdef in to the headers so that vendors can override the version with a function
        string+='''
#ifdef SAMBA_VERSION_VENDOR_FUNCTION
#  define SAMBA_VERSION_STRING SAMBA_VERSION_VENDOR_FUNCTION
#else /* SAMBA_VERSION_VENDOR_FUNCTION */
#  define SAMBA_VERSION_STRING "''' + self.STRING_WITH_NICKNAME + '''"
#endif
'''
        string+="/* Version for mkrelease.sh: \nSAMBA_VERSION_STRING=" + self.STRING_WITH_NICKNAME + "\n */\n"

        return string


def samba_version_file(version_file, path, env=None):
    '''Parse the version information from a VERSION file'''

    f = open(version_file, 'r')
    version_dict = {}
    for line in f:
        line = line.strip()
        if line == '':
            continue
        if line.startswith("#"):
            continue
        try:
            split_line = line.split("=")
            if split_line[1] != "":
                value = split_line[1].strip('"')
                version_dict[split_line[0]] = value
        except:
            print("Failed to parse line %s from %s" % (line, version_file))
            raise

    return SambaVersion(version_dict, path, env=env)



def load_version(env=None):
    '''load samba versions either from ./VERSION or git
    return a version object for detailed breakdown'''
    if not env:
        env = samba_utils.LOAD_ENVIRONMENT()

    version = samba_version_file("./VERSION", ".", env)
    Utils.g_module.VERSION = version.STRING
    return version
