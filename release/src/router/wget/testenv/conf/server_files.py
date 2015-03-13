from conf import hook

""" Pre-Test Hook: ServerFiles
This hook is used to define a set of files on the server's virtual filesystem.
server_files is expected to be dictionary that maps filenames to their
contents. In the future, this can be used to add additional metadat to the
files using the WgetFile class too.

This hook also does some additional processing on the contents of the file. Any
text between {{and}} is replaced by the contents of a class variable of the
same name. This is useful in creating files that contain an absolute link to
another file on the same server. """


@hook()
class ServerFiles:
    def __init__(self, server_files):
        self.server_files = server_files

    def __call__(self, test_obj):
        for server, files in zip(test_obj.servers, self.server_files):
            rules = {f.name: test_obj.get_server_rules(f)
                     for f in files}
            files = {f.name: test_obj._replace_substring(f.content)
                     for f in files}
            server.server_conf(files, rules)
