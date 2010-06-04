// file      : odb/source.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/common.hxx>
#include <odb/source.hxx>

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

      id_member t (*this);
      t.traverse (c);
      semantics::data_member& id (*t.member ());

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

      // insert ()
      //
      os << "void " << traits << "::" << endl
         << "insert (database&, const object_type& obj)"
         << "{"
         << "std::cout << \"insert \" << type_name () << \" id \" << " <<
        "id (obj) << std::endl;"
         << "}";

      // update ()
      //
      os << "void " << traits << "::" << endl
         << "update (database&, const object_type& obj)"
         << "{"
         << "std::cout << \"update \" << type_name () << \" id \" << " <<
        "id (obj) << std::endl;"
         << "}";

      // erase ()
      //
      os << "void " << traits << "::" << endl
         << "erase (database&, const id_type& id)"
         << "{"
         << "std::cout << \"delete \" << type_name () << \" id \" << " <<
        "id << std::endl;"
         << "}";

      // find ()
      //
      os << traits << "::shared_ptr " << endl
         << traits << "::" << endl
         << "find (database&, const id_type& id)"
         << "{"
         << "std::cout << \"select \" << type_name () << \" id \" << " <<
        "id << std::endl;"
         << "shared_ptr r (access::object_factory< " << type <<
        " >::create ());"
         << "r->" << id.name () << " = id;"
         << "return r;"
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

  //ctx.os << "#include <odb/database.hxx>" << endl
  //       << endl;

  ctx.os << "namespace odb"
         << "{";

  unit.dispatch (ctx.unit);

  ctx.os << "}";
}
