#include "config.h"

#include "i18n.h"
#include <locale.h>

#include <stdio.h>
#include <string.h>

struct _testcase {
  char *locale;
  char *untranslated;
  char *expected;
};

typedef struct _testcase testcase;

/* Translators: Just ignore the stuff in the test subdirectory. */
static testcase testcases[] = {
  { "de_DE.UTF-8", 
    N_("[DO_NOT_TRANSLATE_THIS_MARKER]"), 
       "[DO_NOT_TRANSLATE_THIS_MARKER_de]" },
  { "C",  
    N_("[DO_NOT_TRANSLATE_THIS_MARKER]"), 
    N_("[DO_NOT_TRANSLATE_THIS_MARKER]") },
};

int main(int argc, char *argv[])
{
  char *localedir;
  int i;

  if (argc != 2) {
    puts("Syntax: test-nls <localedir>\n");
    return 1;
  }

  localedir = argv[1];

  do {
    const char *newloc = setlocale(LC_ALL, NULL);
    printf("Default locale: %s\n", newloc);
  } while (0);


  for (i=0; i < sizeof(testcases)/sizeof(testcases[0]); i++) {
    char *locale       = testcases[i].locale;
    char *untranslated = testcases[i].untranslated;
    char *expected     = testcases[i].expected;
    char *translation;

    if (1) {
      printf("setlocale(\"%s\")\n", locale);
      const char *actual_locale = setlocale(LC_MESSAGES, locale);
      if (actual_locale == NULL) {
	fprintf(stderr, "Error: Cannot set locale to %s.\n", locale);
	return 4;
      }
      printf("new locale: %s\n", actual_locale);
    }

    if (1) {
      const char *basedir = bindtextdomain(GETTEXT_PACKAGE, localedir);
      printf("message basedir: %s\n", basedir);
    }

    if (1) {
      const char *domain = textdomain(GETTEXT_PACKAGE);
      printf("message domain: %s\n", domain);
    }

    if (1) {
      const char *codeset = bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
      printf("message codeset: %s\n", codeset);
    }

    puts("before translation");
    translation = gettext(untranslated);
    puts("after translation");

    if (strcmp(expected, translation) != 0) {
      fprintf(stderr,
	      "locale:       %s\n"
	      "localedir:    %s\n"
	      "untranslated: %s\n"
	      "expected:     %s\n"
	      "translation:  %s\n"
	      "Error: translation != expected\n",
	      locale,
	      localedir,
	      untranslated,
	      expected, 
	      translation);
      
      return 1;
    } else {
      fprintf(stderr,
	      "expected:     %s\n"
	      "translation:  %s\n"
	      "Match!\n",
	      expected,
	      translation);
    }
  }
  return 0;
}
