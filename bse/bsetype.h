/* BSE - Bedevilled Sound Engine
 * Copyright (C) 1998-2003 Tim Janik
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef __BSE_TYPE_H__
#define __BSE_TYPE_H__

#include	<bse/bsedefs.h>

G_BEGIN_DECLS

/* --- typedefs --- */
#define BSE_TYPE_PROCEDURE	G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_BSE_FIRST + 3)

/* type macros
 */
#define	BSE_TYPE_IS_PROCEDURE(type)	(G_TYPE_FUNDAMENTAL (type) == BSE_TYPE_PROCEDURE)
#define	BSE_CLASS_NAME(class)		(g_type_name (G_TYPE_FROM_CLASS (class)))
#define	BSE_CLASS_TYPE(class)		(G_TYPE_FROM_CLASS (class))
#define	BSE_TYPE_IS_OBJECT(type)	(g_type_is_a ((type), BSE_TYPE_OBJECT))

/* --- extra types --- */
extern GType bse_type_id_packed_pointer;
#define BSE_TYPE_PACKED_POINTER (bse_type_id_packed_pointer)


/* --- prototypes --- */
void   bse_type_init                  (void);
gchar* bse_type_blurb                 (GType            type);
void   bse_type_set_blurb             (GType            type,
                                       const gchar     *blurb);
GType  bse_type_register_static       (GType            parent_type,
                                       const gchar     *type_name,
                                       const gchar     *type_blurb,
                                       const GTypeInfo *info);
GType  bse_type_register_abstract     (GType            parent_type,
                                       const gchar     *type_name,
                                       const gchar     *type_blurb,
                                       const GTypeInfo *info);
GType  bse_type_register_dynamic      (GType            parent_type,
                                       const gchar     *type_name,
                                       const gchar     *type_blurb,
                                       GTypePlugin     *plugin);
void   bse_type_register_export_chain (BseExportNode   *chain,
                                       const gchar     *owner);


/* --- implementation details --- */

/* magic macros to define type initialization function within
 * .c files. they identify builtin type functions for magic post
 * processing and help resolving runtime type id retrival.
 */
#define	BSE_TYPE_ID(BseTypeName)	(bse_type_builtin_id_##BseTypeName)
#ifdef BSE_COMPILATION
#  define BSE_BUILTIN_PROTO(BseTypeName) GType bse_type_builtin_register_##BseTypeName (void)
#  define BSE_BUILTIN_TYPE(BseTypeName)	 extern BSE_BUILTIN_PROTO (BseTypeName); \
                                         GType bse_type_builtin_register_##BseTypeName (void)
#  define BSE_DUMMY_TYPE(BseTypeName)	 BSE_BUILTIN_PROTO (BseTypeName) { return 0; } \
                                         extern BSE_BUILTIN_PROTO (BseTypeName)
#endif /* BSE_COMPILATION */


/* --- customized pspec constructors --- */
GParamSpec*     bse_param_spec_enum (const gchar    *name,
                                     const gchar    *nick,
                                     const gchar    *blurb,
                                     gint            default_value, /* can always be 0 */
                                     GType           enum_type,
                                     const gchar    *hints);


/* -- auto generated type ids --- */
#include        <bse/bsegentypes.h>


/* --- dynamic config --- */
#define BSE_GCONFIG(cfg) (bse_global_config->cfg)
extern BseGConfig        *bse_global_config;    /* from bsegconfig.[hc] */


/* --- provide IDL pspec initializers --- */
#define	sfidl_pspec_Bool(group, name, nick, blurb, dflt, hints)			\
  sfi_pspec_set_group (sfi_pspec_bool (name, nick, blurb, dflt, hints), group)
#define	sfidl_pspec_Bool_default(group, name)	\
  sfi_pspec_set_group (sfi_pspec_bool (name, NULL, NULL, FALSE, SFI_PARAM_DEFAULT), group)
#define	sfidl_pspec_Trigger(group, name, nick, blurb)			\
  sfi_pspec_set_group (sfi_pspec_bool (name, nick, blurb, FALSE, "trigger:" SFI_PARAM_GUI), group)
#define	sfidl_pspec_Int(group, name, nick, blurb, dflt, min, max, step, hints)	\
  sfi_pspec_set_group (sfi_pspec_int (name, nick, blurb, dflt, min, max, step, hints), group)
#define	sfidl_pspec_Int_default(group, name)	\
  sfi_pspec_set_group (sfi_pspec_int (name, NULL, NULL, 0, G_MININT, G_MAXINT, 256, SFI_PARAM_DEFAULT), group)
#define	sfidl_pspec_Num(group, name, nick, blurb, dflt, min, max, step, hints)	\
  sfi_pspec_set_group (sfi_pspec_num (name, nick, blurb, dflt, min, max, step, hints), group)
#define	sfidl_pspec_Num_default(group, name)	\
  sfi_pspec_set_group (sfi_pspec_num (name, NULL, NULL, 0, SFI_MINNUM, SFI_MAXNUM, 1000, SFI_PARAM_DEFAULT), group)
#define	sfidl_pspec_UInt(group, name, nick, blurb, dflt, hints)	\
  sfi_pspec_set_group (sfi_pspec_int (name, nick, blurb, dflt, 0, G_MAXINT, 1, hints), group)
#define	sfidl_pspec_Real(group, name, nick, blurb, dflt, min, max, step, hints)	\
  sfi_pspec_set_group (sfi_pspec_real (name, nick, blurb, dflt, min, max, step, hints), group)
#define	sfidl_pspec_Real_default(group, name)	\
  sfi_pspec_set_group (sfi_pspec_real (name, NULL, NULL, 0, -SFI_MAXREAL, SFI_MAXREAL, 10, SFI_PARAM_DEFAULT), group)
#define	sfidl_pspec_Perc(group, name, nick, blurb, dflt, hints)	\
  sfi_pspec_set_group (sfi_pspec_real (name, nick, blurb, dflt, 0.0, 100.0, 5.0, "scale:" hints), group)
#define	sfidl_pspec_Balance(group, name, nick, blurb, dflt, hints)	\
  sfi_pspec_set_group (sfi_pspec_real (name, nick, blurb, dflt, -100.0, +100.0, 5.0, "scale:" hints), group)
#define	sfidl_pspec_Note(group, name, nick, blurb, dflt, hints)			\
  sfi_pspec_set_group (sfi_pspec_note (name, nick, blurb, dflt, SFI_MIN_NOTE, SFI_MAX_NOTE, FALSE, hints), group)
#define	sfidl_pspec_Octave(group, name, nick, blurb, dflt, hints)			\
  sfi_pspec_set_group (sfi_pspec_int (name, nick, blurb, dflt, BSE_MIN_OCTAVE, BSE_MAX_OCTAVE, 4, hints), group)
#define	sfidl_pspec_Freq(group, name, nick, blurb, dflt, hints)			\
  sfi_pspec_set_group (bse_param_spec_freq (name, nick, blurb, dflt, hints), group)
#define	sfidl_pspec_Frequency(group, name, nick, blurb, dflt, min, max, hints)			\
  sfi_pspec_set_group (sfi_pspec_real (name, nick, blurb, dflt, min, max, 10.0, "scale:" hints), group)
#define sfidl_pspec_Gain(group, name, nick, blurb, dflt, min, max, step, hints) \
  sfi_pspec_set_group (sfi_pspec_real (name, nick, blurb, dflt, min, max, step, hints), group)
#define	sfidl_pspec_FineTune(group, name, nick, blurb, hints)			\
  sfi_pspec_set_group (sfi_pspec_int (name, nick, blurb, 0, BSE_MIN_FINE_TUNE, BSE_MAX_FINE_TUNE, 10, hints), group)
#define	sfidl_pspec_Choice_default(group, name, cvalues)	\
  sfi_pspec_set_group (sfi_pspec_choice (name, NULL, NULL, NULL, cvalues, SFI_PARAM_DEFAULT), group)
#define	sfidl_pspec_GEnum(group, name, nick, blurb, dval, hints, etype)	\
  sfi_pspec_set_group (bse_param_spec_genum (name, nick, blurb, etype, dval, hints), group)
#define	sfidl_pspec_GEnum_default(group, name, etype)	\
  sfi_pspec_set_group (bse_param_spec_genum (name, NULL, NULL, etype, 0, SFI_PARAM_DEFAULT), group)
#define	sfidl_pspec_String(group, name, nick, blurb, dflt, hints)			\
  sfi_pspec_set_group (sfi_pspec_string (name, nick, blurb, dflt, hints), group)
#define	sfidl_pspec_String_default(group, name)  \
  sfi_pspec_set_group (sfi_pspec_string (name, NULL, NULL, NULL, SFI_PARAM_DEFAULT), group)
#define	sfidl_pspec_Proxy_default(group, name)  \
  sfi_pspec_set_group (sfi_pspec_proxy (name, NULL, NULL, SFI_PARAM_DEFAULT), group)
#define	sfidl_pspec_Object_default(group, name, otype)	\
  sfi_pspec_set_group (bse_param_spec_object (name, NULL, NULL, otype, SFI_PARAM_DEFAULT), group)
#define	sfidl_pspec_BoxedSeq(group, name, nick, blurb, hints, element_pspec)		\
  sfi_pspec_set_group (sfi_pspec_seq (name, nick, blurb, element_pspec, hints), group)
#define	sfidl_pspec_BoxedRec(group, name, nick, blurb, hints, fields)			\
  sfi_pspec_set_group (sfi_pspec_rec (name, nick, blurb, fields, hints), group)
#define	sfidl_pspec_BoxedRec_default(group, name, fields)	\
  sfi_pspec_set_group (sfi_pspec_rec (name, NULL, NULL, fields, SFI_PARAM_DEFAULT), group)
#define	sfidl_pspec_BBlock(group, name, nick, blurb, hints)				\
  sfi_pspec_set_group (sfi_pspec_bblock (name, nick, blurb, hints), group)


G_END_DECLS

#endif /* __BSE_TYPE_H__ */
