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
#include <stdio.h>      /* sscanf() */
#include "sfiserial.h"
#include "sfiparams.h"
#include "sfitime.h"
#include "sfinote.h"


/* --- parsing aids --- */
#define parse_or_return(scanner, token)  G_STMT_START{ \
  GScanner *__s = (scanner); guint _t = (token); \
  if (g_scanner_get_next_token (__s) != _t) \
    return _t; \
}G_STMT_END
#define peek_or_return(scanner, token)   G_STMT_START{ \
  GScanner *__s = (scanner); guint _t = (token); \
  if (g_scanner_peek_next_token (__s) != _t) { \
    g_scanner_get_next_token (__s); /* advance position for error-handler */ \
    return _t; \
  } \
}G_STMT_END
#define	NULL_STRING	"#f"
static inline gboolean
check_parse_null (GScanner *scanner)
{
  if (scanner->token == '#' && g_scanner_peek_next_token (scanner) == 'f')
    {
      g_scanner_get_next_token (scanner);
      return TRUE;
    }
  else
    return FALSE;
}
static GTokenType
scanner_skip_statement (GScanner *scanner,
			guint     level)
{
  while (level)
    {
      g_scanner_get_next_token (scanner);
      if (scanner->token == G_TOKEN_EOF ||
	  scanner->token == G_TOKEN_ERROR)
	return ')';
      if (scanner->token == '(')
	level++;
      else if (scanner->token == ')')
	level--;
    }
  return G_TOKEN_NONE;
}


/* --- storage helpers --- */
#define	gstring_puts(gstring, string)	g_string_append (gstring, string)
#define	gstring_putc(gstring, vchar)	g_string_append_c (gstring, vchar)
#define	gstring_printf			g_string_append_printf
static void
gstring_break (GString  *gstring,
	       gboolean *needs_break,
	       guint     indent)
{
  gchar *s = g_new (gchar, indent + 1);
  memset (s, ' ', indent);
  s[indent] = 0;
  gstring_putc (gstring, '\n');
  gstring_puts (gstring, s);
  g_free (s);
  *needs_break = FALSE;
}
static void
gstring_check_break (GString  *gstring,
		     gboolean *needs_break,
		     guint     indent)
{
  if (*needs_break)
    gstring_break (gstring, needs_break, indent);
}


/* --- functions --- */
static GTokenType
sfi_scanner_parse_real_num (GScanner *scanner,
			    SfiReal  *real_p,
			    SfiNum   *num_p)
{
  gboolean negate = FALSE;
  gdouble vdouble;
  guint64 ui64;

  g_scanner_get_next_token (scanner);
  if (scanner->token == '-')
    {
      negate = TRUE;
      g_scanner_get_next_token (scanner);
    }
  if (scanner->token == G_TOKEN_INT)
    {
      ui64 = scanner->value.v_int64;
      vdouble = ui64;
    }
  else if (scanner->token == G_TOKEN_FLOAT)
    {
      vdouble = scanner->value.v_float;
      ui64 = vdouble;
    }
  else
    return G_TOKEN_INT;
  if (num_p)
    {
      *num_p = ui64;
      if (negate)
	*num_p = - *num_p;
    }
  if (real_p)
    *real_p = negate ? -vdouble : vdouble;
  return G_TOKEN_NONE;
}


/* --- next generation serialization --- */
static GTokenType
sfi_serialize_primitives (SfiSCategory scat,
			  GValue      *value,
			  GString     *gstring,
			  GScanner    *scanner)
{
  switch (scat)
    {
    case SFI_SCAT_BOOL:
      if (gstring)
	{
	  gstring_puts (gstring, sfi_value_get_bool (value) ? "#t" : "#f");
	}
      else
	{
	  gboolean v_bool = FALSE;
	  g_scanner_get_next_token (scanner);
	  if (scanner->token == G_TOKEN_INT && scanner->value.v_int64 <= 1)
	    v_bool = scanner->value.v_int64 != 0;
	  else if (scanner->token == '#')
	    {
	      g_scanner_get_next_token (scanner);
	      if (scanner->token == 't' || scanner->token == 'T')
		v_bool = TRUE;
	      else if (scanner->token == 'f' || scanner->token == 'F')
		v_bool = FALSE;
	      else
		return 'f';
	    }
	  else
	    return '#';
	  sfi_value_set_bool (value, v_bool);
	}
      break;
    case SFI_SCAT_INT:
      if (gstring)
	{
	  gstring_printf (gstring, "%d", sfi_value_get_int (value));
	}
      else
	{
	  SfiNum num;
	  GTokenType token = sfi_scanner_parse_real_num (scanner, NULL, &num);
	  if (token != G_TOKEN_NONE)
	    return token;
	  sfi_value_set_int (value, num);
	}
      break;
    case SFI_SCAT_NUM:
      if (gstring)
	{
	  SfiNum num = sfi_value_get_num (value);
	  gstring_printf (gstring, "%lld", num);
	}
      else
	{
	  SfiNum num;
	  GTokenType token = sfi_scanner_parse_real_num (scanner, NULL, &num);
	  if (token != G_TOKEN_NONE)
	    return token;
	  sfi_value_set_num (value, num);
	}
      break;
    case SFI_SCAT_REAL:
      if (gstring)
	{
	  gstring_printf (gstring, "%.18g", sfi_value_get_real (value));
	}
      else
	{
	  SfiReal real;
	  GTokenType token = sfi_scanner_parse_real_num (scanner, &real, NULL);
	  if (token != G_TOKEN_NONE)
	    return token;
	  sfi_value_set_real (value, real);
	}
      break;
    case SFI_SCAT_STRING:
      if (gstring)
	{
	  gchar *cstring = sfi_value_get_string (value);
	  if (cstring)
	    {
	      gchar *string = g_strescape (cstring, NULL);
	      gstring_putc (gstring, '"');
	      gstring_puts (gstring, string);
	      gstring_putc (gstring, '"');
	      g_free (string);
	    }
	  else
	    gstring_puts (gstring, NULL_STRING);
	}
      else
	{
	  g_scanner_get_next_token (scanner);
	  if (check_parse_null (scanner))
	    sfi_value_set_string (value, NULL);
	  else if (scanner->token == G_TOKEN_STRING)
	    sfi_value_set_string (value, scanner->value.v_string);
	  else
	    return G_TOKEN_STRING;
	}
      break;
    case SFI_SCAT_CHOICE:
      if (gstring)
	{
	  gchar *cstring = sfi_value_get_string (value);
	  if (!cstring)
	    gstring_puts (gstring, NULL_STRING);
	  else
	    gstring_printf (gstring, "%s", cstring);
	}
      else
	{
	  g_scanner_get_next_token (scanner);
	  if (check_parse_null (scanner))
	    sfi_value_set_choice (value, NULL);
	  else if (scanner->token == G_TOKEN_IDENTIFIER)
	    sfi_value_set_choice (value, scanner->value.v_identifier);
	  else
	    return G_TOKEN_IDENTIFIER;
	}
      break;
    case SFI_SCAT_PROXY:
      if (gstring)
	{
	  SfiProxy proxy = sfi_value_get_proxy (value);
	  gstring_printf (gstring, "%lu", proxy);
	}
      else
	{
	  SfiNum num;
	  GTokenType token = sfi_scanner_parse_real_num (scanner, NULL, &num);
          if (token != G_TOKEN_NONE)
	    return token;
	  sfi_value_set_proxy (value, num);
	}
      break;
    case SFI_SCAT_BBLOCK:
      if (gstring)
	{
	  SfiBBlock *bblock = sfi_value_get_bblock (value);
	  if (!bblock)
	    gstring_puts (gstring, NULL_STRING);
	  else
	    {
	      guint i;
	      gstring_puts (gstring, "(");
	      if (bblock->n_bytes)
		gstring_printf (gstring, "%u", bblock->bytes[0]);
	      for (i = 1; i < bblock->n_bytes; i++)
		gstring_printf (gstring, " %u", bblock->bytes[i]);
	      gstring_puts (gstring, ")");
	    }
	}
      else
	{
	  g_scanner_get_next_token (scanner);
	  if (check_parse_null (scanner))
	    sfi_value_set_bblock (value, NULL);
	  else if (scanner->token == '(')
	    {
	      SfiBBlock *bblock;
	      bblock = sfi_bblock_new ();
	      sfi_value_set_bblock (value, bblock);
	      sfi_bblock_unref (bblock);
	      while (g_scanner_get_next_token (scanner) != ')')
		if (scanner->token == G_TOKEN_INT)
		  sfi_bblock_append1 (bblock, scanner->value.v_int64);
		else
		  return G_TOKEN_INT;
	    }
          else
	    return '(';
	}
      break;
    case SFI_SCAT_FBLOCK:
      if (gstring)
	{
	  SfiFBlock *fblock = sfi_value_get_fblock (value);
	  if (!fblock)
	    gstring_puts (gstring, NULL_STRING);
	  else
	    {
	      guint i;
	      gstring_puts (gstring, "(");
	      if (fblock->n_values)
		gstring_printf (gstring, "%.9g", fblock->values[0]);
	      for (i = 1; i < fblock->n_values; i++)
		gstring_printf (gstring, " %.9g", fblock->values[i]);
	      gstring_puts (gstring, ")");
	    }
	}
      else
	{
	  g_scanner_get_next_token (scanner);
	  if (check_parse_null (scanner))
	    sfi_value_set_fblock (value, NULL);
	  else if (scanner->token == '(')
	    {
	      SfiFBlock *fblock;
	      GTokenType token;
	      SfiReal real;
	      fblock = sfi_fblock_new ();
	      sfi_value_set_fblock (value, fblock);
	      sfi_fblock_unref (fblock);
              while (g_scanner_peek_next_token (scanner) != ')')
		{
		  token = sfi_scanner_parse_real_num (scanner, &real, NULL);
		  if (token != G_TOKEN_NONE)
		    return G_TOKEN_FLOAT;
		  sfi_fblock_append1 (fblock, real);
		}
	      parse_or_return (scanner, ')');
	    }
	  else
	    return '(';
	}
      break;
    case SFI_SCAT_NOTE:
      if (gstring)
	{
	  gchar *string = sfi_note_to_string (sfi_value_get_int (value));
	  gstring_printf (gstring, "%s", string);
	  g_free (string);
	}
      else
	{
	  gchar *error = NULL;
	  SfiNum num;
	  if (g_scanner_peek_next_token (scanner) == G_TOKEN_STRING) // FIXME: deprecated syntax
	    {
	      g_scanner_get_next_token (scanner);
	      g_scanner_warn (scanner, "deprecated syntax: encountered string instead of note symbol");
	    }
	  else
	    {
	      parse_or_return (scanner, G_TOKEN_IDENTIFIER);
	    }
	  num = sfi_note_from_string_err (scanner->value.v_identifier, &error);
	  if (error)
	    g_scanner_warn (scanner, "%s", error);
	  g_free (error);
	  sfi_value_set_int (value, num);
	}
      break;
    case SFI_SCAT_TIME:
      if (gstring)
	{
	  gchar *string = sfi_time_to_string (sfi_time_to_utc (sfi_value_get_num (value)));
	  gstring_printf (gstring, "\"%s\"", string);
	  g_free (string);
	}
      else
	{
	  SfiTime ustime;
	  gchar *error = NULL;
	  parse_or_return (scanner, G_TOKEN_STRING);
	  ustime = sfi_time_from_string_err (scanner->value.v_string, &error);
	  if (error)
	    g_scanner_warn (scanner, "%s", error);
	  g_free (error);
	  if (ustime < 1)
	    ustime = SFI_MIN_TIME;
	  sfi_value_set_num (value, sfi_time_from_utc (ustime));
	}
      break;
    default:
      if (gstring)
	g_error ("%s: unimplemented category (%u)", G_STRLOC, scat);
      else
	{
	  g_scanner_warn (scanner, "unimplemented category (%u)", scat);
	  return G_TOKEN_ERROR;
	}
    }
  return G_TOKEN_NONE;
}

void
sfi_value_store_typed (const GValue *value,
		       GString      *gstring)
{
  SfiSCategory scat;
  
  g_return_if_fail (G_IS_VALUE (value));
  g_return_if_fail (gstring != NULL);

  scat = sfi_categorize_type (G_VALUE_TYPE (value)) & SFI_SCAT_TYPE_MASK;
  switch (scat)
    {
      SfiSeq *seq;
      SfiRec *rec;
    case SFI_SCAT_BOOL:
    case SFI_SCAT_INT:
    case SFI_SCAT_NUM:
    case SFI_SCAT_REAL:
    case SFI_SCAT_STRING:
    case SFI_SCAT_CHOICE:
    case SFI_SCAT_PROXY:
    case SFI_SCAT_BBLOCK:
    case SFI_SCAT_FBLOCK:
      gstring_printf (gstring, "(%c ", scat);
      sfi_serialize_primitives (scat, (GValue*) value, gstring, NULL);
      gstring_putc (gstring, ')');
      break;
    case SFI_SCAT_SEQ:
      gstring_printf (gstring, "(%c", scat);
      seq = sfi_value_get_seq (value);
      if (!seq)
	gstring_puts (gstring, " " NULL_STRING);
      else
	{
	  guint i;
	  gstring_puts (gstring, " (");
	  for (i = 0; i < seq->n_elements; i++)
	    {
	      if (i)
		gstring_putc (gstring, ' ');
	      sfi_value_store_typed (seq->elements + i, gstring);
	    }
	  gstring_putc (gstring, ')');
	}
      gstring_putc (gstring, ')');
      break;
    case SFI_SCAT_REC:
      gstring_printf (gstring, "(%c", scat);
      rec = sfi_value_get_rec (value);
      if (!rec)
	gstring_puts (gstring, " " NULL_STRING);
      else
	{
	  guint i;
	  sfi_rec_sort (rec);
          gstring_puts (gstring, " (");
	  for (i = 0; i < rec->n_fields; i++)
	    {
	      if (i)
		gstring_putc (gstring, ' ');
	      gstring_printf (gstring, "(%s ", rec->field_names[i]);
	      sfi_value_store_typed (rec->fields + i, gstring);
	      gstring_putc (gstring, ')');
	    }
          gstring_putc (gstring, ')');
	}
      gstring_putc (gstring, ')');
      break;
    default:
      g_error ("%s: unimplemented category (%u)", G_STRLOC, scat);
    }
}

GTokenType
sfi_value_parse_typed (GValue   *value,
		       GScanner *scanner)
{
  SfiSCategory scat;

  g_return_val_if_fail (value != NULL && G_VALUE_TYPE (value) == 0, G_TOKEN_ERROR);
  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);

  parse_or_return (scanner, '(');
  scat = g_scanner_get_next_token (scanner);
  if (!((scat >= 'a' && scat <= 'z') ||
	(scat >= 'A' && scat <= 'Z')))
    return G_TOKEN_IDENTIFIER;
  switch (scat)
    {
      GTokenType token;
    case SFI_SCAT_BOOL:
    case SFI_SCAT_INT:
    case SFI_SCAT_NUM:
    case SFI_SCAT_REAL:
    case SFI_SCAT_STRING:
    case SFI_SCAT_CHOICE:
    case SFI_SCAT_PROXY:
    case SFI_SCAT_BBLOCK:
    case SFI_SCAT_FBLOCK:
      g_value_init (value, sfi_category_type (scat));
      token = sfi_serialize_primitives (scat, value, NULL, scanner);
      if (token != G_TOKEN_NONE)
	return token;
      parse_or_return (scanner, ')');
      break;
    case SFI_SCAT_SEQ:
      g_value_init (value, SFI_TYPE_SEQ);
      g_scanner_get_next_token (scanner);
      if (check_parse_null (scanner))
	sfi_value_set_seq (value, NULL);
      else if (scanner->token == '(')
	{
	  SfiSeq *seq;
	  seq = sfi_seq_new ();
	  sfi_value_set_seq (value, seq);
	  sfi_seq_unref (seq);
	  while (g_scanner_peek_next_token (scanner) != ')')
	    {
	      GValue *evalue = sfi_value_empty ();
	      token = sfi_value_parse_typed (evalue, scanner);
	      if (token != G_TOKEN_NONE)
		{
		  sfi_value_free (evalue);
		  return token;
		}
	      sfi_seq_append (seq, evalue);
	      sfi_value_free (evalue);
	    }
	  parse_or_return (scanner, ')');
	}
      else
	return '(';
      parse_or_return (scanner, ')');
      break;
    case SFI_SCAT_REC:
      g_value_init (value, SFI_TYPE_REC);
      g_scanner_get_next_token (scanner);
      if (check_parse_null (scanner))
	sfi_value_set_rec (value, NULL);
      else if (scanner->token == '(')
	{
	  SfiRec *rec;
	  rec = sfi_rec_new ();
	  sfi_value_set_rec (value, rec);
	  sfi_rec_unref (rec);
	  while (g_scanner_peek_next_token (scanner) != ')')
	    {
	      GValue *fvalue;
	      gchar *field_name;
	      parse_or_return (scanner, '(');
	      parse_or_return (scanner, G_TOKEN_IDENTIFIER);
	      field_name = g_strdup (scanner->value.v_identifier);
	      fvalue = sfi_value_empty ();
	      token = sfi_value_parse_typed (fvalue, scanner);
	      if (token != G_TOKEN_NONE || g_scanner_peek_next_token (scanner) != ')')
		{
 		  g_free (field_name);
		  sfi_value_free (fvalue);
		  if (token == G_TOKEN_NONE)
		    {
		      g_scanner_get_next_token (scanner);	/* eat ')' */
		      token = ')';
		    }
		  return token;
		}
	      g_scanner_get_next_token (scanner);	/* eat ')' */
	      sfi_rec_set (rec, field_name, fvalue);
	      g_free (field_name);
	      sfi_value_free (fvalue);
	    }
	  parse_or_return (scanner, ')');
	}
      else
	return '(';
      parse_or_return (scanner, ')');
      break;
    default:
      g_scanner_warn (scanner, "skipping value of unknown category `%c'", scat);
      return scanner_skip_statement (scanner, 1);
    }
  return G_TOKEN_NONE;
}

static void
value_store_param (const GValue *value,
		   GString      *gstring,
		   gboolean     *needs_break,
		   GParamSpec   *pspec,
		   guint         indent)
{
  SfiSCategory scat = sfi_categorize_pspec (pspec);

  gstring_check_break (gstring, needs_break, indent);
  switch (scat)
    {
      SfiSeq *seq;
      SfiRec *rec;
    case SFI_SCAT_BOOL:
    case SFI_SCAT_INT:
    case SFI_SCAT_NUM:
    case SFI_SCAT_REAL:
    case SFI_SCAT_STRING:
    case SFI_SCAT_CHOICE:
    case SFI_SCAT_PROXY:
    case SFI_SCAT_BBLOCK:
    case SFI_SCAT_FBLOCK:
    case SFI_SCAT_NOTE:
    case SFI_SCAT_TIME:
      sfi_serialize_primitives (scat, (GValue*) value, gstring, NULL);
      break;
    case SFI_SCAT_SEQ:
      seq = sfi_value_get_seq (value);
      if (!seq)
	gstring_puts (gstring, NULL_STRING);
      else
	{
          GParamSpec *espec = sfi_pspec_get_seq_element (pspec);
	  guint i, nth = 0;
	  /* we ignore non conforming elements */
	  if (espec)
	    {
	      for (i = 0; i < seq->n_elements; i++)
		if (G_VALUE_HOLDS (seq->elements + i, G_PARAM_SPEC_VALUE_TYPE (espec)))
		  {
		    if (nth == 0)
		      {
			gstring_break (gstring, needs_break, indent);
			gstring_puts (gstring, "("); /* open sequence */
		      }
		    else if (nth % 5)
		      gstring_putc (gstring, ' ');
		    else
		      *needs_break = TRUE;
		    nth++;
		    value_store_param (seq->elements + i, gstring, needs_break, espec, indent + 1);
		  }
	    }
          if (nth == 0)
	    gstring_puts (gstring, "("); /* open sequence */
	  gstring_putc (gstring, ')'); /* close sequence */
	}
      break;
    case SFI_SCAT_REC:
      rec = sfi_value_get_rec (value);
      if (!rec)
	gstring_puts (gstring, NULL_STRING);
      else
	{
	  SfiRecFields fspecs = sfi_pspec_get_rec_fields (pspec);
	  guint i, nth = 0;
          /* we ignore non conforming fields */
	  for (i = 0; i < fspecs.n_fields; i++)
	    {
	      GValue *fvalue = sfi_rec_get (rec, fspecs.fields[i]->name);
	      if (fvalue && G_VALUE_HOLDS (fvalue, G_PARAM_SPEC_VALUE_TYPE (fspecs.fields[i])))
		{
		  if (nth++ == 0)
		    {
		      gstring_break (gstring, needs_break, indent);
		      gstring_puts (gstring, "("); /* open record */
		    }
		  else
		    gstring_break (gstring, needs_break, indent + 1);
		  gstring_printf (gstring, "(%s ", fspecs.fields[i]->name); /* open field */
		  value_store_param (fvalue, gstring, needs_break, fspecs.fields[i], indent + 2 + 1);
		  gstring_putc (gstring, ')'); /* close field */
		}
	    }
	  if (nth == 0)
	    gstring_puts (gstring, "("); /* open record */
          gstring_putc (gstring, ')'); /* close record */
	}
      break;
    default:
      g_error ("%s: unimplemented category (%u)", G_STRLOC, scat);
    }
}

void
sfi_value_store_param (const GValue *value,
		       GString      *gstring,
		       GParamSpec   *pspec,
		       guint         indent)
{
  gboolean needs_break = FALSE;

  g_return_if_fail (G_IS_VALUE (value));
  g_return_if_fail (gstring != NULL);
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (G_VALUE_HOLDS (value, G_PARAM_SPEC_VALUE_TYPE (pspec)));

  gstring_break (gstring, &needs_break, indent);
  gstring_printf (gstring, "(%s ", pspec->name);
  value_store_param (value, gstring, &needs_break, pspec, indent + 2);
  gstring_putc (gstring, ')');
}

static GTokenType
value_parse_param (GValue     *value,
		   GScanner   *scanner,
		   GParamSpec *pspec,
		   gboolean    close_statement)
{
  SfiSCategory scat;

  scat = sfi_categorize_pspec (pspec);
  switch (scat)
    {
      GTokenType token;
    case SFI_SCAT_BOOL:
    case SFI_SCAT_INT:
    case SFI_SCAT_NUM:
    case SFI_SCAT_REAL:
    case SFI_SCAT_STRING:
    case SFI_SCAT_CHOICE:
    case SFI_SCAT_PROXY:
    case SFI_SCAT_BBLOCK:
    case SFI_SCAT_FBLOCK:
    case SFI_SCAT_NOTE:
    case SFI_SCAT_TIME:
      token = sfi_serialize_primitives (scat, value, NULL, scanner);
      if (token != G_TOKEN_NONE)
	return token;
      break;
    case SFI_SCAT_SEQ:
      g_scanner_get_next_token (scanner);
      if (check_parse_null (scanner))
	sfi_value_set_seq (value, NULL);
      else if (scanner->token == '(')
	{
	  GParamSpec *espec = sfi_pspec_get_seq_element (pspec);
	  SfiSeq *seq;
	  seq = sfi_seq_new ();
	  sfi_value_set_seq (value, seq);
	  sfi_seq_unref (seq);
	  if (espec)
	    {
	      GValue *evalue = sfi_value_empty ();
	      g_value_init (evalue, G_PARAM_SPEC_VALUE_TYPE (espec));
	      while (g_scanner_peek_next_token (scanner) != ')')
		{
		  token = value_parse_param (evalue, scanner, espec, FALSE);
		  if (token != G_TOKEN_NONE)
		    {
		      sfi_value_free (evalue);
		      return token;
		    }
		  sfi_seq_append (seq, evalue);
		}
	      sfi_value_free (evalue);
	    }
	  parse_or_return (scanner, ')');
	}
      else
	return '(';
      break;
    case SFI_SCAT_REC:
      g_scanner_get_next_token (scanner);
      if (check_parse_null (scanner))
	sfi_value_set_rec (value, NULL);
      else if (scanner->token == '(')
	{
	  SfiRec *rec;
	  rec = sfi_rec_new ();
	  sfi_value_set_rec (value, rec);
	  sfi_rec_unref (rec);
	  while (g_scanner_peek_next_token (scanner) != ')')
	    {
	      GParamSpec *fspec;
	      parse_or_return (scanner, '(');
	      parse_or_return (scanner, G_TOKEN_IDENTIFIER);
	      fspec = sfi_pspec_get_rec_field (pspec, scanner->value.v_identifier);
	      if (!fspec)
		{
		  g_scanner_warn (scanner, "skipping unknown record field `%s'", scanner->value.v_identifier);
		  token = scanner_skip_statement (scanner, 1);
		  if (token != G_TOKEN_NONE)
		    return token;
		}
	      else
		{
		  GValue *fvalue = sfi_value_empty ();
                  g_value_init (fvalue, G_PARAM_SPEC_VALUE_TYPE (fspec));
		  token = value_parse_param (fvalue, scanner, fspec, TRUE);
		  if (token != G_TOKEN_NONE)
		    {
		      sfi_value_free (fvalue);
		      return token;
		    }
		  sfi_rec_set (rec, fspec->name, fvalue);
		  sfi_value_free (fvalue);
		}
	    }
	  parse_or_return (scanner, ')');
	}
      else
	return '(';
      break;
    default:
      if (close_statement)
	{
	  g_scanner_warn (scanner, "skipping value of unknown category `%c'", scat);
	  return scanner_skip_statement (scanner, 1);
	}
      else
	{
	  g_scanner_error (scanner, "unable to parse value of unknown category `%c'", scat);
	  return G_TOKEN_ERROR;
	}
    }
  if (close_statement)
    parse_or_return (scanner, ')');
  return G_TOKEN_NONE;
}

GTokenType
sfi_value_parse_param_rest (GValue     *value,
			    GScanner   *scanner,
			    GParamSpec *pspec)
{
  g_return_val_if_fail (value != NULL && G_VALUE_TYPE (value) == 0, G_TOKEN_ERROR);
  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), G_TOKEN_ERROR);

  /* the scanner better be at the pspec name */
  g_return_val_if_fail (scanner->token == G_TOKEN_IDENTIFIER, G_TOKEN_ERROR);
  g_return_val_if_fail (strcmp (scanner->value.v_identifier, pspec->name) == 0, G_TOKEN_ERROR);

  g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (pspec));

  return value_parse_param (value, scanner, pspec, TRUE);
}
