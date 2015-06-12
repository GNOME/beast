// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#include "bsescripthelper.hh"

#include "topconfig.h"
#include "bsecategories.hh"
#include "bsejanitor.hh"
#include "bseserver.hh"
#include "bseglue.hh"
#include "bsemain.hh"
#include <string.h>
#include <stdlib.h>


/* --- prototypes --- */
static void		bse_script_procedure_init	(BseScriptProcedureClass *klass,
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
static void
bse_script_procedure_init (BseScriptProcedureClass *klass,
			   BseScriptData           *sdata)
{
  BseProcedureClass *proc = (BseProcedureClass*) klass;
  SfiRing *ring;
  guint n;

  klass->sdata = sdata;
  proc->execute = bse_script_procedure_exec;

  /* we support a limited parameter set for scripts */
  n = sfi_ring_length (sdata->params);
  proc->in_pspecs = g_new (GParamSpec*, n + 1);
  for (ring = sdata->params; ring; ring = sfi_ring_walk (ring, sdata->params))
    {
      gchar *f1 = NULL, *f2 = NULL;
      GParamSpec *pspec = bse_script_param_spec ((char*) ring->data, sdata->script_file, sdata->name, &f1, &f2);
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
			  const gchar *options,
			  const gchar *category,
			  const gchar *blurb,
                          const gchar *file,
                          guint        line,
			  const gchar *authors,
			  const gchar *license,
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
  sdata->params = sfi_ring_copy_deep (params, (SfiRingDataFunc) g_strdup, NULL);

  script_info.class_data = sdata;

  tname = g_strconcat ("bse-script-", name, NULL);
  type = g_type_register_static (BSE_TYPE_PROCEDURE, tname, &script_info, GTypeFlags (0));
  g_free (tname);
  if (type)
    {
      if (category && category[0])
        bse_categories_register (category, NULL, type, NULL);
      if (options && options[0])
        bse_type_add_options (type, options);
      if (blurb && blurb[0])
        bse_type_add_blurb (type, _(blurb), file, line);
      if (authors && authors[0])
        bse_type_add_authors (type, authors);
      if (license && license[0])
        bse_type_add_license (type, license);
    }
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

  params = sfi_ring_append (params, g_strdup_format ("--bse-eval"));
  params = sfi_ring_append (params, g_strdup_format ("(load \"%s\")"
						     "(apply %s (bse-script-fetch-args))",
						     sdata->script_file,
						     sdata->name));
  shellpath = g_strdup_format ("%s/%s-%s", bse_main_args->path_binaries, "bsescm", BIN_VERSION);
  error = bse_server_run_remote (server, shellpath,
				 params, sdata->script_file, BSE_PROCEDURE_NAME (proc), &janitor);
  g_free (shellpath);
  sfi_ring_free_deep (params, g_free);

  if (error)
    g_message ("failed to start script \"%s::%s\": %s",
	       sdata->script_file, BSE_PROCEDURE_NAME (proc), bse_error_blurb (error));
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
      g_object_set_qdata_full ((GObject*) janitor, quark_script_args, sfi_seq_copy_deep (seq), GDestroyNotify (sfi_seq_unref));
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
      const guint vargs_pos = 8;

      if (!seq || seq->n_elements < vargs_pos || !sfi_seq_check (seq, SFI_TYPE_SFI_STRING))
	retval = sfi_value_string ("invalid arguments supplied");
      else
	{
	  SfiRing *params = NULL;
	  GType type;
	  guint i;

	  for (i = vargs_pos; i < seq->n_elements; i++)
	    params = sfi_ring_append (params, (void*) sfi_value_get_string (sfi_seq_get (seq, i)));
	  type = bse_script_proc_register (janitor->script_name,
					   sfi_value_get_string (sfi_seq_get (seq, 0)),
					   sfi_value_get_string (sfi_seq_get (seq, 1)),
					   sfi_value_get_string (sfi_seq_get (seq, 2)),
					   sfi_value_get_string (sfi_seq_get (seq, 3)),
					   sfi_value_get_string (sfi_seq_get (seq, 4)),
					   g_ascii_strtoull (sfi_value_get_string (sfi_seq_get (seq, 5)), NULL, 10),
					   sfi_value_get_string (sfi_seq_get (seq, 6)),
					   sfi_value_get_string (sfi_seq_get (seq, 7)),
					   params);
          (void) type;
          sfi_ring_free (params);
	  retval = sfi_value_bool (TRUE);	// success
	}
      return retval;
    }
  else if (strcmp (message, "bse-client-msg-script-args") == 0)
    {
      SfiSeq *seq = (SfiSeq*) g_object_get_qdata ((GObject*) janitor, quark_script_args);
      GValue *rvalue = sfi_value_seq (seq);
      g_object_set_qdata ((GObject*) janitor, quark_script_args, NULL);
      return rvalue;
    }
  return NULL;
}

SfiRing*
bse_script_path_list_files (void)
{
  SfiRing *files, *ring = NULL;
  if (bse_main_args->override_script_path)
    {
      files = sfi_file_crawler_list_files (bse_main_args->override_script_path, "*.scm", G_FILE_TEST_IS_REGULAR);
      ring = sfi_ring_concat (ring, sfi_ring_sort (files, (SfiCompareFunc) strcmp, NULL));
      return ring;      /* override */
    }
  if (1)
    {
      files = sfi_file_crawler_list_files (BSE_PATH_SCRIPTS, "*.scm", G_FILE_TEST_IS_REGULAR);
      ring = sfi_ring_concat (ring, sfi_ring_sort (files, (SfiCompareFunc) strcmp, NULL));
    }
  if (BSE_GCONFIG (script_path) && BSE_GCONFIG (script_path)[0])
    {
      files = sfi_file_crawler_list_files (BSE_GCONFIG (script_path), "*.scm", G_FILE_TEST_IS_REGULAR);
      ring = sfi_ring_concat (ring, sfi_ring_sort (files, (SfiCompareFunc) strcmp, NULL));
    }
  return ring;
}

BseErrorType
bse_script_file_register (const gchar *file_name,
			  BseJanitor **janitor_p)
{
  BseServer *server = bse_server_get ();
  SfiRing *params = NULL;
  const char *proc_name = "registration hook";
  BseErrorType error;

  params = sfi_ring_append (params, g_strdup ("--bse-enable-register"));
  params = sfi_ring_append (params, g_strdup ("--bse-eval"));
  params = sfi_ring_append (params, g_strdup_format ("(load \"%s\")", file_name));
  char *shellpath = g_strdup_format ("%s/%s-%s", bse_main_args->path_binaries, "bsescm", BIN_VERSION);
  *janitor_p = NULL;
  error = bse_server_run_remote (server, shellpath,
				 params, file_name, proc_name, janitor_p);
  g_free (shellpath);
  sfi_ring_free_deep (params, g_free);

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

#define PARAM_HINTS	SFI_PARAM_STANDARD ":lax-validation:"

static GParamSpec*
bse_script_param_spec (gchar       *pspec_desc,
		       const gchar *script_name,
		       const gchar *func_name,
		       gchar      **free1,
		       gchar      **free2)
{
  gchar *pstring = strchr (pspec_desc, ':');
  gchar *dflt, *cname;

  if (!pstring)
    return NULL;
  *pstring++ = 0;
  dflt = strchr (pstring, ':');
  if (!dflt)
    return NULL;
  *dflt++ = 0;
  cname = make_sname (pstring);
  const gchar *nick = _(pstring);
  *free1 = cname;
  gchar *blurb = g_strdup_format ("Parameter \"%s\" to function <%s> in script \"%s\"",
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
