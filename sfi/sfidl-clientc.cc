/* SFI - Synthesis Fusion Kit Interface
 * Copyright (C) 2002 Stefan Westerfeld
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
#include <glib-extra.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <vector>
#include <map>
#include "sfidl-namespace.h"


/* --- variables --- */
static  GScannerConfig  scanner_config_template = {
  const_cast<gchar *>   /* FIXME: glib should use const gchar* here */
  (
   " \t\r\n"
   )                    /* cset_skip_characters */,
  const_cast<gchar *>
  (
   G_CSET_a_2_z
   "_"
   G_CSET_A_2_Z
   )                    /* cset_identifier_first */,
  const_cast<gchar *>
  (
   G_CSET_a_2_z
   "_0123456789"
   G_CSET_A_2_Z
   )                    /* cset_identifier_nth */,
  const_cast<gchar *>
  ( "#\n" )             /* cpair_comment_single */,
  
  TRUE                  /* case_sensitive */,
  
  TRUE                  /* skip_comment_multi */,
  TRUE                  /* skip_comment_single */,
  TRUE                  /* scan_comment_multi */,
  TRUE                  /* scan_identifier */,
  TRUE                  /* scan_identifier_1char */,
  FALSE                 /* scan_identifier_NULL */,
  TRUE                  /* scan_symbols */,
  FALSE                 /* scan_binary */,
  TRUE                  /* scan_octal */,
  TRUE                  /* scan_float */,
  TRUE                  /* scan_hex */,
  FALSE                 /* scan_hex_dollar */,
  FALSE                 /* scan_string_sq */,
  TRUE                  /* scan_string_dq */,
  TRUE                  /* numbers_2_int */,
  FALSE                 /* int_2_float */,
  FALSE                 /* identifier_2_string */,
  TRUE                  /* char_2_token */,
  TRUE                  /* symbol_2_token */,
  FALSE                 /* scope_0_fallback */,
};

#define TOKEN_CLASS      GTokenType(G_TOKEN_LAST + 1)
#define TOKEN_CHOICE     GTokenType(G_TOKEN_LAST + 2)
#define TOKEN_NAMESPACE  GTokenType(G_TOKEN_LAST + 3)
#define TOKEN_RECORD     GTokenType(G_TOKEN_LAST + 4)
#define TOKEN_SEQUENCE   GTokenType(G_TOKEN_LAST + 5)

#define parse_or_return(token)  G_STMT_START{ \
  GTokenType _t = GTokenType(token); \
  if (g_scanner_get_next_token (scanner) != _t) \
    return _t; \
}G_STMT_END
#define peek_or_return(token)   G_STMT_START{ \
  GScanner *__s = (scanner); GTokenType _t = GTokenType(token); \
  if (g_scanner_peek_next_token (__s) != _t) { \
    g_scanner_get_next_token (__s); /* advance position for error-handler */ \
    return _t; \
  } \
}G_STMT_END
#define parse_string_or_return(str) G_STMT_START{ \
  if (g_scanner_get_next_token (scanner) != G_TOKEN_STRING) \
    return G_TOKEN_STRING; \
  str = scanner->value.v_string; \
}G_STMT_END
#define parse_int_or_return(i) G_STMT_START{ \
  bool negate = (g_scanner_peek_next_token (scanner) == GTokenType('-')); \
  if (negate) \
    g_scanner_get_next_token(scanner); \
  if (g_scanner_get_next_token (scanner) != G_TOKEN_INT) \
    return G_TOKEN_INT; \
  i = scanner->value.v_int; \
  if (negate) i = -i; \
}G_STMT_END
#define parse_float_or_return(f) G_STMT_START{ \
  bool negate = false; \
  GTokenType t = g_scanner_get_next_token (scanner); \
  if (t == GTokenType('-')) \
  { \
    negate = true; \
    t = g_scanner_get_next_token (scanner); \
  } \
  if (t == G_TOKEN_INT) \
    f = scanner->value.v_int; \
  else if (t == G_TOKEN_FLOAT) \
    f = scanner->value.v_float; \
  else \
    return G_TOKEN_FLOAT; \
  if (negate) f = -f; \
}G_STMT_END
#define debug(x)

namespace Conf {
  bool        generateExtern = false;
  const char *generateInit = 0;
  bool        generateData = false;
  bool        generateTypeH = false;
  bool        generateTypeC = false;
  bool        generateBoxedTypes = false;
  bool        generateIdlLineNumbers = false;
  bool        generateSignalStuff = false;
  bool        generateProcedures = false;
  string      target = "c";
  string      namespaceCut = "";
  string      namespaceAdd = "";
};

struct ParamDef {
  string type;
  string name;
  
  string pspec;
  int    line;
  string args;
};

struct EnumComponent {
  string name;
  string text;
  
  int    value;
};

struct EnumDef {
  /**
   * name if the enum, "_anonymous_" for anonymous enum - of course, when
   * using namespaces, this can also lead to things like "Arts::_anonymous_",
   * which would mean an anonymous enum in the Arts namespace
   */
  string name;
  
  vector<EnumComponent> contents;
};

struct RecordDef {
  string name;
  
  vector<ParamDef> contents;
};

struct SequenceDef {
  string name;
  ParamDef content;
};

struct MethodDef {
  string name;

  vector<ParamDef> params;
  ParamDef result;
};

struct ClassDef {
  string name;
  string inherits;
  
  vector<MethodDef> methods;
  vector<MethodDef> signals;
};

class IdlParser {
protected:
  vector<string>      includedNames;
  vector<string>      types;
  bool                insideInclude;
  
  vector<EnumDef>     enums;
  vector<SequenceDef> sequences;
  vector<RecordDef>   records;
  vector<ClassDef>    classes;
  vector<MethodDef>   procedures;
  
  GScanner *scanner;
  
  void printError (const gchar *format, ...);
  
  void addEnumTodo(const EnumDef& edef);
  void addRecordTodo(const RecordDef& rdef);
  void addSequenceTodo(const SequenceDef& sdef);
  void addClassTodo(const ClassDef& cdef);

  GTokenType parseNamespace ();
  GTokenType parseEnumDef ();
  GTokenType parseEnumComponent (EnumComponent& comp, int& value);
  GTokenType parseRecordDef ();
  GTokenType parseRecordField (ParamDef& comp);
  GTokenType parseSequenceDef ();
  GTokenType parseParamDefHints (ParamDef &def);
  GTokenType parseClass ();
  GTokenType parseMethodDef (MethodDef& def);
public:
  IdlParser (const char *file_name, int fd);
  
  bool parse ();
 
  string fileName() const                           { return scanner->input_name; }
  const vector<EnumDef>& getEnums () const	    { return enums; }
  const vector<SequenceDef>& getSequences () const  { return sequences; }
  const vector<RecordDef>& getRecords () const	    { return records; }
  const vector<ClassDef>& getClasses () const	    { return classes; }
  const vector<MethodDef>& getProcedures () const   { return procedures; }
  const vector<string>& getTypes () const           { return types; }
  
  SequenceDef findSequence (const string& name) const;
  RecordDef findRecord (const string& name) const;
  
  bool isEnum(const string& type) const;
  bool isSequence(const string& type) const;
  bool isRecord(const string& type) const;
  bool isClass(const string& type) const;
};


bool IdlParser::isEnum(const string& type) const
{
  vector<EnumDef>::const_iterator i;
  
  for(i=enums.begin();i != enums.end(); i++)
    if(i->name == type) return true;
  
  return false;
}

bool IdlParser::isSequence(const string& type) const
{
  vector<SequenceDef>::const_iterator i;
  
  for(i=sequences.begin();i != sequences.end(); i++)
    if(i->name == type) return true;
  
  return false;
}

bool IdlParser::isRecord(const string& type) const
{
  vector<RecordDef>::const_iterator i;
  
  for(i=records.begin();i != records.end(); i++)
    if(i->name == type) return true;
  
  return false;
}

bool IdlParser::isClass(const string& type) const
{
  vector<ClassDef>::const_iterator i;
  
  for(i=classes.begin();i != classes.end(); i++)
    if(i->name == type) return true;
  
  return false;
}

SequenceDef IdlParser::findSequence(const string& name) const
{
  vector<SequenceDef>::const_iterator i;
  
  for(i=sequences.begin();i != sequences.end(); i++)
    if(i->name == name) return *i;
  
  return SequenceDef();
}

RecordDef IdlParser::findRecord(const string& name) const
{
  vector<RecordDef>::const_iterator i;
  
  for(i=records.begin();i != records.end(); i++)
    if(i->name == name) return *i;
  
  return RecordDef();
}

IdlParser::IdlParser(const char *file_name, int fd)
  : insideInclude (false)
{
  scanner = g_scanner_new (&scanner_config_template);
  
  const char *syms[] = { "class", "choice", "namespace", "record", "sequence", 0 };
  for (int n = 0; syms[n]; n++)
    g_scanner_add_symbol (scanner, syms[n], GUINT_TO_POINTER (TOKEN_CLASS + n));
  
  g_scanner_input_file (scanner, fd);
  scanner->input_name = g_strdup (file_name);
  scanner->max_parse_errors = 1;
  scanner->parse_errors = 0;
}

void IdlParser::printError(const gchar *format, ...)
{
  va_list args;
  gchar *string;
  
  va_start (args, format);
  string = g_strdup_vprintf (format, args);
  va_end (args);
  
  if (scanner->parse_errors < scanner->max_parse_errors)
    g_scanner_error (scanner, "%s", string);
  
  g_free (string);
}

bool IdlParser::parse ()
{
  /* define primitive types into the basic namespace */
  ModuleHelper::define("Bool");
  ModuleHelper::define("Int");
  ModuleHelper::define("Num");
  ModuleHelper::define("Real");
  ModuleHelper::define("String");
  ModuleHelper::define("Proxy"); /* FIXME: remove this as soon as "real" interface types exist */
  ModuleHelper::define("BBlock");
  ModuleHelper::define("FBlock");
  ModuleHelper::define("PSpec");
  
  GTokenType expected_token = G_TOKEN_NONE;
  
  while (!g_scanner_eof (scanner) && expected_token == G_TOKEN_NONE)
    {
      g_scanner_get_next_token (scanner);
      
      if (scanner->token == G_TOKEN_EOF)
        break;
      else if (scanner->token == TOKEN_NAMESPACE)
        expected_token = parseNamespace ();
      else
        expected_token = G_TOKEN_EOF; /* '('; */
    }
  
  if (expected_token != G_TOKEN_NONE)
    {
      g_scanner_unexp_token (scanner, expected_token, NULL, NULL, NULL, NULL, TRUE);
      return false;
    }

  return true;
}

GTokenType IdlParser::parseNamespace()
{
  debug("parse namespace\n");
  parse_or_return (G_TOKEN_IDENTIFIER);
  ModuleHelper::enter (scanner->value.v_identifier);
  
  parse_or_return (G_TOKEN_LEFT_CURLY);
  
  for(;;)
    {
      if (g_scanner_peek_next_token (scanner) == TOKEN_CHOICE)
	{
	  GTokenType expected_token = parseEnumDef ();
	  if (expected_token != G_TOKEN_NONE)
	    return expected_token;
	}
      else if (g_scanner_peek_next_token (scanner) == TOKEN_RECORD)
	{
	  GTokenType expected_token = parseRecordDef ();
	  if (expected_token != G_TOKEN_NONE)
	    return expected_token;
	}
      else if (g_scanner_peek_next_token (scanner) == TOKEN_SEQUENCE)
	{
	  GTokenType expected_token = parseSequenceDef ();
	  if (expected_token != G_TOKEN_NONE)
	    return expected_token;
	}
      else if (g_scanner_peek_next_token (scanner) == TOKEN_CLASS)
	{
	  GTokenType expected_token = parseClass ();
	  if (expected_token != G_TOKEN_NONE)
	    return expected_token;
	}
      else if (g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER)
	{
	  MethodDef procedure;
	  GTokenType expected_token = parseMethodDef (procedure);
	  if (expected_token != G_TOKEN_NONE)
	    return expected_token;

	  procedure.name = ModuleHelper::define (procedure.name.c_str());
	  procedures.push_back (procedure);
	}
      else
        break;
    }
  
  parse_or_return (G_TOKEN_RIGHT_CURLY);
  parse_or_return (';');
  
  ModuleHelper::leave();
  
  return G_TOKEN_NONE;
}

GTokenType IdlParser::parseEnumDef ()
{
  EnumDef edef;
  int value = 0;
  debug("parse enumdef\n");
  
  parse_or_return (TOKEN_CHOICE);
  parse_or_return (G_TOKEN_IDENTIFIER);
  edef.name = ModuleHelper::define (scanner->value.v_identifier);
  parse_or_return (G_TOKEN_LEFT_CURLY);
  while (g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER)
    {
      EnumComponent comp;
      
      GTokenType expected_token = parseEnumComponent (comp, value);
      if (expected_token != G_TOKEN_NONE)
	return expected_token;
      
      edef.contents.push_back(comp);
    }
  parse_or_return (G_TOKEN_RIGHT_CURLY);
  parse_or_return (';');
  
  addEnumTodo (edef);
  return G_TOKEN_NONE;
}

GTokenType IdlParser::parseEnumComponent (EnumComponent& comp, int& value)
{
  /* MASTER @= (25, "Master Volume"), */
  
  parse_or_return (G_TOKEN_IDENTIFIER);
  comp.name = scanner->value.v_identifier;
  
  /* the hints are optional */
  if (g_scanner_peek_next_token (scanner) == GTokenType('@'))
    {
      g_scanner_get_next_token (scanner);
      
      parse_or_return ('=');
      parse_or_return ('(');
      parse_or_return (G_TOKEN_INT);
      value = scanner->value.v_int;
      parse_or_return (',');
      parse_or_return (G_TOKEN_STRING);
      comp.text = scanner->value.v_string;
      parse_or_return (')');
    }
  
  comp.value = value;
  
  if (g_scanner_peek_next_token (scanner) == GTokenType(','))
    parse_or_return (',');
  else
    peek_or_return ('}');
  
  return G_TOKEN_NONE;
}

GTokenType IdlParser::parseRecordDef ()
{
  RecordDef rdef;
  debug("parse recorddef\n");
  
  parse_or_return (TOKEN_RECORD);
  parse_or_return (G_TOKEN_IDENTIFIER);
  rdef.name = ModuleHelper::define (scanner->value.v_identifier);
  parse_or_return (G_TOKEN_LEFT_CURLY);
  while (g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER)
    {
      ParamDef def;
      
      GTokenType expected_token = parseRecordField (def);
      if (expected_token != G_TOKEN_NONE)
	return expected_token;
      
      rdef.contents.push_back(def);
    }
  parse_or_return (G_TOKEN_RIGHT_CURLY);
  parse_or_return (';');
  
  addRecordTodo (rdef);
  return G_TOKEN_NONE;
}

GTokenType IdlParser::parseRecordField (ParamDef& def)
{
  /* FooVolumeType volume_type; */
  /* float         volume_perc @= ("Volume[%]", "Set how loud something is",
     50, 0.0, 100.0, 5.0,
     ":dial:readwrite"); */
  
  parse_or_return (G_TOKEN_IDENTIFIER);
  def.type = ModuleHelper::qualify (scanner->value.v_identifier);
  def.pspec = def.type;
  def.line = scanner->line;
  
  parse_or_return (G_TOKEN_IDENTIFIER);
  def.name = scanner->value.v_identifier;
  
  /* the hints are optional */
  if (g_scanner_peek_next_token (scanner) == GTokenType('@'))
    {
      g_scanner_get_next_token (scanner);
      
      parse_or_return ('=');
      
      GTokenType expected_token = parseParamDefHints (def);
      if (expected_token != G_TOKEN_NONE)
	return expected_token;
    }
  
  parse_or_return (';');
  return G_TOKEN_NONE;
}

GTokenType IdlParser::parseParamDefHints (ParamDef &def)
{
  if (g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER)
    {
      parse_or_return (G_TOKEN_IDENTIFIER);
      def.pspec = scanner->value.v_identifier;
    }
  
  parse_or_return ('(');

  int bracelevel = 1;
  string args;
  while (!g_scanner_eof (scanner) && bracelevel > 0)
    {
      GTokenType t = g_scanner_get_next_token (scanner);
      gchar *token_as_string = 0, *x = 0;
      
      if(int(t) > 0 && int(t) <= 255)
	{
	  token_as_string = (char *)calloc(2, 1);
	  token_as_string[0] = char(t);
	}
      switch (t)
	{
	case '(':		  bracelevel++;
	  break;
	case ')':		  bracelevel--;
	  break;
	case G_TOKEN_STRING:	  x = g_strescape (scanner->value.v_string, 0);
				  token_as_string = g_strdup_printf ("\"%s\"", x);
				  g_free (x);
	  break;
	case G_TOKEN_INT:	  token_as_string = g_strdup_printf ("%lu", scanner->value.v_int);
	  break;
	case G_TOKEN_FLOAT:	  token_as_string = g_strdup_printf ("%.20g", scanner->value.v_float);
	  break;
	case G_TOKEN_IDENTIFIER:  token_as_string = g_strdup_printf ("%s", scanner->value.v_identifier);
	  break;
	default:
	  if (!token_as_string)
	    return GTokenType (')');
	}
      if (token_as_string && bracelevel)
	{
	  args += token_as_string;
	  g_free (token_as_string);
	}
    }
  def.args = args;
  return G_TOKEN_NONE;
}

GTokenType IdlParser::parseSequenceDef ()
{
  SequenceDef sdef;
  /*
   * sequence {
   *   Int ints @= (...);
   * } IntSeq;
   */
  
  parse_or_return (TOKEN_SEQUENCE);
  parse_or_return (G_TOKEN_IDENTIFIER);
  sdef.name = ModuleHelper::define (scanner->value.v_identifier);
  parse_or_return ('{');
  parseRecordField (sdef.content);
  parse_or_return ('}');
  parse_or_return (';');
  
  addSequenceTodo(sdef);
  return G_TOKEN_NONE;
}

GTokenType IdlParser::parseClass ()
{
  ClassDef cdef;
  debug("parse classdef\n");
  
  parse_or_return (TOKEN_CLASS);
  parse_or_return (G_TOKEN_IDENTIFIER);
  cdef.name = ModuleHelper::define (scanner->value.v_identifier);

  if (g_scanner_peek_next_token (scanner) == GTokenType(':'))
    {
      parse_or_return (':');
      parse_or_return (G_TOKEN_IDENTIFIER);
      cdef.inherits = ModuleHelper::qualify (scanner->value.v_identifier);
    }

  parse_or_return (G_TOKEN_LEFT_CURLY);
  while (g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER)
    {
      MethodDef method;
      GTokenType expected_token = parseMethodDef (method);
      if (expected_token != G_TOKEN_NONE)
	return expected_token;

      if (method.result.type == "signal")
        cdef.signals.push_back(method);
      else
	cdef.methods.push_back(method);
    }
  parse_or_return (G_TOKEN_RIGHT_CURLY);
  parse_or_return (';');
  
  addClassTodo (cdef);
  return G_TOKEN_NONE;
}

GTokenType IdlParser::parseMethodDef (MethodDef& mdef)
{
  parse_or_return (G_TOKEN_IDENTIFIER);
  if (strcmp (scanner->value.v_identifier, "signal") == 0)
    mdef.result.type = "signal";
  else if (strcmp (scanner->value.v_identifier, "void") == 0)
    mdef.result.type = "void";
  else
    {
      mdef.result.type = ModuleHelper::qualify (scanner->value.v_identifier);
      mdef.result.name = "result";
    }

  mdef.result.pspec = mdef.result.type;

  parse_or_return (G_TOKEN_IDENTIFIER);
  mdef.name = scanner->value.v_identifier;

  parse_or_return ('(');
  while (g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER)
    {
      ParamDef def;

      parse_or_return (G_TOKEN_IDENTIFIER);
      def.type = ModuleHelper::qualify (scanner->value.v_identifier);
      def.pspec = def.type;
  
      parse_or_return (G_TOKEN_IDENTIFIER);
      def.name = scanner->value.v_identifier;
      mdef.params.push_back(def);

      if (g_scanner_peek_next_token (scanner) != GTokenType(')'))
	{
	  parse_or_return (',');
	  peek_or_return (G_TOKEN_IDENTIFIER);
	}
    }
  parse_or_return (')');

  if (g_scanner_peek_next_token (scanner) == GTokenType(';'))
    {
      parse_or_return (';');
      return G_TOKEN_NONE;
    }

  parse_or_return ('{');
  while (g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER)
    {
      ParamDef *pd = 0;

      parse_or_return (G_TOKEN_IDENTIFIER);
      string inout = scanner->value.v_identifier;

      if (inout == "out")
	{
	  parse_or_return (G_TOKEN_IDENTIFIER);
	  mdef.result.name = scanner->value.v_identifier;
	  pd = &mdef.result;
	}
      else if(inout == "in")
	{
	  parse_or_return (G_TOKEN_IDENTIFIER);
	  for (vector<ParamDef>::iterator pi = mdef.params.begin(); pi != mdef.params.end(); pi++)
	    {
	      if (pi->name == scanner->value.v_identifier)
		pd = &*pi;
	    }
	}
      else
	{
	  printError("in or out expected in method/procedure details\n");
	  return G_TOKEN_IDENTIFIER;
	}
      
      if (!pd)
	{
	  printError("can't associate method/procedure parameter details");
	  return G_TOKEN_IDENTIFIER;
	}

      pd->line = scanner->line;
 
      parse_or_return ('@');
      parse_or_return ('=');
      
      GTokenType expected_token = parseParamDefHints (*pd);
      if (expected_token != G_TOKEN_NONE)
	return expected_token;

      parse_or_return (';');
    }
  parse_or_return ('}');

  return G_TOKEN_NONE;
}

void IdlParser::addEnumTodo(const EnumDef& edef)
{
  enums.push_back(edef);
  
  if (insideInclude)
    {
      includedNames.push_back (edef.name);
    }
  else
    {
      types.push_back (edef.name);
    }
}

void IdlParser::addRecordTodo(const RecordDef& rdef)
{
  records.push_back(rdef);
  
  if (insideInclude)
    {
      includedNames.push_back (rdef.name);
    }
  else
    {
      types.push_back (rdef.name);
    }
}

void IdlParser::addSequenceTodo(const SequenceDef& sdef)
{
  sequences.push_back(sdef);
  
  if (insideInclude)
    {
      includedNames.push_back (sdef.name);
    }
  else
    {
      types.push_back (sdef.name);
    }
}

void IdlParser::addClassTodo(const ClassDef& cdef)
{
  classes.push_back(cdef);
  
  if (insideInclude)
    {
      includedNames.push_back (cdef.name);
    }
  else
    {
      types.push_back (cdef.name);
    }
}

class CodeGenerator {
protected:
  const IdlParser& parser;

  string makeNamespaceSubst (const string& name);
  string makeLowerName (const string& name, char seperator = '_');
  string makeUpperName (const string& name);
  string makeMixedName (const string& name);
  string makeLMixedName (const string& name);

  CodeGenerator(const IdlParser& parser) : parser (parser) {
  }

public:
  virtual void run () = 0;
};

class CodeGeneratorC : public CodeGenerator {
protected:
  
  string makeParamSpec (const ParamDef& pdef);
  string makeGTypeName (const string& name);
  string createTypeCode (const string& type, const string& name, int model);
  
public:
  CodeGeneratorC(const IdlParser& parser) : CodeGenerator(parser) {
  }
  void run ();
};

string CodeGenerator::makeNamespaceSubst (const string& name)
{
  if(name.substr (0, Conf::namespaceCut.length ()) == Conf::namespaceCut)
    return Conf::namespaceAdd + name.substr (Conf::namespaceCut.length ());
  else
    return name; /* pattern not matched */
}

string CodeGenerator::makeLowerName (const string& name, char seperator)
{
  bool lastupper = true, upper = true, lastunder = true;
  string::const_iterator i;
  string result;
  string sname = makeNamespaceSubst (name);
  
  for(i = sname.begin(); i != sname.end(); i++)
    {
      lastupper = upper;
      upper = isupper(*i);
      if (!lastupper && upper && !lastunder)
	{
	  result += seperator;
	  lastunder = true;
	}
      if(*i == ':' || *i == '_')
	{
	  if(!lastunder)
	    {
	      result += seperator;
	      lastunder = true;
	    }
	}
      else
	{
	  result += tolower(*i);
	  lastunder = false;
	}
    }
  return result;
}

string CodeGenerator::makeUpperName (const string& name)
{
  string lname = makeLowerName (name);
  string result;
  string::const_iterator i;
  
  for(i = lname.begin(); i != lname.end(); i++)
    result += toupper(*i);
  return result;
}

string CodeGenerator::makeMixedName (const string& name)
{
  bool lastupper = true, upper = true, lastunder = true;
  string::const_iterator i;
  string result;
  string sname = makeNamespaceSubst (name);
  
  for(i = sname.begin(); i != sname.end(); i++)
    {
      lastupper = upper;
      upper = isupper(*i);
      if (!lastupper && upper && !lastunder)
	{
	  lastunder = true;
	}
      if(*i == ':' || *i == '_')
	{
	  if(!lastunder)
	    {
	      lastunder = true;
	    }
	}
      else
	{
	  if(lastunder)
	    result += toupper (*i);
	  else
	    result += tolower (*i);
	  lastunder = false;
	}
    }
  return result;
}

string CodeGenerator::makeLMixedName (const string& name)
{
  string result = makeMixedName (name);

  if (!result.empty()) result[0] = tolower(result[0]);
  return result;
}

string CodeGeneratorC::makeGTypeName(const string& name)
{
  return makeUpperName (NamespaceHelper::namespaceOf (name)
                      + "::Type" + NamespaceHelper::nameOf(name));
}

string CodeGeneratorC::makeParamSpec(const ParamDef& pdef)
{
  string pspec;
  
  if (parser.isEnum (pdef.type))
    {
      pspec = "sfidl_pspec_Enum";
      if (pdef.args == "")
	pspec += "_default (\"" + pdef.name + "\",";
      else
	pspec += " (\"" + pdef.name + "\"," + pdef.args + ",";
      pspec += makeLowerName (pdef.type) + "_values)";
    }
  else if (parser.isRecord (pdef.type))
    {
      pspec = "sfidl_pspec_Rec";
      if (pdef.args == "")
	pspec += "_default (\"" + pdef.name + "\",";
      else
	pspec += " (\"" + pdef.name + "\"," + pdef.args + ",";
      pspec += makeLowerName (pdef.type) + "_fields)";
    }
  else if (parser.isSequence (pdef.type))
    {
      const SequenceDef& sdef = parser.findSequence (pdef.type);
      pspec = "sfidl_pspec_Seq";
      if (pdef.args == "")
	pspec += "_default (\"" + pdef.name + "\",";
      else
	pspec += " (\"" + pdef.name + "\"," + pdef.args + ",";
      pspec += makeLowerName (pdef.type) + "_content)";
    }
  else
    {
      pspec = "sfidl_pspec_" + pdef.pspec;
      if (pdef.args == "")
	pspec += "_default (\"" + pdef.name + "\")";
      else
	pspec += " (\"" + pdef.name + "\"," + pdef.args + ")";
    }
  return pspec;
}

#define MODEL_ARG         1
#define MODEL_RET         2
#define MODEL_ARRAY       3
#define MODEL_FREE        4
#define MODEL_COPY        5
#define MODEL_NEW         6
#define MODEL_FROM_VALUE  7
#define MODEL_TO_VALUE    8
#define MODEL_VCALL_ARG   9
#define MODEL_VCALL_CONV  10
#define MODEL_VCALL_CFREE 11

string CodeGeneratorC::createTypeCode(const string& type, const string &name, int model)
{
  g_return_val_if_fail (model != MODEL_ARG || model != MODEL_RET ||
                        model != MODEL_ARRAY || name == "", "bad");
  g_return_val_if_fail (model != MODEL_FREE || model != MODEL_COPY || model != MODEL_NEW ||
                        model != MODEL_FROM_VALUE || model != MODEL_TO_VALUE || name != "", "bad");

  if (parser.isRecord (type) || parser.isSequence (type))
    {
      if (model == MODEL_ARG)         return makeMixedName (type)+"*";
      if (model == MODEL_RET)         return makeMixedName (type)+"*";
      if (model == MODEL_ARRAY)       return makeMixedName (type)+"**";
      if (model == MODEL_FREE)        return makeLowerName (type)+"_free ("+name+")";
      if (model == MODEL_COPY)        return makeLowerName (type)+"_copy_shallow ("+name+")";
      if (model == MODEL_NEW)         return name + " = " + makeLowerName (type)+"_new ()";

      if (parser.isSequence (type))
      {
	if (model == MODEL_TO_VALUE)
	  return "sfi_value_seq (" + makeLowerName (type)+"_to_seq ("+name+"))";
	if (model == MODEL_FROM_VALUE) 
	  return makeLowerName (type)+"_from_seq (sfi_value_get_seq ("+name+"))";
	if (model == MODEL_VCALL_ARG) 
	  return "'Q', "+name+",";
	if (model == MODEL_VCALL_CONV) 
	  return makeLowerName (type)+"_to_seq ("+name+")";
	if (model == MODEL_VCALL_CFREE) 
	  return "sfi_seq_unref ("+name+")";
      }
      else
      {
	if (model == MODEL_TO_VALUE)   
	  return "sfi_value_rec (" + makeLowerName (type)+"_to_rec ("+name+"))";
	if (model == MODEL_FROM_VALUE)
	  return makeLowerName (type)+"_from_rec (sfi_value_get_rec ("+name+"))";
	if (model == MODEL_VCALL_ARG) 
	  return "'R', "+name+",";
	if (model == MODEL_VCALL_CONV) 
	  return makeLowerName (type)+"_to_rec ("+name+")";
	if (model == MODEL_VCALL_CFREE) 
	  return "sfi_rec_unref ("+name+")";
      }
    }
  else if (parser.isEnum (type))
    {
      /*
       * FIXME: this has to be changed to a language binding where enums 
       * are using the typdef enum type we also generate - therefore, we
       * also need to register a glib typesystem type to do the conversion
       * between the actual integer value and the string
       */
      if (model == MODEL_ARG)         return "gchar*";
      if (model == MODEL_RET)         return "gchar*";
      if (model == MODEL_ARRAY)       return "gchar**";
      if (model == MODEL_FREE)        return "g_free (" + name + ")";
      if (model == MODEL_COPY)        return "g_strdup (" + name + ")";;
      if (model == MODEL_NEW)         return "";
      if (model == MODEL_TO_VALUE)    return "sfi_value_enum ("+name+")";
      // FIXME: do we want sfi_value_dup_enum?
      if (model == MODEL_FROM_VALUE)  return "g_strdup (sfi_value_get_enum ("+name+"))";
      if (model == MODEL_VCALL_ARG)   return "'c', "+makeLowerName (type)+"_to_choice ("+name+"),";
      if (model == MODEL_VCALL_CONV)  return "";
      if (model == MODEL_VCALL_CFREE) return "";
    }
  else if (parser.isClass (type) || type == "Proxy")
    {
      /*
       * FIXME: we're currently not using the type of the proxy anywhere
       * it might for instance be worthwile being able to ensure that if
       * we're expecting a "SfkServer" object, we will have one
       */
      if (model == MODEL_ARG)         return "SfiProxy";
      if (model == MODEL_RET)         return "SfiProxy";
      if (model == MODEL_ARRAY)       return "SfiProxy*";
      if (model == MODEL_FREE)        return "";
      if (model == MODEL_COPY)        return name;
      if (model == MODEL_NEW)         return "";
      if (model == MODEL_TO_VALUE)    return "sfi_value_proxy ("+name+")";
      if (model == MODEL_FROM_VALUE)  return "sfi_value_get_proxy ("+name+")";
      if (model == MODEL_VCALL_ARG)   return "'p', "+name+",";
      if (model == MODEL_VCALL_CONV)  return "";
      if (model == MODEL_VCALL_CFREE) return "";
    }
  else if (type == "String")
    {
      if (model == MODEL_ARG)         return "gchar*";
      if (model == MODEL_RET)         return "gchar*";
      if (model == MODEL_ARRAY)       return "gchar**";
      if (model == MODEL_FREE)        return "g_free (" + name + ")";
      if (model == MODEL_COPY)        return "g_strdup (" + name + ")";;
      if (model == MODEL_NEW)         return "";
      if (model == MODEL_TO_VALUE)    return "sfi_value_string ("+name+")";
      // FIXME: do we want sfi_value_dup_string?
      if (model == MODEL_FROM_VALUE)  return "g_strdup (sfi_value_get_string ("+name+"))";
      if (model == MODEL_VCALL_ARG)   return "'s', "+name+",";
      if (model == MODEL_VCALL_CONV)  return "";
      if (model == MODEL_VCALL_CFREE) return "";
    }
  else if (type == "BBlock")
    {
      if (model == MODEL_ARG)         return "SfiBBlock*";
      if (model == MODEL_RET)         return "SfiBBlock*";
      if (model == MODEL_ARRAY)       return "SfiBBlock**";
      if (model == MODEL_FREE)        return "sfi_bblock_unref (" + name + ")";
      if (model == MODEL_COPY)        return "sfi_bblock_ref (" + name + ")";;
      if (model == MODEL_NEW)         return name + " = sfi_bblock_new ()";
      if (model == MODEL_TO_VALUE)    return "sfi_value_bblock ("+name+")";
      if (model == MODEL_FROM_VALUE)  return "sfi_bblock_ref (sfi_value_get_bblock ("+name+"))";
      if (model == MODEL_VCALL_ARG)   return "'B', "+name+",";
      if (model == MODEL_VCALL_CONV)  return "";
      if (model == MODEL_VCALL_CFREE) return "";
    }
  else if (type == "FBlock")
    {
      if (model == MODEL_ARG)         return "SfiFBlock*";
      if (model == MODEL_RET)         return "SfiFBlock*";
      if (model == MODEL_ARRAY)       return "SfiFBlock**";
      if (model == MODEL_FREE)        return "sfi_fblock_unref (" + name + ")";
      if (model == MODEL_COPY)        return "sfi_fblock_ref (" + name + ")";;
      if (model == MODEL_NEW)         return name + " = sfi_fblock_new ()";
      if (model == MODEL_TO_VALUE)    return "sfi_value_fblock ("+name+")";
      if (model == MODEL_FROM_VALUE)  return "sfi_fblock_ref (sfi_value_get_fblock ("+name+"))";
      if (model == MODEL_VCALL_ARG)   return "'F', "+name+",";
      if (model == MODEL_VCALL_CONV)  return "";
      if (model == MODEL_VCALL_CFREE) return "";
    }
  else
    {
      if (model == MODEL_ARG)         return "Sfi" + type;
      if (model == MODEL_RET)         return "Sfi" + type;
      if (model == MODEL_ARRAY)       return "Sfi" + type + "*";
      if (model == MODEL_FREE)        return "";
      if (model == MODEL_COPY)        return name;
      if (model == MODEL_NEW)         return "";
      if (model == MODEL_TO_VALUE)    return "sfi_value_" + makeLowerName(type) + " ("+name+")";
      if (model == MODEL_FROM_VALUE)  return "sfi_value_get_" + makeLowerName(type) + " ("+name+")";
      if (model == MODEL_VCALL_ARG)
	{
	  if (type == "Real")	      return "'r', "+name+",";
	  if (type == "Bool")	      return "'b', "+name+",";
	  if (type == "Int")	      return "'i', "+name+",";
	  if (type == "Num")	      return "'n', "+name+",";
	}
      if (model == MODEL_VCALL_CONV)  return "";
      if (model == MODEL_VCALL_CFREE) return "";
    }
  return "*createTypeCode*unknown*";
}

void CodeGeneratorC::run ()
{
  vector<SequenceDef>::const_iterator si;
  vector<RecordDef>::const_iterator ri;
  vector<EnumDef>::const_iterator ei;
  vector<ParamDef>::const_iterator pi;
  vector<ClassDef>::const_iterator ci;
  vector<MethodDef>::const_iterator mi;
  
  if (Conf::generateTypeH)
    {
      printf("\n");
      for (si = parser.getSequences().begin(); si != parser.getSequences().end(); si++)
	{
	  string mname = makeMixedName (si->name);
	  printf("typedef struct _%s %s;\n", mname.c_str(), mname.c_str());
	}
      for (ri = parser.getRecords().begin(); ri != parser.getRecords().end(); ri++)
	{
	  string mname = makeMixedName (ri->name);
	  printf("typedef struct _%s %s;\n", mname.c_str(), mname.c_str());
	}
      for(ei = parser.getEnums().begin(); ei != parser.getEnums().end(); ei++)
	{
	  string mname = makeMixedName (ei->name);
	  printf("\ntypedef enum {\n");
	  for (vector<EnumComponent>::const_iterator ci = ei->contents.begin(); ci != ei->contents.end(); ci++)
	    {
	      string ename = makeUpperName (NamespaceHelper::namespaceOf(ei->name) + ci->name);
	      printf("  %s = %d,\n", ename.c_str(), ci->value);
	    }
	  printf("} %s;\n", mname.c_str());
	}
      
      printf("\n");
      for (si = parser.getSequences().begin(); si != parser.getSequences().end(); si++)
	{
	  string mname = makeMixedName (si->name.c_str());
	  string array = createTypeCode (si->content.type, "", MODEL_ARRAY);
	  string elements = si->content.name;
	  
	  printf("struct _%s {\n", mname.c_str());
	  printf("  guint n_%s;\n", elements.c_str ());
	  printf("  %s %s;\n", array.c_str(), elements.c_str());
	  printf("};\n");
	}
      for (ri = parser.getRecords().begin(); ri != parser.getRecords().end(); ri++)
	{
	  string mname = makeMixedName (ri->name.c_str());
	  
	  printf("struct _%s {\n", mname.c_str());
	  for (pi = ri->contents.begin(); pi != ri->contents.end(); pi++)
	    {
	      printf("  %s %s;\n", createTypeCode(pi->type, "", MODEL_ARG).c_str(), pi->name.c_str());
	    }
	  printf("};\n");
	}

      printf("\n");
      for (si = parser.getSequences().begin(); si != parser.getSequences().end(); si++)
	{
	  string ret = createTypeCode (si->name, "", MODEL_RET);
	  string arg = createTypeCode (si->name, "", MODEL_ARG);
	  string element = createTypeCode (si->content.type, "", MODEL_ARG);
	  string lname = makeLowerName (si->name.c_str());
	  
	  printf("%s %s_new (void);\n", ret.c_str(), lname.c_str());
	  printf("void %s_append (%s seq, %s element);\n", lname.c_str(), arg.c_str(), element.c_str());
	  printf("%s %s_copy_shallow (%s seq);\n", ret.c_str(), lname.c_str(), arg.c_str());
	  printf("%s %s_from_seq (SfiSeq *sfi_seq);\n", ret.c_str(), lname.c_str());
	  printf("SfiSeq *%s_to_seq (%s seq);\n", lname.c_str(), arg.c_str());
	  printf("void %s_resize (%s seq, guint new_size);\n", lname.c_str(), arg.c_str());
	  printf("void %s_free (%s seq);\n", lname.c_str(), arg.c_str());
	  printf("\n");
	}
      for (ri = parser.getRecords().begin(); ri != parser.getRecords().end(); ri++)
	{
	  string ret = createTypeCode (ri->name, "", MODEL_RET);
	  string arg = createTypeCode (ri->name, "", MODEL_ARG);
	  string lname = makeLowerName (ri->name.c_str());
	  
	  printf("%s %s_new (void);\n", ret.c_str(), lname.c_str());
	  printf("%s %s_copy_shallow (%s rec);\n", ret.c_str(), lname.c_str(), arg.c_str());
	  printf("%s %s_from_rec (SfiRec *sfi_rec);\n", ret.c_str(), lname.c_str());
	  printf("SfiRec *%s_to_rec (%s rec);\n", lname.c_str(), arg.c_str());
	  printf("void %s_free (%s rec);\n", lname.c_str(), arg.c_str());
	  printf("\n");
	}
      printf("\n");
    }
  
  if (Conf::generateExtern)
    {
      for(ei = parser.getEnums().begin(); ei != parser.getEnums().end(); ei++)
	printf("extern SfiEnumValues %s_values;\n", makeLowerName (ei->name).c_str());
      
      for(ri = parser.getRecords().begin(); ri != parser.getRecords().end(); ri++)
      {
	printf("extern SfiRecFields %s_fields;\n",makeLowerName (ri->name).c_str());
        if (Conf::generateBoxedTypes)
	  printf("extern GType %s;\n", makeGTypeName (ri->name).c_str());
      }

      if (Conf::generateBoxedTypes)
      {
	for(si = parser.getSequences().begin(); si != parser.getSequences().end(); si++)
	  printf("extern GType %s;\n", makeGTypeName (si->name).c_str());
      }
      printf("\n");
    }
  
  if (Conf::generateTypeC)
    {
      for (si = parser.getSequences().begin(); si != parser.getSequences().end(); si++)
	{
	  string ret = createTypeCode (si->name, "", MODEL_RET);
	  string arg = createTypeCode (si->name, "", MODEL_ARG);
	  string element = createTypeCode (si->content.type, "", MODEL_ARG);
	  string elements = si->content.name;
	  string lname = makeLowerName (si->name.c_str());
	  string mname = makeMixedName (si->name.c_str());
	  
	  printf("%s\n", ret.c_str());
	  printf("%s_new (void)\n", lname.c_str());
	  printf("{\n");
	  printf("  return g_new0 (%s, 1);\n",mname.c_str());
	  printf("}\n\n");
	  
	  string elementCopy = createTypeCode (si->content.type, "element", MODEL_COPY);
	  printf("void\n");
	  printf("%s_append (%s seq, %s element)\n", lname.c_str(), arg.c_str(), element.c_str());
	  printf("{\n");
	  printf("  g_return_if_fail (seq != NULL);\n");
	  printf("\n");
	  printf("  seq->%s = g_realloc (seq->%s, "
		 "(seq->n_%s + 1) * sizeof (seq->%s[0]));\n",
		 elements.c_str(), elements.c_str(), elements.c_str(), elements.c_str());
	  printf("  seq->%s[seq->n_%s++] = %s;\n", elements.c_str(), elements.c_str(),
	         elementCopy.c_str());
	  printf("}\n\n");
	  
	  printf("%s\n", ret.c_str());
	  printf("%s_copy_shallow (%s seq)\n", lname.c_str(), arg.c_str());
	  printf("{\n");
	  printf("  %s seq_copy = NULL;\n", arg.c_str ());
          printf("  if (seq)\n");
          printf("    {\n");
	  printf("      guint i;\n");
	  printf("      seq_copy = %s_new ();\n", lname.c_str());
	  printf("      for (i = 0; i < seq->n_%s; i++)\n", elements.c_str());
	  printf("        %s_append (seq_copy, seq->%s[i]);\n", lname.c_str(), elements.c_str());
          printf("    }\n");
	  printf("  return seq_copy;\n");
	  printf("}\n\n");
	  
	  string elementFromValue = createTypeCode (si->content.type, "element", MODEL_FROM_VALUE);
	  printf("%s\n", ret.c_str());
	  printf("%s_from_seq (SfiSeq *sfi_seq)\n", lname.c_str());
	  printf("{\n");
	  printf("  %s seq;\n", arg.c_str());
	  printf("  guint i, length;\n");
	  printf("\n");
	  printf("  g_return_val_if_fail (sfi_seq != NULL, NULL);\n");
	  printf("\n");
	  printf("  length = sfi_seq_length (sfi_seq);\n");
	  printf("  seq = g_new0 (%s, 1);\n",mname.c_str());
	  printf("  seq->n_%s = length;\n", elements.c_str());
	  printf("  seq->%s = g_malloc (seq->n_%s * sizeof (seq->%s[0]));\n\n",
	         elements.c_str(), elements.c_str(), elements.c_str());
	  printf("  for (i = 0; i < length; i++)\n");
	  printf("  {\n");
	  printf("    GValue *element = sfi_seq_get (sfi_seq, i);\n");
	  printf("    seq->%s[i] = %s;\n", elements.c_str(), elementFromValue.c_str());
	  printf("  }\n");
	  printf("  return seq;\n");
	  printf("}\n\n");

	  string elementToValue = createTypeCode (si->content.type, "seq->" + elements + "[i]", MODEL_TO_VALUE);
	  printf("SfiSeq *\n");
	  printf("%s_to_seq (%s seq)\n", lname.c_str(), arg.c_str());
	  printf("{\n");
	  printf("  SfiSeq *sfi_seq;\n");
	  printf("  guint i;\n");
	  printf("\n");
	  printf("  g_return_val_if_fail (seq != NULL, NULL);\n");
	  printf("\n");
	  printf("  sfi_seq = sfi_seq_new ();\n");
	  printf("  for (i = 0; i < seq->n_%s; i++)\n", elements.c_str());
	  printf("  {\n");
	  printf("    GValue *element = %s;\n", elementToValue.c_str());
	  printf("    sfi_seq_append (sfi_seq, element);\n");
	  printf("    sfi_value_free (element);\n");        // FIXME: couldn't we have take_append
	  printf("  }\n");
	  printf("  return sfi_seq;\n");
	  printf("}\n\n");

	  string element_i_free = createTypeCode (si->content.type, "seq->" + elements + "[i]", MODEL_FREE);
	  printf("void\n");
	  printf("%s_resize (%s seq, guint new_size)\n", lname.c_str(), arg.c_str());
	  printf("{\n");
	  printf("  g_return_if_fail (seq != NULL);\n");
	  printf("\n");
	  if (element_i_free != "")
	    {
	      printf("  if (seq->n_%s > new_size)\n", elements.c_str());
	      printf("    {\n");
	      printf("      guint i;\n");
	      printf("      for (i = new_size; i < seq->n_%s; i++)\n", elements.c_str());
	      printf("        %s;\n", element_i_free.c_str());
	      printf("    }\n");
	    }
	  printf("\n");
	  printf("  seq->%s = g_realloc (seq->%s, new_size * sizeof (seq->%s[0]));\n",
                 elements.c_str(), elements.c_str(), elements.c_str(), elements.c_str());
	  printf("  if (new_size > seq->n_%s)\n", elements.c_str());
	  printf("    memset (&seq->%s[seq->n_%s], 0, sizeof(seq->%s[0]) * (new_size - seq->n_%s));\n",
                 elements.c_str(), elements.c_str(), elements.c_str(), elements.c_str());
	  printf("  seq->n_%s = new_size;\n", elements.c_str());
	  printf("}\n\n");

	  printf("void\n");
	  printf("%s_free (%s seq)\n", lname.c_str(), arg.c_str());
	  printf("{\n");
          printf("  if (seq)\n");
          printf("    {\n");
	  if (element_i_free != "")
	    {
	      printf("      guint i;\n");
	      printf("      for (i = 0; i < seq->n_%s; i++)\n", elements.c_str());
	      printf("        %s;\n", element_i_free.c_str());
	    }
	  printf("      g_free (seq);\n");
          printf("    }\n");
	  printf("}\n\n");
	  printf("\n");
	}
      for (ri = parser.getRecords().begin(); ri != parser.getRecords().end(); ri++)
	{
	  string ret = createTypeCode (ri->name, "", MODEL_RET);
	  string arg = createTypeCode (ri->name, "", MODEL_ARG);
	  string lname = makeLowerName (ri->name.c_str());
	  string mname = makeMixedName (ri->name.c_str());
	  
	  printf("%s\n", ret.c_str());
	  printf("%s_new (void)\n", lname.c_str());
	  printf("{\n");
	  printf("  %s rec = g_new0 (%s, 1);\n", arg.c_str(), mname.c_str());
	  for (pi = ri->contents.begin(); pi != ri->contents.end(); pi++)
	    {
	      string init =  createTypeCode(pi->type, "rec->" + pi->name, MODEL_NEW);
	      if (init != "") printf("  %s;\n",init.c_str());
	    }
	  printf("  return rec;\n");
	  printf("}\n\n");
	  
	  printf("%s\n", ret.c_str());
	  printf("%s_copy_shallow (%s rec)\n", lname.c_str(), arg.c_str());
	  printf("{\n");
	  printf("  %s rec_copy = NULL;\n", arg.c_str());
	  printf("  if (rec)\n");
	  printf("    {\n");
	  printf("      rec_copy = %s_new ();\n", lname.c_str());
	  for (pi = ri->contents.begin(); pi != ri->contents.end(); pi++)
	    {
	      string copy =  createTypeCode(pi->type, "rec->" + pi->name, MODEL_COPY);
	      printf("      rec_copy->%s = %s;\n", pi->name.c_str(), copy.c_str());
	    }
	  printf("    }\n");
	  printf("  return rec_copy;\n");
	  printf("}\n\n");
	  
	  printf("%s\n", ret.c_str());
	  printf("%s_from_rec (SfiRec *sfi_rec)\n", lname.c_str());
	  printf("{\n");
	  printf("  GValue *element;\n");
	  printf("  %s rec;\n", arg.c_str());
	  printf("\n");
	  printf("  g_return_val_if_fail (sfi_rec != NULL, NULL);\n");
	  printf("\n");
	  printf("  rec = g_new0 (%s, 1);\n", mname.c_str());
	  for (pi = ri->contents.begin(); pi != ri->contents.end(); pi++)
	    {
	      string elementFromValue = createTypeCode (pi->type, "element", MODEL_FROM_VALUE);
	      printf("  element = sfi_rec_get (sfi_rec, \"%s\");\n", pi->name.c_str());
	      printf("  rec->%s = %s;\n", pi->name.c_str(), elementFromValue.c_str());
	    }
	  printf("  return rec;\n");
	  printf("}\n\n");
	  
	  printf("SfiRec *\n");
	  printf("%s_to_rec (%s rec)\n", lname.c_str(), arg.c_str());
	  printf("{\n");
	  printf("  SfiRec *sfi_rec;\n");
	  printf("  GValue *element;\n");
	  printf("\n");
	  printf("  g_return_val_if_fail (rec != NULL, NULL);\n");
          printf("\n");
	  printf("  sfi_rec = sfi_rec_new ();\n");
	  for (pi = ri->contents.begin(); pi != ri->contents.end(); pi++)
	    {
	      string elementToValue = createTypeCode (pi->type, "rec->" + pi->name, MODEL_TO_VALUE);
	      printf("  element = %s;\n", elementToValue.c_str());
	      printf("  sfi_rec_set (sfi_rec, \"%s\", element);\n", pi->name.c_str());
	      printf("  sfi_value_free (element);\n");        // FIXME: couldn't we have take_set
	    }
	  printf("  return sfi_rec;\n");
	  printf("}\n\n");
	  
	  printf("void\n");
	  printf("%s_free (%s rec)\n", lname.c_str(), arg.c_str());
	  printf("{\n");
	  printf("  if (rec)\n");
	  printf("    {\n");
	  for (pi = ri->contents.begin(); pi != ri->contents.end(); pi++)
	    {
	      string free =  createTypeCode(pi->type, "rec->" + pi->name, MODEL_FREE);
	      if (free != "") printf("      %s;\n",free.c_str());
	    }
	  printf("      g_free (rec);\n");
	  printf("    }\n");
	  printf("}\n\n");
	  printf("\n");
	}
    }
  
  if (Conf::generateData)
    {
      for(ei = parser.getEnums().begin(); ei != parser.getEnums().end(); ei++)
	{
	  string name = makeLowerName (ei->name);
	  printf("static const GEnumValue %s_value[%d] = {\n",name.c_str(), ei->contents.size());
	  for (vector<EnumComponent>::const_iterator ci = ei->contents.begin(); ci != ei->contents.end(); ci++)
	    {
	      printf("  { %d, \"%s\", \"%s\" },\n", ci->value, ci->name.c_str(), ci->text.c_str());
	    }
	  printf("  { 0, NULL, NULL }\n");
	  printf("};\n");
	  printf("SfiEnumValues %s_values = { %d, %s_value };\n", name.c_str(), ei->contents.size(), name.c_str());
	  printf("\n");
	}
      
      for(ri = parser.getRecords().begin(); ri != parser.getRecords().end(); ri++)
	{
	  string name = makeLowerName (ri->name);
	  
	  printf("static GParamSpec *%s_field[%d];\n", name.c_str(), ri->contents.size());
	  printf("SfiRecFields %s_fields = { %d, %s_field };\n", name.c_str(), ri->contents.size(), name.c_str());

	  if (Conf::generateBoxedTypes)
	    {
	      string mname = makeMixedName (ri->name);
	      
	      printf("static void\n");
	      printf("%s_boxed2rec (const GValue *src_value, GValue *dest_value)\n", name.c_str());
	      printf("{\n");
	      printf("  sfi_value_take_rec (dest_value,\n");
	      printf("    %s_to_rec (g_value_get_boxed (src_value)));\n", name.c_str());
	      printf("}\n");
	      
	      printf("static void\n");
	      printf("%s_rec2boxed (const GValue *src_value, GValue *dest_value)\n", name.c_str());
	      printf("{\n");
	      printf("  g_value_set_boxed_take_ownership (dest_value,\n");
	      printf("    %s_from_rec (sfi_value_get_rec (src_value)));\n", name.c_str());
	      printf("}\n");
	      
	      printf("static SfiBoxedRecordInfo %s_boxed_info = {\n", name.c_str());
	      printf("  \"%s\",\n", mname.c_str());
	      printf("  { %d, %s_field },\n", ri->contents.size(), name.c_str());
	      printf("  (SfiBoxedToRec) %s_to_rec,\n", name.c_str());
	      printf("  (SfiBoxedFromRec) %s_from_rec,\n", name.c_str());
	      printf("  %s_boxed2rec,\n", name.c_str());
	      printf("  %s_rec2boxed,\n", name.c_str());
	      printf("};\n");
	      printf("GType %s = 0;\n", makeGTypeName (ri->name).c_str());
	    }
	  printf("\n");
	}
      for(si = parser.getSequences().begin(); si != parser.getSequences().end(); si++)
	{
	  string name = makeLowerName (si->name);
	  
	  printf("static GParamSpec *%s_content;\n", name.c_str());

	  if (Conf::generateBoxedTypes)
	    {
	      string mname = makeMixedName (si->name);
	      
	      printf("static void\n");
	      printf("%s_boxed2seq (const GValue *src_value, GValue *dest_value)\n", name.c_str());
	      printf("{\n");
	      printf("  sfi_value_take_seq (dest_value,\n");
	      printf("    %s_to_seq (g_value_get_boxed (src_value)));\n", name.c_str());
	      printf("}\n");
	      
	      printf("static void\n");
	      printf("%s_seq2boxed (const GValue *src_value, GValue *dest_value)\n", name.c_str());
	      printf("{\n");
	      printf("  g_value_set_boxed_take_ownership (dest_value,\n");
	      printf("    %s_from_seq (sfi_value_get_seq (src_value)));\n", name.c_str());
	      printf("}\n");
	      
	      printf("static SfiBoxedSequenceInfo %s_boxed_info = {\n", name.c_str());
	      printf("  \"%s\",\n", mname.c_str());
	      printf("  NULL, /* %s_content */\n", name.c_str());
	      printf("  (SfiBoxedToSeq) %s_to_seq,\n", name.c_str());
	      printf("  (SfiBoxedFromSeq) %s_from_seq,\n", name.c_str());
	      printf("  %s_boxed2seq,\n", name.c_str());
	      printf("  %s_seq2boxed,\n", name.c_str());
	      printf("};\n");
	      printf("GType %s = 0;\n", makeGTypeName (si->name).c_str());
	    }
	  printf("\n");
	}
    }

  if (Conf::generateInit)
    {
      bool first = true;
      printf("static void\n%s (void)\n", Conf::generateInit);
      printf("{\n");

      /*
       * It is important to follow the declaration order of the idl file here, as for
       * instance a ParamDef inside a record might come from a sequence, and a ParamDef
       * inside a Sequence might come from a record - to avoid using yet-unitialized
       * ParamDefs, we follow the getTypes() 
       */
      vector<string>::const_iterator ti;

      for(ti = parser.getTypes().begin(); ti != parser.getTypes().end(); ti++)
	{
	  if (parser.isRecord (*ti) || parser.isSequence (*ti))
	    {
	      if(!first) printf("\n");
	      first = false;
	    }
	  if (parser.isRecord (*ti))
	    {
	      const RecordDef& rdef = parser.findRecord (*ti);

	      string name = makeLowerName (rdef.name);
	      int f = 0;

	      for (pi = rdef.contents.begin(); pi != rdef.contents.end(); pi++, f++)
		{
		  if (Conf::generateIdlLineNumbers)
		    printf("#line %u \"%s\"\n", pi->line, parser.fileName().c_str());
		  printf("  %s_field[%d] = %s;\n", name.c_str(), f, makeParamSpec (*pi).c_str());
		}
	    }
	  if (parser.isSequence (*ti))
	    {
	      const SequenceDef& sdef = parser.findSequence (*ti);

	      string name = makeLowerName (sdef.name);
	      int f = 0;

	      if (Conf::generateIdlLineNumbers)
		printf("#line %u \"%s\"\n", sdef.content.line, parser.fileName().c_str());
	      printf("  %s_content = %s;\n", name.c_str(), makeParamSpec (sdef.content).c_str());
	    }
	}
      if (Conf::generateBoxedTypes)
      {
	for(ri = parser.getRecords().begin(); ri != parser.getRecords().end(); ri++)
	  {
	    string gname = makeGTypeName(ri->name);
	    string name = makeLowerName(ri->name);

	    printf("  %s = sfi_boxed_make_record (&%s_boxed_info,\n", gname.c_str(), name.c_str());
	    printf("    (GBoxedCopyFunc) %s_copy_shallow,\n", name.c_str());
	    printf("    (GBoxedFreeFunc) %s_free);\n", name.c_str());
	  }
      	for(si = parser.getSequences().begin(); si != parser.getSequences().end(); si++)
	  {
	    string gname = makeGTypeName(si->name);
	    string name = makeLowerName(si->name);

	    printf("  %s_boxed_info.element = %s_content;\n", name.c_str(), name.c_str());
	    printf("  %s = sfi_boxed_make_sequence (&%s_boxed_info,\n", gname.c_str(), name.c_str());
	    printf("    (GBoxedCopyFunc) %s_copy_shallow,\n", name.c_str());
	    printf("    (GBoxedFreeFunc) %s_free);\n", name.c_str());
	  }
}
      printf("}\n");
    }

  if (Conf::generateSignalStuff)
    {
      for (ci = parser.getClasses().begin(); ci != parser.getClasses().end(); ci++)
	{
	  vector<MethodDef>::const_iterator si;

	  for (si = ci->signals.begin(); si != ci->signals.end(); si++)
	    {
	      string fullname = makeLowerName (ci->name + "::" + si->name);

	      printf("void %s_frobnicator (SignalContext *sigcontext) {\n", fullname.c_str());
	      printf("  /* TODO: do something meaningful here */\n");
	      for (pi = si->params.begin(); pi != si->params.end(); pi++)
		{
		  string arg = createTypeCode(pi->type, "", MODEL_ARG);
		  printf("  %s %s;\n", arg.c_str(), pi->name.c_str());
		}
	      printf("}\n");
	    }
	}
    }

  if (Conf::generateProcedures)
    {
      for (mi = parser.getProcedures().begin(); mi != parser.getProcedures().end(); mi++)
	{
	  /* FIXME:
	   *  - handle result types other than void
	   *  - generate code for methods as well (not only procedures)
	   */
	  if (mi->result.type == "void")
	    {
	      string mname = makeLowerName(mi->name);
	      string dname = makeLowerName(mi->name, '-');

	      bool first = true;
	      printf("void %s (", mname.c_str());
	      for(pi = mi->params.begin(); pi != mi->params.end(); pi++)
		{
		  string arg = createTypeCode(pi->type, "", MODEL_ARG);
		  if(!first) printf(", ");
		  first = false;
		  printf("%s %s", arg.c_str(), pi->name.c_str());
		}
	      printf(") {\n");

	      map<string, string> cname;
	      for(pi = mi->params.begin(); pi != mi->params.end(); pi++)
		{
		  string conv = createTypeCode (pi->type, pi->name, MODEL_VCALL_CONV);
		  if (conv != "")
		    {
		      cname[pi->name] = pi->name + "__c";

		      string arg = createTypeCode(pi->type, "", MODEL_ARG);
		      printf("  %s %s__c = %s;\n", arg.c_str(), pi->name.c_str(), conv.c_str());
		    }
		  else
		    cname[pi->name] = pi->name;
		}

	      printf("  sfi_glue_vcall_void (\"%s\", ", dname.c_str());
	      for(pi = mi->params.begin(); pi != mi->params.end(); pi++)
	        printf("%s ", createTypeCode(pi->type, cname[pi->name], MODEL_VCALL_ARG).c_str());
	      printf("0);\n");

	      for(pi = mi->params.begin(); pi != mi->params.end(); pi++)
		{
		  string cfree = createTypeCode (pi->type, cname[pi->name], MODEL_VCALL_CFREE);
		  if (cfree != "")
		    printf("  %s;\n", cfree.c_str());
		}

	      printf("}\n");
	    }
	}
    }
}

class CodeGeneratorQt : public CodeGenerator {
public:
  CodeGeneratorQt(const IdlParser& parser) : CodeGenerator(parser) {
  }
  void run ();
};

void CodeGeneratorQt::run ()
{
  NamespaceHelper nspace(stdout);

  if (Conf::generateProcedures)
    {
      vector<MethodDef>::const_iterator mi;
      for (mi = parser.getProcedures().begin(); mi != parser.getProcedures().end(); mi++)
	{
	  if (mi->result.type == "void" && mi->params.empty())
	    {
	      nspace.setFromSymbol (mi->name);
	      string mname = makeLMixedName(nspace.printableForm (mi->name));
	      string dname = makeLowerName(mi->name, '-');

	      printf("void %s () {\n", mname.c_str());
	      printf("  sfi_glue_vcall_void (\"%s\", 0);\n", dname.c_str());
	      printf("}\n");
	    }
	}
    }
}

void exitUsage(char *name)
{
  fprintf(stderr,"usage: %s [ <options> ] <idlfile>\n",name);
  fprintf(stderr,"\nOptions:\n");
  fprintf(stderr, " -g <target>  select code generation target (c, qt)\n");
  fprintf(stderr, " -x           generate extern statements\n");
  fprintf(stderr, " -d           generate data\n");
  fprintf(stderr, " -i <name>    generate init function\n");
  fprintf(stderr, " -t           generate c code for types (header)\n");
  fprintf(stderr, " -T           generate c code for types (impl)\n");
  fprintf(stderr, " -n <subst>   specify target namespace, either directly\n");
  fprintf(stderr, "              (-n Brahms) or as substitution (-n Bse/Bsw)\n");
  fprintf(stderr, " -b           generate boxed types registration code\n");
  fprintf(stderr, " -l           generate #line directives relative to .sfidl file\n");
  fprintf(stderr, " -s           generate signal stuff (pointless test code)\n");
  fprintf(stderr, " -p           generate procedure/method wrappers\n");
  exit(1);
}

int main (int argc, char **argv)
{
  CodeGenerator *codeGenerator = 0;

  /*
   * parse command line options
   */
  int c;
  while((c = getopt(argc, argv, "xdi:tTn:blsg:p")) != -1)
    {
      switch(c)
	{
	case 'x': Conf::generateExtern = true;
	  break;
	case 'd': Conf::generateData = true;
	  break;
	case 'i': Conf::generateInit = optarg;
	  break;
	case 't': Conf::generateTypeH = true;
	  break;
	case 'T': Conf::generateTypeC = true;
	  break;
	case 'n': {
		    string sub = optarg;

		    int i = sub.find ("/", 0);
		    if(i > 0)
		    {
		      Conf::namespaceCut = sub.substr (0, i);
		      Conf::namespaceAdd = sub.substr (i+1, sub.size()-(i+1));
		    }
		    else
		    {
		      Conf::namespaceAdd = sub;
		    }
		  }
	  break;
	case 'b': Conf::generateBoxedTypes = true;
	  break;
	case 'l': Conf::generateIdlLineNumbers = true;
	  break;
	case 's': Conf::generateSignalStuff = true;
	  break;
	case 'p': Conf::generateProcedures = true;
	  break;
	case 'g': Conf::target = optarg;
	  break;
	default:  exitUsage(argv[0]);
	  break;
	}
    }
  if((argc-optind) != 1) exitUsage(argv[0]);
  const char *inputfile = argv[optind];
  
  int fd = open (inputfile, O_RDONLY);
  if (fd < 0)
    {
      fprintf(stderr, "can't open inputfile %s (%s)\n", inputfile, strerror (errno));
      return 1;
    }
  
  IdlParser parser (inputfile, fd);
  if (!parser.parse())
    {
      /* parse error */
      return 1;
    }

  if (Conf::target == "c")
    codeGenerator = new CodeGeneratorC (parser);

  if (Conf::target == "qt")
    codeGenerator = new CodeGeneratorQt (parser);

  if (!codeGenerator)
    {
      fprintf(stderr, "no known target given (-g option)\n");
      return 1;
    }

  printf("\n/*-------- begin %s generated code --------*/\n\n\n",argv[0]);
  codeGenerator->run ();
  printf("\n\n/*-------- end %s generated code --------*/\n\n\n",argv[0]);

  delete codeGenerator;
  return 0;
}

/* vim:set ts=8 sts=2 sw=2: */
