// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#ifndef __GXK_ACTION_H__
#define __GXK_ACTION_H__

#include "gxkutils.hh"

G_BEGIN_DECLS


#define GXK_ACTION_PRIORITY     (G_PRIORITY_HIGH - 10)


/* --- structures --- */
typedef gboolean (*GxkActionCheck)        (gpointer        user_data,
                                           size_t          action_id,
                                           guint64         action_stamp);
typedef void     (*GxkActionExec)         (gpointer        user_data,
                                           size_t          action_id);
typedef struct GxkActionGroup GxkActionGroup;   /* prototyped */
typedef struct GxkActionList  GxkActionList;
typedef struct {
  const gchar  *key;            /* untranslated name (used for accel paths) */
  gconstpointer action_data;    /* for gxk_action_activate_callback() */
  const gchar  *name;
  const gchar  *accelerator;
  const gchar  *tooltip;
  size_t        action_id;
  const gchar  *stock_icon;     /* stock_id for the icon or NULL */
} GxkAction;

typedef struct {
  const gchar  *name;           /* subject to i18n (key) */
  const gchar  *accelerator;
  const gchar  *tooltip;        /* subject to i18n */
  size_t        action_id;
  const gchar  *stock_icon;     /* stock_id for the icon */
} GxkStockAction;

/* --- public API --- */
guint64         gxk_action_inc_cache_stamp      (void);
GxkActionList*  gxk_action_list_create          (void);
GxkActionList*  gxk_action_list_create_grouped  (GxkActionGroup         *agroup);
void            gxk_action_list_add_actions     (GxkActionList          *alist,
                                                 guint                   n_actions,
                                                 const GxkStockAction   *actions,
                                                 const gchar            *i18n_domain,
                                                 GxkActionCheck          acheck,
                                                 GxkActionExec           aexec,
                                                 gpointer                user_data);
void            gxk_action_list_add_translated  (GxkActionList          *alist,
                                                 const gchar            *key,           /* untranslated name */
                                                 const gchar            *name,          /* translated (key) */
                                                 const gchar            *accelerator,
                                                 const gchar            *tooltip,       /* translated */
                                                 size_t                  action_id,
                                                 const gchar            *stock_icon,
                                                 GxkActionCheck          acheck,
                                                 GxkActionExec           aexec,
                                                 gpointer                user_data);
GxkActionList*  gxk_action_list_sort            (GxkActionList          *alist);
GxkActionList*  gxk_action_list_merge           (GxkActionList          *alist1,
                                                 GxkActionList          *alist2);
GxkActionList*  gxk_action_list_copy            (GxkActionList          *alist);
guint           gxk_action_list_get_n_actions   (GxkActionList          *alist);
void            gxk_action_list_get_action      (GxkActionList          *alist,
                                                 guint                   nth,
                                                 GxkAction              *action);
void            gxk_action_list_regulate_widget (GxkActionList          *alist,
                                                 guint                   nth,
                                                 GtkWidget              *widget);
void            gxk_action_list_force_regulate  (GtkWidget              *widget);
void            gxk_action_list_free            (GxkActionList          *alist);
void            gxk_action_activate_callback    (gconstpointer          action_data);
void      gxk_widget_update_actions_upwards     (gpointer                widget);
void      gxk_widget_update_actions_downwards   (gpointer                widget);
void      gxk_widget_update_actions             (gpointer                widget);

/* --- publishing --- */
void      gxk_widget_publish_action_list        (gpointer                widget,
                                                 const gchar            *prefix,
                                                 GxkActionList          *alist);
GSList*   gxk_widget_peek_action_widgets        (gpointer                widget,
                                                 const gchar            *prefix,
                                                 size_t                  action_id);
void      gxk_widget_publish_actions            (gpointer                widget,
                                                 const gchar            *prefix,
                                                 guint                   n_actions,
                                                 const GxkStockAction   *actions,
                                                 const gchar            *i18n_domain,
                                                 GxkActionCheck          acheck,
                                                 GxkActionExec           aexec);
void      gxk_widget_publish_actions_grouped    (gpointer                widget,
                                                 GxkActionGroup         *group,
                                                 const gchar            *prefix,
                                                 guint                   n_actions,
                                                 const GxkStockAction   *actions,
                                                 const gchar            *i18n_domain,
                                                 GxkActionCheck          acheck,
                                                 GxkActionExec           aexec);
void      gxk_widget_publish_translated         (gpointer                widget,
                                                 const gchar            *prefix,
                                                 const gchar            *key,           /* untranslated name */
                                                 const gchar            *name,          /* translated (key) */
                                                 const gchar            *accelerator,
                                                 const gchar            *tooltip,       /* translated */
                                                 size_t                  action_id,
                                                 const gchar            *stock_icon,
                                                 GxkActionCheck          acheck,
                                                 GxkActionExec           aexec);
void      gxk_widget_publish_grouped_translated (gpointer                widget,
                                                 GxkActionGroup         *group,
                                                 const gchar            *prefix,
                                                 const gchar            *key,           /* untranslated name */
                                                 const gchar            *name,          /* translated (key) */
                                                 const gchar            *accelerator,
                                                 const gchar            *tooltip,       /* translated */
                                                 size_t                  action_id,
                                                 const gchar            *stock_icon,
                                                 GxkActionCheck          acheck,
                                                 GxkActionExec           aexec);
void      gxk_widget_republish_actions          (gpointer                widget,
                                                 const gchar            *prefix,
                                                 gpointer                source_widget);
typedef void  (*GxkActionClient)                (gpointer                client_data,
                                                 GtkWindow              *window,
                                                 const gchar            *prefix,
                                                 GxkActionList          *action_list,
                                                 GtkWidget              *publisher);
void      gxk_window_add_action_client          (GtkWindow              *window,
                                                 GxkActionClient         added_func,
                                                 gpointer                client_data);
void      gxk_window_remove_action_client       (GtkWindow              *window,
                                                 gpointer                client_data);


/* --- action groups --- */
#define GXK_TYPE_ACTION_GROUP              (gxk_action_group_get_type ())
#define GXK_ACTION_GROUP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GXK_TYPE_ACTION_GROUP, GxkActionGroup))
#define GXK_ACTION_GROUP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GXK_TYPE_ACTION_GROUP, GxkActionGroupClass))
#define GXK_IS_ACTION_GROUP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GXK_TYPE_ACTION_GROUP))
#define GXK_IS_ACTION_GROUP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GXK_TYPE_ACTION_GROUP))
#define GXK_ACTION_GROUP_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), GXK_TYPE_ACTION_GROUP, GxkActionGroupClass))
struct GxkActionGroup {
  GObject parent_instance;
  size_t  action_id;
  guint   lock_count;
  guint   invert_dups : 1;
};
typedef struct {
  GObjectClass parent_class;
  void       (*changed)      (GxkActionGroup *self);
} GxkActionGroupClass;
GType           gxk_action_group_get_type       (void);
GxkActionGroup* gxk_action_group_new            (void);
void            gxk_action_group_select         (GxkActionGroup        *agroup,
                                                 size_t                 action_id);
void            gxk_action_group_lock           (GxkActionGroup        *agroup);
void            gxk_action_group_unlock         (GxkActionGroup        *agroup);
void            gxk_action_group_dispose        (GxkActionGroup        *agroup);
GxkActionGroup* gxk_action_toggle_new           (void);


G_END_DECLS

#endif /* __GXK_ACTION_H__ */
