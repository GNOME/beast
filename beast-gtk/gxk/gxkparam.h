/* GXK - Gtk+ Extension Kit
 * Copyright (C) 2002-2003 Tim Janik
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GXK_PARAM_H__
#define __GXK_PARAM_H__

#include "gxkutils.h"

G_BEGIN_DECLS

/* --- macros --- */
#define GXK_IS_PARAM(p) (p && G_IS_PARAM_SPEC (p->pspec))

/* --- typedefs, structures & enums --- */
typedef struct _GxkParamBinding GxkParamBinding;
typedef struct {
  GParamSpec	  *pspec;
  GValue	   value;
  GSList          *objects;       /* of type GObject* */
  guint            editable : 1;  /* whether widgets should be editable */
  guint            sensitive : 1; /* whether widgets should be sensitive */
  guint		   updating : 1;  /* flag to guard recursions */
  guint		   constant : 1;  /* whether binding allowes writes */
  guint		   treadonly : 1; /* binding is temporarily RO */
  guint		   ueditable : 1; /* user determined editability */
  /* binding data */
  GxkParamBinding *binding;
  union {
    gpointer    v_pointer;
    gulong	v_long;
  }		   bdata[1];    /* flexible array */
} GxkParam;
struct _GxkParamBinding
{
  guint16         n_data_fields;
  void          (*setup)                (GxkParam       *param,
                                         gpointer        user_data);
  void		(*set_value)		(GxkParam	*param,
					 const GValue	*value);
  void		(*get_value)		(GxkParam	*param,
					 GValue		*value);
  /* optional: */
  void		(*destroy)		(GxkParam	*param);
  gboolean	(*check_writable)	(GxkParam	*param);
};
typedef void    (*GxkParamUpdateFunc)   (GxkParam       *param,
                                         GtkObject      *object);

/* --- functions --- */
GxkParam*     gxk_param_new                 (GParamSpec         *pspec,
                                             GxkParamBinding    *binding,
                                             gpointer            user_data);
GxkParam*     gxk_param_new_constant        (GParamSpec         *pspec,
                                             GxkParamBinding    *binding,
                                             gpointer            user_data);
void          gxk_param_update              (GxkParam           *param);
void          gxk_param_add_object          (GxkParam           *param,
                                             GtkObject          *object);
void          gxk_object_set_param_callback (GtkObject          *object,
                                             GxkParamUpdateFunc  ufunc);
void          gxk_param_remove_object       (GxkParam           *param,
                                             GtkObject          *object);
void          gxk_param_apply_value         (GxkParam           *param);
void          gxk_param_apply_default       (GxkParam           *param);
void          gxk_param_set_editable        (GxkParam           *param,
                                             gboolean            editable);
const gchar*  gxk_param_get_name            (GxkParam           *param);
gchar*        gxk_param_dup_tooltip         (GxkParam           *param);
void          gxk_param_set_devel_tips      (gboolean            enabled);
void          gxk_param_destroy             (GxkParam           *param);


/* --- param value binding --- */
typedef void (*GxkParamValueNotify)    (gpointer             notify_data,
                                        GxkParam            *param);
GxkParam* gxk_param_new_value          (GParamSpec          *pspec,
                                        GxkParamValueNotify  notify,
                                        gpointer             notify_data);
GxkParam* gxk_param_new_constant_value (GParamSpec          *pspec,
                                        GxkParamValueNotify  notify,
                                        gpointer             notify_data);


/* --- param view/editor --- */
typedef struct {
  gchar      *name, *nick;
} GxkParamEditorIdent;
typedef struct {
  GxkParamEditorIdent ident;
  struct {
    GType        type;
    const gchar *type_name;
    guint        all_int_nums : 1;
    guint        all_float_nums : 1;
  }              type_match;
  struct {
    gchar      *options;        /* required pspec options */
    gint8       rating;
    guint       editing : 1;
  }             features;
  GtkWidget*  (*create_widget)  (GxkParam       *param,
                                 const gchar    *tooltip,
                                 guint           variant);
  void        (*update)         (GxkParam       *param,
                                 GtkWidget      *widget);
  guint         variant;
} GxkParamEditor;
void         gxk_param_register_editor  (GxkParamEditor         *editor,
                                         const gchar            *i18n_domain);
void         gxk_param_register_aliases (const gchar           **aliases);
gchar**      gxk_param_list_editors     (void);
guint        gxk_param_editor_score     (const gchar            *editor_name,
                                         GParamSpec             *pspec);
const gchar* gxk_param_lookup_editor    (const gchar            *editor_name,
                                         GParamSpec             *pspec);
GtkWidget*   gxk_param_create_editor    (GxkParam               *param,
                                         const gchar            *editor_name);


/* --- param implementation utils --- */
typedef struct {
  guint char_chars,   char_digits;
  guint uchar_chars,  uchar_digits;
  guint int_chars,    int_digits;
  guint uint_chars,   uint_digits;
  guint long_chars,   long_digits;
  guint ulong_chars,  ulong_digits;
  guint int64_chars,  int64_digits;
  guint uint64_chars, uint64_digits;
  guint float_chars,  float_digits;
  guint double_chars, double_digits;
  guint string_chars, string_digits;
} GxkParamEditorSizes;
const GxkParamEditorSizes* gxk_param_get_editor_sizes (void);
void                       gxk_param_set_editor_sizes (const GxkParamEditorSizes *esizes);
gboolean       gxk_param_entry_key_press        (GtkEntry    *entry,
                                                 GdkEventKey *event);
void           gxk_param_entry_set_text         (GxkParam    *param,
                                                 GtkWidget   *entry,
                                                 const gchar *text);
void           gxk_param_entry_connect_handlers (GxkParam    *param,
                                                 GtkWidget   *entry,
                                                 void       (*changed) (GtkWidget*,
                                                                        GxkParam*));
gboolean       gxk_param_ensure_focus                 (GtkWidget *widget);
GtkAdjustment* gxk_param_get_adjustment               (GxkParam  *param);
GtkAdjustment* gxk_param_get_adjustment_with_stepping (GxkParam  *param,
                                                       gdouble    pstepping);
GtkAdjustment* gxk_param_get_log_adjustment           (GxkParam  *param);

G_END_DECLS

#endif /* __GXK_PARAM_H__ */

/* vim:set ts=8 sts=2 sw=2: */
