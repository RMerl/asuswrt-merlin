This directory contains code for use with, and code made by, the
automatic code generation tool "Trunnel".

Trunnel generates binary parsers and formatters for simple data
structures. It aims for human-readable, obviously-correct outputs over
maximum efficiency or flexibility.

The .trunnel files are the inputs here; the .c and .h files are the outputs.

To add a new structure:
   - Add a new .trunnel file or expand an existing one to describe the format
     of the structure.
   - Regenerate the .c and .h files.  To do this, you run
     "scripts/codegen/run_trunnel.sh".  You'll need trunnel installed.
   - Add the .trunnel, .c, and .h files to include.am

For the Trunnel source code, and more documentation about using Trunnel,
see https://gitweb.torproject.org/trunnel.git , especially
    https://gitweb.torproject.org/trunnel.git/tree/README
and https://gitweb.torproject.org/trunnel.git/tree/doc/trunnel.md

