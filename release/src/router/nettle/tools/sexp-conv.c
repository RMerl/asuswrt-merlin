/* sexp-conv.c

   Conversion tool for handling the different flavours of sexp syntax.

   Copyright (C) 2002 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_FCNTL_LOCKING
# if HAVE_SYS_TYPES_H
#  include <sys/types.h>
# endif
# if HAVE_UNISTD_H
#  include <unistd.h>
# endif
# include <fcntl.h>
#endif

#include "buffer.h"
#include "nettle-meta.h"

#include "getopt.h"

#include "input.h"
#include "output.h"
#include "parse.h"

#define BUG_ADDRESS "nettle-bugs@lists.lysator.liu.se"


/* Conversion functions. */

/* Should be called with input->token being the first token of the
 * expression, to be converted, and return with input->token being the
 * last token of the expression. */
static void
sexp_convert_item(struct sexp_parser *parser,
		  struct sexp_compound_token *token,
		  struct sexp_output *output, enum sexp_mode mode_out,
		  unsigned indent)
{
  if (mode_out == SEXP_TRANSPORT)
    {
      sexp_put_char(output, '{');
      sexp_put_code_start(output, &nettle_base64);
      sexp_convert_item(parser, token, output, SEXP_CANONICAL, 0);
      sexp_put_code_end(output);
      sexp_put_char(output, '}');
    }
  else switch(token->type)
    {
    case SEXP_LIST_END:
      die("Unmatched end of list.\n");
    case SEXP_EOF:
      die("Unexpected end of file.\n");
    case SEXP_CODING_END:
      die("Unexpected end of coding.\n");

    case SEXP_LIST_START:
      {
	unsigned item;

	sexp_put_char(output, '(');
  
	for (item = 0;
	     sexp_parse(parser, token), token->type != SEXP_LIST_END;
	     item++)
	  {
	    if (mode_out == SEXP_ADVANCED)
	      {
		/* FIXME: Adapt pretty printing to handle a big first
		 * element. */
		switch (item)
		  {
		  case 0:
		    if (token->type == SEXP_COMMENT)
		      {
			indent = output->pos;
			/* Disable the indentation setup for next item */
			item++;
		      }
		    break;
		    
		  case  1:
		    sexp_put_char(output, ' ');
		    indent = output->pos;
		    break;

		  default:
		    sexp_put_newline(output, indent);
		    break;
		  }
	      }

	    sexp_convert_item(parser, token, output, mode_out, indent);
	  }
	sexp_put_char(output, ')');

	break;
      }      
      
    case SEXP_STRING:
      sexp_put_string(output, mode_out, &token->string);
      break;

    case SEXP_DISPLAY:
      sexp_put_char(output, '[');
      sexp_put_string(output, mode_out, &token->display);
      sexp_put_char(output, ']');
      sexp_put_string(output, mode_out, &token->string);      
      break;

    case SEXP_COMMENT:
      if (mode_out == SEXP_ADVANCED)
	{
	  sexp_put_data(output, token->string.size, token->string.contents);
	  sexp_put_soft_newline(output, indent);
	}
      break;
    default:
      /* Internal error */
      abort();
    }
}


/* Argument parsing and main program */

/* The old lsh sexp-conv program took the following options:
 *
 * Usage: sexp-conv [OPTION...]
 *             Conversion: sexp-conv [options] <INPUT-SEXP >OUTPUT
 *   or:  sexp-conv [OPTION...]
 *             Fingerprinting: sexp-conv --raw-hash [ --hash=ALGORITHM ]
 *             <PUBLIC-KEY
 * Reads an s-expression on stdin, and outputs the same s-expression on stdout,
 * possibly using a different encoding. By default, output uses the advanced
 * encoding. 
 * 
 *       --hash=Algorithm       Hash algorithm (default sha1).
 *       --once                 Process exactly one s-expression.
 *       --raw-hash             Output the hash for the canonical representation
 *                              of the object, in hexadecimal.
 *       --replace=Substitution An expression `/before/after/' replaces all
 *                              occurances of the atom `before' with `after'. The
 *                              delimiter `/' can be any single character.
 *       --select=Operator      Select a subexpression (e.g `caddr') for
 *                              processing.
 *       --spki-hash            Output an SPKI hash for the object.
 *       --debug                Print huge amounts of debug information
 *       --log-file=File name   Append messages to this file.
 *   -q, --quiet                Suppress all warnings and diagnostic messages
 *       --trace                Detailed trace
 *   -v, --verbose              Verbose diagnostic messages
 * 
 *  Valid sexp-formats are transport, canonical, advanced, and international.
 * 
 *  Valid sexp-formats are transport, canonical, advanced, advanced-hex and
 *  international.
 *   -f, --output-format=format Variant of the s-expression syntax to generate.
 *   -i, --input-format=format  Variant of the s-expression syntax to accept.
 * 
 *   -?, --help                 Give this help list
 *       --usage                Give a short usage message
 *   -V, --version              Print program version
 */ 

struct conv_options
{
  /* Output mode */
  enum sexp_mode mode;
  int prefer_hex;
  int once;
  int lock;
  unsigned width;
  const struct nettle_hash *hash;
};

enum { OPT_ONCE = 300, OPT_HASH, OPT_LOCK, OPT_HELP };

static int
match_argument(const char *given, const char *name)
{
  /* FIXME: Allow abbreviations */
  return !strcmp(given, name);
}

static void
parse_options(struct conv_options *o,
	      int argc, char **argv)
{  
  o->mode = SEXP_ADVANCED;
  o->prefer_hex = 0;
  o->once = 0;
  o->lock = 0;
  o->hash = NULL;
  o->width = 72;
  
  for (;;)
    {
      static const struct nettle_hash *hashes[] =
	{ &nettle_md5, &nettle_sha1, &nettle_sha256, NULL };
  
      static const struct option options[] =
	{
	  /* Name, args, flag, val */
	  { "help", no_argument, NULL, OPT_HELP },
	  { "version", no_argument, NULL, 'V' },
	  { "once", no_argument, NULL, OPT_ONCE },
	  { "syntax", required_argument, NULL, 's' },
	  { "hash", optional_argument, NULL, OPT_HASH },
	  { "raw-hash", optional_argument, NULL, OPT_HASH },
	  { "width", required_argument, NULL, 'w' },
#if HAVE_FCNTL_LOCKING
	  { "lock", no_argument, NULL, OPT_LOCK },
#endif
#if 0
	  /* Not yet implemented */
	  { "replace", required_argument, NULL, OPT_REPLACE },
	  { "select", required_argument, NULL, OPT_SELECT },
	  { "spki-hash", optional_argument, NULL, OPT_SPKI_HASH },
#endif
	  { NULL, 0, NULL, 0 }
	};
      int c;
      int option_index = 0;
      unsigned i;
     
      c = getopt_long(argc, argv, "Vs:w:", options, &option_index);

      switch (c)
	{
	default:
	  abort();
	  
	case -1:
	  if (optind != argc)
	    die("sexp-conv: Command line takes no arguments, only options.\n");
	  return;

	case '?':
	  exit(EXIT_FAILURE);
	  
	case 'w':
	  {
	    char *end;
	    int width = strtol(optarg, &end , 0);
	    if (!*optarg || *end || width < 0)
	      die("sexp-conv: Invalid width `%s'.\n", optarg);

	    o->width = width;
	    break;
	  }
	case 's':
	  if (o->hash)
	    werror("sexp-conv: Combining --hash and -s usually makes no sense.\n");
	  if (match_argument(optarg, "advanced"))
	    o->mode = SEXP_ADVANCED;
	  else if (match_argument(optarg, "transport"))
	    o->mode = SEXP_TRANSPORT;
	  else if (match_argument(optarg, "canonical"))
	    o->mode = SEXP_CANONICAL;
	  else if (match_argument(optarg, "hex"))
	    {
	      o->mode = SEXP_ADVANCED;
	      o->prefer_hex = 1;
	    }
	  else
	    die("Available syntax variants: advanced, transport, canonical\n");
	  break;

	case OPT_ONCE:
	  o->once = 1;
	  break;
	
	case OPT_HASH:
	  o->mode = SEXP_CANONICAL;
	  if (!optarg)
	    o->hash = &nettle_sha1;
	  else
	    for (i = 0;; i++)
	      {
		if (!hashes[i])
		  die("sexp_conv: Unknown hash algorithm `%s'\n",
		      optarg);
	      
		if (match_argument(optarg, hashes[i]->name))
		  {
		    o->hash = hashes[i];
		    break;
		  }
	      }
	  break;
#if HAVE_FCNTL_LOCKING
	case OPT_LOCK:
	  o->lock = 1;
	  break;
#endif
	case OPT_HELP:
	  printf("Usage: sexp-conv [OPTION...]\n"
		 "  Conversion:     sexp-conv [OPTION...] <INPUT-SEXP\n"
		 "  Fingerprinting: sexp-conv --hash=HASH <INPUT-SEXP\n\n"
		 "Reads an s-expression on stdin, and outputs the same\n"
		 "sexp on stdout, possibly with a different syntax.\n\n"
		 "       --hash[=ALGORITHM]   Outputs only the hash of the expression.\n"
		 "                            Available hash algorithms:\n"
		 "                            ");
	  for(i = 0; hashes[i]; i++)
	    {
	      if (i) printf(", ");
	      printf("%s", hashes[i]->name);
	    }
	  printf(" (default is sha1).\n"
		 "   -s, --syntax=SYNTAX      The syntax used for the output. Available\n"
		 "                            variants: advanced, hex, transport, canonical\n"
		 "       --once               Process only the first s-expression.\n"
		 "   -w, --width=WIDTH        Linewidth for base64 encoded data.\n"
		 "                            Zero means no limit.\n"
#if HAVE_FCNTL_LOCKING
		 "       --lock               Lock output file.\n"
#endif
		 "       --raw-hash           Alias for --hash, for compatibility\n"
		 "                            with lsh-1.x.\n\n"
		 "Report bugs to " BUG_ADDRESS ".\n");
	  exit(EXIT_SUCCESS);

	case 'V':
	  printf("sexp-conv (" PACKAGE_STRING ")\n");
	  exit (EXIT_SUCCESS);
	}
    }
}

int
main(int argc, char **argv)
{
  struct conv_options options;
  struct sexp_input input;
  struct sexp_parser parser;
  struct sexp_compound_token token;
  struct sexp_output output;

  parse_options(&options, argc, argv);

  sexp_input_init(&input, stdin);
  sexp_parse_init(&parser, &input, SEXP_ADVANCED);
  sexp_compound_token_init(&token);
  sexp_output_init(&output, stdout,
		   options.width, options.prefer_hex);

#if HAVE_FCNTL_LOCKING
  if (options.lock)
    {
      struct flock fl;
  
      memset(&fl, 0, sizeof(fl));
      fl.l_type = F_WRLCK;
      fl.l_whence = SEEK_SET;
      fl.l_start = 0;
      fl.l_len = 0; /* Means entire file. */
      
      if (fcntl(STDOUT_FILENO, F_SETLKW, &fl) == -1)
	die("Locking output file failed: %s\n", strerror(errno));
    }
#endif /* HAVE_FCNTL_LOCKING */
  if (options.hash)
    {
      /* Leaks the context, but that doesn't matter */
      void *ctx = xalloc(options.hash->context_size);
      sexp_output_hash_init(&output, options.hash, ctx);
    }
  
  sexp_get_char(&input);
  
  sexp_parse(&parser, &token);
  
  if (token.type == SEXP_EOF)
    {
      if (options.once)
	die("sexp-conv: No input expression.\n");
      return EXIT_SUCCESS;
    }
  
  do 
    {
      sexp_convert_item(&parser, &token, &output, options.mode, 0);
      if (options.hash)
	{
	  sexp_put_digest(&output);
	  sexp_put_newline(&output, 0);
	}
      else if (options.mode != SEXP_CANONICAL)
	sexp_put_newline(&output, 0);
	  
      sexp_parse(&parser, &token);
    }
  while (!options.once && token.type != SEXP_EOF);

  sexp_compound_token_clear(&token);
  
  if (fflush(output.f) < 0)
    die("Final fflush failed: %s.\n", strerror(errno));
  
  return EXIT_SUCCESS;
}
