/* BSE - Bedevilled Sound Engine
 * Copyright (C) 2002 Tim Janik
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
#include "bsejanitor.h"
#include "bsemain.h"
#include "bseglue.h"
#include "bseserver.h"
#include "bsecontainer.h"
#include "bseprocedure.h"
#include "bsescripthelper.h"

enum
{
  PROP_0,
  PROP_USER_MSG_TYPE,
  PROP_USER_MSG,
  PROP_CONNECTED,
  PROP_IDENT,
};


/* --- prototypes --- */
static void	bse_janitor_class_init		(BseJanitorClass	*class);
static void	bse_janitor_init		(BseJanitor		*janitor);
static void	bse_janitor_finalize		(GObject	        *object);
static void     bse_janitor_set_property	(GObject		*janitor,
						 guint          	 param_id,
						 const GValue         	*value,
						 GParamSpec     	*pspec);
static void     bse_janitor_get_property	(GObject	     	*janitor,
						 guint          	 param_id,
						 GValue         	*value,
						 GParamSpec     	*pspec);
static void     bse_janitor_set_parent		(BseItem                *item,
						 BseItem                *parent);
static void	janitor_install_jsource		(BseJanitor		*self);
static gboolean	janitor_kill_jsource		(gpointer		 data);
static void	janitor_port_closed		(SfiComPort		*port,
						 gpointer		 close_data);
static GValue*	janitor_client_msg		(SfiGlueDecoder		*decoder,
						 gpointer		 user_data,
						 const gchar		*message,
						 const GValue		*value);


/* --- variables --- */
static GTypeClass *parent_class = NULL;
static GSList     *janitor_stack = NULL;
static guint       signal_action = 0;
static guint       signal_action_changed = 0;
static guint       signal_closed = 0;
static guint       signal_progress = 0;


/* --- functions --- */
BSE_BUILTIN_TYPE (BseJanitor)
{
  static const GTypeInfo janitor_info = {
    sizeof (BseJanitorClass),
    
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) bse_janitor_class_init,
    (GClassFinalizeFunc) NULL,
    NULL /* class_data */,
    
    sizeof (BseJanitor),
    0 /* n_preallocs */,
    (GInstanceInitFunc) bse_janitor_init,
  };
  
  return bse_type_register_static (BSE_TYPE_ITEM,
				   "BseJanitor",
				   "BSE connection interface object",
				   &janitor_info);
}

static void
bse_janitor_class_init (BseJanitorClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  BseObjectClass *object_class = BSE_OBJECT_CLASS (class);
  BseItemClass *item_class = BSE_ITEM_CLASS (class);
  
  parent_class = g_type_class_peek_parent (class);
  
  gobject_class->set_property = bse_janitor_set_property;
  gobject_class->get_property = bse_janitor_get_property;
  gobject_class->finalize = bse_janitor_finalize;
  
  item_class->set_parent = bse_janitor_set_parent;
  
  bse_object_class_add_param (object_class, NULL,
			      PROP_USER_MSG_TYPE,
			      bse_param_spec_genum ("user-msg-type", "User Message Type", NULL,
						    BSE_TYPE_USER_MSG_TYPE, BSE_USER_MSG_INFO,
						    SFI_PARAM_GUI));
  bse_object_class_add_param (object_class, NULL,
			      PROP_USER_MSG,
			      sfi_pspec_string ("user-msg", "User Message", NULL,
						NULL, SFI_PARAM_GUI));
  bse_object_class_add_param (object_class, NULL,
			      PROP_CONNECTED,
			      sfi_pspec_bool ("connected", "Connected", NULL,
					      FALSE, SFI_PARAM_SERVE_GUI SFI_PARAM_READABLE));
  bse_object_class_add_param (object_class, NULL,
			      PROP_IDENT,
			      sfi_pspec_string ("ident", "Script Identifier", NULL,
						NULL, SFI_PARAM_GUI));
  
  signal_progress = bse_object_class_add_signal (object_class, "progress",
						 G_TYPE_NONE, 1, G_TYPE_FLOAT);
  signal_action_changed = bse_object_class_add_dsignal (object_class, "action-changed",
							G_TYPE_NONE, 2,
							G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE, G_TYPE_INT);
  signal_action = bse_object_class_add_dsignal (object_class, "action",
						G_TYPE_NONE, 2,
						G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE, G_TYPE_INT);
  signal_closed = bse_object_class_add_signal (object_class, "closed",
					       G_TYPE_NONE, 0);
}

static void
bse_janitor_init (BseJanitor *self)
{
  self->kill_pending = FALSE;
  self->port = NULL;
  self->context = NULL;
  self->decoder = NULL;
  self->source = NULL;
  self->user_msg_type = BSE_USER_MSG_INFO;
  self->user_msg = NULL;
  self->script_name = NULL;
  self->proc_name = NULL;
  self->actions = NULL;
}

static void
bse_janitor_set_property (GObject      *object,
			  guint         param_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
  BseJanitor *self = BSE_JANITOR (object);
  
  switch (param_id)
    {
    case PROP_USER_MSG_TYPE:
      self->user_msg_type = g_value_get_enum (value);
      break;
    case PROP_USER_MSG:
      g_free (self->user_msg);
      self->user_msg = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, param_id, pspec);
      break;
    }
}

static void
bse_janitor_get_property (GObject    *object,
			  guint       param_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
  BseJanitor *self = BSE_JANITOR (object);
  
  switch (param_id)
    {
    case PROP_USER_MSG_TYPE:
      g_value_set_enum (value, self->user_msg_type);
      break;
    case PROP_USER_MSG:
      sfi_value_set_string (value, self->user_msg);
      break;
    case PROP_CONNECTED:
      sfi_value_set_bool (value, self->port && self->port->connected);
      break;
    case PROP_IDENT:
      sfi_value_set_string (value, bse_janitor_get_ident (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, param_id, pspec);
      break;
    }
}

static void
bse_janitor_finalize (GObject *object)
{
  BseJanitor *self = BSE_JANITOR (object);
  
  g_return_if_fail (self->port == NULL);
  g_return_if_fail (self->source == NULL);
  
  while (self->actions)
    {
      BseJanitorAction *a = self->actions->data;
      bse_janitor_remove_action (self, g_quark_to_string (a->action));
    }
  
  g_free (self->user_msg);
  g_free (self->script_name);
  g_free (self->proc_name);
  
  /* chain parent class' handler */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

BseJanitor*
bse_janitor_new (SfiComPort *port)
{
  BseJanitor *self;
  
  g_return_val_if_fail (port != NULL, NULL);
  
  self = g_object_new (BSE_TYPE_JANITOR, NULL);
  bse_container_add_item (BSE_CONTAINER (bse_server_get ()), BSE_ITEM (self));
  g_assert (((BseItem*) self)->parent != NULL);

  /* store the port */
  self->port = sfi_com_port_ref (port);
  sfi_com_port_set_close_func (self->port, janitor_port_closed, self);
  /* create server-side glue context */
  self->context = bse_glue_context_intern (port->ident);
  /* create server-side decoder */
  self->decoder = sfi_glue_context_decoder (port, self->context);
  sfi_glue_decoder_add_handler (self->decoder, janitor_client_msg, self);
  /* main loop integration */
  janitor_install_jsource (self);
  
  return self;
}

void
bse_janitor_set_script (BseJanitor  *self,
			const gchar *script_name)
{
  g_return_if_fail (BSE_IS_JANITOR (self));
  
  g_free (self->script_name);
  self->script_name = g_strdup (script_name);
  if (!self->user_msg && script_name)
    {
      self->user_msg = g_strdup (script_name);
      g_object_notify (self, "user-msg");
    }
}

const gchar*
bse_janitor_get_script (BseJanitor *self)
{
  g_return_val_if_fail (BSE_IS_JANITOR (self), NULL);
  
  return self->script_name;
}

const gchar*
bse_janitor_get_ident (BseJanitor *self)
{
  g_return_val_if_fail (BSE_IS_JANITOR (self), NULL);
  
  return self->port ? self->port->ident : NULL;
}

/* bse_janitor_progress
 * @self:     janitor object
 * @progress: progress value
 *
 * Signal progress, @progress is either a value between 0 and 1
 * to indicate completion status or is -1 to indicate progress
 * of unknown amount.
 */
void
bse_janitor_progress (BseJanitor *self,
		      gfloat      progress)
{
  g_return_if_fail (BSE_IS_JANITOR (self));
  
  if (progress < 0)
    progress = -1;
  else
    progress = CLAMP (progress, 0, 1.0);
  g_signal_emit (self, signal_progress, 0, progress);
}

static BseJanitorAction*
find_action (BseJanitor *self,
	     GQuark      aquark)
{
  GSList *slist;
  for (slist = self->actions; slist; slist = slist->next)
    {
      BseJanitorAction *a = slist->data;
      if (a->action == aquark)
	return a;
    }
  return NULL;
}

void
bse_janitor_add_action (BseJanitor  *self,
			const gchar *action,
			const gchar *name,
			const gchar *blurb)
{
  BseJanitorAction *a;
  
  g_return_if_fail (BSE_IS_JANITOR (self));
  g_return_if_fail (action != NULL);
  g_return_if_fail (name != NULL);
  g_return_if_fail (!BSE_OBJECT_DISPOSING (self));
  
  a = find_action (self, g_quark_try_string (action));
  if (!a)
    {
      a = g_new0 (BseJanitorAction, 1);
      a->action = g_quark_from_string (action);
      self->actions = g_slist_append (self->actions, a);
    }
  a->name = g_strdup (name);
  a->blurb = g_strdup (blurb);
  g_signal_emit (self, signal_action_changed, a->action, g_quark_to_string (a->action), g_slist_index (self->actions, a));
}

void
bse_janitor_remove_action (BseJanitor  *self,
			   const gchar *action)
{
  BseJanitorAction *a;
  
  g_return_if_fail (BSE_IS_JANITOR (self));
  g_return_if_fail (action != NULL);
  
  a = find_action (self, g_quark_try_string (action));
  if (a)
    {
      GQuark aquark;
      
      self->actions = g_slist_remove (self->actions, a);
      aquark = a->action;
      g_free (a->name);
      g_free (a->blurb);
      g_free (a);
      if (!BSE_OBJECT_DISPOSING (self))
	g_signal_emit (self, signal_action_changed, aquark, g_quark_to_string (aquark), g_slist_length (self->actions));
    }
}

void
bse_janitor_trigger_action (BseJanitor  *self,
			    const gchar *action)
{
  BseJanitorAction *a;
  
  g_return_if_fail (BSE_IS_JANITOR (self));
  g_return_if_fail (action != NULL);
  
  a = find_action (self, g_quark_try_string (action));
  if (a && !BSE_OBJECT_DISPOSING (self))
    g_signal_emit (self, signal_action, a->action, g_quark_to_string (a->action), g_slist_index (self->actions, a));
}

BseJanitor*
bse_janitor_get_current (void)
{
  return janitor_stack ? janitor_stack->data : NULL;
}

static void
queue_kill (BseJanitor *self)
{
  self->kill_pending = TRUE;
  sfi_com_port_close_remote (self->port, TRUE);
  bse_idle_now (janitor_kill_jsource, g_object_ref (self));
  g_signal_emit (self, signal_closed, 0);
  g_object_notify (self, "connected");
}

void
bse_janitor_queue_kill (BseJanitor *self)
{
  g_return_if_fail (BSE_IS_JANITOR (self));
  g_return_if_fail (self->kill_pending == FALSE);

  if (BSE_ITEM (self)->parent)
    bse_container_remove_item (BSE_CONTAINER (BSE_ITEM (self)->parent), BSE_ITEM (self));
  else
    queue_kill (self);
}

static void
bse_janitor_set_parent (BseItem *item,
			BseItem *parent)
{
  BseJanitor *self = BSE_JANITOR (item);
  
  if (!parent &&	/* removal */
      !self->kill_pending)
    queue_kill (self);

  /* chain parent class' handler */
  BSE_ITEM_CLASS (parent_class)->set_parent (item, parent);
}

static GValue*
janitor_client_msg (SfiGlueDecoder *decoder,
		    gpointer        user_data,
		    const gchar    *message,
		    const GValue   *value)
{
  BseJanitor *self = BSE_JANITOR (user_data);
  GValue *rvalue;
  rvalue = bse_script_check_client_msg (decoder, self, message, value);
  if (rvalue)
    return rvalue;
  return NULL;
}


/* --- main loop intergration --- */
typedef struct {
  GSource     source;
  BseJanitor *janitor;
} JSource;

static gboolean
janitor_prepare (GSource *source,
		 gint    *timeout_p)
{
  BseJanitor *self = ((JSource*) source)->janitor;
  return sfi_glue_decoder_pending (self->decoder);
}

static gboolean
janitor_check (GSource *source)
{
  BseJanitor *self = ((JSource*) source)->janitor;
  return sfi_glue_decoder_pending (self->decoder);
}

static gboolean
janitor_dispatch (GSource    *source,
		  GSourceFunc callback,
		  gpointer    user_data)
{
  BseJanitor *self = ((JSource*) source)->janitor;
  SfiComPort *port = self->port;

  if (!port)
    return TRUE;        /* keep source alive */

  janitor_stack = g_slist_prepend (janitor_stack, self);
  sfi_glue_decoder_dispatch (self->decoder);
  janitor_stack = g_slist_remove (janitor_stack, self);

#if 0
  if (port->gstring_stdout->len)
    {
      g_printerr ("%s:O: %s", port->ident, port->gstring_stdout->str);
      g_string_truncate (port->gstring_stdout, 0);
    }
  if (port->gstring_stderr->len)
    {
      g_printerr ("%s:E: %s", port->ident, port->gstring_stderr->str);
      g_string_truncate (port->gstring_stderr, 0);
    }
#endif
  if (!port->connected && !self->kill_pending)
    {
      g_print ("QUEUE-KILL on %s\n", self->port->ident);
      bse_janitor_queue_kill (self);
    }
  return TRUE;
}

static void
janitor_install_jsource (BseJanitor *self)
{
  static GSourceFuncs jsource_funcs = {
    janitor_prepare,
    janitor_check,
    janitor_dispatch,
  };
  GSource *source = g_source_new (&jsource_funcs, sizeof (JSource));
  JSource *jsource = (JSource*) source;
  SfiRing *ring;
  GPollFD *pfd;

  g_return_if_fail (self->source == NULL);

  jsource->janitor = self;
  self->source = source;
  g_source_set_priority (source, BSE_PRIORITY_PROG_IFACE);
  ring = sfi_glue_decoder_list_poll_fds (self->decoder);
  pfd = sfi_ring_pop_head (&ring);
  while (pfd)
    {
      g_source_add_poll (source, pfd);
      pfd = sfi_ring_pop_head (&ring);
    }
  g_source_attach (source, bse_main_context);
}

static gboolean
janitor_kill_jsource (gpointer data)
{
  BseJanitor *self = BSE_JANITOR (data);

  g_return_val_if_fail (self->source != NULL, FALSE);

  g_source_destroy (self->source);
  self->source = NULL;
  sfi_glue_decoder_destroy (self->decoder);
  self->decoder = NULL;
  sfi_glue_context_destroy (self->context);
  self->context = NULL;
  sfi_com_port_set_close_func (self->port, NULL, NULL);
  sfi_com_port_unref (self->port);
  self->port = NULL;
  g_object_unref (self);
  return FALSE;
}

static void
janitor_port_closed (SfiComPort *port,
		     gpointer    close_data)
{
  BseJanitor *self = BSE_JANITOR (close_data);
  if (!self->kill_pending)
    bse_janitor_queue_kill (self);
}
