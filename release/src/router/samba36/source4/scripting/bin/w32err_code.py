#!/usr/bin/env python

# Unix SMB/CIFS implementation.
# Copyright (C) Kamen Mazdrashki <kamen.mazdrashki@postpath.com> 2009
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

"""Import generete werror.h/doserr.c files from WSPP HTML"""

import re
import os
import sys
import urllib
import pprint
from xml.dom import minidom
from optparse import OptionParser, OptionGroup

_wspp_werror_url = 'http://msdn.microsoft.com/en-us/library/cc231199%28PROT.10%29.aspx'

class WerrorHtmlParser(object):
    """
    Parses HTML from WSPP documentation generating dictionary of
    dictionaries with following keys:
      - "err_hex" - hex number (as string)
      - "err_name" - error name
      - "err_desc" - error long description
    For the key of returned dictionary err_hex is used,
    i.e. "hex-error-code-str" => {error dictionary object}
    """

    ERROR_PREFIX = ['ERROR_', 'NERR_', 'FRS_', 'RPC_', 'EPT_', 'OR_', 'WAIT_TIMEOUT']
    ERROR_REPLACE = ['ERROR_']

    def __init__(self, opt):
        self.opt = opt
        self._errors_skipped = []
        pass

    def _is_error_code_name(self, err_name):
        for pref in self.ERROR_PREFIX:
            if err_name.startswith(pref):
                return True
        return False

    def _make_werr_name(self, err_name):
        err_name = err_name.upper()
        for pref in self.ERROR_REPLACE:
            if err_name.startswith(pref):
                return err_name.replace(pref, 'WERR_', 1)
        return 'WERR_' + err_name

    def parse_url(self, url):
        errors = {}
        html = self._load_url(url)

        # let minidom to parse the tree, should be:
        # table -> tr -> td
        #     p -> [hex code, br, error code]
        #     p -> [description]
        table_node = minidom.parseString(html)
        for row_node in table_node.getElementsByTagName("tr"):
            # verify we got right number of td elements
            td_nodes = row_node.getElementsByTagName('td')
            if len(td_nodes) != 2:
                continue
            # now get the real data
            p_nodes = row_node.getElementsByTagName('p')
            if len(p_nodes) != 2:   continue
            if len(p_nodes[0].childNodes) != 3: continue
            if len(p_nodes[1].childNodes) != 1: continue
            err_hex = str(p_nodes[0].childNodes[0].nodeValue)
            err_name = str(p_nodes[0].childNodes[2].nodeValue)
            err_desc = p_nodes[1].childNodes[0].nodeValue.encode('utf-8')
            err_desc = err_desc.replace('"', '\\"').replace("\'", "\\'")
            # do some checking
            if not err_hex.startswith('0x'):    continue
            if not self._is_error_code_name(err_name):
                self._errors_skipped.append("%s - %s - %d" % (err_name, err_hex, int(err_hex, 16)))
                continue
            # create entry
            err_name = self._make_werr_name(err_name)
            err_def = {'err_hex': err_hex,
                       'err_name': err_name,
                       'err_desc': err_desc,
                       'code': int(err_hex, 16)}
            errors[err_def['code']] = err_def

        # print skipped errors
        if self.opt.print_skipped and len(self._errors_skipped):
            print "\nErrors skipped during HTML parsing:"
            pprint.pprint(self._errors_skipped)
            print "\n"

        return errors

    def _load_url(self, url):
        html_str = ""
        try:
            fp = urllib.urlopen(url)
            for line in fp:
                html_str += line.strip()
            fp.close()
        except IOError, e:
            print "error loading url: " + e.strerror
            pass

        # currently ERROR codes are rendered as table
        # locate table chunk with ERROR_SUCCESS
        html = [x for x in html_str.split('<table ') if "ERROR_SUCCESS" in x]
        html = '<table ' + html[0]
        pos = html.find('</table>')
        if pos == -1:
            return '';
        html = html[:pos] + '</table>'

        # html clean up
        html = re.sub(r'<a[^>]*>(.*?)</a>', r'\1', html)

        return html


class WerrorGenerator(object):
    """
    provides methods to generate parts of werror.h and doserr.c files
    """

    FNAME_WERRORS   = 'w32errors.lst'
    FNAME_WERROR_DEFS  = 'werror_defs.h'
    FNAME_DOSERR_DEFS = 'doserr_defs.c'
    FNAME_DOSERR_DESC = 'doserr_desc.c'

    def __init__(self, opt):
        self.opt = opt
        self._out_dir = opt.out_dir
        pass

    def _open_out_file(self, fname):
        fname = os.path.join(self._out_dir, fname)
        return open(fname, "w")

    def _gen_werrors_list(self, errors):
        """uses 'errors' dictionary to display list of Win32 Errors"""

        fp = self._open_out_file(self.FNAME_WERRORS)
        for err_code in sorted(errors.keys()):
            err_name = errors[err_code]['err_name']
            fp.write(err_name)
            fp.write("\n")
        fp.close()

    def _gen_werror_defs(self, errors):
        """uses 'errors' dictionary to generate werror.h file"""

        fp = self._open_out_file(self.FNAME_WERROR_DEFS)
        for err_code in sorted(errors.keys()):
            err_name = errors[err_code]['err_name']
            err_hex = errors[err_code]['err_hex']
            fp.write('#define %s\tW_ERROR(%s)' % (err_name, err_hex))
            fp.write("\n")
        fp.close()

    def _gen_doserr_defs(self, errors):
        """uses 'errors' dictionary to generate defines in doserr.c file"""

        fp = self._open_out_file(self.FNAME_DOSERR_DEFS)
        for err_code in sorted(errors.keys()):
            err_name = errors[err_code]['err_name']
            fp.write('\t{ "%s", %s },' % (err_name, err_name))
            fp.write("\n")
        fp.close()

    def _gen_doserr_descriptions(self, errors):
        """uses 'errors' dictionary to generate descriptions in doserr.c file"""

        fp = self._open_out_file(self.FNAME_DOSERR_DESC)
        for err_code in sorted(errors.keys()):
            err_name = errors[err_code]['err_name']
            fp.write('\t{ %s, "%s" },' % (err_name, errors[err_code]['err_desc']))
            fp.write("\n")
        fp.close()

    def _lookup_error_by_name(self, err_name, defined_errors):
        for err in defined_errors.itervalues():
            if err['err_name'] == err_name:
                return err
        return None

    def _filter_errors(self, errors, defined_errors):
        """
        returns tuple (new_erros, diff_code_errors, diff_name_errors)
        new_errors - dictionary of errors not in defined_errors
        diff_code_errors - list of errors found in defined_errors
                           but with different value
        diff_name_errors - list of errors found with same code in
                            defined_errors, but with different name
        Most critical is diff_code_errors list to be empty!
        """
        new_errors = {}
        diff_code_errors = []
        diff_name_errors = []
        for err_def in errors.itervalues():
            add_error = True
            # try get defined error by code
            if defined_errors.has_key(err_def['code']):
                old_err = defined_errors[err_def['code']]
                if err_def['err_name'] != old_err['err_name']:
                    warning = {'msg': 'New and Old errors has different error names',
                               'err_new': err_def,
                               'err_old': old_err}
                    diff_name_errors.append(warning)

            # sanity check for errors with same name but different values
            old_err = self._lookup_error_by_name(err_def['err_name'], defined_errors)
            if old_err:
                if err_def['code'] != old_err['code']:
                    warning = {'msg': 'New and Old error defs has different error value',
                               'err_new': err_def,
                               'err_old': old_err}
                    diff_code_errors.append(warning)
                # exclude error already defined with same name
                add_error = False
            # do add the error in new_errors if everything is fine
            if add_error:
                new_errors[err_def['code']] = err_def
            pass
        return (new_errors, diff_code_errors, diff_name_errors)

    def generate(self, errors):
        # load already defined error codes
        werr_parser = WerrorParser(self.opt)
        (defined_errors,
         no_value_errors) = werr_parser.load_err_codes(self.opt.werror_file)
        if not defined_errors:
            print "\nUnable to load existing errors file: %s" % self.opt.werror_file
            sys.exit(1)
        if self.opt.verbose and len(no_value_errors):
            print "\nWarning: there are errors defines using macro value:"
            pprint.pprint(no_value_errors)
            print ""
        # filter generated error codes
        (new_errors,
         diff_code_errors,
         diff_name_errors) = self._filter_errors(errors, defined_errors)
        if diff_code_errors:
            print("\nFound %d errors with same names but different error values! Aborting."
                  % len(diff_code_errors))
            pprint.pprint(diff_code_errors)
            sys.exit(2)

        if diff_name_errors:
            print("\nFound %d errors with same values but different names (should be normal)"
                  % len(diff_name_errors))
            pprint.pprint(diff_name_errors)

        # finally generate output files
        self._gen_werror_defs(new_errors)
        self._gen_doserr_defs(new_errors)
        self._gen_werrors_list(errors)
        self._gen_doserr_descriptions(errors)
        pass

class WerrorParser(object):
    """
    Parses errors defined in werror.h file
    """

    def __init__(self, opt):
        self.opt = opt
        pass

    def _parse_werror_line(self, line):
        m = re.match('#define[ \t]*(.*?)[ \t]*W_ERROR\((.*?)\)', line)
        if not m or (len(m.groups()) != 2):
            return None
        if len(m.group(1)) == 0:
            return None
        if str(m.group(2)).startswith('0x'):
            err_code = int(m.group(2), 16)
        elif m.group(2).isdigit():
            err_code = int(m.group(2))
        else:
            self.err_no_values.append(line)
            return None
        return {'err_name': str(m.group(1)),
                'err_hex': "0x%08X" % err_code,
                'code': err_code}
        pass

    def load_err_codes(self, fname):
        """
        Returns tuple of:
            dictionary of "hex_err_code" => {code, name}
                "hex_err_code" is string
                "code" is int value for the error
            list of errors that was ignored for some reason
        """
        # reset internal variables
        self.err_no_values = []
        err_codes = {}
        fp = open(fname)
        for line in fp.readlines():
            err_def = self._parse_werror_line(line)
            if err_def:
                err_codes[err_def['code']] = err_def
        fp.close();
        return (err_codes, self.err_no_values)



def _generate_files(opt):
    parser = WerrorHtmlParser(opt)
    errors = parser.parse_url(opt.url)

    out = WerrorGenerator(opt)
    out.generate(errors)
    pass


if __name__ == '__main__':
    _cur_dir = os.path.abspath(os.path.dirname(__file__))
    opt_parser = OptionParser(usage="usage: %prog [options]", version="%prog 0.3")
    opt_group = OptionGroup(opt_parser, "Main options")
    opt_group.add_option("--url", dest="url",
                         default=_wspp_werror_url,
                         help="url for w32 error codes html - may be local file")
    opt_group.add_option("--out", dest="out_dir",
                         default=_cur_dir,
                         help="output dir for generated files")
    opt_group.add_option("--werror", dest="werror_file",
                         default=os.path.join(_cur_dir, 'werror.h'),
                         help="path to werror.h file")
    opt_group.add_option("--print_skipped",
                        action="store_true", dest="print_skipped", default=False,
                        help="print errors skipped during HTML parsing")
    opt_group.add_option("-q", "--quiet",
                        action="store_false", dest="verbose", default=True,
                        help="don't print warnings to stdout")

    opt_parser.add_option_group(opt_group)

    (options, args) = opt_parser.parse_args()

    # add some options to be used internally
    options.err_defs_file = os.path.join(options.out_dir, WerrorGenerator.FNAME_WERROR_DEFS)
    options.dos_defs_file = os.path.join(options.out_dir, WerrorGenerator.FNAME_DOSERR_DEFS)
    options.dos_desc_file = os.path.join(options.out_dir, WerrorGenerator.FNAME_DOSERR_DESC)

    # check options
    _generate_files(options)
