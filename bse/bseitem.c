/* BSE - Bedevilled Sound Engine
 * Copyright (C) 1998-2002 Tim Janik
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */
#include        "bseitem.h"

#include        "bsesuper.h"
#include        "bsestorage.h"
#include        "bseprocedure.h"
#include        "bsemarshal.h"
#include	"bsemain.h"



enum {
  SIGNAL_SEQID_CHANGED,
  SIGNAL_LAST
};


/* --- prototypes --- */
static void		bse_item_class_init_base	(BseItemClass		*class);
static void		bse_item_class_finalize_base	(BseItemClass		*class);
static void		bse_item_class_init		(BseItemClass		*class);
static void		bse_item_init			(BseItem		*item);
static BseProxySeq*	bse_item_list_no_proxies	(BseItem		*item,
							 guint                   param_id,
							 GParamSpec             *pspec);
static void		bse_item_do_dispose		(GObject		*object);
static void		bse_item_do_finalize		(GObject		*object);
static void		bse_item_do_set_uname		(BseObject		*object,
							 const gchar		*uname);
static guint		bse_item_do_get_seqid		(BseItem		*item);
static void		bse_item_do_set_parent		(BseItem                *item,
							 BseItem                *parent);
static void             bse_item_store_property		(BseObject		*object,
							 BseStorage		*storage,
							 GValue			*value,
							 GParamSpec		*pspec);
static GTokenType       bse_item_restore_property	(BseObject		*object,
							 BseStorage		*storage,
							 GValue			*value,
							 GParamSpec		*pspec);


/* --- variables --- */
static GTypeClass *parent_class = NULL;
static GSList     *item_seqid_changed_queue = NULL;
static guint       item_signals[SIGNAL_LAST] = { 0, };


/* --- functions --- */
BSE_BUILTIN_TYPE (BseItem)
{
  static const GTypeInfo item_info = {
    sizeof (BseItemClass),
    
    (GBaseInitFunc) bse_item_class_init_base,
    (GBaseFinalizeFunc) bse_item_class_finalize_base,
    (GClassInitFunc) bse_item_class_init,
    (GClassFinalizeFunc) NULL,
    NULL /* class_data */,
    
    sizeof (BseItem),
    0 /* n_preallocs */,
    (GInstanceInitFunc) bse_item_init,
  };
  
  g_assert (BSE_ITEM_FLAGS_USHIFT < BSE_OBJECT_FLAGS_MAX_SHIFT);
  
  return bse_type_register_static (BSE_TYPE_OBJECT,
				   "BseItem",
				   "Base type for objects managed by a container",
				   &item_info);
}

static void
bse_item_class_init_base (BseItemClass *class)
{
  class->list_proxies = bse_item_list_no_proxies;
}

static void
bse_item_class_finalize_base (BseItemClass *class)
{
}

static void
bse_item_class_init (BseItemClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  BseObjectClass *object_class = BSE_OBJECT_CLASS (class);
  
  parent_class = g_type_class_peek_parent (class);
  
  gobject_class->dispose = bse_item_do_dispose;
  gobject_class->finalize = bse_item_do_finalize;
  
  object_class->store_property = bse_item_store_property;
  object_class->restore_property = bse_item_restore_property;
  object_class->set_uname = bse_item_do_set_uname;
  
  class->set_parent = bse_item_do_set_parent;
  class->get_seqid = bse_item_do_get_seqid;
  
  item_signals[SIGNAL_SEQID_CHANGED] = bse_object_class_add_signal (object_class, "seqid_changed",
								    bse_marshal_VOID__NONE, NULL,
								    G_TYPE_NONE, 0);
}

static void
bse_item_init (BseItem *item)
{
  item->parent = NULL;
}

static void
bse_item_do_dispose (GObject *gobject)
{
  BseItem *item = BSE_ITEM (gobject);

  /* force removal from parent */
  if (item->parent)
    bse_container_remove_item (BSE_CONTAINER (item->parent), item);

  /* chain parent class' handler */
  G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

static void
bse_item_do_finalize (GObject *object)
{
  BseItem *item = BSE_ITEM (object);
  
  item_seqid_changed_queue = g_slist_remove (item_seqid_changed_queue, item);
  
  /* chain parent class' handler */
  G_OBJECT_CLASS (parent_class)->finalize (object);
  
  g_return_if_fail (item->use_count == 0);
}

static BseProxySeq*
bse_item_list_no_proxies (BseItem    *item,
			  guint       param_id,
			  GParamSpec *pspec)
{
  return bse_proxy_seq_new ();
}

static void
bse_item_do_set_uname (BseObject   *object,
		       const gchar *uname)
{
  BseItem *item = BSE_ITEM (object);
  
  /* ensure that item names within their container are unique,
   * and that we don't end up with a NULL uname
   */
  if (!BSE_IS_CONTAINER (item->parent) ||
      (uname && !bse_container_lookup_item (BSE_CONTAINER (item->parent), uname)))
    {
      /* chain parent class' set_uname handler */
      BSE_OBJECT_CLASS (parent_class)->set_uname (object, uname);
    }
}

static void
bse_item_do_set_parent (BseItem *item,
			BseItem *parent)
{
  item->parent = parent;
}

typedef struct {
  BseItem              *item;
  gpointer              data;
  BseProxySeq          *proxies;
  GType                 base_type;
  BseItemCheckContainer ccheck;
  BseItemCheckProxy     pcheck;
} GatherData;

static gboolean
gather_child (BseItem *child,
	      gpointer data)
{
  GatherData *gdata = data;
  
  if (child != gdata->item &&
      g_type_is_a (G_OBJECT_TYPE (child), gdata->base_type) &&
      (!gdata->pcheck || gdata->pcheck (child, gdata->item, gdata->data)))
    bse_proxy_seq_append (gdata->proxies, BSE_OBJECT_ID (child));
  return TRUE;
}

BseProxySeq*
bse_item_gather_proxies (BseItem              *item,
			 BseProxySeq          *proxies,
			 GType		       base_type,
			 BseItemCheckContainer ccheck,
			 BseItemCheckProxy     pcheck,
			 gpointer              data)
{
  GatherData gdata;
  
  g_return_val_if_fail (BSE_IS_ITEM (item), NULL);
  g_return_val_if_fail (proxies != NULL, NULL);
  g_return_val_if_fail (g_type_is_a (base_type, BSE_TYPE_ITEM), NULL);
  
  gdata.item = item;
  gdata.data = data;
  gdata.proxies = proxies;
  gdata.base_type = base_type;
  gdata.ccheck = ccheck;
  gdata.pcheck = pcheck;
  
  item = BSE_IS_CONTAINER (item) ? item : item->parent;
  while (item)
    {
      BseContainer *container = BSE_CONTAINER (item);
      if (!gdata.ccheck || gdata.ccheck (container, gdata.item, gdata.data))
	bse_container_forall_items (container, gather_child, &gdata);
      item = item->parent;
    }
  return proxies;
}

BseProxySeq*
bse_item_list_proxies (BseItem     *item,
		       const gchar *property)
{
  BseItemClass *class;
  GParamSpec *pspec;
  
  g_return_val_if_fail (BSE_IS_ITEM (item), NULL);
  g_return_val_if_fail (property != NULL, NULL);
  
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (item), property);
  if (!pspec)
    return NULL;
  class = g_type_class_peek (pspec->owner_type);
  return class->list_proxies (item, pspec->param_id, pspec);
}

void
bse_item_use (BseItem *item)
{
  g_return_if_fail (BSE_IS_ITEM (item));
  g_return_if_fail (G_OBJECT (item)->ref_count > 0);
  
  if (!item->use_count)
    bse_object_ref (item);
  item->use_count++;
}

void
bse_item_set_parent (BseItem *item,
		     BseItem *parent)
{
  g_return_if_fail (BSE_IS_ITEM (item));
  if (parent)
    {
      g_return_if_fail (item->parent == NULL);
      g_return_if_fail (BSE_IS_CONTAINER (parent));
    }
  else
    g_return_if_fail (item->parent != NULL);
  g_return_if_fail (BSE_ITEM_GET_CLASS (item)->set_parent != NULL); /* paranoid */
  
  bse_object_ref (BSE_OBJECT (item));
  if (parent)
    bse_object_ref (BSE_OBJECT (parent));
  
  BSE_ITEM_GET_CLASS (item)->set_parent (item, parent);
  
  if (parent)
    bse_object_unref (BSE_OBJECT (parent));
  else if (!item->use_count)
    g_object_run_dispose (G_OBJECT (item));
    
  bse_object_unref (BSE_OBJECT (item));
}

static guint
bse_item_do_get_seqid (BseItem *item)
{
  if (item->parent)
    return bse_container_get_item_seqid (BSE_CONTAINER (item->parent), item);
  else
    return 0;
}

static gboolean
idle_handler_seqid_changed (gpointer data)
{
  BSE_THREADS_ENTER ();
  
  while (item_seqid_changed_queue)
    {
      GSList *slist = item_seqid_changed_queue;
      BseItem *item = slist->data;
      
      item_seqid_changed_queue = slist->next;
      g_slist_free_1 (slist);
      if (item->parent)
	g_signal_emit (item, item_signals[SIGNAL_SEQID_CHANGED], 0);
    }
  
  BSE_THREADS_LEAVE ();
  
  return FALSE;
}

void
bse_item_queue_seqid_changed (BseItem *item)
{
  g_return_if_fail (BSE_IS_ITEM (item));
  g_return_if_fail (BSE_ITEM (item)->parent != NULL);
  
  if (!item_seqid_changed_queue)
    bse_idle_notify (idle_handler_seqid_changed, NULL);
  
  if (!g_slist_find (item_seqid_changed_queue, item))
    item_seqid_changed_queue = g_slist_prepend (item_seqid_changed_queue, item);
}

guint
bse_item_get_seqid (BseItem *item)
{
  g_return_val_if_fail (BSE_IS_ITEM (item), 0);
  g_return_val_if_fail (BSE_ITEM_GET_CLASS (item)->get_seqid != NULL, 0); /* paranoid */
  
  return BSE_ITEM_GET_CLASS (item)->get_seqid (item);
}

BseItem*
bse_item_common_ancestor (BseItem *item1,
			  BseItem *item2)
{
  g_return_val_if_fail (BSE_IS_ITEM (item1), NULL);
  g_return_val_if_fail (BSE_IS_ITEM (item2), NULL);
  
  do
    {
      BseItem *item = item2;
      
      do
	{
	  if (item == item1)
	    return item;
	  item = item->parent;
	}
      while (item);
      item1 = item1->parent;
    }
  while (item1);
  
  return NULL;
}

/**
 * bse_item_cross_ref
 * @owner:        reference owner
 * @ref_item:     item to be referenced by @owner
 * @uncross_func: notifier to be executed on uncrossing
 *
 * Install a weak cross reference from @owner to @ref_item.
 * The two items must have a common ancestor when the cross
 * reference is installed. Once their ancestry changes so that
 * they don't have a common ancestor anymore, @uncross_func()
 * is executed.
 */
void
bse_item_cross_ref (BseItem         *owner,
		    BseItem         *ref_item,
		    BseItemUncross   uncross_func)
{
  BseItem *container;
  
  g_return_if_fail (BSE_IS_ITEM (owner));
  g_return_if_fail (BSE_IS_ITEM (ref_item));
  g_return_if_fail (uncross_func != NULL);
  
  container = bse_item_common_ancestor (owner, ref_item);
  
  if (container)
    bse_container_cross_ref (BSE_CONTAINER (container), owner, ref_item, uncross_func);
  else
    g_warning ("%s: `%s' and `%s' have no common anchestor", G_STRLOC,
	       BSE_OBJECT_TYPE_NAME (owner),
	       BSE_OBJECT_TYPE_NAME (ref_item));
}

/**
 * bse_item_cross_unref
 * @owner:        reference owner
 * @ref_item:     item referenced by @owner
 *
 * Removes a cross reference previously installed via
 * bse_item_cross_ref() without executing the associated notifier.
 */
void
bse_item_cross_unref (BseItem *owner,
		      BseItem *ref_item)
{
  BseItem *container;
  
  g_return_if_fail (BSE_IS_ITEM (owner));
  g_return_if_fail (BSE_IS_ITEM (ref_item));
  
  container = bse_item_common_ancestor (owner, ref_item);
  
  if (container)
    bse_container_cross_unref (BSE_CONTAINER (container), owner, ref_item, FALSE);
  else
    g_warning ("%s: `%s' and `%s' have no common anchestor", G_STRLOC,
	       BSE_OBJECT_TYPE_NAME (owner),
	       BSE_OBJECT_TYPE_NAME (ref_item));
}

/**
 * bse_item_uncross
 * @owner:        reference owner
 * @ref_item:     item referenced by @owner
 *
 * Destroys an existing cross reference previously installed via
 * bse_item_cross_ref() by executing the associated notifier.
 */
void
bse_item_uncross (BseItem *owner,
		  BseItem *ref_item)
{
  BseItem *container;
  
  g_return_if_fail (BSE_IS_ITEM (owner));
  g_return_if_fail (BSE_IS_ITEM (ref_item));
  
  container = bse_item_common_ancestor (owner, ref_item);
  
  if (container)
    bse_container_cross_unref (BSE_CONTAINER (container), owner, ref_item, TRUE);
  else
    g_warning ("%s: `%s' and `%s' have no common anchestor", G_STRLOC,
	       BSE_OBJECT_TYPE_NAME (owner),
	       BSE_OBJECT_TYPE_NAME (ref_item));
}

static gboolean
cross_list_func (BseItem *owner,
		 BseItem *ref_item,
		 gpointer data_p)
{
  gpointer *data = data_p;
  BseItem *item = data[0];
  
  if (item == ref_item)
    data[1] = g_list_prepend (data[1], owner);
  
  return TRUE;
}

GList*
bse_item_list_cross_owners (BseItem *item)
{
  gpointer data[2] = { item, NULL };
  
  g_return_val_if_fail (BSE_IS_ITEM (item), NULL);
  
  do
    {
      if (BSE_IS_CONTAINER (item))
	bse_container_cross_forall (BSE_CONTAINER (item), cross_list_func, data);
      item = item->parent;
    }
  while (item);
  
  return data[1];
}

static gboolean
cross_check_func (BseItem *owner,
		  BseItem *ref_item,
		  gpointer data_p)
{
  gpointer *data = data_p;
  BseItem *item = data[0];
  
  if (item == ref_item)
    {
      data[1] = GINT_TO_POINTER (TRUE);
      
      return FALSE;
    }
  else
    return TRUE;
}

gboolean
bse_item_has_cross_owners (BseItem *item)
{
  gpointer data[2] = { item, GINT_TO_POINTER (FALSE) };
  
  g_return_val_if_fail (BSE_IS_ITEM (item), FALSE);
  
  do
    {
      if (BSE_IS_CONTAINER (item))
	bse_container_cross_forall (BSE_CONTAINER (item), cross_check_func, data);
      item = item->parent;
    }
  while (item);
  
  return GPOINTER_TO_INT (data[1]);
}

BseSuper*
bse_item_get_super (BseItem *item)
{
  g_return_val_if_fail (BSE_IS_ITEM (item), NULL);
  
  while (!BSE_IS_SUPER (item) && item)
    item = item->parent;
  
  return item ? BSE_SUPER (item) : NULL;
}

BseProject*
bse_item_get_project (BseItem *item)
{
  BseSuper *super;
  
  g_return_val_if_fail (BSE_IS_ITEM (item), NULL);
  
  super = bse_item_get_super (item);
  
  return super ? bse_super_get_project (super) : NULL;
}

gboolean
bse_item_has_ancestor (BseItem *item,
		       BseItem *ancestor)
{
  g_return_val_if_fail (BSE_IS_ITEM (item), FALSE);
  g_return_val_if_fail (BSE_IS_ITEM (ancestor), FALSE);
  
  while (item->parent)
    {
      item = item->parent;
      if (item == ancestor)
	return TRUE;
    }
  
  return FALSE;
}

static inline BseErrorType
bse_item_execva_i (BseItem     *item,
		   const gchar *procedure,
		   va_list      var_args,
		   gboolean     skip_oparams)
{
  GType type, proc_type;
  BseErrorType error;
  GValue obj_value;
  guint l2;
  
  /* FIXME: we could need faster lookups here */
  type = BSE_OBJECT_TYPE (item);
  l2 = strlen (procedure);
  do
    {
      gchar *p, *name, *type_name = g_type_name (type);
      guint l1 = strlen (type_name);
      
      name = strcpy (g_new (gchar, l1 + 1 + l2 + 1), type_name);
      p = name + l1;
      *(p++) = '+';
      strcpy (p, procedure);
      
      proc_type = bse_procedure_lookup (name);
      g_free (name);
      type = g_type_parent (type);
    }
  while (!proc_type && g_type_is_a (type, BSE_TYPE_ITEM));
  
  if (!proc_type)
    {
      g_warning ("No such procedure \"%s\" for item `%s'",
		 procedure,
		 BSE_OBJECT_TYPE_NAME (item));
      return BSE_ERROR_INTERNAL;
    }
  
  obj_value.g_type = 0;
  g_value_init (&obj_value, BSE_TYPE_ITEM);
  g_value_set_object (&obj_value, item);
  error = bse_procedure_marshal_valist (proc_type, &obj_value, NULL, NULL, skip_oparams, var_args);
  g_value_unset (&obj_value);
  return error;
}

BseErrorType
bse_item_exec_proc (gpointer	 _item,
		    const gchar *procedure,
		    ...)
{
  BseItem *item = _item;
  va_list var_args;
  BseErrorType error;
  
  g_return_val_if_fail (BSE_IS_ITEM (item), BSE_ERROR_INTERNAL);
  g_return_val_if_fail (procedure != NULL, BSE_ERROR_INTERNAL);
  
  va_start (var_args, procedure);
  error = bse_item_execva_i (item, procedure, var_args, FALSE);
  va_end (var_args);
  
  return error;
}

BseErrorType
bse_item_exec_void_proc (gpointer     _item,
			 const gchar *procedure,
			 ...)
{
  BseItem *item = _item;
  va_list var_args;
  BseErrorType error;
  
  g_return_val_if_fail (BSE_IS_ITEM (item), BSE_ERROR_INTERNAL);
  g_return_val_if_fail (procedure != NULL, BSE_ERROR_INTERNAL);
  
  va_start (var_args, procedure);
  error = bse_item_execva_i (item, procedure, var_args, TRUE);
  va_end (var_args);
  
  return error;
}

BseStorage*
bse_item_open_undo (BseItem     *item,
		    const gchar *undo_group)
{
  g_return_val_if_fail (BSE_IS_ITEM (item), NULL);
  g_return_val_if_fail (undo_group != NULL, NULL);
  
  return NULL;
}

void
bse_item_close_undo (BseItem    *item,
		     BseStorage *storage)
{
  g_return_if_fail (BSE_IS_ITEM (item));
  g_return_if_fail (BSE_IS_STORAGE (storage));
}

static void
bse_item_store_property (BseObject  *object,
			 BseStorage *storage,
			 GValue     *value,
			 GParamSpec *pspec)
{
  if (g_type_is_a (G_VALUE_TYPE (value), BSE_TYPE_ITEM))
    {
      BseItem *item = BSE_ITEM (object);
      
      bse_storage_break (storage);
      
      bse_storage_handle_break (storage);
      bse_storage_putc (storage, '(');
      bse_storage_puts (storage, pspec->name);
      bse_storage_putc (storage, ' ');
      bse_storage_put_item_link (storage, item, g_value_get_object (value));
      bse_storage_putc (storage, ')');
    }
  else
    BSE_OBJECT_CLASS (parent_class)->store_property (object, storage, value, pspec);
}

static void
object_link_resolved (gpointer     data,
		      BseStorage  *storage,
		      BseItem     *item,
		      BseItem     *dest_item,
		      const gchar *error)
{
  if (error)
    bse_storage_warn (storage, error);
  else
    {
      GParamSpec *pspec = data;
      GValue value = { 0, };
      
      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_value_set_object (&value, dest_item);
      g_object_set_property (G_OBJECT (item), pspec->name, &value);
      g_value_unset (&value);
    }
}

static GTokenType
bse_item_restore_property (BseObject  *object,
			   BseStorage *storage,
			   GValue     *value,
			   GParamSpec *pspec)
{
  if (g_type_is_a (G_VALUE_TYPE (value), BSE_TYPE_ITEM))
    {
      BseItem *item = BSE_ITEM (object);
      GTokenType token;
      
      /* parse the value for this pspec, including the trailing closing ')' */
      token = bse_storage_parse_item_link (storage, item, object_link_resolved, pspec);
      bse_storage_scanner_parse_or_return (storage->scanner, ')');
      
      /* we cannot provide the object value at this time */
      g_value_set_object (value, NULL);
      g_object_set_property (G_OBJECT (object), pspec->name, value);
      
      return token;
    }
  else
    return BSE_OBJECT_CLASS (parent_class)->restore_property (object, storage, value, pspec);
}
