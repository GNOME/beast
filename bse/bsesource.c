/* BSE - Bedevilled Sound Engine
 * Copyright (C) 1998-1999, 2000-2002 Tim Janik
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
#include <string.h>
#include <sfi/gbsearcharray.h>
#include "bsesource.h"

#include "bsecontainer.h"
#include "bsestorage.h"
#include "gslcommon.h"
#include "gslengine.h"


/* --- macros --- */
#define parse_or_return         bse_storage_scanner_parse_or_return
#define peek_or_return          bse_storage_scanner_peek_or_return


/* --- typedefs & enums --- */
enum {
  SIGNAL_IO_CHANGED,
  SIGNAL_LAST
};
enum {
  PROP_0,
  PROP_POS_X,
  PROP_POS_Y,
};
typedef struct
{
  guint	      id;
  union {
    struct {
      GslModule  *imodule;
      GslModule  *omodule;
    } mods;
    struct {
      gpointer d1;
      gpointer d2;
    } data;
  } u;
} BseSourceContext;

#define	BSE_SOURCE_N_CONTEXTS(source)	(g_bsearch_array_get_n_nodes ((source)->contexts))


/* --- prototypes --- */
static void         bse_source_class_base_init		(BseSourceClass	*class);
static void         bse_source_class_base_finalize	(BseSourceClass	*class);
static void         bse_source_class_init		(BseSourceClass	*class);
static void         bse_source_init			(BseSource	*source,
							 BseSourceClass	*class);
static void	    bse_source_set_property		(GObject	*object,
							 guint           param_id,
							 const GValue   *value,
							 GParamSpec     *pspec);
static void	    bse_source_get_property		(GObject        *object,
							 guint           param_id,
							 GValue         *value,
							 GParamSpec     *pspec);
static void         bse_source_dispose			(GObject	*object);
static void         bse_source_finalize			(GObject	*object);
static void         bse_source_real_prepare		(BseSource	*source);
static void	    bse_source_real_context_create	(BseSource      *source,
							 guint           context_handle,
							 GslTrans       *trans);
static void	    bse_source_real_context_connect	(BseSource      *source,
							 guint           context_handle,
							 GslTrans       *trans);
static void	    bse_source_real_context_dismiss	(BseSource      *source,
							 guint           context_handle,
							 GslTrans       *trans);
static void         bse_source_real_reset		(BseSource	*source);
static void	    bse_source_real_add_input		(BseSource	*source,
							 guint     	 ichannel,
							 BseSource 	*osource,
							 guint     	 ochannel);
static void	    bse_source_real_remove_input	(BseSource	*source,
							 guint		 ichannel,
							 BseSource      *osource,
							 guint           ochannel);
static void	    bse_source_real_store_private	(BseObject	*object,
							 BseStorage	*storage);
static BseTokenType bse_source_real_restore_private	(BseObject      *object,
							 BseStorage     *storage);
static gint	    contexts_compare			(gconstpointer	 bsearch_node1, /* key */
							 gconstpointer	 bsearch_node2);


/* --- variables --- */
static GTypeClass          *parent_class = NULL;
static guint                source_signals[SIGNAL_LAST] = { 0, };
static const GBSearchConfig context_config = {
  sizeof (BseSourceContext),
  contexts_compare,
  G_BSEARCH_ARRAY_ALIGN_POWER2,
};


/* --- functions --- */
BSE_BUILTIN_TYPE (BseSource)
{
  static const GTypeInfo source_info = {
    sizeof (BseSourceClass),
    
    (GBaseInitFunc) bse_source_class_base_init,
    (GBaseFinalizeFunc) bse_source_class_base_finalize,
    (GClassInitFunc) bse_source_class_init,
    (GClassFinalizeFunc) NULL,
    NULL /* class_data */,
    
    sizeof (BseSource),
    0 /* n_preallocs */,
    (GInstanceInitFunc) bse_source_init,
  };

  g_assert (BSE_SOURCE_FLAGS_USHIFT < BSE_OBJECT_FLAGS_MAX_SHIFT);
  
  return bse_type_register_static (BSE_TYPE_ITEM,
				   "BseSource",
				   "Base type for sound sources",
				   &source_info);
}

static void
bse_source_class_base_init (BseSourceClass *class)
{
  class->channel_defs.n_ichannels = 0;
  class->channel_defs.ichannel_names = NULL;
  class->channel_defs.ichannel_cnames = NULL;
  class->channel_defs.ichannel_blurbs = NULL;
  class->channel_defs.ijstreams = NULL;
  class->channel_defs.n_jstreams = 0;
  class->channel_defs.n_ochannels = 0;
  class->channel_defs.ochannel_names = NULL;
  class->channel_defs.ochannel_cnames = NULL;
  class->channel_defs.ochannel_blurbs = NULL;
}

static void
bse_source_class_base_finalize (BseSourceClass *class)
{
  guint i;
  
  for (i = 0; i < class->channel_defs.n_ichannels; i++)
    {
      g_free (class->channel_defs.ichannel_cnames[i]);
      g_free (class->channel_defs.ichannel_names[i]);
      g_free (class->channel_defs.ichannel_blurbs[i]);
    }
  g_free (class->channel_defs.ichannel_names);
  g_free (class->channel_defs.ichannel_cnames);
  g_free (class->channel_defs.ichannel_blurbs);
  g_free (class->channel_defs.ijstreams);
  class->channel_defs.n_jstreams = 0;
  class->channel_defs.n_ichannels = 0;
  class->channel_defs.ichannel_names = NULL;
  class->channel_defs.ichannel_cnames = NULL;
  class->channel_defs.ichannel_blurbs = NULL;
  class->channel_defs.ijstreams = NULL;
  for (i = 0; i < class->channel_defs.n_ochannels; i++)
    {
      g_free (class->channel_defs.ochannel_cnames[i]);
      g_free (class->channel_defs.ochannel_names[i]);
      g_free (class->channel_defs.ochannel_blurbs[i]);
    }
  g_free (class->channel_defs.ochannel_names);
  g_free (class->channel_defs.ochannel_cnames);
  g_free (class->channel_defs.ochannel_blurbs);
  class->channel_defs.n_ochannels = 0;
  class->channel_defs.ochannel_names = NULL;
  class->channel_defs.ochannel_cnames = NULL;
  class->channel_defs.ochannel_blurbs = NULL;
}

static void
bse_source_class_init (BseSourceClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  BseObjectClass *object_class = BSE_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);
  
  gobject_class->set_property = bse_source_set_property;
  gobject_class->get_property = bse_source_get_property;
  gobject_class->dispose = bse_source_dispose;
  gobject_class->finalize = bse_source_finalize;

  object_class->store_private = bse_source_real_store_private;
  object_class->restore_private = bse_source_real_restore_private;

  class->prepare = bse_source_real_prepare;
  class->context_create = bse_source_real_context_create;
  class->context_connect = bse_source_real_context_connect;
  class->context_dismiss = bse_source_real_context_dismiss;
  class->reset = bse_source_real_reset;
  class->add_input = bse_source_real_add_input;
  class->remove_input = bse_source_real_remove_input;

  bse_object_class_add_param (object_class, "Position",
			      PROP_POS_X,
			      sfi_pspec_real ("pos_x", "Position X", NULL,
					      0, -SFI_MAXREAL, SFI_MAXREAL, 10,
					      SFI_PARAM_STORAGE));
  bse_object_class_add_param (object_class, "Position",
			      PROP_POS_Y,
			      sfi_pspec_real ("pos_y", "Position Y", NULL,
					      0, -SFI_MAXREAL, SFI_MAXREAL, 10,
					      SFI_PARAM_STORAGE));

  source_signals[SIGNAL_IO_CHANGED] = bse_object_class_add_signal (object_class, "io_changed",
								   G_TYPE_NONE, 0);
}

static void
bse_source_init (BseSource      *source,
		 BseSourceClass *class)
{
  source->channel_defs = &BSE_SOURCE_CLASS (class)->channel_defs;
  source->inputs = g_new0 (BseSourceInput, BSE_SOURCE_N_ICHANNELS (source));
  source->outputs = NULL;
  source->contexts = NULL;
  source->pos_x = 0;
  source->pos_y = 0;
}

static void
bse_source_set_property (GObject      *object,
			 guint         param_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
  BseSource *source = BSE_SOURCE (object);
  switch (param_id)
    {
    case PROP_POS_X:
      source->pos_x = sfi_value_get_real (value);
      break;
    case PROP_POS_Y:
      source->pos_y = sfi_value_get_real (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
bse_source_get_property (GObject    *object,
			 guint       param_id,
			 GValue     *value,
			 GParamSpec *pspec)
{
  BseSource *source = BSE_SOURCE (object);
  switch (param_id)
    {
    case PROP_POS_X:
      sfi_value_set_real (value, source->pos_x);
      break;
    case PROP_POS_Y:
      sfi_value_set_real (value, source->pos_y);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
bse_source_dispose (GObject *object)
{
  BseSource *source = BSE_SOURCE (object);

  bse_source_clear_ochannels (source);
  if (BSE_SOURCE_PREPARED (source))
    {
      g_warning (G_STRLOC ": source still prepared during destruction");
      bse_source_reset (source);
    }

  bse_source_clear_ichannels (source);

  /* chain parent class' handler */
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
bse_source_finalize (GObject *object)
{
  BseSource *source = BSE_SOURCE (object);
  guint i;

  for (i = 0; i < BSE_SOURCE_N_ICHANNELS (source); i++)
    if (BSE_SOURCE_IS_JOINT_ICHANNEL (source, i))
      g_free (BSE_SOURCE_INPUT (source, i)->jdata.joints);
  g_free (source->inputs);
  source->inputs = NULL;

  /* chain parent class' handler */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gchar*
channel_dup_canonify (const gchar *name)
{
  gchar *cname = g_new (gchar, strlen (name) + 1);
  const gchar *s;
  gchar *c = cname;

  for (s = name; *s; s++)
    if ((*s >= '0' && *s <= '9') ||
	(*s >= 'a' && *s <= 'z'))
      *c++ = *s;
    else if (*s >= 'A' && *s <= 'Z')
      *c++ = *s - 'A' + 'a';
    else
      *c++ = '-';
  *c++ = 0;
  return cname;
}

static guint
bse_source_class_add_ijchannel (BseSourceClass *source_class,
				const gchar    *name,
				const gchar    *blurb,
				gboolean        is_joint_channel)
{
  BseSourceChannelDefs *defs;
  guint i;
  gchar *cname;

  g_return_val_if_fail (BSE_IS_SOURCE_CLASS (source_class), 0);
  g_return_val_if_fail (name != NULL, 0);
  if (!blurb)
    blurb = name;

  cname = channel_dup_canonify (name);
  i = 0; // FIXME: bse_source_find_ichannel (source, cname);
  if (i == ~0)
    {
      g_warning ("source class `%s' already has a channel \"%s\" with canonical name \"%s\"",
		 G_OBJECT_CLASS_NAME (source_class),
		 name, cname);
      g_free (cname);
      return ~0;
    }
  defs = &source_class->channel_defs;
  i = defs->n_ichannels++;
  defs->ichannel_names = g_renew (gchar*, defs->ichannel_names, defs->n_ichannels);
  defs->ichannel_cnames = g_renew (gchar*, defs->ichannel_cnames, defs->n_ichannels);
  defs->ichannel_blurbs = g_renew (gchar*, defs->ichannel_blurbs, defs->n_ichannels);
  defs->ijstreams = g_renew (guint, defs->ijstreams, defs->n_ichannels);
  defs->ichannel_names[i] = g_strdup (name);
  defs->ichannel_cnames[i] = cname;
  defs->ichannel_blurbs[i] = g_strdup (blurb);
  if (is_joint_channel)
    {
      defs->ijstreams[i] = defs->n_jstreams++;
      defs->ijstreams[i] |= BSE_SOURCE_JSTREAM_FLAG;
    }
  else
    defs->ijstreams[i] = i - defs->n_jstreams;

  return i;
}

guint
bse_source_class_add_ichannel (BseSourceClass *source_class,
			       const gchar    *name,
			       const gchar    *blurb)
{
  return bse_source_class_add_ijchannel (source_class, name, blurb, FALSE);
}

guint
bse_source_class_add_jchannel (BseSourceClass *source_class,
			       const gchar    *name,
			       const gchar    *blurb)
{
  return bse_source_class_add_ijchannel (source_class, name, blurb, TRUE);
}

guint
bse_source_find_ichannel (BseSource   *source,
			  const gchar *ichannel_cname)
{
  guint i;

  g_return_val_if_fail (BSE_IS_SOURCE (source), ~0);
  g_return_val_if_fail (ichannel_cname != NULL, ~0);

  for (i = 0; i < BSE_SOURCE_N_ICHANNELS (source); i++)
    if (strcmp (BSE_SOURCE_ICHANNEL_CNAME (source, i), ichannel_cname) == 0)
      return i;
  return ~0;
}

guint
bse_source_class_add_ochannel (BseSourceClass *source_class,
			       const gchar    *name,
			       const gchar    *blurb)
{
  BseSourceChannelDefs *defs;
  guint i;
  gchar *cname;
  
  g_return_val_if_fail (BSE_IS_SOURCE_CLASS (source_class), 0);
  g_return_val_if_fail (name != NULL, 0);
  if (!blurb)
    blurb = name;
  
  cname = channel_dup_canonify (name);
  i = 0; // FIXME: bse_source_find_ochannel (source, cname);
  if (i == ~0)
    {
      g_warning ("source class `%s' already has a channel \"%s\" with canonical name \"%s\"",
		 G_OBJECT_CLASS_NAME (source_class),
		 name, cname);
      g_free (cname);
      return ~0;
    }
  defs = &source_class->channel_defs;
  i = defs->n_ochannels++;
  defs->ochannel_names = g_renew (gchar*, defs->ochannel_names, defs->n_ochannels);
  defs->ochannel_cnames = g_renew (gchar*, defs->ochannel_cnames, defs->n_ochannels);
  defs->ochannel_blurbs = g_renew (gchar*, defs->ochannel_blurbs, defs->n_ochannels);
  defs->ochannel_names[i] = g_strdup (name);
  defs->ochannel_cnames[i] = cname;
  defs->ochannel_blurbs[i] = g_strdup (blurb);
  
  return i;
}

guint
bse_source_find_ochannel (BseSource   *source,
			  const gchar *ochannel_cname)
{
  guint i;

  g_return_val_if_fail (BSE_IS_SOURCE (source), ~0);
  g_return_val_if_fail (ochannel_cname != NULL, ~0);

  for (i = 0; i < BSE_SOURCE_N_OCHANNELS (source); i++)
    if (strcmp (BSE_SOURCE_OCHANNEL_CNAME (source, i), ochannel_cname) == 0)
      return i;
  return ~0;
}

static gint
contexts_compare (gconstpointer bsearch_node1, /* key */
		  gconstpointer bsearch_node2)
{
  const BseSourceContext *c1 = bsearch_node1;
  const BseSourceContext *c2 = bsearch_node2;

  return G_BSEARCH_ARRAY_CMP (c1->id, c2->id);
}

static void
bse_source_real_prepare (BseSource *source)
{
}

void
bse_source_prepare (BseSource *source)
{
  g_return_if_fail (BSE_IS_SOURCE (source));
  g_return_if_fail (!BSE_SOURCE_PREPARED (source));
  g_return_if_fail (source->contexts == NULL);
  
  bse_object_ref (BSE_OBJECT (source));
  source->contexts = g_bsearch_array_create (&context_config);
  BSE_OBJECT_SET_FLAGS (source, BSE_SOURCE_FLAG_PREPARED);
  BSE_SOURCE_GET_CLASS (source)->prepare (source);
  bse_object_unref (BSE_OBJECT (source));
}

static void
bse_source_real_reset (BseSource *source)
{
}

void
bse_source_reset (BseSource *source)
{
  guint n_contexts;

  g_return_if_fail (BSE_IS_SOURCE (source));
  g_return_if_fail (BSE_SOURCE_PREPARED (source));
  g_return_if_fail (source->contexts != NULL);
  
  bse_object_ref (BSE_OBJECT (source));
  n_contexts = BSE_SOURCE_N_CONTEXTS (source);
  if (n_contexts)
    {
      GslTrans *trans = gsl_trans_open ();
      while (n_contexts)
	{
	  BseSourceContext *context = g_bsearch_array_get_nth (source->contexts,
							       &context_config,
							       n_contexts - 1);
	  bse_source_dismiss_context (source, context->id, trans);
	  n_contexts = BSE_SOURCE_N_CONTEXTS (source);
	}
      gsl_trans_commit (trans);
    }
  gsl_engine_wait_on_trans ();
  BSE_OBJECT_UNSET_FLAGS (source, BSE_SOURCE_FLAG_PREPARED);
  BSE_SOURCE_GET_CLASS (source)->reset (source);
  g_bsearch_array_free (source->contexts, &context_config);
  source->contexts = NULL;
  bse_object_unref (BSE_OBJECT (source));
}

static void
bse_source_real_context_create	(BseSource *source,
				 guint      context_handle,
				 GslTrans  *trans)
{
}

static inline BseSourceContext*
context_nth (BseSource *source,
	     guint      index)
{
  g_return_val_if_fail (index < BSE_SOURCE_N_CONTEXTS (source), NULL);

  return g_bsearch_array_get_nth (source->contexts, &context_config, index);
}

static inline BseSourceContext*
context_lookup (BseSource *source,
		guint      context_handle)
{
  BseSourceContext key;

  key.id = context_handle;
  return g_bsearch_array_lookup (source->contexts, &context_config, &key);
}

gboolean
bse_source_has_context (BseSource *source,
			guint      context_handle)
{
  BseSourceContext *context;

  g_return_val_if_fail (BSE_IS_SOURCE (source), FALSE);

  context = context_lookup (source, context_handle);

  return context != NULL;
}

guint*
bse_source_context_ids (BseSource *source,
			guint     *n_ids)
{
  guint *cids, i;

  g_return_val_if_fail (BSE_IS_SOURCE (source), FALSE);
  g_return_val_if_fail (n_ids != NULL, FALSE);

  cids = g_new (guint, BSE_SOURCE_N_CONTEXTS (source));
  for (i = 0; i < BSE_SOURCE_N_CONTEXTS (source); i++)
    {
      BseSourceContext *context = context_nth (source, i);

      cids[i] = context->id;
    }
  *n_ids = BSE_SOURCE_N_CONTEXTS (source);

  return cids;
}

gpointer
bse_source_get_context_data (BseSource *source,
			     guint      context_handle)
{
  BseSourceContext *context;

  g_return_val_if_fail (BSE_IS_SOURCE (source), NULL);
  g_return_val_if_fail (BSE_SOURCE_PREPARED (source), NULL);
  g_return_val_if_fail (!BSE_SOURCE_N_ICHANNELS (source) && !BSE_SOURCE_N_OCHANNELS (source), NULL);
  g_return_val_if_fail (context_handle > 0, NULL);

  context = context_lookup (source, context_handle);
  return context ? context->u.data.d2 : NULL;
}

static void
source_create_context (BseSource               *source,
		       guint                    context_handle,
		       gpointer                 data,
		       BseSourceFreeContextData free_data,
		       const gchar             *str_loc,
		       GslTrans                *trans)
{
  BseSourceContext *context, key = { 0, };

  context = context_lookup (source, context_handle);
  if (context)
    {
      g_warning ("%s: context %u on %p exists already", str_loc, context->id, source);
      return;
    }

  g_object_ref (source);
  key.id = context_handle;
  key.u.data.d1 = free_data;
  key.u.data.d2 = data;
  source->contexts = g_bsearch_array_insert (source->contexts, &context_config, &key);
  BSE_SOURCE_GET_CLASS (source)->context_create (source, key.id, trans);
  context = context_lookup (source, context_handle);
  g_return_if_fail (context != NULL);
  if (BSE_SOURCE_N_ICHANNELS (source) && !context->u.mods.imodule)
    g_warning ("%s: source `%s' failed to create %s module",
	       str_loc,
	       G_OBJECT_TYPE_NAME (source), "input");
  if (BSE_SOURCE_N_OCHANNELS (source) && !context->u.mods.omodule)
    g_warning ("%s: source `%s' failed to create %s module",
               str_loc,
	       G_OBJECT_TYPE_NAME (source), "output");
  g_object_unref (source);
}

void
bse_source_create_context_with_data (BseSource      *source,
				     guint           context_handle,
				     gpointer        data,
				     BseSourceFreeContextData free_data,
				     GslTrans       *trans)
{
  g_return_if_fail (BSE_IS_SOURCE (source));
  g_return_if_fail (BSE_SOURCE_PREPARED (source));
  g_return_if_fail (!BSE_SOURCE_N_ICHANNELS (source) && !BSE_SOURCE_N_OCHANNELS (source));
  g_return_if_fail (context_handle > 0);
  g_return_if_fail (trans != NULL);

  source_create_context (source, context_handle, data, free_data, G_STRLOC, trans);
}

void
bse_source_create_context (BseSource *source,
			   guint      context_handle,
			   GslTrans  *trans)
{
  g_return_if_fail (BSE_IS_SOURCE (source));
  g_return_if_fail (BSE_SOURCE_PREPARED (source));
  g_return_if_fail (context_handle > 0);
  g_return_if_fail (trans != NULL);

  source_create_context (source, context_handle, NULL, NULL, G_STRLOC, trans);
}

static void
bse_source_context_connect_ichannel (BseSource        *source,
				     BseSourceContext *context,
				     guint             ichannel,
				     GslTrans         *trans,
				     guint	       first_joint)
{
  BseSourceInput *input = BSE_SOURCE_INPUT (source, ichannel);

  if (BSE_SOURCE_IS_JOINT_ICHANNEL (source, ichannel))
    {
      guint i;

      for (i = first_joint; i < input->jdata.n_joints; i++)
	{
	  BseSourceOutput *output = input->jdata.joints + i;

	  if (output->osource)
	    {
	      GslModule *omodule = bse_source_get_context_omodule (output->osource,
								   context->id);
	      gsl_trans_add (trans,
			     gsl_job_jconnect (omodule,
					       BSE_SOURCE_OCHANNEL_OSTREAM (output->osource,
									   output->ochannel),
					       context->u.mods.imodule,
					       BSE_SOURCE_ICHANNEL_JSTREAM (source, ichannel)));
	    }
	}
    }
  else
    {
      if (input->idata.osource)
	{
	  GslModule *omodule = bse_source_get_context_omodule (input->idata.osource,
							       context->id);
	  gsl_trans_add (trans,
			 gsl_job_connect (omodule,
					  BSE_SOURCE_OCHANNEL_OSTREAM (input->idata.osource,
								       input->idata.ochannel),
					  context->u.mods.imodule,
					  BSE_SOURCE_ICHANNEL_ISTREAM (source, ichannel)));
	}
    }
}

static void
bse_source_real_context_connect	(BseSource *source,
				 guint      context_handle,
				 GslTrans  *trans)
{
  guint i;

  for (i = 0; i < BSE_SOURCE_N_ICHANNELS (source); i++)
    {
      BseSourceContext *context = context_lookup (source, context_handle);

      bse_source_context_connect_ichannel (source, context, i, trans, 0);
    }
}

void
bse_source_connect_context (BseSource *source,
			    guint      context_handle,
			    GslTrans  *trans)
{
  BseSourceContext *context;

  g_return_if_fail (BSE_IS_SOURCE (source));
  g_return_if_fail (BSE_SOURCE_PREPARED (source));
  g_return_if_fail (context_handle > 0);
  g_return_if_fail (trans != NULL);

  context = context_lookup (source, context_handle);
  if (context)
    {
      g_object_ref (source);
      BSE_SOURCE_GET_CLASS (source)->context_connect (source, context_handle, trans);
      g_object_unref (source);
    }
  else
    g_warning ("%s: no such context %u", G_STRLOC, context_handle);
}

static void
bse_source_real_context_dismiss	(BseSource *source,
				 guint      context_handle,
				 GslTrans  *trans)
{
  BseSourceContext *context = context_lookup (source, context_handle);

  if (BSE_SOURCE_N_ICHANNELS (source) || BSE_SOURCE_N_OCHANNELS (source))
    {
      if (context->u.mods.imodule)
	gsl_trans_add (trans, gsl_job_discard (context->u.mods.imodule));
      if (context->u.mods.omodule && context->u.mods.omodule != context->u.mods.imodule)
	gsl_trans_add (trans, gsl_job_discard (context->u.mods.omodule));
      context->u.mods.imodule = NULL;
      context->u.mods.omodule = NULL;
    }
}

void
bse_source_dismiss_context (BseSource *source,
			    guint      context_handle,
			    GslTrans  *trans)
{
  BseSourceContext *context;

  g_return_if_fail (BSE_IS_SOURCE (source));
  g_return_if_fail (BSE_SOURCE_PREPARED (source));
  g_return_if_fail (context_handle > 0);
  g_return_if_fail (trans != NULL);

  context = context_lookup (source, context_handle);
  if (context)
    {
      BseSourceFreeContextData free_cdata = NULL;
      gpointer cdata = NULL;
      g_object_ref (source);
      BSE_SOURCE_GET_CLASS (source)->context_dismiss (source, context_handle, trans);
      context = context_lookup (source, context_handle);
      g_return_if_fail (context != NULL);
      if (BSE_SOURCE_N_ICHANNELS (source) && context->u.mods.imodule)
	g_warning ("%s: source `%s' failed to dismiss %s module",
		   G_STRLOC,
		   G_OBJECT_TYPE_NAME (source), "input");
      if (BSE_SOURCE_N_OCHANNELS (source) && context->u.mods.omodule)
	g_warning ("%s: source `%s' failed to dismiss %s module",
		   G_STRLOC,
		   G_OBJECT_TYPE_NAME (source), "output");
      if (BSE_SOURCE_N_OCHANNELS (source) == 0 &&
	  BSE_SOURCE_N_ICHANNELS (source) == 0)
	{
	  free_cdata = context->u.data.d1;
	  cdata = context->u.data.d2;
	}
      source->contexts = g_bsearch_array_remove (source->contexts, &context_config,
						 g_bsearch_array_get_index (source->contexts,
									    &context_config,
									    context));
      if (free_cdata)
	free_cdata (source, cdata, trans);
      g_object_unref (source);
    }
  else
    g_warning ("%s: no such context %u", G_STRLOC, context_handle);
}

void
bse_source_set_context_imodule (BseSource *source,
				guint	   context_handle,
				GslModule *imodule)
{
  BseSourceContext *context;

  g_return_if_fail (BSE_IS_SOURCE (source));
  g_return_if_fail (BSE_SOURCE_PREPARED (source));
  g_return_if_fail (context_handle > 0);
  g_return_if_fail (BSE_SOURCE_N_ICHANNELS (source) > 0);
  if (imodule)
    {
      g_return_if_fail (GSL_MODULE_N_JSTREAMS (imodule) >= BSE_SOURCE_N_JOINT_ICHANNELS (source));
      if (BSE_SOURCE_N_JOINT_ICHANNELS (source))
	{
	  guint n_non_joint_ichannels = BSE_SOURCE_N_ICHANNELS (source) - BSE_SOURCE_N_JOINT_ICHANNELS (source);
	  g_return_if_fail (GSL_MODULE_N_ISTREAMS (imodule) >= n_non_joint_ichannels);
	}
      else
	g_return_if_fail (GSL_MODULE_N_ISTREAMS (imodule) >= BSE_SOURCE_N_ICHANNELS (source));
    }

  context = context_lookup (source, context_handle);
  if (!context)
    {
      g_warning ("%s: no such context %u", G_STRLOC, context_handle);
      return;
    }
  if (imodule)
    g_return_if_fail (context->u.mods.imodule == NULL);
  else
    g_return_if_fail (context->u.mods.imodule != NULL);

  context->u.mods.imodule = imodule;
}

GslModule*
bse_source_get_context_imodule (BseSource *source,
				guint      context_handle)
{
  BseSourceContext *context;

  g_return_val_if_fail (BSE_IS_SOURCE (source), NULL);
  g_return_val_if_fail (BSE_SOURCE_PREPARED (source), NULL);
  g_return_val_if_fail (BSE_SOURCE_N_ICHANNELS (source) > 0, NULL);
  
  context = context_lookup (source, context_handle);
  if (!context)
    {
      g_warning ("%s: no such context %u", G_STRLOC, context_handle);
      return NULL;
    }
  return context->u.mods.imodule;
}

void
bse_source_set_context_omodule (BseSource *source,
				guint	   context_handle,
				GslModule *omodule)
{
  BseSourceContext *context;

  g_return_if_fail (BSE_IS_SOURCE (source));
  g_return_if_fail (BSE_SOURCE_PREPARED (source));
  g_return_if_fail (context_handle > 0);
  g_return_if_fail (BSE_SOURCE_N_OCHANNELS (source) > 0);
  if (omodule)
    g_return_if_fail (GSL_MODULE_N_OSTREAMS (omodule) >= BSE_SOURCE_N_OCHANNELS (source));

  context = context_lookup (source, context_handle);
  if (!context)
    {
      g_warning ("%s: no such context %u", G_STRLOC, context_handle);
      return;
    }
  if (omodule)
    g_return_if_fail (context->u.mods.omodule == NULL);
  else
    g_return_if_fail (context->u.mods.omodule != NULL);

  context->u.mods.omodule = omodule;
}

GslModule*
bse_source_get_context_omodule (BseSource *source,
				guint      context_handle)
{
  BseSourceContext *context;
  
  g_return_val_if_fail (BSE_IS_SOURCE (source), NULL);
  g_return_val_if_fail (BSE_SOURCE_PREPARED (source), NULL);
  g_return_val_if_fail (BSE_SOURCE_N_OCHANNELS (source) > 0, NULL);
  
  context = context_lookup (source, context_handle);
  if (!context)
    {
      g_warning ("%s: no such context %u", G_STRLOC, context_handle);
      return NULL;
    }
  return context->u.mods.omodule;
}

void
bse_source_set_context_module (BseSource *source,
			       guint      context_handle,
			       GslModule *module)
{
  g_return_if_fail (BSE_IS_SOURCE (source));
  g_return_if_fail (BSE_SOURCE_PREPARED (source));
  g_return_if_fail (module != NULL);
  g_return_if_fail (GSL_MODULE_N_OSTREAMS (module) >= BSE_SOURCE_N_OCHANNELS (source));
  g_return_if_fail (GSL_MODULE_N_ISTREAMS (module) + GSL_MODULE_N_JSTREAMS (module) >= BSE_SOURCE_N_ICHANNELS (source));
  
  if (BSE_SOURCE_N_ICHANNELS (source))
    bse_source_set_context_imodule (source, context_handle, module);
  if (BSE_SOURCE_N_OCHANNELS (source))
    bse_source_set_context_omodule (source, context_handle, module);
}

void
bse_source_flow_access_module (BseSource    *source,
			       guint         context_handle,
			       guint64       tick_stamp,
			       GslAccessFunc access_func,
			       gpointer      data,
			       GslFreeFunc   data_free_func,
			       GslTrans     *trans)
{
  BseSourceContext *context;
  GslModule *m1, *m2;

  g_return_if_fail (BSE_IS_SOURCE (source));
  g_return_if_fail (BSE_SOURCE_PREPARED (source));
  g_return_if_fail (access_func != NULL);
  g_return_if_fail (context_handle > 0);
  g_return_if_fail (BSE_SOURCE_N_ICHANNELS (source) || BSE_SOURCE_N_OCHANNELS (source));
  
  context = context_lookup (source, context_handle);
  if (!context)
    {
      g_warning ("%s: no such context %u", G_STRLOC, context_handle);
      return;
    }
  m1 = context->u.mods.imodule;
  m2 = context->u.mods.omodule;
  if (m1 == m2)
    m1 = NULL;

  if (m1 || m2)
    {
      GslTrans *my_trans = trans ? trans : gsl_trans_open ();

      if (m1)
	gsl_trans_add (my_trans, gsl_flow_job_access (m1, tick_stamp, access_func, data,
						      m2 ? NULL : data_free_func));
      if (m2)
	gsl_trans_add (my_trans, gsl_flow_job_access (m2, tick_stamp, access_func, data,
						      data_free_func));
      if (!trans)
	gsl_trans_commit (my_trans);
    }
  else if (data_free_func)
    data_free_func (data);
}

void
bse_source_flow_access_modules (BseSource    *source,
				guint64       tick_stamp,
				GslAccessFunc access_func,
				gpointer      data,
				GslFreeFunc   data_free_func,
				GslTrans     *trans)
{
  GSList *modules = NULL;
  guint i;

  g_return_if_fail (BSE_IS_SOURCE (source));
  g_return_if_fail (BSE_SOURCE_PREPARED (source));
  g_return_if_fail (access_func != NULL);
  g_return_if_fail (BSE_SOURCE_N_ICHANNELS (source) || BSE_SOURCE_N_OCHANNELS (source));
  
  for (i = 0; i < BSE_SOURCE_N_CONTEXTS (source); i++)
    {
      BseSourceContext *context = context_nth (source, i);

      if (context->u.mods.imodule)
	modules = g_slist_prepend (modules, context->u.mods.imodule);
      else if (context->u.mods.omodule && context->u.mods.omodule != context->u.mods.imodule)
	modules = g_slist_prepend (modules, context->u.mods.omodule);
    }
  
  if (modules)
    {
      GslTrans *my_trans = trans ? trans : gsl_trans_open ();
      GSList *slist;
      
      for (slist = modules; slist; slist = slist->next)
	gsl_trans_add (my_trans, gsl_flow_job_access (slist->data, tick_stamp, access_func, data,
						      slist->next ? NULL : data_free_func));
      if (!trans)
	gsl_trans_commit (my_trans);
      g_slist_free (modules);
    }
  else if (data_free_func)
    data_free_func (data);
}

void
bse_source_access_modules (BseSource    *source,
			   GslAccessFunc access_func,
			   gpointer      data,
			   GslFreeFunc   data_free_func,
			   GslTrans     *trans)
{
  GSList *modules = NULL;
  guint i;
  
  g_return_if_fail (BSE_IS_SOURCE (source));
  g_return_if_fail (BSE_SOURCE_PREPARED (source));
  g_return_if_fail (access_func != NULL);
  g_return_if_fail (BSE_SOURCE_N_ICHANNELS (source) || BSE_SOURCE_N_OCHANNELS (source));
  
  for (i = 0; i < BSE_SOURCE_N_CONTEXTS (source); i++)
    {
      BseSourceContext *context = context_nth (source, i);

      if (context->u.mods.imodule)
	modules = g_slist_prepend (modules, context->u.mods.imodule);
      else if (context->u.mods.omodule && context->u.mods.omodule != context->u.mods.imodule)
	modules = g_slist_prepend (modules, context->u.mods.omodule);
    }

  if (modules)
    {
      GslTrans *my_trans = trans ? trans : gsl_trans_open ();
      GSList *slist;

      for (slist = modules; slist; slist = slist->next)
	gsl_trans_add (my_trans, gsl_job_access (slist->data, access_func, data,
						 slist->next ? NULL : data_free_func));
      if (!trans)
	gsl_trans_commit (my_trans);
      g_slist_free (modules);
    }
  else if (data_free_func)
    data_free_func (data);
}

typedef struct {
  guint  member_offset;
  guint  member_size;
} AccessData;

static void
op_access_update (GslModule *module,
		  gpointer   data)
{
  AccessData *adata = data;
  guint8 *m = module->user_data;

  memcpy (m + adata->member_offset, adata + 1, adata->member_size);
}

void
bse_source_update_modules (BseSource *source,
			   guint      member_offset,
			   gpointer   member_data,
			   guint      member_size,
			   GslTrans  *trans)
{
  AccessData *adata;

  g_return_if_fail (BSE_IS_SOURCE (source));
  g_return_if_fail (BSE_SOURCE_PREPARED (source));
  g_return_if_fail (member_data != NULL);
  g_return_if_fail (member_size > 0);

  adata = g_malloc (sizeof (AccessData) + member_size);
  adata->member_offset = member_offset;
  adata->member_size = member_size;
  memcpy (adata + 1, member_data, member_size);
  bse_source_access_modules (source, op_access_update, adata, g_free, trans);
}

static void
bse_source_real_add_input (BseSource *source,
			   guint      ichannel,
			   BseSource *osource,
			   guint      ochannel)
{
  BseSourceInput *input = BSE_SOURCE_INPUT (source, ichannel);
  guint j = 0;

  if (BSE_SOURCE_IS_JOINT_ICHANNEL (source, ichannel))
    {
      j = input->jdata.n_joints++;
      input->jdata.joints = g_renew (BseSourceOutput, input->jdata.joints, input->jdata.n_joints);
      input->jdata.joints[j].osource = osource;
      input->jdata.joints[j].ochannel = ochannel;
    }
  else
    {
      g_return_if_fail (input->idata.osource == NULL);

      input->idata.osource = osource;
      input->idata.ochannel = ochannel;
    }
  osource->outputs = g_slist_prepend (osource->outputs, source);

  if (BSE_SOURCE_PREPARED (source) && BSE_SOURCE_N_CONTEXTS (source))
    {
      GslTrans *trans = gsl_trans_open ();
      guint c;
      
      for (c = 0; c < BSE_SOURCE_N_CONTEXTS (source); c++)
	{
	  BseSourceContext *context = context_nth (source, c);
	  
	  bse_source_context_connect_ichannel (source, context, ichannel, trans, j);
	}
      gsl_trans_commit (trans);
    }
}

static gint
check_jchannel_connection (BseSource *source,
			   guint      ichannel,
			   BseSource *osource,
			   guint      ochannel)
{
  BseSourceInput *input = BSE_SOURCE_INPUT (source, ichannel);

  if (BSE_SOURCE_IS_JOINT_ICHANNEL (source, ichannel))
    {
      guint j;

      for (j = 0; j < input->jdata.n_joints; j++)
	if (input->jdata.joints[j].osource == osource &&
	    input->jdata.joints[j].ochannel == ochannel)
	  break;
      return j < input->jdata.n_joints ? j : -1;
    }
  else
    return ochannel == input->idata.ochannel && osource == input->idata.osource ? 0 : -1;
}

BseErrorType
bse_source_set_input (BseSource *source,
		      guint      ichannel,
		      BseSource *osource,
		      guint      ochannel)
{
  g_return_val_if_fail (BSE_IS_SOURCE (source), BSE_ERROR_INTERNAL);
  g_return_val_if_fail (BSE_IS_SOURCE (osource), BSE_ERROR_INTERNAL);
  g_return_val_if_fail (BSE_ITEM (source)->parent == BSE_ITEM (osource)->parent, BSE_ERROR_INTERNAL);
  if (BSE_SOURCE_PREPARED (source))	/* FIXME: check context sets */
    {
      g_return_val_if_fail (BSE_SOURCE_PREPARED (osource), BSE_ERROR_INTERNAL); /* paranoid, checked parent already */
      g_return_val_if_fail (BSE_SOURCE_N_CONTEXTS (source) == BSE_SOURCE_N_CONTEXTS (osource), BSE_ERROR_INTERNAL);
    }
  else
    g_return_val_if_fail (!BSE_SOURCE_PREPARED (osource), BSE_ERROR_INTERNAL);

  if (ichannel >= BSE_SOURCE_N_ICHANNELS (source))
    return BSE_ERROR_SOURCE_NO_SUCH_ICHANNEL;
  if (ochannel >= BSE_SOURCE_N_OCHANNELS (osource))
    return BSE_ERROR_SOURCE_NO_SUCH_OCHANNEL;
  if (BSE_SOURCE_IS_JOINT_ICHANNEL (source, ichannel))
    {
      if (check_jchannel_connection (source, ichannel, osource, ochannel) >= 0)
	return BSE_ERROR_SOURCE_CHANNELS_CONNECTED;
    }
  else if (BSE_SOURCE_INPUT (source, ichannel)->idata.osource)
    return BSE_ERROR_SOURCE_ICHANNEL_IN_USE;

  g_object_ref (source);
  g_object_ref (osource);
  BSE_SOURCE_GET_CLASS (source)->add_input (source, ichannel, osource, ochannel);
  g_signal_emit (source, source_signals[SIGNAL_IO_CHANGED], 0);
  g_signal_emit (osource, source_signals[SIGNAL_IO_CHANGED], 0);
  g_object_unref (source);
  g_object_unref (osource);
  
  return BSE_ERROR_NONE;
}

static void
bse_source_real_remove_input (BseSource *source,
			      guint      ichannel,
			      BseSource *osource,
			      guint      ochannel)
{
  BseSourceInput *input = BSE_SOURCE_INPUT (source, ichannel);
  GslTrans *trans = NULL;
  gint j = -1;

  if (BSE_SOURCE_IS_JOINT_ICHANNEL (source, ichannel))
    {
      j = check_jchannel_connection (source, ichannel, osource, ochannel);
      g_return_if_fail (j >= 0);
    }
  else
    g_return_if_fail (osource == BSE_SOURCE_INPUT (source, ichannel)->idata.osource);

  if (BSE_SOURCE_PREPARED (source) && BSE_SOURCE_N_CONTEXTS (source))
    {
      guint c;
      trans = gsl_trans_open ();
      for (c = 0; c < BSE_SOURCE_N_CONTEXTS (source); c++)
	{
	  BseSourceContext *context = context_nth (source, c);
	  if (BSE_SOURCE_IS_JOINT_ICHANNEL (source, ichannel))
	    {
	      BseSourceContext *src_context = context_nth (osource, c);
	      gsl_trans_add (trans, gsl_job_jdisconnect (context->u.mods.imodule,
							 BSE_SOURCE_ICHANNEL_JSTREAM (source, ichannel),
							 src_context->u.mods.omodule,
							 BSE_SOURCE_OCHANNEL_OSTREAM (osource, ochannel)));
	    }
	  else
	    {
	      for (c = 0; c < BSE_SOURCE_N_CONTEXTS (source); c++)
		{
		  BseSourceContext *context = context_nth (source, c);
		  
		  gsl_trans_add (trans, gsl_job_disconnect (context->u.mods.imodule,
							    BSE_SOURCE_ICHANNEL_ISTREAM (source, ichannel)));
		}
	    }
	}
    }

  if (BSE_SOURCE_IS_JOINT_ICHANNEL (source, ichannel))
    {
      guint k = --input->jdata.n_joints;

      input->jdata.joints[j].osource = input->jdata.joints[k].osource;
      input->jdata.joints[j].ochannel = input->jdata.joints[k].ochannel;
    }
  else
    {
      input->idata.osource = NULL;
      input->idata.ochannel = 0;
    }
  osource->outputs = g_slist_remove (osource->outputs, source);

  if (trans)
    gsl_trans_commit (trans);
}

BseErrorType
bse_source_unset_input (BseSource *source,
			guint      ichannel,
			BseSource *osource,
			guint      ochannel)
{
  g_return_val_if_fail (BSE_IS_SOURCE (source), BSE_ERROR_INTERNAL);
  g_return_val_if_fail (BSE_IS_SOURCE (osource), BSE_ERROR_INTERNAL);
  g_return_val_if_fail (BSE_ITEM (source)->parent == BSE_ITEM (osource)->parent, BSE_ERROR_INTERNAL);
  if (BSE_SOURCE_PREPARED (source))     /* FIXME: check context sets */
    {
      g_return_val_if_fail (BSE_SOURCE_PREPARED (osource), BSE_ERROR_INTERNAL);	/* paranoid, checked parent already */
      g_return_val_if_fail (BSE_SOURCE_N_CONTEXTS (source) == BSE_SOURCE_N_CONTEXTS (osource), BSE_ERROR_INTERNAL);
    }
  else
    g_return_val_if_fail (!BSE_SOURCE_PREPARED (osource), BSE_ERROR_INTERNAL);

  if (ichannel >= BSE_SOURCE_N_ICHANNELS (source))
    return BSE_ERROR_SOURCE_NO_SUCH_ICHANNEL;
  if (ochannel >= BSE_SOURCE_N_OCHANNELS (osource))
    return BSE_ERROR_SOURCE_NO_SUCH_OCHANNEL;
  if (check_jchannel_connection (source, ichannel, osource, ochannel) < 0)
    return BSE_ERROR_SOURCE_NO_SUCH_CONNECTION;

  g_object_ref (source);
  g_object_ref (osource);
  BSE_SOURCE_GET_CLASS (source)->remove_input (source, ichannel, osource, ochannel);
  g_signal_emit (source, source_signals[SIGNAL_IO_CHANGED], 0);
  g_signal_emit (osource, source_signals[SIGNAL_IO_CHANGED], 0);
  g_object_unref (osource);
  g_object_unref (source);

  return BSE_ERROR_NONE;
}

static SfiRing*
add_inputs_recurse (SfiRing   *ring,
		    BseSource *source)
{
  guint i, j;

  for (i = 0; i < BSE_SOURCE_N_ICHANNELS (source); i++)
    {
      BseSourceInput *input = BSE_SOURCE_INPUT (source, i);

      if (BSE_SOURCE_IS_JOINT_ICHANNEL (source, i))
	for (j = 0; j < input->jdata.n_joints; j++)
	  {
	    BseSource *isource = input->jdata.joints[j].osource;
	    if (!BSE_SOURCE_COLLECTED (isource))
	      {
		BSE_OBJECT_SET_FLAGS (isource, BSE_SOURCE_FLAG_COLLECTED);
		ring = sfi_ring_append (ring, isource);
	      }
	  }
      else if (input->idata.osource)
	{
	  BseSource *isource = input->idata.osource;
	  if (!BSE_SOURCE_COLLECTED (isource))
	    {
	      BSE_OBJECT_SET_FLAGS (isource, BSE_SOURCE_FLAG_COLLECTED);
	      ring = sfi_ring_append (ring, isource);
	    }
	}
    }
  return ring;
}

SfiRing*
bse_source_collect_inputs_recursive (BseSource *source)
{
  SfiRing *node, *ring = NULL;

  g_return_val_if_fail (BSE_IS_SOURCE (source), NULL);

  ring = add_inputs_recurse (ring, source);
  for (node = ring; node; node = sfi_ring_walk (node, ring))
    ring = add_inputs_recurse (ring, node->data);
  return ring;
}

void
bse_source_free_collection (SfiRing *ring)
{
  SfiRing *node;

  for (node = ring; node; node = sfi_ring_walk (node, ring))
    {
      BseSource *source = BSE_SOURCE (node->data);
      BSE_OBJECT_UNSET_FLAGS (source, BSE_SOURCE_FLAG_COLLECTED);
    }
  sfi_ring_free (ring);
}

void
bse_source_clear_ichannels (BseSource *source)
{
  gboolean io_changed = FALSE;
  guint i;

  g_return_if_fail (BSE_IS_SOURCE (source));

  g_object_ref (source);
  for (i = 0; i < BSE_SOURCE_N_ICHANNELS (source); i++)
    {
      BseSourceInput *input = BSE_SOURCE_INPUT (source, i);
      BseSource *osource;

      if (BSE_SOURCE_IS_JOINT_ICHANNEL (source, i))
	{
	  guint ochannel;

	  while (input->jdata.n_joints)
	    {
	      osource = input->jdata.joints[0].osource;
	      ochannel = input->jdata.joints[0].ochannel;

	      io_changed = TRUE;
	      g_object_ref (osource);
	      BSE_SOURCE_GET_CLASS (source)->remove_input (source, i, osource, ochannel);
	      g_signal_emit (osource, source_signals[SIGNAL_IO_CHANGED], 0);
	      g_object_unref (osource);
	    }
	}
      else if (input->idata.osource)
	{
	  osource = input->idata.osource;

	  io_changed = TRUE;
	  g_object_ref (osource);
	  BSE_SOURCE_GET_CLASS (source)->remove_input (source, i, osource, input->idata.ochannel);
	  g_signal_emit (osource, source_signals[SIGNAL_IO_CHANGED], 0);
	  g_object_unref (osource);
	}
    }
  if (io_changed)
    g_signal_emit (source, source_signals[SIGNAL_IO_CHANGED], 0);
  g_object_unref (source);
}

void
bse_source_clear_ochannels (BseSource *source)
{
  gboolean io_changed = FALSE;
  
  g_return_if_fail (BSE_IS_SOURCE (source));

  g_object_ref (source);
  while (source->outputs)
    {
      BseSource *isource = source->outputs->data;
      guint i;
      
      g_object_ref (isource);
      for (i = 0; i < BSE_SOURCE_N_ICHANNELS (isource); i++)
	{
	  BseSourceInput *input = BSE_SOURCE_INPUT (isource, i);

	  if (BSE_SOURCE_IS_JOINT_ICHANNEL (isource, i))
	    {
	      guint j;

	      for (j = 0; j < input->jdata.n_joints; j++)
		if (input->jdata.joints[j].osource == source)
		  break;
	      if (j < input->jdata.n_joints)
		{
		  io_changed = TRUE;
		  BSE_SOURCE_GET_CLASS (isource)->remove_input (isource, i,
								source, input->jdata.joints[j].ochannel);
		  g_signal_emit (isource, source_signals[SIGNAL_IO_CHANGED], 0);
		  break;
		}
	    }
	  else if (input->idata.osource == source)
	    {
	      io_changed = TRUE;
	      BSE_SOURCE_GET_CLASS (isource)->remove_input (isource, i,
							    source, input->idata.ochannel);
	      g_signal_emit (isource, source_signals[SIGNAL_IO_CHANGED], 0);
	      break;
	    }
	}
      g_object_unref (isource);
    }
  if (io_changed)
    g_signal_emit (source, source_signals[SIGNAL_IO_CHANGED], 0);
  g_object_unref (source);
}

static void
bse_source_real_store_private (BseObject  *object,
			       BseStorage *storage)
{
  BseSource *source = BSE_SOURCE (object);
  guint i, j;

  /* chain parent class' handler */
  if (BSE_OBJECT_CLASS (parent_class)->store_private)
    BSE_OBJECT_CLASS (parent_class)->store_private (object, storage);
  
  for (i = 0; i < BSE_SOURCE_N_ICHANNELS (source); i++)
    {
      BseSourceInput *input = BSE_SOURCE_INPUT (source, i);
      GSList *slist, *outputs = NULL;
      
      if (BSE_SOURCE_IS_JOINT_ICHANNEL (source, i))
	for (j = 0; j < input->jdata.n_joints; j++)
	  outputs = g_slist_append (outputs, input->jdata.joints + j);
      else if (input->idata.osource)
	outputs = g_slist_append (outputs, &input->idata);
      
      for (slist = outputs; slist; slist = slist->next)
	{
	  BseSourceOutput *output = slist->data;
	  
	  bse_storage_break (storage);
	  bse_storage_printf (storage,
			      "(source-input \"%s\" ",
			      BSE_SOURCE_ICHANNEL_CNAME (source, i));
	  bse_storage_put_item_link (storage, BSE_ITEM (source), BSE_ITEM (output->osource));
	  bse_storage_printf (storage, " \"%s\")", BSE_SOURCE_OCHANNEL_CNAME (output->osource, output->ochannel));
	}
      g_slist_free (outputs);
    }
}

typedef struct _DeferredInput DeferredInput;
struct _DeferredInput
{
  DeferredInput *next;
  gchar		*ichannel_name;
  gchar         *osource_path;
  gchar         *ochannel_name;
};


static void
resolve_osource_input (gpointer     data,
		       BseStorage  *storage,
		       BseItem     *from_item,
		       BseItem     *to_item,
		       const gchar *error)
{
  DeferredInput *dinput = data;
  BseSource *source = BSE_SOURCE (from_item);
  BseSource *osource = to_item ? BSE_SOURCE (to_item) : NULL;

  if (!osource && dinput->osource_path)		/* deprecated compat hack */
    osource = (BseSource*) bse_container_resolve_upath ((BseContainer*) bse_item_get_project (from_item), dinput->osource_path);

  if (error)
    bse_storage_warn (storage,
		      "failed to connect input \"%s\" of `%s' to output \"%s\" of `%s': %s",
		      dinput->ichannel_name ? dinput->ichannel_name : "???",
		      BSE_OBJECT_UNAME (source),
		      dinput->ochannel_name ? dinput->ochannel_name : "???",
		      osource ? BSE_OBJECT_UNAME (osource) : "???",
		      error);
  else
    {
      BseErrorType cerror;

      if (!osource)
	cerror = BSE_ERROR_NOT_FOUND;
      else if (!dinput->ichannel_name)
	cerror = BSE_ERROR_SOURCE_NO_SUCH_ICHANNEL;
      else if (!dinput->ochannel_name)
	cerror = BSE_ERROR_SOURCE_NO_SUCH_OCHANNEL;
      else
	cerror = bse_source_set_input (source,
				       bse_source_find_ichannel (source, dinput->ichannel_name),
				       osource,
				       bse_source_find_ochannel (osource, dinput->ochannel_name));
      if (cerror)
	bse_storage_warn (storage,
			  "failed to connect input \"%s\" of `%s' to output \"%s\" of `%s': %s",
			  dinput->ichannel_name ? dinput->ichannel_name : "???",
			  BSE_OBJECT_UNAME (source),
			  dinput->ochannel_name ? dinput->ochannel_name : "???",
			  osource ? BSE_OBJECT_UNAME (osource) : "???",
			  bse_error_blurb (cerror));
    }

  g_free (dinput->ichannel_name);
  g_free (dinput->osource_path);
  g_free (dinput->ochannel_name);
  g_free (dinput);
}

static BseTokenType
bse_source_real_restore_private (BseObject  *object,
				 BseStorage *storage)
{
  BseSource *source = BSE_SOURCE (object);
  GScanner *scanner = storage->scanner;
  GTokenType expected_token = BSE_TOKEN_UNMATCHED;
  DeferredInput *dinput;
  
  /* chain parent class' handler */
  if (BSE_OBJECT_CLASS (parent_class)->restore_private)
    expected_token = BSE_OBJECT_CLASS (parent_class)->restore_private (object, storage);
  
  /* feature source-input keywords */
  if (expected_token != BSE_TOKEN_UNMATCHED ||
      g_scanner_peek_next_token (scanner) != G_TOKEN_IDENTIFIER ||
      !bse_string_equals ("source-input", scanner->next_value.v_identifier))
    return expected_token;
  
  g_scanner_get_next_token (scanner); /* eat "source-input" */

  /* parse ichannel name */
  parse_or_return (scanner, G_TOKEN_STRING);
  dinput = g_new0 (DeferredInput, 1);
  dinput->ichannel_name = g_strdup (scanner->value.v_string);

  /* parse osource upath and queue handler */
  if (g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER)	/* bad, bad, hack */
    {
      dinput->osource_path = g_strdup (scanner->next_value.v_identifier);
      /* error = */ bse_storage_parse_item_link (storage, BSE_ITEM (source), resolve_osource_input, dinput);
      bse_storage_warn (storage, "deprecated syntax: non-string uname path: %s", dinput->osource_path);
    }
  else
    {
      expected_token = bse_storage_parse_item_link (storage, BSE_ITEM (source), resolve_osource_input, dinput);
      if (expected_token != G_TOKEN_NONE)
	return expected_token;
    }

  /* parse ochannel name */
  parse_or_return (scanner, G_TOKEN_STRING);
  peek_or_return (scanner, ')');
  dinput->ochannel_name = g_strdup (scanner->value.v_string);

  /* read closing brace */
  return g_scanner_get_next_token (scanner) == ')' ? G_TOKEN_NONE : ')';
}
