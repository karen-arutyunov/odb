// file      : odb/relational/header.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
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
      options const& ops (ctx.options);

      traversal::unit unit;
      traversal::defines unit_defines;
      traversal::namespace_ ns;
      instance<class_> c;

      unit >> unit_defines >> ns;
      unit_defines >> c;

      traversal::defines ns_defines;

      ns >> ns_defines >> ns;
      ns_defines >> c;

      os << "#include <odb/mysql/version.hxx>" << endl
         << "#include <odb/mysql/forward.hxx>" << endl
         << "#include <odb/mysql/mysql-types.hxx>" << endl;

      if (ops.generate_query ())
        os << "#include <odb/mysql/query.hxx>" << endl;

      os << endl
         << "#include <odb/details/buffer.hxx>" << endl
         << endl;

      os << "namespace odb"
         << "{";

      unit.dispatch (ctx.unit);

      os << "}";
    }
  }
}
