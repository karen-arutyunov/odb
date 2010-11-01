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

class context
{
public:
  typedef std::size_t size_t;
  typedef std::string string;
  typedef ::options options_type;

  // Predicates.
  //
public:

  // Composite value type is a class type that was explicitly marked
  // as value type and there was no SQL type mapping provided for it
  // by the user (specifying the SQL type makes the value type simple).
  //
  static bool
  comp_value (semantics::class_& c)
  {
    return c.count ("value") && !c.count ("type");
  }

  // Return the class object if this type is a composite value type
  // and NULL otherwise.
  //
  static semantics::class_*
  comp_value (semantics::type& t)
  {
    semantics::class_* c (dynamic_cast<semantics::class_*> (&t));
    return c != 0 && t.count ("value") && !t.count ("type") ? c : 0;
  }

  // Database names and types.
  //
public:
  string
  table_name (semantics::type&) const;

  string
  column_name (semantics::data_member&) const;

  virtual string
  column_type (semantics::data_member&) const;

  // C++ names.
  //
public:
  // Cleaned-up member name that can be used in public interfaces.
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
  column_count (semantics::class_&);

  // Per-database customizable functionality.
  //
public:
  // Return empty string if there is no mapping.
  //
  virtual string
  column_type_impl (semantics::data_member&) const;

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
  struct data
  {
    virtual ~data () {}

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
