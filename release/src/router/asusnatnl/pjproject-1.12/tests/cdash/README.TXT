                                
                           PJSIP CDASH AUTOMATED TESTS
                        --------------------------------


1. What is this

   This directory contains the scripts to run the automated, Python based tests
of PJSIP source codes, across platforms, and submit the test results to a CDash
test monitoring dashboard.

   Stuffs that are included in the test scope:
    - configure (for GNU platforms, e.g. Linux, msys, and MacOS X)
    - build
    - standard unit tests (pjlib-test, pjsip-test, etc.)
    - pjsua's Python based blackbox tests


2. Requirements

   To run this test, you will need:
    - Python (2.4 or later)
    - curl (http://curl.haxx.se)
    - a CDash server to receive test submissions (http://www.cdash.org)
    - ccdash to submit the tests (http://trac.pjsip.org/ccdash)


3. Configuration

   Create a Python configuration file by copying from "cfg_site_sample.py". 
Save it as "cfg_site.py". You may create more than one configurations for your
site and save them as different files.


4. Running

   To execute tests for GNU based targets:

     $ python main.py cfg_gnu


   To execute tests for MSVC based target:

     $ python main.py cfg_msvc

   To execute tests for Symbian target:

     $ python main.py cfg_symbian


   If you have a different site configuration file, you may specify it in the
arguments, e.g.:

     $ python main.py cfg_gnu my_site_config

   For more information you can also pass "-h" or "--help", e.g.:

     $ python main.py cfg_gnu --help

