// file      : odb/relational/mysql/schema.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/schema.hxx>

#include <odb/relational/mysql/common.hxx>
#include <odb/relational/mysql/context.hxx>

namespace relational
{
  namespace mysql
  {
    namespace schema
    {
      namespace relational = relational::schema;

      //
      // Create.
      //

      struct create_common: virtual relational::create_common
      {
        virtual void
        create_table_post ()
        {
          os << ")";

          string const& engine (options.mysql_engine ());

          if (engine != "default")
            os << endl
               << " ENGINE=" << engine;

          os << endl;
        }
      };

      struct object_columns: relational::object_columns, context
      {
        object_columns (base const& x): base (x) {}

        virtual void
        null (semantics::data_member& m)
        {
          if (!context::null (m, prefix_))
            os << " NOT NULL";
          else
          {
            // MySQL TIMESTAMP is by default NOT NULL. If we want it
            // to contain NULL values, we need to explicitly declare
            // the column as NULL.
            //
            if (column_sql_type (m, prefix_).type == sql_type::TIMESTAMP)
              os << " NULL";
          }
        }

        virtual void
        default_bool (semantics::data_member&, bool v)
        {
          // MySQL has TRUE and FALSE as just aliases for 1 and 0. Still
          // use them for self-documentation.
          //
          os << " DEFAULT " << (v ? "TRUE" : "FALSE");
        }

        virtual void
        default_enum (semantics::data_member& m, tree en, string const& name)
        {
          // Make sure the column is mapped to an ENUM or integer type.
          //
          sql_type const& t (column_sql_type (m));

          switch (t.type)
          {
          case sql_type::ENUM:
          case sql_type::TINYINT:
          case sql_type::SMALLINT:
          case sql_type::MEDIUMINT:
          case sql_type::INT:
          case sql_type::BIGINT:
            break;
          default:
            {
              cerr << m.file () << ":" << m.line () << ":" << m.column ()
                   << ": error: column with default value specified as C++ "
                   << "enumerator must map to MySQL ENUM or integer type"
                   << endl;

              throw generation_failed ();
            }
          }

          using semantics::enum_;
          using semantics::enumerator;

          enumerator& er (dynamic_cast<enumerator&> (*unit.find (en)));
          enum_& e (er.enum_ ());

          if (t.type == sql_type::ENUM)
          {
            // Assuming the enumerators in the C++ enum and MySQL ENUM are
            // in the same order, calculate the poistion of the C++
            // enumerator and use that as an index in the MySQL ENUM.
            //
            size_t pos (0);

            for (enum_::enumerates_iterator i (e.enumerates_begin ()),
                   end (e.enumerates_end ()); i != end; ++i)
            {
              if (&i->enumerator () == &er)
                break;

              pos++;
            }

            if (pos < t.enumerators.size ())
              os << " DEFAULT " << t.enumerators[pos];
            else
            {
              cerr << m.file () << ":" << m.line () << ":" << m.column ()
                   << ": error: unable to map C++ enumerator '" << name
                   << "' to MySQL ENUM value" << endl;

              throw generation_failed ();
            }
          }
          else
          {
            if (e.unsigned_ ())
              os << " DEFAULT " << er.value ();
            else
              os << " DEFAULT " << static_cast<long long> (er.value ());
          }
        }

        virtual void
        constraints (semantics::data_member& m)
        {
          base::constraints (m);

          if (m.count ("auto"))
            os << " AUTO_INCREMENT";
        }

      };
      entry<object_columns> object_columns_;

      struct member_create: relational::member_create, create_common
      {
        member_create (base const& x): base (x) {}
      };
      entry<member_create> member_create_;

      struct class_create: relational::class_create, create_common
      {
        class_create (base const& x): base (x) {}
      };
      entry<class_create> class_create_;
    }
  }
}
