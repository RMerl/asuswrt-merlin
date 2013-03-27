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

#include "defs.h"
#include "arch-utils.h"
#include "gdbcmd.h"
#include "gdbtypes.h"
#include "reggroups.h"
#include "target.h"
#include "target-descriptions.h"
#include "vec.h"
#include "xml-support.h"
#include "xml-tdesc.h"

#include "gdb_assert.h"
#include "gdb_obstack.h"
#include "hashtab.h"

/* Types.  */

typedef struct property
{
  char *key;
  char *value;
} property_s;
DEF_VEC_O(property_s);

/* An individual register from a target description.  */

typedef struct tdesc_reg
{
  /* The name of this register.  In standard features, it may be
     recognized by the architecture support code, or it may be purely
     for the user.  */
  char *name;

  /* The register number used by this target to refer to this
     register.  This is used for remote p/P packets and to determine
     the ordering of registers in the remote g/G packets.  */
  long target_regnum;

  /* If this flag is set, GDB should save and restore this register
     around calls to an inferior function.  */
  int save_restore;

  /* The name of the register group containing this register, or NULL
     if the group should be automatically determined from the
     register's type.  If this is "general", "float", or "vector", the
     corresponding "info" command should display this register's
     value.  It can be an arbitrary string, but should be limited to
     alphanumeric characters and internal hyphens.  Currently other
     strings are ignored (treated as NULL).  */
  char *group;

  /* The size of the register, in bits.  */
  int bitsize;

  /* The type of the register.  This string corresponds to either
     a named type from the target description or a predefined
     type from GDB.  */
  char *type;

  /* The target-described type corresponding to TYPE, if found.  */
  struct type *gdb_type;
} *tdesc_reg_p;
DEF_VEC_P(tdesc_reg_p);

/* A named type from a target description.  */
typedef struct type *type_p;
DEF_VEC_P(type_p);

/* A feature from a target description.  Each feature is a collection
   of other elements, e.g. registers and types.  */

typedef struct tdesc_feature
{
  /* The name of this feature.  It may be recognized by the architecture
     support code.  */
  char *name;

  /* The registers associated with this feature.  */
  VEC(tdesc_reg_p) *registers;

  /* The types associated with this feature.  */
  VEC(type_p) *types;
} *tdesc_feature_p;
DEF_VEC_P(tdesc_feature_p);

/* A target description.  */

struct target_desc
{
  /* The architecture reported by the target, if any.  */
  const struct bfd_arch_info *arch;

  /* Any architecture-specific properties specified by the target.  */
  VEC(property_s) *properties;

  /* The features associated with this target.  */
  VEC(tdesc_feature_p) *features;
};

/* Per-architecture data associated with a target description.  The
   target description may be shared by multiple architectures, but
   this data is private to one gdbarch.  */

struct tdesc_arch_data
{
  /* A list of registers, indexed by GDB's internal register number.
     During initialization of the gdbarch this list is used to store
     registers which the architecture assigns a fixed register number.
     Registers which are NULL in this array, or off the end, are
     treated as zero-sized and nameless (i.e. placeholders in the
     numbering).  */
  VEC(tdesc_reg_p) *registers;

  /* Functions which report the register name, type, and reggroups for
     pseudo-registers.  */
  gdbarch_register_name_ftype *pseudo_register_name;
  gdbarch_register_type_ftype *pseudo_register_type;
  gdbarch_register_reggroup_p_ftype *pseudo_register_reggroup_p;
};

/* Global state.  These variables are associated with the current
   target; if GDB adds support for multiple simultaneous targets, then
   these variables should become target-specific data.  */

/* A flag indicating that a description has already been fetched from
   the current target, so it should not be queried again.  */

static int target_desc_fetched;

/* The description fetched from the current target, or NULL if the
   current target did not supply any description.  Only valid when
   target_desc_fetched is set.  Only the description initialization
   code should access this; normally, the description should be
   accessed through the gdbarch object.  */

static const struct target_desc *current_target_desc;

/* Other global variables.  */

/* The filename to read a target description from.  */

static char *target_description_filename;

/* A handle for architecture-specific data associated with the
   target description (see struct tdesc_arch_data).  */

static struct gdbarch_data *tdesc_data;

/* Fetch the current target's description, and switch the current
   architecture to one which incorporates that description.  */

void
target_find_description (void)
{
  /* If we've already fetched a description from the target, don't do
     it again.  This allows a target to fetch the description early,
     during its to_open or to_create_inferior, if it needs extra
     information about the target to initialize.  */
  if (target_desc_fetched)
    return;

  /* The current architecture should not have any target description
     specified.  It should have been cleared, e.g. when we
     disconnected from the previous target.  */
  gdb_assert (gdbarch_target_desc (current_gdbarch) == NULL);

  /* First try to fetch an XML description from the user-specified
     file.  */
  current_target_desc = NULL;
  if (target_description_filename != NULL
      && *target_description_filename != '\0')
    current_target_desc
      = file_read_description_xml (target_description_filename);

  /* Next try to read the description from the current target using
     target objects.  */
  if (current_target_desc == NULL)
    current_target_desc = target_read_description_xml (&current_target);

  /* If that failed try a target-specific hook.  */
  if (current_target_desc == NULL)
    current_target_desc = target_read_description (&current_target);

  /* If a non-NULL description was returned, then update the current
     architecture.  */
  if (current_target_desc)
    {
      struct gdbarch_info info;

      gdbarch_info_init (&info);
      info.target_desc = current_target_desc;
      if (!gdbarch_update_p (info))
	warning (_("Architecture rejected target-supplied description"));
      else
	{
	  struct tdesc_arch_data *data;

	  data = gdbarch_data (current_gdbarch, tdesc_data);
	  if (tdesc_has_registers (current_target_desc)
	      && data->registers == NULL)
	    warning (_("Target-supplied registers are not supported "
		       "by the current architecture"));
	}
    }

  /* Now that we know this description is usable, record that we
     fetched it.  */
  target_desc_fetched = 1;
}

/* Discard any description fetched from the current target, and switch
   the current architecture to one with no target description.  */

void
target_clear_description (void)
{
  struct gdbarch_info info;

  if (!target_desc_fetched)
    return;

  target_desc_fetched = 0;
  current_target_desc = NULL;

  gdbarch_info_init (&info);
  if (!gdbarch_update_p (info))
    internal_error (__FILE__, __LINE__,
		    _("Could not remove target-supplied description"));
}

/* Return the global current target description.  This should only be
   used by gdbarch initialization code; most access should be through
   an existing gdbarch.  */

const struct target_desc *
target_current_description (void)
{
  if (target_desc_fetched)
    return current_target_desc;

  return NULL;
}


/* Direct accessors for target descriptions.  */

/* Return the string value of a property named KEY, or NULL if the
   property was not specified.  */

const char *
tdesc_property (const struct target_desc *target_desc, const char *key)
{
  struct property *prop;
  int ix;

  for (ix = 0; VEC_iterate (property_s, target_desc->properties, ix, prop);
       ix++)
    if (strcmp (prop->key, key) == 0)
      return prop->value;

  return NULL;
}

/* Return the BFD architecture associated with this target
   description, or NULL if no architecture was specified.  */

const struct bfd_arch_info *
tdesc_architecture (const struct target_desc *target_desc)
{
  return target_desc->arch;
}


/* Return 1 if this target description includes any registers.  */

int
tdesc_has_registers (const struct target_desc *target_desc)
{
  int ix;
  struct tdesc_feature *feature;

  if (target_desc == NULL)
    return 0;

  for (ix = 0;
       VEC_iterate (tdesc_feature_p, target_desc->features, ix, feature);
       ix++)
    if (! VEC_empty (tdesc_reg_p, feature->registers))
      return 1;

  return 0;
}

/* Return the feature with the given name, if present, or NULL if
   the named feature is not found.  */

const struct tdesc_feature *
tdesc_find_feature (const struct target_desc *target_desc,
		    const char *name)
{
  int ix;
  struct tdesc_feature *feature;

  for (ix = 0;
       VEC_iterate (tdesc_feature_p, target_desc->features, ix, feature);
       ix++)
    if (strcmp (feature->name, name) == 0)
      return feature;

  return NULL;
}

/* Return the name of FEATURE.  */

const char *
tdesc_feature_name (const struct tdesc_feature *feature)
{
  return feature->name;
}

/* Return the type associated with ID in the context of FEATURE, or
   NULL if none.  */

struct type *
tdesc_named_type (const struct tdesc_feature *feature, const char *id)
{
  int ix;
  struct type *gdb_type;

  /* First try target-defined types.  */
  for (ix = 0; VEC_iterate (type_p, feature->types, ix, gdb_type); ix++)
    if (strcmp (TYPE_NAME (gdb_type), id) == 0)
      return gdb_type;

  /* Next try some predefined types.  Note that none of these types
     depend on the current architecture; some of the builtin_type_foo
     variables are swapped based on the architecture.  */
  if (strcmp (id, "int8") == 0)
    return builtin_type_int8;

  if (strcmp (id, "int16") == 0)
    return builtin_type_int16;

  if (strcmp (id, "int32") == 0)
    return builtin_type_int32;

  if (strcmp (id, "int64") == 0)
    return builtin_type_int64;

  if (strcmp (id, "uint8") == 0)
    return builtin_type_uint8;

  if (strcmp (id, "uint16") == 0)
    return builtin_type_uint16;

  if (strcmp (id, "uint32") == 0)
    return builtin_type_uint32;

  if (strcmp (id, "uint64") == 0)
    return builtin_type_uint64;

  if (strcmp (id, "ieee_single") == 0)
    return builtin_type_ieee_single;

  if (strcmp (id, "ieee_double") == 0)
    return builtin_type_ieee_double;

  if (strcmp (id, "arm_fpa_ext") == 0)
    return builtin_type_arm_ext;

  return NULL;
}


/* Support for registers from target descriptions.  */

/* Construct the per-gdbarch data.  */

static void *
tdesc_data_init (struct obstack *obstack)
{
  struct tdesc_arch_data *data;

  data = OBSTACK_ZALLOC (obstack, struct tdesc_arch_data);
  return data;
}

/* Similar, but for the temporary copy used during architecture
   initialization.  */

struct tdesc_arch_data *
tdesc_data_alloc (void)
{
  return XZALLOC (struct tdesc_arch_data);
}

/* Free something allocated by tdesc_data_alloc, if it is not going
   to be used (for instance if it was unsuitable for the
   architecture).  */

void
tdesc_data_cleanup (void *data_untyped)
{
  struct tdesc_arch_data *data = data_untyped;

  VEC_free (tdesc_reg_p, data->registers);
  xfree (data);
}

/* Search FEATURE for a register named NAME.  */

int
tdesc_numbered_register (const struct tdesc_feature *feature,
			 struct tdesc_arch_data *data,
			 int regno, const char *name)
{
  int ixr;
  struct tdesc_reg *reg;

  for (ixr = 0;
       VEC_iterate (tdesc_reg_p, feature->registers, ixr, reg);
       ixr++)
    if (strcasecmp (reg->name, name) == 0)
      {
	/* Make sure the vector includes a REGNO'th element.  */
	while (regno >= VEC_length (tdesc_reg_p, data->registers))
	  VEC_safe_push (tdesc_reg_p, data->registers, NULL);
	VEC_replace (tdesc_reg_p, data->registers, regno, reg);
	return 1;
      }

  return 0;
}

/* Search FEATURE for a register whose name is in NAMES.  */

int
tdesc_numbered_register_choices (const struct tdesc_feature *feature,
				 struct tdesc_arch_data *data,
				 int regno, const char *const names[])
{
  int i;

  for (i = 0; names[i] != NULL; i++)
    if (tdesc_numbered_register (feature, data, regno, names[i]))
      return 1;

  return 0;
}

/* Look up a register by its GDB internal register number.  */

static struct tdesc_reg *
tdesc_find_register (struct gdbarch *gdbarch, int regno)
{
  struct tdesc_reg *reg;
  struct tdesc_arch_data *data;

  data = gdbarch_data (gdbarch, tdesc_data);
  if (regno < VEC_length (tdesc_reg_p, data->registers))
    return VEC_index (tdesc_reg_p, data->registers, regno);
  else
    return NULL;
}

/* Return the name of register REGNO, from the target description or
   from an architecture-provided pseudo_register_name method.  */

const char *
tdesc_register_name (int regno)
{
  struct tdesc_reg *reg = tdesc_find_register (current_gdbarch, regno);
  int num_regs = gdbarch_num_regs (current_gdbarch);
  int num_pseudo_regs = gdbarch_num_pseudo_regs (current_gdbarch);

  if (reg != NULL)
    return reg->name;

  if (regno >= num_regs && regno < num_regs + num_pseudo_regs)
    {
      struct tdesc_arch_data *data = gdbarch_data (current_gdbarch,
						   tdesc_data);
      gdb_assert (data->pseudo_register_name != NULL);
      return data->pseudo_register_name (regno);
    }

  return "";
}

static struct type *
tdesc_register_type (struct gdbarch *gdbarch, int regno)
{
  struct tdesc_reg *reg = tdesc_find_register (gdbarch, regno);
  int num_regs = gdbarch_num_regs (gdbarch);
  int num_pseudo_regs = gdbarch_num_pseudo_regs (gdbarch);

  if (reg == NULL && regno >= num_regs && regno < num_regs + num_pseudo_regs)
    {
      struct tdesc_arch_data *data = gdbarch_data (gdbarch, tdesc_data);
      gdb_assert (data->pseudo_register_type != NULL);
      return data->pseudo_register_type (gdbarch, regno);
    }

  if (reg == NULL)
    /* Return "int0_t", since "void" has a misleading size of one.  */
    return builtin_type_int0;

  /* First check for a predefined or target defined type.  */
  if (reg->gdb_type)
    return reg->gdb_type;

  /* Next try size-sensitive type shortcuts.  */
  if (strcmp (reg->type, "float") == 0)
    {
      if (reg->bitsize == gdbarch_float_bit (gdbarch))
	return builtin_type_float;
      else if (reg->bitsize == gdbarch_double_bit (gdbarch))
	return builtin_type_double;
      else if (reg->bitsize == gdbarch_long_double_bit (gdbarch))
	return builtin_type_long_double;
    }
  else if (strcmp (reg->type, "int") == 0)
    {
      if (reg->bitsize == gdbarch_long_bit (gdbarch))
	return builtin_type_long;
      else if (reg->bitsize == TARGET_CHAR_BIT)
	return builtin_type_char;
      else if (reg->bitsize == gdbarch_short_bit (gdbarch))
	return builtin_type_short;
      else if (reg->bitsize == gdbarch_int_bit (gdbarch))
	return builtin_type_int;
      else if (reg->bitsize == gdbarch_long_long_bit (gdbarch))
	return builtin_type_long_long;
      else if (reg->bitsize == gdbarch_ptr_bit (gdbarch))
	/* A bit desperate by this point... */
	return builtin_type_void_data_ptr;
    }
  else if (strcmp (reg->type, "code_ptr") == 0)
    return builtin_type_void_func_ptr;
  else if (strcmp (reg->type, "data_ptr") == 0)
    return builtin_type_void_data_ptr;
  else
    internal_error (__FILE__, __LINE__,
		    "Register \"%s\" has an unknown type \"%s\"",
		    reg->name, reg->type);

  warning (_("Register \"%s\" has an unsupported size (%d bits)"),
	   reg->name, reg->bitsize);
  return builtin_type_long;
}

static int
tdesc_remote_register_number (struct gdbarch *gdbarch, int regno)
{
  struct tdesc_reg *reg = tdesc_find_register (gdbarch, regno);

  if (reg != NULL)
    return reg->target_regnum;
  else
    return -1;
}

/* Check whether REGNUM is a member of REGGROUP.  Registers from the
   target description may be classified as general, float, or vector.
   Unlike a gdbarch register_reggroup_p method, this function will
   return -1 if it does not know; the caller should handle registers
   with no specified group.

   Arbitrary strings (other than "general", "float", and "vector")
   from the description are not used; they cause the register to be
   displayed in "info all-registers" but excluded from "info
   registers" et al.  The names of containing features are also not
   used.  This might be extended to display registers in some more
   useful groupings.

   The save-restore flag is also implemented here.  */

int
tdesc_register_in_reggroup_p (struct gdbarch *gdbarch, int regno,
			      struct reggroup *reggroup)
{
  struct tdesc_reg *reg = tdesc_find_register (gdbarch, regno);

  if (reg != NULL && reg->group != NULL)
    {
      int general_p = 0, float_p = 0, vector_p = 0;

      if (strcmp (reg->group, "general") == 0)
	general_p = 1;
      else if (strcmp (reg->group, "float") == 0)
	float_p = 1;
      else if (strcmp (reg->group, "vector") == 0)
	vector_p = 1;

      if (reggroup == float_reggroup)
	return float_p;

      if (reggroup == vector_reggroup)
	return vector_p;

      if (reggroup == general_reggroup)
	return general_p;
    }

  if (reg != NULL
      && (reggroup == save_reggroup || reggroup == restore_reggroup))
    return reg->save_restore;

  return -1;
}

/* Check whether REGNUM is a member of REGGROUP.  Registers with no
   group specified go to the default reggroup function and are handled
   by type.  */

static int
tdesc_register_reggroup_p (struct gdbarch *gdbarch, int regno,
			   struct reggroup *reggroup)
{
  int num_regs = gdbarch_num_regs (gdbarch);
  int num_pseudo_regs = gdbarch_num_pseudo_regs (gdbarch);
  int ret;

  if (regno >= num_regs && regno < num_regs + num_pseudo_regs)
    {
      struct tdesc_arch_data *data = gdbarch_data (gdbarch, tdesc_data);
      gdb_assert (data->pseudo_register_reggroup_p != NULL);
      return data->pseudo_register_reggroup_p (gdbarch, regno, reggroup);
    }

  ret = tdesc_register_in_reggroup_p (gdbarch, regno, reggroup);
  if (ret != -1)
    return ret;

  return default_register_reggroup_p (gdbarch, regno, reggroup);
}

/* Record architecture-specific functions to call for pseudo-register
   support.  */

void
set_tdesc_pseudo_register_name (struct gdbarch *gdbarch,
				gdbarch_register_name_ftype *pseudo_name)
{
  struct tdesc_arch_data *data = gdbarch_data (gdbarch, tdesc_data);

  data->pseudo_register_name = pseudo_name;
}

void
set_tdesc_pseudo_register_type (struct gdbarch *gdbarch,
				gdbarch_register_type_ftype *pseudo_type)
{
  struct tdesc_arch_data *data = gdbarch_data (gdbarch, tdesc_data);

  data->pseudo_register_type = pseudo_type;
}

void
set_tdesc_pseudo_register_reggroup_p
  (struct gdbarch *gdbarch,
   gdbarch_register_reggroup_p_ftype *pseudo_reggroup_p)
{
  struct tdesc_arch_data *data = gdbarch_data (gdbarch, tdesc_data);

  data->pseudo_register_reggroup_p = pseudo_reggroup_p;
}

/* Update GDBARCH to use the target description for registers.  */

void
tdesc_use_registers (struct gdbarch *gdbarch,
		     struct tdesc_arch_data *early_data)
{
  int num_regs = gdbarch_num_regs (gdbarch);
  int i, ixf, ixr;
  const struct target_desc *target_desc;
  struct tdesc_feature *feature;
  struct tdesc_reg *reg;
  struct tdesc_arch_data *data;
  htab_t reg_hash;

  target_desc = gdbarch_target_desc (gdbarch);

  /* We can't use the description for registers if it doesn't describe
     any.  This function should only be called after validating
     registers, so the caller should know that registers are
     included.  */
  gdb_assert (tdesc_has_registers (target_desc));

  data = gdbarch_data (gdbarch, tdesc_data);
  data->registers = early_data->registers;
  xfree (early_data);

  /* Build up a set of all registers, so that we can assign register
     numbers where needed.  The hash table expands as necessary, so
     the initial size is arbitrary.  */
  reg_hash = htab_create (37, htab_hash_pointer, htab_eq_pointer, NULL);
  for (ixf = 0;
       VEC_iterate (tdesc_feature_p, target_desc->features, ixf, feature);
       ixf++)
    for (ixr = 0;
	 VEC_iterate (tdesc_reg_p, feature->registers, ixr, reg);
	 ixr++)
      {
	void **slot = htab_find_slot (reg_hash, reg, INSERT);

	*slot = reg;
      }

  /* Remove any registers which were assigned numbers by the
     architecture.  */
  for (ixr = 0; VEC_iterate (tdesc_reg_p, data->registers, ixr, reg); ixr++)
    if (reg)
      htab_remove_elt (reg_hash, reg);

  /* Assign numbers to the remaining registers and add them to the
     list of registers.  The new numbers are always above gdbarch_num_regs.
     Iterate over the features, not the hash table, so that the order
     matches that in the target description.  */

  gdb_assert (VEC_length (tdesc_reg_p, data->registers) <= num_regs);
  while (VEC_length (tdesc_reg_p, data->registers) < num_regs)
    VEC_safe_push (tdesc_reg_p, data->registers, NULL);
  for (ixf = 0;
       VEC_iterate (tdesc_feature_p, target_desc->features, ixf, feature);
       ixf++)
    for (ixr = 0;
	 VEC_iterate (tdesc_reg_p, feature->registers, ixr, reg);
	 ixr++)
      if (htab_find (reg_hash, reg) != NULL)
	{
	  VEC_safe_push (tdesc_reg_p, data->registers, reg);
	  num_regs++;
	}

  htab_delete (reg_hash);

  /* Update the architecture.  */
  set_gdbarch_num_regs (gdbarch, num_regs);
  set_gdbarch_register_name (gdbarch, tdesc_register_name);
  set_gdbarch_register_type (gdbarch, tdesc_register_type);
  set_gdbarch_remote_register_number (gdbarch,
				      tdesc_remote_register_number);
  set_gdbarch_register_reggroup_p (gdbarch, tdesc_register_reggroup_p);
}


/* Methods for constructing a target description.  */

static void
tdesc_free_reg (struct tdesc_reg *reg)
{
  xfree (reg->name);
  xfree (reg->type);
  xfree (reg->group);
  xfree (reg);
}

void
tdesc_create_reg (struct tdesc_feature *feature, const char *name,
		  int regnum, int save_restore, const char *group,
		  int bitsize, const char *type)
{
  struct tdesc_reg *reg = XZALLOC (struct tdesc_reg);

  reg->name = xstrdup (name);
  reg->target_regnum = regnum;
  reg->save_restore = save_restore;
  reg->group = group ? xstrdup (group) : NULL;
  reg->bitsize = bitsize;
  reg->type = type ? xstrdup (type) : xstrdup ("<unknown>");

  /* If the register's type is target-defined, look it up now.  We may not
     have easy access to the containing feature when we want it later.  */
  reg->gdb_type = tdesc_named_type (feature, reg->type);

  VEC_safe_push (tdesc_reg_p, feature->registers, reg);
}

static void
tdesc_free_feature (struct tdesc_feature *feature)
{
  struct tdesc_reg *reg;
  int ix;

  for (ix = 0; VEC_iterate (tdesc_reg_p, feature->registers, ix, reg); ix++)
    tdesc_free_reg (reg);
  VEC_free (tdesc_reg_p, feature->registers);

  /* There is no easy way to free xmalloc-allocated types, nor is
     there a way to allocate types on an obstack not associated with
     an objfile.  Therefore we never free types.  Since we only ever
     parse an identical XML document once, this memory leak is mostly
     contained.  */
  VEC_free (type_p, feature->types);

  xfree (feature->name);
  xfree (feature);
}

struct tdesc_feature *
tdesc_create_feature (struct target_desc *tdesc, const char *name)
{
  struct tdesc_feature *new_feature = XZALLOC (struct tdesc_feature);

  new_feature->name = xstrdup (name);

  VEC_safe_push (tdesc_feature_p, tdesc->features, new_feature);
  return new_feature;
}

void
tdesc_record_type (struct tdesc_feature *feature, struct type *type)
{
  /* The type's ID should be used as its TYPE_NAME.  */
  gdb_assert (TYPE_NAME (type) != NULL);

  VEC_safe_push (type_p, feature->types, type);
}

struct target_desc *
allocate_target_description (void)
{
  return XZALLOC (struct target_desc);
}

static void
free_target_description (void *arg)
{
  struct target_desc *target_desc = arg;
  struct tdesc_feature *feature;
  struct property *prop;
  int ix;

  for (ix = 0;
       VEC_iterate (tdesc_feature_p, target_desc->features, ix, feature);
       ix++)
    tdesc_free_feature (feature);
  VEC_free (tdesc_feature_p, target_desc->features);

  for (ix = 0;
       VEC_iterate (property_s, target_desc->properties, ix, prop);
       ix++)
    {
      xfree (prop->key);
      xfree (prop->value);
    }
  VEC_free (property_s, target_desc->properties);

  xfree (target_desc);
}

struct cleanup *
make_cleanup_free_target_description (struct target_desc *target_desc)
{
  return make_cleanup (free_target_description, target_desc);
}

void
set_tdesc_property (struct target_desc *target_desc,
		    const char *key, const char *value)
{
  struct property *prop, new_prop;
  int ix;

  gdb_assert (key != NULL && value != NULL);

  for (ix = 0; VEC_iterate (property_s, target_desc->properties, ix, prop);
       ix++)
    if (strcmp (prop->key, key) == 0)
      internal_error (__FILE__, __LINE__,
		      _("Attempted to add duplicate property \"%s\""), key);

  new_prop.key = xstrdup (key);
  new_prop.value = xstrdup (value);
  VEC_safe_push (property_s, target_desc->properties, &new_prop);
}

void
set_tdesc_architecture (struct target_desc *target_desc,
			const struct bfd_arch_info *arch)
{
  target_desc->arch = arch;
}


static struct cmd_list_element *tdesc_set_cmdlist, *tdesc_show_cmdlist;
static struct cmd_list_element *tdesc_unset_cmdlist;

/* Helper functions for the CLI commands.  */

static void
set_tdesc_cmd (char *args, int from_tty)
{
  help_list (tdesc_set_cmdlist, "set tdesc ", -1, gdb_stdout);
}

static void
show_tdesc_cmd (char *args, int from_tty)
{
  cmd_show_list (tdesc_show_cmdlist, from_tty, "");
}

static void
unset_tdesc_cmd (char *args, int from_tty)
{
  help_list (tdesc_unset_cmdlist, "unset tdesc ", -1, gdb_stdout);
}

static void
set_tdesc_filename_cmd (char *args, int from_tty,
			struct cmd_list_element *c)
{
  target_clear_description ();
  target_find_description ();
}

static void
show_tdesc_filename_cmd (struct ui_file *file, int from_tty,
			 struct cmd_list_element *c,
			 const char *value)
{
  if (value != NULL && *value != '\0')
    printf_filtered (_("\
The target description will be read from \"%s\".\n"),
		     value);
  else
    printf_filtered (_("\
The target description will be read from the target.\n"));
}

static void
unset_tdesc_filename_cmd (char *args, int from_tty)
{
  xfree (target_description_filename);
  target_description_filename = NULL;
  target_clear_description ();
  target_find_description ();
}

void
_initialize_target_descriptions (void)
{
  tdesc_data = gdbarch_data_register_pre_init (tdesc_data_init);

  add_prefix_cmd ("tdesc", class_maintenance, set_tdesc_cmd, _("\
Set target description specific variables."),
		  &tdesc_set_cmdlist, "set tdesc ",
		  0 /* allow-unknown */, &setlist);
  add_prefix_cmd ("tdesc", class_maintenance, show_tdesc_cmd, _("\
Show target description specific variables."),
		  &tdesc_show_cmdlist, "show tdesc ",
		  0 /* allow-unknown */, &showlist);
  add_prefix_cmd ("tdesc", class_maintenance, unset_tdesc_cmd, _("\
Unset target description specific variables."),
		  &tdesc_unset_cmdlist, "unset tdesc ",
		  0 /* allow-unknown */, &unsetlist);

  add_setshow_filename_cmd ("filename", class_obscure,
			    &target_description_filename,
			    _("\
Set the file to read for an XML target description"), _("\
Show the file to read for an XML target description"), _("\
When set, GDB will read the target description from a local\n\
file instead of querying the remote target."),
			    set_tdesc_filename_cmd,
			    show_tdesc_filename_cmd,
			    &tdesc_set_cmdlist, &tdesc_show_cmdlist);

  add_cmd ("filename", class_obscure, unset_tdesc_filename_cmd, _("\
Unset the file to read for an XML target description.  When unset,\n\
GDB will read the description from the target."),
	   &tdesc_unset_cmdlist);
}
