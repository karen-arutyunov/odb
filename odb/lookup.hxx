// file      : odb/lookup.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_LOOKUP_HXX
#define ODB_LOOKUP_HXX

#include <odb/gcc.hxx>

#include <string>

#include <odb/cxx-lexer.hxx>

namespace lookup
{
  struct invalid_name
  {
  };

  struct unable_to_resolve
  {
    unable_to_resolve (std::string const& n, bool l): name_ (n), last_ (l) {}

    std::string const&
    name () const {return name_;}

    // Last component in the name.
    //
    bool
    last () const {return last_;}

  private:
    std::string name_;
    bool last_;
  };

  std::string
  parse_scoped_name (std::string& token,
                     cpp_ttype& type,
                     cxx_lexer& lexer);

  tree
  resolve_scoped_name (std::string& token,
                       cpp_ttype& type,
                       cpp_ttype& previous_type,
                       cxx_lexer& lexer,
                       tree start_scope,
                       std::string& name,
                       bool is_type,
                       tree* end_scope = 0);
}

#endif // ODB_LOOKUP_HXX
