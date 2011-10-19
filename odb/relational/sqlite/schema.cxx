// file      : odb/relational/sqlite/schema.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/schema.hxx>

#include <odb/relational/sqlite/common.hxx>
#include <odb/relational/sqlite/context.hxx>

namespace relational
{
  namespace sqlite
  {
    namespace schema
    {
      namespace relational = relational::schema;

      //
      // Create.
      //

      struct object_columns: relational::object_columns, context
      {
        object_columns (base const& x): base (x) {}

        virtual void
        default_enum (semantics::data_member& m, tree en, string const&)
        {
          // Make sure the column is mapped to INTEGER.
          //
          if (column_sql_type (m).type != sql_type::INTEGER)
          {
            cerr << m.file () << ":" << m.line () << ":" << m.column ()
                 << ": error: column with default value specified as C++ "
                 << "enumerator must map to SQLite INTEGER" << endl;

            throw operation_failed ();
          }

          using semantics::enumerator;

          enumerator& e (dynamic_cast<enumerator&> (*unit.find (en)));

          if (e.enum_ ().unsigned_ ())
            os << " DEFAULT " << e.value ();
          else
            os << " DEFAULT " << static_cast<long long> (e.value ());
        }

        virtual void
        constraints (semantics::data_member& m)
        {
          base::constraints (m);

          if (m.count ("auto"))
          {
            if (options.sqlite_lax_auto_id ())
              os << " /*AUTOINCREMENT*/";
            else
              os << " AUTOINCREMENT";
          }
        }

        virtual void
        reference (semantics::data_member& m)
        {
          // In SQLite, by default, constraints are immediate.
          //
          if (semantics::class_* c =
              object_pointer (member_utype (m, prefix_)))
          {
            os << " REFERENCES " << table_qname (*c) << " (" <<
              column_qname (*id_member (*c)) << ") " <<
              "DEFERRABLE INITIALLY DEFERRED";
          }
          else
            base::reference (m);
        }

      };
      entry<object_columns> object_columns_;
    }
  }
}
