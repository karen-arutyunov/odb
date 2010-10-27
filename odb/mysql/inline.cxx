// file      : odb/mysql/inline.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/mysql/common.hxx>
#include <odb/mysql/inline.hxx>

namespace mysql
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

        if (c.count ("object"))
          traverse_object (c);
        else if (comp_value (c))
          traverse_value (c);
      }

      virtual void
      traverse_object (type& c)
      {
        string const& type (c.fq_name ());
        string traits ("access::object_traits< " + type + " >");

        id_member t;
        t.traverse (c);
        semantics::data_member& id (*t.member ());

        os << "// " << c.name () << endl
           << "//" << endl
           << endl;

        // query_type
        //
        if (options.generate_query ())
        {
          os << "inline" << endl
             << traits << "::query_type::" << endl
             << "query_type ()"
             << "{"
             << "}";

          os << "inline" << endl
             << traits << "::query_type::" << endl
             << "query_type (const std::string& q)" << endl
             << "  : query_base_type (q)"
             << "{"
             << "}";

          os << "inline" << endl
             << traits << "::query_type::" << endl
             << "query_type (const query_base_type& q)" << endl
             << "  : query_base_type (q)"
             << "{"
             << "}";
        }

        // id ()
        //
        os << "inline" << endl
           << traits << "::id_type" << endl
           << traits << "::" << endl
           << "id (const object_type& obj)"
           << "{"
           << "return obj." << id.name () << ";" << endl
           << "}";
      }

      virtual void
      traverse_value (type&)
      {
        /*
        string const& type (c.fq_name ());
        string traits ("access::composite_value_traits< " + type + " >");

        os << "// " << c.name () << endl
           << "//" << endl
           << endl;

        */
      }
    };
  }

  void
  generate_inline (context& ctx)
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
