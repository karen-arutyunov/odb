// file      : odb/semantics/relational/model.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/relational.hxx>

namespace semantics
{
  namespace relational
  {
    model::
    model (xml::parser& p)
        : qscope (p, *this),
          version_ (p.attribute<version_type> ("version"))
    {
    }

    void model::
    serialize (xml::serializer& s) const
    {
      s.start_element (xmlns, "model");
      s.namespace_decl (xmlns, ""); // @@ evo
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

          // model
          //
          {
            type_info ti (typeid (model));
            ti.add_base (typeid (qscope));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
