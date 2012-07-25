// file      : odb/lookup.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
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
  parse_scoped_name (cxx_lexer&,
                     cpp_ttype&,
                     std::string& tl, // Token literal.
                     tree& tn);       // Token node.

  // If trailing_scope is true, then this function also handles
  // names in the 'foo::bar::<something-other-than-name>' form.
  // In this case token will be <something-other-than-name> and
  // ptt will be CPP_SCOPE.
  //
  tree
  resolve_scoped_name (cxx_lexer&,
                       cpp_ttype&,
                       std::string& tl, // Token literal.
                       tree& tn,        // Token node.
                       cpp_ttype& ptt,  // Previous token type.
                       tree start_scope,
                       std::string& name,
                       bool is_type,
                       bool trailing_scope = false,
                       tree* end_scope = 0);
}

#endif // ODB_LOOKUP_HXX
