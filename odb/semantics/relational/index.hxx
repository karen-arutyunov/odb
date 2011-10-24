// file      : odb/semantics/relational/index.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_INDEX_HXX
#define ODB_SEMANTICS_RELATIONAL_INDEX_HXX

#include <odb/semantics/relational/elements.hxx>
#include <odb/semantics/relational/key.hxx>
#include <odb/semantics/relational/table.hxx>

namespace semantics
{
  namespace relational
  {
    // Note that unlike other keys, indexes are defined in the model
    // scope, not table scope.
    //
    class index: public key
    {
    public:
      relational::table&
      table () const
      {
        return contains_begin ()->column ().table ();
      }

    public:
      index (string const& id)
          : key (id)
      {
      }

      virtual string
      kind () const
      {
        return "index";
      }
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_INDEX_HXX
