// file      : odb/relational/oracle/model.cxx
// author    : Constantin Michael <constantin@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <sstream>

#include <odb/relational/model.hxx>

#include <odb/relational/oracle/common.hxx>
#include <odb/relational/oracle/context.hxx>

using namespace std;

namespace relational
{
  namespace oracle
  {
    namespace model
    {
      namespace relational = relational::model;

      struct object_columns: relational::object_columns, context
      {
        object_columns (base const& x): base (x) {}

        virtual string
        default_bool (semantics::data_member&, bool v)
        {
          // Oracle seems to have the TRUE and FALSE literals, though no
          // boolean type.
          //
          return v ? "TRUE" : "FALSE";
        }

        virtual string
        default_enum (semantics::data_member& m, tree en, string const&)
        {
          // Make sure the column is mapped to Oracle NUMBER.
          //
          sql_type t (column_sql_type (m));

          if (t.type != sql_type::NUMBER)
          {
            cerr << m.file () << ":" << m.line () << ":" << m.column ()
                 << ": error: column with default value specified as C++ "
                 << "enumerator must map to Oracle NUMBER" << endl;

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

        virtual void
        reference (semantics::data_member&)
        {
        }
      };
      entry<object_columns> object_columns_;
    }
  }
}
