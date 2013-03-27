/*
 * Program to generate the so-thresh testcase,
 * including associated linked-against shared libraries.
 * Build as:
 *
 *        cc -g -o genso-thresh genso-thresh.c
 *
 * Invoke as:
 *
 *        genso-thresh
 *
 * It will put all the code in the current directory (".").
 *
 * A makefile can also be generated if the -makemake option is used.
 * To use the makefile:
 *
 *        make -f so-thresh.mk all
 *
 * The name of the application is
 *
 *        so-thresh
 *
 * (Revised from a program by John Bishop.  --rehrauer)
 */

#include <stdio.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

int main (argc, argv)
int    argc;
char **argv;
{
#define NUMBER_OF_INT_VARS 1500
#define NUMBER_OF_LIBS 3
    int     lib_num = NUMBER_OF_LIBS;
    int     i;
    int     i2;
    FILE   *main_file;
    FILE   *lib_file;
    FILE   *make_file;
    FILE   *link_file;

    char  testcase_name [1000];
    char  linkfile_name [1000];
    char  makefile_name [1000];
    char  mainfile_name [1000];

    char    file_name[100];
    /*
     *        0123456789       <-- length of field
     *  "./fil0000000002.c";   <-- typical filename
     *   12345678901234567890  <-- length of string
     *           10        20
     *                     ^where null goes
     */
    char    file_name_core[100];

    /* Verify input.
    */
    if ((argc < 1) || (argc > 2) || (argv == NULL) ||
        ((argc == 2) && (strcmp (argv[1], "-makemake") != 0)))
      {
        printf ("** Syntax: %s [-makemake]\n", argv[0]);
        return;
      }

    if (strncmp (argv[0], "gen", 3) != 0)
      {
        printf ("** This tool expected to be named \"gen<something>\"\n");
        return;
      }
    strcpy (testcase_name, argv[0]+3);

    strcpy (linkfile_name, testcase_name);
    strcat (linkfile_name, ".lopt");
    link_file = fopen (linkfile_name, "w");
    fprintf (link_file, "# Linker options for %s test\n", testcase_name);
    
    /* Generate the makefile, if requested.
       */
    if (argc == 2)
      {
        strcpy (makefile_name, testcase_name);
        strcat (makefile_name, ".mk.new");
        make_file = fopen (makefile_name, "w");
        printf ("  Note: New makefile (%s) generated.\n", makefile_name);
        printf ("  May want to update existing makefile, if any.\n");
        fprintf (make_file, "# Generated automatically by %s\n", argv[0]);
        fprintf (make_file, "# Make file for %s test\n", testcase_name);
        fprintf (make_file, "\n");
        fprintf (make_file, "CFLAGS = +DA1.1 -g\n");
        fprintf (make_file, "\n");
        fprintf (make_file, "# This is how to build this generator.\n");
        fprintf (make_file, "%s.o: %s.c\n", argv[0], argv[0]);
        fprintf (make_file, "\t$(CC) $(CFLAGS) -o %s.o -c %s.c\n", argv[0], argv[0]);
        fprintf (make_file, "%s: %s.o\n", argv[0], argv[0]);
        fprintf (make_file, "\t$(CC) $(CFLAGS) -o %s %s.o\n", argv[0], argv[0]);
        fprintf (make_file, "\n");
        fprintf (make_file, "# This is how to run this generator.\n");
        fprintf (make_file, "# This target should be made before the 'all' target,\n");
        fprintf (make_file, "# to ensure that the shlib sources are all available.\n");
        fprintf (make_file, "require_shlibs: %s\n", argv[0]);
        for (i=0; i < lib_num; i++)
          {
            fprintf (make_file, "\tif ! [ -a lib%2.2d_%s.c ] ; then \\\n", i, testcase_name);
            fprintf (make_file, "\t  %s ; \\\n", argv[0]);
            fprintf (make_file, "\tfi\n");
          }
        fprintf (make_file, "\n");
        fprintf (make_file, "# This is how to build all the shlibs.\n");
        fprintf (make_file, "# Be sure to first make the require_shlibs target!\n");
        for (i=0; i < lib_num; i++)
          {
            fprintf (make_file, "lib%2.2d_%s.o: lib%2.2d_%s.c\n", i, testcase_name, i, testcase_name);
            fprintf (make_file, "\t$(CC) $(CFLAGS) +Z -o lib%2.2d_%s.o -c lib%2.2d_%s.c\n", i, testcase_name, i, testcase_name);
            fprintf (make_file, "lib%2.2d-%s.sl: lib%2.2d-%s.o\n", i, testcase_name, i, testcase_name);
            fprintf (make_file, "\t$(LD) $(LDFLAGS) -b -o lib%2.2d-%s.sl lib%2.2d-%s.o\n", i, testcase_name, i, testcase_name);
          }
        fprintf (make_file, "\n");
fprintf (make_file, "# For convenience, here's names for all pieces of all shlibs.\n");
        fprintf (make_file, "SHLIB_SOURCES = \\\n");
        for (i=0; i < lib_num-1; i++)
          fprintf (make_file, "\tlib%2.2d-%s.c \\\n", i, testcase_name);
        fprintf (make_file, "\tlib%2.2d-%s.c\n", lib_num-1, testcase_name);
        fprintf (make_file, "SHLIB_OBJECTS = $(SHLIB_SOURCES:.c=.o)\n");
        fprintf (make_file, "SHLIBS = $(SHLIB_SOURCES:.c=.sl)\n");
        fprintf (make_file, "SHLIB_NAMES = $(SHLIB_SOURCES:.c=)\n");
        fprintf (make_file, "EXECUTABLES = $(SHLIBS) %s %s\n", argv[0], testcase_name);
        fprintf (make_file, "OBJECT_FILES = $(SHLIB_OBJECTS) %s.o %s.o\n", argv[0], testcase_name);
        fprintf (make_file, "\n");
        fprintf (make_file, "shlib_objects: $(SHLIB_OBJECTS)\n");
        fprintf (make_file, "shlibs: $(SHLIBS)\n");
        fprintf (make_file, "\n");
        fprintf (make_file, "# This is how to build the debuggable testcase that uses the shlibs.\n");
        fprintf (make_file, "%s.o: %s.c\n", testcase_name, testcase_name);
        fprintf (make_file, "\t$(CC) $(CFLAGS) -o %s.o -c %s.c\n", testcase_name, testcase_name);
        fprintf (make_file, "%s: shlibs %s.o\n", testcase_name, testcase_name);
        fprintf (make_file, "\t$(LD) $(LDFLAGS) -o %s -lc -L. ", testcase_name);
        fprintf (make_file, "-c %s /opt/langtools/lib/end.o /lib/crt0.o %s.o\n", linkfile_name, testcase_name);
        fprintf (make_file, "\n");
        fprintf (make_file, "# Yeah, but you should first make the require_shlibs target!\n");
        fprintf (make_file, "all: %s %s\n", testcase_name, argv[0]);
        fprintf (make_file, "\n");
        fprintf (make_file, "# To remove everything built via this makefile...\n");
        fprintf (make_file, "clean:\n");
        /* Do this carefully, to avoid hitting silly HP-UX ARG_MAX limits... */
        fprintf (make_file, "\trm -f lib0*-%s.*\n", testcase_name);
        fprintf (make_file, "\trm -f lib1*-%s.*\n", testcase_name);
        fprintf (make_file, "\trm -f lib2*-%s.*\n", testcase_name);
        fprintf (make_file, "\trm -f lib3*-%s.*\n", testcase_name);
        fprintf (make_file, "\trm -f lib4*-%s.*\n", testcase_name);
        fprintf (make_file, "\trm -f lib5*-%s.*\n", testcase_name);
        fprintf (make_file, "\trm -f lib6*-%s.*\n", testcase_name);
        fprintf (make_file, "\trm -f lib7*-%s.*\n", testcase_name);
        fprintf (make_file, "\trm -f lib8*-%s.*\n", testcase_name);
        fprintf (make_file, "\trm -f lib9*-%s.*\n", testcase_name);
        fprintf (make_file, "\trm -f %s %s %s %s.c\n", argv[0], testcase_name, linkfile_name, testcase_name);
        fprintf (make_file, "\n");
        fclose (make_file);
      }

    /* Generate the code for the libraries.
       */
    for (i=0; i < lib_num; i++) {

        /* Generate the names for the library.
         */
        sprintf (file_name, "lib%2.2d-%s.c", i, testcase_name);
        sprintf (file_name_core, "lib%2.2d-%s", i, testcase_name);

        /* Generate the source code.
         */
        lib_file = fopen (file_name, "w");
        fprintf (lib_file, "/* Shared library file number %d */\n", i);
        fprintf (lib_file, "#include <stdio.h>\n\n");
        fprintf (lib_file, "/* The following variables largely exist to bloat this library's debug info. */\n");
        fprintf (lib_file, "static char c_static_buf_%d [100];\n", i);
        for (i2=0; i2<NUMBER_OF_INT_VARS; i2++)
          fprintf (lib_file, "int i_%d_%d;\n", i, i2);
        fprintf (lib_file, "\nint r_%d ()\n", i);
        fprintf (lib_file, "{\n");
        for (i2=0; i2<NUMBER_OF_INT_VARS; i2++)
          fprintf (lib_file, "    i_%d_%d = %d*%d;\n", i, i2, i2, i2);
        fprintf (lib_file, "    return 1;\n");
        fprintf (lib_file, "}\n\n");
        fprintf (lib_file, "/* end of generated file */\n");
        fclose (lib_file);

        /* Add a linker options line
           */
        fprintf (link_file, "-l%2.2d-%s\n", i, testcase_name);
    }

    /* Generate the "main" file.
     */
    strcpy (mainfile_name, testcase_name);
    strcat (mainfile_name, ".c");
    main_file = fopen (mainfile_name, "w");
    fprintf (main_file, "/* Generated test progam with %d shared libraries. */\n\n",
             lib_num);
    fprintf (main_file, "#include <stdio.h>\n\n");

    for (i = 0; i < lib_num; i++) {
      fprintf (main_file, "extern int r_%d();\n", i);
    }

    fprintf (main_file, "\n");
    fprintf (main_file, "int main()\n");
    fprintf (main_file, "{\n");
    fprintf (main_file, "    int accum;\n");
    fprintf (main_file, "    int lib_num = %d;\n", lib_num);
  
    for (i = 0; i < lib_num; i++) {
      fprintf (main_file, "    accum += r_%d();\n", i);
    }

    fprintf (main_file, "    printf( \"Final value: %%d, should be %%d\\n\", accum, lib_num );\n\n");
    fprintf (main_file, "    return 0;\n");
    fprintf (main_file, "}\n\n");
    fprintf (main_file, "/* end of generated file */\n");
    fclose (main_file);

    /* Finish up the link file and the build file
     */
    fclose (link_file);
}

/* End of file */
