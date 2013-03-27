BEGIN	{
	  FS="\"";
	  print "/* ==> Do not modify this file!!  It is created automatically";
	  print "   by copying.awk.  Modify copying.awk instead.  <== */";
	  print ""
	  print "#include \"defs.h\""
	  print "#include \"command.h\""
	  print "#include \"gdbcmd.h\""
	  print ""
	  print "static void show_copying_command (char *, int);"
	  print ""
	  print "static void show_warranty_command (char *, int);"
	  print ""
	  print "void _initialize_copying (void);"
	  print ""
	  print "extern int immediate_quit;";
	  print "static void";
	  print "show_copying_command (char *ignore, int from_tty)";
	  print "{";
	  print "  immediate_quit++;";
	}
NR == 1,/^[ 	]*15\. Disclaimer of Warranty\.[ 	]*$/	{
	  if ($0 ~ //)
	    {
	      printf "  printf_filtered (\"\\n\");\n";
	    }
	  else if ($0 !~ /^[ 	]*15\. Disclaimer of Warranty\.[ 	]*$/) 
	    {
	      printf "  printf_filtered (\"";
	      for (i = 1; i < NF; i++)
		printf "%s\\\"", $i;
	      printf "%s\\n\");\n", $NF;
	    }
	}
/^[	 ]*15\. Disclaimer of Warranty\.[ 	]*$/	{
	  print "  immediate_quit--;";
	  print "}";
	  print "";
	  print "static void";
	  print "show_warranty_command (char *ignore, int from_tty)";
	  print "{";
	  print "  immediate_quit++;";
	}
/^[ 	]*15\. Disclaimer of Warranty\.[ 	]*$/, /^[ 	]*END OF TERMS AND CONDITIONS[ 	]*$/{  
	  if (! ($0 ~ /^[ 	]*END OF TERMS AND CONDITIONS[ 	]*$/)) 
	    {
	      printf "  printf_filtered (\"";
	      for (i = 1; i < NF; i++)
		printf "%s\\\"", $i;
	      printf "%s\\n\");\n", $NF;
	    }
	}
END	{
	  print "  immediate_quit--;";
	  print "}";
	  print "";
	  print "void"
	  print "_initialize_copying (void)";
	  print "{";
	  print "  add_cmd (\"copying\", no_class, show_copying_command,";
	  print "	   _(\"Conditions for redistributing copies of GDB.\"),";
	  print "	   &showlist);";
	  print "  add_cmd (\"warranty\", no_class, show_warranty_command,";
	  print "	   _(\"Various kinds of warranty you do not have.\"),";
	  print "	   &showlist);";
	  print "";
	  print "  /* For old-timers, allow \"info copying\", etc.  */";
	  print "  add_info (\"copying\", show_copying_command,";
	  print "	    _(\"Conditions for redistributing copies of GDB.\"));";
	  print "  add_info (\"warranty\", show_warranty_command,";
	  print "	    _(\"Various kinds of warranty you do not have.\"));";
	  print "}";
	}
