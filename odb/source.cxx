// file      : odb/source.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/common.hxx>
#include <odb/context.hxx>
#include <odb/generate.hxx>
#include <odb/diagnostics.hxx>

using namespace std;

namespace source
{
  struct class_: traversal::class_, virtual context
  {
    virtual void
    traverse (type& c)
    {
      if (!options.at_once () && class_file (c) != unit.file ())
        return;

      if (object (c))
        traverse_object (c);
      else if (view (c))
        traverse_view (c);
    }

    void
    traverse_object (type&);

    void
    traverse_view (type&);
  };
}

void source::class_::
traverse_object (type& c)
{
  bool poly (polymorphic (c));
  bool abst (abstract (c));
  bool reuse_abst (abst && !poly);

  // The rest only applies to dynamic milti-database support and non-
  // reuse-abstract objects.
  //
  if (reuse_abst || options.multi_database () != multi_database::dynamic)
    return;

  string const& type (class_fq_name (c));
  string traits ("access::object_traits_impl< " + type + ", id_default >");

  os << "// " << class_name (c) << endl
     << "//" << endl
     << endl;

  os << "const " << traits << "::" << endl
     << "function_table_type*" << endl
     << traits << "::" << endl
     << "function_table[database_count];"
     << endl;
}

void source::class_::
traverse_view (type&)
{
}

namespace source
{
  void
  generate ()
  {
    context ctx;
    ostream& os (ctx.os);

    traversal::unit unit;
    traversal::defines unit_defines;
    typedefs unit_typedefs (false);
    traversal::namespace_ ns;
    class_ c;

    unit >> unit_defines >> ns;
    unit_defines >> c;
    unit >> unit_typedefs >> c;

    traversal::defines ns_defines;
    typedefs ns_typedefs (false);

    ns >> ns_defines >> ns;
    ns_defines >> c;
    ns >> ns_typedefs >> c;

    os << "namespace odb"
       << "{";

    unit.dispatch (ctx.unit);

    os << "}";
  }
}
