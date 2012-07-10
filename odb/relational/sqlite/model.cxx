// file      : odb/relational/sqlite/model.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <sstream>

#include <odb/relational/model.hxx>

#include <odb/relational/sqlite/common.hxx>
#include <odb/relational/sqlite/context.hxx>

using namespace std;

namespace relational
{
  namespace sqlite
  {
    namespace model
    {
      namespace relational = relational::model;

      struct object_columns: relational::object_columns, context
      {
        object_columns (base const& x): base (x) {}

        virtual string
        default_enum (semantics::data_member& m, tree en, string const&)
        {
          // Make sure the column is mapped to INTEGER.
          //
          sql_type const& t (parse_sql_type (column_type (), m, false));
          if (t.type != sql_type::INTEGER)
          {
            cerr << m.file () << ":" << m.line () << ":" << m.column ()
                 << ": error: column with default value specified as C++ "
                 << "enumerator must map to SQLite INTEGER" << endl;

            throw operation_failed ();
          }

          using semantics::enumerator;

          enumerator& e (dynamic_cast<enumerator&> (*unit.find (en)));

          ostringstream ostr;

          if (e.enum_ ().unsigned_ ())
            ostr << e.value ();
          else
            ostr << static_cast<long long> (e.value ());

          return ostr.str ();
        }
      };
      entry<object_columns> object_columns_;
    }
  }
}
