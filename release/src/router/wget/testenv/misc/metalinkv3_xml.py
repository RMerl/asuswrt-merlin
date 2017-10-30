from test.http_test import HTTPTest
from misc.wget_file import WgetFile
import hashlib

class Metalinkv3_XML:

    """ Metalink/XML v3 object """

    # Initialize the Metalink object
    def __init__ (self):
        self.reset ()

    # Reset the Metalink object
    def reset (self):
        self.LocalFiles = []        # list of WgetFile objects
        self.ServerFiles = [[]]     # list of WgetFile objects
        self.ExpectedFiles = []     # list of WgetFile objects
        self.LocalFiles_Set = []    # used as `list (set (var))`
        self.ServerFiles_Set = [[]] # used as `list (set (var))`
        self.ExpectedFiles_Set = [] # used as `list (set (var))`
        self.Xml = ''               # Metalink/XML content
        self.XmlName = ''           # Metalink/XML file name
        self.XmlFile = None         # Metalink/XML WgetFile object
        self.Xml_Header = '<?xml version="1.0" encoding="utf-8"?>\n' + \
                          '<metalink version="3.0" xmlns="http://www.metalinker.org/">\n' + \
                          '  <publisher>\n' + \
                          '    <name>GNU Wget</name>\n' + \
                          '  </publisher>\n' + \
                          '  <license>\n' + \
                          '    <name>GNU GPL</name>\n' + \
                          '    <url>http://www.gnu.org/licenses/gpl.html</url>\n' + \
                          '  </license>\n' + \
                          '  <identity>Wget Test Files</identity>\n' + \
                          '  <version>1.2.3</version>\n' + \
                          '  <description>Wget Test Files description</description>\n' + \
                          '  <files>\n'
        self.Xml_Footer = '  </files>\n' + \
                          '</metalink>\n'

    # Print the Metalink object.
    def print_meta (self):

        print (self.Xml)
        print ("LocalFiles = " + str (self.LocalFiles_Set))
        print ("ServerFiles = " + str (self.ServerFiles_Set))
        print ("ExpectedFiles = " + str (self.ExpectedFiles_Set))

    # Add LocalFiles as WgetFile objects
    #
    # ["file_name", "content"],
    # ["file_name", "content"]
    def add_LocalFiles (self, *local_files):

        for (file_name, content) in local_files:
            if not file_name in self.LocalFiles_Set:
                self.LocalFiles_Set.append (file_name)
                self.LocalFiles.append (WgetFile (file_name, content))

    # Add ServerFiles as WgetFile objects
    #
    # ["file_name", "content"],
    # ["file_name", "content"]
    def add_ServerFiles (self, *server_files):

        for (file_name, content) in server_files:
            if not file_name in self.ServerFiles_Set[0]:
                self.ServerFiles_Set[0].append (file_name)
                self.ServerFiles[0].append (WgetFile (file_name, content))

    # Add ExpectedFiles as WgetFile objects
    #
    # ["file_name", "content"],
    # ["file_name", "content"]
    def add_ExpectedFiles (self, *expected_files):

        for (file_name, content) in expected_files:
            if not file_name in self.ExpectedFiles_Set:
                self.ExpectedFiles_Set.append (file_name)
                self.ExpectedFiles.append (WgetFile (file_name, content))

    # Run a Wget HTTP test for the Metalink object.
    def http_test (self, command_line, expected_retcode):

        pre_test = {
            "ServerFiles"     : self.ServerFiles, # list of WgetFile objects as [[]]
            "LocalFiles"      : self.LocalFiles,  # list of WgetFile objects as []
        }

        test_options = {
            "WgetCommands"    : command_line, # Wget cli
            "Urls"            : [[]],         # Wget urls
        }

        post_test = {
            "ExpectedFiles"   : self.ExpectedFiles, # list of WgetFile objects as []
            "ExpectedRetcode" : expected_retcode,   # Wget return status code
        }

        http_test = HTTPTest (
            pre_hook=pre_test,
            test_params=test_options,
            post_hook=post_test,
        )

        http_test.server_setup()
        # Get and use dynamic server sockname
        srv_host, srv_port = http_test.servers[0].server_inst.socket.getsockname ()

        self.set_srv (srv_host, srv_port)

        err = http_test.begin ()

        return err

    # Set the Wget server host and port in the Metalink/XML content.
    def set_srv (self, srv_host, srv_port):

        self.Xml = self.Xml.replace('{{SRV_HOST}}', srv_host)
        self.Xml = self.Xml.replace('{{SRV_PORT}}', str (srv_port))

        if self.XmlFile is not None:
            self.XmlFile.content = self.Xml

    # Create the Metalink/XML file.
    #
    # Add the Metalink/XML file to the list of ExpectedFiles.
    #
    # size:
    #   True     auto-compute size
    #   None     no <size></size>
    #    any     use this size
    #
    # hash_sha256:
    #   False    no <verification></verification>
    #   True     auto-compute sha256
    #   None     no <hash></hash>
    #    any     use this hash
    #
    # ARGUMENTS:
    #
    # "xml_name",                                                 # Metalink/XML file name
    # ["file_name", "save_name", "content", size, hash_sha256,    # metalink:file
    #  ["srv_file", "srv_content", utype, location, preference],  # resource
    #  ["srv_file", "srv_content", utype, location, preference]], # resource
    # ["file_name", "save_name", "content", size, hash_sha256,
    #  ["srv_file", "srv_content", utype, location, preference],
    #  ["srv_file", "srv_content", utype, location, preference]]
    def xml (self, xml_name, *xml_data):

        self.Xml = self.Xml_Header

        for (file_name, save_name, content, size, hash_sha256, *resources) in xml_data:
            self.Xml += self.file_tag (file_name, save_name, content, size, hash_sha256, resources) + '\n'

        self.Xml += self.Xml_Footer

        self.XmlName = xml_name
        self.XmlFile = WgetFile (xml_name, self.Xml)

        if not xml_name in self.LocalFiles_Set:
            self.LocalFiles_Set.append (xml_name)
            self.LocalFiles.append (self.XmlFile)

        if not xml_name in self.ExpectedFiles_Set:
            self.ExpectedFiles_Set.append (xml_name)
            self.ExpectedFiles.append (self.XmlFile)

    # Create the file tag.
    #
    # Add the file to be saved to the list of ExpectedFiles.
    #
    # size:
    #   True     auto-compute size
    #   None     no <size></size>
    #    any     use this size
    #
    # hash_sha256:
    #   False    no <verification></verification>
    #   True     auto-compute sha256
    #   None     no <hash></hash>
    #    any     use this hash
    #
    # ARGUMENTS:
    #
    # ["file_name", "save_name", "content", size, hash_sha256,    # metalink:file
    #  ["srv_file", "srv_content", utype, location, preference],  # resource
    #  ["srv_file", "srv_content", utype, location, preference]]  # resource
    def file_tag (self, file_name, save_name, content, size, hash_sha256, resources):

        Tag = '    <file name="' + file_name + '">\n'

        if save_name is not None:
            self.add_ExpectedFiles ([save_name, content])

        size_Tag = self.size_tag (content, size)

        if size_Tag is not None:
            Tag += size_Tag + '\n'

        verification_Tag = self.verification_tag (content, hash_sha256)

        if verification_Tag is not None:
            Tag += verification_Tag + '\n'

        Tag += self.resources_tag (resources) + '\n'

        Tag += '    </file>'

        return Tag

    # Create the size tag.
    #
    # size:
    #   True     auto-compute size
    #   None     no <size></size>
    #    any     use this size
    #
    # ARGUMENTS:
    #
    # "content", size
    def size_tag (self, content = None, size = None):

        Tag = None

        if content is not None and size is True:
            size = len (content)

        if size is not None:
            Tag = '      <size>' + str (size) + '</size>'

        return Tag

    # Create the verification tag.
    #
    # hash_sha256:
    #   False    no <verification></verification>
    #   True     auto-compute sha256
    #   None     no <hash></hash>
    #    any     use this hash
    #
    # ARGUMENTS:
    #
    # "content", hash_sha256
    def verification_tag (self, content = None, hash_sha256 = None):

        Tag = None

        if hash_sha256 is not False:

            if content is not None and hash_sha256 is True:
                hash_sha256 = hashlib.sha256 (content.encode ('UTF-8')).hexdigest ()

            if hash_sha256 is None:
                Tag = '      <verification>\n' + \
                      '      </verification>'
            else:
                Tag = '      <verification>\n' + \
                      '        <hash type="sha256">' + str (hash_sha256) + '</hash>\n' + \
                      '      </verification>'

        return Tag

    # Create the resources tag.
    #
    # ARGUMENTS:
    #
    # ["srv_file", "srv_content", utype, location, preference],   # resource
    # ["srv_file", "srv_content", utype, location, preference]    # resource
    def resources_tag (self, resources):

        Tag = '      <resources>\n'

        for (srv_file, srv_content, utype, location, preference) in resources:
            Tag += self.url_tag (srv_file, srv_content, utype, location, preference) + '\n'

        Tag += '      </resources>'

        return Tag

    # Create the url tag.
    #
    # Add the file to the list of Files when there is a content.
    #
    # ARGUMENTS:
    #
    # "srv_file", "srv_content", utype, location, preference      # resource
    def url_tag (self, srv_file, srv_content = None, utype = "http", location = None, preference = 999999):

        Loc = ''

        if location is not None:
            Loc = 'location="' + location + '" '

        Tag = '        ' + \
              '<url ' + \
              'type="' + utype + '" ' + \
              Loc + \
              'preference="' + str (preference) + '">' + \
              'http://{{SRV_HOST}}:{{SRV_PORT}}/' + srv_file + \
               '</url>'

        if srv_content is not None:
            self.add_ServerFiles ([srv_file, srv_content])

        return Tag
