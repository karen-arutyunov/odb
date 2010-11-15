// file      : odb/semantics/namespace.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_NAMESPACE_HXX
#define ODB_SEMANTICS_NAMESPACE_HXX

#include <odb/semantics/elements.hxx>

namespace semantics
{
  class namespace_: public scope
  {
  public:
    namespace_ (path const&, size_t line, size_t column, tree);

    // Resolve conflict between scope::scope and nameable::scope.
    //
    using nameable::scope;

  protected:
    namespace_ ();
  };
}

#endif // ODB_SEMANTICS_NAMESPACE_HXX
