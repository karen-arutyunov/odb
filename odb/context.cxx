// file      : odb/context.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

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
}

context::
context (ostream& os_,
         semantics::unit& u,
         options_type const& ops,
         data_ptr d)
    : data_ (d ? d : data_ptr (new (shared) data)),
      os (os_),
      unit (u),
      options (ops),
      keyword_set (data_->keyword_set_)
{
  for (size_t i (0); i < sizeof (keywords) / sizeof (char*); ++i)
    data_->keyword_set_.insert (keywords[i]);
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

context::
~context ()
{
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
  return m.count ("column") ? m.get<string> ("column") : public_name_impl (m);
}

string context::
column_type (semantics::data_member& m) const
{
  if (m.count ("type"))
    return m.get<string> ("type");

  semantics::type& t (m.type ());

  if (t.count ("type"))
    return t.get<string> ("type");

  // Don't use the name hint here so that we get the primary name (e.g.,
  // ::std::string) instead of a user typedef (e.g., my_string).
  //
  string const& name (t.fq_name ());
  type_map_type::const_iterator i (data_->type_map_.find (name));

  if (i != data_->type_map_.end ())
    return m.count ("id") ? i->second.id_type : i->second.type;

  cerr << m.file () << ":" << m.line () << ":" << m.column () << ":"
       << " error: unable to map C++ type '" << name << "' used in "
       << "data member '" << m.name () << "' to a database type" << endl;

  cerr << m.file () << ":" << m.line () << ":" << m.column () << ":"
       << " info: use '#pragma db type' to specify the database type"
       << endl;

  throw generation_failed ();
}

string context::
public_name (semantics::data_member& m) const
{
  return escape (public_name_impl (m));
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

namespace
{
  struct column_count_impl: traversal::class_
  {
    column_count_impl ()
        : member_ (*this)
    {
      *this >> names_ >> member_;
      *this >> inherits_ >> *this;
    }

    size_t
    count () const
    {
      return member_.count_;
    }

    virtual void
    traverse (semantics::class_& c)
    {
      if (c.count ("column-count"))
        member_.count_ += c.get<size_t> ("column-count");
      else
      {
        size_t n (member_.count_);
        inherits (c);
        names (c);
        c.set ("column-count", member_.count_ - n);
      }
    }

  private:
    struct member: traversal::data_member
    {
      member (column_count_impl& cc): count_ (0), cc_ (cc) {}

      virtual void
      traverse (semantics::data_member& m)
      {
        if (m.count ("transient"))
          return;

        if (context::comp_value (m.type ()))
          cc_.dispatch (m.type ());
        else
          count_++;
      }

      size_t count_;
      column_count_impl& cc_;
    };

    member member_;
    traversal::names names_;

    traversal::inherits inherits_;
  };
}

size_t context::
column_count (semantics::class_& c)
{
  if (c.count ("column-count"))
    return c.get<size_t> ("column-count");

  column_count_impl t;
  t.traverse (c);
  return t.count ();
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
