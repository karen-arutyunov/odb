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
      // Create.
      //

      struct object_columns: relational::object_columns
      {
        object_columns (base const& x): base (x) {}

        virtual void
        type (semantics::data_member& m)
        {
          if (m.count ("auto"))
          {
            // @@ Is this even vaguely correct?
            //

            const sql_type& t (pgsql::context::current ().column_sql_type (m));

            if (t.type == sql_type::INTEGER)
              os << " SERIAL";
            else if (t.type == sql_type::BIGINT)
              os << " BIGSERIAL";
            else
            {
              cerr << m.file () << ":" << m.line () << ":" << m.column ()
                   << ": error: auto increment columns must be either of "
                   << "PostgreSQL type INTEGER or BIGINT" << endl;

              throw generation_failed ();
            }
          }
        }
      };
      entry<object_columns> object_columns_;
    }
  }
}
