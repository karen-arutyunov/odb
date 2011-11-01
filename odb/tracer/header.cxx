// file      : odb/tracer/header.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/common.hxx>
#include <odb/context.hxx>
#include <odb/tracer/generate.hxx>

using namespace std;

namespace tracer
{
  namespace
  {
    struct class_: traversal::class_, context
    {
      virtual void
      traverse (type& c)
      {
        if (c.file () != unit.file ())
          return;

        if (!object (c))
          return;

        string const& type (c.fq_name ());

        semantics::data_member& id (*id_member (c));
        bool auto_id (id.count ("auto"));

        os << "// " << c.name () << endl
           << "//" << endl;

        os << "template <>" << endl
           << "class access::object_traits< " << type << " >"
           << "{"
           << "public:" << endl;

        // object_type & pointer_type
        //
        os << "typedef " << type << " object_type;"
           << "typedef object_type* pointer_type;";

        // id_type
        //
        {
          semantics::names* hint;
          semantics::type& t (utype (id, hint));

          os << "typedef " << t.fq_name (hint) << " id_type;"
             << endl;
        }

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

        // erase (id_type)
        //
        os << "static void" << endl
           << "erase (database&, const id_type&);"
           << endl;

        // erase (object_type)
        //
        os << "static void" << endl
           << "erase (database&, const object_type&);"
           << endl;

        // find ()
        //
        os << "static pointer_type" << endl
           << "find (database&, const id_type&);"
           << endl;

        os << "static bool" << endl
           << "find (database&, const id_type&, object_type&);";

        // callback ()
        //
        os << "static void" << endl
           << "callback (database&, object_type&, callback_event);"
           <<  endl;

        os << "static void" << endl
           << "callback (database&, const object_type&, callback_event);"
           <<  endl;

        os << "};";
      }
    };
  }

  namespace header
  {
    void
    generate ()
    {
      context ctx;
      ostream& os (ctx.os);

      traversal::unit unit;
      traversal::defines unit_defines;
      traversal::namespace_ ns;
      class_ c;

      unit >> unit_defines >> ns;
      unit_defines >> c;

      traversal::defines ns_defines;

      ns >> ns_defines >> ns;
      ns_defines >> c;

      os << "namespace odb"
         << "{";

      unit.dispatch (ctx.unit);

      os << "}";
    }
  }
}
