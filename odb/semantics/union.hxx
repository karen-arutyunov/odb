// file      : odb/semantics/union.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_UNION_HXX
#define ODB_SEMANTICS_UNION_HXX

#include <semantics/elements.hxx>

namespace semantics
{
  class union_: public virtual type, public scope
  {
  public:
    union_ (path const& file, size_t line, size_t column, tree tn)
        : node (file, line, column), type (tn)
    {
    }

    // Resolve conflict between scope::scope and nameable::scope.
    //
    using nameable::scope;

  protected:
    union_ ()
    {
    }
  };
}

#endif // ODB_SEMANTICS_UNION_HXX
