/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_CONFPARSE_H
#define TOR_CONFPARSE_H

/** Enumeration of types which option values can take */
typedef enum config_type_t {
  CONFIG_TYPE_STRING = 0,   /**< An arbitrary string. */
  CONFIG_TYPE_FILENAME,     /**< A filename: some prefixes get expanded. */
  CONFIG_TYPE_UINT,         /**< A non-negative integer less than MAX_INT */
  CONFIG_TYPE_INT,          /**< Any integer. */
  CONFIG_TYPE_PORT,         /**< A port from 1...65535, 0 for "not set", or
                             * "auto".  */
  CONFIG_TYPE_INTERVAL,     /**< A number of seconds, with optional units*/
  CONFIG_TYPE_MSEC_INTERVAL,/**< A number of milliseconds, with optional
                              * units */
  CONFIG_TYPE_MEMUNIT,      /**< A number of bytes, with optional units*/
  CONFIG_TYPE_DOUBLE,       /**< A floating-point value */
  CONFIG_TYPE_BOOL,         /**< A boolean value, expressed as 0 or 1. */
  CONFIG_TYPE_AUTOBOOL,     /**< A boolean+auto value, expressed 0 for false,
                             * 1 for true, and -1 for auto  */
  CONFIG_TYPE_ISOTIME,      /**< An ISO-formatted time relative to UTC. */
  CONFIG_TYPE_CSV,          /**< A list of strings, separated by commas and
                              * optional whitespace. */
  CONFIG_TYPE_CSV_INTERVAL, /**< A list of strings, separated by commas and
                              * optional whitespace, representing intervals in
                              * seconds, with optional units */
  CONFIG_TYPE_LINELIST,     /**< Uninterpreted config lines */
  CONFIG_TYPE_LINELIST_S,   /**< Uninterpreted, context-sensitive config lines,
                             * mixed with other keywords. */
  CONFIG_TYPE_LINELIST_V,   /**< Catch-all "virtual" option to summarize
                             * context-sensitive config lines when fetching.
                             */
  CONFIG_TYPE_ROUTERSET,    /**< A list of router names, addrs, and fps,
                             * parsed into a routerset_t. */
  CONFIG_TYPE_OBSOLETE,     /**< Obsolete (ignored) option. */
} config_type_t;

/** An abbreviation for a configuration option allowed on the command line. */
typedef struct config_abbrev_t {
  const char *abbreviated;
  const char *full;
  int commandline_only;
  int warn;
} config_abbrev_t;

/* Handy macro for declaring "In the config file or on the command line,
 * you can abbreviate <b>tok</b>s as <b>tok</b>". */
#define PLURAL(tok) { #tok, #tok "s", 0, 0 }

/** A variable allowed in the configuration file or on the command line. */
typedef struct config_var_t {
  const char *name; /**< The full keyword (case insensitive). */
  config_type_t type; /**< How to interpret the type and turn it into a
                       * value. */
  off_t var_offset; /**< Offset of the corresponding member of or_options_t. */
  const char *initvalue; /**< String (or null) describing initial value. */
} config_var_t;

/** Represents an English description of a configuration variable; used when
 * generating configuration file comments. */
typedef struct config_var_description_t {
  const char *name;
  const char *description;
} config_var_description_t;

/** Type of a callback to validate whether a given configuration is
 * well-formed and consistent. See options_trial_assign() for documentation
 * of arguments. */
typedef int (*validate_fn_t)(void*,void*,void*,int,char**);

/** Information on the keys, value types, key-to-struct-member mappings,
 * variable descriptions, validation functions, and abbreviations for a
 * configuration or storage format. */
typedef struct config_format_t {
  size_t size; /**< Size of the struct that everything gets parsed into. */
  uint32_t magic; /**< Required 'magic value' to make sure we have a struct
                   * of the right type. */
  off_t magic_offset; /**< Offset of the magic value within the struct. */
  config_abbrev_t *abbrevs; /**< List of abbreviations that we expand when
                             * parsing this format. */
  config_var_t *vars; /**< List of variables we recognize, their default
                       * values, and where we stick them in the structure. */
  validate_fn_t validate_fn; /**< Function to validate config. */
  /** If present, extra is a LINELIST variable for unrecognized
   * lines.  Otherwise, unrecognized lines are an error. */
  config_var_t *extra;
} config_format_t;

/** Macro: assert that <b>cfg</b> has the right magic field for format
 * <b>fmt</b>. */
#define CONFIG_CHECK(fmt, cfg) STMT_BEGIN                               \
    tor_assert(fmt && cfg);                                             \
    tor_assert((fmt)->magic ==                                          \
               *(uint32_t*)STRUCT_VAR_P(cfg,fmt->magic_offset));        \
  STMT_END

void *config_new(const config_format_t *fmt);
void config_line_append(config_line_t **lst,
                        const char *key, const char *val);
config_line_t *config_lines_dup(const config_line_t *inp);
const config_line_t *config_line_find(const config_line_t *lines,
                                      const char *key);
void config_free(const config_format_t *fmt, void *options);
int config_lines_eq(config_line_t *a, config_line_t *b);
int config_count_key(const config_line_t *a, const char *key);
config_line_t *config_get_assigned_option(const config_format_t *fmt,
                                          const void *options, const char *key,
                                          int escape_val);
int config_is_same(const config_format_t *fmt,
                   const void *o1, const void *o2,
                   const char *name);
void config_init(const config_format_t *fmt, void *options);
void *config_dup(const config_format_t *fmt, const void *old);
char *config_dump(const config_format_t *fmt, const void *default_options,
                  const void *options, int minimal,
                  int comment_defaults);
int config_assign(const config_format_t *fmt, void *options,
                  config_line_t *list,
                  int use_defaults, int clear_first, char **msg);
config_var_t *config_find_option_mutable(config_format_t *fmt,
                                         const char *key);
const config_var_t *config_find_option(const config_format_t *fmt,
                                       const char *key);

int config_get_lines(const char *string, config_line_t **result, int extended);
void config_free_lines(config_line_t *front);
const char *config_expand_abbrev(const config_format_t *fmt,
                                 const char *option,
                                 int command_line, int warn_obsolete);

#endif

