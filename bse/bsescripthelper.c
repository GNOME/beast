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
#include "bsecomwire.h"
#include "bsescriptcontrol.h"
#include <string.h>
#include <stdlib.h>


/* --- prototypes --- */
static void		bse_script_procedure_init	(BseScriptProcedureClass *class,
							 BseScriptData		 *sdata);
static BseErrorType	bse_script_procedure_exec	(BseProcedureClass	 *proc,
							 const GValue		 *in_values,
							 GValue			 *out_values);
static void		bse_script_send_event		(SfiGlueCodec		 *codoex,
							 gpointer		  user_data,
							 const gchar		 *message);
static GValue*		bse_script_check_client_msg	(SfiGlueCodec		 *codec,
							 gpointer		  data,
							 const gchar		 *msg,
							 GValue			 *value,
							 gboolean		 *handled);
static gboolean		bse_script_dispatcher		(gpointer		  data,
							 guint			  request,
							 const gchar		 *request_msg,
							 BseComWire		 *wire);
static GParamSpec*	bse_script_param_spec		(gchar			 *pspec_desc,
							 const gchar		 *script_name,
							 const gchar		 *func_name,
							 gchar			**free1,
							 gchar			**free2);
static void		bse_script_param_stringify	(GString		 *gstring,
							 const GValue		 *value,
							 GParamSpec		 *pspec);


/* --- functions --- */
static GSList*
string_list_copy_deep (GSList *xlist)
{
  GSList *slist, *dlist = NULL;
  for (slist = xlist; slist; slist = slist->next)
    dlist = g_slist_prepend (dlist, g_strdup (slist->data));
  return g_slist_reverse (dlist);
}

static void
string_list_free_deep (GSList *slist)
{
  while (slist)
    {
      GSList *tmp = slist->next;
      g_free (slist->data);
      slist = tmp;
    }
}

static void
bse_script_procedure_init (BseScriptProcedureClass *class,
			   BseScriptData           *sdata)
{
  BseProcedureClass *proc = (BseProcedureClass*) class;
  GSList *slist;
  guint n;

  proc->name = g_type_name (G_TYPE_FROM_CLASS (proc));
  proc->blurb = sdata->blurb;
  proc->help = sdata->help;
  proc->author = sdata->author;
  proc->copyright = sdata->copyright;
  proc->date = sdata->date;
  class->sdata = sdata;
  proc->execute = bse_script_procedure_exec;

  /* we support a limited parameter set for scripts */
  n = g_slist_length (sdata->params);
  proc->in_pspecs = g_new (GParamSpec*, n + 1);
  for (slist = sdata->params; slist; slist = slist->next)
    {
      gchar *f1 = NULL, *f2 = NULL;
      GParamSpec *pspec = bse_script_param_spec (slist->data, sdata->script_file, sdata->name, &f1, &f2);
      g_free (f1);
      g_free (f2);
      if (pspec)
	{
	  proc->in_pspecs[proc->n_in_pspecs++] = pspec;
	  g_param_spec_sink (g_param_spec_ref (pspec));
	}
      else
	g_message ("unable to register parameter for function \"%s\" in script \"%s\" from: %s",
		   sdata->name, sdata->script_file, (gchar*) slist->data);
    }
  proc->in_pspecs[proc->n_in_pspecs] = NULL;
}

GType
bse_script_proc_register (const gchar *script_file,
			  const gchar *name,
			  const gchar *category,
			  const gchar *blurb,
			  const gchar *help,
			  const gchar *author,
			  const gchar *copyright,
			  const gchar *date,
			  GSList      *params)
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
  if (g_slist_length (params) > BSE_PROCEDURE_MAX_IN_PARAMS)
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
  sdata->author = g_strdup (author);
  sdata->copyright = g_strdup (copyright);
  sdata->date = g_strdup (date);
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
  GSList *params = NULL;
  GString *gstring = g_string_new ("");
  SfiGlueCodec *codec;
  BseScriptControl *sctrl;
  BseErrorType error;
  gchar *shellpath;
  guint i;

  for (i = 0; i < proc->n_in_pspecs; i++)
    bse_script_param_stringify (gstring, in_values + i, proc->in_pspecs[i]);

  params = g_slist_prepend (params, g_strdup_printf ("--bse-enable-server"));
  params = g_slist_prepend (params, g_strdup_printf ("(load \"%s\")"
						     "(%s %s)",
						     sdata->script_file,
						     sdata->name,
						     gstring->str));
  g_string_free (gstring, TRUE);
  shellpath = g_strdup_printf ("%s/%s", BSW_PATH_BINARIES, "bswshell");
  codec = sfi_glue_codec_new (bse_glue_context (),
			      bse_script_send_event,
			      bse_script_check_client_msg);
  error = bse_server_run_remote (server, shellpath,
				 bse_script_dispatcher, codec, (GDestroyNotify) sfi_glue_codec_destroy,
				 params, sdata->script_file, proc->name, &sctrl);
  if (sctrl)
    sfi_glue_codec_set_user_data (codec, sctrl, NULL);
  g_free (shellpath);
  string_list_free_deep (params);

  if (error)
    g_message ("failed to start script \"%s::%s\": %s",
	       sdata->script_file, proc->name, bse_error_blurb (error));
  else
    bse_script_control_set_file_name (sctrl, sdata->script_file);
  
  return error;
}

static GValue*
bse_script_check_client_msg (SfiGlueCodec *codec,
			     gpointer      data,
			     const gchar  *msg,
			     GValue       *value,
			     gboolean     *handled)
{
  BseScriptControl *sctrl = codec->user_data;
  GValue *retval = NULL;
  
  if (!msg)
    return retval;
  if (strcmp (msg, "bse-script-register") == 0 && SFI_VALUE_HOLDS_SEQ (value))
    {
      SfiSeq *seq = sfi_value_get_seq (value);

      *handled = TRUE;
      if (!seq || seq->n_elements < 7 || !sfi_seq_check (seq, SFI_TYPE_STRING))
	retval = sfi_value_string ("invalid arguments supplied");
      else
	{
	  GSList *params = NULL;
	  GType type;
	  guint i;

	  for (i = seq->n_elements - 1; i >= 7; i--)
	    params = g_slist_prepend (params, sfi_value_get_string (sfi_seq_get (seq, i)));
	  type = bse_script_proc_register (bse_script_control_get_file_name (sctrl),
					   sfi_value_get_string (sfi_seq_get (seq, 0)),
					   sfi_value_get_string (sfi_seq_get (seq, 1)),
					   sfi_value_get_string (sfi_seq_get (seq, 2)),
					   sfi_value_get_string (sfi_seq_get (seq, 3)),
					   sfi_value_get_string (sfi_seq_get (seq, 4)),
					   sfi_value_get_string (sfi_seq_get (seq, 5)),
					   sfi_value_get_string (sfi_seq_get (seq, 6)),
					   params);
	  g_slist_free (params);
	}
    }
  return retval;
}

static gboolean
bse_script_dispatcher (gpointer        data,
		       guint           request,
		       const gchar    *request_msg,
		       BseComWire     *wire)
{
  SfiGlueCodec *codec = data;
  BseScriptControl *sctrl = codec->user_data;
  gchar *result;

  /* avoid spurious invocations */
  if (!wire->connected)
    return FALSE;

  /* log current wire */
  bse_script_control_push_current (sctrl);

  /* dispatch serialized commands and fetch result.
   */
  result = sfi_glue_codec_process (codec, request_msg);

  /* and send result back through the wire */
  bse_com_wire_send_result (wire, request, result);
  g_free (result);

  /* unlog wire */
  bse_script_control_pop_current ();

  /* we handled this request_msg */
  return TRUE;
}

static void
bse_script_send_event (SfiGlueCodec *codec,
		       gpointer      data,
		       const gchar  *message)
{
  BseScriptControl *sctrl = codec->user_data;
  BseComWire *wire = sctrl->wire;
  guint request_id;

  request_id = bse_com_wire_send_request (wire, message);
  /* one-way message */
  bse_com_wire_forget_request (wire, request_id);
}

GSList*
bse_script_dir_list_files (const gchar *dir_list)
{
  GSList *slist = bse_search_path_list_files (dir_list, "*.scm", NULL, G_FILE_TEST_IS_REGULAR);

  return g_slist_sort (slist, (GCompareFunc) strcmp);
}

BseErrorType
bse_script_file_register (const gchar *file_name)
{
  BseServer *server = bse_server_get ();
  GSList *params = NULL;
  gchar *shellpath, *proc_name = "registration hook";
  BseScriptControl *sctrl;
  SfiGlueCodec *codec;
  BseErrorType error;

  params = g_slist_prepend (params, g_strdup_printf ("--bse-enable-register"));
  params = g_slist_prepend (params, g_strdup_printf ("(load \"%s\")", file_name));
  shellpath = g_strdup_printf ("%s/%s", BSW_PATH_BINARIES, "bswshell");
  codec = sfi_glue_codec_new (bse_glue_context (),
			      bse_script_send_event,
			      bse_script_check_client_msg);
  error = bse_server_run_remote (server, shellpath,
				 bse_script_dispatcher, codec, (GDestroyNotify) sfi_glue_codec_destroy,
				 params, file_name, proc_name, &sctrl);
  if (sctrl)
    sfi_glue_codec_set_user_data (codec, sctrl, NULL);
  g_free (shellpath);
  string_list_free_deep (params);

  if (!error)
    bse_script_control_set_file_name (sctrl, file_name);
  
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
    return sfi_param_spec_string (cname, nick, blurb, dflt, PARAM_HINTS);
  else if (strcmp (pspec_desc, "BseParamBool") == 0)	/* "BseParamBool:Mark-me:0" */
    return sfi_param_spec_bool (cname, nick, blurb, strtol (dflt, NULL, 10), PARAM_HINTS);
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
      return sfi_param_spec_int (cname, nick, blurb, val, min, max, step, PARAM_HINTS);
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
      return sfi_param_spec_real (cname, nick, blurb, val, min, max, step, PARAM_HINTS);
    }
  else if (strcmp (pspec_desc, "BseNote") == 0)		/* "BseNote:Note:C-2" */
    {
      gint dfnote = bse_note_from_string (dflt);
      if (dfnote == BSE_NOTE_UNPARSABLE)
	dfnote = BSE_NOTE_VOID;
      return sfi_param_spec_note (cname, nick, blurb, dfnote, PARAM_HINTS);
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

static void
bse_script_param_stringify (GString      *gstring,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  switch (G_TYPE_FUNDAMENTAL (G_VALUE_TYPE (value)))
    {
      gchar *str;
      GObject *obj;
    case G_TYPE_STRING:
      str = sfi_value_get_string (value);
      str = g_strescape (str ? str : "", NULL);
      g_string_printfa (gstring, "\"%s\"", str);
      g_free (str);
      break;
    case G_TYPE_BOOLEAN:
      g_string_printfa (gstring, "#%c", sfi_value_get_bool (value) ? 't' : 'f');
      break;
    case G_TYPE_INT:
      g_string_printfa (gstring, "%d", sfi_value_get_int (value));
      break;
    case G_TYPE_FLOAT:
      g_string_printfa (gstring, "%.17g", sfi_value_get_real (value));
      break;
    case G_TYPE_OBJECT:
      obj = bse_value_get_object (value);
      g_string_printfa (gstring, "%u", BSE_IS_ITEM (obj) ? BSE_OBJECT_ID (obj) : 0);
      break;
    default:
      g_assert_not_reached ();
    }
  g_string_append (gstring, " ");
}
