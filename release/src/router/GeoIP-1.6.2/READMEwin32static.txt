To make a static GeoIP.lib, edit the top level
Makefile.vc to reflect where the GeoIP.dat database
file should be placed, as well as the locations
of the lib, include, and bin directories for installation.
Then give the command
   nmake /f Makefile.vc
This will build the GeoIP.lib library, as well as available
application and test programs. The command
   nmake /f Makefile.vc test
will run available tests in the test/ subdirectory.
   nmake /f Makefile.vc install
will then copy the lib and header files to the locations
specified in the top-level Makefile.vc, as well as
available application programs in the apps/ subdirectory.
   nmake /f Makefile.vc clean
will remove intermediate object and executable files.

