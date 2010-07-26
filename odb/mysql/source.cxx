// file      : odb/mysql/source.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/mysql/common.hxx>
#include <odb/mysql/source.hxx>

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

        // insert_query
        //
        os << "const char* const " << traits << "::insert_query = " << endl
           << "\"INSERT INTO\";"
           << endl;

        // bind ()
        //
        os << "void " << traits << "::" << endl
           << "bind (MYSQL_BIND*, image_type&)"
           << "{"
           << "}";

        // init (image, object)
        //
        os << "void " << traits << "::" << endl
           << "init (image_type&, object_type&)"
           << "{"
           << "}";

        // init (object, image)
        //
        os << "void " << traits << "::" << endl
           << "init (object_type&, image_type&)"
           << "{"
           << "}";

        // persist ()
        //
        os << "void " << traits << "::" << endl
           << "persist (database&, object_type& obj)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "insert_statement<object_type>& st (" << endl
           << "conn.statement_cache ().find<object_type> ().insert ());"
           << "init (st.image (), obj);"
           << "st.execute ();"
           << "}";

        // store ()
        //
        os << "void " << traits << "::" << endl
           << "store (database&, object_type& obj)"
           << "{"
           << "}";

        // erase ()
        //
        os << "void " << traits << "::" << endl
           << "erase (database&, const id_type& id)"
           << "{"
           << "}";

        // find ()
        //
        os << traits << "::pointer_type" << endl
           << traits << "::" << endl
           << "find (database&, const id_type& id)"
           << "{"
           << "return 0;"
           << "}";

        os << "bool " << traits << "::" << endl
           << "find (database&, const id_type& id, object_type& obj)"
           << "{"
           << "return false;"
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

    ctx.os << "#include <odb/mysql/database.hxx>" << endl
           << "#include <odb/mysql/transaction.hxx>" << endl
           << "#include <odb/mysql/connection.hxx>" << endl
           << "#include <odb/mysql/statement.hxx>" << endl
           << "#include <odb/mysql/exceptions.hxx>" << endl
           << endl;

    ctx.os << "namespace odb"
           << "{";

    unit.dispatch (ctx.unit);

    ctx.os << "}";
  }
}
