#!/usr/bin/perl

my @translator_help = (
 "#. The strings in e2fsck's problem.c can be very hard to translate,\n",
 "#. since the strings are expanded in two different ways.  First of all,\n",
 "#. there is an \@-expansion, where strings like \"\@i\" are expanded to\n",
 "#. \"inode\", and so on.  In order to make it easier for translators, the\n",
 "#. e2fsprogs po template file has been enhanced with comments that show\n",
 "#. the \@-expansion, for the strings in the problem.c file.\n",
 "#.\n",
 "#. Translators are free to use the \@-expansion facility if they so\n",
 "#. choose, by providing translations for strings in e2fsck/message.c.\n",
 "#. These translation can completely replace an expansion; for example,\n",
 "#. if \"bblock\" (which indicated that \"\@b\" would be expanded to \"block\")\n",
 "#. is translated as \"ddatenverlust\", then \"\@d\" will be expanded to\n",
 "#. \"datenverlust\".  Alternatively, translators can simply not use the\n",
 "#. \@-expansion facility at all.\n",
 "#.\n",
 "#. The second expansion which is done for e2fsck's problem.c messages is\n",
 "#. a dynamic %-expansion, which expands %i as an inode number, and so\n",
 "#. on.  A table of these expansions can be found below.  Note that\n",
 "#. %-expressions that begin with \"%D\" and \"%I\" are two-character\n",
 "#. expansions; so for example, \"%Iu\" expands to the inode's user id\n",
 "#. ownership field (inode->i_uid).\n",
 "#.  \n",
 "#.	%b	<blk>			block number\n",
 "#.	%B	<blkcount>		integer\n",
 "#.	%c	<blk2>			block number\n",
 "#.	%Di	<dirent> -> ino		inode number\n",
 "#.	%Dn	<dirent> -> name	string\n",
 "#.	%Dr	<dirent> -> rec_len\n",
 "#.	%Dl	<dirent> -> name_len\n",
 "#.	%Dt	<dirent> -> filetype\n",
 "#.	%d	<dir> 			inode number\n",
 "#.	%g	<group>			integer\n",
 "#.	%i	<ino>			inode number\n",
 "#.	%Is	<inode> -> i_size\n",
 "#.	%IS	<inode> -> i_extra_isize\n",
 "#.	%Ib	<inode> -> i_blocks\n",
 "#.	%Il	<inode> -> i_links_count\n",
 "#.	%Im	<inode> -> i_mode\n",
 "#.	%IM	<inode> -> i_mtime\n",
 "#.	%IF	<inode> -> i_faddr\n",
 "#.	%If	<inode> -> i_file_acl\n",
 "#.	%Id	<inode> -> i_dir_acl\n",
 "#.	%Iu	<inode> -> i_uid\n",
 "#.	%Ig	<inode> -> i_gid\n",
 "#.	%j	<ino2>			inode number\n",
 "#.	%m	<com_err error message>\n",
 "#.	%N	<num>\n",
 "#.	%p		ext2fs_get_pathname of directory <ino>\n",
 "#.	%P		ext2fs_get_pathname of <dirent>->ino with <ino2> as\n",
 "#.				the containing directory.  (If dirent is NULL\n",
 "#.				then return the pathname of directory <ino2>)\n",
 "#.	%q		ext2fs_get_pathname of directory <dir>\n",
 "#.	%Q		ext2fs_get_pathname of directory <ino> with <dir> as\n",
 "#.				the containing directory.\n",
 "#.	%s	<str>			miscellaneous string\n",
 "#.	%S		backup superblock\n",
 "#.	%X	<num>	hexadecimal format\n",
 "#.\n");

my $is_problem_file = 0;
my $save_msg;
my $msg_accum = "";
my $msg;
my $expanded = 0;
my $lines = 0;

sub do_expand {
    $msg =~ s/\@a/extended attribute/g;
    $msg =~ s/\@A/error allocating/g;
    $msg =~ s/\@b/block/g;
    $msg =~ s/\@B/bitmap/g;
    $msg =~ s/\@c/compress/g;
    $msg =~ s/\@C/conflicts with some other fs block/g;
    $msg =~ s/\@i/inode/g;
    $msg =~ s/\@I/illegal/g;
    $msg =~ s/\@j/journal/g;
    $msg =~ s/\@D/deleted/g;
    $msg =~ s/\@d/directory/g;
    $msg =~ s/\@e/entry/g;
    $msg =~ s/\@E/entry '%Dn' in %p (%i)/g;
    $msg =~ s/\@f/filesystem/g;
    $msg =~ s/\@F/for inode %i (%Q) is/g;
    $msg =~ s/\@g/group/g;
    $msg =~ s/\@h/HTREE directory inode/g;
    $msg =~ s/\@l/lost+found/g;
    $msg =~ s/\@L/is a link/g;
    $msg =~ s/\@m/multiply-claimed/g;
    $msg =~ s/\@n/invalid/g;
    $msg =~ s/\@o/orphaned/g;
    $msg =~ s/\@p/problem in/g;
    $msg =~ s/\@q/quota/g;
    $msg =~ s/\@r/root inode/g;
    $msg =~ s/\@s/should be/g;
    $msg =~ s/\@S/superblock/g;
    $msg =~ s/\@u/unattached/g;
    $msg =~ s/\@v/device/g;
    $msg =~ s/\@x/extent/g;
    $msg =~ s/\@z/zero-length/g;
    $msg =~ s/\@\@/@/g;
}


while (<>) {
    $lines++;
    if ($lines == 6) {
	print @translator_help;
    }
    if (/^#: /)
    {
	$is_problem_file = (/^#: e2fsck\/problem/) ? 1 : 0;
    }
    $msg = "";
    if (/^msgid / && $is_problem_file) {
	($msg) = /^msgid "(.*)"$/;
	$save_msgid = $_;
	if ($msg =~ /\@/) {
	    $expanded++;
	}
	&do_expand();
	if ($msg ne "") {
	    $msg_accum = $msg_accum . "#. \@-expanded: $msg\n";
	}
	next;
    }
    if (/^"/ && $is_problem_file) {
	($msg) = /^"(.*)"$/;
	$save_msgid = $save_msgid . $_;
	if ($msg =~ /\@/) {
	    $expanded++;
	}
	&do_expand();
	$msg_accum = $msg_accum . "#. \@-expanded: $msg\n";
	next;
    }
    if (/^msgstr / && $is_problem_file) {
	if ($expanded) {
	    print $msg_accum;
	}
	print $save_msgid;
	$msg_accum = "";
	$expanded = 0;
    }
    print $_;
}
