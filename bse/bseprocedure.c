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
#include        "bseprocedure.h"

#include        <gobject/gvaluecollector.h>
#include        "bseobject.h"
#include        "bseserver.h"
#include        "bsestorage.h"
#include        "bseexports.h"
#include        <string.h>


/* --- macros --- */
#define parse_or_return         bse_storage_scanner_parse_or_return
#define peek_or_return          bse_storage_scanner_peek_or_return
#define DEBUG                   sfi_debug_keyfunc ("procs")
#define HACK_DEBUG /* very slow and leaks memory */ while (0) g_message


/* --- prototypes --- */
extern void     bse_type_register_procedure_info  (GTypeInfo                *info);
static void     bse_procedure_base_init           (BseProcedureClass        *proc);
static void     bse_procedure_base_finalize       (BseProcedureClass        *proc);
static void     bse_procedure_init                (BseProcedureClass        *proc,
                                                   const BseExportNodeProc  *pnode);


/* --- functions --- */
extern void
bse_type_register_procedure_info (GTypeInfo *info)
{
  static const GTypeInfo proc_info = {
    sizeof (BseProcedureClass),
    
    (GBaseInitFunc) bse_procedure_base_init,
    (GBaseFinalizeFunc) bse_procedure_base_finalize,
    (GClassInitFunc) NULL,
    (GClassFinalizeFunc) NULL,
    NULL /* class_data */,
    
    /* non classed type stuff */
    0, 0, NULL,
  };
  
  *info = proc_info;
}

static void
bse_procedure_base_init (BseProcedureClass *proc)
{
  proc->name = NULL;
  proc->blurb = NULL;
  proc->private_id = 0;
  proc->help = NULL;
  proc->authors = NULL;
  proc->copyright = NULL;
  proc->n_in_pspecs = 0;
  proc->in_pspecs = NULL;
  proc->n_out_pspecs = 0;
  proc->out_pspecs = NULL;
  proc->execute = NULL;
}

static void
bse_procedure_base_finalize (BseProcedureClass *proc)
{
  guint i;
  
  proc->name = NULL;
  proc->blurb = NULL;
  proc->help = NULL;
  proc->authors = NULL;
  proc->copyright = NULL;
  
  /* give up type references */
  for (i = 0; proc->class_refs[i]; i++)
    g_type_class_unref (proc->class_refs[i]);
  g_free (proc->class_refs);
  proc->class_refs = NULL;

  for (i = 0; i < proc->n_in_pspecs; i++)
    g_param_spec_unref (proc->in_pspecs[i]);
  g_free (proc->in_pspecs);
  for (i = 0; i < proc->n_out_pspecs; i++)
    g_param_spec_unref (proc->out_pspecs[i]);
  g_free (proc->out_pspecs);
  
  proc->execute = NULL;
}

static void
bse_procedure_init (BseProcedureClass       *proc,
                    const BseExportNodeProc *pnode)
{
  GParamSpec *in_pspecs[BSE_PROCEDURE_MAX_IN_PARAMS + 8];
  GParamSpec *out_pspecs[BSE_PROCEDURE_MAX_OUT_PARAMS + 8];
  guint i, j;
  gchar *const_name, *const_blurb;
  
  memset (in_pspecs, 0, sizeof (in_pspecs));
  memset (out_pspecs, 0, sizeof (out_pspecs));
  
  proc->name = g_type_name (BSE_PROCEDURE_TYPE (proc));
  proc->blurb = bse_type_blurb (BSE_PROCEDURE_TYPE (proc));
  proc->private_id = pnode->private_id;
  
  /* init procedure class from plugin,
   * paranoia check certain class members
   */
  const_name = proc->name;
  const_blurb = proc->blurb;
  pnode->init (proc, in_pspecs, out_pspecs);
  if (proc->name != const_name)
    {
      proc->name = const_name;
      g_warning ("procedure \"%s\" redefines procedure name", proc->name);
    }
  if (proc->blurb != const_blurb)
    {
      proc->blurb = const_blurb;
      g_warning ("procedure \"%s\" redefines procedure blurb", proc->name);
    }
  if (proc->n_in_pspecs || proc->in_pspecs ||
      proc->n_out_pspecs || proc->out_pspecs ||
      proc->execute)
    {
      proc->n_in_pspecs = 0;
      proc->in_pspecs = NULL;
      proc->n_out_pspecs = 0;
      proc->out_pspecs = NULL;
      proc->execute = NULL;
      g_warning ("procedure \"%s\" messes with reserved class members", proc->name);
    }
  
  /* check input parameters and setup specifications */
  for (i = 0; i < BSE_PROCEDURE_MAX_IN_PARAMS; i++)
    if (in_pspecs[i])
      {
        if ((in_pspecs[i]->flags & G_PARAM_READWRITE) != G_PARAM_READWRITE)
          g_warning ("procedure \"%s\": input parameter \"%s\" has invalid flags",
                     proc->name,
                     in_pspecs[i]->name);
        g_param_spec_ref (in_pspecs[i]);
        g_param_spec_sink (in_pspecs[i]);
      }
    else
      break;
  if (i == BSE_PROCEDURE_MAX_IN_PARAMS && in_pspecs[i])
    g_warning ("procedure \"%s\" exceeds maximum number of input parameters (%u)",
               proc->name, BSE_PROCEDURE_MAX_IN_PARAMS);
  proc->n_in_pspecs = i;
  proc->in_pspecs = g_new (GParamSpec*, proc->n_in_pspecs + 1);
  memcpy (proc->in_pspecs, in_pspecs, sizeof (in_pspecs[0]) * proc->n_in_pspecs);
  proc->in_pspecs[proc->n_in_pspecs] = NULL;
  
  /* check output parameters and setup specifications */
  for (i = 0; i < BSE_PROCEDURE_MAX_OUT_PARAMS; i++)
    if (out_pspecs[i])
      {
        if ((out_pspecs[i]->flags & G_PARAM_READWRITE) != G_PARAM_READWRITE)
          g_warning ("procedure \"%s\": output parameter \"%s\" has invalid flags",
                     proc->name,
                     out_pspecs[i]->name);
        g_param_spec_ref (out_pspecs[i]);
        g_param_spec_sink (out_pspecs[i]);
      }
    else
      break;
  if (i == BSE_PROCEDURE_MAX_OUT_PARAMS && out_pspecs[i])
    g_warning ("procedure \"%s\" exceeds maximum number of output parameters (%u)",
               proc->name, BSE_PROCEDURE_MAX_OUT_PARAMS);
  proc->n_out_pspecs = i;
  proc->out_pspecs = g_new (GParamSpec*, proc->n_out_pspecs + 1);
  memcpy (proc->out_pspecs, out_pspecs, sizeof (out_pspecs[0]) * proc->n_out_pspecs);
  proc->out_pspecs[proc->n_out_pspecs] = NULL;

  /* keep type references */
  proc->class_refs = g_new (GTypeClass*, proc->n_in_pspecs + proc->n_out_pspecs + 1);
  j = 0;
  for (i = 0; i < proc->n_in_pspecs; i++)
    if (G_TYPE_IS_CLASSED ((G_PARAM_SPEC_VALUE_TYPE (proc->in_pspecs[i]))))
      proc->class_refs[j++] = g_type_class_ref (G_PARAM_SPEC_VALUE_TYPE (proc->in_pspecs[i]));
  for (i = 0; i < proc->n_out_pspecs; i++)
    if (G_TYPE_IS_CLASSED ((G_PARAM_SPEC_VALUE_TYPE (proc->out_pspecs[i]))))
      proc->class_refs[j++] = g_type_class_ref (G_PARAM_SPEC_VALUE_TYPE (proc->out_pspecs[i]));
  proc->class_refs[j++] = NULL;

  /* hookup execute method */
  proc->execute = pnode->exec;
}

void
bse_procedure_complete_info (const BseExportNodeProc *pnode,
                             GTypeInfo               *info)
{
  info->class_size = sizeof (BseProcedureClass);
  info->class_init = (GClassInitFunc) bse_procedure_init;
  info->class_finalize = (GClassFinalizeFunc) NULL;
  info->class_data = pnode;
}

const gchar*
bse_procedure_type_register (const gchar *name,
                             const gchar *blurb,
                             BsePlugin   *plugin,
                             GType       *ret_type)
{
  GType   type, base_type = 0;
  gchar *p;
  
  g_return_val_if_fail (ret_type != NULL, bse_error_blurb (BSE_ERROR_INTERNAL));
  *ret_type = 0;
  g_return_val_if_fail (name != NULL, bse_error_blurb (BSE_ERROR_INTERNAL));
  g_return_val_if_fail (plugin != NULL, bse_error_blurb (BSE_ERROR_INTERNAL));
  
  type = g_type_from_name (name);
  if (type)
    return "Procedure already registered";
  
  p = strchr (name, '+');
  if (p)
    {
      /* enforce <OBJECT>+<METHOD> syntax */
      if (!p[1])
        return "Procedure name invalid";
      
      p = g_strndup (name, p - name);
      base_type = g_type_from_name (p);
      g_free (p);
      if (!g_type_is_a (base_type, BSE_TYPE_OBJECT))
        return "Procedure base type invalid";
    }
  
  type = bse_type_register_dynamic (BSE_TYPE_PROCEDURE,
                                    name,
                                    blurb,
                                    G_TYPE_PLUGIN (plugin));
  
  *ret_type = type;
  
  return NULL;
}

GType
bse_procedure_lookup (const gchar *proc_name)
{
  GType type;
  
  g_return_val_if_fail (proc_name != NULL, 0);
  
  type = g_type_from_name (proc_name);
  return BSE_TYPE_IS_PROCEDURE (type) ? type : 0;
}

static void
signal_exec_status (BseErrorType       error,
                    BseProcedureClass *proc,
                    GValue            *first_ovalue)
{
#if 0
  /* signal script status, supporting BseErrorType-outparam procedures
   */
  if (!error && proc->n_out_pspecs == 1 &&
      g_type_is_a (G_VALUE_TYPE (first_ovalue), BSE_TYPE_ERROR_TYPE))
    {
      BseErrorType verror = g_value_get_enum (first_ovalue);
      
      bse_server_exec_status (bse_server_get (), BSE_EXEC_STATUS_DONE, proc->name, verror ? 0 : 1, verror);
    }
  else
    bse_server_exec_status (bse_server_get (), BSE_EXEC_STATUS_DONE, proc->name, error ? 0 : 1, error);
#endif
}

static BseErrorType
call_proc (BseProcedureClass  *proc,
           GValue             *ivalues,
           GValue             *ovalues,
           BseProcedureMarshal marshal,
           gpointer            marshal_data)
{
  guint i, bail_out = FALSE;
  BseErrorType error;
  
  for (i = 0; i < proc->n_in_pspecs; i++)
    {
      GParamSpec *pspec = proc->in_pspecs[i];
      
      if (g_param_value_validate (pspec, ivalues + i) && !(pspec->flags & G_PARAM_LAX_VALIDATION))
        {
          g_warning ("%s: input arg `%s' contains invalid value",
                     proc->name,
                     pspec->name);
          bail_out = TRUE;
        }
    }
  
  if (bail_out)
    error = BSE_ERROR_PROC_PARAM_INVAL;
  else
    {
      if (sfi_debug_test_key ("procs"))
        {
          if (proc->n_in_pspecs && G_TYPE_IS_OBJECT (G_PARAM_SPEC_VALUE_TYPE (proc->in_pspecs[0])))
            DEBUG ("executing procedure \"%s\" on object %s",
                   proc->name, bse_object_debug_name (g_value_get_object (ivalues + 0)));
          else
            DEBUG ("executing procedure \"%s\"", proc->name);
        }
      if (marshal)
        error = marshal (marshal_data, proc, ivalues, ovalues);
      else
        error = proc->execute (proc, ivalues, ovalues);
    }
  
  for (i = 0; i < proc->n_in_pspecs; i++)
    {
      GParamSpec *pspec = proc->in_pspecs[i];
      
      if (g_param_value_validate (pspec, ivalues + i) && !(pspec->flags & G_PARAM_LAX_VALIDATION))
        g_warning ("%s: internal procedure error: output arg `%s' had invalid value",
                   proc->name,
                   pspec->name);
    }
  
  return error;
}

BseErrorType
bse_procedure_marshal (GType               proc_type,
                       const GValue       *ivalues,
                       GValue             *ovalues,
                       BseProcedureMarshal marshal,
                       gpointer            marshal_data)
{
  BseProcedureClass *proc;
  GValue tmp_ivalues[BSE_PROCEDURE_MAX_IN_PARAMS];
  GValue tmp_ovalues[BSE_PROCEDURE_MAX_OUT_PARAMS];
  guint i, bail_out = FALSE;
  BseErrorType error;
  
  g_return_val_if_fail (BSE_TYPE_IS_PROCEDURE (proc_type), BSE_ERROR_INTERNAL);
  
  proc = g_type_class_ref (proc_type);
  for (i = 0; i < proc->n_in_pspecs; i++)
    {
      GParamSpec *pspec = proc->in_pspecs[i];
      
      tmp_ivalues[i].g_type = 0;
      g_value_init (tmp_ivalues + i, G_PARAM_SPEC_VALUE_TYPE (pspec));
      if (!sfi_value_transform (ivalues + i, tmp_ivalues + i))
        {
          g_warning ("%s: input arg `%s' has invalid type `%s' (expected `%s')",
                     proc->name,
                     pspec->name,
                     G_VALUE_TYPE_NAME (ivalues + i),
                     g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
          bail_out = TRUE;
        }
    }
  for (i = 0; i < proc->n_out_pspecs; i++)
    {
      tmp_ovalues[i].g_type = 0;
      g_value_init (tmp_ovalues + i, G_PARAM_SPEC_VALUE_TYPE (proc->out_pspecs[i]));
    }
  
  if (bail_out)
    error = BSE_ERROR_PROC_PARAM_INVAL;
  else
    error = call_proc (proc, tmp_ivalues, tmp_ovalues, marshal, marshal_data);
  signal_exec_status (error, proc, tmp_ovalues);
  
  for (i = 0; i < proc->n_in_pspecs; i++)
    g_value_unset (tmp_ivalues + i);
  for (i = 0; i < proc->n_out_pspecs; i++)
    {
      GParamSpec *pspec = proc->out_pspecs[i];
      
      if (!sfi_value_transform (tmp_ovalues + i, ovalues + i))
        g_warning ("%s: output arg `%s' of type `%s' cannot be converted into `%s'",
                   proc->name,
                   pspec->name,
                   g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
                   G_VALUE_TYPE_NAME (ovalues + i));
      g_value_unset (tmp_ovalues + i);
    }
  g_type_class_unref (proc);
  
  return error;
}

static inline BseErrorType
bse_procedure_call_collect (BseProcedureClass  *proc,
                            const GValue       *first_value,
                            BseProcedureMarshal marshal,
                            gpointer            marshal_data,
                            gboolean            skip_call,
                            gboolean            skip_ovalues,
                            GValue             *ivalues,
                            GValue             *ovalues,
                            va_list             var_args)
{
  guint i, bail_out = FALSE;
  BseErrorType error = BSE_ERROR_NONE;

  HACK_DEBUG ("call %s: ", proc->name);

  /* collect first arg */
  if (first_value && first_value != ivalues) /* may skip this since call_proc() does extra validation */
    {
      if (proc->n_in_pspecs < 1)
        g_warning ("%s: input arg supplied for procedure taking `void'",
                   proc->name);
      else
        {
          GParamSpec *pspec = proc->in_pspecs[0];
          ivalues[0].g_type = 0;
          g_value_init (ivalues + 0, G_PARAM_SPEC_VALUE_TYPE (pspec));
          if (!sfi_value_transform (first_value, ivalues + 0))
            {
              g_warning ("%s: input arg `%s' has invalid type `%s' (expected `%s')",
                         proc->name,
                         pspec->name,
                         G_VALUE_TYPE_NAME (first_value),
                         g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
              bail_out = TRUE;
            }
        }
    }
  if (first_value)
    HACK_DEBUG ("  arg[%u]<%s>: %s", 0, g_type_name (ivalues[0].g_type), g_strdup_value_contents (ivalues) /* memleak */);
  /* collect remaining args */
  for (i = first_value ? 1 : 0; i < proc->n_in_pspecs; i++)
    {
      GParamSpec *pspec = proc->in_pspecs[i];
      gchar *error_msg = NULL;
      
      ivalues[i].g_type = 0;
      g_value_init (ivalues + i, G_PARAM_SPEC_VALUE_TYPE (pspec));
      if (!bail_out)
        G_VALUE_COLLECT (ivalues + i, var_args, 0, &error_msg);
      if (error_msg)
        {
          g_warning ("%s: failed to collect arg `%s' of type `%s': %s",
                     proc->name,
                     pspec->name,
                     G_VALUE_TYPE_NAME (ivalues + i),
                     error_msg);
          g_free (error_msg);
          bail_out = TRUE;
        }
      HACK_DEBUG ("  arg[%u]<%s>: %s", i, g_type_name (ivalues[i].g_type), g_strdup_value_contents (ivalues + i) /* memleak */);
    }

  if (!skip_call)
    {
      /* initialize return values */
      for (i = 0; i < proc->n_out_pspecs; i++)
        {
          ovalues[i].g_type = 0;
          g_value_init (ovalues + i, G_PARAM_SPEC_VALUE_TYPE (proc->out_pspecs[i]));
        }

      /* execute procedure */
      if (bail_out)
        error = BSE_ERROR_PROC_PARAM_INVAL;
      else
        error = call_proc (proc, ivalues, ovalues, marshal, marshal_data);
      HACK_DEBUG ("  call result: %s", bse_error_blurb (error));
      signal_exec_status (error, proc, ovalues);

      /* free input arguments */
      for (i = 0; i < proc->n_in_pspecs; i++)
        g_value_unset (ivalues + i);

      /* copy return values into locations */
      for (i = 0; i < proc->n_out_pspecs; i++)
        {
          GParamSpec *pspec = proc->out_pspecs[i];
          gchar *error_msg = NULL;
          
          if (!skip_ovalues)
            G_VALUE_LCOPY (ovalues + i, var_args, 0, &error_msg);
          if (error_msg)
            {
              g_warning ("%s: failed to return arg `%s' of type `%s': %s",
                         proc->name,
                         pspec->name,
                         G_VALUE_TYPE_NAME (ovalues + i),
                         error_msg);
              g_free (error_msg);
              skip_ovalues = TRUE;
            }
          g_value_unset (ovalues + i);
        }
    }
  else
    HACK_DEBUG ("  call skipped");
  
  return error;
}

/**
 * bse_procedure_marshal_valist
 * @proc_type:    a type derived from %BSE_TYPE_PROCEDURE
 * @first_value:  the first input argument if not to be collected
 * @marshal:      function marshalling the procedure call or %NULL
 * @marshal_data: data passed in to @marshal
 * @skip_ovalues: whether return value locations should be collected and filled in
 * @var_args:     #va_list to collect input args from
 * @RETURNS:      #BseErrorType value of error if any occoured
 *
 * Collect input arguments for a procedure call from a #va_list and
 * call the procedure, optionally via @marshal. If @skip_ovalues is
 * %FALSE, the procedure return values will be stored in return
 * value locations also collected from @var_args.
 */
BseErrorType
bse_procedure_marshal_valist (GType               proc_type,
                              const GValue       *first_value,
                              BseProcedureMarshal marshal,
                              gpointer            marshal_data,
                              gboolean            skip_ovalues,
                              va_list             var_args)
{
  GValue tmp_ivalues[BSE_PROCEDURE_MAX_IN_PARAMS];
  GValue tmp_ovalues[BSE_PROCEDURE_MAX_OUT_PARAMS];
  BseProcedureClass *proc;
  BseErrorType error;

  g_return_val_if_fail (BSE_TYPE_IS_PROCEDURE (proc_type), BSE_ERROR_INTERNAL);

  proc = g_type_class_ref (proc_type);
  error = bse_procedure_call_collect (proc, first_value, marshal, marshal_data,
                                      FALSE, skip_ovalues, tmp_ivalues, tmp_ovalues, var_args);
  g_type_class_unref (proc);
  return error;
}

/**
 * bse_procedure_collect_input_args
 * @proc:        valid #BseProcedureClass
 * @first_value: the first input argument if not to be collected
 * @var_args:    #va_list to collect input args from
 * @ivalues:     uninitialized GValue array with at least proc->n_in_pspecs members
 * @RETURNS:     #BseErrorType value of error if any occoured during collection
 *
 * Collect input arguments for a procedure call from a #va_list. The first
 * value may be supplied as @first_value and will then not be collected.
 * @ivalues must be at least @proc->n_in_pspecs elements long and all elements
 * will be initialized after the function returns (even in error cases).
 * @first_value may be the same adress as @ivalues, in whic hcase the first
 * argument is entirely ignored and collection simply starts out with the
 * second argument.
 */
BseErrorType
bse_procedure_collect_input_args (BseProcedureClass  *proc,
                                  const GValue       *first_value,
                                  va_list             var_args,
                                  GValue              ivalues[BSE_PROCEDURE_MAX_IN_PARAMS])
{
  g_return_val_if_fail (BSE_IS_PROCEDURE_CLASS (proc), BSE_ERROR_INTERNAL);

  return bse_procedure_call_collect (proc, first_value, NULL, NULL,
                                     TRUE, TRUE, ivalues, NULL, var_args);
}

BseErrorType
bse_procedure_exec (const gchar *proc_name,
                    ...)
{
  GType proc_type;
  
  g_return_val_if_fail (proc_name != NULL, BSE_ERROR_INTERNAL);
  
  proc_type = bse_procedure_lookup (proc_name);
  if (!proc_type)
    {
      g_warning ("%s: no such procedure", proc_name);
      return BSE_ERROR_PROC_NOT_FOUND;
    }
  else
    {
      BseErrorType error;
      va_list var_args;
      
      va_start (var_args, proc_name);
      error = bse_procedure_marshal_valist (proc_type, NULL, NULL, NULL, FALSE, var_args);
      va_end (var_args);
      return error;
    }
}

BseErrorType
bse_procedure_exec_void (const gchar *proc_name,
                         ...)
{
  GType proc_type;
  
  g_return_val_if_fail (proc_name != NULL, BSE_ERROR_INTERNAL);
  
  proc_type = bse_procedure_lookup (proc_name);
  if (!proc_type)
    {
      g_warning ("%s: no such procedure", proc_name);
      return BSE_ERROR_PROC_NOT_FOUND;
    }
  else
    {
      BseErrorType error;
      va_list var_args;
      
      va_start (var_args, proc_name);
      error = bse_procedure_marshal_valist (proc_type, NULL, NULL, NULL, TRUE, var_args);
      va_end (var_args);
      return error;
    }
}

BseErrorType
bse_procedure_execvl (BseProcedureClass  *proc,
                      GSList             *in_value_list,
                      GSList             *out_value_list,
                      BseProcedureMarshal marshal,
                      gpointer            marshal_data)
{
  GValue tmp_ivalues[BSE_PROCEDURE_MAX_IN_PARAMS];
  GValue tmp_ovalues[BSE_PROCEDURE_MAX_OUT_PARAMS];
  BseErrorType error;
  GSList *slist;
  guint i;
  
  /* FIXME: bad, bad compat: bse_procedure_execvl() */
  
  for (i = 0, slist = in_value_list; slist && i < proc->n_in_pspecs; i++, slist = slist->next)
    memcpy (tmp_ivalues + i, slist->data, sizeof (tmp_ivalues[0]));
  if (slist || i != proc->n_in_pspecs)
    {
      g_warning ("%s: invalid number of arguments supplied to procedure \"%s\"", G_STRLOC, proc->name);
      return BSE_ERROR_PROC_PARAM_INVAL;
    }
  for (i = 0, slist = out_value_list; slist && i < proc->n_out_pspecs; i++, slist = slist->next)
    memcpy (tmp_ovalues + i, slist->data, sizeof (tmp_ovalues[0]));
  if (slist || i != proc->n_out_pspecs)
    {
      g_warning ("%s: invalid number of arguments supplied to procedure \"%s\"", G_STRLOC, proc->name);
      return BSE_ERROR_PROC_PARAM_INVAL;
    }
  error = bse_procedure_marshal (BSE_PROCEDURE_TYPE (proc), tmp_ivalues, tmp_ovalues, marshal, marshal_data);
  for (i = 0, slist = out_value_list; slist && i < proc->n_out_pspecs; i++, slist = slist->next)
    memcpy (slist->data, tmp_ovalues + i, sizeof (tmp_ivalues[0]));
  return error;
}
