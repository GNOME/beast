/* BSW-SCM - Bedevilled Sound Engine Scheme Wrapper
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
#define G_LOG_DOMAIN "BswShell"

#include <string.h>
#include <errno.h>
#include "bswscminterp.h"
#include <bse/bsecomwire.h>
#include <bse/bseglue.h>

/* Data types:
 * SCM
 * Constants:
 * SCM_BOOL_T, SCM_BOOL_F
 * Object:
 * SCM_UNSPECIFIED
 * Checks:
 * SCM_IMP()	- is immediate?
 * SCM_NIMP()	- is not immediate?
 *
 * catching exceptions:
 * typedef SCM (*scm_catch_body_t) (void *data);
 * typedef SCM (*scm_catch_handler_t) (void *data,
 * SCM tag = SCM_BOOL_T; means catch-all
 * SCM gh_catch(SCM tag, scm_catch_body_t body, void *body_data,
 *              scm_catch_handler_t handler, void *handler_data);
 */

#define	BSE_SCM_NIL	SCM_UNSPECIFIED
#define	BSE_SCM_NILP(x)	((x) == SCM_UNSPECIFIED)


/* --- prototypes --- */
static SCM	bse_scm_from_value		(const GValue	*value);



/* --- SCM GC hooks --- */
typedef void (*BswScmFreeFunc) ();
typedef struct {
  gpointer       data;
  BswScmFreeFunc free_func;
  gsize          size_hint;
} BswScmGCCell;
static gulong tc_gc_cell = 0;
static void
bsw_scm_enter_gc (SCM           *scm_gc_list,
		  gpointer       data,
		  BswScmFreeFunc free_func,
		  gsize          size_hint)
{
  BswScmGCCell *gc_cell;
  SCM s_cell = 0;

  g_return_if_fail (scm_gc_list != NULL);
  g_return_if_fail (free_func != NULL);

  // g_printerr ("GCCell allocating %u bytes (%p).\n", size_hint, free_func);

  gc_cell = g_new (BswScmGCCell, 1);
  gc_cell->data = data;
  gc_cell->free_func = free_func;
  gc_cell->size_hint = size_hint + sizeof (BswScmGCCell);

  SCM_NEWSMOB (s_cell, tc_gc_cell, gc_cell);
  *scm_gc_list = gh_cons (s_cell, *scm_gc_list);
  scm_done_malloc (gc_cell->size_hint);
}

static SCM
bsw_scm_mark_gc_cell (SCM scm_gc_cell)
{
  // BswScmGCCell *gc_cell = (BswScmGCCell*) SCM_CDR (scm_gc_cell);

  // g_printerr ("GCCell mark %u bytes (%p).\n", gc_cell->size_hint, gc_cell->free_func);

  /* scm_gc_mark (gc_cell->something); */

  return SCM_BOOL_F;
}

static scm_sizet
bsw_scm_free_gc_cell (SCM scm_gc_cell)
{
  BswScmGCCell *gc_cell = (BswScmGCCell*) SCM_CDR (scm_gc_cell);
  scm_sizet size = gc_cell->size_hint;

  // g_printerr ("GCCell freeing %u bytes (%p).\n", size, gc_cell->free_func);

  gc_cell->free_func (gc_cell->data);
  g_free (gc_cell);

  return size;
}


/* --- SCM Glue GC Plateau --- */
static gulong tc_glue_gc_plateau = 0;
static guint  scm_glue_gc_plateau_blocker = 0;
typedef struct {
  guint    size_hint;
  gboolean active_plateau;
} GcPlateau;

SCM
bsw_scm_make_gc_plateau (guint size_hint)
{
  SCM s_gcplateau = SCM_UNSPECIFIED;
  GcPlateau *gp = g_new (GcPlateau, 1);

  scm_glue_gc_plateau_blocker++;
  gp->size_hint = size_hint;
  gp->active_plateau = TRUE;
  SCM_NEWSMOB (s_gcplateau, tc_glue_gc_plateau, gp);
  scm_done_malloc (gp->size_hint);
  return s_gcplateau;
}

void
bsw_scm_destroy_gc_plateau (SCM s_gcplateau)
{
  GcPlateau *gp;

  g_assert (SCM_NIMP (s_gcplateau) && SCM_CAR (s_gcplateau) == tc_glue_gc_plateau);

  gp = (GcPlateau*) SCM_CDR (s_gcplateau);
  if (gp->active_plateau)
    {
      gp->active_plateau = FALSE;
      g_assert (scm_glue_gc_plateau_blocker > 0);
      scm_glue_gc_plateau_blocker--;
      if (scm_glue_gc_plateau_blocker == 0)
	sfi_glue_gc_run ();
    }
}

static scm_sizet
bsw_scm_gc_plateau_free (SCM s_gcplateau)
{
  GcPlateau *gp = (GcPlateau*) SCM_CDR (s_gcplateau);
  guint size_hint = gp->size_hint;

  bsw_scm_destroy_gc_plateau (s_gcplateau);
  g_free (gp);
  
  return size_hint;
}


/* --- SCM Glue Record --- */
static gulong tc_glue_rec = 0;
static SCM
bsw_scm_make_glue_rec (SfiRec *rec)
{
  SCM s_rec = 0;

  g_return_val_if_fail (rec != NULL, SCM_UNSPECIFIED);

  sfi_rec_ref (rec);
  SCM_NEWSMOB (s_rec, tc_glue_rec, rec);
  return s_rec;
}

static scm_sizet
bsw_scm_free_glue_rec (SCM scm_rec)
{
  SfiRec *rec = (SfiRec*) SCM_CDR (scm_rec);

  sfi_rec_unref (rec);
  return 0;
}

SCM
bsw_scm_glue_rec_print (SCM scm_rec)
{
  SCM port = scm_current_output_port ();
  SfiRec *rec;
  guint i;

  SCM_ASSERT ((SCM_NIMP (scm_rec) && SCM_CAR (scm_rec) == tc_glue_rec), scm_rec, SCM_ARG1, "bsw-record-print");

  rec = (SfiRec*) SCM_CDR (scm_rec);
  scm_puts ("'(", port);
  for (i = 0; i < rec->n_fields; i++)
    {
      const gchar *name = rec->field_names[i];
      GValue *value = rec->fields + i;

      scm_puts ("(", port);
      scm_puts (name, port);
      scm_puts (" . ", port);
      scm_display (bse_scm_from_value (value), port);
      scm_puts (")", port);
    }
  scm_puts (")", port);

  return SCM_UNSPECIFIED;
}

SCM
bsw_scm_glue_rec_get (SCM scm_rec,
		      SCM s_field)
{
  SCM gcplateau = bsw_scm_make_gc_plateau (1024);
  GValue *val;
  SfiRec *rec;
  gchar *name;
  SCM s_val;

  SCM_ASSERT ((SCM_NIMP (scm_rec) && SCM_CAR (scm_rec) == tc_glue_rec), scm_rec, SCM_ARG1, "bsw-record-get");
  SCM_ASSERT (SCM_SYMBOLP (s_field),  s_field,  SCM_ARG2, "bsw-record-get");

  rec = (SfiRec*) SCM_CDR (scm_rec);
  name = g_strndup (SCM_ROCHARS (s_field), SCM_LENGTH (s_field));
  val = sfi_rec_get (rec, name);
  g_free (name);
  if (val)
    s_val = bse_scm_from_value (val);
  else
    s_val = SCM_UNSPECIFIED;

  bsw_scm_destroy_gc_plateau (gcplateau);
  return s_val;
}


/* --- SCM Glue Proxy --- */
static gulong tc_glue_proxy = 0;
static SCM
bsw_scm_make_glue_proxy (SfiProxy proxy)
{
  SCM s_proxy = 0;

  SCM_NEWSMOB (s_proxy, tc_glue_proxy, proxy);
  return s_proxy;
}


/* --- SCM procedures --- */
static gboolean server_enabled = FALSE;

void
bsw_scm_enable_server (gboolean enabled)
{
  server_enabled = enabled != FALSE;
}

SCM
bsw_scm_server_get (void)
{
  SfiProxy server;
  SCM s_retval;

  BSW_SCM_DEFER_INTS ();
  server = server_enabled ? bsw_proxy_get_server () : 0;
  BSW_SCM_ALLOW_INTS ();
  s_retval = bsw_scm_make_glue_proxy (server);

  return s_retval;
}

static GValue*
bse_value_from_scm (SCM sval)
{
  GValue *value;
  if (SCM_BOOLP (sval))
    value = sfi_value_bool (!SCM_FALSEP (sval));
  else if (SCM_INUMP (sval))
    value = sfi_value_int (scm_num2long (sval, (char*) SCM_ARG1, "bse_value_from_scm"));
  else if (SCM_REALP (sval))
    value = sfi_value_real (scm_num2dbl (sval, "bse_value_from_scm"));
  else if (SCM_BIGP (sval))
    value = sfi_value_num (scm_num2long_long (sval, (char*) SCM_ARG1, "bse_value_from_scm"));
  else if (SCM_SYMBOLP (sval))
    value = sfi_value_lchoice (SCM_ROCHARS (sval), SCM_LENGTH (sval));
  else if (SCM_ROSTRINGP (sval))
    value = sfi_value_lstring (SCM_ROCHARS (sval), SCM_LENGTH (sval));
  else if (SCM_CONSP (sval))
    {
      SfiSeq *seq = sfi_seq_new ();
      SCM node;
      for (node = sval; SCM_CONSP (node); node = SCM_CDR (node))
	{
	  GValue *v = bse_value_from_scm (SCM_CAR (node));
	  sfi_seq_append (seq, v);
	  sfi_value_free (v);
	}
      value = sfi_value_seq (seq);
      sfi_seq_unref (seq);
    }
  else if (SCM_NIMP (sval) && SCM_CAR (sval) == tc_glue_proxy)
    {
      SfiProxy proxy = (SfiProxy) SCM_CDR (sval);
      value = sfi_value_proxy (proxy);
    }
  else if (SCM_NIMP (sval) && SCM_CAR (sval) == tc_glue_rec)
    {
      SfiRec *rec = (SfiRec*) SCM_CDR (sval);
      value = sfi_value_rec (rec);
    }
  else
    value = NULL;
  return value;
}

static SCM
bse_scm_from_value (const GValue *value)
{
  SCM gcplateau = bsw_scm_make_gc_plateau (1024);
  SCM sval = SCM_UNSPECIFIED;
  switch (sfi_categorize_type (G_VALUE_TYPE (value)))
    {
      const gchar *str;
      SfiSeq *seq;
      SfiRec *rec;
    case SFI_SCAT_BOOL:
      sval = sfi_value_get_bool (value) ? SCM_BOOL_T : SCM_BOOL_F;
      break;
    case SFI_SCAT_INT:
      sval = gh_long2scm (sfi_value_get_int (value));
      break;
    case SFI_SCAT_NUM:
      sval = scm_long_long2big (sfi_value_get_num (value));
      break;
    case SFI_SCAT_REAL:
      sval = gh_double2scm (sfi_value_get_real (value));
      break;
    case SFI_SCAT_STRING:
      str = sfi_value_get_string (value);
      sval = str ? gh_str02scm (str) : BSE_SCM_NIL;
      break;
    case SFI_SCAT_CHOICE:
      str = sfi_value_get_choice (value);
      sval = str ? SCM_CAR (scm_intern0 (str)) : BSE_SCM_NIL;
      break;
    case SFI_SCAT_BBLOCK:
      sval = BSE_SCM_NIL;
      g_warning ("FIXME: implement SfiBBlock -> SCM byte vector conversion");
      break;
    case SFI_SCAT_FBLOCK:
      sval = BSE_SCM_NIL;
      g_warning ("FIXME: implement SfiFBlock -> SCM float vector conversion");
      break;
    case SFI_SCAT_PROXY:
      sval = bsw_scm_make_glue_proxy (sfi_value_get_proxy (value));
      break;
    case SFI_SCAT_SEQ:
      seq = sfi_value_get_seq (value);
      sval = SCM_EOL;
      if (seq)
	{
	  guint i = seq->n_elements;
	  while (i--)
	    sval = scm_cons (bse_scm_from_value (seq->elements + i), sval);
	}
      break;
    case SFI_SCAT_REC:
      rec = sfi_value_get_rec (value);
      if (rec)
	sval = bsw_scm_make_glue_rec (rec);
      else
	sval = BSE_SCM_NIL;
      break;
    default:
      g_error ("invalid value type while converting to SCM: %s", g_type_name (G_VALUE_TYPE (value)));
      break;
    }
  bsw_scm_destroy_gc_plateau (gcplateau);
  return sval;
}

SCM
bsw_scm_glue_set_prop (SCM s_proxy,
		       SCM s_prop_name,
		       SCM s_value)
{
  SCM gcplateau = bsw_scm_make_gc_plateau (1024);
  SCM gclist = SCM_EOL;
  SfiProxy proxy;
  gchar *prop_name;
  GValue *value;

  SCM_ASSERT (SCM_IMP (s_proxy),  s_proxy,  SCM_ARG1, "bsw-set-prop");
  SCM_ASSERT (SCM_STRINGP (s_prop_name), s_prop_name, SCM_ARG2, "bsw-set-prop");

  BSW_SCM_DEFER_INTS ();

  proxy = gh_scm2long (s_proxy);
  prop_name = g_strndup (SCM_ROCHARS (s_prop_name), SCM_LENGTH (s_prop_name));
  bsw_scm_enter_gc (&gclist, prop_name, g_free, SCM_LENGTH (s_prop_name));

  value = bse_value_from_scm (s_value);
  if (value)
    {
      sfi_glue_proxy_set_property (proxy, prop_name, value);
      sfi_value_free (value);
    }
  else
    scm_wrong_type_arg ("bse-set-prop", SCM_ARG3, s_value);

  BSW_SCM_ALLOW_INTS ();
  
  bsw_scm_destroy_gc_plateau (gcplateau);
  return SCM_UNSPECIFIED;
}

SCM
bsw_scm_glue_get_prop (SCM s_proxy,
		       SCM s_prop_name)
{
  SCM gcplateau = bsw_scm_make_gc_plateau (1024);
  SCM gclist = SCM_EOL;
  SCM s_retval = SCM_UNSPECIFIED;
  SfiProxy proxy;
  gchar *prop_name;
  const GValue *value;

  SCM_ASSERT (SCM_IMP (s_proxy),  s_proxy,  SCM_ARG1, "bsw-get-prop");
  SCM_ASSERT (SCM_STRINGP (s_prop_name), s_prop_name, SCM_ARG2, "bsw-get-prop");

  BSW_SCM_DEFER_INTS ();

  proxy = gh_scm2long (s_proxy);
  prop_name = g_strndup (SCM_ROCHARS (s_prop_name), SCM_LENGTH (s_prop_name));
  bsw_scm_enter_gc (&gclist, prop_name, g_free, SCM_LENGTH (s_prop_name));

  value = sfi_glue_proxy_get_property (proxy, prop_name);
  if (value)
    {
      s_retval = bse_scm_from_value (value);
      sfi_glue_gc_free_now ((gpointer) value, sfi_value_free);
    }

  BSW_SCM_ALLOW_INTS ();
  
  bsw_scm_destroy_gc_plateau (gcplateau);
  return s_retval;
}

SCM
bsw_scm_glue_call (SCM s_proc_name,
		   SCM s_arg_list)
{
  SCM gcplateau = bsw_scm_make_gc_plateau (4096);
  SCM gclist = SCM_EOL;
  SCM node, s_retval = SCM_UNSPECIFIED;
  gchar *proc_name;
  GValue *value;
  SfiSeq *seq;
  
  SCM_ASSERT (SCM_STRINGP (s_proc_name),  s_proc_name,  SCM_ARG1, "bsw-glue-call");
  SCM_ASSERT (SCM_CONSP (s_arg_list) || s_arg_list == SCM_EOL,  s_arg_list,  SCM_ARG2, "bsw-glue-call");

  BSW_SCM_DEFER_INTS ();
  
  proc_name = g_strndup (SCM_ROCHARS (s_proc_name), SCM_LENGTH (s_proc_name));
  bsw_scm_enter_gc (&gclist, proc_name, g_free, SCM_LENGTH (s_proc_name));

  seq = sfi_seq_new ();
  bsw_scm_enter_gc (&gclist, seq, sfi_seq_unref, 1024);
  for (node = s_arg_list; SCM_CONSP (node); node = SCM_CDR (node))
    {
      SCM arg = SCM_CAR (node);

      value = bse_value_from_scm (arg);
      if (!value)
	break;
      sfi_seq_append (seq, value);
      sfi_value_free (value);
    }

  value = sfi_glue_call_seq (proc_name, seq);
  sfi_seq_clear (seq);
  if (value)
    {
      s_retval = bse_scm_from_value (value);
      sfi_glue_gc_free_now (value, sfi_value_free);
    }

  BSW_SCM_ALLOW_INTS ();

  bsw_scm_destroy_gc_plateau (gcplateau);

  return s_retval;
}

typedef struct {
  gulong proxy;
  gchar *signal;
  SCM s_lambda;
  const SfiSeq *tmp_args;
} SigData;

static void
signal_handler_destroyed (gpointer data)
{
  SigData *sdata = data;

  scm_unprotect_object (sdata->s_lambda);
  sdata->s_lambda = SCM_EOL;
  g_free (sdata->signal);
  g_free (sdata);
}

static SCM
marshal_sproc (void *data)
{
  SigData *sdata = data;
  SCM s_ret, args = SCM_EOL;
  const SfiSeq *seq = sdata->tmp_args;
  guint i;

  sdata->tmp_args = NULL;

  g_return_val_if_fail (seq != NULL && seq->n_elements > 0, SCM_UNSPECIFIED);

  i = seq->n_elements;
  while (i--)
    {
      SCM arg = bse_scm_from_value (seq->elements + i);
      args = gh_cons (arg, args);
    }

  s_ret = scm_apply (sdata->s_lambda, args, SCM_EOL);

  return SCM_UNSPECIFIED;
}

static void
signal_handler (gpointer      sig_data,
		const gchar  *signal,
		const SfiSeq *args)
{
  SCM_STACKITEM stack_item;
  SigData *sdata = sig_data;

  sdata->tmp_args = args;
  scm_internal_cwdr ((scm_catch_body_t) marshal_sproc, sdata,
		     scm_handle_by_message_noexit, "BSW", &stack_item);
}

SCM
bsw_scm_signal_connect (SCM s_proxy,
			SCM s_signal,
			SCM s_lambda)
{
  gulong proxy, id;
  SigData *sdata;
  
  SCM_ASSERT (SCM_IMP (s_proxy), s_proxy,  SCM_ARG1, "bsw-signal-connect");
  SCM_ASSERT (SCM_STRINGP (s_signal), s_signal, SCM_ARG2, "bsw-signal-connect");
  SCM_ASSERT (gh_procedure_p (s_lambda), s_lambda,  SCM_ARG3, "bsw-signal-connect");

  proxy = gh_scm2ulong (s_proxy);

  BSW_SCM_DEFER_INTS ();
  sdata = g_new0 (SigData, 1);
  sdata->proxy = proxy;
  sdata->signal = g_strndup (SCM_ROCHARS (s_signal), SCM_LENGTH (s_signal));
  sdata->s_lambda = s_lambda;
  scm_protect_object (sdata->s_lambda);
  id = 0; // FIXME: id = sfi_glue_signal_connect (proxy, sdata->signal, signal_handler, sdata, signal_handler_destroyed);
  BSW_SCM_ALLOW_INTS ();
  
  return gh_ulong2scm (id);
}

static gboolean script_register_enabled = FALSE;

void
bsw_scm_enable_script_register (gboolean enabled)
{
  script_register_enabled = enabled != FALSE;
}

SCM
bsw_scm_script_register (SCM s_name,
			 SCM s_category,
			 SCM s_blurb,
			 SCM s_help,
			 SCM s_author,
			 SCM s_copyright,
			 SCM s_date,
			 SCM s_params)
{
  SCM gcplateau = bsw_scm_make_gc_plateau (4096);
  SCM node;
  guint i;

  SCM_ASSERT (SCM_SYMBOLP (s_name),      s_name,      SCM_ARG1, "bsw-script-register");
  SCM_ASSERT (SCM_STRINGP (s_category),  s_category,  SCM_ARG2, "bsw-script-register");
  SCM_ASSERT (SCM_STRINGP (s_blurb),     s_blurb,     SCM_ARG3, "bsw-script-register");
  SCM_ASSERT (SCM_STRINGP (s_help),      s_help,      SCM_ARG4, "bsw-script-register");
  SCM_ASSERT (SCM_STRINGP (s_author),    s_author,    SCM_ARG5, "bsw-script-register");
  SCM_ASSERT (SCM_STRINGP (s_copyright), s_copyright, SCM_ARG6, "bsw-script-register");
  SCM_ASSERT (SCM_STRINGP (s_date),      s_date,      SCM_ARG7, "bsw-script-register");
  for (node = s_params, i = 8; SCM_CONSP (node); node = SCM_CDR (node), i++)
    {
      SCM arg = SCM_CAR (node);
      if (!SCM_STRINGP (arg))
	scm_wrong_type_arg ("bsw-script-register", i, arg);
    }

  BSW_SCM_DEFER_INTS ();
  if (script_register_enabled)
    {
      SfiSeq *seq = sfi_seq_new ();
      GValue *val, *rval;

      sfi_seq_append (seq, val = sfi_value_lstring (SCM_ROCHARS (s_name), SCM_LENGTH (s_name)));
      sfi_value_free (val);
      sfi_seq_append (seq, val = sfi_value_lstring (SCM_ROCHARS (s_category), SCM_LENGTH (s_category)));
      sfi_value_free (val);
      sfi_seq_append (seq, val = sfi_value_lstring (SCM_ROCHARS (s_blurb), SCM_LENGTH (s_blurb)));
      sfi_value_free (val);
      sfi_seq_append (seq, val = sfi_value_lstring (SCM_ROCHARS (s_help), SCM_LENGTH (s_help)));
      sfi_value_free (val);
      sfi_seq_append (seq, val = sfi_value_lstring (SCM_ROCHARS (s_author), SCM_LENGTH (s_author)));
      sfi_value_free (val);
      sfi_seq_append (seq, val = sfi_value_lstring (SCM_ROCHARS (s_copyright), SCM_LENGTH (s_copyright)));
      sfi_value_free (val);
      sfi_seq_append (seq, val = sfi_value_lstring (SCM_ROCHARS (s_date), SCM_LENGTH (s_date)));
      sfi_value_free (val);
      
      for (node = s_params; SCM_CONSP (node); node = SCM_CDR (node))
	{
	  SCM arg = SCM_CAR (node);
	  sfi_seq_append (seq, val = sfi_value_lstring (SCM_ROCHARS (arg), SCM_LENGTH (arg)));
	  sfi_value_free (val);
	}

      val = sfi_value_seq (seq);
      rval = sfi_glue_client_msg ("bse-script-register", val);
      sfi_value_free (val);
      if (SFI_VALUE_HOLDS_STRING (rval))
	{
	  gchar *name = g_strndup (SCM_ROCHARS (s_name), SCM_LENGTH (s_name));
	  g_message ("while registering \"%s\": %s", name, sfi_value_get_string (rval));
	  g_free (name);
	}
      sfi_glue_gc_free_now (rval, sfi_value_free);
    }
  BSW_SCM_ALLOW_INTS ();

  bsw_scm_destroy_gc_plateau (gcplateau);
  return SCM_UNSPECIFIED;
}

static BswSCMWire *bse_iteration_wire = NULL;

SCM
bsw_scm_context_pending (void)
{
  gboolean pending;

  BSW_SCM_DEFER_INTS ();
  if (bse_iteration_wire)
    bsw_scm_wire_dispatch_io (bse_iteration_wire, 0);
  pending = FALSE; // FIXME: sfi_glue_context_pending (sfi_glue_fetch_context (G_STRLOC));
  BSW_SCM_ALLOW_INTS ();

  return gh_bool2scm (pending);
}

SCM
bsw_scm_context_iteration (SCM s_may_block)
{
#if 0 // FIXME
  if (sfi_glue_context_pending (sfi_glue_fetch_context (G_STRLOC)))
    sfi_glue_context_dispatch (sfi_glue_fetch_context (G_STRLOC));
  else
#endif
    if (gh_scm2bool (s_may_block))
    {
      if (bse_iteration_wire)
	bsw_scm_wire_dispatch_io (bse_iteration_wire, 1500);
      else
	g_usleep (1500 * 1000);
    }
  return SCM_UNSPECIFIED;
}


/* --- initialization --- */
static gchar*
send_to_wire (gpointer     user_data,
	      const gchar *message)
{
  gchar *response;

  response = bsw_scm_wire_do_request (user_data, message);

  return response;
}

static void
register_types (gchar **types)
{
  SCM gcplateau = bsw_scm_make_gc_plateau (2048);

  while (*types)
    {
      gchar **names = sfi_glue_list_method_names (*types);
      gchar *sname = bsw_type_name_to_sname (*types);
      gchar *s;
      guint i;
      
      if (strncmp (sname, "bsw-", 4) == 0)
	{
	  s = g_strdup_printf ("(define (bsw-is-%s proxy) (bsw-item-check-is-a proxy \"%s\"))",
			       sname + 4, *types);
	  gh_eval_str (s);
	  g_free (s);
	}
      for (i = 0; names[i]; i++)
	{
	  gchar *s = g_strdup_printf ("(define %s-%s (lambda list (bsw-glue-call \"%s+%s\" list)))",
				      sname, names[i], *types, names[i]);
	  gh_eval_str (s);
	  g_free (s);
	}
      g_free (sname);

      names = sfi_glue_iface_children (*types);
      register_types (names);

      types++;
    }
  bsw_scm_destroy_gc_plateau (gcplateau);
}

void
bsw_scm_interp_init (BswSCMWire *wire)
{
  gchar **procs, *procs2[2];
  guint i;

  if (wire)
    {
      g_error ("remote shell not currently supported");
      bse_iteration_wire = wire;
      // sfi_glue_context_push (sfi_glue_codec_context (send_to_wire, wire, NULL));
    }
  else
    sfi_glue_context_push (bse_glue_context ("BswShell"));

  tc_gc_cell = scm_make_smob_type ("BswScmGCCell", 0);
  scm_set_smob_mark (tc_gc_cell, bsw_scm_mark_gc_cell);
  scm_set_smob_free (tc_gc_cell, bsw_scm_free_gc_cell);

  tc_glue_gc_plateau = scm_make_smob_type ("BswScmGcPlateau", 0);
  scm_set_smob_free (tc_glue_gc_plateau, bsw_scm_gc_plateau_free);

  tc_glue_rec = scm_make_smob_type ("BswGlueRec", 0);
  scm_set_smob_free (tc_glue_rec, bsw_scm_free_glue_rec);
  gh_new_procedure ("bsw-record-get", bsw_scm_glue_rec_get, 2, 0, 0);
  gh_new_procedure ("bsw-record-print", bsw_scm_glue_rec_print, 1, 0, 0);

  tc_glue_proxy = scm_make_smob_type ("SfiProxy", 0);

  gh_new_procedure ("bsw-glue-call", bsw_scm_glue_call, 2, 0, 0);
  gh_new_procedure ("bsw-glue-set-prop", bsw_scm_glue_set_prop, 3, 0, 0);
  gh_new_procedure ("bsw-glue-get-prop", bsw_scm_glue_get_prop, 2, 0, 0);

  gh_eval_str ("(define (bsw-is-null proxy) (= proxy 0))");
  
  procs = sfi_glue_list_proc_names ();
  for (i = 0; procs[i]; i++)
    if (strncmp (procs[i], "bse-", 4) == 0)
      {
	gchar *s = g_strdup_printf ("(define bsw-%s (lambda list (bsw-glue-call \"%s\" list)))", procs[i] + 4, procs[i]);
	gh_eval_str (s);
	g_free (s);
      }

  procs2[0] = sfi_glue_base_iface ();
  procs2[1] = NULL;
  register_types (procs2);

  gh_new_procedure0_0 ("bsw-server-get", bsw_scm_server_get);
  gh_new_procedure ("bsw-script-register", bsw_scm_script_register, 7, 0, 1);
  // FIXME: gh_new_procedure ("bsw-enum-match?", bsw_scm_enum_match, 2, 0, 0);
  gh_new_procedure ("bsw-signal-connect", bsw_scm_signal_connect, 3, 0, 0);
  gh_new_procedure ("bsw-context-pending", bsw_scm_context_pending, 0, 0, 0);
  gh_new_procedure ("bsw-context-iteration", bsw_scm_context_iteration, 1, 0, 0);
}


/* --- SCM-Wire --- */
struct _BswSCMWire
{
  BseComWire wire;
};

static gboolean
wire_ispatch (gpointer        data,
	      guint           request,
	      const gchar    *request_msg,
	      BseComWire     *wire)
{
  /* avoid spurious invocations */
  if (!wire->connected)
    return FALSE;

  /* dispatch serialized events */
  // FIXME: sfi_glue_codec_enqueue_event (sfi_glue_fetch_context (G_STRLOC), request_msg);

  /* events don't return results */
  bse_com_wire_discard_request (wire, request);

  /* we handled this request_msg */
  return TRUE;
}

BswSCMWire*
bsw_scm_wire_from_pipe (const gchar *ident,
			gint         remote_input,
			gint         remote_output)
{
  BseComWire *wire = bse_com_wire_from_pipe (ident, remote_input, remote_output);

  bse_com_wire_set_dispatcher (wire, wire_ispatch, NULL, NULL);

  return (BswSCMWire*) wire;
}

gchar*
bsw_scm_wire_do_request (BswSCMWire  *swire,
			 const gchar *request_msg)
{
  BseComWire *wire = (BseComWire*) swire;
  guint request_id;

  g_return_val_if_fail (wire != NULL, NULL);
  g_return_val_if_fail (wire->connected != FALSE, NULL);
  g_return_val_if_fail (request_msg != NULL, NULL);

  request_id = bse_com_wire_send_request (wire, request_msg);
  while (wire->connected)
    {
      gchar *result = bse_com_wire_receive_result (wire, request_id);

      /* have result? then we're done */
      if (result)
	return result;
      /* still need to dispatch incoming requests */
      if (!bse_com_wire_receive_dispatch (wire))
	{
	  /* nothing to dispatch, process I/O
	   */
	  /* block until new data is available */
	  bse_com_wire_select (wire, 1000);
	  /* handle new data if any */
	  bse_com_wire_process_io (wire);
	}
    }

  bsw_scm_wire_died (swire);

  return NULL;  /* never reached */
}

void
bsw_scm_wire_dispatch_io (BswSCMWire *swire,
			  guint       timeout)
{
  BseComWire *wire = (BseComWire*) swire;

  g_return_if_fail (wire != NULL);

  if (!wire->connected)
    bsw_scm_wire_died (swire);

  /* dispatch incoming requests */
  if (!bse_com_wire_receive_dispatch (wire))
    {
      /* nothing to dispatch, process I/O
       */
      /* block until new data is available */
      bse_com_wire_select (wire, timeout);
      /* handle new data if any */
      bse_com_wire_process_io (wire);
    }

  if (!wire->connected)
    bsw_scm_wire_died (swire);
}

void
bsw_scm_wire_died (BswSCMWire *swire)
{
  BseComWire *wire = (BseComWire*) swire;

  bse_com_wire_destroy (wire);
  exit (0);
}
