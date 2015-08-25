# $Id: get-footprint.py 1352 2007-06-08 01:41:25Z bennylp $
# 
# This file is used to generate PJSIP/PJMEDIA footprint report.
# To use this file, just run it in pjsip-apps/build directory, to
# produce footprint.txt and footprint.htm report files.
#
import os
import sys
import string
import time

compile_flags1 = [
    # Base
    ['BASE',			'Empty application size'],
    ['',			'Subtotal: Empty application size'],

    ['HAS_PJLIB', 		'Minimum PJLIB only'],

    # Subtotal
    ['',			'Subtotal'],

    # PJLIB-UTIL
    ['HAS_PJLIB_STUN',		'STUN client'],
    ['HAS_PJLIB_GETOPT',	'getopt() functionality'],
    
    # Subtotal
    ['',			'TOTAL']
]

compile_flags = [
    # Base
    ['BASE',			'Empty application size'],
    ['', 			'Subtotal: empty application size on this platform'],

    ['HAS_PJLIB', 		'PJLIB (pool, data structures, hash tables, ioqueue, socket, timer heap, etc.). ' +
			        'For targets that statically link application with LIBC, the size includes ' +
			        'various LIBC functions that are used by PJLIB.'],
    ['', 			'Subtotal: Application linked with PJLIB'],

    # PJLIB-UTIL
    ['HAS_PJLIB_STUN',		'PJLIB-UTIL STUN client'],
    ['HAS_PJLIB_GETOPT',	'PJLIB-UTIL getopt() functionality'],
    ['HAS_PJLIB_SCANNER',	'PJLIB-UTIL text scanner (needed by SIP parser)'],
    ['HAS_PJLIB_XML',		'PJLIB-UTIL tiny XML (parsing and API) (needs text scanner)'],
    ['HAS_PJLIB_DNS',		'PJLIB-UTIL DNS packet and parsing'],
    ['HAS_PJLIB_RESOLVER',	'PJLIB-UTIL Asynchronous DNS resolver/caching engine'],
    ['HAS_PJLIB_CRC32',		'PJLIB-UTIL CRC32 algorithm'],
    ['HAS_PJLIB_HMAC_MD5',	'PJLIB-UTIL HMAC-MD5 algorithm'],
    ['HAS_PJLIB_HMAC_SHA1',	'PJLIB-UTIL HMAC-SHA1 algorithm'],

    # PJSIP
    ['HAS_PJSIP_CORE_MSG_ELEM',	'PJSIP Core - Messaging Elements and Parsing (message, headers, SIP URI, TEL URI/RFC 3966, etc.)'],
    ['HAS_PJSIP_CORE',		'PJSIP Core - Endpoint (transport management, module management, event distribution, etc.)'],
    ['HAS_PJSIP_CORE_MSG_UTIL',	'PJSIP Core - Stateless operations, SIP SRV, server resolution and fail-over'],
    ['HAS_PJSIP_UDP_TRANSPORT',	'PJSIP UDP transport'],
    ['',			'Subtotal: A minimalistic SIP application (parsing, UDP transport+STUN, no transaction)'],
   
    ['HAS_PJSIP_TCP_TRANSPORT',	'PJSIP TCP transport'],
    ['HAS_PJSIP_TLS_TRANSPORT',	'PJSIP TLS transport'],
    ['HAS_PJSIP_INFO',		'PJSIP INFO support (RFC 2976) (no special treatment, thus the zero size)'],
    ['HAS_PJSIP_TRANSACTION',	'PJSIP transaction and stateful API'],
    ['HAS_PJSIP_AUTH_CLIENT',	'PJSIP digest authentication client'],
    ['HAS_PJSIP_UA_LAYER',	'PJSIP User agent layer and base dialog and usage management (draft-ietf-sipping-dialogusage-01)'],
    ['HAS_PJMEDIA_SDP',		'PJMEDIA SDP Parsing and API (RFC 2327), needed by SDP negotiator'],
    ['HAS_PJMEDIA_SDP_NEGOTIATOR','PJMEDIA SDP negotiator (RFC 3264), needed by INVITE session'],
    ['HAS_PJSIP_INV_SESSION',	'PJSIP INVITE session API'],
    ['HAS_PJSIP_REGC',		'PJSIP client registration API'],
    ['',			'Subtotal: Minimal SIP application with registration (including digest authentication)'],
    
    ['HAS_PJSIP_EVENT_FRAMEWORK','PJSIP Event/SUBSCRIBE framework, RFC 3265 (needed by call transfer, and presence)'],
    ['HAS_PJSIP_CALL_TRANSFER',	'PJSIP Call Transfer/REFER support (RFC 3515)'],
    ['',			'Subtotal: Minimal SIP application with call transfer'],


    ['HAS_PJSIP_PRESENCE',	'PJSIP Presence subscription, including PIDF/X-PIDF support (RFC 3856, RFC 3863, etc) (needs XML)'],
    ['HAS_PJSIP_MESSAGE',	'PJSIP Instant Messaging/MESSAGE support (RFC 3428) (no special treatment, thus the zero size)'],
    ['HAS_PJSIP_IS_COMPOSING',	'PJSIP Message Composition indication (RFC 3994)'],

    # Subtotal
    ['',			'Subtotal: Complete PJSIP package (call, registration, presence, IM) +STUN +GETOPT (+PJLIB), no media'],
    
    # PJNATH
    ['HAS_PJNATH_STUN',		'PJNATH STUN'],
    ['HAS_PJNATH_ICE',		'PJNATH ICE'],

    # PJMEDIA
    ['HAS_PJMEDIA_EC',		'PJMEDIA accoustic echo cancellation'],
    ['HAS_PJMEDIA_SND_DEV',	'PJMEDIA sound device backend (platform specific)'],
    ['HAS_PJMEDIA_SILENCE_DET',	'PJMEDIA Adaptive silence detector'],
    ['HAS_PJMEDIA',		'PJMEDIA endpoint'],
    ['HAS_PJMEDIA_PLC',		'PJMEDIA Packet Lost Concealment implementation (needed by G.711, GSM, and sound device port)'],
    ['HAS_PJMEDIA_SND_PORT',	'PJMEDIA sound device media port'],
    ['HAS_PJMEDIA_RESAMPLE',	'PJMEDIA resampling algorithm (large filter disabled)'],
    ['HAS_PJMEDIA_G711_CODEC',	'PJMEDIA G.711 codec (PCMA/PCMU, including PLC) (may have already been linked by other module)'],
    ['HAS_PJMEDIA_CONFERENCE',	'PJMEDIA conference bridge (needs resampling and silence detector)'],
    ['HAS_PJMEDIA_MASTER_PORT',	'PJMEDIA master port'],
    ['HAS_PJMEDIA_RTP',		'PJMEDIA stand-alone RTP'],
    ['HAS_PJMEDIA_RTCP',	'PJMEDIA stand-alone RTCP and media quality calculation'],
    ['HAS_PJMEDIA_JBUF',	'PJMEDIA stand-alone adaptive jitter buffer'],
    ['HAS_PJMEDIA_STREAM',	'PJMEDIA stream for remote media communication (needs RTP, RTCP, and jitter buffer)'],
    ['HAS_PJMEDIA_TONEGEN',	'PJMEDIA tone generator'],
    ['HAS_PJMEDIA_UDP_TRANSPORT','PJMEDIA UDP media transport'],
    ['HAS_PJMEDIA_FILE_PLAYER',	'PJMEDIA WAV file player'],
    ['HAS_PJMEDIA_FILE_CAPTURE',	'PJMEDIA WAV file writer'],
    ['HAS_PJMEDIA_MEM_PLAYER',	'PJMEDIA fixed buffer player'],
    ['HAS_PJMEDIA_MEM_CAPTURE',	'PJMEDIA fixed buffer writer'],
    ['HAS_PJMEDIA_ICE',		'PJMEDIA ICE transport'],

    # Subtotal
    ['',			'Subtotal: Complete SIP and all PJMEDIA features (G.711 codec only)'],
    
    # Codecs
    ['HAS_PJMEDIA_GSM_CODEC',	'PJMEDIA GSM codec (including PLC)'],
    ['HAS_PJMEDIA_SPEEX_CODEC',	'PJMEDIA Speex codec (narrowband, wideband, ultra-wideband)'],
    ['HAS_PJMEDIA_ILBC_CODEC',	'PJMEDIA iLBC codec'],

    # Total
    ['',			'TOTAL: complete libraries (+all codecs)'],
]

# Executable size report, tuple of:
#   <all flags>, <flags added>, <text size>, <data>, <bss>, <description>
exe_size = []

#
# Write the report to text file
#
def print_text_report(filename):
    output = open(filename, 'w')

    output.write('PJSIP and PJMEDIA footprint report\n')
    output.write('Auto-generated by pjsip-apps/build/get-footprint.py\n')
    output.write('\n')

    # Write Revision info.
    f = os.popen('svn info | grep Revision')
    output.write(f.readline())

    output.write('Date: ')
    output.write(time.asctime())
    output.write('\n')
    output.write('\n')

    # Write individual module size
    output.write('Footprint (in bytes):\n')
    output.write('   .text   .data    .bss    Module Description\n')
    output.write('==========================================================\n')
	
    for i in range(1, len(exe_size)):
	e = exe_size[i]
	prev = exe_size[i-1]
	
	if e[1]<>'':
	    output.write(' ')
	    output.write(  string.rjust(`string.atoi(e[2]) - string.atoi(prev[2])`, 8) )
	    output.write(  string.rjust(`string.atoi(e[3]) - string.atoi(prev[3])`, 8) )
	    output.write(  string.rjust(`string.atoi(e[4]) - string.atoi(prev[4])`, 8) )
	    output.write('   ' + e[5] + '\n')
	else:
	    output.write(' ------------------------\n')	
	    output.write(' ')
	    output.write( string.rjust(e[2], 8) )
	    output.write( string.rjust(e[3], 8) )
	    output.write( string.rjust(e[4], 8) )
	    output.write('   ' + e[5] + '\n')
	    output.write('\n')	

	
    # Done    
    output.close()


#
# Write the report to HTML file
#
def print_html_report():

    # Get Revision info.
    f = os.popen('svn info | grep Revision')
    revision = f.readline().split()[1]

    # Get Machine, OS, and CC name
    f = os.popen('make -f Footprint.mak print_name')
    names = f.readline().split()
    m = names[0]
    o = names[1]
    cc = names[2]
    cc_ver = names[3]

    # Open HTML file
    filename = 'footprint-' + m + '-' + o + '.htm'
    output = open(filename, 'w')

    title = 'PJSIP and PJMEDIA footprint report for ' + m + '-' + o + ' target'
    output.write('<HTML><HEAD>\n');
    output.write(' <TITLE>' + title + '</TITLE>\n')
    output.write(' <LINK href="/style/style.css" type="text/css" rel="stylesheet">\n')
    output.write('</HEAD>\n');
    output.write('<BODY bgcolor="white">\n');
    output.write('<!--#include virtual="/header.html" -->')

    output.write(' <H1>' + title + '</H1>\n')
    output.write('Auto-generated by pjsip-apps/build/get-footprint.py script\n')
    output.write('<p>Date: ' + time.asctime() + '<BR>\n')
    output.write('Revision: r' + revision + '</p>\n\n')
    output.write('<HR>\n')
    output.write('\n')

    # Info
    output.write('<H2>Build Configuration</H2>\n')

    # build.mak
    output.write('\n<H3>build.mak</H3>\n')
    output.write('<tt>\n')
    f = open('../../build.mak', 'r')
    s = f.readlines()
    for l in s:
	output.write(l + '<BR>\n')
    output.write('</tt>\n')
    output.write('<p>Using ' + cc + ' version ' + cc_ver +'</p>\n')

    # user.mak
    output.write('\n<H3>user.mak</H3>\n')
    output.write('<tt>\n')
    f = open('../../user.mak', 'r')
    s = f.readlines()
    for l in s:
	output.write(l + '<BR>\n')
    output.write('</tt>\n')

    # config_site.h
    output.write('\n<H3>&lt;pj/config.site.h&gt;</H3>\n')
    output.write('<tt>\n')
    f = os.popen('cpp -dM -I../../pjlib/include ../../pjlib/include/pj/config_site.h | grep PJ')
    s = f.readlines()
    for l in s:
	output.write(l + '<BR>\n')
    output.write('</tt>\n')



    # Write individual module size
    output.write('<H2>Footprint Report</H2>\n')
    output.write('<p>The table below shows the footprint of individual feature, in bytes.</p>')
    output.write('<TABLE border="1" cellpadding="2" cellspacing="0">\n' + 
		  '<TR bgcolor="#e8e8ff">\n' + 
		  '  <TD align="center"><strong>.text</strong></TD>\n' +
		  '  <TD align="center"><strong>.data</strong></TD>\n' +
		  '  <TD align="center"><strong>.bss</strong></TD>\n' +
		  '  <TD align="center"><strong>Features/Module Description</strong></TD>\n' +
		  '</TR>\n')
	
	
    for i in range(1, len(exe_size)):
	e = exe_size[i]
	prev = exe_size[i-1]
	
	output.write('<TR>\n')
	if e[1]<>'':
	    output.write( '  <TD align="right">' + `string.atoi(e[2]) - string.atoi(prev[2])` + '</TD>\n')
	    output.write( '  <TD align="right">' + `string.atoi(e[3]) - string.atoi(prev[3])` + '</TD>\n')
	    output.write( '  <TD align="right">' + `string.atoi(e[4]) - string.atoi(prev[4])` + '</TD>\n' )
	    output.write( '  <TD>' + e[5] + '</TD>\n')
	else:
	    empty_size = exe_size[1]
	    output.write('<TR bgcolor="#e8e8ff">\n')
	    output.write( '  <TD align="right">&nbsp;</TD>\n')
	    output.write( '  <TD align="right">&nbsp;</TD>\n')
	    output.write( '  <TD align="right">&nbsp;</TD>\n')
	    output.write( '  <TD><strong>' + e[5] + ': .text=' + e[2]+ ', .data=' + e[3] + ', .bss=' + e[4] )
	    output.write( '\n </strong> <BR>(Size minus empty application size: ' + \
			    '.text=' + `string.atoi(e[2]) - string.atoi(empty_size[2])` + \
			    ', .data=' + `string.atoi(e[3]) - string.atoi(empty_size[3])` + \
			    ', .data=' + `string.atoi(e[4]) - string.atoi(empty_size[4])` + \
			    ')\n' )
	    output.write( ' </TD>\n')

	output.write('</TR>\n')

    output.write('</TABLE>\n')
    output.write('<!--#include virtual="/footer.html" -->')
    output.write('</BODY>\n')
    output.write('</HTML>\n')
	
    # Done    
    output.close()




#
# Get the size of individual feature
#
def get_size(all_flags, flags, desc):
	file = 'footprint.exe'
	# Remove file
	rc = os.system("make -f Footprint.mak FCFLAGS='" + all_flags + "' clean")
	# Make the executable
	cmd = "make -f Footprint.mak FCFLAGS='" + all_flags + "' all"
	#print cmd
	rc = os.system(cmd)
	if rc <> 0:
		sys.exit(1)

	# Run 'size' against the executable
	f = os.popen('size ' + file)
	# Skip header of the 'size' output
	f.readline()
	# Get the sizes
	size = f.readline()
	f.close()
	# Split into tokens
	tokens = size.split()
	# Build the size tuple and add to exe_size
	elem = all_flags, flags, tokens[0], tokens[1], tokens[2], desc
	exe_size.append(elem)
	# Remove file
	rc = os.system("make -f Footprint.mak FCFLAGS='" + all_flags + "' clean")
	
# Main
elem = '', '',  '0', '0', '0', ''
exe_size.append(elem)

all_flags = ''
for elem in compile_flags:
    if elem[0] <> '':
	flags = '-D' + elem[0]
	all_flags += flags + ' '
	get_size(all_flags, elem[0], elem[1])
    else:
	e = exe_size[len(exe_size)-1]
	n = all_flags, '', e[2], e[3], e[4], elem[1]
	exe_size.append(n)
	

#print_text_report('footprint.txt')	
print_html_report()

