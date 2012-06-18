The mdnsNSP is a NameSpace Provider. It hooks into the Windows name resolution infrastructure to allow any software using the standard Windows APIs for resolving names to work with DNS-SD. For example, when the mdnsNSP is installed, you can type "http://computer.local./" in Internet Explorer and it will resolve "computer.local." using DNS-SD and go to the web site (assuming you have a computer named "computer" on the local network and advertised via DNS-SD).

NSP's are implemented DLLs and must be installed to work. NSP DLLs export an NSPStartup function, which is called when the NSP is used, and NSPStartup provides information about itself (e.g. version number, compatibility information, and a list of function pointers for each of the supported NSP routines).

If you need to register the mdnsNSP, you can use the NSPTool (sources for it are provided along with the mdnsNSP) with the following line from the DOS command line prompt ("<path>" is the actual parent path of the DLL):

NSPTool -install "mdnsNSP" "B600E6E9-553B-4a19-8696-335E5C896153" "<path>"

You can remove remove the mdnsNSP with the following line:

NSPTool -remove "B600E6E9-553B-4a19-8696-335E5C896153"

For more information, check out the following URL:

<http://msdn.microsoft.com/library/default.asp?url=/library/en-us/winsock/winsock/name_space_service_providers_2.asp>
