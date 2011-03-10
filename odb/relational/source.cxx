// file      : odb/relational/source.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/source.hxx>
#include <odb/relational/generate.hxx>

using namespace std;

namespace relational
{
  namespace source
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
      class_ c;

      unit >> unit_defines >> ns;
      unit_defines >> c;

      traversal::defines ns_defines;

      ns >> ns_defines >> ns;
      ns_defines >> c;

      //
      //
      os << "#include <odb/cache-traits.hxx>" << endl;

      if (ctx.embedded_schema)
        os << "#include <odb/schema-catalog-impl.hxx>" << endl;

      os << endl;

      //
      //
      os << "#include <odb/mysql/mysql.hxx>" << endl
         << "#include <odb/mysql/traits.hxx>" << endl
         << "#include <odb/mysql/database.hxx>" << endl
         << "#include <odb/mysql/transaction.hxx>" << endl
         << "#include <odb/mysql/connection.hxx>" << endl
         << "#include <odb/mysql/statement.hxx>" << endl
         << "#include <odb/mysql/statement-cache.hxx>" << endl
         << "#include <odb/mysql/object-statements.hxx>" << endl
         << "#include <odb/mysql/container-statements.hxx>" << endl
         << "#include <odb/mysql/exceptions.hxx>" << endl;

      if (ops.generate_query ())
        os << "#include <odb/mysql/result.hxx>" << endl;

      os << endl;

      // Details includes.
      //
      os << "#include <odb/details/unused.hxx>" << endl;

      if (ops.generate_query ())
        os << "#include <odb/details/shared-ptr.hxx>" << endl;

      os << endl;

      os << "namespace odb"
             << "{";

      unit.dispatch (ctx.unit);

      os << "}";
    }
  }
}
