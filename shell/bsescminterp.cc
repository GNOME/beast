// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#include <string.h>
#include <errno.h>
#include "bsescminterp.hh"
#include <sfi/sficomwire.hh>
#include <bse/bseglue.hh>

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
 * typedef SCM (*scm_t_catch_body) (void *data);
 * typedef SCM (*scm_catch_handler_t) (void *data,
 * SCM tag = SCM_BOOL_T; means catch-all
 * SCM gh_catch(SCM tag, scm_t_catch_body body, void *body_data,
 *              scm_catch_handler_t handler, void *handler_data);
 */

#define	BSE_SCM_NIL	SCM_UNSPECIFIED
#define	BSE_SCM_NILP(x)	((x) == SCM_UNSPECIFIED)


/* allow guile version special casing */
#define GUILE_CHECK_VERSION(major,minor,micro)    \
    (SCM_MAJOR_VERSION > (major) || \
     (SCM_MAJOR_VERSION == (major) && SCM_MINOR_VERSION > (minor)) || \
     (SCM_MAJOR_VERSION == (major) && SCM_MINOR_VERSION == (minor) && \
      SCM_MICRO_VERSION >= (micro)))

#if GUILE_CHECK_VERSION (1, 8, 0)
#define BSE_SCM_DEFER_INTS()            do ; while (0)
#define BSE_SCM_ALLOW_INTS()            do ; while (0)
#define IS_SCM_INT(s_scm)               SCM_I_INUMP (s_scm)     // scm_is_integer() breaks for non-fractional floats
#define SFI_NUM_FROM_SCM(s_scm)         ((SfiNum) scm_to_int64 (s_scm))
#define STRING_CHARS_FROM_SCM(s_scm)    scm_i_string_chars (s_scm)
#define STRING_LENGTH_FROM_SCM(s_scm)   scm_i_string_length (s_scm)
#define IS_SCM_STRING(s_scm)            scm_is_string (s_scm)
#define IS_SCM_SYMBOL(s_scm)            scm_is_symbol (s_scm)
#define IS_SCM_BOOL(s_scm)              scm_is_bool (s_scm)
#define IS_SCM_PAIR(s_scm)              scm_is_pair (s_scm)
#else   /* 1.6.x */
#define BSE_SCM_DEFER_INTS()            SCM_REDEFER_INTS        // guard around GC-protected code portions; with incremental int-
#define BSE_SCM_ALLOW_INTS()            SCM_REALLOW_INTS        // blocking. guile recovers from unbalanced defer/allow pairs.
#define IS_SCM_INT(s_scm)               SCM_INUMP (s_scm)
#define SFI_NUM_FROM_SCM(s_scm)         ((SfiNum) scm_num2long_long ((s_scm), 1, "num2int64"))
#define STRING_CHARS_FROM_SCM(s_scm)    SCM_ROCHARS (s_scm)
#define STRING_LENGTH_FROM_SCM(s_scm)   SCM_LENGTH (s_scm)
#define IS_SCM_STRING(s_scm)            SCM_STRINGP (s_scm)
#define IS_SCM_SYMBOL(s_scm)            SCM_SYMBOLP (s_scm)
#define IS_SCM_BOOL(s_scm)              SCM_BOOLP (s_scm)
#define IS_SCM_PAIR(s_scm)              SCM_CONSP (s_scm)
#endif
#define IS_SCM_BIG(s_scm)               SCM_BIGP (s_scm)
#define IS_SCM_SFI_NUM(s_scm)           (IS_SCM_INT (s_scm) || IS_SCM_BIG (s_scm))

/* --- prototypes --- */
static SCM	bse_scm_from_value		(const GValue	*value);
static GValue*	bse_value_from_scm		(SCM		 sval);

/* --- misc utilities --- */
static inline SfiNum
num_from_scm (SCM s_num)
{
  SfiNum num = 0; /* int64 */
  if (IS_SCM_SFI_NUM (s_num))
    num = SFI_NUM_FROM_SCM (s_num);
  return num;
}

static inline gchar*
strdup_from_scm (SCM s_string)
{
  if (IS_SCM_STRING (s_string))
    return g_strndup (STRING_CHARS_FROM_SCM (s_string), STRING_LENGTH_FROM_SCM (s_string));
  else if (IS_SCM_SYMBOL (s_string))
    {
      SCM s_sym_string = scm_symbol_to_string (s_string);
      return strdup_from_scm (s_sym_string);
    }
  else
    return NULL;
}

static inline GValue*
string_value_from_scm (SCM s_string)
{
  if (IS_SCM_STRING (s_string))
    return sfi_value_lstring (STRING_CHARS_FROM_SCM (s_string), STRING_LENGTH_FROM_SCM (s_string));
  else if (IS_SCM_SYMBOL (s_string))
    {
      SCM s_sym_string = scm_symbol_to_string (s_string);
      return sfi_value_lstring (STRING_CHARS_FROM_SCM (s_sym_string), STRING_LENGTH_FROM_SCM (s_sym_string));
    }
  else
    return sfi_value_string (NULL);
}

static inline GValue*
choice_value_from_scm (SCM s_string)
{
  if (IS_SCM_STRING (s_string))
    return sfi_value_lchoice (STRING_CHARS_FROM_SCM (s_string), STRING_LENGTH_FROM_SCM (s_string));
  else if (IS_SCM_SYMBOL (s_string))
    {
      SCM s_sym_string = scm_symbol_to_string (s_string);
      return sfi_value_lchoice (STRING_CHARS_FROM_SCM (s_sym_string), STRING_LENGTH_FROM_SCM (s_sym_string));
    }
  else
    return sfi_value_choice (NULL);
}

/* --- SCM GC hooks --- */
static gulong tc_glue_gc_cell = 0;
#define SCM_IS_GLUE_GC_CELL(sval)    (SCM_SMOB_PREDICATE (tc_glue_gc_cell, sval))
#define SCM_GET_GLUE_GC_CELL(sval)   ((BseScmGCCell*) SCM_SMOB_DATA (sval))
typedef void (*BseScmFreeFunc) (void *data);
typedef struct {
  gpointer       data;
  BseScmFreeFunc free_func;
  gsize          size_hint;
} BseScmGCCell;
static void
bse_scm_enter_gc (SCM           *scm_gc_list,
		  gpointer       data,
		  BseScmFreeFunc free_func, // GC callbacks may run in any thread
		  gsize          size_hint)
{
  BseScmGCCell *gc_cell;
  SCM s_cell = 0;
  assert_return (scm_gc_list != NULL);
  assert_return (free_func != NULL);
  // printerr ("GCCell allocating %u bytes (%p).\n", size_hint, free_func);
  gc_cell = g_new (BseScmGCCell, 1);
  gc_cell->data = data;
  gc_cell->free_func = free_func;
  gc_cell->size_hint = size_hint + sizeof (BseScmGCCell);
  SCM_NEWSMOB (s_cell, tc_glue_gc_cell, gc_cell);
  *scm_gc_list = gh_cons (s_cell, *scm_gc_list);
}

static SCM
bse_scm_mark_gc_cell (SCM scm_gc_cell) /* called from any thread */
{
  // BseScmGCCell *gc_cell = (BseScmGCCell*) SCM_CDR (scm_gc_cell);
  // printerr ("GCCell mark %u bytes (%p).\n", gc_cell->size_hint, gc_cell->free_func);
  /* scm_gc_mark (gc_cell->something); */
  return SCM_BOOL_F;
}

static scm_sizet
bse_scm_free_gc_cell (SCM scm_gc_cell) /* called from any thread */
{
  BseScmGCCell *gc_cell = SCM_GET_GLUE_GC_CELL (scm_gc_cell);
  // printerr ("GCCell freeing %u bytes (%p).\n", size, gc_cell->free_func);
  gc_cell->free_func (gc_cell->data);
  g_free (gc_cell);
  return 0;
}


/* --- SCM Glue GC Plateau --- */
static gulong tc_glue_gc_plateau = 0;
#define SCM_IS_GLUE_GC_PLATEAU(sval)    (SCM_SMOB_PREDICATE (tc_glue_gc_plateau, sval))
#define SCM_GET_GLUE_GC_PLATEAU(sval)   ((GcPlateau*) SCM_SMOB_DATA (sval))
static guint  scm_glue_gc_plateau_blocker = 0;
typedef struct {
  guint    size_hint;
  gboolean active_plateau;
} GcPlateau;

SCM
bse_scm_make_gc_plateau (guint size_hint)
{
  SCM s_gcplateau = SCM_UNSPECIFIED;
  GcPlateau *gp = g_new (GcPlateau, 1);
  scm_glue_gc_plateau_blocker++;
  gp->size_hint = size_hint;
  gp->active_plateau = TRUE;
  SCM_NEWSMOB (s_gcplateau, tc_glue_gc_plateau, gp);
  return s_gcplateau;
}

void
bse_scm_destroy_gc_plateau (SCM s_gcplateau)
{
  GcPlateau *gp;
  assert (SCM_IS_GLUE_GC_PLATEAU (s_gcplateau));
  gp = SCM_GET_GLUE_GC_PLATEAU (s_gcplateau);
  if (gp->active_plateau)
    {
      gp->active_plateau = FALSE;
      assert (scm_glue_gc_plateau_blocker > 0);
      scm_glue_gc_plateau_blocker--;
      if (scm_glue_gc_plateau_blocker == 0)
	sfi_glue_gc_run ();
    }
}

static scm_sizet
bse_scm_gc_plateau_free (SCM s_gcplateau) /* called from any thread */
{
  GcPlateau *gp = SCM_GET_GLUE_GC_PLATEAU (s_gcplateau);
  bse_scm_destroy_gc_plateau (s_gcplateau);
  g_free (gp);
  return 0;
}


/* --- SCM Glue Record --- */
static gulong tc_glue_rec = 0;
#define SCM_IS_GLUE_REC(sval)    (SCM_SMOB_PREDICATE (tc_glue_rec, sval))
#define SCM_GET_GLUE_REC(sval)   ((SfiRec*) SCM_SMOB_DATA (sval))
static SCM
bse_scm_from_glue_rec (SfiRec *rec)
{
  SCM s_rec = 0;

  assert_return (rec != NULL, SCM_UNSPECIFIED);

  sfi_rec_ref (rec);
  SCM_NEWSMOB (s_rec, tc_glue_rec, rec);
  return s_rec;
}

static SCM
bse_scm_glue_rec_new (SCM sfields)
{
  SfiRec *rec;
  SCM s_rec = 0;
  if (!SCM_UNBNDP (sfields))
    SCM_ASSERT (IS_SCM_PAIR (sfields) || SCM_EQ_P (sfields, SCM_EOL), sfields, SCM_ARG1, "bse-rec-new");
  rec = sfi_rec_new ();
  s_rec = bse_scm_from_glue_rec (rec);
  sfi_rec_unref (rec);
  if (!SCM_UNBNDP (sfields))
    {
      SCM node;
      for (node = sfields; IS_SCM_PAIR (node); node = SCM_CDR (node))
        {
          SCM scons = SCM_CAR (node);
          SCM_ASSERT (IS_SCM_PAIR (scons), sfields, SCM_ARG1, "bse-rec-new");
          bse_scm_glue_rec_set (s_rec, SCM_CAR (scons), SCM_CDR (scons));
        }
    }
  return s_rec;
}

static scm_sizet
bse_scm_free_glue_rec (SCM scm_rec) /* called from any thread */
{
  SfiRec *rec = SCM_GET_GLUE_REC (scm_rec);
  sfi_rec_unref (rec);
  return 0;
}

SCM
bse_scm_glue_rec_print (SCM scm_rec)
{
  SCM port = scm_current_output_port ();
  SfiRec *rec;
  guint i;

  SCM_ASSERT (SCM_IS_GLUE_REC (scm_rec), scm_rec, SCM_ARG1, "bse-rec-print");

  rec = SCM_GET_GLUE_REC (scm_rec);
  scm_puts ("'(", port);
  for (i = 0; i < rec->n_fields; i++)
    {
      const gchar *name = rec->field_names[i];
      GValue *value = rec->fields + i;

      if (i)
        scm_puts (" ", port);
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
bse_scm_glue_rec_get (SCM scm_rec,
		      SCM s_field)
{
  SCM gcplateau = bse_scm_make_gc_plateau (1024);
  GValue *val;
  SfiRec *rec;
  gchar *name;
  SCM s_val;

  SCM_ASSERT (SCM_IS_GLUE_REC (scm_rec), scm_rec, SCM_ARG1, "bse-rec-get");
  SCM_ASSERT (IS_SCM_SYMBOL (s_field),  s_field,  SCM_ARG2, "bse-rec-get");

  rec = SCM_GET_GLUE_REC (scm_rec);
  name = strdup_from_scm (s_field);
  val = sfi_rec_get (rec, name);
  g_free (name);
  if (val)
    s_val = bse_scm_from_value (val);
  else
    s_val = SCM_UNSPECIFIED;

  bse_scm_destroy_gc_plateau (gcplateau);
  return s_val;
}

SCM
bse_scm_glue_rec_set (SCM scm_rec,
		      SCM s_field,
                      SCM s_value)
{
  SCM gcplateau = bse_scm_make_gc_plateau (1024);
  GValue *val;
  SfiRec *rec;
  gchar *name;

  SCM_ASSERT (SCM_IS_GLUE_REC (scm_rec), scm_rec, SCM_ARG1, "bse-rec-set");
  SCM_ASSERT (IS_SCM_SYMBOL (s_field),  s_field,  SCM_ARG2, "bse-rec-set");

  rec = SCM_GET_GLUE_REC (scm_rec);
  val = bse_value_from_scm (s_value);
  if (!val)
    SCM_ASSERT (FALSE, s_value, SCM_ARG3, "bse-rec-set");
  name = strdup_from_scm (s_field);
  sfi_rec_set (rec, name, val);
  g_free (name);
  bse_scm_destroy_gc_plateau (gcplateau);
  return SCM_UNSPECIFIED;
}


/* --- SCM Glue Proxy --- */
static gulong tc_glue_proxy = 0;
#define SCM_IS_GLUE_PROXY(sval)    (SCM_SMOB_PREDICATE (tc_glue_proxy, sval))
#define SCM_GET_GLUE_PROXY(sval)   ((SfiProxy) SCM_SMOB_DATA (sval))
static SCM    glue_null_proxy;
static SCM
bse_scm_make_glue_proxy (SfiProxy proxy)
{
  SCM s_proxy = 0;
  if (!proxy)
    return glue_null_proxy;
  SCM_NEWSMOB (s_proxy, tc_glue_proxy, proxy);
  return s_proxy;
}

static SCM
bse_scm_proxy_equalp (SCM scm_p1,
		      SCM scm_p2)
{
  SfiProxy p1 = SCM_GET_GLUE_PROXY (scm_p1);
  SfiProxy p2 = SCM_GET_GLUE_PROXY (scm_p2);

  return SCM_BOOL (p1 == p2);
}

static int
bse_scm_proxy_print (SCM              scm_p1,
                     SCM              port,
                     scm_print_state *pstate)
{
  SfiProxy p1 = SCM_GET_GLUE_PROXY (scm_p1);
  std::string str = Bse::string_format ("%08lx (ID:%04lx)", (unsigned long) SCM_SMOB_DATA (scm_p1), (unsigned long) p1);
  scm_puts ("#<SfiProxy ", port);
  scm_puts (str.c_str(), port);
  scm_puts (">", port);
  return 1;
}

static SCM
bse_scm_proxy_is_null (SCM scm_proxy)
{
  if (SCM_IS_GLUE_PROXY (scm_proxy))
    {
      SfiProxy p = SCM_GET_GLUE_PROXY (scm_proxy);
      return SCM_BOOL (p == 0);
    }
  return SCM_BOOL_F;
}

static SCM
bse_scm_proxy_get_null (SCM scm_proxy)
{
  return glue_null_proxy;
}

/* --- SCM procedures --- */
static gboolean server_enabled = FALSE;

void
bse_scm_enable_server (gboolean enabled)
{
  server_enabled = enabled != FALSE;
}

SCM
bse_scm_server_get (void)
{
  SfiProxy server;
  SCM s_retval;

  static const int bse_server_id = 1; /* HACK */

  BSE_SCM_DEFER_INTS ();
  server = server_enabled ? bse_server_id : 0;
  BSE_SCM_ALLOW_INTS ();
  s_retval = bse_scm_make_glue_proxy (server);

  return s_retval;
}

static GValue*
bse_value_from_scm (SCM sval)
{
  GValue *value;
  if (IS_SCM_BOOL (sval))
    value = sfi_value_bool (!SCM_FALSEP (sval));
  else if (IS_SCM_INT (sval))
    value = sfi_value_int (num_from_scm (sval));
  else if (IS_SCM_BIG (sval))
    value = sfi_value_num (num_from_scm (sval));
  else if (SCM_REALP (sval))
    value = sfi_value_real (scm_num2dbl (sval, "bse_value_from_scm"));
  else if (IS_SCM_SYMBOL (sval))
    value = choice_value_from_scm (sval);
  else if (IS_SCM_STRING (sval))
    value = string_value_from_scm (sval);
  else if (IS_SCM_PAIR (sval))
    {
      SfiSeq *seq = sfi_seq_new ();
      SCM node;
      for (node = sval; IS_SCM_PAIR (node); node = SCM_CDR (node))
	{
	  GValue *v = bse_value_from_scm (SCM_CAR (node));
	  sfi_seq_append (seq, v);
	  sfi_value_free (v);
	}
      value = sfi_value_seq (seq);
      sfi_seq_unref (seq);
    }
  else if (SCM_IS_GLUE_PROXY (sval))
    {
      SfiProxy proxy = SCM_GET_GLUE_PROXY (sval);
      value = sfi_value_proxy (proxy);
    }
  else if (SCM_IS_GLUE_REC (sval))
    {
      SfiRec *rec = SCM_GET_GLUE_REC (sval);
      value = sfi_value_rec (rec);
    }
  else
    value = NULL;
  return value;
}

static SCM
bse_scm_from_value (const GValue *value)
{
  SCM gcplateau = bse_scm_make_gc_plateau (1024);
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
      sval = scm_long_long2num (sfi_value_get_num (value));
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
      sval = str ? gh_symbol2scm (str) : BSE_SCM_NIL;
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
      sval = bse_scm_make_glue_proxy (sfi_value_get_proxy (value));
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
	sval = bse_scm_from_glue_rec (rec);
      else
	sval = BSE_SCM_NIL;
      break;
    default:
      g_error ("invalid value type while converting to SCM: %s", g_type_name (G_VALUE_TYPE (value)));
      break;
    }
  bse_scm_destroy_gc_plateau (gcplateau);
  return sval;
}

SCM
bse_scm_glue_set_prop (SCM s_proxy,
		       SCM s_prop_name,
		       SCM s_value)
{
  SCM gcplateau = bse_scm_make_gc_plateau (1024);
  SCM gclist = SCM_EOL;
  SfiProxy proxy;
  gchar *prop_name;
  GValue *value;

  SCM_ASSERT (SCM_IS_GLUE_PROXY (s_proxy),  s_proxy,  SCM_ARG1, "bse-glue-set-prop");
  SCM_ASSERT (IS_SCM_STRING (s_prop_name), s_prop_name, SCM_ARG2, "bse-glue-set-prop");

  BSE_SCM_DEFER_INTS ();

  proxy = SCM_GET_GLUE_PROXY (s_proxy);
  prop_name = strdup_from_scm (s_prop_name);
  bse_scm_enter_gc (&gclist, prop_name, g_free, STRING_LENGTH_FROM_SCM (s_prop_name));

  value = bse_value_from_scm (s_value);
  if (value)
    {
      sfi_glue_proxy_set_property (proxy, prop_name, value);
      sfi_value_free (value);
    }
  else
    scm_wrong_type_arg ("bse-set-prop", SCM_ARG3, s_value);

  BSE_SCM_ALLOW_INTS ();

  bse_scm_destroy_gc_plateau (gcplateau);
  return SCM_UNSPECIFIED;
}

SCM
bse_scm_glue_get_prop (SCM s_proxy,
		       SCM s_prop_name)
{
  SCM gcplateau = bse_scm_make_gc_plateau (1024);
  SCM gclist = SCM_EOL;
  SCM s_retval = SCM_UNSPECIFIED;
  SfiProxy proxy;
  gchar *prop_name;
  const GValue *value;

  SCM_ASSERT (SCM_IS_GLUE_PROXY (s_proxy),  s_proxy,  SCM_ARG1, "bse-glue-get-prop");
  SCM_ASSERT (IS_SCM_STRING (s_prop_name), s_prop_name, SCM_ARG2, "bse-glue-get-prop");

  BSE_SCM_DEFER_INTS ();

  proxy = SCM_GET_GLUE_PROXY (s_proxy);
  prop_name = strdup_from_scm (s_prop_name);
  bse_scm_enter_gc (&gclist, prop_name, g_free, STRING_LENGTH_FROM_SCM (s_prop_name));

  value = sfi_glue_proxy_get_property (proxy, prop_name);
  if (value)
    {
      s_retval = bse_scm_from_value (value);
      sfi_glue_gc_free_now ((void*) value, BseScmFreeFunc (sfi_value_free));
    }

  BSE_SCM_ALLOW_INTS ();

  bse_scm_destroy_gc_plateau (gcplateau);
  return s_retval;
}

SCM
bse_scm_glue_call (SCM s_proc_name,
		   SCM s_arg_list)
{
  SCM gcplateau = bse_scm_make_gc_plateau (4096);
  SCM gclist = SCM_EOL;
  SCM node, s_retval = SCM_UNSPECIFIED;
  gchar *proc_name;
  GValue *value;
  SfiSeq *seq;

  SCM_ASSERT (IS_SCM_STRING (s_proc_name),  s_proc_name,  SCM_ARG1, "bse-glue-call");
  SCM_ASSERT (IS_SCM_PAIR (s_arg_list) || s_arg_list == SCM_EOL,  s_arg_list,  SCM_ARG2, "bse-glue-call");

  BSE_SCM_DEFER_INTS ();

  proc_name = strdup_from_scm (s_proc_name);
  bse_scm_enter_gc (&gclist, proc_name, g_free, STRING_LENGTH_FROM_SCM (s_proc_name));

  seq = sfi_seq_new ();
  bse_scm_enter_gc (&gclist, seq, BseScmFreeFunc (sfi_seq_unref), 1024); // FIXME: GC callbacks may run in any thread
  for (node = s_arg_list; IS_SCM_PAIR (node); node = SCM_CDR (node))
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
      sfi_glue_gc_free_now (value, BseScmFreeFunc (sfi_value_free));
    }

  BSE_SCM_ALLOW_INTS ();

  bse_scm_destroy_gc_plateau (gcplateau);

  return s_retval;
}

typedef struct {
  gulong        proxy;
  gchar        *signal;
  SCM           s_lambda;
  guint         n_args;
  const GValue *args;
} SignalData;

static void
signal_data_free (gpointer  data,
                  GClosure *closure)
{
  SignalData *sdata = (SignalData*) data;
  SCM s_lambda = sdata->s_lambda;
  g_free (sdata->signal);
  g_free (sdata);
  scm_gc_unprotect_object (s_lambda);
}

static SCM
signal_marshal_sproc (void *data)
{
  SignalData *sdata = (SignalData*) data;
  SCM args = SCM_EOL;
  guint i;

  i = sdata->n_args;
  assert_return (sdata->n_args > 0, SCM_UNSPECIFIED);
  sdata->n_args = 0;

  while (i--)
    args = gh_cons (bse_scm_from_value (sdata->args + i), args);

  SCM s_ret = scm_apply (sdata->s_lambda, args, SCM_EOL);
  (void) s_ret;

  return SCM_UNSPECIFIED;
}

static void
signal_closure_marshal (GClosure       *closure,
                        GValue         *return_value,
                        guint           n_param_values,
                        const GValue   *param_values,
                        gpointer        invocation_hint,
                        gpointer        marshal_data)
{
  SCM_STACKITEM stack_item;
  SignalData *sdata = (SignalData*) closure->data;
  sdata->n_args = n_param_values;
  sdata->args = param_values;
  scm_internal_cwdr ((scm_t_catch_body) signal_marshal_sproc, sdata,
                     scm_handle_by_message_noexit, const_cast<char*> ("BSE"), &stack_item);
}

SCM
bse_scm_signal_connect (SCM s_proxy,
			SCM s_signal,
			SCM s_lambda)
{
  SfiProxy proxy;
  gulong id;
  SignalData *sdata;
  GClosure *closure;

  SCM_ASSERT (SCM_IS_GLUE_PROXY (s_proxy), s_proxy,  SCM_ARG1, "bse-signal-connect");
  proxy = SCM_GET_GLUE_PROXY (s_proxy);

  SCM_ASSERT (IS_SCM_STRING (s_signal), s_signal, SCM_ARG2, "bse-signal-connect");
  SCM_ASSERT (gh_procedure_p (s_lambda), s_lambda,  SCM_ARG3, "bse-signal-connect");

  scm_gc_protect_object (s_lambda);

  BSE_SCM_DEFER_INTS ();
  sdata = g_new0 (SignalData, 1);
  sdata->proxy = proxy;
  sdata->signal = strdup_from_scm (s_signal);
  sdata->s_lambda = s_lambda;
  closure = g_closure_new_simple (sizeof (GClosure), sdata);
  g_closure_add_finalize_notifier (closure, sdata, signal_data_free);
  g_closure_set_marshal (closure, signal_closure_marshal);
  id = sfi_glue_signal_connect_closure (proxy, sdata->signal, closure, NULL);
  BSE_SCM_ALLOW_INTS ();

  return gh_ulong2scm (id);
}

SCM
bse_scm_signal_disconnect (SCM s_proxy,
                           SCM s_handler_id)
{
  SfiProxy proxy;
  gulong id;

  SCM_ASSERT (SCM_IS_GLUE_PROXY (s_proxy), s_proxy,  SCM_ARG1, "bse-signal-disconnect");
  proxy = SCM_GET_GLUE_PROXY (s_proxy);

  id = scm_num2ulong (s_handler_id, SCM_ARG2, "bse-signal-disconnect");
  if (id < 1)
    scm_out_of_range ("bse-signal-disconnect", s_handler_id);

  sfi_glue_signal_disconnect (proxy, id);

  return SCM_UNSPECIFIED;
}

SCM
bse_scm_choice_match (SCM s_ch1,
                      SCM s_ch2)
{
  SCM_ASSERT (IS_SCM_SYMBOL (s_ch1), s_ch1, SCM_ARG1, "bse-choice-match?");
  SCM_ASSERT (IS_SCM_SYMBOL (s_ch2), s_ch2, SCM_ARG2, "bse-choice-match?");

  gchar *ch1 = strdup_from_scm (s_ch1);
  gchar *ch2 = strdup_from_scm (s_ch2);
  int res = sfi_choice_match (ch1, ch2);
  g_free (ch1);
  g_free (ch2);
  return SCM_BOOL (res);
}

static char*
text_concat_scm (const char *prefix,
                 SCM         s_string)
{
  char *p2 = strdup_from_scm (s_string);
  char *result = g_strconcat (prefix ? prefix : "", prefix && p2 ? "\n" : "", p2, NULL);
  g_free (p2);
  return result;
}

static int
msg_bit_type_match (const gchar *string)
{
  if (sfi_choice_match_detailed ("bse-msg-text0", string, TRUE) ||
      sfi_choice_match_detailed ("bse-msg-title", string, TRUE))
    return 0;
  if (sfi_choice_match_detailed ("bse-msg-text1", string, TRUE) ||
      sfi_choice_match_detailed ("bse-msg-primary", string, TRUE))
    return 1;
  if (sfi_choice_match_detailed ("bse-msg-text2", string, TRUE) ||
      sfi_choice_match_detailed ("bse-msg-secondary", string, TRUE))
    return 2;
  if (sfi_choice_match_detailed ("bse-msg-text3", string, TRUE) ||
      sfi_choice_match_detailed ("bse-msg-detail", string, TRUE))
    return 3;
  if (sfi_choice_match_detailed ("bse-msg-check", string, TRUE))
    return 4;
  return -1;
}

SCM
bse_scm_script_message (SCM s_type,
                        SCM s_bits)
{
  SCM gcplateau = bse_scm_make_gc_plateau (4096);

  SCM_ASSERT (IS_SCM_SYMBOL (s_type),   s_type,   SCM_ARG2, "bse-script-message");

  /* figure message level */
  BSE_SCM_DEFER_INTS();
  gchar *strtype = strdup_from_scm (s_type);
  Bse::String mtype = strtype ? strtype : "";
  g_free (strtype);
  BSE_SCM_ALLOW_INTS();
  if (!strtype)
    scm_wrong_type_arg ("bse-script-message", 2, s_type);

  /* figure argument list length */
  guint i = 0;
  SCM node = s_bits;
  while (IS_SCM_PAIR (node))
    node = SCM_CDR (node), i++;
  if (i == 0)
    scm_misc_error ("bse-script-message", "Wrong number of arguments", SCM_BOOL_F);

  /* build message bit list */
  char *title = NULL, *primary = NULL, *secondary = NULL, *detail = NULL, *check = NULL;
  i = 2;
  node = s_bits;
  while (IS_SCM_PAIR (node))
    {
      /* read first arg, a symbol */
      SCM arg1 = SCM_CAR (node);
      node = SCM_CDR (node);
      i++;
      if (!IS_SCM_SYMBOL (arg1))
	scm_wrong_type_arg ("bse-script-message", i, arg1);
      /* check symbol contents */
      BSE_SCM_DEFER_INTS();
      gchar *mtag = strdup_from_scm (arg1);
      int tag = msg_bit_type_match (mtag);
      g_free (mtag);
      BSE_SCM_ALLOW_INTS();
      if (tag < 0)
        scm_wrong_type_arg ("bse-script-message", i, arg1);
      /* list must continue */
      if (!IS_SCM_PAIR (node))
        scm_misc_error ("bse-script-message", "Wrong number of arguments", SCM_BOOL_F);
      /* read second arg, a string */
      SCM arg2 = SCM_CAR (node);
      node = SCM_CDR (node);
      i++;
      if (!IS_SCM_STRING (arg2))
        scm_wrong_type_arg ("bse-script-message", i, arg2);
      /* add message bit from string */
      BSE_SCM_DEFER_INTS();
      switch (tag)
        {
        case 0:
          title = text_concat_scm (title, arg2);
          sfi_glue_gc_add (title, g_free);
          break;
        case 1:
          primary = text_concat_scm (primary, arg2);
          sfi_glue_gc_add (primary, g_free);
          break;
        case 2:
          secondary = text_concat_scm (secondary, arg2);
          sfi_glue_gc_add (secondary, g_free);
          break;
        case 3:
          detail = text_concat_scm (detail, arg2);
          sfi_glue_gc_add (detail, g_free);
          break;
        case 4:
          check = text_concat_scm (check, arg2);
          sfi_glue_gc_add (check, g_free);
          break;
        }
      BSE_SCM_ALLOW_INTS();
    }

  BSE_SCM_DEFER_INTS ();
  SfiSeq *args = sfi_seq_new ();
  /* keep arguments in sync with bsejanitor.proc */
  sfi_seq_append_string (args, "BseScmShell");
  sfi_seq_append_string (args, mtype.c_str());
  sfi_seq_append_string (args, title);
  sfi_seq_append_string (args, primary);
  sfi_seq_append_string (args, secondary);
  sfi_seq_append_string (args, detail);
  sfi_seq_append_string (args, check);
  sfi_glue_call_seq ("bse-script-send-message", args);
  sfi_seq_unref (args);
  BSE_SCM_ALLOW_INTS ();

  bse_scm_destroy_gc_plateau (gcplateau);
  return SCM_UNSPECIFIED;
}

static gboolean script_register_enabled = FALSE;

void
bse_scm_enable_script_register (gboolean enabled)
{
  script_register_enabled = enabled != FALSE;
  if (script_register_enabled)
    {
      /* enable position recording wchih is required for __FILE__ and __LINE__ emulation */
      SCM_DEVAL_P = 1;
      SCM_BACKTRACE_P = 1;
      SCM_RECORD_POSITIONS_P = 1;
      SCM_RESET_DEBUG_MODE;
    }
}

SCM
bse_scm_script_register (SCM s_name,
			 SCM s_options,
			 SCM s_category,
			 SCM s_blurb,
			 SCM s_author,
			 SCM s_license,
			 SCM s_params)
{
  SCM gcplateau = bse_scm_make_gc_plateau (4096);
  SCM node;
  guint i;

  SCM_ASSERT (IS_SCM_SYMBOL (s_name),      s_name,      SCM_ARG1, "bse-script-register");
  SCM_ASSERT (IS_SCM_STRING (s_options),   s_options,   SCM_ARG2, "bse-script-register");
  SCM_ASSERT (IS_SCM_STRING (s_category),  s_category,  SCM_ARG3, "bse-script-register");
  SCM_ASSERT (IS_SCM_STRING (s_blurb),     s_blurb,     SCM_ARG4, "bse-script-register");
  SCM_ASSERT (IS_SCM_STRING (s_author),    s_author,    SCM_ARG5, "bse-script-register");
  SCM_ASSERT (IS_SCM_STRING (s_license),   s_license,   SCM_ARG6, "bse-script-register");
  for (node = s_params, i = 7; IS_SCM_PAIR (node); node = SCM_CDR (node), i++)
    {
      SCM arg = SCM_CAR (node);
      if (!IS_SCM_STRING (arg))
	scm_wrong_type_arg ("bse-script-register", i, arg);
    }


  // implement: (source-properties (frame-source (stack-ref (make-stack #t) 0)))
  // to get at the source properties 'file and 'line of the registration caller
  SCM s_stack = scm_make_stack (SCM_BOOL_T, SCM_EOL); // scm_cons (scm_int2num (4), SCM_EOL));
  SCM s_file = SCM_BOOL_F, s_line = SCM_BOOL_F;
  if (SCM_STACKP (s_stack))
    {
      SCM s_frame = scm_stack_ref (s_stack, scm_int2num (0));
      if (SCM_FRAMEP (s_frame))
        {
          SCM s_fsrc  = scm_frame_source (s_frame);
          s_file = scm_source_property (s_fsrc, scm_sym_filename);
          s_line = scm_source_property (s_fsrc, scm_sym_line);
        }
    }

  BSE_SCM_DEFER_INTS ();
  if (script_register_enabled)
    {
      SfiSeq *seq = sfi_seq_new ();
      GValue *val, *rval;

      sfi_seq_append (seq, val = string_value_from_scm (s_name));
      sfi_value_free (val);
      sfi_seq_append (seq, val = string_value_from_scm (s_options));
      sfi_value_free (val);
      sfi_seq_append (seq, val = string_value_from_scm (s_category));
      sfi_value_free (val);
      sfi_seq_append (seq, val = string_value_from_scm (s_blurb));
      sfi_value_free (val);
      if (IS_SCM_STRING (s_file))
        sfi_seq_append (seq, val = string_value_from_scm (s_file));
      else
        sfi_seq_append (seq, val = sfi_value_string ("Scheme"));
      sfi_value_free (val);
      std::string str = Bse::string_format ("%u", (int) (IS_SCM_SFI_NUM (s_line) ? num_from_scm (s_line) + 1 : 0));
      sfi_seq_append (seq, val = sfi_value_string (str.c_str()));
      sfi_value_free (val);
      sfi_seq_append (seq, val = string_value_from_scm (s_author));
      sfi_value_free (val);
      sfi_seq_append (seq, val = string_value_from_scm (s_license));
      sfi_value_free (val);

      for (node = s_params; IS_SCM_PAIR (node); node = SCM_CDR (node))
	{
	  SCM arg = SCM_CAR (node);
	  sfi_seq_append (seq, val = string_value_from_scm (arg));
	  sfi_value_free (val);
	}

      val = sfi_value_seq (seq);
      rval = sfi_glue_client_msg ("bse-client-msg-script-register", val);
      sfi_value_free (val);
      if (SFI_VALUE_HOLDS_STRING (rval))
	{
	  gchar *name = strdup_from_scm (s_name);
	  g_message ("while registering \"%s\": %s", name, sfi_value_get_string (rval));
	  g_free (name);
	}
      sfi_glue_gc_free_now (rval, BseScmFreeFunc (sfi_value_free));
    }
  BSE_SCM_ALLOW_INTS ();

  bse_scm_destroy_gc_plateau (gcplateau);
  return SCM_UNSPECIFIED;
}

SCM
bse_scm_gettext (SCM s_string)
{
  SCM_ASSERT (IS_SCM_STRING (s_string), s_string, SCM_ARG1, "bse-gettext");
  BSE_SCM_DEFER_INTS ();
  gchar *string = strdup_from_scm (s_string);
  const gchar *cstring = bse_gettext (string);
  SCM s_ret = scm_makfrom0str (cstring);
  g_free (string);
  BSE_SCM_ALLOW_INTS ();
  return s_ret;
}

SCM
bse_scm_gettext_q (SCM s_string)
{
  SCM_ASSERT (IS_SCM_STRING (s_string), s_string, SCM_ARG1, "bse-gettext-q");
  BSE_SCM_DEFER_INTS ();
  gchar *string = strdup_from_scm (s_string);
  const gchar *cstring = bse_gettext (string);
  if (string == cstring)
    {
      const gchar *c = strchr (cstring, '|');
      cstring = c ? c + 1 : cstring;
    }
  SCM s_ret = scm_makfrom0str (cstring);
  g_free (string);
  BSE_SCM_ALLOW_INTS ();
  return s_ret;
}

static SCM
bse_scm_script_args (void)
{
  SCM gcplateau = bse_scm_make_gc_plateau (4096);
  SCM args;
  GValue *rvalue;

  BSE_SCM_DEFER_INTS ();
  rvalue = sfi_glue_client_msg ("bse-client-msg-script-args", NULL);
  if (SFI_VALUE_HOLDS_SEQ (rvalue))
    args = bse_scm_from_value (rvalue);
  else
    args = SCM_EOL;
  sfi_glue_gc_free_now (rvalue, BseScmFreeFunc (sfi_value_free));
  BSE_SCM_ALLOW_INTS ();

  bse_scm_destroy_gc_plateau (gcplateau);
  return args;
}

SCM
bse_scm_context_pending (void)
{
  gboolean pending;

  BSE_SCM_DEFER_INTS ();
  pending = sfi_glue_context_pending ();
  BSE_SCM_ALLOW_INTS ();

  return gh_bool2scm (pending);
}

SCM
bse_scm_context_iteration (SCM s_may_block)
{
  BSE_SCM_DEFER_INTS ();
  if (sfi_glue_context_pending ())
    sfi_glue_context_dispatch ();
  else if (gh_scm2bool (s_may_block))
    {
      do
        {
          BSE_SCM_ALLOW_INTS ();
          g_main_context_iteration (g_main_context_default(), TRUE);
          BSE_SCM_DEFER_INTS ();
        }
      while (!sfi_glue_context_pending ());
      sfi_glue_context_dispatch ();
    }
  BSE_SCM_ALLOW_INTS ();
  return SCM_UNSPECIFIED;
}


/* --- initialization --- */
typedef SCM (*BseScmFunc) ();

static void
register_types (const gchar **types)
{
  SCM gcplateau = bse_scm_make_gc_plateau (2048);

  while (*types)
    {
      const gchar **names = sfi_glue_list_method_names (*types);
      gchar *sname = g_type_name_to_sname (*types);
      gchar *s;
      guint i;

      if (strncmp (sname, "bse-", 4) == 0)
	{
	  s = g_strdup_format ("(define (bse-is-%s proxy) (bse-item-check-is-a proxy \"%s\"))",
			       sname + 4, *types);
	  gh_eval_str (s);
	  g_free (s);
	}
      for (i = 0; names[i]; i++)
	{
	  gchar *s = g_strdup_format ("(define %s-%s (lambda list (bse-glue-call \"%s+%s\" list)))",
				      sname, names[i], *types, names[i]);
	  gh_eval_str (s);
	  g_free (s);
	}
      g_free (sname);

      names = sfi_glue_iface_children (*types);
      register_types (names);

      types++;
    }
  bse_scm_destroy_gc_plateau (gcplateau);
}

void
bse_scm_interp_init (void)
{
  const gchar **procs, *procs2[2];
  guint i;

  tc_glue_gc_cell = scm_make_smob_type ("BseScmGCCell", 0);
  scm_set_smob_mark (tc_glue_gc_cell, bse_scm_mark_gc_cell);
  scm_set_smob_free (tc_glue_gc_cell, bse_scm_free_gc_cell);

  tc_glue_gc_plateau = scm_make_smob_type ("BseScmGcPlateau", 0);
  scm_set_smob_free (tc_glue_gc_plateau, bse_scm_gc_plateau_free);

  tc_glue_rec = scm_make_smob_type ("BseGlueRec", 0);
  scm_set_smob_free (tc_glue_rec, bse_scm_free_glue_rec);
  gh_new_procedure ("bse-rec-get", BseScmFunc (bse_scm_glue_rec_get), 2, 0, 0);
  gh_new_procedure ("bse-rec-set", BseScmFunc (bse_scm_glue_rec_set), 3, 0, 0);
  gh_new_procedure ("bse-rec-new", BseScmFunc (bse_scm_glue_rec_new), 0, 1, 0);
  gh_new_procedure ("bse-rec-print", BseScmFunc (bse_scm_glue_rec_print), 1, 0, 0);

  tc_glue_proxy = scm_make_smob_type ("SfiProxy", 0);
  SCM_NEWSMOB (glue_null_proxy, tc_glue_proxy, 0);
  scm_permanent_object (glue_null_proxy);
  scm_set_smob_equalp (tc_glue_proxy, bse_scm_proxy_equalp);
  scm_set_smob_print (tc_glue_proxy, bse_scm_proxy_print);
  gh_new_procedure ("bse-proxy-is-null?", BseScmFunc (bse_scm_proxy_is_null), 1, 0, 0);
  gh_new_procedure ("bse-proxy-get-null", BseScmFunc (bse_scm_proxy_get_null), 0, 1, 0);

  gh_new_procedure ("bse-glue-call", BseScmFunc (bse_scm_glue_call), 2, 0, 0);
  gh_new_procedure ("bse-glue-set-prop", BseScmFunc (bse_scm_glue_set_prop), 3, 0, 0);
  gh_new_procedure ("bse-glue-get-prop", BseScmFunc (bse_scm_glue_get_prop), 2, 0, 0);

  procs = sfi_glue_list_proc_names ();
  for (i = 0; procs[i]; i++)
    if (strncmp (procs[i], "bse-", 4) == 0)
      {
	gchar *s = g_strdup_format ("(define bse-%s (lambda list (bse-glue-call \"%s\" list)))", procs[i] + 4, procs[i]);
	gh_eval_str (s);
	g_free (s);
      }

  procs2[0] = sfi_glue_base_iface ();
  procs2[1] = NULL;
  register_types (procs2);

  gh_new_procedure0_0 ("bse-server-get", bse_scm_server_get);
  gh_new_procedure ("bse-script-register", BseScmFunc (bse_scm_script_register), 6, 0, 1);
  gh_new_procedure ("bse-script-fetch-args", BseScmFunc (bse_scm_script_args), 0, 0, 0);
  gh_new_procedure ("bse-choice-match?", BseScmFunc (bse_scm_choice_match), 2, 0, 0);
  gh_new_procedure ("bse-signal-connect", BseScmFunc (bse_scm_signal_connect), 3, 0, 0);
  gh_new_procedure ("bse-signal-disconnect", BseScmFunc (bse_scm_signal_disconnect), 2, 0, 0);
  gh_new_procedure ("bse-context-pending", BseScmFunc (bse_scm_context_pending), 0, 0, 0);
  gh_new_procedure ("bse-context-iteration", BseScmFunc (bse_scm_context_iteration), 1, 0, 0);
  gh_new_procedure ("bse-script-message", BseScmFunc (bse_scm_script_message), 1, 0, 1);
  gh_new_procedure ("bse-gettext", BseScmFunc (bse_scm_gettext), 1, 0, 0);
  gh_new_procedure ("bse-gettext-q", BseScmFunc (bse_scm_gettext_q), 1, 0, 0);
}
