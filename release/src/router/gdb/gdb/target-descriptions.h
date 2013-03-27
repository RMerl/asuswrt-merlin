/* Target description support for GDB.

   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

   Contributed by CodeSourcery.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef TARGET_DESCRIPTIONS_H
#define TARGET_DESCRIPTIONS_H 1

struct tdesc_feature;
struct tdesc_arch_data;
struct tdesc_reg;
struct target_desc;
struct target_ops;
struct type;

/* Fetch the current target's description, and switch the current
   architecture to one which incorporates that description.  */

void target_find_description (void);

/* Discard any description fetched from the current target, and switch
   the current architecture to one with no target description.  */

void target_clear_description (void);

/* Return the global current target description.  This should only be
   used by gdbarch initialization code; most access should be through
   an existing gdbarch.  */

const struct target_desc *target_current_description (void);

/* Record architecture-specific functions to call for pseudo-register
   support.  If tdesc_use_registers is called and gdbarch_num_pseudo_regs
   is greater than zero, then these should be called as well.
   They are equivalent to the gdbarch methods with similar names,
   except that they will only be called for pseudo registers.  */

void set_tdesc_pseudo_register_name
  (struct gdbarch *gdbarch, gdbarch_register_name_ftype *pseudo_name);

void set_tdesc_pseudo_register_type
  (struct gdbarch *gdbarch, gdbarch_register_type_ftype *pseudo_type);

void set_tdesc_pseudo_register_reggroup_p
  (struct gdbarch *gdbarch,
   gdbarch_register_reggroup_p_ftype *pseudo_reggroup_p);

/* Update GDBARCH to use the target description for registers.  Fixed
   register assignments are taken from EARLY_DATA, which is freed.
   All registers which have not been assigned fixed numbers are given
   numbers above the current value of gdbarch_num_regs.
   gdbarch_num_regs and various  register-related predicates are updated to
   refer to the target description.  This function should only be called from
   the architecture's gdbarch initialization routine, and only after
   successfully validating the required registers.  */

void tdesc_use_registers (struct gdbarch *gdbarch,
			  struct tdesc_arch_data *early_data);

/* Allocate initial data for validation of a target description during
   gdbarch initialization.  */

struct tdesc_arch_data *tdesc_data_alloc (void);

/* Clean up data allocated by tdesc_data_alloc.  This should only
   be called to discard the data; tdesc_use_registers takes ownership
   of its EARLY_DATA argument.  */

void tdesc_data_cleanup (void *data_untyped);

/* Search FEATURE for a register named NAME.  Record REGNO and the
   register in DATA; when tdesc_use_registers is called, REGNO will be
   assigned to the register.  1 is returned if the register was found,
   0 if it was not.  */

int tdesc_numbered_register (const struct tdesc_feature *feature,
			     struct tdesc_arch_data *data,
			     int regno, const char *name);

/* Search FEATURE for a register with any of the names from NAMES
   (NULL-terminated).  Record REGNO and the register in DATA; when
   tdesc_use_registers is called, REGNO will be assigned to the
   register.  1 is returned if the register was found, 0 if it was
   not.  */

int tdesc_numbered_register_choices (const struct tdesc_feature *feature,
				     struct tdesc_arch_data *data,
				     int regno, const char *const names[]);


/* Accessors for target descriptions.  */

/* Return the BFD architecture associated with this target
   description, or NULL if no architecture was specified.  */

const struct bfd_arch_info *tdesc_architecture
  (const struct target_desc *);

/* Return the string value of a property named KEY, or NULL if the
   property was not specified.  */

const char *tdesc_property (const struct target_desc *,
			    const char *key);

/* Return 1 if this target description describes any registers.  */

int tdesc_has_registers (const struct target_desc *);

/* Return the feature with the given name, if present, or NULL if
   the named feature is not found.  */

const struct tdesc_feature *tdesc_find_feature (const struct target_desc *,
						const char *name);

/* Return the name of FEATURE.  */

const char *tdesc_feature_name (const struct tdesc_feature *feature);

/* Return the type associated with ID in the context of FEATURE, or
   NULL if none.  */

struct type *tdesc_named_type (const struct tdesc_feature *feature,
			       const char *id);

/* Return the name of register REGNO, from the target description or
   from an architecture-provided pseudo_register_name method.  */

const char *tdesc_register_name (int regno);

/* Check whether REGNUM is a member of REGGROUP using the target
   description.  Return -1 if the target description does not
   specify a group.  */

int tdesc_register_in_reggroup_p (struct gdbarch *gdbarch, int regno,
				  struct reggroup *reggroup);

/* Methods for constructing a target description.  */

struct target_desc *allocate_target_description (void);
struct cleanup *make_cleanup_free_target_description (struct target_desc *);
void set_tdesc_architecture (struct target_desc *,
			     const struct bfd_arch_info *);
void set_tdesc_property (struct target_desc *,
			 const char *key, const char *value);

struct tdesc_feature *tdesc_create_feature (struct target_desc *tdesc,
					    const char *name);
void tdesc_record_type (struct tdesc_feature *feature, struct type *type);

void tdesc_create_reg (struct tdesc_feature *feature, const char *name,
		       int regnum, int save_restore, const char *group,
		       int bitsize, const char *type);

#endif /* TARGET_DESCRIPTIONS_H */
