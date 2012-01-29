// file      : odb/relational/header.cxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/header.hxx>
#include <odb/relational/generate.hxx>

using namespace std;

namespace relational
{
  namespace header
  {
    void
    generate ()
    {
      context ctx;
      ostream& os (ctx.os);

      instance<include> i;
      i->generate ();

      os << "namespace odb"
         << "{";

      {
        traversal::unit unit;
        traversal::defines unit_defines;
        typedefs unit_typedefs (false);
        traversal::namespace_ ns;
        instance<class1> c;

        unit >> unit_defines >> ns;
        unit_defines >> c;
        unit >> unit_typedefs >> c;

        traversal::defines ns_defines;
        typedefs ns_typedefs (false);

        ns >> ns_defines >> ns;
        ns_defines >> c;
        ns >> ns_typedefs >> c;

        unit.dispatch (ctx.unit);
      }

      {
        traversal::unit unit;
        traversal::defines unit_defines;
        typedefs unit_typedefs (false);
        traversal::namespace_ ns;
        instance<class2> c;

        unit >> unit_defines >> ns;
        unit_defines >> c;
        unit >> unit_typedefs >> c;

        traversal::defines ns_defines;
        typedefs ns_typedefs (false);

        ns >> ns_defines >> ns;
        ns_defines >> c;
        ns >> ns_typedefs >> c;

        unit.dispatch (ctx.unit);
      }

      os << "}";
    }
  }
}
