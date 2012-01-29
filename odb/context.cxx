// file      : odb/context.cxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <cctype> // std::toupper
#include <cassert>

#include <odb/context.hxx>
#include <odb/common.hxx>
#include <odb/pragma.hxx>

#include <odb/relational/mssql/context.hxx>
#include <odb/relational/mysql/context.hxx>
#include <odb/relational/oracle/context.hxx>
#include <odb/relational/pgsql/context.hxx>
#include <odb/relational/sqlite/context.hxx>

using namespace std;

//
// view_object
//

string view_object::
name () const
{
  if (!alias.empty ())
    return alias;

  return kind == object ? context::class_name (*obj) : tbl_name.string ();
}

//
// context
//

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
}

auto_ptr<context>
create_context (ostream& os,
                semantics::unit& unit,
                options const& ops,
                semantics::relational::model* m)
{
  auto_ptr<context> r;

  switch (ops.database ())
  {
  case database::mssql:
    {
      r.reset (new relational::mssql::context (os, unit, ops, m));
      break;
    }
  case database::mysql:
    {
      r.reset (new relational::mysql::context (os, unit, ops, m));
      break;
    }
  case database::oracle:
    {
      r.reset (new relational::oracle::context (os, unit, ops, m));
      break;
    }
  case database::pgsql:
    {
      r.reset (new relational::pgsql::context (os, unit, ops, m));
      break;
    }
  case database::sqlite:
    {
      r.reset (new relational::sqlite::context (os, unit, ops, m));
      break;
    }
  }

  return r;
}

context::
~context ()
{
  if (current_ == this)
    current_ = 0;
}

context::
context (ostream& os_,
         semantics::unit& u,
         options_type const& ops,
         data_ptr d)
    : data_ (d ? d : data_ptr (new (shared) data (os_))),
      os (data_->os_),
      unit (u),
      options (ops),
      db (options.database ()),
      keyword_set (data_->keyword_set_),
      include_regex (data_->include_regex_),
      embedded_schema (ops.generate_schema () &&
                       ops.schema_format ().count (schema_format::embedded)),
      top_object (data_->top_object_),
      cur_object (data_->cur_object_)
{
  assert (current_ == 0);
  current_ = this;

  for (size_t i (0); i < sizeof (keywords) / sizeof (char*); ++i)
    data_->keyword_set_.insert (keywords[i]);

  for (strings::const_iterator i (ops.include_regex ().begin ());
       i != ops.include_regex ().end (); ++i)
    data_->include_regex_.push_back (regexsub (*i));
}

context::
context ()
  : data_ (current ().data_),
    os (current ().os),
    unit (current ().unit),
    options (current ().options),
    db (current ().db),
    keyword_set (current ().keyword_set),
    include_regex (current ().include_regex),
    embedded_schema (current ().embedded_schema),
    top_object (current ().top_object),
    cur_object (current ().cur_object)
{
}

context* context::current_;

bool context::
readonly (data_member_path const& mp, data_member_scope const& ms)
{
  assert (mp.size () == ms.size ());

  data_member_scope::const_reverse_iterator si (ms.rbegin ());

  for (data_member_path::const_reverse_iterator pi (mp.rbegin ());
       pi != mp.rend ();
       ++pi, ++si)
  {
    semantics::data_member& m (**pi);

    if (m.count ("readonly"))
      return true;

    // Check if any of the classes in the inheritance chain for the
    // class containing this member are readonly.
    //
    class_inheritance_chain const& ic (*si);

    assert (ic.back () == &m.scope ());

    for (class_inheritance_chain::const_reverse_iterator ci (ic.rbegin ());
         ci != ic.rend ();
         ++ci)
    {
      semantics::class_& c (**ci);

      if (c.count ("readonly"))
        return true;
    }
  }

  return false;
}

bool context::
readonly (semantics::data_member& m)
{
  if (m.count ("readonly"))
    return true;

  // Check if the whole class (object or composite value) is marked
  // as readonly.
  //
  if (m.scope ().count ("readonly"))
    return true;

  return false;
}

bool context::
null (semantics::data_member& m)
{
  semantics::type& t (utype (m));

  if (object_pointer (t))
  {
    // By default pointers can be null.
    //
    if (m.count ("null"))
      return true;

    if (!m.count ("not-null"))
    {
      if (t.count ("null"))
        return true;

      if (!t.count ("not-null"))
      {
        return true;
      }
    }

    return false;
  }
  else
  {
    // Everything else by default is not null.
    //
    if (m.count ("null"))
      return true;

    if (!m.count ("not-null"))
    {
      if (t.count ("null"))
        return true;

      if (!t.count ("not-null"))
      {
        // Check if this type is a wrapper.
        //
        if (t.get<bool> ("wrapper"))
        {
          // First see if it is null by default.
          //
          if (t.get<bool> ("wrapper-null-handler") &&
              t.get<bool> ("wrapper-null-default"))
            return true;

          // Otherwise, check the wrapped type.
          //
          if (t.get<semantics::type*> ("wrapper-type")->count ("null"))
            return true;
        }
      }
    }

    return false;
  }
}

bool context::
null (semantics::data_member& m, string const& kp)
{
  if (kp.empty ())
    return null (m);

  semantics::type& c (utype (m));
  semantics::type& t (member_utype (m, kp));

  if (object_pointer (t))
  {
    if (m.count (kp + "-null"))
      return true;

    if (!m.count (kp + "-not-null"))
    {
      if (c.count (kp + "-null"))
        return true;

      if (!c.count (kp + "-not-null"))
      {
        if (t.count ("null"))
          return true;

        if (!t.count ("not-null"))
        {
          return true;
        }
      }
    }

    return false;
  }
  else
  {
    if (m.count (kp + "-null"))
      return true;

    if (!m.count (kp + "-not-null"))
    {
      if (c.count (kp + "-null"))
        return true;

      if (!c.count (kp + "-not-null"))
      {
        if (t.count ("null"))
          return true;

        if (!t.count ("not-null"))
        {
          // Check if this type is a wrapper.
          //
          if (t.get<bool> ("wrapper"))
          {
            // First see if it is null by default.
            //
            if (t.get<bool> ("wrapper-null-handler") &&
                t.get<bool> ("wrapper-null-default"))
              return true;

            // Otherwise, check the wrapped type.
            //
            semantics::type& wt (
              utype (*t.get<semantics::type*> ("wrapper-type")));

            if (wt.count ("null"))
              return true;
          }
        }
      }
    }

    return false;
  }
}

context::class_kind_type context::
class_kind (semantics::class_& c)
{
  if (object (c))
    return class_object;
  else if (view (c))
    return class_view;
  else if (composite (c))
    return class_composite;
  else
    return class_other;
}

string context::
class_name (semantics::class_& c)
{
  return c.is_a<semantics::class_instantiation> ()
    ? c.get<semantics::names*> ("tree-hint")->name ()
    : c.name ();
}

string context::
class_fq_name (semantics::class_& c)
{
  return c.is_a<semantics::class_instantiation> ()
    ? c.fq_name (c.get<semantics::names*> ("tree-hint"))
    : c.fq_name ();
}

semantics::path context::
class_file (semantics::class_& c)
{
  return c.is_a<semantics::class_instantiation> ()
    ? semantics::path (LOCATION_FILE (c.get<location_t> ("location")))
    : c.file ();
}

string context::
upcase (string const& s)
{
  string r;
  string::size_type n (s.size ());

  r.reserve (n);

  for (string::size_type i (0); i < n; ++i)
    r.push_back (toupper (s[i]));

  return r;
}

void context::
diverge (streambuf* sb)
{
  data_->os_stack_.push (data_->os_.rdbuf ());
  data_->os_.rdbuf (sb);
}

void context::
restore ()
{
  data_->os_.rdbuf (data_->os_stack_.top ());
  data_->os_stack_.pop ();
}

semantics::type& context::
utype (semantics::type& t)
{
  if (semantics::qualifier* q = dynamic_cast<semantics::qualifier*> (&t))
    return q->base_type ();
  else
    return t;
}

semantics::type& context::
utype (semantics::type& t, semantics::names*& hint)
{
  if (semantics::qualifier* q = dynamic_cast<semantics::qualifier*> (&t))
  {
    hint = q->qualifies ().hint ();
    return q->base_type ();
  }
  else
    return t;
}

semantics::type& context::
utype (semantics::data_member& m, semantics::names*& hint)
{
  semantics::type& t (m.type ());

  if (semantics::qualifier* q = dynamic_cast<semantics::qualifier*> (&t))
  {
    hint = q->qualifies ().hint ();
    return q->base_type ();
  }
  else
  {
    hint = m.belongs ().hint ();
    return t;
  }
}

bool context::
const_type (semantics::type& t)
{
  if (semantics::qualifier* q = dynamic_cast<semantics::qualifier*> (&t))
    return q->const_ ();

  return false;
}

semantics::type& context::
member_type (semantics::data_member& m, string const& key_prefix)
{
  // This function returns the potentially-qualified type but for
  // intermediate types we use unqualified versions.
  //
  if (key_prefix.empty ())
    return m.type ();

  string const key (key_prefix + "-tree-type");

  if (m.count (key))
    return *indirect_value<semantics::type*> (m, key);

  // "See throught" wrappers.
  //
  semantics::type& t (utype (m));

  if (semantics::type* wt = wrapper (t))
    return *indirect_value<semantics::type*> (utype (*wt), key);
  else
    return *indirect_value<semantics::type*> (t, key);
}

bool context::
composite_ (semantics::class_& c)
{
  bool r (true);

  if (c.count ("value"))
  {
    for (pragma_name_set::const_iterator i (simple_value_pragmas_.begin ()),
           e (simple_value_pragmas_.end ()); i != e; ++i)
    {
      if (c.count (*i))
      {
        r = false;
        break;
      }
    }
  }
  else
    r = false;

  c.set ("composite-value", r);
  return r;
}

qname context::
schema (semantics::scope& s) const
{
  if (s.count ("qualified-schema"))
    return s.get<qname> ("qualified-schema");

  qname r;

  for (semantics::scope* ps (&s);; ps = &ps->scope_ ())
  {
    using semantics::namespace_;

    namespace_* ns (dynamic_cast<namespace_*> (ps));

    if (ns == 0)
      continue; // Some other scope.

    if (ns->extension ())
      ns = &ns->original ();

    if (ns->count ("schema"))
    {
      qname n (ns->get<qname> ("schema"));
      n.append (r);
      n.swap (r);

      if (r.fully_qualified ())
        break;
    }

    if (ns->global_scope ())
      break;
  }

  // If we are still not fully qualified, add the schema that was
  // specified on the command line.
  //
  if (!r.fully_qualified () && options.schema_specified ())
  {
    qname n (options.schema ());
    n.append (r);
    n.swap (r);
  }

  s.set ("qualified-schema", r);
  return r;
}

qname context::
table_name (semantics::class_& c) const
{
  if (c.count ("qualified-table"))
    return c.get<qname> ("qualified-table");

  qname r;

  bool sf (c.count ("schema"));

  if (c.count ("table"))
  {
    r = c.get<qname> ("table");

    if (sf)
    {
      // If we have both schema and qualified table, see which takes
      // precedence based on order. If the table is unqualifed, then
      // add the schema.
      //
      sf = !r.qualified () ||
        c.get<location_t> ("table-location") <
        c.get<location_t> ("schema-location");
    }
  }
  else
    r = class_name (c);

  if (sf)
  {
    qname n (c.get<qname> ("schema"));
    n.append (r.uname ());
    n.swap (r);
  }

  // Unless we are fully qualified, add any schemas that were
  // specified on the namespaces and/or with the command line
  // option.
  //
  if (!r.fully_qualified ())
  {
    qname n (schema (c.scope ()));
    n.append (r);
    n.swap (r);
  }

  // Add the table prefix if specified.
  //
  if (options.table_prefix_specified ())
    r.uname () = options.table_prefix () + r.uname ();

  c.set ("qualified-table", r);
  return r;
}

qname context::
table_name (semantics::class_& obj, data_member_path const& mp) const
{
  table_prefix tp (schema (obj.scope ()), table_name (obj) + "_", 1);

  if (mp.size () == 1)
  {
    // Container directly in the object.
    //
    return table_name (*mp.back (), tp);
  }
  else
  {
    data_member_path::const_iterator i (mp.begin ());

    // The last member is the container.
    //
    for (data_member_path::const_iterator e (mp.end () - 1); i != e; ++i)
      object_members_base::append (**i, tp);

    return table_name (**i, tp);
  }
}

qname context::
table_name (semantics::data_member& m, table_prefix const& p) const
{
  // The table prefix passed as the second argument must include
  // the table prefix specified with the --table-prefix option.
  //
  string const& gp (options.table_prefix ());
  assert (p.prefix.uname ().compare (0, gp.size (), gp) == 0);

  qname r;

  // If a custom table name was specified, then ignore the top-level
  // table prefix (this corresponds to a container directly inside an
  // object). If the container table is unqualifed, then we use the
  // object schema. If it is fully qualified, then we use that name.
  // Finally, if it is qualified by not fully qualifed, then we
  // append the object's namespace schema.
  //
  if (m.count ("table"))
  {
    qname n (m.get<qname> ("table"));

    if (n.fully_qualified ())
      r = n.qualifier ();
    else
    {
      if (n.qualified ())
      {
        r = p.schema;
        r.append (n.qualifier ());
      }
      else
        r = p.prefix.qualifier ();
    }

    r.append (p.level == 1 ? gp : p.prefix.uname ());
    r += n.uname ();
  }
  else
  {
    r = p.prefix;
    r += public_name_db (m);
  }

  return r;
}

string context::
column_name (semantics::data_member& m) const
{
  if (m.count ("column"))
    return m.get<table_column> ("column").column;
  else
    return public_name_db (m);
}

string context::
column_name (data_member_path const& mp) const
{
  // The path can lead to a composite value member and column names for
  // such members are derived dynamically using the same derivation
  // process as when generating object columns (see object_columns_base).
  //
  string r;

  for (data_member_path::const_iterator i (mp.begin ()); i != mp.end (); ++i)
  {
    semantics::data_member& m (**i);

    if (composite_wrapper (utype (m)))
      r += object_columns_base::column_prefix (m);
    else
      r += column_name (m);
  }

  return r;
}

string context::
column_name (semantics::data_member& m, string const& p, string const& d) const
{
  // A container column name can be specified for the member or for the
  // container type.
  //
  string key (p + "-column");

  if (m.count (key))
    return m.get<string> (key);
  else
  {
    semantics::type& t (utype (m));

    if (t.count (key))
      return t.get<string> (key);
  }

  return d;
}

string context::
column_type (semantics::data_member& m, string const& kp)
{
  return kp.empty ()
    ? m.get<string> ("column-type")
    : indirect_value<string> (m, kp + "-column-type");
}

string context::
column_options (semantics::data_member& m)
{
  // Accumulate options from both type and member.
  //
  semantics::type& t (utype (m));

  string r;

  if (t.count ("options"))
  {
    strings const& o (t.get<strings> ("options"));

    for (strings::const_iterator i (o.begin ()); i != o.end (); ++i)
    {
      if (i->empty ())
        r.clear ();
      else
      {
        if (!r.empty ())
          r += ' ';

        r += *i;
      }
    }
  }

  if (m.count ("options"))
  {
    strings const& o (m.get<strings> ("options"));

    for (strings::const_iterator i (o.begin ()); i != o.end (); ++i)
    {
      if (i->empty ())
        r.clear ();
      else
      {
        if (!r.empty ())
          r += ' ';

        r += *i;
      }
    }
  }

  return r;
}

string context::
column_options (semantics::data_member& m, string const& kp)
{
  if (kp.empty ())
    return column_options (m);

  string k (kp + "-options");

  // Accumulate options from type, container, and member.
  //
  semantics::type& c (utype (m));
  semantics::type& t (member_utype (m, kp));

  string r;

  if (t.count ("options"))
  {
    strings const& o (t.get<strings> ("options"));

    for (strings::const_iterator i (o.begin ()); i != o.end (); ++i)
    {
      if (i->empty ())
        r.clear ();
      else
      {
        if (!r.empty ())
          r += ' ';

        r += *i;
      }
    }
  }

  if (c.count (k))
  {
    strings const& o (c.get<strings> (k));

    for (strings::const_iterator i (o.begin ()); i != o.end (); ++i)
    {
      if (i->empty ())
        r.clear ();
      else
      {
        if (!r.empty ())
          r += ' ';

        r += *i;
      }
    }
  }

  if (m.count (k))
  {
    strings const& o (m.get<strings> (k));

    for (strings::const_iterator i (o.begin ()); i != o.end (); ++i)
    {
      if (i->empty ())
        r.clear ();
      else
      {
        if (!r.empty ())
          r += ' ';

        r += *i;
      }
    }
  }

  return r;
}

string context::
database_type_impl (semantics::type& t, semantics::names* hint, bool id)
{
  type_map_type::const_iterator end (data_->type_map_.end ()), i (end);

  // First check the hinted name. This allows us to handle things like
  // size_t which is nice to map to the same type irrespective of the
  // actual type. Since this type can be an alias for the one we are
  // interested in, go into nested hints.
  //
  for (; hint != 0 && i == end; hint = hint->hint ())
  {
    i = data_->type_map_.find (t.fq_name (hint));
  }

  // If the hinted name didn't work, try the primary name (e.g.,
  // ::std::string) instead of a user typedef (e.g., my_string).
  //
  if (i == end)
    i = data_->type_map_.find (t.fq_name ());

  if (i != end)
    return id ? i->second.id_type : i->second.type;
  else
    return string ();
}

static string
public_name_impl (semantics::data_member& m)
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

string context::
public_name_db (semantics::data_member& m) const
{
  return public_name_impl (m);
}

string context::
public_name (semantics::data_member& m) const
{
  return escape (public_name_impl (m));
}

string context::
flat_name (string const& fq)
{
  string r;
  r.reserve (fq.size ());

  for (string::size_type i (0), n (fq.size ()); i < n; ++i)
  {
    char c (fq[i]);

    if (c == ':')
    {
      if (!r.empty ())
        r += '_';
      ++i; // Skip the second ':'.
    }
    else
      r += c;
  }

  return r;
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

static string
charlit (unsigned int u)
{
  string r ("\\x");
  bool lead (true);

  for (short i (7); i >= 0; --i)
  {
    unsigned int x ((u >> (i * 4)) & 0x0F);

    if (lead)
    {
      if (x == 0)
        continue;

      lead = false;
    }

    r += static_cast<char> (x < 10 ? ('0' + x) : ('A' + x - 10));
  }

  return r;
}

static string
strlit_ascii (string const& str)
{
  string r;
  string::size_type n (str.size ());

  // In most common cases we will have that many chars.
  //
  r.reserve (n + 2);

  r += '"';

  bool escape (false);

  for (string::size_type i (0); i < n; ++i)
  {
    unsigned int u (static_cast<unsigned int> (str[i]));

    // [128 - ]     - unrepresentable
    // 127          - \x7F
    // [32  - 126]  - as is
    // [0   - 31]   - \X or \xXX
    //

    if (u < 32 || u == 127)
    {
      switch (u)
      {
      case '\n':
        {
          r += "\\n";
          break;
        }
      case '\t':
        {
          r += "\\t";
          break;
        }
      case '\v':
        {
          r += "\\v";
          break;
        }
      case '\b':
        {
          r += "\\b";
          break;
        }
      case '\r':
        {
          r += "\\r";
          break;
        }
      case '\f':
        {
          r += "\\f";
          break;
        }
      case '\a':
        {
          r += "\\a";
          break;
        }
      default:
        {
          r += charlit (u);
          escape = true;
          break;
        }
      }
    }
    else if (u < 127)
    {
      if (escape)
      {
        // Close and open the string so there are no clashes.
        //
        r += '"';
        r += '"';

        escape = false;
      }

      switch (u)
      {
      case '"':
        {
          r += "\\\"";
          break;
        }
      case '\\':
        {
          r += "\\\\";
          break;
        }
      default:
        {
          r += static_cast<char> (u);
          break;
        }
      }
    }
    else
    {
      // @@ Unrepresentable character.
      //
      r += '?';
    }
  }

  r += '"';

  return r;
}

string context::
strlit (string const& str)
{
  return strlit_ascii (str);
}

namespace
{
  struct column_count_impl: object_members_base
  {
    virtual void
    traverse_simple (semantics::data_member& m)
    {
      c_.total++;

      if (m.count ("id"))
        c_.id++;
      else if (context::inverse (m))
        c_.inverse++;
      else if (context::readonly (member_path_, member_scope_))
        c_.readonly++;
      else if (context::version (m))
        c_.optimistic_managed++;
    }

    context::column_count_type c_;
  };
}

context::column_count_type context::
column_count (semantics::class_& c)
{
  if (!c.count ("column-count"))
  {
    column_count_impl t;
    t.traverse (c);
    c.set ("column-count", t.c_);
  }

  return c.get<column_count_type> ("column-count");
}

namespace
{
  struct has_a_impl: object_members_base
  {
    has_a_impl (unsigned short flags)
        : r_ (0), flags_ (flags)
    {
    }

    size_t
    result () const
    {
      return r_;
    }

    virtual void
    traverse_simple (semantics::data_member&)
    {
      if (context::is_a (member_path_, member_scope_, flags_))
        r_++;
    }

    virtual void
    traverse_container (semantics::data_member&, semantics::type& c)
    {
      // We don't cross the container boundaries (separate table).
      //
      unsigned short f (flags_ & (context::test_container |
                                  context::test_straight_container |
                                  context::test_inverse_container |
                                  context::test_readonly_container));

      if (context::is_a (member_path_,
                         member_scope_,
                         f,
                         context::container_vt (c),
                         "value"))
        r_++;
    }

    virtual void
    traverse_object (semantics::class_& c)
    {
      if ((flags_ & context::exclude_base) == 0)
        inherits (c);

      names (c);
    }

  private:
    size_t r_;
    unsigned short flags_;
  };
}

bool context::
is_a (data_member_path const& mp,
      data_member_scope const& ms,
      unsigned short f,
      semantics::type& t,
      string const& kp)
{
  bool r (false);

  semantics::data_member& m (*mp.back ());

  if (f & test_pointer)
    r = r || object_pointer (t);

  if (f & test_eager_pointer)
    r = r || (object_pointer (t) && !lazy_pointer (t));

  if (f & test_lazy_pointer)
    r = r || (object_pointer (t) && lazy_pointer (t));

  if ((f & (test_container |
            test_straight_container |
            test_inverse_container |
            test_readonly_container)) != 0)
  {
    if (f & test_container)
      r = r || container (m);

    if (f & test_straight_container)
      r = r || (container(m) && !inverse (m, kp));

    if (f & test_inverse_container)
      r = r || (container (m) && inverse (m, kp));

    if (f & test_readonly_container)
      r = r || (container (m) && readonly (mp, ms));
  }

  return r;
}

size_t context::
has_a (semantics::class_& c, unsigned short flags)
{
  has_a_impl impl (flags);
  impl.dispatch (c);
  return impl.result ();
}

string context::
process_include_path (string const& ip, bool prefix, char open)
{
  bool t (options.include_regex_trace ());
  string p (prefix ? options.include_prefix () : string ());

  if (!p.empty () && p[p.size () - 1] != '/')
    p.append ("/");

  string path (p + ip), r;

  if (t)
    cerr << "include: '" << path << "'" << endl;

  bool found (false);

  for (regex_mapping::const_iterator i (include_regex.begin ());
       i != include_regex.end (); ++i)
  {
    if (t)
      cerr << "try: '" << i->regex () << "' : ";

    if (i->match (path))
    {
      r = i->replace (path);
      found = true;

      if (t)
        cerr << "'" << r << "' : ";
    }

    if (t)
      cerr << (found ? '+' : '-') << endl;

    if (found)
      break;
  }

  if (!found)
    r = path;

  // Add brackets or quotes unless the path already has them.
  //
  if (!r.empty () && r[0] != '"' && r[0] != '<')
  {
    bool b (open == '<' || (open == '\0' && options.include_with_brackets ()));
    char op (b ? '<' : '"'), cl (b ? '>' : '"');
    r = op + r + cl;
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
