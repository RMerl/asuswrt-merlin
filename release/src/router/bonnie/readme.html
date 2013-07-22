<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<HTML>
<HEAD><TITLE>Bonnie++ Documentation</TITLE></HEAD>
<BODY>

<UL><LI><B>Introduction</B><BR>
This benchmark is named <B>Bonnie++</B>, it is based on the <B>Bonnie</B>
benchmark written by <A HREF="MAILTO:tbray@textuality.com">Tim Bray</A>. I was
originally hoping to work with Tim on developing the
next version of Bonnie, but we could not agree on the issue of whether C++
should be used in the program. Tim has graciously given me permission to use
the name "bonnie++" for my program which is based around his benchmark.<BR>
Bonnie++ adds the facility to test more than 2G of storage on a 32bit
machine, and tests for file creat(), stat(), unlink() operations.<BR>
Also it will output in CSV spread-sheet format to standard output. If you use
the "-q" option for quiet mode then the human-readable version will go to
stderr so redirecting stdout to a file will get only the csv in the file.
The program bon_csv2html takes csv format data on stdin and writes a HTML
file on standard output which has a nice display of all the data. The program
bon_csv2txt takes csv format data on stdin and writes a formatted plain text
version on stdout, this was originally written to work with 80 column braille
displays, but can also work well in email.<BR></LI>
<LI><B>A note on blocking writes</B><BR>
I have recently added a <B>-b</B> option to cause a fsync() after every
write (and a fsync() of the directory after file create or delete). This is
what you probably want to do if testing performance of mail or database
servers as they like to sync everything. The default is to allow write-back
caching in the OS which is what you want if testing performance for copying
files, compiling, etc.<BR></LI>
<LI><B>Waiting for semaphores</B><BR>
There is often a need to test multiple types of IO at the same time. This is
most important for testing RAID arrays where you will almost never see full
performance with only one process active. Bonnie++ 2.0 will address this
issue, but there is also a need for more flexibility than the ability to
create multiple files in the same directory and fork processes to access
them (which is what version 2.0 will do). There is also need to perform tests
such as determining whether access to an NFS server will load the system and
slow down access to a local hard drive. <A HREF="c.kagerhuber@t-online.net">
Christian Kagerhuber</A> contributed the initial code to do semaphores so that
several copies of Bonnie++ can be run in a synchronised fashion.  This means
you can have 8 copies of Bonnie++ doing per-char reads to test out your 8CPU
system!</LI>
<LI><B>Summary of tests</B><BR>
The first 6 tests are from the original Bonnie: Specifically, these are the
types of filesystem activity that have been observed to be bottlenecks in
I/O-intensive applications, in particular the text database work done in
connection with the New Oxford English Dictionary Project at the University
of Waterloo.<BR>
It initially performs a series of tests on a file (or files) of known size.
By default, that size is 200 MiB (but that's not enough - see below). For
each test, Bonnie reports the number of Kilo-bytes processed per elapsed
second, and the % CPU usage (sum of user and system). If a size &gt;1G is
specified then we will use a number of files of size 1G or less. This way
we can use a 32bit program to test machines with 8G of RAM! NB I have not
yet tested more than 2100M of file storage. If you test with larger storage
then this please send me the results.<BR>
The next 6 tests involve file create/stat/unlink to simulate some operations
that are common bottlenecks on large Squid and INN servers, and machines with
tens of thousands of mail files in /var/spool/mail.<BR>
In each case, an attempt is made to keep optimizers from noticing it's
all bogus. The idea is to make sure that these are real transfers to/from
user space to the physical disk.<P></LI>
<LI><B>Test Details</B><BR>
<UL><LI>The file IO tests are:
<OL>
<LI><B>Sequential Output</B>
<OL>
<LI>Per-Character. The file is written using the putc() stdio macro.
The loop that does the writing should be small enough to fit into any
reasonable I-cache. The CPU overhead here is that required to do the
stdio code plus the OS file space allocation.</LI>

<LI>Block. The file is created using write(2). The CPU overhead
should be just the OS file space allocation.</LI>

<LI>Rewrite. Each BUFSIZ of the file is read with read(2), dirtied, and
rewritten with write(2), requiring an lseek(2). Since no space
allocation is done, and the I/O is well-localized, this should test the
effectiveness of the filesystem cache and the speed of data transfer.</LI>
</OL>
</LI>

<LI><B>Sequential Input</B>
<OL>
<LI>Per-Character. The file is read using the getc() stdio macro. Once
again, the inner loop is small. This should exercise only stdio and
sequential input.</LI>

<LI>Block. The file is read using read(2). This should be a very pure
test of sequential input performance.</LI>
</OL>
</LI>

<LI><B>Random Seeks</B><BR>

This test runs SeekProcCount processes (default 3) in parallel, doing a total of
8000 lseek()s to locations in the file specified by random() in bsd systems,
drand48() on sysV systems. In each case, the block is read with read(2).
In 10% of cases, it is dirtied and written back with write(2).<BR>

The idea behind the SeekProcCount processes is to make sure there's always
a seek queued up.<BR>

AXIOM: For any unix filesystem, the effective number of lseek(2) calls
per second declines asymptotically to near 30, once the effect of
caching is defeated.<BR>
One thing to note about this is that the number of disks in a RAID set
increases the number of seeks. For read using RAID-1 (mirroring) will
double the number of seeks. For write using RAID-0 will multiply the number
of writes by the number of disks in the RAID-0 set (provided that enough
seek processes exist).<BR>

The size of the file has a strong nonlinear effect on the results of
this test. Many Unix systems that have the memory available will make
aggressive efforts to cache the whole thing, and report random I/O rates
in the thousands per second, which is ridiculous. As an extreme
example, an IBM RISC 6000 with 64 MiB of memory reported 3,722 per second
on a 50 MiB file. Some have argued that bypassing the cache is artificial
since the cache is just doing what it's designed to. True, but in any
application that requires rapid random access to file(s) significantly
larger than main memory which is running on a system which is doing
significant other work, the caches will inevitably max out. There is
a hard limit hiding behind the cache which has been observed by the
author to be of significant import in many situations - what we are trying
to do here is measure that number.</LI>
</OL>
</LI>

<LI>
The file creation tests use file names with 7 digits numbers and a random
number (from 0 to 12) of random alpha-numeric characters.
For the sequential tests the random characters in the file name follow the
number. For the random tests the random characters are first.<BR>
The sequential tests involve creating the files in numeric order, then
stat()ing them in readdir() order (IE the order they are stored in the
directory which is very likely to be the same order as which they were
created), and deleting them in the same order.<BR>
For the random tests we create the files in an order that will appear
random to the file system (the last 7 characters are in numeric order on
the files). Then we stat() random files (NB this will return very good
results on file systems with sorted directories because not every file
will be stat()ed and the cache will be more effective). After that we
delete all the files in random order.<BR>
If a maximum size greater than 0 is specified then when each file is created
it will have a random amount of data written to it. Then when the file is
stat()ed it's data will be read.
</LI>
</UL>
</LI>
<LI><B>COPYRIGHT NOTICE</B><BR>
* Copyright &copy; <A HREF="MAILTO:tbray@textuality.com">Tim Bray
(tbray@textuality.com)</A>, 1990.<BR>
* Copyright &copy; <A HREF="MAILTO:russell@coker.com.au">Russell Coker
(russell@coker.com.au)</A> 1999.<P>
I have updated the program, added support for &gt;2G on 32bit machines, and
tests for file creation.<BR>
Licensed under the GPL version 2.0.
</LI><LI>
<B>DISCLAIMER</B><BR>
This program is provided AS IS with no warranty of any kind, and<BR>
The author makes no representation with respect to the adequacy of this
program for any particular purpose or with respect to its adequacy to
produce any particular result, and<BR>
The authors shall not be liable for loss or damage arising out of
the use of this program regardless of how sustained, and
In no event shall the author be liable for special, direct, indirect
or consequential damage, loss, costs or fees or expenses of any
nature or kind.<P>

NB The results of running this program on live server machines can include
extremely bad performance of server processes, and excessive consumption of
disk space and/or Inodes which may cause the machine to cease performing it's
designated tasks. Also the benchmark results are likely to be bad.<P>
Do not run this program on live production machines.
</LI>
</UL>
</BODY>
</HTML>
