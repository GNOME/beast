// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#include "bsecategories.hh"
#include "bsesource.hh"
#include "bseprocedure.hh"
#include "bsemain.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

static gchar *indent_inc = NULL;
static guint spacing = 1;
static FILE *f_out = NULL;
static GType root = 0;
static gboolean recursion = TRUE;
static gboolean feature_blurb = FALSE;
static gboolean feature_channels = FALSE;

/*
  #define	O_SPACE	"\\as"
  #define	O_ESPACE " "
  #define	O_BRANCH "\\aE"
  #define	O_VLINE "\\al"
  #define	O_LLEAF	"\\aL"
  #define	O_KEY_FILL "_"
*/

#define	O_SPACE	" "
#define	O_ESPACE ""
#define	O_BRANCH "+"
#define	O_VLINE "|"
#define	O_LLEAF	"`"
#define	O_KEY_FILL "_"

static void
show_nodes (GType        type,
	    GType        sibling,
	    const gchar *indent)
{
  GType   *children;
  guint i;

  if (!type)
    return;

  children = g_type_children (type, NULL);

  if (type != root)
    for (i = 0; i < spacing; i++)
      fprintf (f_out, "%s%s\n", indent, O_VLINE);

  fprintf (f_out, "%s%s%s%s",
	   indent,
	   sibling ? O_BRANCH : (type != root ? O_LLEAF : O_SPACE),
	   O_ESPACE,
	   g_type_name (type));

  for (i = strlen (g_type_name (type)); i <= strlen (indent_inc); i++)
    fputs (O_KEY_FILL, f_out);

  if (feature_blurb && bse_type_get_blurb (type))
    {
      fputs ("\t[", f_out);
      fputs (bse_type_get_blurb (type), f_out);
      fputs ("]", f_out);
    }

  if (G_TYPE_IS_ABSTRACT (type))
    fputs ("\t(abstract)", f_out);

  if (feature_channels && g_type_is_a (type, BSE_TYPE_SOURCE))
    {
      BseSourceClass *klass = (BseSourceClass*) g_type_class_ref (type);
      gchar buffer[1024];

      sprintf (buffer,
	       "\t(ichannels %u) (ochannels %u)",
	       klass->channel_defs.n_ichannels,
	       klass->channel_defs.n_ochannels);
      fputs (buffer, f_out);
      g_type_class_unref (klass);
    }

  fputc ('\n', f_out);

  if (children && recursion)
    {
      gchar *new_indent;
      GType   *child;

      if (sibling)
	new_indent = g_strconcat (indent, O_VLINE, indent_inc, NULL);
      else
	new_indent = g_strconcat (indent, O_SPACE, indent_inc, NULL);

      for (child = children; *child; child++)
	show_nodes (child[0], child[1], new_indent);

      g_free (new_indent);
    }

  g_free (children);
}

static void
show_procdoc (void)
{
  BseCategorySeq *cseq;
  guint i;
  const gchar *nullstr = ""; // "???";

  cseq = bse_categories_match_typed ("*", BSE_TYPE_PROCEDURE);
  for (i = 0; i < cseq->n_cats; i++)
    {
      GType type = g_type_from_name (cseq->cats[i]->type);
      BseProcedureClass *klass = (BseProcedureClass*) g_type_class_ref (type);
      gchar *pname = g_type_name_to_sname (cseq->cats[i]->type);
      const gchar *blurb = bse_type_get_blurb (type);
      guint j;

      fprintf (f_out, "/**\n * %s\n", pname);
      for (j = 0; j < klass->n_in_pspecs; j++)
	{
	  GParamSpec *pspec = G_PARAM_SPEC (klass->in_pspecs[j]);

	  fprintf (f_out, " * @%s: %s\n",
		   pspec->name,
		   g_param_spec_get_blurb (pspec) ? g_param_spec_get_blurb (pspec) : nullstr);
	}
      for (j = 0; j < klass->n_out_pspecs; j++)
	{
	  GParamSpec *pspec = G_PARAM_SPEC (klass->out_pspecs[j]);

	  fprintf (f_out, " * @Returns: %s: %s\n",
		   pspec->name,
		   g_param_spec_get_blurb (pspec) ? g_param_spec_get_blurb (pspec) : nullstr);
	}
      if (blurb)
	fprintf (f_out, " * %s\n", blurb);
      fprintf (f_out, " **/\n");
      g_type_class_unref (klass);
      g_free (pname);
    }
  bse_category_seq_free (cseq);
}

static gint
help (gchar *arg)
{
  fprintf (stderr, "usage: query <qualifier> [-r <type>] [-{i|b} \"\"] [-s #] [-{h|x|y}]\n");
  fprintf (stderr, "       -r       specifiy root type\n");
  fprintf (stderr, "       -n       don't descend type tree\n");
  fprintf (stderr, "       -p       include plugins\n");
  fprintf (stderr, "       -x       show type blurbs\n");
  fprintf (stderr, "       -y       show source channels\n");
  fprintf (stderr, "       -h       guess what ;)\n");
  fprintf (stderr, "       -b       specifiy indent string\n");
  fprintf (stderr, "       -i       specifiy incremental indent string\n");
  fprintf (stderr, "       -s       specifiy line spacing\n");
  fprintf (stderr, "       -:f      make all warnings fatal\n");
  fprintf (stderr, "qualifiers:\n");
  fprintf (stderr, "       froots     iterate over fundamental roots\n");
  fprintf (stderr, "       tree       print BSE type tree\n");
  fprintf (stderr, "       procdoc    print procedure documentation\n");

  return arg != NULL;
}

int
main (gint   argc,
      gchar *argv[])
{
  gboolean gen_froots = 0;
  gboolean gen_tree = 0;
  gboolean gen_procdoc = 0;
  gchar *root_name = NULL;
  const char *iindent = "";
  const char *pluginbool = "load-core-plugins=0";
  const char *scriptbool = "load-core-scripts=0";
  f_out = stdout;

  bse_init_inprocess (&argc, argv, "BseQuery", Bse::cstrings_to_vector (pluginbool, scriptbool, NULL));
  // bse_init_test (&argc, argv);
  int i;
  for (i = 1; i < argc; i++)
    {
      if (strcmp ("-s", argv[i]) == 0)
	{
	  i++;
	  if (i < argc)
	    spacing = atoi (argv[i]);
	}
      else if (strcmp ("-i", argv[i]) == 0)
	{
	  i++;
	  if (i < argc)
	    {
	      char *p;
	      guint n;

	      p = argv[i];
	      while (*p)
		p++;
	      n = p - argv[i];
	      indent_inc = g_new (gchar, n * strlen (O_SPACE) + 1);
	      *indent_inc = 0;
	      while (n)
		{
		  n--;
		  strcpy (indent_inc, O_SPACE);
		}
	    }
	}
      else if (strcmp ("-b", argv[i]) == 0)
	{
	  i++;
	  if (i < argc)
	    iindent = argv[i];
	}
      else if (strcmp ("-r", argv[i]) == 0)
	{
	  i++;
	  if (i < argc)
	    root_name = argv[i];
	}
      else if (strcmp ("-n", argv[i]) == 0)
	{
	  recursion = FALSE;
	}
      else if (strcmp ("-x", argv[i]) == 0)
	{
	  feature_blurb = TRUE;
	}
      else if (strcmp ("-y", argv[i]) == 0)
	{
	  feature_channels = TRUE;
	}
      else if (strcmp ("froots", argv[i]) == 0)
	{
	  gen_froots = 1;
	}
      else if (strcmp ("tree", argv[i]) == 0)
	{
	  gen_tree = 1;
	}
      else if (strcmp ("procdoc", argv[i]) == 0)
	{
	  gen_procdoc = 1;
	}
      else if (strcmp ("-p", argv[i]) == 0)
        pluginbool = "load-core-plugins=1";
      else if (strcmp ("-:f", argv[i]) == 0)
	{
	  g_log_set_always_fatal (GLogLevelFlags (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL |
                                                  g_log_set_always_fatal (GLogLevelFlags (G_LOG_FATAL_MASK))));
	}
      else if (strcmp ("-h", argv[i]) == 0)
	{
	  return help (NULL);
	}
      else if (strcmp ("--help", argv[i]) == 0)
	{
	  return help (NULL);
	}
      else
	return help (argv[i]);
    }
  // bse_init_inprocess (&argc, argv, "BseQuery", Bse::cstrings_to_vector (pluginbool, scriptbool, NULL));
  if (root_name)
    root = g_type_from_name (root_name);
  else
    root = BSE_TYPE_OBJECT;
  if (!gen_froots && !gen_tree && !gen_procdoc)
    return help (argv[i-1]);
  if (!indent_inc)
    {
      indent_inc = g_new (gchar, strlen (O_SPACE) + 1);
      *indent_inc = 0;
      strcpy (indent_inc, O_SPACE);
      strcpy (indent_inc, O_SPACE);
      strcpy (indent_inc, O_SPACE);
    }

  if (gen_tree)
    show_nodes (root, 0, iindent);
  if (gen_froots)
    {
      root = ~0;
      for (i = 0; i <= G_TYPE_FUNDAMENTAL_MAX; i += G_TYPE_MAKE_FUNDAMENTAL (1))
	{
	  const char *name = g_type_name (i);
	  if (name)
	    show_nodes (i, 0, iindent);
	}
    }
  if (gen_procdoc)
    show_procdoc ();

  return 0;
}
