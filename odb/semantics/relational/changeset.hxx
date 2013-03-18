// file      : odb/semantics/relational/changeset.hxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_CHANGESET_HXX
#define ODB_SEMANTICS_RELATIONAL_CHANGESET_HXX

#include <odb/semantics/relational/elements.hxx>

namespace semantics
{
  namespace relational
  {
    class changeset: public qscope
    {
    public:
      typedef relational::version version_type;

      version_type
      version () const {return version_;}

    public:
      changeset (version_type v): version_ (v) {}
      changeset (changeset const&, graph&);
      changeset (xml::parser&, graph&);

      virtual string
      kind () const {return "changeset";}

      virtual void
      serialize (xml::serializer&) const;

    public:
      using qscope::add_edge_left;
      using qscope::add_edge_right;

    private:
      version_type version_;
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_CHANGESET_HXX
