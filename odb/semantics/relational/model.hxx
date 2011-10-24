// file      : odb/semantics/relational/model.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_MODEL_HXX
#define ODB_SEMANTICS_RELATIONAL_MODEL_HXX

#include <odb/semantics/relational/elements.hxx>

namespace semantics
{
  namespace relational
  {
    class model: public graph<node, edge>, public scope
    {
    public:
      model ()
      {
      }

      virtual string
      kind () const
      {
        return "model";
      }

    public:
      using scope::add_edge_left;
      using scope::add_edge_right;

    private:
      model (model const&);
      model& operator= (model const&);
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_MODEL_HXX
