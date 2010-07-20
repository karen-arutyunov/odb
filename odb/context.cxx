// file      : odb/context.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/context.hxx>

using namespace std;

namespace
{
  char const* keywords[] =
  {
    "NULL",
    "and",
    "asm",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "class",
    "compl",
    "const",
    "const_cast",
    "continue",
    "default",
    "delete",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "end_eq",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "not",
    "not_eq",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "register",
    "reinterpret_cast",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_cast",
    "struct",
    "switch",
    "template",
    "this",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "wchar_t",
    "while",
    "xor",
    "xor_eq"
  };

  struct type_map_entry
  {
    const char* const cxx_type;
    const char* const db_type;
  };

  type_map_entry mysql_type_map[] =
  {
    {"bool", "TINYINT(1)"},

    {"char", "TINYINT"},
    {"signed char", "TINYINT"},
    {"unsigned char", "TINYINT UNSIGNED"},

    {"short int", "SMALLINT"},
    {"short unsigned int", "SMALLINT UNSIGNED"},

    {"int", "INT"},
    {"unsigned int", "INT UNSIGNED"},

    {"long int", "BIGINT"},
    {"long unsigned int", "BIGINT UNSIGNED"},

    {"long long int", "BIGINT"},
    {"long long unsigned int", "BIGINT UNSIGNED"},

    {"float", "FLOAT"},
    {"double", "DOUBLE"},

    {"::std::string", "TEXT"}
  };
}

context::
context (ostream& os_,
         semantics::unit& unit_,
         options_type const& ops)
    : data_ (new (shared) data),
      os (os_),
      unit (unit_),
      options (ops),
      keyword_set (data_->keyword_set_)
{
  for (size_t i (0); i < sizeof (keywords) / sizeof (char*); ++i)
    data_->keyword_set_.insert (keywords[i]);

  // Populate the C++ type to DB type map.
  //
  {
    size_t n;
    type_map_entry* p;

    switch (options.database ())
    {
    case database::mysql:
      {
        p = mysql_type_map;
        n = sizeof (mysql_type_map) / sizeof (type_map_entry);
        break;
      }
    default:
      {
        p = 0;
        n = 0;
        break;
      }
    }

    for (size_t i (0); i < n; ++i)
      data_->type_map_.insert (
        type_map_type::value_type (p[i].cxx_type, p[i].db_type));
  }
}

context::
context (context& c)
    : data_ (c.data_),
      os (c.os),
      unit (c.unit),
      options (c.options),
      keyword_set (c.keyword_set)
{
}

string context::
table_name (semantics::type& t) const
{
  if (t.count ("table"))
    return t.get<string> ("table");
  else
    return t.name ();
}

string context::
column_name (semantics::data_member& m) const
{
  if (m.count ("column"))
    return m.get<string> ("column");
  else
  {
    string s (m.name ());
    size_t n (s.size ());

    // Do basic processing: remove trailing and leading underscores
    // as well as the 'm_' prefix.
    //
    // @@ What if the resulting names conflict?
    //
    size_t b (0), e (n - 1);

    if (n > 2 && s[0] == 'm' && s[1] == '_')
      b += 2;

    for (; b <= e && s[b] == '_'; b++) ;
    for (; e >= b && s[e] == '_'; e--) ;

    return b > e ? s : string (s, b, e - b + 1);
  }
}

string context::
db_type (semantics::data_member& m) const
{
  if (m.count ("type"))
    return m.get<string> ("type");

  string const& name (m.type ().fq_name ());
  type_map_type::const_iterator i (data_->type_map_.find (name));

  if (i != data_->type_map_.end ())
    return i->second;

  cerr << m.file () << ":" << m.line () << ":" << m.column ()
       << " error: unable to map C++ type '" << name << "' used in "
       << "data member '" << m.name () << "' to a database type" << endl;

  cerr << m.file () << ":" << m.line () << ":" << m.column ()
       << " info: use '#pragma odb type' to specify the database type"
       << endl;

  throw generation_failed ();
}

string context::
escape (string const& name) const
{
  typedef string::size_type size;

  string r;
  size n (name.size ());

  // In most common cases we will have that many characters.
  //
  r.reserve (n);

  for (size i (0); i < n; ++i)
  {
    char c (name[i]);

    if (i == 0)
    {
      if (!((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '_'))
        r = (c >= '0' && c <= '9') ? "cxx_" : "cxx";
    }

    if (!((c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') ||
          c == '_'))
      r += '_';
    else
      r += c;
  }

  if (r.empty ())
    r = "cxx";

  // Custom reserved words.
  //
  /*
  reserved_name_map_type::const_iterator i (reserved_name_map.find (r));

  if (i != reserved_name_map.end ())
  {
    if (!i->second.empty ())
      return i->second;
    else
      r += L'_';
  }
  */

  // Keywords
  //
  if (keyword_set.find (r) != keyword_set.end ())
  {
    r += '_';

    // Re-run custom words.
    //
    /*
    i = reserved_name_map.find (r);

    if (i != reserved_name_map.end ())
    {
      if (!i->second.empty ())
        return i->second;
      else
        r += L'_';
    }
    */
  }

  return r;
}

// namespace
//

void namespace_::
traverse (type& ns)
{
  // Only traverse namespaces from the main file.
  //
  if (ns.file () == unit.file ())
  {
    string name (ns.name ());

    if (name.empty ())
      os << "namespace";
    else
      os << "namespace " << name;

    os << "{";

    traversal::namespace_::traverse (ns);

    os << "}";
  }
}
