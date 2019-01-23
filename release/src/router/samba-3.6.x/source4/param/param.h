/* 
   Unix SMB/CIFS implementation.
   Generic parameter parsing interface
   Copyright (C) Jelmer Vernooij					  2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _PARAM_H /* _PARAM_H */
#define _PARAM_H 

struct parmlist_entry;

struct param_context {
	struct param_section *sections;
};

struct param_section {
	const char *name;
	struct param_section *prev, *next;
	struct parmlist *parameters;
};

struct param_context;
struct smbsrv_connection;

#define Auto (2)

typedef NTSTATUS (*init_module_fn) (void);

/* this needs to be a string which is not in the C library. We
   previously used "init_module", but that meant that modules which
   did not define this function ended up calling the C library
   function init_module() which makes a system call */
#define SAMBA_INIT_MODULE "samba_init_module"

enum server_role {
	ROLE_STANDALONE=0,
	ROLE_DOMAIN_MEMBER=1,
	ROLE_DOMAIN_CONTROLLER=2,
};

enum sid_generator {
	SID_GENERATOR_INTERNAL=0,
	SID_GENERATOR_BACKEND=1,
};

enum announce_as {/* Types of machine we can announce as. */
	ANNOUNCE_AS_NT_SERVER=1,
	ANNOUNCE_AS_WIN95=2,
	ANNOUNCE_AS_WFW=3,
	ANNOUNCE_AS_NT_WORKSTATION=4
};

struct loadparm_context;
struct loadparm_service;
struct smbcli_options;
struct smbcli_session_options;
struct gensec_settings;

#ifdef CONFIG_H_IS_FROM_SAMBA
#include "param/param_proto.h"
#endif

const char **lpcfg_interfaces(struct loadparm_context *);
const char *lpcfg_realm(struct loadparm_context *);
const char *lpcfg_netbios_name(struct loadparm_context *);
const char *lpcfg_private_dir(struct loadparm_context *);
int lpcfg_server_role(struct loadparm_context *);

void reload_charcnv(struct loadparm_context *lp_ctx);

struct loadparm_service *lpcfg_default_service(struct loadparm_context *lp_ctx);
struct parm_struct *lpcfg_parm_table(void);


char *lpcfg_tls_keyfile(TALLOC_CTX *mem_ctx, struct loadparm_context *);
char *lpcfg_tls_certfile(TALLOC_CTX *mem_ctx, struct loadparm_context *);
char *lpcfg_tls_cafile(TALLOC_CTX *mem_ctx, struct loadparm_context *);
char *lpcfg_tls_dhpfile(TALLOC_CTX *mem_ctx, struct loadparm_context *);
char *lpcfg_tls_crlfile(TALLOC_CTX *mem_ctx, struct loadparm_context *);

const char *lpcfg_servicename(const struct loadparm_service *service);


const char *lpcfg_get_parametric(struct loadparm_context *lp_ctx,
			      struct loadparm_service *service,
			      const char *type, const char *option);

const char *lpcfg_parm_string(struct loadparm_context *lp_ctx,
			   struct loadparm_service *service, const char *type,
			   const char *option);
const char **lpcfg_parm_string_list(TALLOC_CTX *mem_ctx,
				 struct loadparm_context *lp_ctx,
				 struct loadparm_service *service,
				 const char *type,
				 const char *option, const char *separator);
int lpcfg_parm_int(struct loadparm_context *lp_ctx,
		struct loadparm_service *service, const char *type,
		const char *option, int default_v);
int lpcfg_parm_bytes(struct loadparm_context *lp_ctx,
		  struct loadparm_service *service, const char *type,
		  const char *option, int default_v);
unsigned long lpcfg_parm_ulong(struct loadparm_context *lp_ctx,
			    struct loadparm_service *service, const char *type,
			    const char *option, unsigned long default_v);
double lpcfg_parm_double(struct loadparm_context *lp_ctx,
		      struct loadparm_service *service, const char *type,
		      const char *option, double default_v);
bool lpcfg_parm_bool(struct loadparm_context *lp_ctx,
		     struct loadparm_service *service, const char *type,
		     const char *option, bool default_v);
struct loadparm_service *lpcfg_add_service(struct loadparm_context *lp_ctx,
				     const struct loadparm_service *pservice,
				     const char *name);
bool lpcfg_add_home(struct loadparm_context *lp_ctx,
		 const char *pszHomename,
		 struct loadparm_service *default_service,
		 const char *user, const char *pszHomedir);
bool lpcfg_add_printer(struct loadparm_context *lp_ctx,
		    const char *pszPrintername,
		    struct loadparm_service *default_service);
struct parm_struct *lpcfg_parm_struct(const char *name);
void *lpcfg_parm_ptr(struct loadparm_context *lp_ctx,
		  struct loadparm_service *service, struct parm_struct *parm);
bool lpcfg_file_list_changed(struct loadparm_context *lp_ctx);

bool lpcfg_do_global_parameter(struct loadparm_context *lp_ctx,
			    const char *pszParmName, const char *pszParmValue);
bool lpcfg_do_service_parameter(struct loadparm_context *lp_ctx,
			     struct loadparm_service *service,
			     const char *pszParmName, const char *pszParmValue);

/**
 * Process a parameter.
 */
bool lpcfg_do_global_parameter_var(struct loadparm_context *lp_ctx,
				const char *pszParmName, const char *fmt, ...);
bool lpcfg_set_cmdline(struct loadparm_context *lp_ctx, const char *pszParmName,
		    const char *pszParmValue);
bool lpcfg_set_option(struct loadparm_context *lp_ctx, const char *option);

/**
 * Display the contents of a single services record.
 */
bool lpcfg_dump_a_parameter(struct loadparm_context *lp_ctx,
			 struct loadparm_service *service,
			 const char *parm_name, FILE * f);

/**
 * Return info about the next service  in a service. snum==-1 gives the globals.
 * Return NULL when out of parameters.
 */
struct parm_struct *lpcfg_next_parameter(struct loadparm_context *lp_ctx, int snum, int *i,
				      int allparameters);

/**
 * Unload unused services.
 */
void lpcfg_killunused(struct loadparm_context *lp_ctx,
		   struct smbsrv_connection *smb,
		   bool (*snumused) (struct smbsrv_connection *, int));

/**
 * Initialise the global parameter structure.
 */
struct loadparm_context *loadparm_init(TALLOC_CTX *mem_ctx);
struct loadparm_context *loadparm_init_global(bool load_default);
const char *lpcfg_configfile(struct loadparm_context *lp_ctx);
bool lpcfg_load_default(struct loadparm_context *lp_ctx);
const char *lp_default_path(void);

/**
 * Load the services array from the services file.
 *
 * Return True on success, False on failure.
 */
bool lpcfg_load(struct loadparm_context *lp_ctx, const char *filename);

/**
 * Return the max number of services.
 */
int lpcfg_numservices(struct loadparm_context *lp_ctx);

/**
 * Display the contents of the services array in human-readable form.
 */
void lpcfg_dump(struct loadparm_context *lp_ctx, FILE *f, bool show_defaults,
	     int maxtoprint);

/**
 * Display the contents of one service in human-readable form.
 */
void lpcfg_dump_one(FILE *f, bool show_defaults, struct loadparm_service *service, struct loadparm_service *sDefault);
struct loadparm_service *lpcfg_servicebynum(struct loadparm_context *lp_ctx,
					 int snum);
struct loadparm_service *lpcfg_service(struct loadparm_context *lp_ctx,
				    const char *service_name);

/**
 * A useful volume label function.
 */
const char *volume_label(struct loadparm_service *service, struct loadparm_service *sDefault);

/**
 * If we are PDC then prefer us as DMB
 */
const char *lpcfg_printername(struct loadparm_service *service, struct loadparm_service *sDefault);

/**
 * Return the max print jobs per queue.
 */
int lpcfg_maxprintjobs(struct loadparm_service *service, struct loadparm_service *sDefault);
struct smb_iconv_convenience *lpcfg_iconv_convenience(struct loadparm_context *lp_ctx);
void lpcfg_smbcli_options(struct loadparm_context *lp_ctx,
			 struct smbcli_options *options);
void lpcfg_smbcli_session_options(struct loadparm_context *lp_ctx,
				 struct smbcli_session_options *options);
struct dcerpc_server_info *lpcfg_dcerpc_server_info(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx);
struct gensec_settings *lpcfg_gensec_settings(TALLOC_CTX *, struct loadparm_context *);


/* The following definitions come from param/generic.c  */

struct param_section *param_get_section(struct param_context *ctx, const char *name);
struct parmlist_entry *param_section_get(struct param_section *section, 
				    const char *name);
struct parmlist_entry *param_get (struct param_context *ctx, const char *name, const char *section_name);
struct param_section *param_add_section(struct param_context *ctx, const char *section_name);
struct parmlist_entry *param_get_add(struct param_context *ctx, const char *name, const char *section_name);
const char *param_get_string(struct param_context *ctx, const char *param, const char *section);
int param_set_string(struct param_context *ctx, const char *param, const char *value, const char *section);
const char **param_get_string_list(struct param_context *ctx, const char *param, const char *separator, const char *section);
int param_set_string_list(struct param_context *ctx, const char *param, const char **list, const char *section);
int param_get_int(struct param_context *ctx, const char *param, int default_v, const char *section);
void param_set_int(struct param_context *ctx, const char *param, int value, const char *section);
unsigned long param_get_ulong(struct param_context *ctx, const char *param, unsigned long default_v, const char *section);
void param_set_ulong(struct param_context *ctx, const char *name, unsigned long value, const char *section);
struct param_context *param_init(TALLOC_CTX *mem_ctx);
int param_read(struct param_context *ctx, const char *fn);
int param_use(struct loadparm_context *lp_ctx, struct param_context *ctx);
int param_write(struct param_context *ctx, const char *fn);

/* The following definitions come from param/util.c  */


/**
 * @file
 * @brief Misc utility functions
 */
bool lpcfg_is_mydomain(struct loadparm_context *lp_ctx,
			     const char *domain);

bool lpcfg_is_my_domain_or_realm(struct loadparm_context *lp_ctx,
			      const char *domain);

/**
  see if a string matches either our primary or one of our secondary 
  netbios aliases. do a case insensitive match
*/
bool lpcfg_is_myname(struct loadparm_context *lp_ctx, const char *name);

/**
 A useful function for returning a path in the Samba lock directory.
**/
char *lock_path(TALLOC_CTX* mem_ctx, struct loadparm_context *lp_ctx,
			 const char *name);

/**
 * @brief Returns an absolute path to a file in the directory containing the current config file
 *
 * @param name File to find, relative to the config file directory.
 *
 * @retval Pointer to a talloc'ed string containing the full path.
 **/
char *config_path(TALLOC_CTX* mem_ctx, struct loadparm_context *lp_ctx,
			   const char *name);

/**
 * @brief Returns an absolute path to a file in the Samba private directory.
 *
 * @param name File to find, relative to PRIVATEDIR.
 * if name is not relative, then use it as-is
 *
 * @retval Pointer to a talloc'ed string containing the full path.
 **/
char *private_path(TALLOC_CTX* mem_ctx, 
			    struct loadparm_context *lp_ctx,
			    const char *name);

/**
  return a path in the smbd.tmp directory, where all temporary file
  for smbd go. If NULL is passed for name then return the directory 
  path itself
*/
char *smbd_tmp_path(TALLOC_CTX *mem_ctx, 
			     struct loadparm_context *lp_ctx,
			     const char *name);

/**
 * Obtain the init function from a shared library file
 */
init_module_fn load_module(TALLOC_CTX *mem_ctx, const char *path);

/**
 * Obtain list of init functions from the modules in the specified
 * directory
 */
init_module_fn *load_modules(TALLOC_CTX *mem_ctx, const char *path);

/**
 * Run the specified init functions.
 *
 * @return true if all functions ran successfully, false otherwise
 */
bool run_init_functions(init_module_fn *fns);

/**
 * Load the initialization functions from DSO files for a specific subsystem.
 *
 * Will return an array of function pointers to initialization functions
 */
init_module_fn *load_samba_modules(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx, const char *subsystem);
const char *lpcfg_messaging_path(TALLOC_CTX *mem_ctx,
				       struct loadparm_context *lp_ctx);
struct smb_iconv_convenience *smb_iconv_convenience_reinit_lp(TALLOC_CTX *mem_ctx,
							      struct loadparm_context *lp_ctx,
							      struct smb_iconv_convenience *old_ic);

const char *lpcfg_sam_name(struct loadparm_context *lp_ctx);

/* The following definitions come from lib/version.c  */

const char *samba_version_string(void);


#endif /* _PARAM_H */
