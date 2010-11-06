// file      : odb/tracer/header.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/common.hxx>
#include <odb/tracer/header.hxx>

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

        semantics::data_member& id (id_member (c));
        bool auto_id (id.count ("auto"));

        os << "// " << c.name () << endl
           << "//" << endl;

        os << "template <>" << endl
           << "class access::object_traits< " << type << " >: " << endl
           << "  public access::object_memory< " << type << " >," << endl
           << "  public access::object_factory< " << type << " >"
           << "{"
           << "public:" << endl;

        // object_type & shared_ptr
        //
        os << "typedef " << type << " object_type;";

        // id_type
        //
        os << "typedef " << id.type ().fq_name (id.belongs ().hint ()) <<
          " id_type;"
           << endl;

        // type_name ()
        //
        os << "static const char*" << endl
           << "type_name ();"
           << endl;

        // id ()
        //
        os << "static id_type" << endl
           << "id (const object_type&);"
           << endl;

        // persist ()
        //
        os << "static void" << endl
           << "persist (database&, " << (auto_id ? "" : "const ") <<
          "object_type&);"
           << endl;

        // update ()
        //
        os << "static void" << endl
           << "update (database&, const object_type&);"
           << endl;

        // erase ()
        //
        os << "static void" << endl
           << "erase (database&, const id_type&);"
           << endl;

        // find ()
        //
        os << "static pointer_type" << endl
           << "find (database&, const id_type&);"
           << endl;

        os << "static bool" << endl
           << "find (database&, const id_type&, object_type&);";

        os << "};";
      }
    };
  }

  void
  generate_header (context& ctx)
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

    ctx.os << "namespace odb"
           << "{";

    unit.dispatch (ctx.unit);

    ctx.os << "}";
  }
}
