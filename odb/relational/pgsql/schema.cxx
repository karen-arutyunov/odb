// file      : odb/relational/pgsql/schema.cxx
// author    : Constantin Michael <constantin@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/schema.hxx>

#include <odb/relational/pgsql/common.hxx>
#include <odb/relational/pgsql/context.hxx>

namespace relational
{
  namespace pgsql
  {
    namespace schema
    {
      namespace relational = relational::schema;

      //
      // Drop.
      //

      struct drop_common: virtual relational::drop_common
      {
        virtual void
        drop_table (string const& table)
        {
          os << "DROP TABLE IF EXISTS " << quote_id (table) << " CASCADE"
             << endl;
        }
      };

      struct member_drop: relational::member_drop, drop_common
      {
        member_drop (base const& x): base (x) {}
      };
      entry<member_drop> member_drop_;

      struct class_drop: relational::class_drop, drop_common
      {
        class_drop (base const& x): base (x) {}
      };
      entry<class_drop> class_drop_;

      //
      // Create.
      //

      struct object_columns: relational::object_columns, context
      {
        object_columns (base const& x): base (x) {}

        virtual void
        type (semantics::data_member& m)
        {
          if (m.count ("auto"))
          {
            const sql_type& t (column_sql_type (m));

            if (t.type == sql_type::INTEGER)
              os << "SERIAL";
            else if (t.type == sql_type::BIGINT)
              os << "BIGSERIAL";
            else
            {
              cerr << m.file () << ":" << m.line () << ":" << m.column ()
                   << ": error: automatically assigned object ID must map "
                   << "to PostgreSQL INTEGER or BIGINT" << endl;

              throw generation_failed ();
            }
          }
          else
          {
            base::type (m);
          }
        }
      };
      entry<object_columns> object_columns_;
    }
  }
}
