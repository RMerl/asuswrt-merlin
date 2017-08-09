#!/usr/bin/env python
"""
ntlogon.py written by Timothy (rhacer) Grant

Copyright 1999 - 2002 by Timothy Grant 

is distributed under the terms of the GNU Public License.

The format for the configuration file is as follows:

While there is some room for confusion, we attempt to process things in
order of specificity: Global first, Group second, User third, OS Type
forth. This order can be debated forever, but it seems to make the most
sense.

# Everything in the Global section applies to all users logging on to the
# network
[Global]
@ECHO "Welcome to our network!!!"
NET TIME \\\\servername /SET /YES
NET USE F: \\\\servername\\globalshare /YES

# Map the private user area in the global section so we don't have to
# create individual user entries for each user!
NET USE U: \\\\servername\\%U /YES

# Group entries, User entries and OS entries each start with the
# keyword followed by a dash followed by--appropriately enough the Group
# name, the User name, or the OS name.
[Group-admin]
@ECHO "Welcome administrators!"
NET USE G: \\\\servername\\adminshare1 /YES
NET USE I: \\\\servername\\adminshare2 /YES

[Group-peons]
@ECHO "Be grateful we let you use computers!"
NET USE G: \\\\servername\\peonshare1 /YES

[Group-hackers]
@ECHO "What can I do for you today great one?"
NET USE G: \\\\servername\\hackershare1 /YES
NET USE I: \\\\servername\\adminshare2 /YES

[User-fred]
@ECHO "Hello there Fred!"
NET USE F: \\\\servername\\fredsspecialshare /YES

[OS-WfWg]
@ECHO "Time to upgrade it?"

# End configuration file

usage: ntlogon [-g | --group=groupname] 
               [-u | --user=username]
               [-o | --os=osname]
               [-m | --machine=netbiosname]
               [-f | --templatefile=filename]
               [-d | --dir=netlogon directory]
               [-v | --version]
               [-h | --help]
               [--pause]
               [--debug]
""" 
#   
#" This quote mark is an artifact of the inability of my editor to
#  correctly colour code anything after the triple-quoted docstring.
#  if your editor does not have this flaw, feel free to remove it.


import sys
import getopt
import re
import string
import os

version = "ntlogon.py v0.8"

def buildScript(buf, sections, group, user, ostype, machine, debug, pause):
    """
    buildScript() Takes the contents of the template file and builds
    a DOS batch file to be executed as an NT logon script. It does this
    by determining which sections of the configuration file should be included
    and creating a list object that contains each line contained in each
    included section.  The list object is then returned to the calling 
    routine.

    All comments (#) are removed. A REM is inserted to show
    which section of the configuration file each line comes from.
    We leave blanklines as they are sometimes useful for debugging

    We also replace all of the Samba macros (e.g., %U, %G, %a, %m) with their
    expanded versions which have been passed to us by smbd
    """
    hdrstring   = ''
    script = []

    #
    # These are the Samba macros that we currently know about.
    # any user defined macros will also be added to this dictionary.
    # We do not store the % sign as part of the macro name.
    # The replace routine will prepend the % sign to all possible
    # replacements.
    # 
    macros = {
        		'U': user,
                'G': group,
                'a': ostype,
                'm': machine
             }

    #
    # Process each section defined in the list sections
    #
    for s in sections:
        # print 'searching for: ' + s

        idx = 0

        while idx < len(buf):
            ln = buf[idx]

            #
            # We need to set up a regex for each possible section we
            # know about. This is slightly complicated due to the fact
            # that section headers contain user defined text.
            #
            if s == 'Global':
                hdrstring = '\[ *' + s + ' *\]'
            elif s == 'Group':
                hdrstring = '\[ *' + s + ' *- *' + group + ' *\]'
            elif s == 'User':
                hdrstring = '\[ *' + s + ' *- *' + user + ' *\]'
            elif s == 'OS':
                hdrstring = '\[ *' + s + ' *- *' + ostype + ' *\]'
            elif s == 'Machine':
	        hdrstring = '\[ *' + s + ' *- *' + machine + ' *\]'

            #
            # See if we have found a section header
            #
            if re.search(r'(?i)' + hdrstring, ln):
                idx = idx + 1   # increment the counter to move to the next
                                # line.

                x = re.match(r'([^#\r\n]*)', ln)    # Determine the section
                                                    # name and strip out CR/LF
                                                    # and comment information

                if debug:
                    print 'rem ' + x.group(1) + ' commands'
                else:
                    # create the rem at the beginning of each section of the
                    # logon script.
                    script.append('rem ' + x.group(1) + ' commands') 

                #
                # process each line until we have found another section
                # header
                #
                while not re.search(r'.*\[.*\].*', buf[idx]):

                    #
                    # strip comments and line endings
                    #
                    x = re.match(r'([^#\r\n]*)', buf[idx])

                    if string.strip(x.group(1)) != '' :
                        # if there is still content  after stripping comments and
                        # line endings then this is a line to process

                        line = x.group(1)

                        #
                        # Check to see if this is a macro definition line
                        #
                        vardef = re.match(r'(.*)=(.*)', line)

                        if vardef:
                            varname = string.strip(vardef.group(1))		# Strip leading and
                            varsub  = string.strip(vardef.group(2))		# and trailing spaces

                            if varname == '':
                                print "Error: No substition name specified line: %d" % idx
                                sys.exit(1)

                            if varsub == '':
                                print "Error: No substitution text provided line: %d" % idx
                                sys.exit(1)

                            if macros.has_key(varname):
                                print "Warning: macro %s redefined line: %d" % (varname, idx)

                            macros[varname] = varsub
                            idx = idx + 1
                            continue

                        #
                        # Replace all the  macros that we currently
                        # know about.
                        #
                        # Iterate over the dictionary that contains all known
                        # macro substitutions.
                        #
                        # We test for a macro name by prepending % to each dictionary
                        # key.
                        #
                        for varname in macros.keys():
                            line = re.sub(r'%' + varname + r'(\W)',
                                          macros[varname] + r'\1', line)

                        if debug:
                            print line
                            if pause:
                                print 'pause'
                        else:
                            script.append(line)

                    idx = idx + 1

                    if idx == len(buf):
                        break   # if we have reached the end of the file
                                # stop processing.

            idx = idx + 1   # increment the line counter

        if debug:
            print ''
        else:
            script.append('')

    return script

# End buildScript()

def run():
    """
    run() everything starts here. The main routine reads the command line
    arguments, opens and reads the configuration file.
    """
    configfile  = '/etc/ntlogon.conf'   # Default configuration file
    group       = ''                    # Default group
    user        = ''                    # Default user
    ostype      = ''                    # Default os
    machine     = ''			# Default machine type
    outfile     = 'logon.bat'           # Default batch file name
                                        #   this file name WILL take on the form
                                        #   username.bat if a username is specified
    debug       = 0                     # Default debugging mode
    pause		= 0						# Default pause mode
    outdir      = '/usr/local/samba/netlogon/'   # Default netlogon directory

    sections    = ['Global', 'Machine', 'OS', 'Group', 'User'] # Currently supported
                                                    # configuration file 
                                                    # sections

    options, args = getopt.getopt(sys.argv[1:], 'd:f:g:ho:u:m:v', 
                                 ['templatefile=', 
                                  'group=',
                                  'help',
                                  'os=',
                                  'user=',
                                  'machine=',
                                  'dir=',
                                  'version',
                                  'pause',
                                  'debug'])

    #
    # Process the command line arguments
    #
    for i in options:
        # template file to process
        if (i[0] == '-f') or (i[0] == '--templatefile'):
            configfile = i[1]
            # print 'configfile = ' + configfile

        # define the group to be used
        elif (i[0] == '-g') or (i[0] == '--group'):
            group = i[1]
            # print 'group = ' + group

        # define the os type
        elif (i[0] == '-o') or (i[0] == '--os'):
            ostype = i[1]
            # print 'os = ' + os

        # define the user
        elif (i[0] == '-u') or (i[0] == '--user'):
            user = i[1]
            outfile = user + '.bat' # Setup the output file name
            # print 'user = ' + user

        # define the machine
        elif (i[0] == '-m') or (i[0] == '--machine'):
            machine = i[1]

        # define the netlogon directory
        elif (i[0] == '-d') or (i[0] == '--dir'):
            outdir = i[1]
            # print 'outdir = ' + outdir

        # if we are asked to turn on debug info, do so.
        elif (i[0] == '--debug'):
            debug = 1
            # print 'debug = ' + debug

        # if we are asked to turn on the automatic pause functionality, do so
        elif (i[0] == '--pause'):
            pause = 1
            # print 'pause = ' + pause

        # if we are asked for the version number, print it.
        elif (i[0] == '-v') or (i[0] == '--version'):
            print version
            sys.exit(0)

        # if we are asked for help print the docstring.
        elif (i[0] == '-h') or (i[0] == '--help'):
            print __doc__
            sys.exit(0)

    #
    # open the configuration file
    #    
    try:
        iFile = open(configfile, 'r')
    except IOError:
        print 'Unable to open configuration file: ' + configfile
        sys.exit(1)


    #
    # open the output file
    #    
    if not debug:
        try:
            oFile = open(outdir + outfile, 'w')
        except IOError:
            print 'Unable to open logon script file: ' + outdir + outfile
            sys.exit(1)

    buf = iFile.readlines() # read in the entire configuration file

    #
    # call the script building routine
    #
    script = buildScript(buf, sections, group, user, ostype, machine, debug, pause)

    #
    # write out the script file
    #
    if not debug:
        for ln in script:
            oFile.write(ln + '\r\n')
            if pause:
                if string.strip(ln) != '':			# Because whitespace
                    oFile.write('pause' + '\r\n')	# is a useful tool, we
                    								# don't put pauses after
                                                    # an empty line.


# End run()

#
# immediate-mode commands, for drag-and-drop or execfile() execution
#
if __name__ == '__main__':
    run()
else:
    print "Module ntlogon.py imported."
    print "To run, type: ntlogon.run()"
    print "To reload after changes to the source, type: reload(ntlogon)"

#
# End NTLogon.py
#
