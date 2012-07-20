// file      : odb/semantics/relational/index.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_INDEX_HXX
#define ODB_SEMANTICS_RELATIONAL_INDEX_HXX

#include <odb/semantics/relational/elements.hxx>
#include <odb/semantics/relational/key.hxx>

namespace semantics
{
  namespace relational
  {
    // Note that in our model indexes are defined in the table scope.
    //
    class index: public key
    {
    public:
      index (string const& id): key (id) {}

      virtual string
      kind () const
      {
        return "index";
      }
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_INDEX_HXX
