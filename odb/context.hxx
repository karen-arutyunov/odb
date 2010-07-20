// file      : odb/context.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

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
  string
  table_name (semantics::type&) const;

  string
  column_name (semantics::data_member&) const;

  string
  db_type (semantics::data_member&) const;

public:
  // Escape C++ keywords, reserved names, and illegal characters.
  //
  string
  escape (string const&) const;

private:
  struct data;
  cutl::shared_ptr<data> data_;

public:
  std::ostream& os;
  semantics::unit& unit;
  options_type const& options;

  typedef std::set<string> keyword_set_type;
  keyword_set_type const& keyword_set;

  typedef std::map<string, string> type_map_type;

private:
  struct data
  {
    keyword_set_type keyword_set_;
    type_map_type type_map_;
  };

public:
  context (std::ostream&, semantics::unit&, options_type const&);
  context (context&);

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
