





Network Printing Working Group                 L. McLaughlin III, Editor
Request for Comments:  1179                         The Wollongong Group
                                                             August 1990


                      Line Printer Daemon Protocol

Status of this Memo

   This RFC describes an existing print server protocol widely used on
   the Internet for communicating between line printer daemons (both
   clients and servers).  This memo is for informational purposes only,
   and does not specify an Internet standard.  Please refer to the
   current edition of the "IAB Official Protocol Standards" for the
   standardization state and status of this protocol.  Distribution of
   this memo is unlimited.

1. Introduction

   The Berkeley versions of the Unix(tm) operating system provide line
   printer spooling with a collection of programs: lpr (assign to
   queue), lpq (display the queue), lprm (remove from queue), and lpc
   (control the queue).  These programs interact with an autonomous
   process called the line printer daemon.  This RFC describes the
   protocols with which a line printer daemon client may control
   printing.

   This memo is based almost entirely on the work of Robert Knight at
   Princeton University.  I gratefully acknowledge his efforts in
   deciphering the UNIX lpr protocol and producing earlier versions of
   this document.

2. Model of Printing Environment

   A group of hosts request services from a line printer daemon process
   running on a host.  The services provided by the process are related
   to printing jobs.  A printing job produces output from one file.
   Each job will have a unique job number which is between 0 and 999,
   inclusive.  The jobs are requested by users which have names.  These
   user names may not start with a digit.

3. Specification of the Protocol

   The specification includes file formats for the control and data
   files as well as messages used by the protocol.






McLaughlin                                                      [Page 1]

RFC 1179                          LPR                        August 1990


3.1 Message formats

   LPR is a a TCP-based protocol.  The port on which a line printer
   daemon listens is 515.  The source port must be in the range 721 to
   731, inclusive.  A line printer daemon responds to commands send to
   its port.  All commands begin with a single octet code, which is a
   binary number which represents the requested function.  The code is
   immediately followed by the ASCII name of the printer queue name on
   which the function is to be performed.  If there are other operands
   to the command, they are separated from the printer queue name with
   white space (ASCII space, horizontal tab, vertical tab, and form
   feed).  The end of the command is indicated with an ASCII line feed
   character.

4. Diagram Conventions

   The diagrams in the rest of this RFC use these conventions.  These
   diagrams show the format of an octet stream sent to the server.  The
   outermost box represents this stream.  Each box within the outermost
   one shows one portion of the stream.  If the contents of the box is
   two decimal digits, this indicates that the binary 8 bit value is to
   be used.  If the contents is two uppercase letters, this indicates
   that the corresponding ASCII control character is to be used.  An
   exception to this is that the character SP can be interpreted as
   white space.  (See the preceding section for a definition.)  If the
   contents is a single letter, the ASCII code for this letter must be
   sent.  Otherwise, the contents are intended to be mnemonic of the
   contents of the field which is a sequence of octets.

5. Daemon commands

   The verbs in the command names should be interpreted as statements
   made to the daemon.  Thus, the command "Print any waiting jobs" is an
   imperative to the line printer daemon to which it is sent.  A new
   connection must be made for each command to be given to the daemon.

5.1 01 - Print any waiting jobs

      +----+-------+----+
      | 01 | Queue | LF |
      +----+-------+----+
      Command code - 1
      Operand - Printer queue name

   This command starts the printing process if it not already running.






McLaughlin                                                      [Page 2]

RFC 1179                          LPR                        August 1990


5.2 02 - Receive a printer job

      +----+-------+----+
      | 02 | Queue | LF |
      +----+-------+----+
      Command code - 2
      Operand - Printer queue name

   Receiving a job is controlled by a second level of commands.  The
   daemon is given commands by sending them over the same connection.
   The commands are described in the next section (6).

   After this command is sent, the client must read an acknowledgement
   octet from the daemon.  A positive acknowledgement is an octet of
   zero bits.  A negative acknowledgement is an octet of any other
   pattern.

5.3 03 - Send queue state (short)

      +----+-------+----+------+----+
      | 03 | Queue | SP | List | LF |
      +----+-------+----+------+----+
      Command code - 3
      Operand 1 - Printer queue name
      Other operands - User names or job numbers

   If the user names or job numbers or both are supplied then only those
   jobs for those users or with those numbers will be sent.

   The response is an ASCII stream which describes the printer queue.
   The stream continues until the connection closes.  Ends of lines are
   indicated with ASCII LF control characters.  The lines may also
   contain ASCII HT control characters.

5.4 04 - Send queue state (long)

      +----+-------+----+------+----+
      | 04 | Queue | SP | List | LF |
      +----+-------+----+------+----+
      Command code - 4
      Operand 1 - Printer queue name
      Other operands - User names or job numbers

   If the user names or job numbers or both are supplied then only those
   jobs for those users or with those numbers will be sent.

   The response is an ASCII stream which describes the printer queue.
   The stream continues until the connection closes.  Ends of lines are



McLaughlin                                                      [Page 3]

RFC 1179                          LPR                        August 1990


   indicated with ASCII LF control characters.  The lines may also
   contain ASCII HT control characters.

5.5 05 - Remove jobs

      +----+-------+----+-------+----+------+----+
      | 05 | Queue | SP | Agent | SP | List | LF |
      +----+-------+----+-------+----+------+----+
      Command code - 5
      Operand 1 - Printer queue name
      Operand 2 - User name making request (the agent)
      Other operands - User names or job numbers

   This command deletes the print jobs from the specified queue which
   are listed as the other operands.  If only the agent is given, the
   command is to delete the currently active job.  Unless the agent is
   "root", it is not possible to delete a job which is not owned by the
   user.  This is also the case for specifying user names instead of
   numbers.  That is, agent "root" can delete jobs by user name but no
   other agents can.

6. Receive job subcommands

   These commands  are processed when  the line printer  daemon  has
   been given the  receive job command.  The  daemon will continue  to
   process commands until the connection is closed.

   After a subcommand is sent, the client must wait for an
   acknowledgement from the daemon.  A positive acknowledgement is an
   octet of zero bits.  A negative acknowledgement is an octet of any
   other pattern.

   LPR clients SHOULD be able to sent the receive data file and receive
   control file subcommands in either order.  LPR servers MUST be able
   to receive the control file subcommand first and SHOULD be able to
   receive the data file subcommand first.

6.1 01 - Abort job

      Command code - 1
      +----+----+
      | 01 | LF |
      +----+----+

   No operands should be supplied.  This subcommand will remove any
   files which have been created during this "Receive job" command.





McLaughlin                                                      [Page 4]

RFC 1179                          LPR                        August 1990


6.2 02 - Receive control file

      +----+-------+----+------+----+
      | 02 | Count | SP | Name | LF |
      +----+-------+----+------+----+
      Command code - 2
      Operand 1 - Number of bytes in control file
      Operand 2 - Name of control file

   The control file must be an ASCII stream with the ends of lines
   indicated by ASCII LF.  The total number of bytes in the stream is
   sent as the first operand.  The name of the control file is sent as
   the second.  It should start with ASCII "cfA", followed by a three
   digit job number, followed by the host name which has constructed the
   control file.  Acknowledgement processing must occur as usual after
   the command is sent.

   The next "Operand 1" octets over the same TCP connection are the
   intended contents of the control file.  Once all of the contents have
   been delivered, an octet of zero bits is sent as an indication that
   the file being sent is complete.  A second level of acknowledgement
   processing must occur at this point.

6.3 03 - Receive data file

      +----+-------+----+------+----+
      | 03 | Count | SP | Name | LF |
      +----+-------+----+------+----+
      Command code - 3
      Operand 1 - Number of bytes in data file
      Operand 2 - Name of data file

   The data file may contain any 8 bit values at all.  The total number
   of bytes in the stream may be sent as the first operand, otherwise
   the field should be cleared to 0.  The name of the data file should
   start with ASCII "dfA".  This should be followed by a three digit job
   number.  The job number should be followed by the host name which has
   constructed the data file.  Interpretation of the contents of the
   data file is determined by the contents of the corresponding control
   file.  If a data file length has been specified, the next "Operand 1"
   octets over the same TCP connection are the intended contents of the
   data file.  In this case, once all of the contents have been
   delivered, an octet of zero bits is sent as an indication that the
   file being sent is complete.  A second level of acknowledgement
   processing must occur at this point.






McLaughlin                                                      [Page 5]

RFC 1179                          LPR                        August 1990


7. Control file lines

   This section  discusses the format of  the lines in the  control file
   which is sent to the line printer daemon.

   Each line of the control file consists of a single, printable ASCII
   character which represents a function to be performed when the file
   is printed.  Interpretation of these command characters are case-
   sensitive.  The rest of the line after the command character is the
   command's operand.  No leading white space is permitted after the
   command character.  The line ends with an ASCII new line.

   Those commands which have a lower case letter as a command code are
   used to specify an actual printing request.  The commands which use
   upper case are used to describe parametric values or background
   conditions.

   Some commands must be included in every control file.  These are 'H'
   (responsible host) and 'P' (responsible user).  Additionally, there
   must be at least one lower case command to produce any output.

7.1 C - Class for banner page

      +---+-------+----+
      | C | Class | LF |
      +---+-------+----+
      Command code - 'C'
      Operand - Name of class for banner pages

   This command sets the class name to be printed on the banner page.
   The name must be 31 or fewer octets.  The name can be omitted.  If it
   is, the name of the host on which the file is printed will be used.
   The class is conventionally used to display the host from which the
   printing job originated.  It will be ignored unless the print banner
   command ('L') is also used.

7.2 H - Host name

      +---+------+----+
      | H | Host | LF |
      +---+------+----+
      Command code - 'H'
      Operand - Name of host

   This command specifies the name of the host which is to be treated as
   the source of the print job.  The command must be included in the
   control file.  The name of the host must be 31 or fewer octets.




McLaughlin                                                      [Page 6]

RFC 1179                          LPR                        August 1990


7.3 I - Indent Printing

      +---+-------+----+
      | I | count | LF |
      +---+-------+----+
      Command code - 'I'
      Operand - Indenting count

   This command specifies that, for files which are printed with the
   'f', of columns given.  (It is ignored for other output generating
   commands.)  The identing count operand must be all decimal digits.

7.4 J - Job name for banner page

      +---+----------+----+
      | J | Job name | LF |
      +---+----------+----+
      Command code - 'J'
      Operand - Job name

   This command sets the job name to be printed on the banner page.  The
   name of the job must be 99 or fewer octets.  It can be omitted.  The
   job name is conventionally used to display the name of the file or
   files which were "printed".  It will be ignored unless the print
   banner command ('L') is also used.

7.5 L - Print banner page

      +---+------+----+
      | L | User | LF |
      +---+------+----+
      Command code - 'L'
      Operand - Name of user for burst pages

   This command causes the banner page to be printed.  The user name can
   be omitted.  The class name for banner page and job name for banner
   page commands must precede this command in the control file to be
   effective.

7.6 M - Mail When Printed

      +---+------+----+
      | M | user | LF |
      +---+------+----+
      Command code - 'M'
      Operand - User name

   This entry causes mail to be sent to the user given as the operand at



McLaughlin                                                      [Page 7]

RFC 1179                          LPR                        August 1990


   the host specified by the 'H' entry when the printing operation ends
   (successfully or unsuccessfully).

7.7 N - Name of source file

      +---+------+----+
      | N | Name | LF |
      +---+------+----+
      Command code - 'N'
      Operand - File name

   This command specifies the name of the file from which the data file
   was constructed.  It is returned on a query and used in printing with
   the 'p' command when no title has been given.  It must be 131 or
   fewer octets.

7.8 P - User identification

      +---+------+----+
      | P | Name | LF |
      +---+------+----+
      Command code - 'P'
      Operand - User id

   This command specifies the user identification of the entity
   requesting the printing job.  This command must be included in the
   control file.  The user identification must be 31 or fewer octets.

7.9 S - Symbolic link data

      +---+--------+----+-------+----+
      | S | device | SP | inode | LF |
      +---+--------+----+-------+----+
      Command code - 'S'
      Operand 1 - Device number
      Operand 2 - Inode number

   This command is used to record symbolic link data on a Unix system so
   that changing a file's directory entry after a file is printed will
   not print the new file.  It is ignored if the data file is not
   symbolically linked.










McLaughlin                                                      [Page 8]

RFC 1179                          LPR                        August 1990


7.10 T - Title for pr

      +---+-------+----+
      | T | title | LF |
      +---+-------+----+
      Command code - 'T'
      Operand - Title text

   This command provides a title for a file which is to be printed with
   either the 'p' command.  (It is ignored by all of the other printing
   commands.)  The title must be 79 or fewer octets.

7.11 U - Unlink data file

      +---+------+----+
      | U | file | LF |
      +---+------+----+
      Command code - 'U'
      Operand - File to unlink

   This command indicates that the specified file is no longer needed.
   This should only be used for data files.

7.12 W - Width of output

      +---+-------+----+
      | W | width | LF |
      +---+-------+----+
      Command code - 'W'
      Operand - Width count

   This command limits the output to the specified number of columns for
   the 'f', 'l', and 'p' commands.  (It is ignored for other output
   generating commands.)  The width count operand must be all decimal
   digits.  It may be silently reduced to some lower value.  The default
   value for the width is 132.

7.13 1 - troff R font

      +---+------+----+
      | 1 | file | LF |
      +---+------+----+
      Command code - '1'
      Operand - File name

   This command specifies the file name for the troff R font.  [1] This
   is the font which is printed using Times Roman by default.




McLaughlin                                                      [Page 9]

RFC 1179                          LPR                        August 1990


7.14 2 - troff I font

      +---+------+----+
      | 2 | file | LF |
      +---+------+----+
      Command code - '2'
      Operand - File name

   This command specifies the file name for the troff I font.  [1] This
   is the font which is printed using Times Italic by default.

7.15 3 - troff B font

      +---+------+----+
      | 3 | file | LF |
      +---+------+----+
      Command code - '3'
      Operand - File name

   This command specifies the file name for the troff B font.  [1] This
   is the font which is printed using Times Bold by default.

7.16 4 - troff S font

      +---+------+----+
      | 4 | file | LF |
      +---+------+----+
      Command code - '4'
      Operand - File name

   This command specifies the file name for the troff S font.  [1] This
   is the font which is printed using Special Mathematical Font by
   default.

7.17 c - Plot CIF file

      +---+------+----+
      | c | file | LF |
      +---+------+----+
      Command code - 'c'
      Operand - File to plot

   This command causes the data file to be plotted, treating the data as
   CIF (CalTech Intermediate Form) graphics language. [2]







McLaughlin                                                     [Page 10]

RFC 1179                          LPR                        August 1990


7.18 d - Print DVI file

      +---+------+----+
      | d | file | LF |
      +---+------+----+
      Command code - 'd'
      Operand - File to print

   This command causes the data file to be printed, treating the data as
   DVI (TeX output). [3]

7.19 f - Print formatted file

      +---+------+----+
      | f | file | LF |
      +---+------+----+
      Command code - 'f'
      Operand - File to print

   This command cause the data file to be printed as a plain text file,
   providing page breaks as necessary.  Any ASCII control characters
   which are not in the following list are discarded: HT, CR, FF, LF,
   and BS.

7.20 g - Plot file

      +---+------+----+
      | g | file | LF |
      +---+------+----+
      Command code - 'g'
      Operand - File to plot

   This command causes the data file to be plotted, treating the data as
   output from the Berkeley Unix plot library. [1]

7.21 k - Reserved for use by Kerberized LPR clients and servers.

7.22 l - Print file leaving control characters

      +---+------+----+
      | l | file | LF |
      +---+------+----+
      Command code - 'l' (lower case L)
      Operand - File to print

   This command causes the specified data file to printed without
   filtering the control characters (as is done with the 'f' command).




McLaughlin                                                     [Page 11]

RFC 1179                          LPR                        August 1990


7.23 n - Print ditroff output file

      +---+------+----+
      | n | file | LF |
      +---+------+----+
      Command code - 'n'
      Operand - File to print

   This command prints the data file to be printed, treating the data as
   ditroff output. [4]

7.24 o - Print Postscript output file

      +---+------+----+
      | o | file | LF |
      +---+------+----+
      Command code - 'o'
      Operand - File to print

   This command prints the data file to be printed, treating the data as
   standard Postscript input.

7.25 p - Print file with 'pr' format

      +---+------+----+
      | p | file | LF |
      +---+------+----+
      Command code - 'p'
      Operand - File to print

   This command causes the data file to be printed with a heading, page
   numbers, and pagination.  The heading should include the date and
   time that printing was started, the title, and a page number
   identifier followed by the page number.  The title is the name of
   file as specified by the 'N' command, unless the 'T' command (title)
   has been given.  After a page of text has been printed, a new page is
   started with a new page number.  (There is no way to specify the
   length of the page.)

7.26 r - File to print with FORTRAN carriage control

      +---+------+----+
      | r | file | LF |
      +---+------+----+
      Command code - 'r'
      Operand - File to print

   This command causes the data file to be printed, interpreting the



McLaughlin                                                     [Page 12]

RFC 1179                          LPR                        August 1990


   first column of each line as FORTRAN carriage control.  The FORTRAN
   standard limits this to blank, "1", "0", and "+" carriage controls.
   Most FORTRAN programmers also expect "-" (triple space) to work as
   well.

7.27 t - Print troff output file

      +---+------+----+
      | t | file | LF |
      +---+------+----+
      Command code - 't'
      Operand - File to print

   This command prints the data file as Graphic Systems C/A/T
   phototypesetter input.  [5] This is the standard output of the Unix
   "troff" command.

7.28 v - Print raster file

      +---+------+----+
      | v | file | LF |
      +---+------+----+
      Command code - 'v'
      Operand - File to print

   This command prints a Sun raster format file. [6]

7.29 z - Reserved for future use with the Palladium print system.

REFERENCES and BIBLIOGRAPHY

   [1] Computer Science Research Group, "UNIX Programmer's Reference
       Manual", USENIX, 1986.

   [2] Hon and Sequin, "A Guide to LSI Implementation", XEROX PARC,
       1980.

   [3] Knuth, D., "TeX The Program".

   [4] Kernighan, B., "A Typesetter-independent TROFF".

   [5] "Model C/A/T Phototypesetter", Graphic Systems, Inc. Hudson, N.H.

   [6] Sun Microsystems, "Pixrect Reference Manual", Sun Microsystems,
       Mountain View, CA, 1988.






McLaughlin                                                     [Page 13]

RFC 1179                          LPR                        August 1990


Security Considerations

   Security issues are not discussed in this memo.

Author's Address

   Leo J. McLaughlin III
   The Wollongong Group
   1129 San Antonio Road
   Palo Alto, CA 94303

   Phone: 415-962-7100

   EMail: ljm@twg.com





































McLaughlin                                                     [Page 14]
