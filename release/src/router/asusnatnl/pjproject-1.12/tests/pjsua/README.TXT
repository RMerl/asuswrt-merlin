
                           PJSUA TEST FRAMEWORK
                        =========================

0. What is this
---------------
This is the automated testing scripts for pjsua. It can do many things (just 
don't ask it to write good documentation :) ).


1. Requirements
---------------
To run the tests you need:
 - Python (tested with Python 2.5.2)
 - pjsua application, built and placed in pjsip-apps/bin directory
 - the pjsua must be built with:
     - SRTP enabled (the default)


2. Using
--------
To run all the tests:
  $ python [OPTIONS] runall.py

To run individual test:
  $ python [OPTIONS] run.py MODULE CONFIG

Where options:
  -e EXE	use EXE as pjsua executable
  -n		use null audio
  -r TEST	(for runall.py only) resume test at TEST

  
For each individual tests, the run.py is the main entry for the test. It 
imports the various inc_xxx.py files, and it will load the MODULE. The MODULE 
contains specific test flows, and we have few of them:

  - mod_run.py: 
  	a simple test which just run pjsua with the configuration from CONFIG 
	file and checks if pjsua can start properly.

  - mod_call.py: 
  	call testing where it spawns two pjsua instances each with configura-
	tions as specified in CONFIG file, makes one pjsua call the other, and 
	checks if the call can be established.

  - mod_pres.py:
  	presence testing

  - mod_sendto.py:
  	Simple UAC to send arbitrary SIP message to pjsua. Good to test
	various incoming INVITE scenarios

  - mod_media_playrec.py:
  	Mainly for resampling quality testing

  - mod_pesq.py
  	Measure call quality of various call settings with PESQ, for people 
	who have PESQ tool and license

Example:
  $ python run.py mod_run.py scripts-run/100_simple.py
  $ python run.py mod_call.py scripts-call/100_simple.py


