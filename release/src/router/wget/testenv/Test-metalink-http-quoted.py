#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile
import hashlib
from base64 import b64encode

"""
    This is to test Metalink/HTTP quoted values support in Wget.
"""

# Helper function for hostname, port and digest substitution
def SubstituteServerInfo (text, host, port, digest):
    text = text.replace('{{FILE1_HASH}}', digest)
    text = text.replace('{{SRV_HOST}}', host)
    text = text.replace('{{SRV_PORT}}', str (port))
    return text

############# File Definitions ###############################################
File1 = "Would you like some Tea?"
File1_corrupted = "Would you like some Coffee?"
File1_lowPref = "Do not take this"
File1_sha256 = b64encode (hashlib.sha256 (File1.encode ('UTF-8')).digest ()).decode ('ascii')
Signature = '''-----BEGIN PGP SIGNATURE-----
Version: GnuPG v1.0.7 (GNU/Linux)

This is no valid signature. But it should be downloaded.
The attempt to verify should fail but should not prevent
a successful metalink resource retrieval (the sig failure
should not be fatal).
-----END PGP SIGNATURE-----
'''
File2 = "No meta data for this file."

LinkHeaders = [
    # This file has low priority and should not be picked.
    "<http://{{SRV_HOST}}:{{SRV_PORT}}/File1_lowPref>; rel=\"duplicate\"; pri=\"9\"; geo=\"pl\"",
    # This file should be picked second, after hash failure.
    "<http://0.0.0.0/File1_try2_badconnection>; rel =\"duplicate\";pref; pri=\"7\"",
    # This signature download will fail.
    "<http://{{SRV_HOST}}:{{SRV_PORT}}/Sig2.asc>; rel=\"describedby\"; type=\"application/pgp-signature\"",
    # Two good signatures
    "<http://{{SRV_HOST}}:{{SRV_PORT}}/Sig.asc>; rel=\"describedby\"; type=\"application/pgp-signature\"",
    "<http://{{SRV_HOST}}:{{SRV_PORT}}/Sig.asc>; rel=\"describedby\"; type=\"application/pgp-signature\"",
    # Bad URL scheme
    "<invalid_url>; rel=\"duplicate\"; pri=\"4\"",
    # rel missing
    "<http://{{SRV_HOST}}:{{SRV_PORT}}/File1>; pri=\"1\"; pref",
    # invalid rel
    "<http://{{SRV_HOST}}:{{SRV_PORT}}/File1>; rel=\"strange\"; pri=\"4\"",
    # This file should be picked first, because it has the lowest pri among preferred.
    "<http://{{SRV_HOST}}:{{SRV_PORT}}/File1_try1_corrupted>; rel=\"duplicate\"; geo=\"su\"; pri=\"4\"; pref",
    # This file should NOT be picked third due to preferred location set to 'uk'
    "<http://{{SRV_HOST}}:{{SRV_PORT}}/File1_badgeo>; rel =\"duplicate\";pri=\"5\"",
    # This file should be picked as third try, and it should succeed
    "<http://{{SRV_HOST}}:{{SRV_PORT}}/File1_try3_ok>; rel=\'duplicate\'; pri=\"5\";geo=\"uk\""
    ]
DigestHeader = "SHA-256=\'{{FILE1_HASH}}\'"

# This will be filled as soon as we know server hostname and port
MetaFileRules = {'SendHeader' : {}}

FileOkServer = WgetFile ("File1_try3_ok", File1)
FileBadPref = WgetFile ("File1_lowPref", File1_lowPref)
FileBadHash = WgetFile ("File1_try1_corrupted", File1_corrupted)
MetaFile = WgetFile ("test.meta", rules=MetaFileRules)
# In case of Metalink over HTTP, the local file name is
# derived from the URL suffix.
FileOkLocal = WgetFile ("test.meta", File1)
SigFile = WgetFile ("Sig.asc", Signature)
FileNoMeta = WgetFile ("File2", File2)

WGET_OPTIONS = "--metalink-over-http --preferred-location=uk"
WGET_URLS = [["test.meta", "File2"]]

Files = [[FileOkServer, FileBadPref, FileBadHash, MetaFile, SigFile, FileNoMeta]]
Existing_Files = []

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [FileNoMeta, FileOkLocal]

RequestList = [
    [
        "HEAD /test.meta",
        "GET /Sig2.asc",
        "GET /Sig.asc",
        "GET /File1_try1_corrupted",
        "GET /File1_try3_ok",
        "HEAD /File2",
        "GET /File2",
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
    "FilesCrawled"      : RequestList,
}

http_test = HTTPTest (
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test,
)

http_test.server_setup()
srv_host, srv_port = http_test.servers[0].server_inst.socket.getsockname ()

MetaFileRules["SendHeader"] = {
        'Link': [ SubstituteServerInfo (LinkHeader, srv_host, srv_port, File1_sha256)
                    for LinkHeader in LinkHeaders ],
        'Digest': SubstituteServerInfo (DigestHeader, srv_host, srv_port, File1_sha256),
}

err = http_test.begin ()

exit (err)
