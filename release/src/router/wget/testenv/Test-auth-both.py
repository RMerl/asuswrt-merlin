#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    This test ensures Wget's Basic Authorization Negotiation.
    Also, we ensure that Wget saves the host after a successful auth and
    doesn't wait for a challenge the second time.
"""
############# File Definitions ###############################################
File1 = "Would you like some Tea?"
File2 = "With lemon or cream?"
File3 = "Sure you're joking Mr. Feynman"

File1_rules = {
    "Authentication"    : {
        "Type"          : "Both",
        "User"          : "Sauron",
        "Pass"          : "TheEye",
        "Parm"          : {
            "qop"       : "auth"
        }
    },
    "RejectHeader"      : {
        "Authorization" : "Basic U2F1cm9uOlRoZUV5ZQ=="
    }
}
File2_rules = {
    "Authentication"    : {
        "Type"          : "Both_inline",
        "User"          : "Sauron",
        "Pass"          : "TheEye",
        "Parm"          : {
            "qop"       : "auth"
        }
    },
    "RejectHeader"      : {
        "Authorization" : "Basic U2F1cm9uOlRoZUV5ZQ=="
    }
}
File3_rules = {
    "Authentication"    : {
        "Type"          : "Digest",
        "User"          : "Sauron",
        "Pass"          : "TheEye",
        "Parm"          : {
            "qop"       : "auth"
        }

    }
}

A_File = WgetFile ("File1", File1, rules=File1_rules)
B_File = WgetFile ("File2", File2, rules=File2_rules)
C_File = WgetFile ("File3", File3, rules=File3_rules)

WGET_OPTIONS = "--user=Sauron --password=TheEye"
WGET_URLS = [["File1", "File2", "File3"]]

Files = [[A_File, B_File, C_File]]

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [A_File, B_File, C_File]

################ Pre and Post Test Hooks #####################################
pre_test = {
    "ServerFiles"       : Files
}
test_options = {
    "WgetCommands"      : WGET_OPTIONS,
    "Urls"              : WGET_URLS
}
post_test = {
    "ExpectedFiles"     : ExpectedDownloadedFiles,
    "ExpectedRetcode"   : ExpectedReturnCode
}

err = HTTPTest (
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test
).begin ()

exit (err)
