// file      : odb/context.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_CONTEXT_HXX
#define ODB_CONTEXT_HXX

#include <map>
#include <set>
#include <string>
#include <ostream>
#include <cstddef> // std::size_t
#include <iostream>

#include <cutl/shared-ptr.hxx>

#include <odb/database.hxx>
#include <odb/options.hxx>
#include <odb/semantics.hxx>
#include <odb/traversal.hxx>

using std::endl;
using std::cerr;

class generation_failed {};

// Keep this enum synchronized with the one in libodb/odb/pointer-traits.hxx.
//
enum pointer_kind
{
  pk_raw,
  pk_unique,
  pk_shared,
  pk_weak
};

// Keep this enum synchronized with the one in libodb/odb/container-traits.hxx.
//
enum container_kind
{
  ck_ordered,
  ck_set,
  ck_multiset,
  ck_map,
  ck_multimap
};

class context
{
public:
  typedef std::size_t size_t;
  typedef std::string string;
  typedef ::options options_type;

public:
  static semantics::type&
  member_type (semantics::data_member& m, string const& key_prefix)
  {
    return key_prefix.empty ()
      ? m.type ()
      : *m.type ().get<semantics::type*> ("tree-" + key_prefix + "-type");
  }

  // Predicates.
  //
public:

  // Composite value type is a class type that was explicitly marked
  // as value type and there was no database type mapping provided for
  // it by the user (specifying the database type makes the value type
  // simple).
  //
  static bool
  comp_value (semantics::class_& c)
  {
    if (c.count ("composite-value"))
      return c.get<bool> ("composite-value");
    else
      return comp_value_ (c);
  }

  // Return the class object if this type is a composite value type
  // and NULL otherwise.
  //
  static semantics::class_*
  comp_value (semantics::type& t)
  {
    semantics::class_* c (dynamic_cast<semantics::class_*> (&t));
    return c != 0 && comp_value (*c) ? c : 0;
  }

  static bool
  container (semantics::type& t)
  {
    return t.count ("container-kind");
  }

  static semantics::class_*
  object_pointer (semantics::type& t)
  {
    return t.get<semantics::class_*> ("element-type", 0);
  }

  // Database names and types.
  //
public:
  string
  table_name (semantics::class_&) const;

  // Table name for the container member.
  //
  struct table_prefix
  {
    table_prefix () {}
    table_prefix (string const& p, size_t l): prefix (p), level (l) {}

    string prefix;
    size_t level;
  };

  string
  table_name (semantics::data_member&, table_prefix const&) const;

  string
  column_name (semantics::data_member&) const;

  string
  column_name (semantics::data_member&,
               string const& key_prefix,
               string const& default_name) const;

  virtual string
  column_type (semantics::data_member&,
               string const& key_prefix = string ()) const;

  // Cleaned-up member name that can be used for database names.
  //
  string
  public_name_db (semantics::data_member&) const;

  // C++ names.
  //
public:
  // Cleaned-up and escaped member name that can be used in public C++
  // interfaces.
  //
  string
  public_name (semantics::data_member&) const;

  // Escape C++ keywords, reserved names, and illegal characters.
  //
  string
  escape (string const&) const;

  // Counts and other information.
  //
public:
  static size_t
  in_column_count (semantics::class_&);

  static size_t
  out_column_count (semantics::class_&);

  semantics::data_member&
  id_member (semantics::class_&);

  // Object pointer information.
  //
public:
  typedef ::pointer_kind pointer_kind_type;

  static pointer_kind_type
  pointer_kind (semantics::type& p)
  {
    return p.get<pointer_kind_type> ("pointer-kind");
  }

  static bool
  lazy_pointer (semantics::type& p)
  {
    return p.get<bool> ("pointer-lazy");
  }

  static bool
  weak_pointer (semantics::type& p)
  {
    return pointer_kind (p) == pk_weak;
  }

  static bool
  null_pointer (semantics::data_member& m)
  {
    return !(m.count ("not-null") || m.type ().count ("not-null"));
  }

  static bool
  null_pointer (semantics::data_member& m, string const& key_prefix)
  {
    if (key_prefix.empty ())
      return null_pointer (m);

    return !(m.count (key_prefix + "-not-null") ||
             m.type ().count ("not-null") ||
             member_type (m, key_prefix).count ("not-null"));
  }

  static semantics::data_member*
  inverse (semantics::data_member& m)
  {
    return object_pointer (m.type ())
      ? m.get<semantics::data_member*> ("inverse", 0)
      : 0;
  }

  static semantics::data_member*
  inverse (semantics::data_member& m, string const& key_prefix)
  {
    if (key_prefix.empty ())
      return inverse (m);

    return object_pointer (member_type (m, key_prefix))
      ? m.get<semantics::data_member*> (key_prefix + "-inverse", 0)
      : 0;
  }

  // Container information.
  //
public:
  typedef ::container_kind container_kind_type;

  static container_kind_type
  container_kind (semantics::type& c)
  {
    return c.get<container_kind_type> ("container-kind");
  }

  static semantics::type&
  container_vt (semantics::type& c)
  {
    return *c.get<semantics::type*> ("tree-value-type");
  }

  static semantics::type&
  container_it (semantics::type& c)
  {
    return *c.get<semantics::type*> ("tree-index-type");
  }

  static semantics::type&
  container_kt (semantics::type& c)
  {
    return *c.get<semantics::type*> ("tree-key-type");
  }

  static bool
  unordered (semantics::data_member& m)
  {
    return m.count ("unordered") || m.type ().count ("unordered");
  }

  // The 'is a' and 'has a' tests. The has_a test currently does not
  // cross the container boundaries.
  //
public:
  static unsigned short const test_pointer = 0x01;
  static unsigned short const test_eager_pointer = 0x02;
  static unsigned short const test_lazy_pointer = 0x04;
  static unsigned short const test_container = 0x08;
  static unsigned short const test_straight_container = 0x10;
  static unsigned short const test_inverse_container = 0x20;

  static bool
  is_a (semantics::data_member& m, unsigned short flags)
  {
    return is_a (m, flags, m.type (), "");
  }

  static bool
  is_a (semantics::data_member&,
        unsigned short flags,
        semantics::type&,
        string const& key_prefix);

  static bool
  has_a (semantics::type&, unsigned short flags);

private:
  static bool
  comp_value_ (semantics::class_&);

protected:
  struct data;
  typedef cutl::shared_ptr<data> data_ptr;
  data_ptr data_;

public:
  std::ostream& os;
  semantics::unit& unit;
  options_type const& options;

  typedef std::set<string> keyword_set_type;
  keyword_set_type const& keyword_set;

  struct db_type_type
  {
    db_type_type () {}
    db_type_type (string const& t, string const& it)
        : type (t), id_type (it)
    {
    }

    string type;
    string id_type;
  };

  typedef std::map<string, db_type_type> type_map_type;

protected:
  typedef unsigned short column_type_flags;

  static column_type_flags const ctf_none = 0;

  // Default type should be NULL-able.
  //
  static column_type_flags const ctf_default_null = 0x01;

  // Get object id reference instead of object id.
  //
  static column_type_flags const ctf_object_id_ref = 0x02;

  struct data
  {
    virtual ~data () {}

    // Per-database customizable functionality.
    //
  public:
    // Return empty string if there is no mapping. The second argument
    // is the custom type or empty string if it is not specified.
    //
    virtual string
    column_type_impl (semantics::type&,
                      string const& type,
                      semantics::context&,
                      column_type_flags) const;

    keyword_set_type keyword_set_;
    type_map_type type_map_;
  };

public:
  context (std::ostream&,
           semantics::unit&,
           options_type const&,
           data_ptr = data_ptr ());
  context (context&);

  virtual
  ~context ();

private:
  context&
  operator= (context const&);
};

// Checks if scope Y names any of X.
//
template <typename X, typename Y>
bool
has (Y& y)
{
  for (semantics::scope::names_iterator i (y.names_begin ()),
         e (y.names_end ()); i != e; ++i)
    if (i->named (). template is_a<X> ())
      return true;

  return false;
}

// Standard namespace traverser.
//
struct namespace_: traversal::namespace_, context
{
  namespace_ (context& c) : context (c) {}

  virtual void
  traverse (type&);
};

#endif // ODB_CONTEXT_HXX
