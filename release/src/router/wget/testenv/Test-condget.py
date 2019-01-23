#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    Simple test for HTTP Conditional-GET Requests using the -N command
"""
############# File Definitions ###############################################
# Keep same length !
Cont1 = """THIS IS 1 FILE"""
Cont2 = """THIS IS 2 FILE"""
Cont3 = """THIS IS 3 FILE"""
Cont4 = """THIS IS 4 FILE"""

# Local Wget files

# These have same timestamp as remote files
UpToDate_Local_File1 = WgetFile ("UpToDateFile1", Cont1, timestamp="1995-01-01 00:00:00")
UpToDate_Local_File2 = WgetFile ("UpToDateFile2", Cont1, timestamp="1995-01-01 00:00:00")
UpToDate_Local_File3 = WgetFile ("UpToDateFile3", Cont1, timestamp="1995-01-01 00:00:00")

# This is newer than remote (expected same behaviour as for above files)
Newer_Local_File = WgetFile ("NewerFile", Cont1, timestamp="1995-02-02 02:02:02")

# This is older than remote - should be clobbered
Outdated_Local_File = WgetFile ("UpdatedFile",  Cont2, timestamp="1990-01-01 00:00:00")

UpToDate_Rules1 = {
    "SendHeader" : {
        # RFC1123 format
        "Last-Modified" : "Sun, 01 Jan 1995 00:00:00 GMT",
    },
    "Response": 304,
    "ExpectHeader" : {
        "If-Modified-Since" : "Sun, 01 Jan 1995 00:00:00 GMT"
    },
}

UpToDate_Rules2 = {
    "SendHeader" : {
        # RFC850 format
        "Last-Modified" : "Sunday, 01-Jan-95 00:00:00 GMT",
    },
    "Response": 304,
    "ExpectHeader" : {
        "If-Modified-Since" : "Sun, 01 Jan 1995 00:00:00 GMT"
    },
}

UpToDate_Rules3 = {
    "SendHeader" : {
        # Asctime format
        "Last-Modified" : "Sun Jan 01 00:00:00 1995",
    },
    "Response": 304,
    "ExpectHeader" : {
        "If-Modified-Since" : "Sun, 01 Jan 1995 00:00:00 GMT"
    },
}

Newer_Rules = {
    "SendHeader" : {
        # Asctime format
        "Last-Modified" : "Sun Jan 01 00:00:00 1995",
    },
    "Response": 304,
    "ExpectHeader" : {
        "If-Modified-Since" : "Thu, 02 Feb 1995 02:02:02 GMT"
    },
}

Outdated_Rules = {
    "SendHeader" : {
        # RFC850 format
        "Last-Modified" : "Thursday, 01-Jan-15 00:00:00 GMT",
    },
    "ExpectHeader" : {
        "If-Modified-Since" : "Mon, 01 Jan 1990 00:00:00 GMT",
    },
}

UpToDate_Server_File1 = WgetFile ("UpToDateFile1", Cont3, rules=UpToDate_Rules1)
UpToDate_Server_File2 = WgetFile ("UpToDateFile2", Cont3, rules=UpToDate_Rules2)
UpToDate_Server_File3 = WgetFile ("UpToDateFile3", Cont3, rules=UpToDate_Rules3)
Newer_Server_File = WgetFile ("NewerFile", Cont3, rules=Newer_Rules)
Updated_Server_File = WgetFile  ("UpdatedFile",  Cont4, rules=Outdated_Rules)

WGET_OPTIONS = "-N"
WGET_URLS = [["UpToDateFile1", "UpToDateFile2", "UpToDateFile3", "NewerFile",
              "UpdatedFile", ]]

Files = [[UpToDate_Server_File1, UpToDate_Server_File2, UpToDate_Server_File3,
          Newer_Server_File, Updated_Server_File, ]]

Existing_Files = [UpToDate_Local_File1, UpToDate_Local_File2,
                  UpToDate_Local_File3, Newer_Local_File, Outdated_Local_File]

ExpectedReturnCode = 0

# The uptodate file should not be downloaded
ExpectedDownloadedFiles = [UpToDate_Local_File1, UpToDate_Local_File2,
                           UpToDate_Local_File3, Newer_Local_File,
                           Updated_Server_File]

# Kind of hack to ensure proper request types
Request_List = [
    [
        "GET /UpToDateFile1",
        "GET /UpToDateFile2",
        "GET /UpToDateFile3",
        "GET /NewerFile",
        "GET /UpdatedFile",
    ]
]

################ Pre and Post Test Hooks #####################################
pre_test = {
    "ServerFiles"       : Files,
    "LocalFiles"        : Existing_Files
}
test_options = {
    "WgetCommands"      : WGET_OPTIONS,
    "Urls"              : WGET_URLS
}
post_test = {
    "ExpectedFiles"     : ExpectedDownloadedFiles,
    "ExpectedRetcode"   : ExpectedReturnCode,
    "FilesCrawled"      : Request_List,
}

err = HTTPTest (
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test
).begin ()

exit (err)
