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
#include "bsescripthelper.h"

#include "../PKG_config.h"
#include "bsecategories.h"
#include "bseserver.h"
#include "bseglue.h"
#include "bsejanitor.h"
#include <string.h>
#include <stdlib.h>


/* --- prototypes --- */
static void		bse_script_procedure_init	(BseScriptProcedureClass *class,
							 BseScriptData		 *sdata);
static BseErrorType	bse_script_procedure_exec	(BseProcedureClass	 *proc,
							 const GValue		 *in_values,
							 GValue			 *out_values);
static GParamSpec*	bse_script_param_spec		(gchar			 *pspec_desc,
							 const gchar		 *script_name,
							 const gchar		 *func_name,
							 gchar			**free1,
							 gchar			**free2);


/* --- variables --- */
static GQuark quark_script_args = 0;


/* --- functions --- */
static SfiRing*
string_list_copy_deep (SfiRing *ring)
{
  return sfi_ring_copy_deep (ring, (SfiRingDataFunc) g_strdup, NULL);
}

static void
string_list_free_deep (SfiRing *ring)
{
  sfi_ring_free_deep (ring, (SfiRingDataFunc) g_free, NULL);
}

static void
bse_script_procedure_init (BseScriptProcedureClass *class,
			   BseScriptData           *sdata)
{
  BseProcedureClass *proc = (BseProcedureClass*) class;
  SfiRing *ring;
  guint n;
  
  proc->name = g_type_name (G_TYPE_FROM_CLASS (proc));
  proc->blurb = sdata->blurb;
  proc->help = sdata->help;
  proc->authors = sdata->authors;
  proc->copyright = sdata->copyright;
  class->sdata = sdata;
  proc->execute = bse_script_procedure_exec;
  
  /* we support a limited parameter set for scripts */
  n = sfi_ring_length (sdata->params);
  proc->in_pspecs = g_new (GParamSpec*, n + 1);
  for (ring = sdata->params; ring; ring = sfi_ring_walk (ring, sdata->params))
    {
      gchar *f1 = NULL, *f2 = NULL;
      GParamSpec *pspec = bse_script_param_spec (ring->data, sdata->script_file, sdata->name, &f1, &f2);
      g_free (f1);
      g_free (f2);
      if (pspec)
	{
	  proc->in_pspecs[proc->n_in_pspecs++] = pspec;
	  g_param_spec_sink (g_param_spec_ref (pspec));
	}
      else
	g_message ("unable to register parameter for function \"%s\" in script \"%s\" from: %s",
		   sdata->name, sdata->script_file, (gchar*) ring->data);
    }
  proc->in_pspecs[proc->n_in_pspecs] = NULL;
}

GType
bse_script_proc_register (const gchar *script_file,
			  const gchar *name,
			  const gchar *category,
			  const gchar *blurb,
			  const gchar *help,
			  const gchar *authors,
			  const gchar *copyright,
			  SfiRing     *params)
{
  GTypeInfo script_info = {
    sizeof (BseScriptProcedureClass),
    
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) bse_script_procedure_init,
    (GClassFinalizeFunc) NULL,
    NULL /* class_data */,
    
    /* non classed type stuff */
    0, 0, NULL,
  };
  BseScriptData *sdata;
  gchar *tname;
  GType type;
  
  g_return_val_if_fail (script_file != NULL, 0);
  g_return_val_if_fail (name != NULL, 0);
  if (sfi_ring_length (params) > BSE_PROCEDURE_MAX_IN_PARAMS)
    {
      g_message ("not registering script \"%s\" which needs more than %u parameters",
		 name, BSE_PROCEDURE_MAX_IN_PARAMS);
      return 0;
    }
  
  sdata = g_new0 (BseScriptData, 1);
  sdata->script_file = g_strdup (script_file);
  sdata->name = g_strdup (name);
  sdata->blurb = g_strdup (blurb);
  sdata->help = g_strdup (help);
  sdata->authors = g_strdup (authors);
  sdata->copyright = g_strdup (copyright);
  sdata->params = string_list_copy_deep (params);
  script_info.class_data = sdata;
  
  tname = g_strconcat ("bse-script-", name, NULL);
  type = g_type_register_static (BSE_TYPE_PROCEDURE, tname, &script_info, 0);
  g_free (tname);
  if (type && category && category[0])
    bse_categories_register (category, type);
  return type;
}

static BseErrorType
bse_script_procedure_exec (BseProcedureClass *proc,
			   const GValue      *in_values,
			   GValue            *out_values)
{
  BseScriptProcedureClass *sproc = (BseScriptProcedureClass*) proc;
  BseScriptData *sdata = sproc->sdata;
  BseServer *server = bse_server_get ();
  SfiRing *params = NULL;
  BseJanitor *janitor;
  BseErrorType error;
  gchar *shellpath;
  guint i;
  
  params = sfi_ring_append (params, g_strdup_printf ("--bse-eval"));
  params = sfi_ring_append (params, g_strdup_printf ("(load \"%s\")"
						     "(apply %s (bse-script-fetch-args))",
						     sdata->script_file,
						     sdata->name));
  shellpath = g_strdup_printf ("%s/%s", BSW_PATH_BINARIES, "bswshell");
  error = bse_server_run_remote (server, shellpath,
				 params, sdata->script_file, proc->name, &janitor);
  g_free (shellpath);
  string_list_free_deep (params);
  
  if (error)
    g_message ("failed to start script \"%s::%s\": %s",
	       sdata->script_file, proc->name, bse_error_blurb (error));
  else
    {
      SfiSeq *seq = sfi_seq_new ();
      for (i = 0; i < proc->n_in_pspecs; i++)
	{
	  GValue *v = bse_value_to_sfi (in_values + i);
	  sfi_seq_append (seq, v);
	  sfi_value_free (v);
	}
      if (!quark_script_args)
	quark_script_args = g_quark_from_static_string ("bse-script-helper-script-args");
      g_object_set_qdata_full (janitor, quark_script_args, sfi_seq_copy_deep (seq), sfi_seq_unref);
      sfi_seq_unref (seq);
    }

  return error;
}

GValue*
bse_script_check_client_msg (SfiGlueDecoder *decoder,
			     BseJanitor     *janitor,
			     const gchar    *message,
			     const GValue   *value)
{
  if (!message)
    return NULL;
  if (strcmp (message, "bse-client-msg-script-register") == 0 && SFI_VALUE_HOLDS_SEQ (value))
    {
      SfiSeq *seq = sfi_value_get_seq (value);
      GValue *retval;

      if (!seq || seq->n_elements < 6 || !sfi_seq_check (seq, SFI_TYPE_STRING))
	retval = sfi_value_string ("invalid arguments supplied");
      else
	{
	  SfiRing *params = NULL;
	  GType type;
	  guint i;
	  
	  for (i = 6; i < seq->n_elements; i++)
	    params = sfi_ring_append (params, sfi_value_get_string (sfi_seq_get (seq, i)));
	  type = bse_script_proc_register (bse_janitor_get_script (janitor),
					   sfi_value_get_string (sfi_seq_get (seq, 0)),
					   sfi_value_get_string (sfi_seq_get (seq, 1)),
					   sfi_value_get_string (sfi_seq_get (seq, 2)),
					   sfi_value_get_string (sfi_seq_get (seq, 3)),
					   sfi_value_get_string (sfi_seq_get (seq, 4)),
					   sfi_value_get_string (sfi_seq_get (seq, 5)),
					   params);
	  sfi_ring_free (params);
	  retval = sfi_value_bool (TRUE);	// success
	}
      return retval;
    }
  else if (strcmp (message, "bse-client-msg-script-args") == 0)
    {
      SfiSeq *seq = g_object_get_qdata (janitor, quark_script_args);
      GValue *rvalue = sfi_value_seq (seq);
      g_object_set_qdata (janitor, quark_script_args, NULL);
      return rvalue;
    }
  return NULL;
}

GSList*
bse_script_dir_list_files (const gchar *dir_list)
{
  GSList *slist = bse_search_path_list_files (dir_list, "*.scm", NULL, G_FILE_TEST_IS_REGULAR);
  
  return g_slist_sort (slist, (GCompareFunc) strcmp);
}

BseErrorType
bse_script_file_register (const gchar *file_name,
			  BseJanitor **janitor_p)
{
  BseServer *server = bse_server_get ();
  SfiRing *params = NULL;
  gchar *shellpath, *proc_name = "registration hook";
  BseErrorType error;
  
  params = sfi_ring_append (params, g_strdup_printf ("--bse-enable-register"));
  params = sfi_ring_append (params, g_strdup_printf ("--bse-eval"));
  params = sfi_ring_append (params, g_strdup_printf ("(load \"%s\")", file_name));
  shellpath = g_strdup_printf ("%s/%s", BSW_PATH_BINARIES, "bswshell");
  *janitor_p = NULL;
  error = bse_server_run_remote (server, shellpath,
				 params, file_name, proc_name, janitor_p);
  g_free (shellpath);
  string_list_free_deep (params);
  
  return error;
}

static gchar*
make_sname (const gchar *string)
{
  gchar *p, *cname = g_strdup (string);
  
  for (p = cname; *p; p++)
    {
      if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z'))
	continue;
      else if (*p >= 'A' && *p <= 'Z')
	*p = *p - 'A' + 'a';
      else
	*p = '-';
    }
  return cname;
}

#define PARAM_HINTS	SFI_PARAM_DEFAULT SFI_PARAM_LAX_VALIDATION

static GParamSpec*
bse_script_param_spec (gchar       *pspec_desc,
		       const gchar *script_name,
		       const gchar *func_name,
		       gchar      **free1,
		       gchar      **free2)
{
  gchar *nick = strchr (pspec_desc, ':');
  gchar *dflt, *cname, *blurb;

  g_print ("pspec-descr: %s\n", pspec_desc);
  
  if (!nick)
    return NULL;
  *nick++ = 0;
  dflt = strchr (nick, ':');
  if (!dflt)
    return NULL;
  *dflt++ = 0;
  cname = make_sname (nick);
  *free1 = cname;
  blurb = g_strdup_printf ("Parameter \"%s\" to function <%s> in script \"%s\"",
			   cname, func_name, script_name);
  *free2 = blurb;
  if (strcmp (pspec_desc, "BseParamString") == 0)	/* "BseParamString:Text:Default" */
    return sfi_pspec_string (cname, nick, blurb, dflt, PARAM_HINTS);
  else if (strcmp (pspec_desc, "BseParamBool") == 0)	/* "BseParamBool:Mark-me:0" */
    return sfi_pspec_bool (cname, nick, blurb, strtol (dflt, NULL, 10), PARAM_HINTS);
  else if (strcmp (pspec_desc, "BseParamIRange") == 0)	/* "BseParamIRange:IntNum:16 -100 100 5" */
    {
      glong val, min, max, step;
      gchar *p;
      val = strtol (dflt, &p, 10);
      min = p ? strtol (p, &p, 10) : -100;
      max = p ? strtol (p, &p, 10) : +100;
      if (max < min)
	{
	  step = min;
	  min = max;
	  max = step;
	}
      step = p ? strtol (p, &p, 10) : (max - min) / 100.0;
      val = CLAMP (val, min, max);
      return sfi_pspec_int (cname, nick, blurb, val, min, max, step, PARAM_HINTS);
    }
  else if (strcmp (pspec_desc, "BseParamFRange") == 0)	/* "BseParamFRange:FloatNum:42 0 1000 10" */
    {
      double val, min, max, step;
      gchar *p;
      val = g_strtod (dflt, &p);
      min = p ? g_strtod (p, &p) : -100;
      max = p ? g_strtod (p, &p) : +100;
      if (max < min)
	{
	  step = min;
	  min = max;
	  max = step;
	}
      step = p ? g_strtod (p, &p) : (max - min) / 100.0;
      val = CLAMP (val, min, max);
      return sfi_pspec_real (cname, nick, blurb, val, min, max, step, PARAM_HINTS);
    }
  else if (strcmp (pspec_desc, "BseNote") == 0)		/* "BseNote:Note:C-2" */
    {
      gint dfnote = bse_note_from_string (dflt);
      if (dfnote == BSE_NOTE_UNPARSABLE)
	dfnote = BSE_NOTE_VOID;
      return bse_pspec_note (cname, nick, blurb, dfnote, PARAM_HINTS);
    }
  else if (strncmp (pspec_desc, "BseParamProxy", 13) == 0)	/* "BseParamProxyBseProject:Project:0" */
    {
      GType type = g_type_from_name (pspec_desc + 13);
      
      if (!g_type_is_a (type, BSE_TYPE_ITEM))
	{
	  g_message ("unknown proxy type: %s", pspec_desc + 13);
	  return NULL;
	}
      else
	return bse_param_spec_object (cname, nick, blurb, type, PARAM_HINTS);
    }
  else
    return NULL;
}
