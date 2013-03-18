// file      : odb/semantics/relational/changeset.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>

#include <odb/semantics/relational/changeset.hxx>

namespace semantics
{
  namespace relational
  {
    changeset::
    changeset (changeset const& c, graph& g)
      : qscope (c, g),
        version_ (c.version_)
    {
    }

    changeset::
    changeset (xml::parser& p, graph& g)
        : qscope (p, g),
          version_ (p.attribute<version_type> ("version"))
    {
    }

    void changeset::
    serialize (xml::serializer& s) const
    {
      s.start_element (xmlns, "changeset");
      s.attribute ("version", version_);
      qscope::serialize_content (s);
      s.end_element ();
    }

    // type info
    //
    namespace
    {
      struct init
      {
        init ()
        {
          using compiler::type_info;

          {
            type_info ti (typeid (changeset));
            ti.add_base (typeid (qscope));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
