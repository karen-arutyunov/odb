// file      : odb/tracer/source.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/common.hxx>
#include <odb/tracer/source.hxx>

namespace tracer
{
  namespace
  {
    struct class_: traversal::class_, context
    {
      class_ (context& c)
          : context (c)
      {
      }

      virtual void
      traverse (type& c)
      {
        if (c.file () != unit.file ())
          return;

        if (!c.count ("object"))
          return;

        string const& type (c.fq_name ());
        string traits ("access::object_traits< " + type + " >");

        semantics::data_member& id (id_member (c));
        bool auto_id (id.count ("auto"));

        os << "// " << c.name () << endl
           << "//" << endl
           << endl;

        // type_name ()
        //
        os << "const char* " << traits << "::" << endl
           << "type_name ()"
           << "{"
           << "return \"" << type << "\";"
           << "}";

        // persist ()
        //
        os << "void " << traits << "::" << endl
           << "persist (database&, " << (auto_id ? "" : "const ") <<
          "object_type& obj)"
           << "{"
           << "std::cout << \"insert \" << type_name () << \" id \" << " <<
          "id (obj) << std::endl;"
           << endl
           << "if (id (obj) == id_type ())" << endl
           << "throw object_already_persistent ();"
           << "}";

        // update ()
        //
        os << "void " << traits << "::" << endl
           << "update (database&, const object_type& obj)"
           << "{"
           << "std::cout << \"update \" << type_name () << \" id \" << " <<
          "id (obj) << std::endl;"
           << endl
           << "if (id (obj) == id_type ())" << endl
           << "throw object_not_persistent ();"
           << "}";

        // erase ()
        //
        os << "void " << traits << "::" << endl
           << "erase (database&, const id_type& id)"
           << "{"
           << "std::cout << \"delete \" << type_name () << \" id \" << " <<
          "id << std::endl;"
           << endl
           << "if (id == id_type ())" << endl
           << "throw object_not_persistent ();"
           << "}";

        // find ()
        //
        os << traits << "::pointer_type" << endl
           << traits << "::" << endl
           << "find (database&, const id_type& id)"
           << "{"
           << "std::cout << \"select \" << type_name () << \" id \" << " <<
          "id << std::endl;"
           << endl
           << "if (id == id_type ())" << endl
           << "return pointer_type (0);"
           << endl
           << "pointer_type r (access::object_factory< object_type, " <<
          "pointer_type  >::create ());"
           << "pointer_traits< pointer_type >::guard g (r);"
           << "r->" << id.name () << " = id;"
           << "g.release ();"
           << "return r;"
           << "}";

        os << "bool " << traits << "::" << endl
           << "find (database&, const id_type& id, object_type& obj)"
           << "{"
           << "std::cout << \"select \" << type_name () << \" id \" << " <<
          "id << std::endl;"
           << endl
           << "if (id == id_type ())" << endl
           << "return false;"
           << endl
           << "obj." << id.name () << " = id;"
           << "return true;"
           << "}";
      }
    };
  }

  void
  generate_source (context& ctx)
  {
    traversal::unit unit;
    traversal::defines unit_defines;
    traversal::namespace_ ns;
    class_ c (ctx);

    unit >> unit_defines >> ns;
    unit_defines >> c;

    traversal::defines ns_defines;

    ns >> ns_defines >> ns;
    ns_defines >> c;

    ctx.os << "#include <iostream>" << endl
           << endl;

    ctx.os << "#include <odb/exceptions.hxx>" << endl
           << endl;

    ctx.os << "namespace odb"
           << "{";

    unit.dispatch (ctx.unit);

    ctx.os << "}";
  }
}
