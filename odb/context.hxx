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

  // Database names and types.
  //
public:
  // Cleaned-up member name that can be used in public interfaces
  // such as queries, column names, etc.
  //
  string
  public_name (semantics::data_member&) const;

  string
  table_name (semantics::type&) const;

  string
  column_name (semantics::data_member&) const;

  virtual string
  column_type (semantics::data_member&) const;

public:
  // Escape C++ keywords, reserved names, and illegal characters.
  //
  string
  escape (string const&) const;

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
