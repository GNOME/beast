/* SFI - Synthesis Fusion Kit Interface
 * Copyright (C) 2002 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <string.h>
#include "sfiglueproxy.h"
#include "sfiglue.h"
#include "sfiustore.h"
#include "sfivcall.h"
#include "sfilog.h"
#include <gobject/gvaluecollector.h>
#include <sfi/gbsearcharray.h>


/* --- macros --- */
#define	sfi_proxy_warn_inval(where,proxy)	sfi_warn ("%s: invalid proxy id (%lu)", (where), (proxy))


/* --- structures --- */
typedef struct {
  GQuark     qsignal;
  GHookList *hlist;
} GlueSignal;
typedef struct {
  SfiProxy       proxy;
  GData         *qdata;
  GBSearchArray *signals;
} Proxy;


/* --- prototypes --- */
static GSList*	_sfi_glue_signal_find_closures	(SfiGlueContext *context,
						 SfiProxy	 proxy,
						 const gchar	*signal,
						 gpointer	 closure_data,
						 gpointer	 search_data);
static Proxy*	fetch_proxy	(SfiGlueContext	*context,
				 SfiProxy	 proxy);
static gint	signals_compare	(gconstpointer	 bsearch_node1, /* key */
				 gconstpointer   bsearch_node2);


/* --- variables --- */
static const GBSearchConfig signals_config = {
  sizeof (GlueSignal),
  signals_compare,
  G_BSEARCH_ARRAY_FORCE_SHRINK,
};
static GQuark quark_weak_refs = 0;


/* --- functions --- */
static gint
signals_compare (gconstpointer bsearch_node1, /* key */
		 gconstpointer bsearch_node2)
{
  const GlueSignal *s1 = bsearch_node1;
  const GlueSignal *s2 = bsearch_node2;
  return s1->qsignal < s2->qsignal ? -1 : s1->qsignal != s2->qsignal;
}

static inline Proxy*
peek_proxy (SfiGlueContext *context,
	    SfiProxy        proxy)
{
  return sfi_ustore_lookup (context->proxies, proxy);
}

static GQuark
signal_quark (const gchar *signal)
{
  gchar *c, *sig = g_strdup (signal);
  GQuark quark;

  /* need to canonify signal name */
  c = strchr (sig, '_');
  while (c)
    {
      *c = '-';
      c = strchr (c, '_');
    }
  quark = g_quark_from_string (sig);
  g_free (sig);
  return quark;
}

static inline GlueSignal*
peek_signal (SfiGlueContext *context,
	     Proxy          *p,
	     GQuark          qsignal)
{
  if (qsignal)
    {
      GlueSignal key;
      key.qsignal = qsignal;
      return g_bsearch_array_lookup (p->signals, &signals_config, &key);
    }
  return NULL;
}

static void
free_hook_list (GHookList *hlist)
{
  g_hook_list_clear (hlist);
  g_free (hlist);
}

static void
delete_signal (SfiGlueContext *context,
	       Proxy	      *p,
	       GQuark          qsignal,
	       gboolean        notify_remote)
{
  GlueSignal *sig = peek_signal (context, p, qsignal);
  guint indx = g_bsearch_array_get_index (p->signals, &signals_config, sig);
  const gchar *signal = g_quark_to_string (sig->qsignal);
  sfi_glue_gc_add (sig->hlist, free_hook_list);
  p->signals = g_bsearch_array_remove (p->signals, &signals_config, indx);
  if (notify_remote)
    context->table.proxy_notify (context, p->proxy, signal, FALSE);
}

static GlueSignal*
fetch_signal (SfiGlueContext *context,
	      Proxy          *p,
	      const gchar    *signal)
{
  GQuark quark = signal_quark (signal);
  GlueSignal key, *sig = NULL;

  key.qsignal = quark;
  sig = g_bsearch_array_lookup (p->signals, &signals_config, &key);
  if (sig)
    return sig;
  if (!context->table.proxy_notify (context, p->proxy, signal, TRUE))
    return NULL;
  key.qsignal = quark;
  key.hlist = g_new0 (GHookList, 1);
  g_hook_list_init (key.hlist, sizeof (GHook));
  p->signals = g_bsearch_array_insert (p->signals, &signals_config, &key);
  return g_bsearch_array_lookup (p->signals, &signals_config, &key);
}

static Proxy*
fetch_proxy (SfiGlueContext *context,
	     SfiProxy        proxy)
{
  Proxy *p = sfi_ustore_lookup (context->proxies, proxy);

  if (!p)
    {
      if (!context->table.proxy_watch_release (context, proxy))
	return NULL;
      if (!quark_weak_refs)	// FIXME: move quark initialization into an init function
	quark_weak_refs = g_quark_from_static_string ("SfiProxy-weak-references");
      p = g_new0 (Proxy, 1);
      p->proxy = proxy;
      g_datalist_init (&p->qdata);
      p->signals = g_bsearch_array_create (&signals_config);
      sfi_ustore_insert (context->proxies, proxy, p);
    }
  return p;
}

static void
destroy_glue_proxy (SfiGlueContext *context,
		    Proxy          *p,
		    gboolean        notify_remote)
{
  Proxy tmp = *p;
  guint i;

  /* early unlink */
  sfi_ustore_remove (context->proxies, p->proxy);
  g_free (p);
  p = &tmp;
  /* delete signals */
  i = g_bsearch_array_get_n_nodes (p->signals);
  while (i--)
    {
      GlueSignal *sig = g_bsearch_array_get_nth (p->signals, &signals_config, i);
      delete_signal (context, p, sig->qsignal, notify_remote);
    }
  g_bsearch_array_free (p->signals, &signals_config);
  g_datalist_id_set_data (&p->qdata, quark_weak_refs, NULL);
  g_datalist_clear (&p->qdata);
}

static gboolean
proxy_foreach_slist (gpointer data,
		     gulong   unique_id,
		     gpointer value)
{
  GSList **slist_p = data;
  *slist_p = g_slist_prepend (*slist_p, (gpointer) unique_id);
  return TRUE;
}

void
_sfi_glue_context_clear_proxies (SfiGlueContext *context)
{
  GSList *plist = NULL;

  g_return_if_fail (context != NULL);

  sfi_ustore_foreach (context->proxies, proxy_foreach_slist, &plist);
  while (plist)
    {
      GSList *slist;
      for (slist = plist; slist; slist = slist->next)
	{
	  Proxy *p = peek_proxy (context, (gulong) slist->data);
	  if (p)
	    destroy_glue_proxy (context, p, FALSE);
	}
      g_slist_free (plist);
      plist = NULL;
      sfi_ustore_foreach (context->proxies, proxy_foreach_slist, &plist);
    }
  sfi_ustore_destroy (context->proxies);
  context->proxies = NULL;
}

static void
sfi_glue_proxy_release (SfiGlueContext *context,
			SfiProxy        proxy)
{
  Proxy *p = peek_proxy (context, proxy);

  g_return_if_fail (proxy != 0);

  if (p)
    destroy_glue_proxy (context, p, TRUE);
  else
    sfi_proxy_warn_inval (G_STRLOC, proxy);
}

static void
sfi_glue_proxy_signal (SfiGlueContext *context,
		       SfiProxy        proxy,
		       const gchar    *signal,
		       SfiSeq         *args)
{
  Proxy *p;

  g_return_if_fail (proxy > 0 && signal);

  p = peek_proxy (context, proxy);
  if (p)
    {
      GlueSignal *sig = peek_signal (context, p, signal_quark (signal));
      if (sig)
	{
	  GHookList *hlist = sig->hlist;
	  GHook *hook;
	  sig = NULL;	/* may mutate during callbacks */
	  hook = g_hook_first_valid (hlist, TRUE);
	  while (hook)
	    {
	      gboolean was_in_call = G_HOOK_IN_CALL (hook);

	      hook->flags |= G_HOOK_FLAG_IN_CALL;
	      g_closure_invoke (hook->data, NULL, args->n_elements, args->elements, (gpointer) signal);
	      if (!was_in_call)
		hook->flags &= ~G_HOOK_FLAG_IN_CALL;
	      hook = g_hook_next_valid (hlist, hook, TRUE);
	    }
	}
      else
	sfi_warn ("spurious unknown signal \"%s\" on proxy (%lu)", signal, proxy);
    }
  else
    sfi_warn ("spurious signal \"%s\" on non existing proxy (%lu)", signal, proxy);
}

static void
default_glue_marshal (GClosure       *closure,
		      GValue /*out*/ *return_value,
		      guint           n_param_values,
		      const GValue   *param_values,
		      gpointer        invocation_hint,
		      gpointer        marshal_data)
{
  gpointer arg0, argN;

  g_return_if_fail (return_value == NULL);
  g_return_if_fail (n_param_values > 0);
  g_return_if_fail (SFI_VALUE_HOLDS_PROXY (param_values));

  arg0 = (gpointer) sfi_value_get_proxy (param_values);
  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      argN = arg0;
      arg0 = closure->data;
    }
  else
    argN = closure->data;
  sfi_vcall_void (((GCClosure*) closure)->callback,
		  arg0,
		  n_param_values - 1,
		  param_values + 1,
		  argN);
}

gulong
sfi_glue_signal_connect_closure (SfiProxy       proxy,
				 const gchar   *signal,
				 GClosure      *closure,
				 gpointer       search_data)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  Proxy *p;

  g_return_val_if_fail (proxy > 0, 0);
  g_return_val_if_fail (signal != NULL, 0);
  g_return_val_if_fail (closure != NULL, 0);

  g_closure_ref (closure);
  g_closure_sink (closure);

  p = fetch_proxy (context, proxy);
  if (!p)
    {
      sfi_proxy_warn_inval (G_STRLOC, proxy);
      sfi_glue_gc_add (closure, g_closure_unref);
    }
  else
    {
      GlueSignal *sig = fetch_signal (context, p, signal);
      if (sig)
	{
	  GHook *hook = g_hook_alloc (sig->hlist);
	  hook->data = closure;
	  hook->destroy = (GDestroyNotify) g_closure_unref;
	  hook->func = search_data;
	  if (G_CLOSURE_NEEDS_MARSHAL (closure))
	    g_closure_set_marshal (closure, default_glue_marshal);
	  sig->hlist->seq_id = context->seq_hook_id;
	  g_hook_append (sig->hlist, hook);
	  context->seq_hook_id = sig->hlist->seq_id;
	  return hook->hook_id;
	}
      else
	{
	  g_message ("failed to connect to signal \"%s\" on proxy (%lu)", signal, proxy);
	  sfi_glue_gc_add (closure, g_closure_unref);
	}
    }
  return 0;
}

gulong
sfi_glue_signal_connect_data (SfiProxy       proxy,
			      const gchar   *signal,
			      gpointer       sig_func,
			      gpointer       sig_data,
			      GClosureNotify sig_data_destroy,
			      GConnectFlags  connect_flags)
{
  GClosure *closure;
  if (connect_flags & G_CONNECT_SWAPPED)
    closure = g_cclosure_new_swap (sig_func, sig_data, sig_data_destroy);
  else
    closure = g_cclosure_new (sig_func, sig_data, sig_data_destroy);
  return sfi_glue_signal_connect_closure (proxy, signal, closure, sig_func);
}

void
sfi_glue_signal_disconnect (SfiProxy     proxy,
			    gulong       connection_id)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  Proxy *p;

  g_return_if_fail (proxy > 0);
  g_return_if_fail (connection_id > 0);

  p = fetch_proxy (context, proxy);
  if (!p)
    {
      sfi_proxy_warn_inval (G_STRLOC, proxy);
      return;
    }
  else
    {
      guint i;
      for (i = 0; i < g_bsearch_array_get_n_nodes (p->signals); i++)
	{
	  GlueSignal *sig = g_bsearch_array_get_nth (p->signals, &signals_config, i);
	  GHookList *hlist = sig->hlist;
	  GQuark qsignal = sig->qsignal;
	  sig = NULL;	/* mutates during callback */
	  if (g_hook_destroy (hlist, connection_id))	/* callback */
	    {
	      GHook *hook = g_hook_first_valid (hlist, TRUE);
	      /* figure whether this was the last valid connection */
	      if (hook)
		g_hook_unref (hlist, hook);
	      else
		delete_signal (context, p, qsignal, TRUE);
	      return;
	    }
	}
    }
  g_message ("proxy (%lu) has no signal connection (%lu) to disconnect",
	     proxy, connection_id);
}

static GSList*
_sfi_glue_signal_find_closures (SfiGlueContext *context,
				SfiProxy        proxy,
				const gchar    *signal,
				gpointer	closure_data,
				gpointer        search_data)
{
  GSList *ids = NULL;
  Proxy *p;

  g_return_val_if_fail (proxy > 0, NULL);
  g_return_val_if_fail (search_data != NULL, NULL);

  p = fetch_proxy (context, proxy);
  if (p && signal)
    {
      GlueSignal *sig = peek_signal (context, p, signal_quark (signal));
      if (sig)
	{
	  GHook *hook = sig->hlist->hooks;
	  while (hook)
	    {
	      if (G_HOOK_IS_VALID (hook) && /* test only non-destroyed hooks */
		  hook->func == search_data &&
		  ((GClosure*) hook->data)->data == closure_data)
		ids = g_slist_prepend (ids, (gpointer) hook->hook_id);
	      hook = hook->next;
	    }
	}
    }
  else if (p)
    {
      guint i;
      for (i = 0; i < g_bsearch_array_get_n_nodes (p->signals); i++)
	{
	  GlueSignal *sig = g_bsearch_array_get_nth (p->signals, &signals_config, i);
	  GHook *hook = sig->hlist->hooks;
	  while (hook)
	    {
	      if (G_HOOK_IS_VALID (hook) && /* test only non-destroyed hooks */
		  hook->func == search_data &&
		  ((GClosure*) hook->data)->data == closure_data)
		ids = g_slist_prepend (ids, (gpointer) hook->hook_id);
	      hook = hook->next;
	    }
	}
    }

  return ids;
}

void
sfi_glue_proxy_connect (SfiProxy     proxy,
			const gchar *signal,
			...)
{
  va_list var_args;

  g_return_if_fail (proxy > 0);

  va_start (var_args, signal);
  while (signal)
    {
      gpointer callback = va_arg (var_args, gpointer);
      gpointer data = va_arg (var_args, gpointer);

      if (strncmp (signal, "signal::", 8) == 0)
	sfi_glue_signal_connect_closure (proxy, signal + 8,
					 g_cclosure_new (callback, data, NULL), callback);
      else if (strncmp (signal, "object_signal::", 15) == 0 ||
	       strncmp (signal, "object-signal::", 15) == 0)
	sfi_glue_signal_connect_closure (proxy, signal + 15,
					 g_cclosure_new_object (callback, data), callback);
      else if (strncmp (signal, "swapped_signal::", 16) == 0 ||
	       strncmp (signal, "swapped-signal::", 16) == 0)
	sfi_glue_signal_connect_closure (proxy, signal + 16,
					 g_cclosure_new_swap (callback, data, NULL), callback);
      else if (strncmp (signal, "swapped_object_signal::", 23) == 0 ||
	       strncmp (signal, "swapped-object-signal::", 23) == 0)
	sfi_glue_signal_connect_closure (proxy, signal + 23,
					 g_cclosure_new_object_swap (callback, data), callback);
      else
	{
	  sfi_warn ("%s: invalid signal spec \"%s\"", G_STRLOC, signal);
	  break;
	}
      signal = va_arg (var_args, gchar*);
    }
  va_end (var_args);
}

void
sfi_glue_proxy_disconnect (SfiProxy     proxy,
			   const gchar *signal,
			   ...)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  va_list var_args;

  g_return_if_fail (proxy > 0);

  va_start (var_args, signal);
  while (signal)
    {
      gpointer callback = va_arg (var_args, gpointer);
      gpointer data = va_arg (var_args, gpointer);
      GSList *node, *slist = NULL;

      if (strncmp (signal, "any_signal::", 12) == 0)
	{
	  signal += 12;
	  slist = _sfi_glue_signal_find_closures (context, proxy, signal, data, callback);
	  for (node = slist; node; node = node->next)
	    sfi_glue_signal_disconnect (proxy, (gulong) node->data);
	  g_slist_free (slist);
	}
      else if (strcmp (signal, "any_signal") == 0)
	{
	  slist = _sfi_glue_signal_find_closures (context, proxy, NULL, data, callback);
	  for (node = slist; node; node = node->next)
	    sfi_glue_signal_disconnect (proxy, (gulong) node->data);
	  g_slist_free (slist);
	}
      else
	{
	  sfi_warn ("%s: invalid signal spec \"%s\"", G_STRLOC, signal);
	  break;
	}

      if (!slist)
	sfi_warn ("%s: signal handler %p(%p) is not connected", G_STRLOC, callback, data);
      signal = va_arg (var_args, gchar*);
    }
  va_end (var_args);
}

gboolean
_sfi_glue_proxy_watch_release (SfiProxy proxy)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);

  g_return_val_if_fail (proxy != 0, FALSE);

  return context->table.proxy_watch_release (context, proxy);
}

gboolean
_sfi_glue_proxy_notify (SfiProxy        proxy,
			const gchar    *signal,
			gboolean        enable_notify)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);

  g_return_val_if_fail (proxy != 0, FALSE);
  g_return_val_if_fail (signal != 0, FALSE);

  return context->table.proxy_notify (context, proxy, signal, enable_notify);
}

gpointer
sfi_glue_proxy_get_qdata (SfiProxy proxy,
			  GQuark   quark)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  Proxy *p = peek_proxy (context, proxy);

  g_return_val_if_fail (proxy != 0, NULL);

  return p && quark ? g_datalist_id_get_data (&p->qdata, quark) : NULL;
}

gpointer
sfi_glue_proxy_steal_qdata (SfiProxy proxy,
			    GQuark   quark)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  Proxy *p = peek_proxy (context, proxy);

  g_return_val_if_fail (proxy != 0, NULL);

  return p && quark ? g_datalist_id_remove_no_notify (&p->qdata, quark) : NULL;
}

void
sfi_glue_proxy_set_qdata_full (SfiProxy       proxy,
			       GQuark         quark,
			       gpointer       data,
			       GDestroyNotify destroy)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  Proxy *p;

  g_return_if_fail (proxy != 0);
  g_return_if_fail (quark != 0);

  p = fetch_proxy (context, proxy);
  if (!p)
    {
      sfi_proxy_warn_inval (G_STRLOC, proxy);
      if (destroy)
	sfi_glue_gc_add (data, destroy);
    }
  else
    g_datalist_id_set_data_full (&p->qdata, quark, data, data ? destroy : NULL);
}

typedef struct {
  SfiProxy proxy;
  guint    n_weak_refs;
  struct {
    SfiProxyDestroy notify;
    gpointer        data;
  } weak_refs[1];  /* flexible array */
} ProxyWeakRefs;

static void
proxy_weak_refs_notify (gpointer data)
{
  ProxyWeakRefs *wstack = data;
  guint i;
  for (i = 0; i < wstack->n_weak_refs; i++)
    wstack->weak_refs[i].notify (wstack->weak_refs[i].data, wstack->proxy);
  g_free (wstack);
}

static void
broken_weak_ref (gpointer *wref)
{
  SfiProxyDestroy weak_notify = wref[0];
  gpointer data = wref[1];
  SfiProxy proxy = (SfiProxy) wref[2];
  weak_notify (data, proxy);
  g_free (wref);
}

void
sfi_glue_proxy_weak_ref (SfiProxy        proxy,
			 SfiProxyDestroy weak_notify,
			 gpointer        data)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  Proxy *p;

  g_return_if_fail (proxy > 0);
  g_return_if_fail (weak_notify != NULL);

  p = fetch_proxy (context, proxy);
  if (!p)
    {
      gpointer *wref = g_new (gpointer, 3);
      sfi_proxy_warn_inval (G_STRLOC, proxy);
      wref[0] = weak_notify;
      wref[1] = data;
      wref[2] = (gpointer) proxy;
      sfi_glue_gc_add (wref, broken_weak_ref);
    }
  else
    {
      ProxyWeakRefs *wstack = g_datalist_id_remove_no_notify (&p->qdata, quark_weak_refs);
      guint i;
      if (wstack)
	{
	  i = wstack->n_weak_refs++;
	  wstack = g_realloc (wstack, sizeof (*wstack) + sizeof (wstack->weak_refs[0]) * i);
	}
      else
	{
	  wstack = g_renew (ProxyWeakRefs, NULL, 1);
	  wstack->proxy = proxy;
	  wstack->n_weak_refs = 1;
	  i = 0;
	}
      wstack->weak_refs[i].notify = weak_notify;
      wstack->weak_refs[i].data = data;
      g_datalist_id_set_data_full (&p->qdata, quark_weak_refs, wstack, proxy_weak_refs_notify);
    }
}

void
sfi_glue_proxy_weak_unref (SfiProxy        proxy,
			   SfiProxyDestroy weak_notify,
			   gpointer        data)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  Proxy *p;
  
  g_return_if_fail (proxy > 0);
  g_return_if_fail (weak_notify != NULL);

  p = fetch_proxy (context, proxy);
  if (!p)
    sfi_proxy_warn_inval (G_STRLOC, proxy);
  else
    {
      ProxyWeakRefs *wstack = g_datalist_id_get_data (&p->qdata, quark_weak_refs);
      gboolean found_one = FALSE;
      if (wstack)
	{
	  guint i;
	  
	  for (i = 0; i < wstack->n_weak_refs; i++)
	    if (wstack->weak_refs[i].notify == weak_notify &&
		wstack->weak_refs[i].data == data)
	      {
		found_one = TRUE;
		wstack->n_weak_refs -= 1;
		if (i != wstack->n_weak_refs)
		  {
		    wstack->weak_refs[i].notify = wstack->weak_refs[wstack->n_weak_refs].notify;
		    wstack->weak_refs[i].data = wstack->weak_refs[wstack->n_weak_refs].data;
		  }
		break;
	      }
	}
      if (!found_one)
	sfi_warn (G_STRLOC ": proxy (%lu) has no weak ref %p(%p)", proxy, weak_notify, data);
    }
}

const gchar*
sfi_glue_proxy_iface (SfiProxy proxy)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  gchar *iface;

  g_return_val_if_fail (proxy != 0, NULL);

  iface = context->table.proxy_iface (context, proxy);

  if (iface)
    sfi_glue_gc_add (iface, g_free);
  return iface;
}

gboolean
sfi_glue_proxy_is_a (SfiProxy     proxy,
		     const gchar *type)
{
  g_return_val_if_fail (type != NULL, FALSE);

  if (proxy)
    {
      SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
      return context->table.proxy_is_a (context, proxy, type);
    }
  else
    return FALSE;
}

GParamSpec*
sfi_glue_proxy_get_pspec (SfiProxy     proxy,
			  const gchar *name)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  GParamSpec *pspec;

  g_return_val_if_fail (proxy != 0, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  pspec = context->table.proxy_get_pspec (context, proxy, name);
  if (pspec)
    sfi_glue_gc_add (pspec, g_param_spec_unref);
  return pspec;
}

SfiSCategory
sfi_glue_proxy_get_pspec_scategory (SfiProxy     proxy,
				    const gchar *name)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);

  g_return_val_if_fail (proxy != 0, 0);
  g_return_val_if_fail (name != NULL, 0);

  return context->table.proxy_get_pspec_scategory (context, proxy, name);
}

const gchar**
sfi_glue_proxy_list_properties (SfiProxy     proxy,
				const gchar *first_ancestor,
				const gchar *last_ancestor,
				guint       *n_props)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  gchar **props;

  g_return_val_if_fail (proxy != 0, NULL);

  if (first_ancestor && !first_ancestor[0])
    first_ancestor = NULL;
  if (last_ancestor && !last_ancestor[0])
    last_ancestor = NULL;

  props = context->table.proxy_list_properties (context, proxy, first_ancestor, last_ancestor);
  if (!props)
    props = g_new0 (gchar*, 1);
  sfi_glue_gc_add (props, g_strfreev);
  if (n_props)
    *n_props = g_strlenv (props);
  return (const gchar**) props;
}

void
sfi_glue_proxy_set_property (SfiProxy      proxy,
			     const gchar  *prop,
			     const GValue *value)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);

  g_return_if_fail (proxy != 0);
  g_return_if_fail (prop != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  context->table.proxy_set_property (context, proxy, prop, value);
}

const GValue*
sfi_glue_proxy_get_property (SfiProxy     proxy,
			     const gchar *prop)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  GValue *value;

  g_return_val_if_fail (proxy != 0, NULL);
  g_return_val_if_fail (prop != NULL, NULL);

  value = context->table.proxy_get_property (context, proxy, prop);
  if (value)
    sfi_glue_gc_add (value, sfi_value_free);
  return value;
}

void
sfi_glue_proxy_set (SfiProxy     proxy,
		    const gchar *prop,
		    ...)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  va_list var_args;

  g_return_if_fail (proxy != 0);

  va_start (var_args, prop);
  while (prop)
    {
      SfiSCategory scat = context->table.proxy_get_pspec_scategory (context, proxy, prop);
      GType vtype = sfi_category_type (scat);
      gchar *error = NULL;
      if (vtype)
	{
	  GValue value = { 0, };
	  g_value_init (&value, vtype);
	  G_VALUE_COLLECT (&value, var_args, G_VALUE_NOCOPY_CONTENTS, &error);
	  if (!error)
	    context->table.proxy_set_property (context, proxy, prop, &value);
	  g_value_unset (&value);
	}
      else
	error = g_strdup_printf ("unknown property \"%s\"", prop);
      if (error)
	{
	  sfi_warn ("%s: %s", G_STRLOC, error);
	  g_free (error);
	  break;
	}
      prop = va_arg (var_args, gchar*);
    }
  va_end (var_args);
}

void
sfi_glue_proxy_get (SfiProxy     proxy,
		    const gchar *prop,
		    ...)
{
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  va_list var_args;

  g_return_if_fail (proxy != 0);

  va_start (var_args, prop);
  while (prop)
    {
      GValue *value = context->table.proxy_get_property (context, proxy, prop);
      gchar *error = NULL;
      if (value)
	{
	  sfi_glue_gc_add (value, sfi_value_free);
	  G_VALUE_LCOPY (value, var_args, G_VALUE_NOCOPY_CONTENTS, &error);
	}
      else
        error = g_strdup_printf ("unknown property \"%s\"", prop);
      if (error)
	{
	  sfi_warn ("%s: %s", G_STRLOC, error);
	  g_free (error);
	  break;
	}
      prop = va_arg (var_args, gchar*);
    }
  va_end (var_args);
}

void
sfi_glue_proxy_dispatch_event (SfiSeq         *event)
{
  static gboolean glue_proxy_dispatching = FALSE;
  SfiGlueContext *context = sfi_glue_fetch_context (G_STRLOC);
  SfiGlueEvent event_type;

  g_return_if_fail (glue_proxy_dispatching == FALSE);

  glue_proxy_dispatching = TRUE;

  event_type = sfi_seq_get_int (event, 0);
  switch (event_type)
    {
      SfiProxy proxy;
      const gchar *signal;
      SfiSeq *args;
    case SFI_GLUE_EVENT_RELEASE:
      proxy = sfi_seq_get_proxy (event, 1);
      if (proxy)
	sfi_glue_proxy_release (context, proxy);
      else
	sfi_warn ("%s: release event with invalid proxy ID (%lu)", G_STRLOC, proxy);
      break;
    case SFI_GLUE_EVENT_SIGNAL:
      signal = sfi_seq_get_string (event, 1);
      args = sfi_seq_get_seq (event, 2);
      proxy = args ? sfi_seq_get_proxy (args, 0) : 0;
      if (proxy && signal && signal[0])
	sfi_glue_proxy_signal (context, proxy, signal, args);
      else if (proxy)
	sfi_warn ("%s: signal event with invalid name \"%s\"", G_STRLOC, signal ? signal : "");
      else
	sfi_warn ("%s: signal event with invalid proxy ID (%lu)", G_STRLOC, proxy);
      break;
    default:
      sfi_warn ("%s: ignoring bogus event (type=%u)", G_STRLOC, event_type);
      break;
    }
  glue_proxy_dispatching = FALSE;
}
